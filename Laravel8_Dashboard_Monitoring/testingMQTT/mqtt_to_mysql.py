# import paho.mqtt.client as mqtt
# import mysql.connector
# from datetime import datetime

# db = mysql.connector.connect(
#     host="localhost",
#     user="root",
#     password="",
#     database="iot_monitoring"
# )
# cursor = db.cursor()

# # Simpan data sementara
# current_data = {"suhu": None, "kelembapan": None, "asap": None}

# def save_realtime():
#     now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
#     cursor.execute("DELETE FROM sensor_realtime")  # hanya 1 row
#     cursor.execute(
#         "INSERT INTO sensor_realtime (suhu, kelembapan, asap, created_at) VALUES (%s, %s, %s, %s)",
#         (current_data["suhu"], current_data["kelembapan"], current_data["asap"], now)
#     )
#     db.commit()

# def save_history():
#     now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
#     cursor.execute(
#         "INSERT INTO sensor_history (suhu, kelembapan, asap, created_at) VALUES (%s, %s, %s, %s)",
#         (current_data["suhu"], current_data["kelembapan"], current_data["asap"], now)
#     )
#     db.commit()

# def on_connect(client, userdata, flags, rc):
#     print("Connected with result code " + str(rc))
#     if rc == 0:
#         if flags.get("session present") == 0:
#             print("Session baru, perlu subscribe manual.")
#             client.subscribe("sensor/#", qos=1)
#         else:
#             print("Session lama dipulihkan, tidak perlu subscribe.")

# def on_message(client, userdata, msg):
#     topic = msg.topic
#     payload = msg.payload.decode()
#     print(f"{topic} => {payload}")

#     if topic == "sensor/suhu":
#         current_data["suhu"] = float(payload)
#     elif topic == "sensor/kelembapan":
#         current_data["kelembapan"] = float(payload)
#     elif topic == "sensor/asap":
#         current_data["asap"] = int(payload)

#     if all(value is not None for value in current_data.values()):
#         save_realtime()
#         save_history()
#         current_data["suhu"] = None
#         current_data["kelembapan"] = None
#         current_data["asap"] = None

# client = mqtt.Client(client_id="mqtt_listener", clean_session=False)
# client.username_pw_set("testing", "12345")
# client.on_connect = on_connect
# client.on_message = on_message
# client.connect("192.168.14.100", 1883, 60)

# print("Listening for MQTT messages...")
# client.loop_forever()

#==================//////////////////////////program 2
# import paho.mqtt.client as mqtt
# import mysql.connector
# from datetime import datetime
# import time

# # Koneksi database
# db = mysql.connector.connect(
#     host="localhost",
#     user="root",
#     password="",
#     database="iot_monitoring"
# )
# cursor = db.cursor()

# # Simpan data sementara
# current_data = {"suhu": None, "kelembapan": None, "asap": None}

# def save_realtime():
#     now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
#     cursor.execute("DELETE FROM sensor_realtime")  # hanya 1 row
#     cursor.execute(
#         "INSERT INTO sensor_realtime (suhu, kelembapan, asap, created_at) VALUES (%s, %s, %s, %s)",
#         (current_data["suhu"], current_data["kelembapan"], current_data["asap"], now)
#     )
#     db.commit()

# def save_history():
#     now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
#     cursor.execute(
#         "INSERT INTO sensor_history (suhu, kelembapan, asap, created_at) VALUES (%s, %s, %s, %s)",
#         (current_data["suhu"], current_data["kelembapan"], current_data["asap"], now)
#     )
#     db.commit()

# def on_connect(client, userdata, flags, rc):
#     print("Connected with result code", rc)
#     if rc == 0:
#         if flags.get("session present") == 0:
#             print("Session baru, subscribe manual...")
#             client.subscribe("sensor/#", qos=1)
#         else:
#             print("Session lama dipulihkan, tidak perlu subscribe.")
#     else:
#         print("Gagal koneksi ke broker, kode:", rc)

# def on_disconnect(client, userdata, rc):
#     print("Terputus dari broker MQTT. Kode:", rc)
#     while rc != 0:
#         try:
#             rc = client.reconnect()
#             print("Berhasil reconnect ke broker MQTT")
#         except:
#             print("Gagal reconnect. Coba lagi...")
#             time.sleep(5)

# def on_message(client, userdata, msg):
#     topic = msg.topic
#     payload = msg.payload.decode()
#     print(f"{topic} => {payload}")

#     try:
#         if topic == "sensor/suhu":
#             current_data["suhu"] = float(payload)
#         elif topic == "sensor/kelembapan":
#             current_data["kelembapan"] = float(payload)
#         elif topic == "sensor/asap":
#             current_data["asap"] = int(payload)
#     except ValueError:
#         print(f" Payload tidak valid dari {topic}: {payload}")
#         return

#     if all(value is not None for value in current_data.values()):
#         try:
#             save_realtime()
#             save_history()
#             print(" Data disimpan ke database")
#         except mysql.connector.Error as err:
#             print(f" Database error: {err}")
#         finally:
#             current_data["suhu"] = None
#             current_data["kelembapan"] = None
#             current_data["asap"] = None

# # Inisialisasi client
# client = mqtt.Client(client_id="mqtt_listener", clean_session=False)
# client.username_pw_set("testing", "12345")
# client.on_connect = on_connect
# client.on_message = on_message
# client.on_disconnect = on_disconnect

# client.connect("192.168.14.100", 1883, 60)

# print("Listening for MQTT messages...")
# client.loop_forever()

#====================/////////////////////program 3 add interval 8 detik + control data burst
# import paho.mqtt.client as mqtt
# import mysql.connector
# from datetime import datetime, timedelta
# import time
# import threading
# import queue
# import bisect
# import signal
# import sys

# # ================= CONFIGURATION =================
# MQTT_BROKER = "192.168.14.101"
# MQTT_PORT = 1883
# MQTT_USER = "testing"
# MQTT_PASS = "12345"
# TOPICS = ["sensor/suhu", "sensor/kelembapan", "sensor/asap"]

# DB_CONFIG = {
#     "host": "localhost",
#     "user": "root",
#     "password": "",
#     "database": "iot_monitoring",
#     "autocommit": True
# }

# DATA_INTERVAL = 8  # Interval pengiriman data (detik)
# BURST_TIMEOUT = 1.5 * DATA_INTERVAL  # 12 detik untuk interval 8 detik

# # ================= MAIN PROGRAM =================
# class StableMQTTListener:
#     def __init__(self):
#         # Database
#         self.db = mysql.connector.connect(**DB_CONFIG)
#         self.cursor = self.db.cursor()
        
#         # Data state
#         self.current_data = {"suhu": None, "kelembapan": None, "asap": None}
#         self.burst_queue = queue.Queue()
#         self.burst_mode = False
#         self.last_normal_time = None
#         self.lock = threading.Lock()
        
#         # MQTT Client (Callback API V2)
#         self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, 
#                                 client_id="stable_listener_v1",
#                                 clean_session=False)
#         self.client.username_pw_set(MQTT_USER, MQTT_PASS)
#         self.client.on_connect = self._on_connect
#         self.client.on_message = self._on_message
#         self.client.on_disconnect = self._on_disconnect

#         # Signal handler
#         signal.signal(signal.SIGINT, self._signal_handler)

#     # ================= MQTT HANDLERS =================
#     def _on_connect(self, client, userdata, flags, reason_code, properties):
#         if reason_code == 0:
#             print(" Connected to MQTT Broker")
#             for topic in TOPICS:
#                 client.subscribe(topic, qos=1)
            
#             with self.lock:
#                 self.burst_mode = True  # Masuk mode burst setelah koneksi
#         else:
#             print(f"Connection failed. Code: {reason_code}")

#     def _on_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
#         print(f" Disconnected. Reason: {reason_code}")
#         with self.lock:
#             self.burst_mode = False

#     def _on_message(self, client, userdata, msg):
#         try:
#             value = float(msg.payload.decode())
            
#             with self.lock:
#                 # Update data saat ini
#                 if "suhu" in msg.topic:
#                     self.current_data["suhu"] = value
#                 elif "kelembapan" in msg.topic:
#                     self.current_data["kelembapan"] = value
#                 elif "asap" in msg.topic:
#                     self.current_data["asap"] = value
                
#                 # Cek kelengkapan data
#                 if all(v is not None for v in self.current_data.values()):
#                     if self.burst_mode:
#                         self._handle_burst_data()
#                     else:
#                         self._handle_normal_data()
                    
#                     # Reset data
#                     self.current_data = {"suhu": None, "kelembapan": None, "asap": None}
                        
#         except Exception as e:
#             print(f"Error processing message: {e}")

#     # ================= DATA PROCESSING =================
#     def _handle_burst_data(self):
#         """Simpan data ke antrian burst dan mulai timer"""
#         self.burst_queue.put(self.current_data.copy())
#         print(f"Burst data buffered (queue: {self.burst_queue.qsize()})")
        
#         # Jadwalkan pengecekan akhir burst (hanya sekali)
#         if not hasattr(self, 'burst_timer'):
#             self.burst_timer = threading.Timer(BURST_TIMEOUT, self._process_burst)
#             self.burst_timer.start()

#     def _handle_normal_data(self):
#         """Simpan data normal langsung ke database"""
#         timestamp = datetime.now()
#         self._save_to_database(self.current_data.copy(), timestamp)
#         self.last_normal_time = timestamp
#         print(f" Normal data saved at {timestamp}")

#     def _process_burst(self):
#         """Proses semua data burst dengan timestamp mundur"""
#         with self.lock:
#             if self.burst_queue.empty():
#                 self.burst_mode = False
#                 return
                
#             burst_size = self.burst_queue.qsize()
#             print(f" Processing {burst_size} burst messages...")
            
#             # Base time: waktu sekarang atau last_normal + interval
#             base_time = datetime.now() if not self.last_normal_time else \
#                       self.last_normal_time + timedelta(seconds=DATA_INTERVAL)
            
#             # Proses dari yang terlama (FIFO)
#             for i in range(burst_size):
#                 data = self.burst_queue.get()
#                 timestamp = base_time - timedelta(seconds=(burst_size - i - 1) * DATA_INTERVAL)
#                 self._save_to_database(data, timestamp)
#                 print(f"Burst data {i+1}/{burst_size} at {timestamp}")
            
#             # Keluar dari mode burst
#             self.burst_mode = False
#             del self.burst_timer

#     # ================= DATABASE OPERATIONS =================
#     def _save_to_database(self, data, timestamp):
#         try:
#             # Cari posisi timestamp yang benar
#             self.cursor.execute("SELECT created_at FROM sensor_history ORDER BY created_at")
#             existing_timestamps = [row[0] for row in self.cursor.fetchall()]
#             insert_pos = bisect.bisect_right(existing_timestamps, timestamp)
            
#             # save to database
#             self.cursor.execute("DELETE FROM sensor_realtime")
#             self.cursor.execute(
#                 """INSERT INTO sensor_realtime 
#                 (suhu, kelembapan, asap, created_at) 
#                 VALUES (%s, %s, %s, %s)""",
#                 (data["suhu"], data["kelembapan"], data["asap"], timestamp)
#             )
#             self.cursor.execute(
#                 """INSERT INTO sensor_history 
#                 (suhu, kelembapan, asap, created_at) 
#                 VALUES (%s, %s, %s, %s)""",
#                 (data["suhu"], data["kelembapan"], data["asap"], timestamp)
#             )
            
#             print(f"Saved: {timestamp} (pos: {insert_pos})")
            
#         except Exception as e:
#             print(f"Database error: {e}")
#             self.db.rollback()

#     # ================= SYSTEM CONTROL =================
#     def _signal_handler(self, sig, frame):
#         """Tangani shutdown dengan CTRL+C"""
#         print("Shutting down ")
#         self.client.disconnect()
#         self.cursor.close()
#         self.db.close()
#         sys.exit(0)

#     def start(self):
#         """Mulai listener"""
#         self.client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        
#         # Thread untuk MQTT loop
#         mqtt_thread = threading.Thread(target=self.client.loop_forever)
#         mqtt_thread.daemon = True
#         mqtt_thread.start()
        
#         print("Listener started. Press Ctrl+C to exit.")
#         try:
#             while True:
#                 time.sleep(1)
#         except KeyboardInterrupt:
#             self._signal_handler(None, None)

# if __name__ == "__main__":
#     listener = StableMQTTListener()
#     listener.start()


# listenere test 24 jam 
import paho.mqtt.client as mqtt
import mysql.connector
from datetime import datetime, timedelta, time as dt_time
import time
import threading
import signal
import sys
import json # Untuk debugging atau jika perlu

# ================= CONFIGURATION =================
MQTT_BROKER = "192.168.14.101" # Pastikan ini benar, sesuai dengan IP PC/Laptop Anda
MQTT_PORT = 1883
MQTT_USER = "testing"
MQTT_PASS = "12345"
TOPICS = ["sensor/suhu", "sensor/kelembapan", "sensor/asap"]

DB_CONFIG = {
    "host": "localhost",
    "user": "root",
    "password": "",
    "database": "iot_monitoring",
    "autocommit": False 
}

HOURLY_CHECK_INTERVAL_SECONDS = 60 # Cek setiap 1 menit jika ada jam yang perlu diproses

# ================= MAIN PROGRAM =================
import paho.mqtt.client as mqtt
import mysql.connector
from datetime import datetime, timedelta, time as dt_time
import time
import threading
import signal
import sys
import json # Tidak digunakan secara langsung, tapi bisa berguna untuk debugging

# ================= KONFIGURASI =================
MQTT_BROKER = "192.168.14.101" # Ganti dengan IP Broker MQTT Anda
MQTT_PORT = 1883
MQTT_USER = "testing"
MQTT_PASS = "12345"
TOPICS = ["sensor/suhu", "sensor/kelembapan", "sensor/asap"]

DB_CONFIG = {
    "host": "localhost",
    "user": "root",
    "password": "",
    "database": "iot_monitoring", # Pastikan nama database Anda benar
    "autocommit": False # Kita akan commit secara eksplisit
}

# Interval untuk pengecekan periodik pemrosesan data per jam (misalnya, setiap 5 menit)
# Ini untuk memastikan data diproses bahkan jika tidak ada pesan MQTT baru yang masuk tepat di pergantian jam.
HOURLY_CHECK_INTERVAL_SECONDS = 5 * 60 

# ================= PROGRAM UTAMA =================
class MQTTToMySQLAndHourlyAggregator:
    def __init__(self):
        self.db_connection = None
        self.db_cursor = None
        self._connect_db()

        # Buffer untuk data yang masuk per topik sebelum membentuk satu set lengkap
        self._current_set_data = {"suhu": None, "kelembapan": None, "asap": None}
        
        # Akumulator untuk data per jam
        self.hourly_data_accumulator = {
            "suhu": [],
            "kelembapan": [],
            "asap": []
        }
        self.current_processing_hour_start = None 
        self.lock = threading.Lock() # Lock untuk akses ke shared data
        
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, 
                                client_id="py_hourly_avg_aggregator_v1", # ID Klien unik
                                clean_session=True)
        self.client.username_pw_set(MQTT_USER, MQTT_PASS)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect

        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)

        self.shutdown_event = threading.Event()
        self.hourly_processor_thread = None

    def _connect_db(self):
        """Mencoba membuat atau menyambung ulang koneksi ke database."""
        try:
            if self.db_connection and self.db_connection.is_connected():
                try:
                    self.db_connection.close() 
                except Exception as e_close:
                    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Info: Error saat menutup koneksi DB lama: {e_close}")
            self.db_connection = mysql.connector.connect(**DB_CONFIG)
            self.db_cursor = self.db_connection.cursor()
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Berhasil terhubung/rekoneksi ke database.")
            return True
        except mysql.connector.Error as err:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Error koneksi database: {err}")
            self.db_connection = None
            self.db_cursor = None
            return False

    def _ensure_db_connection(self):
        """Memastikan koneksi database aktif, mencoba konek ulang jika perlu."""
        if self.db_connection is None or not self.db_connection.is_connected():
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Koneksi database terputus. Mencoba menghubungkan ulang...")
            return self._connect_db()
        try:
            self.db_cursor.execute("SELECT 1") # Query ringan untuk tes koneksi
            self.db_cursor.fetchone()
        except (mysql.connector.Error, AttributeError) as e: # AttributeError jika cursor None
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Koneksi database tidak valid ({e}). Mencoba menghubungkan ulang...")
            return self._connect_db()
        return True

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        """Callback saat terhubung ke MQTT Broker."""
        if reason_code == 0: # Koneksi berhasil
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Terhubung ke MQTT Broker: {MQTT_BROKER}")
            for topic in TOPICS:
                client.subscribe(topic, qos=1) # QOS 1 untuk "at least once" delivery
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Subscribe ke topik: {topic}")
        else:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Gagal terhubung ke MQTT Broker. Kode: {reason_code}")

    def _on_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        """Callback saat terputus dari MQTT Broker."""
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Terputus dari MQTT Broker. Kode: {reason_code}")

    def _on_message(self, client, userdata, msg):
        """Callback saat menerima pesan MQTT."""
        try:
            timestamp_received = datetime.now() # Gunakan waktu lokal PC untuk semua timestamp

            topic_parts = msg.topic.split('/')
            if len(topic_parts) < 2:
                print(f"[{timestamp_received.strftime('%Y-%m-%d %H:%M:%S')}] Topik tidak dikenal: {msg.topic}")
                return

            sensor_type = topic_parts[1]
            value_str = msg.payload.decode().strip() # Hilangkan spasi di awal/akhir
            
            try:
                value = float(value_str) # Semua data sensor diharapkan float dari ESP32
                if sensor_type == "asap": # Jika asap, mungkin ingin dibulatkan ke integer
                    value = int(value) 
            except ValueError:
                print(f"[{timestamp_received.strftime('%Y-%m-%d %H:%M:%S')}] Nilai tidak valid untuk {sensor_type} dari topik {msg.topic}: '{value_str}'")
                return

            with self.lock:
                # 1. Akumulasi data untuk statistik per jam
                current_hour_start = timestamp_received.replace(minute=0, second=0, microsecond=0)
                if self.current_processing_hour_start is None: 
                    self.current_processing_hour_start = current_hour_start
                    print(f"[{timestamp_received.strftime('%Y-%m-%d %H:%M:%S')}] Inisialisasi jam proses: {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')}")
                
                # Jika jam berganti, proses data jam sebelumnya
                if current_hour_start > self.current_processing_hour_start:
                    print(f"[{timestamp_received.strftime('%Y-%m-%d %H:%M:%S')}] Pergantian jam terdeteksi. Memproses data untuk jam: {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')}")
                    self._process_and_store_hourly_data(self.current_processing_hour_start)
                    # Reset accumulator untuk jam baru SETELAH data jam lama diproses
                    self.hourly_data_accumulator = {"suhu": [], "kelembapan": [], "asap": []} 
                    self.current_processing_hour_start = current_hour_start
                
                # Tambahkan data ke accumulator untuk jam yang sedang berjalan
                if sensor_type in self.hourly_data_accumulator:
                    self.hourly_data_accumulator[sensor_type].append(value)
                    # Log ini bisa sangat verbose, bisa dikomentari jika sudah berjalan baik
                    # print(f"[{timestamp_received.strftime('%Y-%m-%d %H:%M:%S')}] Data diterima: {sensor_type}={value}. Akumulasi untuk jam: {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')} (S:{len(self.hourly_data_accumulator['suhu'])},K:{len(self.hourly_data_accumulator['kelembapan'])},A:{len(self.hourly_data_accumulator['asap'])})")

                # 2. Update data sementara untuk set realtime/history saat ini
                if sensor_type in self._current_set_data:
                    self._current_set_data[sensor_type] = value
                
                # 3. Cek apakah semua data untuk satu "set" lengkap untuk disimpan ke realtime/history
                if all(val is not None for val in self._current_set_data.values()):
                    print(f"[{timestamp_received.strftime('%Y-%m-%d %H:%M:%S')}] Set data lengkap diterima untuk realtime/history: {self._current_set_data}")
                    self._save_to_realtime_and_history(self._current_set_data.copy(), timestamp_received) 
                    self._current_set_data = {"suhu": None, "kelembapan": None, "asap": None} # Reset

        except Exception as e:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Error memproses pesan: {e} - Topik: {msg.topic} Payload: {msg.payload}")

    def _save_to_realtime_and_history(self, data_set, timestamp_local):
        """Menyimpan satu set data lengkap ke tabel realtime dan history menggunakan waktu lokal."""
        if not self._ensure_db_connection():
            print(f"[{timestamp_local.strftime('%Y-%m-%d %H:%M:%S')}] Tidak bisa menyimpan data realtime/history, koneksi DB tidak ada.")
            return

        try:
            db_timestamp = timestamp_local.strftime('%Y-%m-%d %H:%M:%S')

            # Simpan ke sensor_realtime (hapus dulu, lalu insert)
            self.db_cursor.execute("DELETE FROM sensor_realtime")
            sql_realtime = "INSERT INTO sensor_realtime (suhu, kelembapan, asap, created_at) VALUES (%s, %s, %s, %s)"
            val_realtime = (data_set.get("suhu"), data_set.get("kelembapan"), data_set.get("asap"), db_timestamp)
            self.db_cursor.execute(sql_realtime, val_realtime)
            
            # Simpan ke sensor_history
            sql_history = "INSERT INTO sensor_history (suhu, kelembapan, asap, created_at) VALUES (%s, %s, %s, %s)"
            val_history = (data_set.get("suhu"), data_set.get("kelembapan"), data_set.get("asap"), db_timestamp)
            self.db_cursor.execute(sql_history, val_history)
            
            self.db_connection.commit()
            print(f"[{timestamp_local.strftime('%Y-%m-%d %H:%M:%S')}] Data disimpan ke realtime & history: {data_set} @ {db_timestamp}")

        except mysql.connector.Error as err:
            print(f"[{timestamp_local.strftime('%Y-%m-%d %H:%M:%S')}] Database error saat menyimpan realtime/history: {err}")
            try: self.db_connection.rollback()
            except Exception as e_rb: print(f"[{timestamp_local.strftime('%Y-%m-%d %H:%M:%S')}] Error saat rollback: {e_rb}")
        except Exception as e:
            print(f"[{timestamp_local.strftime('%Y-%m-%d %H:%M:%S')}] Error umum saat menyimpan realtime/history: {e}")

    def _process_and_store_hourly_data(self, hour_to_process_local):
        """Memproses data yang terakumulasi untuk satu jam dan menyimpannya sebagai rata-rata."""
        if not self._ensure_db_connection():
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Tidak bisa memproses data per jam, koneksi DB tidak ada.")
            return
        
        # Ambil data dari accumulator dengan aman (sebelum direset oleh _on_message)
        # Tidak perlu lock di sini jika pemanggilan sudah diatur dengan benar
        # (dipanggil dari _on_message sebelum reset, atau dari _periodic_hourly_processor dengan lock)
        
        avg_suhu, avg_kelembapan, avg_asap_float = None, None, None # Asap bisa float dulu untuk rata-rata
        
        suhu_values = self.hourly_data_accumulator["suhu"]
        kelembapan_values = self.hourly_data_accumulator["kelembapan"]
        asap_values = self.hourly_data_accumulator["asap"]

        points_suhu = len(suhu_values)
        points_kelembapan = len(kelembapan_values)
        points_asap = len(asap_values)
        
        data_points_count = max(points_suhu, points_kelembapan, points_asap, 0)

        if points_suhu > 0: avg_suhu = round(sum(suhu_values) / points_suhu, 2)
        if points_kelembapan > 0: avg_kelembapan = round(sum(kelembapan_values) / points_kelembapan, 2)
        if points_asap > 0: avg_asap_float = sum(asap_values) / points_asap
        
        # Bulatkan avg_asap ke integer jika kolom database adalah INT
        avg_asap_int = int(round(avg_asap_float)) if avg_asap_float is not None else None


        if data_points_count > 0:
            try:
                db_hour_timestamp = hour_to_process_local.strftime('%Y-%m-%d %H:00:00')
                sql = """INSERT INTO hourly_sensor_data 
                         (hour_timestamp, avg_suhu, avg_kelembapan, avg_asap, data_points_count) 
                         VALUES (%s, %s, %s, %s, %s)
                         ON DUPLICATE KEY UPDATE 
                         avg_suhu = VALUES(avg_suhu), avg_kelembapan = VALUES(avg_kelembapan),
                         avg_asap = VALUES(avg_asap), data_points_count = VALUES(data_points_count)"""
                val = (db_hour_timestamp, 
                       avg_suhu, avg_kelembapan, avg_asap_int, 
                       data_points_count)
                self.db_cursor.execute(sql, val)
                self.db_connection.commit()
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Data per jam (rata-rata) disimpan untuk {db_hour_timestamp}: Avg S:{avg_suhu}, K:{avg_kelembapan}, A:{avg_asap_int} (Points: {data_points_count})")
            except mysql.connector.Error as err:
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Database error saat menyimpan data per jam: {err}")
                try: self.db_connection.rollback()
                except Exception as e_rb: print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Error saat rollback: {e_rb}")
            except Exception as e:
                 print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Error umum saat menyimpan data per jam: {e}")
        else:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Tidak ada data terakumulasi untuk diproses pada jam {hour_to_process_local.strftime('%Y-%m-%d %H:00:00')}")

    def _periodic_hourly_processor(self):
        """Dipanggil oleh timer thread untuk memastikan data per jam diproses jika tidak ada pesan baru."""
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Thread _periodic_hourly_processor dimulai.")
        while not self.shutdown_event.is_set():
            now_local = datetime.now() 
            current_hour_start_local = now_local.replace(minute=0, second=0, microsecond=0)
            
            with self.lock:
                if self.current_processing_hour_start is None:
                    self.current_processing_hour_start = current_hour_start_local
                    print(f"[{now_local.strftime('%Y-%m-%d %H:%M:%S')}] Processor per jam diinisialisasi, mulai mengakumulasi untuk jam: {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')}")
                
                if current_hour_start_local > self.current_processing_hour_start:
                    # Cek apakah ada data di accumulator sebelum memanggil proses
                    if any(self.hourly_data_accumulator[key] for key in self.hourly_data_accumulator):
                        print(f"[{now_local.strftime('%Y-%m-%d %H:%M:%S')}] Pengecekan periodik: Memproses data sisa untuk jam {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')}")
                        self._process_and_store_hourly_data(self.current_processing_hour_start)
                    else:
                        print(f"[{now_local.strftime('%Y-%m-%d %H:%M:%S')}] Pengecekan periodik: Tidak ada data terakumulasi untuk jam {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')}")
                    
                    self.hourly_data_accumulator = {"suhu": [], "kelembapan": [], "asap": []}
                    self.current_processing_hour_start = current_hour_start_local
                    print(f"[{now_local.strftime('%Y-%m-%d %H:%M:%S')}] Pengecekan periodik: Mulai mengakumulasi untuk jam baru: {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')}")
            
            self.shutdown_event.wait(HOURLY_CHECK_INTERVAL_SECONDS) 
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Thread _periodic_hourly_processor berhenti.")

    def _signal_handler(self, sig, frame):
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Menerima sinyal shutdown ({sig}). Menutup koneksi...")
        self.shutdown_event.set() 
        
        if self.hourly_processor_thread is not None and self.hourly_processor_thread.is_alive():
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Menunggu timer pemrosesan per jam selesai...")
            self.hourly_processor_thread.join(timeout=HOURLY_CHECK_INTERVAL_SECONDS + 5) 
            if self.hourly_processor_thread.is_alive():
                 print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Timer pemrosesan per jam tidak berhenti dengan normal.")

        with self.lock: 
            if self.current_processing_hour_start and any(val_list for val_list in self.hourly_data_accumulator.values()):
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Memproses sisa data di accumulator untuk jam {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')} sebelum keluar...")
                self._process_and_store_hourly_data(self.current_processing_hour_start)

        if hasattr(self.client, 'is_connected') and self.client.is_connected():
            self.client.loop_stop(force=False) 
            self.client.disconnect()
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Klien MQTT terputus.")
        
        if self.db_cursor:
            try: self.db_cursor.close()
            except Exception as e: print(f"Error menutup cursor: {e}")
        if self.db_connection and self.db_connection.is_connected():
            try: self.db_connection.close()
            except Exception as e: print(f"Error menutup koneksi DB: {e}")
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Koneksi database ditutup.")
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Shutdown selesai.")
        sys.exit(0)

    def start(self):
        self._current_set_data = {"suhu": None, "kelembapan": None, "asap": None}
        self.current_processing_hour_start = datetime.now().replace(minute=0, second=0, microsecond=0) # Gunakan waktu lokal
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Listener dimulai. Akumulasi awal untuk jam: {self.current_processing_hour_start.strftime('%Y-%m-%d %H:00:00')}")
        
        self.hourly_processor_thread = threading.Thread(target=self._periodic_hourly_processor)
        self.hourly_processor_thread.daemon = True
        self.hourly_processor_thread.start()

        while not self.shutdown_event.is_set():
            try:
                if not (hasattr(self.client, 'is_connected') and self.client.is_connected()):
                    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Mencoba terhubung ke MQTT Broker: {MQTT_BROKER}...")
                    self.client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
                
                self.client.loop_start() 
                
                while self.client.is_connected() and not self.shutdown_event.is_set():
                    time.sleep(1) 
                
                if self.client.is_connected() and self.shutdown_event.is_set(): 
                    self.client.loop_stop(force=False)
                    self.client.disconnect()

            except ConnectionRefusedError:
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Koneksi ke MQTT ditolak. Mencoba lagi dalam 10 detik...")
            except OSError as e: 
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] OS Error saat koneksi MQTT: {e}. Mencoba lagi dalam 10 detik...")
            except KeyboardInterrupt: # Tangkap KeyboardInterrupt di loop utama juga
                self._signal_handler(signal.SIGINT, None)
                break # Keluar dari while loop utama
            except Exception as e:
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Error MQTT tidak diketahui: {e}. Mencoba lagi dalam 10 detik...")
            
            if not self.shutdown_event.is_set():
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Mencoba koneksi ulang MQTT dalam 10 detik...")
                self.shutdown_event.wait(10) 
        
        if not self.shutdown_event.is_set(): 
             self._signal_handler(signal.SIGTERM, None)

if __name__ == "__main__":
    listener = MQTTToMySQLAndHourlyAggregator()
    listener.start()