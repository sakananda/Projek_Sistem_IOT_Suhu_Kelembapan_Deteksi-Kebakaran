//================PROGRAM ESP32-S3 ALARM UNIT 
#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <FastLED.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// --- Konfigurasi Wi-Fi ---
const char* WIFI_SSID = "Elitech"; 
const char* WIFI_PASSWORD = "wifis1nko"; 

// --- Konfigurasi IP Statis Wi-Fi ---
IPAddress LOCAL_IP_ALARM(192, 168, 14, 103);
IPAddress GATEWAY_IP_ALARM(192, 168, 13, 1); 
IPAddress SUBNET_MASK_ALARM(255, 255, 252, 0); 
IPAddress PRIMARY_DNS_ALARM(192, 168, 13, 1); 
IPAddress SECONDARY_DNS_ALARM(8, 8, 8, 8);

// --- Konfigurasi MQTT ---
const char* MQTT_BROKER = "192.168.14.101"; 
const int MQTT_PORT = 1883;
const char* MQTT_USER = "testing";
const char* MQTT_PASS = "12345";
const char* MQTT_CLIENT_ID_ALARM_DEVICE = "esp32_s3_alarm"; 
const char* MQTT_TOPIC_SENSOR_ALARM_STATUS = "sensor/alarm";

// --- Konfigurasi Telegram ---
const String TELEGRAM_BOT_TOKEN = "7747537035:AAEZ84UCNlntRaP_uNKDOJK-DH_Yro3XYlM"; 
const String AUTHORIZED_TELEGRAM_CHAT_ID = "1416038310";       

// --- Konfigurasi Pin Hardware ---
#define MOSFET_PIN 38        
#define BREAK_GLASS_PIN 10   
#define LED_PIN 48           
#define NUM_LEDS 1           

// --- Konfigurasi Durasi ---
const unsigned long ARMED_WINDOW_DURATION_MS = 5 * 60 * 1000;  // 5 menit mode siaga
const unsigned long SIREN_DURATION_MS = 5 * 60 * 1000;       // 5 menit sirene aktif
const unsigned long POST_SIREN_COOLDOWN_MS = 1 * 60 * 1000;  // 1 menit tunggu sebelum sirene nyala lagi
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;         
const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;          
const unsigned long TELEGRAM_POLL_INTERVAL_MS = 1000;     
// =============================================================

CRGB leds[NUM_LEDS];
WiFiClient wifiClient; 
MqttClient mqttClient(wifiClient);
WiFiClientSecure telegramSecureClient; 
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, telegramSecureClient);

enum SystemState {
    STATE_BOOTING, STATE_WIFI_CONFIGURING, STATE_WIFI_CONNECTING, STATE_MQTT_CONNECTING,
    STATE_IDLE_DISARMED, STATE_ARMED_WAITING, STATE_ALARM_TRIGGERED, STATE_POST_SIREN_COOLDOWN, // State baru
    STATE_FATAL_ERROR
};
SystemState currentSystemState = STATE_BOOTING;
SystemState previousSystemStateForLed = STATE_BOOTING; 

unsigned long armWindowStartTimeMs = 0; 
unsigned long sirenStartTimeMs = 0;
unsigned long postSirenCooldownStartTimeMs = 0; // Timer untuk cooldown 1 menit
unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastTelegramPollMs = 0;
String armedReasonPayload = ""; 
String activeAlarmReason = ""; 

bool isPayloadAlarmActive(String payload) {
    return (payload == "SMOKE_DETECTED" || 
            payload == "TEMP_HIGH" || 
            payload == "HUMIDITY_HIGH" || 
            payload == "SMOKE_DETECTED_IMAGE_REQ" || 
            payload.startsWith("ALARM_AKTIF"));
}

bool isPayloadConditionNormal(String payload) {
    return (payload == "SMOKE_NORMAL" || 
            payload == "TEMP_NORMAL" || 
            payload == "HUMIDITY_NORMAL" || 
            payload == "ALARM_NONAKTIF" || 
            payload.endsWith("_NORMAL"));
}

void updateLedIndicator() {
    bool forceUpdate = (currentSystemState != previousSystemStateForLed);
    if (!forceUpdate) {
        switch (currentSystemState) {
            case STATE_WIFI_CONNECTING: case STATE_MQTT_CONNECTING:
            case STATE_ARMED_WAITING: case STATE_ALARM_TRIGGERED: case STATE_POST_SIREN_COOLDOWN:
                forceUpdate = true; break;
            case STATE_IDLE_DISARMED:
                 if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
                    forceUpdate = true; 
                } break;
            default: break;
        }
    }
     if (!forceUpdate && currentSystemState == previousSystemStateForLed && currentSystemState == STATE_IDLE_DISARMED) {
        CRGB currentColor = leds[0]; CRGB expectedColor;
        if (mqttClient.connected() && WiFi.status() == WL_CONNECTED) expectedColor = CRGB::Green;
        else if (WiFi.status() == WL_CONNECTED) expectedColor = CRGB::Gold;
        else expectedColor = CRGB::DarkRed;
        if (currentColor != expectedColor) forceUpdate = true;
    }
    if (!forceUpdate) return; 

    switch (currentSystemState) {
        case STATE_BOOTING: leds[0] = CRGB::DarkSlateBlue; break;
        case STATE_WIFI_CONFIGURING: leds[0] = CRGB::Indigo; break;
        case STATE_WIFI_CONNECTING: leds[0] = CHSV(160, 255, 128 + (sin8(millis() / 20) - 128) / 2); break; 
        case STATE_MQTT_CONNECTING: leds[0] = CHSV(50, 255, 128 + (sin8(millis() / 20) - 128) / 2); break;  
        case STATE_IDLE_DISARMED:
            if (mqttClient.connected() && WiFi.status() == WL_CONNECTED) leds[0] = CRGB::Green; 
            else if (WiFi.status() == WL_CONNECTED) leds[0] = CRGB::Gold; 
            else leds[0] = CRGB::DarkRed; 
            break;
        case STATE_ARMED_WAITING: leds[0] = ((millis() / 700) % 2 == 0) ? CRGB::OrangeRed : CRGB::Black; break; 
        case STATE_ALARM_TRIGGERED: leds[0] = ((millis() / 150) % 2 == 0) ? CRGB::Red : CRGB::Black; break;       
        case STATE_POST_SIREN_COOLDOWN: leds[0] = ((millis() / 300) % 2 == 0) ? CRGB::Yellow : CRGB::DarkGoldenrod; break; // Kuning berkedip sedang
        case STATE_FATAL_ERROR: leds[0] = CRGB::DeepPink; break; 
        default: leds[0] = CRGB::Black; break;
    }
    FastLED.show();
    previousSystemStateForLed = currentSystemState;
}

void sendTelegramMessage(String message, bool addTimestamp = true) {
    if (WiFi.status() != WL_CONNECTED) { Serial.println("TELEGRAM: Tidak ada Wi-Fi untuk mengirim pesan."); return; }
    Serial.print("TELEGRAM: Mengirim pesan: "); Serial.println(message);
    if (!bot.sendMessage(AUTHORIZED_TELEGRAM_CHAT_ID, message, "")) { 
        Serial.println("TELEGRAM: Gagal mengirim pesan (polos).");
    } else {
        Serial.println("TELEGRAM: Pesan (polos) terkirim.");
    }
}

void setupWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;
    currentSystemState = STATE_WIFI_CONFIGURING; updateLedIndicator();
    Serial.println("WIFI: Mengkonfigurasi dengan IP Statis...");
    if (!WiFi.config(LOCAL_IP_ALARM, GATEWAY_IP_ALARM, SUBNET_MASK_ALARM, PRIMARY_DNS_ALARM, SECONDARY_DNS_ALARM)) {
        Serial.println("WIFI: Konfigurasi IP Statis GAGAL!");
        currentSystemState = STATE_IDLE_DISARMED; updateLedIndicator(); return;
    }
    Serial.println("WIFI: Konfigurasi IP Statis BERHASIL.");
    currentSystemState = STATE_WIFI_CONNECTING; updateLedIndicator();
    Serial.print("WIFI: Menghubungkan ke: "); Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttemptMs = millis();
}

void onMqttMessage(int messageSize); 

void setupMQTT() {
    if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return;
    currentSystemState = STATE_MQTT_CONNECTING; updateLedIndicator();
    Serial.print("MQTT: Menghubungkan ke Broker: "); Serial.println(MQTT_BROKER);
    mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASS);
    mqttClient.setId(MQTT_CLIENT_ID_ALARM_DEVICE);
    mqttClient.onMessage(onMqttMessage);

    if (mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
        Serial.println("MQTT: Terhubung ke Broker.");
        sendTelegramMessage("ALARM DEVICE:  Terhubung ke MQTT Broker (" + String(MQTT_BROKER) + ").");
        bool subStatusOk = mqttClient.subscribe(MQTT_TOPIC_SENSOR_ALARM_STATUS);
        if(subStatusOk) { Serial.println("MQTT: Berhasil subscribe ke topik status sensor."); }
        else { Serial.println("MQTT: Gagal subscribe ke topik status sensor."); sendTelegramMessage("ALARM DEVICE: ⚠️ Gagal subscribe topik MQTT status sensor!"); }
        if (currentSystemState == STATE_MQTT_CONNECTING) { currentSystemState = STATE_IDLE_DISARMED; }
    } else {
        Serial.print("MQTT: Koneksi Gagal. Error: "); Serial.println(mqttClient.connectError());
        sendTelegramMessage("ALARM DEVICE:  Gagal terhubung ke MQTT Broker. Error: " + String(mqttClient.connectError()));
        if (currentSystemState == STATE_MQTT_CONNECTING) { currentSystemState = STATE_IDLE_DISARMED; }
    }
    lastMqttAttemptMs = millis(); updateLedIndicator();
}

void onMqttMessage(int messageSize) {
    String topic = mqttClient.messageTopic();
    Serial.print("MQTT_RX: Pesan diterima ["); Serial.print(topic); Serial.print("], ukuran: "); Serial.println(messageSize);

    if (topic == MQTT_TOPIC_SENSOR_ALARM_STATUS) {
        String payload = ""; char tempBuf[100]; 
        int len = mqttClient.readBytes(tempBuf, min(messageSize, (int)sizeof(tempBuf)-1));
        tempBuf[len] = '\0'; payload = String(tempBuf);
        while(mqttClient.available() && messageSize > len) { mqttClient.read(); messageSize--; }
        Serial.print("MQTT_RX: Payload Status: "); Serial.println(payload);

        if (isPayloadAlarmActive(payload)) { 
            String specificAlarmType = payload;
            if (payload == "SMOKE_DETECTED" || payload == "SMOKE_DETECTED_IMAGE_REQ") { specificAlarmType = "ASAP TERDETEKSI"; }
            else if (payload == "TEMP_HIGH") { specificAlarmType = "SUHU TINGGI"; }
            else if (payload == "HUMIDITY_HIGH") { specificAlarmType = "KELEMBAPAN TINGGI"; }

            String alertMessage = "ALARM DEVICE: Sistem SIAGA! Sensor mendeteksi: *" + specificAlarmType + "*. Aktifkan Break Glass untuk sirene (siaga 5 mnt).";
            
            if (currentSystemState != STATE_ALARM_TRIGGERED) { // Jangan ubah state jika sirene sudah aktif karena break glass
                if (currentSystemState != STATE_ARMED_WAITING || armedReasonPayload != payload) {
                    sendTelegramMessage(alertMessage); 
                } else if (currentSystemState == STATE_ARMED_WAITING && armedReasonPayload == payload) {
                    Serial.println("MQTT_RX: Menerima payload alarm yang sama saat sudah SIAGA. Timer window siaga direset.");
                    sendTelegramMessage("ALARM DEVICE: Peringatan SIAGA ("+ armedReasonPayload +") dikonfirmasi ulang. Timer siaga direset.");
                }
                
                Serial.println("MQTT_RX: Status Sensor PERINGATAN (" + payload + "). Mengaktifkan/memperbarui ARMED WINDOW.");
                currentSystemState = STATE_ARMED_WAITING;
                armWindowStartTimeMs = millis(); 
                armedReasonPayload = payload;    
                Serial.print("MQTT_RX: armWindowStartTimeMs diatur ke: "); Serial.println(armWindowStartTimeMs);
                Serial.print("MQTT_RX: Armed karena: "); Serial.println(armedReasonPayload);
            } else { // Sistem sudah dalam STATE_ALARM_TRIGGERED (kemungkinan karena break glass)
                 Serial.println("MQTT_RX: Sistem sudah dalam STATE_ALARM_TRIGGERED. Pesan peringatan (" + payload + ") dari sensor dicatat.");
                 sendTelegramMessage("ALARM DEVICE: INFO TAMBAHAN: Sensor juga mendeteksi: *" + specificAlarmType + "* saat alarm utama (sirene) sedang aktif karena " + activeAlarmReason + ".");
            }

        } else if (isPayloadConditionNormal(payload)) {
            Serial.print("MQTT_RX: Diterima payload kondisi normal: "); Serial.println(payload);
            sendTelegramMessage("ALARM DEVICE: Info Sensor: Kondisi " + payload + " terdeteksi NORMAL oleh sensor. Alarm utama mungkin masih aktif, periksa status atau gunakan /siren_off.");
            // Pesan normal TIDAK LAGI men-disarm sistem secara otomatis.
        }
    } 
    updateLedIndicator();
}

void handleTelegramCommands() {
    if (WiFi.status() != WL_CONNECTED) return;
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    for (int i = 0; i < numNewMessages; i++) {
        telegramMessage &msg = bot.messages[i];
        Serial.println("TELEGRAM_RX: Menerima pesan:");
        Serial.print("  Chat ID: "); Serial.println(msg.chat_id);
        Serial.print("  Teks: "); Serial.println(msg.text);
        if (msg.chat_id == AUTHORIZED_TELEGRAM_CHAT_ID) {
            if (msg.text.equalsIgnoreCase("/siren_off")) {
                Serial.println("TELEGRAM_RX: Perintah /siren_off diterima.");
                sendTelegramMessage("ALARM DEVICE:  Sirene dimatikan dan sistem di-disarm via Telegram.", false);
                currentSystemState = STATE_IDLE_DISARMED;
                digitalWrite(MOSFET_PIN, LOW); 
                armWindowStartTimeMs = 0; sirenStartTimeMs = 0; armedReasonPayload = ""; activeAlarmReason = "";
            } else if (msg.text.equalsIgnoreCase("/status")) {
                Serial.println("TELEGRAM_RX: Perintah /status diterima.");
                String statusMsg = " *STATUS SISTEM ALARM*:\n";
                statusMsg += "Koneksi Wi-Fi: " + String(WiFi.status() == WL_CONNECTED ? "Terhubung (`" + WiFi.localIP().toString() + "`)" : "`Terputus`") + "\n";
                statusMsg += "Koneksi MQTT: " + String(mqttClient.connected() ? "Terhubung ke `" + String(MQTT_BROKER) + "`" : "`Terputus`") + "\n";
                statusMsg += "Mode Sistem: `";
                switch(currentSystemState) {
                    case STATE_IDLE_DISARMED: statusMsg += "IDLE / DISARMED"; break;
                    case STATE_ARMED_WAITING: 
                        statusMsg += "SIAGA (Menunggu Break Glass";
                        if (armedReasonPayload.length() > 0) statusMsg += " karena " + armedReasonPayload;
                        statusMsg += ").";
                        if (armWindowStartTimeMs != 0 && (millis() - armWindowStartTimeMs < ARMED_WINDOW_DURATION_MS) ) {
                             statusMsg += "\n  Sisa waktu siaga: " + String((ARMED_WINDOW_DURATION_MS - (millis() - armWindowStartTimeMs)) / 1000) + " dtk.";
                        } else if (armWindowStartTimeMs != 0) { statusMsg += "\n  Timer siaga berakhir."; }
                        break;
                    case STATE_ALARM_TRIGGERED: 
                        statusMsg += "ALARM AKTIF";
                        if (activeAlarmReason.length() > 0) statusMsg += " karena " + activeAlarmReason;
                        statusMsg += "!";
                        if (digitalRead(MOSFET_PIN) == HIGH) {
                            statusMsg += " Sirene BERBUNYI.";
                            if (sirenStartTimeMs != 0 && (millis() - sirenStartTimeMs < SIREN_DURATION_MS)) {
                                statusMsg += "\n  Sisa waktu sirene: " + String((SIREN_DURATION_MS - (millis() - sirenStartTimeMs)) / 1000) + " dtk.";
                            } else if (sirenStartTimeMs != 0) { statusMsg += "\n  Timer sirene berakhir."; }
                        } else { statusMsg += " Sirene MATI (dalam cooldown)."; }
                        break;
                    case STATE_POST_SIREN_COOLDOWN:
                        statusMsg += "SIRENE COOLDOWN (Menunggu 1 menit";
                        if (activeAlarmReason.length() > 0) statusMsg += " karena " + activeAlarmReason + " belum direset";
                        statusMsg += ").";
                         if (postSirenCooldownStartTimeMs != 0 && (millis() - postSirenCooldownStartTimeMs < POST_SIREN_COOLDOWN_MS) ) {
                             statusMsg += "\n  Sisa waktu cooldown: " + String((POST_SIREN_COOLDOWN_MS - (millis() - postSirenCooldownStartTimeMs)) / 1000) + " dtk.";
                        }
                        break;
                    default: statusMsg += String(currentSystemState); break; 
                }
                statusMsg += "`"; bot.sendMessage(msg.chat_id, statusMsg, "Markdown");
            }
        } else {
            Serial.println("TELEGRAM_RX: Pesan dari Chat ID tidak diotorisasi, diabaikan.");
            bot.sendMessage(msg.chat_id, "Maaf, Anda tidak diotorisasi untuk menggunakan bot ini.", "");
        }
        bot.last_message_received = msg.update_id; 
    }
}

void setup() {
    Serial.begin(115200);
    unsigned long bootStartTime = millis();
    while(!Serial && (millis() - bootStartTime < 3000)); 
    Serial.println("\n\n[Alarm Handler Device] Booting (Revised Siren Logic)...");

    pinMode(MOSFET_PIN, OUTPUT); pinMode(BREAK_GLASS_PIN, INPUT_PULLUP); 
    digitalWrite(MOSFET_PIN, LOW); 
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS); FastLED.setBrightness(20); 
    currentSystemState = STATE_BOOTING; updateLedIndicator(); delay(500);
    
    #ifdef ARDUINO_ARCH_ESP32
    telegramSecureClient.setInsecure(); Serial.println("Telegram Client diatur ke insecure.");
    #endif
    sendTelegramMessage("ALARM DEVICE:  Memulai booting (Logika Sirene Direvisi)...");
    setupWiFi(); 
    updateLedIndicator(); 
}

void loop() {
    unsigned long currentTimeMs = millis();
    updateLedIndicator(); 
    if (currentSystemState == STATE_FATAL_ERROR) { delay(10000); return; }

    // Manajemen Koneksi Wi-Fi & MQTT (Sama seperti sebelumnya)
    if (WiFi.status() != WL_CONNECTED) {
        if (currentSystemState != STATE_WIFI_CONNECTING && currentSystemState != STATE_WIFI_CONFIGURING) {
            if(currentSystemState != STATE_BOOTING && previousSystemStateForLed != STATE_WIFI_CONNECTING && previousSystemStateForLed != STATE_WIFI_CONFIGURING) {}
            currentSystemState = STATE_WIFI_CONNECTING; 
        }
        if (currentTimeMs - lastWifiAttemptMs >= WIFI_RETRY_INTERVAL_MS) { Serial.println("LOOP: Wi-Fi terputus. Mencoba koneksi ulang..."); setupWiFi(); }
    } else { 
        if (currentSystemState == STATE_WIFI_CONNECTING || currentSystemState == STATE_WIFI_CONFIGURING) {
            Serial.println("LOOP: Wi-Fi berhasil terhubung. IP: " + WiFi.localIP().toString());
            if (previousSystemStateForLed == STATE_WIFI_CONNECTING || previousSystemStateForLed == STATE_WIFI_CONFIGURING){ sendTelegramMessage("ALARM DEVICE: ✅ Wi-Fi terhubung dengan IP: " + WiFi.localIP().toString());}
            currentSystemState = STATE_MQTT_CONNECTING; lastMqttAttemptMs = 0; 
        }
        if (!mqttClient.connected()) {
            if (currentSystemState != STATE_MQTT_CONNECTING && currentSystemState != STATE_ARMED_WAITING && currentSystemState != STATE_ALARM_TRIGGERED && currentSystemState != STATE_POST_SIREN_COOLDOWN) { currentSystemState = STATE_MQTT_CONNECTING; }
            if (currentTimeMs - lastMqttAttemptMs >= MQTT_RETRY_INTERVAL_MS) { setupMQTT(); }
        } else { 
            mqttClient.poll(); 
            if (currentSystemState == STATE_MQTT_CONNECTING) {
                 if (currentSystemState != STATE_ARMED_WAITING && currentSystemState != STATE_ALARM_TRIGGERED && currentSystemState != STATE_POST_SIREN_COOLDOWN) { currentSystemState = STATE_IDLE_DISARMED; }
            }
        }
    }

    if (WiFi.status() == WL_CONNECTED && (currentTimeMs - lastTelegramPollMs >= TELEGRAM_POLL_INTERVAL_MS)) {
        handleTelegramCommands(); lastTelegramPollMs = currentTimeMs;
    }

    int breakGlassStatus = digitalRead(BREAK_GLASS_PIN); 

    switch (currentSystemState) {
        case STATE_IDLE_DISARMED:
            digitalWrite(MOSFET_PIN, LOW); 
            armWindowStartTimeMs = 0; sirenStartTimeMs = 0; armedReasonPayload = ""; activeAlarmReason = "";
            if (breakGlassStatus == LOW) { 
                Serial.println("LOOP_STATE: [IDLE] Break glass aktif! MENGAKTIFKAN ALARM!");
                sendTelegramMessage("ALARM DEVICE: ‼️ BREAK GLASS AKTIF! Sirene menyala.");
                currentSystemState = STATE_ALARM_TRIGGERED;
                sirenStartTimeMs = currentTimeMs; 
                digitalWrite(MOSFET_PIN, HIGH);   
                activeAlarmReason = "BREAK_GLASS_LANGSUNG";
            }
            break;

        case STATE_ARMED_WAITING:
            digitalWrite(MOSFET_PIN, LOW); 
            if (armWindowStartTimeMs != 0 && (currentTimeMs - armWindowStartTimeMs >= ARMED_WINDOW_DURATION_MS)) {
                Serial.println("LOOP_STATE: [ARMED_WAITING] Timer 5 menit berakhir. Sistem DISARMED.");
                sendTelegramMessage("ALARM DEVICE:  Window siaga 5 menit berakhir tanpa aktivasi break glass (Pemicu: " + armedReasonPayload + "). Sistem DISARMED.");
                currentSystemState = STATE_IDLE_DISARMED; armWindowStartTimeMs = 0; armedReasonPayload = ""; activeAlarmReason = "";
            } else if (breakGlassStatus == LOW) { 
                Serial.println("LOOP_STATE: [ARMED_WAITING] Break glass aktif! MENGAKTIFKAN ALARM!");
                sendTelegramMessage("ALARM DEVICE: ‼️ BREAK GLASS AKTIF saat sistem SIAGA (karena " + armedReasonPayload + ")! Sirene menyala.");
                currentSystemState = STATE_ALARM_TRIGGERED; sirenStartTimeMs = currentTimeMs; 
                digitalWrite(MOSFET_PIN, HIGH); armWindowStartTimeMs = 0; 
                activeAlarmReason = "BREAK_GLASS (saat siaga " + armedReasonPayload + ")"; 
                armedReasonPayload = ""; 
            }
            break;

        case STATE_ALARM_TRIGGERED:
            if (digitalRead(MOSFET_PIN) == HIGH) { // Jika sirene sedang menyala
                if (sirenStartTimeMs != 0 && (currentTimeMs - sirenStartTimeMs >= SIREN_DURATION_MS)) {
                    digitalWrite(MOSFET_PIN, LOW); 
                    Serial.println("LOOP_STATE: [ALARM_TRIGGERED] Durasi sirene 5 menit berakhir. Sirene OFF.");
                    sendTelegramMessage("ALARM DEVICE:  Durasi sirene 5 menit berakhir. Sirene dimatikan.");
                    
                    // Periksa apakah break glass masih aktif untuk masuk ke mode cooldown
                    if (digitalRead(BREAK_GLASS_PIN) == LOW) {
                        Serial.println("LOOP_STATE: Break glass masih aktif. Masuk ke mode cooldown 1 menit.");
                        sendTelegramMessage("ALARM DEVICE: Sirene cooldown 1 menit karena break glass masih aktif. Reset break glass atau gunakan /siren_off.");
                        currentSystemState = STATE_POST_SIREN_COOLDOWN;
                        postSirenCooldownStartTimeMs = currentTimeMs;
                    } else { // Break glass sudah tidak aktif
                        sendTelegramMessage("ALARM DEVICE: Sistem DISARMED (setelah timeout sirene & break glass normal).");
                        currentSystemState = STATE_IDLE_DISARMED; 
                        sirenStartTimeMs = 0; armWindowStartTimeMs = 0; armedReasonPayload = ""; activeAlarmReason = "";
                    }
                }
            }
            // Jika break glass direset SAAT sirene AKTIF (sebelum timeout sirene)
            if (breakGlassStatus == HIGH && digitalRead(MOSFET_PIN) == HIGH && sirenStartTimeMs != 0) { 
                Serial.println("LOOP_STATE: [ALARM_TRIGGERED] Break glass kembali normal (HIGH) saat sirene aktif.");
                sendTelegramMessage("ALARM DEVICE:  Break glass telah di-reset. Sirene dimatikan. Sistem DISARMED.");
                digitalWrite(MOSFET_PIN, LOW); 
                currentSystemState = STATE_IDLE_DISARMED;
                sirenStartTimeMs = 0; armWindowStartTimeMs = 0; armedReasonPayload = ""; activeAlarmReason = "";
            }
            break;

        case STATE_POST_SIREN_COOLDOWN:
            digitalWrite(MOSFET_PIN, LOW); // Pastikan sirene mati selama cooldown
            if (breakGlassStatus == HIGH) { // Jika break glass direset selama cooldown
                Serial.println("LOOP_STATE: [COOLDOWN] Break glass direset. Sistem DISARMED.");
                sendTelegramMessage("ALARM DEVICE:  Break glass direset selama cooldown. Sistem DISARMED.");
                currentSystemState = STATE_IDLE_DISARMED;
                sirenStartTimeMs = 0; armWindowStartTimeMs = 0; armedReasonPayload = ""; activeAlarmReason = ""; postSirenCooldownStartTimeMs = 0;
            } else if (currentTimeMs - postSirenCooldownStartTimeMs >= POST_SIREN_COOLDOWN_MS) {
                Serial.println("LOOP_STATE: [COOLDOWN] Cooldown 1 menit berakhir, break glass masih aktif. AKTIFKAN SIRENE KEMBALI.");
                sendTelegramMessage("ALARM DEVICE: ‼️ Cooldown berakhir, break glass masih aktif! Sirene menyala kembali.");
                currentSystemState = STATE_ALARM_TRIGGERED;
                sirenStartTimeMs = currentTimeMs; // Mulai timer sirene 5 menit lagi
                digitalWrite(MOSFET_PIN, HIGH);
                // activeAlarmReason sudah diset sebelumnya dan tidak perlu diubah
                postSirenCooldownStartTimeMs = 0; // Reset timer cooldown
            }
            break;

        default:
            break;
    }
    delay(50); 
}