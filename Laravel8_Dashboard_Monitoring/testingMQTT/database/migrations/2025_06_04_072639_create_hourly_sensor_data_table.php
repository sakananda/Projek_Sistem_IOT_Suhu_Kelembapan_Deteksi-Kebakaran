<?php
use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

class CreateHourlySensorDataTable extends Migration // Atau sesuaikan nama class jika mengubah
{
    public function up()
    {
        // Jika tabel sudah ada dan ingin diubah, Anda bisa menggunakan Schema::table(...)
        // Untuk kesederhanaan, kita buat baru atau pastikan skemanya seperti ini
        Schema::dropIfExists('hourly_sensor_data'); // Hapus jika ada untuk skema baru
        Schema::create('hourly_sensor_data', function (Blueprint $table) {
            $table->id();
            $table->timestamp('hour_timestamp')->unique()->comment('Timestamp yang menandai awal jam data');
            $table->float('avg_suhu')->nullable()->comment('Rata-rata suhu per jam');
            $table->float('avg_kelembapan')->nullable()->comment('Rata-rata kelembapan per jam');
            $table->integer('avg_asap')->nullable()->comment('Rata-rata asap PPM per jam');
            $table->integer('data_points_count')->default(0)->comment('Jumlah data point yang diagregasi');
            $table->timestamps();
        });
    }

    public function down()
    {
        Schema::dropIfExists('hourly_sensor_data');
    }
}
