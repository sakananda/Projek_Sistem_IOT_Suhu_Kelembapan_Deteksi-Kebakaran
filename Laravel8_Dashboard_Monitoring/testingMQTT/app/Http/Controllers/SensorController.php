<?php

namespace App\Http\Controllers;

use App\Models\HourlySensorData;
use Illuminate\Http\Request;
use App\Models\SensorRealtime;
use App\Models\SensorHistory;
use Carbon\Carbon;

class SensorController extends Controller
{
    public function index()
    {
        $realtimeData = SensorRealtime::latest()->first();
        
        $historyHours = 1; 
        $startTime = Carbon::now('Asia/Jakarta')->format('H:i:s');
        $limitPoints = 450;

        $getFormattedHistoryArrays = function ($sensorType) use ($startTime, $limitPoints) {
            $queryData = SensorHistory::where('created_at', '>=', $startTime)
                                ->orderBy('created_at', 'asc')
                                ->whereNotNull($sensorType)
                                ->select('created_at', $sensorType)
                                ->limit($limitPoints)
                                ->get(); // Ini menghasilkan Collection of Models
            
            return [
                'timestamps' => $queryData->pluck('created_at')->map(function($date) {
                                    return Carbon::parse($date)->toIso8601String(); 
                                 })->all(), // ->all() mengubah Collection menjadi array PHP
                'values'     => $queryData->pluck($sensorType)->all(), // ->all() mengubah Collection menjadi array PHP
            ];
            // $queryData = HourlySensorData::all();
            // return [
            //     'timestamps' => $queryData->pluck('created_at')->all(), // ->all() mengubah Collection menjadi array PHP
            //     'values'     => $queryData->pluck($sensorType)->all(), // ->all() mengubah Collection menjadi array PHP

            //                 ];
        };

        // Variabel ini akan dikirim ke view
        $initialSuhuData = $getFormattedHistoryArrays('suhu');
        $initialKelembapanData = $getFormattedHistoryArrays('kelembapan');
        $initialAsapData = $getFormattedHistoryArrays('asap');
        
        return view('dashboard', compact(
            'realtimeData', 
            'initialSuhuData', 
            'initialKelembapanData', 
            'initialAsapData',
        ));
    }

    public function getRealtime()
    {
        $data = SensorRealtime::latest()->first();
        if ($data && $data->created_at) {
            $data->created_at_iso = Carbon::parse($data->created_at)->toIso8601String();
        }
        return response()->json($data);
    }
}
