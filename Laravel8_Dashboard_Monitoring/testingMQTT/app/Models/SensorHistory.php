<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Database\Eloquent\Model;

class SensorHistory extends Model
{
    protected $table = 'sensor_history';
    // protected $fillable = [
    //     'suhu', 
    //     'kelembapan', 
    //     'asap', 
    //     'created_at', 
    // ];
    // protected $casts = [ 
    //      'suhu'    => 'datetime',
    //         'kelembapan'  => 'float',
    //       'asap'  => 'float',
    //       'created_at'   => 'integer',
    // ];
}