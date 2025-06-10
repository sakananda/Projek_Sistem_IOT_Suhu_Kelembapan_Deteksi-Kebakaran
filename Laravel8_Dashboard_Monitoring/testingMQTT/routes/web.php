<?php

use Illuminate\Support\Facades\Route;
use App\Http\Controllers\SensorController;
use App\Http\Controllers\ReportController;
/*
|--------------------------------------------------------------------------
| Web Routes
|--------------------------------------------------------------------------
|
| Here is where you can register web routes for your application. These
| routes are loaded by the RouteServiceProvider within a group which
| contains the "web" middleware group. Now create something great!
|
*/

Route::get('/welcome', function () {
    return view('welcome');
});

Route::get('/', [SensorController::class, 'index']);

Route::get('/reports', [ReportController::class, 'index'])->name('reports.index');
Route::post('/reports/export-csv', [ReportController::class, 'exportToCsv'])->name('reports.export.csv');