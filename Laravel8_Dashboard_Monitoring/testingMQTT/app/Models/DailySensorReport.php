<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Database\Eloquent\Model;

class DailySensorReport extends Model
{
    use HasFactory;

    protected $table = 'daily_sensor_reports';

    protected $fillable = [
        'report_date',
        'json_data',
        // 'file_path', // Jika Anda menggunakan kolom ini
    ];

    protected $casts = [
        'report_date' => 'date',
        'json_data' => 'array', // Otomatis cast ke/dari JSON saat mengambil/menyimpan
    ];
}
