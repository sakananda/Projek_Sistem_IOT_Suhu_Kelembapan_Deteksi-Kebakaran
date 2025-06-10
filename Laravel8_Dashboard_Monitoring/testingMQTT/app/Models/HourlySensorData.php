<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Database\Eloquent\Model;

class HourlySensorData extends Model
{
    use HasFactory;

    protected $table = 'hourly_sensor_data';

    protected $fillable = [
        'hour_timestamp',
        'min_suhu',
        'max_suhu',
        'min_kelembapan',
        'max_kelembapan',
        'min_asap',
        'max_asap',
        'data_points_count',
    ];

    protected $casts = [
        'hour_timestamp' => 'datetime',
        // Pastikan tipe data di database sesuai, atau cast di sini jika perlu
        'min_suhu' => 'float',
        'max_suhu' => 'float',
        'min_kelembapan' => 'float',
        'max_kelembapan' => 'float',
        'min_asap' => 'integer',
        'max_asap' => 'integer',
    ];
}
