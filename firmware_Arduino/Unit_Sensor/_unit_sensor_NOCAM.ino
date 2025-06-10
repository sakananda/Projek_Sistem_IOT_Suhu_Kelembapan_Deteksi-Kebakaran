//================PROGRAM Utama - ESP32-S3 Sensor Unit (RTOS: HANYA SENSOR, TANPA GAMBAR, Ro Kritis)
// Revisi: Logika pengiriman ulang status alarm independen & Perbaikan SHT40
//==============================================================================================
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoMqttClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "DFRobot_SHT40.h" // Pastikan library ini sudah terinstall dengan benar
// #include "esp_camera.h" // Dihilangkan
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <math.h>

// FreeRTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// --- Konfigurasi Pin (W5500, I2C, Sensor Lain) ---
#define W5500_MOSI 21
#define W5500_MISO 47
#define W5500_SCLK 48
#define W5500_CS   42
#define W5500_RST  14
#define I2C_SDA 38
#define I2C_SCL 39
#define GAS_SENSOR_PIN 3
#define LED_PIN        2

// --- Konfigurasi Threshold Alarm ---
#define THRESHOLD_TEMP_HIGH  40.0 // Sesuaikan nilai ini untuk penggunaan nyata
#define THRESHOLD_HUMID_HIGH 80.0 // Sesuaikan nilai ini untuk penggunaan nyata
#define THRESHOLD_SMOKE_PPM  30.0 // Sesuaikan nilai ini untuk penggunaan nyata

// --- Konfigurasi Jaringan Ethernet ---
byte mac[] = { 0x70, 0x69, 0x69, 0x2D, 0x30, 0x31 };
IPAddress eth_ip(192, 168, 14, 102);
IPAddress eth_gateway(192, 168, 13, 1); // Pastikan gateway ini sesuai dengan jaringan Anda
IPAddress eth_subnet(255, 255, 252, 0); // Subnet /22, mencakup 192.168.12.0 - 192.168.15.255
IPAddress eth_dns(192, 168, 13, 1);     // DNS bisa sama dengan gateway atau DNS publik
IPAddress mqtt_server_ip(192, 168, 14, 101);

// --- Konfigurasi MQTT ---
const char* mqtt_user = "testing";
const char* mqtt_pass = "12345";
const char* mqtt_client_id = "esp32_s3_sensor_no_cam"; // ID Client unik
const char* MQTT_TOPIC_ALARM_STATUS = "sensor/alarm";
const char* MQTT_TOPIC_TEMP = "sensor/suhu";
const char* MQTT_TOPIC_HUMID = "sensor/kelembapan";
const char* MQTT_TOPIC_SMOKE = "sensor/asap";

// --- Objek Global ---
DFRobot_SHT40 SHT40(SHT40_AD1B_IIC_ADDR); // Alamat I2C default untuk SHT40 DFRobot
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
EthernetClient ethClient;
MqttClient mqttClient(ethClient);

// --- Variabel Global Status ---
enum NetworkStatus { NET_DISCONNECTED, NET_ETH_CONNECTING, NET_ETH_CONNECTED, NET_ETH_FAILED_SETUP };
volatile NetworkStatus currentNetworkStatus = NET_DISCONNECTED;
unsigned long lastEthernetRetryTime = 0;
const unsigned long ethernetRetryDelay = 10000;

enum LedState { LED_OFF, LED_ETH_CONNECTING_OR_DOWN, LED_ETH_UP_MQTT_DOWN, LED_ALL_SYSTEM_OK, LED_ERROR_STATE, LED_ACTION_STATE };
volatile LedState currentLedState = LED_OFF;
bool previousErrorState = false;
unsigned long lastLedBlinkTime = 0;
bool ledBlinkStatus = false;

float temperature = 0.0, humidity = 0.0, ppm_smoke = 0.0;
float Ro = 0; // Akan diisi oleh calibrateGasSensor_main
unsigned long lastSendData = 0;
const unsigned long sendDataInterval = 8000;
unsigned long lastReconnectAttemptMQTT = 0;
const unsigned long reconnectIntervalMQTT = 5000;
unsigned long lastDisplayChange = 0;
const unsigned long displayChangeInterval = 5000;
const unsigned long errorDisplayInterval = 3000;
int displayState = 0;

volatile bool alarmSmokeActive = false;
volatile bool alarmTempActive = false;
volatile bool alarmHumidActive = false;
const unsigned long alarmResendTime = 30 * 1000; // Dipercepat untuk testing, kembalikan ke 1-5 menit untuk production

unsigned long lastSmokeAlarmSentTime = 0;
unsigned long lastTempAlarmSentTime = 0;
unsigned long lastHumidAlarmSentTime = 0;

// --- Handle untuk FreeRTOS Task ---
TaskHandle_t mainLogicTaskHandle = NULL;

// --- Deklarasi Forward Fungsi Task ---
void mainLogicTask(void *pvParameters);

// --- Fungsi LED ---
void setLedIndicatorState(LedState newState) {
    if (newState == LED_ERROR_STATE && !previousErrorState) { Serial.println("LED: Memasuki ERROR_STATE."); previousErrorState = true; }
    else if (newState != LED_ERROR_STATE && previousErrorState) { Serial.println("LED: Keluar dari ERROR_STATE."); previousErrorState = false; }
    if (currentLedState == LED_ERROR_STATE && newState != LED_ERROR_STATE && newState != LED_OFF) { return; }
    currentLedState = newState; lastLedBlinkTime = 0;
}

void handleLedIndicator() {
    unsigned long currentTime = millis();
    switch (currentLedState) {
        case LED_OFF: digitalWrite(LED_PIN, LOW); break;
        case LED_ETH_CONNECTING_OR_DOWN: if (currentTime - lastLedBlinkTime > 1000) { ledBlinkStatus = !ledBlinkStatus; digitalWrite(LED_PIN, ledBlinkStatus); lastLedBlinkTime = currentTime; } break;
        case LED_ETH_UP_MQTT_DOWN:       if (currentTime - lastLedBlinkTime > 500)  { ledBlinkStatus = !ledBlinkStatus; digitalWrite(LED_PIN, ledBlinkStatus); lastLedBlinkTime = currentTime; } break;
        case LED_ALL_SYSTEM_OK:          digitalWrite(LED_PIN, HIGH); break;
        case LED_ERROR_STATE:            if (currentTime - lastLedBlinkTime > 100)  { ledBlinkStatus = !ledBlinkStatus; digitalWrite(LED_PIN, ledBlinkStatus); lastLedBlinkTime = currentTime; } break;
        case LED_ACTION_STATE:           digitalWrite(LED_PIN, (millis() / 75) % 2); break;
        default:                         digitalWrite(LED_PIN, LOW); break;
    }
}

// --- Fungsi Koneksi Ethernet ---
void setupEthernet_main() {
    Serial.println("MAIN_TASK: Ethernet: Mencoba koneksi...");
    currentNetworkStatus = NET_ETH_CONNECTING;
    bool oled_available = (u8g2.getU8x8()->display_info != NULL);
    if(oled_available){ u8g2.clearBuffer(); u8g2.setFont(u8g2_font_helvR10_tf); u8g2.drawStr(10, 30, "Init Ethernet..."); u8g2.sendBuffer(); }

    pinMode(W5500_RST, OUTPUT);
    digitalWrite(W5500_RST, LOW); vTaskDelay(pdMS_TO_TICKS(100));
    digitalWrite(W5500_RST, HIGH); vTaskDelay(pdMS_TO_TICKS(300));

    SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI);
    Ethernet.init(W5500_CS);
    Ethernet.begin(mac, eth_ip, eth_dns, eth_gateway, eth_subnet);
    Serial.println("MAIN_TASK: Ethernet: Perintah begin() dijalankan...");

    unsigned long ethernetStartTime = millis(); bool connected = false;
    while(millis() - ethernetStartTime < 10000) {
        if (Ethernet.linkStatus() == LinkON && Ethernet.hardwareStatus() != EthernetNoHardware) {
             if (Ethernet.localIP() != IPAddress(0,0,0,0) && Ethernet.localIP() == eth_ip) {
                Serial.print("MAIN_TASK: Ethernet: Terhubung! IP: "); Serial.println(Ethernet.localIP());
                currentNetworkStatus = NET_ETH_CONNECTED; connected = true;
                if(oled_available){ u8g2.clearBuffer(); u8g2.setFont(u8g2_font_helvB10_te); u8g2.setCursor(5, 20); u8g2.print("Eth: Connected!"); u8g2.setFont(u8g2_font_helvR08_tf); u8g2.setCursor(5, 40); u8g2.print(Ethernet.localIP()); u8g2.sendBuffer(); vTaskDelay(pdMS_TO_TICKS(1000)); }
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500)); Serial.print(".");
    }
    Serial.println();
    if (!connected) {
        Serial.println("MAIN_TASK: Ethernet: Gagal mendapatkan koneksi/IP atau W5500 tidak terdeteksi.");
        currentNetworkStatus = NET_ETH_FAILED_SETUP;
        if(oled_available){ u8g2.clearBuffer(); u8g2.setFont(u8g2_font_helvR10_tf); u8g2.drawStr(5, 20, "Eth Init Failed!"); u8g2.setCursor(5,40); u8g2.print("IP: "); u8g2.print(Ethernet.localIP()); u8g2.sendBuffer(); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
}

// --- Fungsi Koneksi MQTT ---
bool connectMqtt_main() {
    if (currentNetworkStatus != NET_ETH_CONNECTED) { Serial.println("MAIN_TASK: MQTT: Ethernet tidak terhubung."); return false; }
    Serial.print("MAIN_TASK: MQTT: Mencoba koneksi ke: "); Serial.print(mqtt_server_ip.toString()); Serial.print(" dengan Client ID: "); Serial.println(mqtt_client_id);
    mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);
    mqttClient.setId(mqtt_client_id);
    if (!mqttClient.connect(mqtt_server_ip, 1883)) {
        Serial.print("MAIN_TASK: MQTT: Koneksi gagal! Error code: "); Serial.println(mqttClient.connectError());
         if (currentLedState != LED_ERROR_STATE && currentLedState != LED_ACTION_STATE) { setLedIndicatorState(LED_ETH_UP_MQTT_DOWN); }
        return false;
    }
    Serial.println("MAIN_TASK: MQTT: Terhubung ke broker!");
    if (currentLedState != LED_ERROR_STATE && currentLedState != LED_ACTION_STATE) { setLedIndicatorState(LED_ALL_SYSTEM_OK); }
    return true;
}

// --- Fungsi Kalibrasi & Pembacaan Gas Sensor ---
void calibrateGasSensor_main() {
    const int maxCalibrationRetries = 3; int calibrationAttempt = 0;
    bool oled_available = (u8g2.getU8x8()->display_info != NULL);
    do {
        calibrationAttempt++; Serial.printf("MAIN_TASK: Kalibrasi gas (#%d)...\n", calibrationAttempt);
        if (oled_available) {u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);char calMsg[30];sprintf(calMsg,"Kalibrasi Gas (#%d)",calibrationAttempt);u8g2.drawStr(5,20,calMsg);u8g2.drawStr(5,40,"Udara Bersih?");u8g2.sendBuffer();}
        float rs_sum = 0; const int samples = 50; bool vnode_issue = false;
        for (int i=0; i<samples; i++) {
            int raw=analogRead(GAS_SENSOR_PIN); float Vnode=raw*(3.3/4095.0);
            if(Vnode < 0.01){ Serial.printf("MAIN_TASK: Vnode rendah (%.4fV) sampel #%d\n",Vnode, i+1); vnode_issue=true; }
            float Rs;
            if (Vnode < 0.01) { Rs = 2000000; } else { Rs = (5.0 * 19770.0 / Vnode) - 9750.0 - 19770.0; }
            if(isinf(Rs)||isnan(Rs)){Serial.printf("MAIN_TASK: Rs invalid sampel #%d Vnode:%.4f\n",i+1,Vnode); Rs = 2000000;}
            if(Rs<0 && !isinf(Rs) && !isnan(Rs)) Rs=0;
            rs_sum+=Rs; vTaskDelay(pdMS_TO_TICKS(200));
        }
        if(isinf(rs_sum)||isnan(rs_sum)||samples==0){Ro=INFINITY;} else {Ro=(rs_sum/samples)/9.83;}
        Serial.printf("MAIN_TASK: Percobaan Kalibrasi #%d: Ro=%.4f\n",calibrationAttempt,Ro);
        if(isinf(Ro)||isnan(Ro)||Ro > 500000.0||Ro <= 10.0||vnode_issue){
            Serial.printf("MAIN_TASK: Ro tidak valid (inf:%d,NaN:%d,>500k:%d,<=10:%d,Vnode issue:%d)\n",isinf(Ro),isnan(Ro),Ro>500000.0,Ro<=10.0,vnode_issue);
            if(calibrationAttempt<maxCalibrationRetries){
                Serial.println("MAIN_TASK: Mengulang kalibrasi...");
                if(oled_available){u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);u8g2.drawStr(10,20,"Ro Invalid!");char roValMsg[25];if(isinf(Ro)){sprintf(roValMsg,"Ro: inf");}else if(isnan(Ro)){sprintf(roValMsg,"Ro: NaN");}else{dtostrf(Ro,8,2,roValMsg);}u8g2.drawStr(10,40,roValMsg);u8g2.drawStr(10,55,"Ulangi...");u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(2500));}
            } else {break;}
        } else {break;}
    } while(calibrationAttempt<maxCalibrationRetries);

    if(isinf(Ro)||isnan(Ro)||Ro > 500000.0||Ro <= 10.0){
        Serial.println("MAIN_TASK: Kalibrasi GAGAL final. Ro tidak valid.");
        if(oled_available){u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);u8g2.drawStr(10,20,"Kalibrasi GAGAL!");char roValMsg[25];if(isinf(Ro)){sprintf(roValMsg,"Ro: inf");}else if(isnan(Ro)){sprintf(roValMsg,"Ro: NaN");}else{dtostrf(Ro,8,2,roValMsg);}u8g2.drawStr(10,40,roValMsg);u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(3000));}
        if(Ro > 0 && Ro <=10.0) Ro = -1.0f; else if(isinf(Ro)) Ro = -2.0f; else if(isnan(Ro)) Ro = -3.0f; else Ro = -4.0f;
    } else {
        Serial.printf("MAIN_TASK: Kalibrasi OK (#%d). Ro=%.2f ohm\n",calibrationAttempt,Ro);
        if(oled_available){u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);u8g2.drawStr(10,30,"Kalibrasi OK!");char Ro_buf[20];dtostrf(Ro,4,2,Ro_buf);u8g2.drawStr(10,50,Ro_buf);u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(2000));}
    }
}

float bacaPPM_main() {
    if(Ro <= 0){ Serial.printf("MAIN_TASK: Ro tidak valid (%.2f) untuk bacaPPM.\n", Ro); return Ro; }
    float rs_sum=0;const int samples=5;
    for(int i=0;i<samples;i++){
        int raw=analogRead(GAS_SENSOR_PIN);float Vnode=raw*(3.3/4095.0); float Rs;
        if (Vnode < 0.01) { Rs = 2000000; } else { Rs = (5.0*19770.0/Vnode)-9750.0-19770.0; }
        if(Rs<0 && !isinf(Rs) && !isnan(Rs)) Rs=0;
        if(isinf(Rs)||isnan(Rs)){ Serial.printf("MAIN_TASK: Rs invalid bacaPPM #%d Vnode:%.4f\n",i+1,Vnode); return -5.0; }
        rs_sum+=Rs; vTaskDelay(pdMS_TO_TICKS(50));
    }
    float Rs_avg = rs_sum/samples; if(Rs_avg <= 0) return 0;
    float ratio = Rs_avg/Ro; if(ratio <= 0) return 0;
    float log_ppm = (log10(ratio) - 0.53) / -0.44 + 2.3;
    float ppm = pow(10.0, log_ppm);
    return ppm > 0 ? ppm : 0;
}

// --- Task Logika Utama (Berjalan di Core 0) ---
void mainLogicTask(void *pvParameters) {
    Serial.println("mainLogicTask: Berjalan di Core " + String(xPortGetCoreID()));

    pinMode(LED_PIN, OUTPUT);
    setLedIndicatorState(LED_OFF);
    Wire.begin(I2C_SDA, I2C_SCL);

    bool oled_ok = false;
    if (!u8g2.begin()) {
        Serial.println("SETUP_MAIN: Inisialisasi OLED GAGAL!"); setLedIndicatorState(LED_ERROR_STATE);
    } else {
        Serial.println("SETUP_MAIN: Inisialisasi OLED OK."); oled_ok = true;
        u8g2.setPowerSave(0);
        u8g2.clearBuffer(); u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(15, 35, "Starting Up..."); u8g2.sendBuffer(); vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // --- PERBAIKAN SHT40 Inisialisasi ---
    SHT40.begin(); // Panggil begin() tanpa cek return value langsung
    uint32_t sensorID_main_sht = SHT40.getDeviceID(); // Gunakan getDeviceID untuk verifikasi
    Serial.print("SETUP_MAIN: SHT40 Device ID: 0x"); Serial.println(sensorID_main_sht, HEX);
    if (sensorID_main_sht == 0 || sensorID_main_sht == 0xFFFFFFFF || sensorID_main_sht == 0XFF) { // Tambahkan 0xFF untuk beberapa kasus
        Serial.println("SETUP_MAIN: SHT40 tidak terdeteksi!"); setLedIndicatorState(LED_ERROR_STATE);
        if (oled_ok) {u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);u8g2.drawStr(10,30,"SHT40 Error!");u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(2000));}
    } else { Serial.println("SETUP_MAIN: SHT40 OK."); }
    // --- AKHIR PERBAIKAN SHT40 ---


    if (currentLedState != LED_ERROR_STATE) {
        setupEthernet_main();
        if (currentNetworkStatus != NET_ETH_CONNECTED) {
            Serial.println("SETUP_MAIN: Ethernet GAGAL."); setLedIndicatorState(LED_ERROR_STATE);
        } else { Serial.println("SETUP_MAIN: Ethernet OK."); }
    }

    if (currentLedState != LED_ERROR_STATE && currentNetworkStatus == NET_ETH_CONNECTED) {
        Serial.println("SETUP_MAIN: Mencoba koneksi MQTT awal (persisten)...");
        if (oled_ok) {u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);u8g2.drawStr(5,35,"Connecting MQTT...");u8g2.sendBuffer();}
        setLedIndicatorState(LED_ETH_UP_MQTT_DOWN);
        int mqttRetryCount = 0;
        while (!mqttClient.connected() && mqttRetryCount < 5) {
            mqttRetryCount++; Serial.printf("SETUP_MAIN: Mencoba MQTT (#%d)...\n", mqttRetryCount);
            if (connectMqtt_main()) { break; }
            unsigned long singleRetryStartTime = millis();
            while(millis() - singleRetryStartTime < reconnectIntervalMQTT) { handleLedIndicator(); vTaskDelay(pdMS_TO_TICKS(100)); }
            if (oled_ok && !mqttClient.connected()) { u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);char mqttStatusMsg[30];sprintf(mqttStatusMsg,"MQTT Retry #%d",mqttRetryCount);u8g2.drawStr(5,35,mqttStatusMsg);u8g2.sendBuffer();}
        }
        if (mqttClient.connected()) {
            Serial.println("SETUP_MAIN: MQTT awal BERHASIL.");
            if (oled_ok) {u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvB10_te);u8g2.drawStr(10,35,"MQTT OK!");u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(1500));}
        } else {
            Serial.println("SETUP_MAIN: MQTT awal GAGAL setelah beberapa percobaan.");
            setLedIndicatorState(LED_ERROR_STATE);
            if (oled_ok) {u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);u8g2.drawStr(20,35,"MQTT Failed!");u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(1500));}
        }
    } else if (currentLedState != LED_ERROR_STATE) {
        Serial.println("SETUP_MAIN: Ethernet tidak siap, MQTT tidak dicoba.");
        if(currentNetworkStatus != NET_ETH_FAILED_SETUP) setLedIndicatorState(LED_ETH_CONNECTING_OR_DOWN);
    }

    if (currentLedState != LED_ERROR_STATE) {
        calibrateGasSensor_main();
        if (Ro <= 0) {
            Serial.println("SETUP_MAIN: Kalibrasi Sensor Gas GAGAL FINAL. Ini adalah error kritis.");
            setLedIndicatorState(LED_ERROR_STATE);
            if (oled_ok) {u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);u8g2.drawStr(10,30,"Gas Cal Fail!");u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(2000));}
        } else { Serial.println("SETUP_MAIN: Kalibrasi Gas OK."); }
    } else { Serial.println("SETUP_MAIN: Skipping Gas Cal karena error sebelumnya."); }

    if (currentLedState == LED_ERROR_STATE) {
        Serial.println("===== SETUP_MAIN GAGAL =====");
        if (oled_ok) {
            u8g2.setPowerSave(0);u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvR10_tf);
            u8g2.drawStr(10,25,"SYSTEM ERROR");
            if(currentNetworkStatus==NET_ETH_FAILED_SETUP){u8g2.drawStr(10,45,"Ethernet Failed");}
            else if(currentNetworkStatus==NET_ETH_CONNECTED && !mqttClient.connected()){u8g2.drawStr(10,45,"MQTT Conn Fail");}
            else if(Ro <= 0) {u8g2.drawStr(10, 45, "Gas Cal Fail");}
            else {u8g2.drawStr(10,45,"Check Details");}
            u8g2.sendBuffer();
        }
    } else {
        Serial.println("===== SETUP_MAIN BERHASIL =====");
        // --- PERBAIKAN SHT40 Pembacaan Awal ---
        temperature = SHT40.getTemperature(PRECISION_HIGH);
        humidity = SHT40.getHumidity(PRECISION_HIGH);
        // --- AKHIR PERBAIKAN SHT40 ---
        if (Ro > 0) { ppm_smoke = bacaPPM_main(); } else { ppm_smoke = Ro; }
        Serial.printf("Pembacaan awal - T:%.2f H:%.2f S:%.2f PPM\n", temperature, humidity, ppm_smoke);
        if (oled_ok) {
            u8g2.setPowerSave(0);u8g2.clearBuffer();u8g2.setFont(u8g2_font_helvB10_te);
            u8g2.drawStr(10,35,"System Ready!");
            u8g2.sendBuffer();vTaskDelay(pdMS_TO_TICKS(2000));
        }
        lastSendData = millis(); lastDisplayChange = millis();
    }

    // === LOOP UTAMA mainLogicTask ===
    while(true) {
        unsigned long now = millis();
        char oled_buffer_loop[32];
        handleLedIndicator();
        bool oled_display_available_loop = (oled_ok && u8g2.getU8x8()->display_info != NULL);

        if (currentLedState == LED_ERROR_STATE && currentNetworkStatus == NET_ETH_FAILED_SETUP) {
            if (now - lastEthernetRetryTime > ethernetRetryDelay) {
                Serial.println("MAIN_LOOP: Mencoba re-init Ethernet dari error state...");
                setupEthernet_main(); lastEthernetRetryTime = now;
                if (currentNetworkStatus == NET_ETH_CONNECTED) {
                    Serial.println("MAIN_LOOP: Ethernet OK setelah retry. Mencoba MQTT...");
                    currentLedState = LED_OFF; setLedIndicatorState(LED_ETH_UP_MQTT_DOWN);
                    lastReconnectAttemptMQTT = 0;
                }
            }
        } else if (currentLedState != LED_ERROR_STATE) {
            if (Ethernet.linkStatus() != LinkON && currentNetworkStatus != NET_ETH_FAILED_SETUP && currentNetworkStatus != NET_ETH_CONNECTING) {
                if (currentNetworkStatus != NET_DISCONNECTED) {
                    Serial.println("MAIN_LOOP: Ethernet terputus!");
                    if(mqttClient.connected()) mqttClient.stop();
                    currentNetworkStatus=NET_DISCONNECTED; setLedIndicatorState(LED_ETH_CONNECTING_OR_DOWN);
                }
                if (now - lastEthernetRetryTime > ethernetRetryDelay) {
                    Serial.println("MAIN_LOOP: Mencoba re-init Ethernet...");
                    setupEthernet_main(); lastEthernetRetryTime = now;
                    if(currentNetworkStatus == NET_ETH_CONNECTED) lastReconnectAttemptMQTT = 0;
                }
            } else if (currentNetworkStatus == NET_DISCONNECTED && Ethernet.linkStatus() == LinkON && Ethernet.hardwareStatus() != EthernetNoHardware) {
                 Serial.println("MAIN_LOOP: Ethernet kembali terhubung, re-init...");
                 setupEthernet_main();
                 if(currentNetworkStatus == NET_ETH_CONNECTED) lastReconnectAttemptMQTT = 0;
            }

            if (currentNetworkStatus == NET_ETH_CONNECTED) {
                if (!mqttClient.connected()) {
                    if (now - lastReconnectAttemptMQTT >= reconnectIntervalMQTT) {
                        lastReconnectAttemptMQTT = now; Serial.println("MAIN_LOOP: MQTT disconnected. Mencoba konek...");
                        connectMqtt_main();
                    }
                } else {
                    mqttClient.poll();
                    if (currentLedState != LED_ACTION_STATE && currentLedState != LED_ERROR_STATE ) {
                        setLedIndicatorState(LED_ALL_SYSTEM_OK);
                    }
                }
            } else if (currentNetworkStatus != NET_ETH_FAILED_SETUP && currentNetworkStatus != NET_ETH_CONNECTING) {
                if(mqttClient.connected()) mqttClient.stop();
                if(currentLedState != LED_ACTION_STATE && currentLedState != LED_ERROR_STATE){
                    setLedIndicatorState(LED_ETH_CONNECTING_OR_DOWN);
                }
            }
        }

        if (mqttClient.connected() && (now - lastSendData >= sendDataInterval)) {
            lastSendData = now;
            if (currentLedState != LED_ERROR_STATE) setLedIndicatorState(LED_ACTION_STATE);

            // --- PERBAIKAN SHT40 Pembacaan di Loop ---
            temperature = SHT40.getTemperature(PRECISION_HIGH);
            humidity = SHT40.getHumidity(PRECISION_HIGH);
            // --- AKHIR PERBAIKAN SHT40 ---

            if(isnan(temperature) || isnan(humidity)){
                Serial.println("MAIN_LOOP: Gagal baca SHT40!");
            } else {
                 // --- PERBAIKAN SHT40 Heater ---
                if(humidity > 80.0){ // Kembali ke 80.0 seperti kode awal
                     SHT40.enHeater(POWER_CONSUMPTION_H_HEATER_1S); // Gunakan enHeater dan konstanta sebelumnya
                     Serial.println("SHT40 Heater diaktifkan (POWER_CONSUMPTION_H_HEATER_1S).");
                }
                 // --- AKHIR PERBAIKAN SHT40 ---
            }

            if(Ro > 0){ ppm_smoke = bacaPPM_main(); } else { ppm_smoke = Ro; }
            Serial.printf("DATA: T:%.2f H:%.2f S:%.2f PPM (Ro:%.2f)\n",temperature,humidity,ppm_smoke, Ro);

            if(mqttClient.connected()){
                if(!isnan(temperature)){dtostrf(temperature,4,2,oled_buffer_loop);mqttClient.beginMessage(MQTT_TOPIC_TEMP);mqttClient.print(oled_buffer_loop);mqttClient.endMessage();}
                if(!isnan(humidity)){dtostrf(humidity,4,2,oled_buffer_loop);mqttClient.beginMessage(MQTT_TOPIC_HUMID);mqttClient.print(oled_buffer_loop);mqttClient.endMessage();}
                if(ppm_smoke >= 0){dtostrf(ppm_smoke,6,2,oled_buffer_loop);mqttClient.beginMessage(MQTT_TOPIC_SMOKE);mqttClient.print(oled_buffer_loop);mqttClient.endMessage();}
                else { dtostrf(ppm_smoke,4,1,oled_buffer_loop);mqttClient.beginMessage(MQTT_TOPIC_SMOKE);mqttClient.print(oled_buffer_loop); mqttClient.print(" (Error)"); mqttClient.endMessage(); }
            }

            // Asap
            bool currentSmokeAlarmCondition = (ppm_smoke > THRESHOLD_SMOKE_PPM && ppm_smoke >= 0);
            if (currentSmokeAlarmCondition) {
                if (!alarmSmokeActive || (now - lastSmokeAlarmSentTime > alarmResendTime)) {
                    Serial.println("MAIN_LOOP: Asap terdeteksi (status dikirim/resend).");
                    if (mqttClient.connected()) { mqttClient.beginMessage(MQTT_TOPIC_ALARM_STATUS); mqttClient.print("SMOKE_DETECTED"); mqttClient.endMessage(); }
                    alarmSmokeActive = true; lastSmokeAlarmSentTime = now;
                }
            } else {
                if (alarmSmokeActive) {
                    Serial.println("MAIN_LOOP: Asap normal (status dikirim).");
                    if (mqttClient.connected()) { mqttClient.beginMessage(MQTT_TOPIC_ALARM_STATUS); mqttClient.print("SMOKE_NORMAL"); mqttClient.endMessage(); }
                    alarmSmokeActive = false;
                }
            }
            // Suhu
            bool currentTempAlarmCondition = (temperature > THRESHOLD_TEMP_HIGH && !isnan(temperature));
            if (currentTempAlarmCondition) {
                if (!alarmTempActive || (now - lastTempAlarmSentTime > alarmResendTime)) {
                    Serial.println("MAIN_LOOP: Suhu Tinggi (status dikirim/resend).");
                    if (mqttClient.connected()) { mqttClient.beginMessage(MQTT_TOPIC_ALARM_STATUS); mqttClient.print("TEMP_HIGH"); mqttClient.endMessage(); }
                    alarmTempActive = true; lastTempAlarmSentTime = now;
                }
            } else {
                if (alarmTempActive && temperature < (THRESHOLD_TEMP_HIGH - 2.0) && !isnan(temperature)) {
                    Serial.println("MAIN_LOOP: Suhu normal (status dikirim).");
                    if (mqttClient.connected()) { mqttClient.beginMessage(MQTT_TOPIC_ALARM_STATUS); mqttClient.print("TEMP_NORMAL"); mqttClient.endMessage(); }
                    alarmTempActive = false;
                }
            }
            // Kelembapan
            bool currentHumidAlarmCondition = (humidity > THRESHOLD_HUMID_HIGH && !isnan(humidity));
            if (currentHumidAlarmCondition) {
                if (!alarmHumidActive || (now - lastHumidAlarmSentTime > alarmResendTime)) {
                    Serial.println("MAIN_LOOP: Lembap Tinggi (status dikirim/resend).");
                    if (mqttClient.connected()) { mqttClient.beginMessage(MQTT_TOPIC_ALARM_STATUS); mqttClient.print("HUMIDITY_HIGH"); mqttClient.endMessage(); }
                    alarmHumidActive = true; lastHumidAlarmSentTime = now;
                }
            } else {
                if (alarmHumidActive && humidity < (THRESHOLD_HUMID_HIGH - 5.0) && !isnan(humidity)) {
                    Serial.println("MAIN_LOOP: Lembap normal (status dikirim).");
                    if (mqttClient.connected()) { mqttClient.beginMessage(MQTT_TOPIC_ALARM_STATUS); mqttClient.print("HUMIDITY_NORMAL"); mqttClient.endMessage(); }
                    alarmHumidActive = false;
                }
            }

            if(currentLedState==LED_ACTION_STATE){
                if(currentNetworkStatus==NET_ETH_CONNECTED && mqttClient.connected()){setLedIndicatorState(LED_ALL_SYSTEM_OK);}
                else if(currentNetworkStatus==NET_ETH_CONNECTED){setLedIndicatorState(LED_ETH_UP_MQTT_DOWN);}
                else{setLedIndicatorState(LED_ETH_CONNECTING_OR_DOWN);}
            }
        } else if (!mqttClient.connected() && currentNetworkStatus == NET_ETH_CONNECTED) { }

        if (oled_display_available_loop) {
            unsigned long currentActiveDisplayInterval = (currentLedState == LED_ERROR_STATE) ? errorDisplayInterval : displayChangeInterval;
            bool shouldUpdateOledThisCycle = false;
            if (now - lastDisplayChange >= currentActiveDisplayInterval) {
                shouldUpdateOledThisCycle = true;
                if (currentLedState == LED_ERROR_STATE) { displayState = 4; }
                else { displayState = (displayState + 1) % 4; }
            }
            if (shouldUpdateOledThisCycle) {
                lastDisplayChange = now; u8g2.setPowerSave(0); u8g2.clearBuffer();
                if (displayState == 4) {
                    u8g2.setFont(u8g2_font_helvR10_tf); u8g2.drawStr(10, 25, "SYSTEM ERROR");
                    if (currentNetworkStatus == NET_ETH_FAILED_SETUP) { u8g2.drawStr(10, 45, "Ethernet Failed"); }
                    else if (currentLedState == LED_ERROR_STATE && currentNetworkStatus == NET_ETH_CONNECTED && !mqttClient.connected()) { u8g2.drawStr(10, 45, "MQTT Conn Fail"); }
                    else if (currentLedState == LED_ERROR_STATE && Ro <= 0) { u8g2.drawStr(10, 45, "Gas Cal Fail"); }
                    else { u8g2.drawStr(10, 45, "Check Details");}
                } else {
                    u8g2.setFont(u8g2_font_helvB10_te);
                    switch (displayState) {
                        case 0:
                            u8g2.setCursor(10, 18); u8g2.print("Suhu:"); u8g2.setFont(u8g2_font_logisoso28_tr); u8g2.setCursor(10, 55);
                            if(isnan(temperature)) { u8g2.print("N/A"); }
                            else { dtostrf(temperature, 4, 1, oled_buffer_loop); u8g2.print(oled_buffer_loop); u8g2.setFont(u8g2_font_helvR10_tf); u8g2.drawUTF8(u8g2.getCursorX() + 30, u8g2.getCursorY() - 18, "°C"); }
                            break;
                        case 1:
                            u8g2.setCursor(10, 18); u8g2.print("Kelembapan:"); u8g2.setFont(u8g2_font_logisoso28_tr); u8g2.setCursor(10, 55);
                            if(isnan(humidity)) { u8g2.print("N/A"); }
                            else { dtostrf(humidity, 4, 1, oled_buffer_loop); u8g2.print(oled_buffer_loop); u8g2.setFont(u8g2_font_helvR10_tf); u8g2.print(" %"); }
                            break;
                        case 2:
                            u8g2.setCursor(10, 18); u8g2.print("Asap (PPM):"); u8g2.setFont(u8g2_font_logisoso28_tr); u8g2.setCursor(10, 55);
                            if(ppm_smoke < 0) {
                                 if (ppm_smoke == -1.0f) u8g2.print("RoLow"); else if (ppm_smoke == -2.0f) u8g2.print("RoInf");
                                 else if (ppm_smoke == -3.0f) u8g2.print("RoNaN"); else if (ppm_smoke == -4.0f) u8g2.print("RoHi");
                                 else if (ppm_smoke == -5.0f) u8g2.print("RsErr"); else u8g2.print("N/A");
                            } else { dtostrf(ppm_smoke, 5, 0, oled_buffer_loop); u8g2.print(oled_buffer_loop); }
                            break;
                        case 3:
                            u8g2.setFont(u8g2_font_helvR08_tf); u8g2.setCursor(2, 10);
                            if (currentNetworkStatus == NET_ETH_CONNECTED) { u8g2.print("NET: ETH "); u8g2.print(Ethernet.localIP()); }
                            else if (currentNetworkStatus == NET_ETH_CONNECTING) { u8g2.print("NET: Connecting ETH..."); }
                            else if (currentNetworkStatus == NET_ETH_FAILED_SETUP) { u8g2.print("NET: ETH Setup Fail");}
                            else { u8g2.print("NET: ETH Disconnected"); }
                            u8g2.setCursor(2, 22);
                            if (mqttClient.connected()) { u8g2.print("MQTT: Connected"); u8g2.setCursor(2, 34); u8g2.print(mqtt_server_ip.toString()); }
                            else { u8g2.print("MQTT: Disconnected"); }
                            u8g2.setFont(u8g2_font_helvB08_tf); u8g2.setCursor(2, 48);
                            String alarmOledStatus = "ALARM: "; bool anyAlarm = false;
                            if(alarmSmokeActive) { alarmOledStatus += "ASAP "; anyAlarm = true; }
                            if(alarmTempActive) { alarmOledStatus += "SUHU "; anyAlarm = true; }
                            if(alarmHumidActive) { alarmOledStatus += "LMBP "; anyAlarm = true; }
                            if(anyAlarm) { u8g2.print(alarmOledStatus); } else { u8g2.print("Status: Normal"); }
                            break;
                    }
                }
                u8g2.sendBuffer();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    Serial.begin(115200);
    unsigned long bootStartTime = millis();
    while(!Serial && (millis() - bootStartTime < 2000));
    Serial.println("\n===== RTOS ESP32 Sensor Device Booting (SHT40 Fix) =====");
    xTaskCreatePinnedToCore( mainLogicTask, "MainLogicTask", 12288, NULL, 1, &mainLogicTaskHandle, 0);
    Serial.println("Task utama telah dibuat dan dijadwalkan di Core 0.");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}