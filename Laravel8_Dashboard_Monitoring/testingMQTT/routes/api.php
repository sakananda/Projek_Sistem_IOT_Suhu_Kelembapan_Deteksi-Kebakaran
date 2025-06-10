<?php
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;
use App\Http\Controllers\SensorController;

Route::get('/sensor-realtime', [SensorController::class, 'getRealtime']);
