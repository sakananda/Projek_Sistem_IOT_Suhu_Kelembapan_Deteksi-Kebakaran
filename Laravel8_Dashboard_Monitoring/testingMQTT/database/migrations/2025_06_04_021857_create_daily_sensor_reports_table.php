<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

class CreateDailySensorReportsTable extends Migration
{
    /**
     * Run the migrations.
     *
     * @return void
     */
    public function up()
    {
        Schema::create('daily_sensor_reports', function (Blueprint $table) {
            $table->id();
            $table->date('report_date')->unique()->comment('Tanggal laporan (YYYY-MM-DD)');
            $table->longText('json_data')->comment('Data sensor per jam dalam format JSON untuk hari tersebut');
            // Opsional: Jika Anda ingin menyimpan path ke file JSON fisik juga
            // $table->string('file_path')->nullable(); 
            $table->timestamps();
        });
    }

    /**
     * Reverse the migrations.
     *
     * @return void
     */
    public function down()
    {
        Schema::dropIfExists('daily_sensor_reports');
    }
}
