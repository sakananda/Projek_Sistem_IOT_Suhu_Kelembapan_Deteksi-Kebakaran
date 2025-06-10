# Implementasi Sistem Monitoring Suhu, Kelembapan, dan Peringatan Kebakaran Berbasis IoT

Proyek ini merupakan hasil dari Kerja Praktek di PT. XYZ, sebuah perusahaan manufaktur alat kesehatan. [cite_start]Proyek ini bertujuan untuk mengatasi tantangan operasional terkait keselamatan kerja dan efisiensi proses manual di area manufaktur dan gudang melalui penerapan teknologi Internet of Things (IoT).

## Deskripsi Proyek

[cite_start]Sistem ini dirancang untuk melakukan pemantauan suhu, kelembapan, dan deteksi potensi kebakaran (asap) secara *real-time*. [cite_start]Tujuannya adalah untuk meningkatkan keselamatan kerja dengan menyediakan data lingkungan yang akurat dan sistem peringatan dini otomatis, menggantikan proses pencatatan manual yang lambat dan tidak efisien.


## Fitur Utama

-   Monitoring suhu, kelembapan, dan asap (PPM) secara *real-time*.
-   [cite_start]Dashboard web untuk visualisasi data historis dan terkini.
-   [cite_start]Sistem alarm fisik yang terdiri dari sirine dan lampu indikator.
-   [cite_start]Input darurat manual melalui tombol *Emergency Break Glass*.
-   [cite_start]Notifikasi instan ke Telegram saat alarm terpicu atau sistem berubah status.
-   [cite_start]Pencatatan data secara persisten ke database MySQL, dengan agregasi data per jam.

## Teknologi & Komponen yang Digunakan

### Perangkat Keras
-   **Unit Sensor**:
    -   [cite_start]Mikrokontroler: **ESP32-S3 Wroom N16R8** 
    -   [cite_start]Sensor: **DFRobot SHT40** (Suhu & Kelembapan) dan **Gravity MQ2** (Asap/Gas) 
    -   [cite_start]Konektivitas: **W5500 Ethernet Module** 
    -   [cite_start]Display: **LCD OLED 1.3"** 
    -   [cite_start]Catu Daya: **Adapter 12V** dengan *backup* **Baterai 18650 BMS 3S** 
-   **Unit Kontrol Alarm**:
    -   [cite_start]Mikrokontroler: **YD ESP32-S3 N16R8** 
    -   [cite_start]Input: Tombol **Emergency Break Glass** 
    -   [cite_start]Output: **Sirine 12V** & **Lampu Indikator** 

### Perangkat Lunak & Protokol
-   **Firmware**: C/C++ (Arduino)
-   **Backend & Dashboard**: Laravel 8, Python, MySQL
-   **Protokol Komunikasi**: MQTT
-   **Notifikasi**: Telegram Bot API

## Pengembang

-   **Diva Sakananda** (Teknik Komputer Pens)

