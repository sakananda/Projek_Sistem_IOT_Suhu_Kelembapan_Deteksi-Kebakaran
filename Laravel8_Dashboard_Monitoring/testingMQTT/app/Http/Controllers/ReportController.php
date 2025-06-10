<?php
namespace App\Http\Controllers;

use Illuminate\Http\Request;
use App\Models\DailySensorReport;
use Carbon\Carbon;
use Illuminate\Support\Facades\Log;
use Illuminate\Support\Facades\Response;

class ReportController extends Controller
{
    public function index()
    {
        $availableReports = DailySensorReport::orderBy('report_date', 'desc')
                                ->pluck('report_date')
                                ->map(function ($date) {
                                    return Carbon::parse($date);
                                });
        return view('reports.index', compact('availableReports'));
    }

    public function exportToCsv(Request $request)
    {
        $request->validate([
            'report_date' => 'required|date_format:Y-m-d',
        ]);

        $reportDate = $request->input('report_date');
        $report = DailySensorReport::where('report_date', $reportDate)->first();

        if (!$report || empty($report->json_data)) {
            return redirect()->route('reports.index')->with('error', 'Laporan untuk tanggal '.$reportDate.' tidak ditemukan atau data kosong.');
        }

        $fileName = 'laporan_sensor_harian_' . $reportDate . '.csv';
        $hourlyDataArray = $report->json_data; // Ini adalah array dari data per jam

        if (!is_array($hourlyDataArray) || empty($hourlyDataArray)) {
            Log::warning("Data JSON untuk laporan tanggal {$reportDate} kosong atau bukan array.");
            return redirect()->route('reports.index')->with('error', 'Format data laporan tidak valid atau kosong untuk tanggal '.$reportDate.'.');
        }

        $headers = [
            'Content-type'        => 'text/csv',
            'Content-Disposition' => "attachment; filename={$fileName}",
            'Pragma'              => 'no-cache',
            'Cache-Control'       => 'must-revalidate, post-check=0, pre-check=0',
            'Expires'             => '0',
        ];

        $callback = function() use ($hourlyDataArray, $reportDate) {
            $file = fopen('php://output', 'w');
            
            // Header Laporan
            fputcsv($file, ['Laporan Sensor Harian']);
            fputcsv($file, ['Tanggal:', Carbon::parse($reportDate)->format('d F Y')]);
            fputcsv($file, []); // Baris kosong

            // Header Kolom Data Per Jam
            fputcsv($file, [
                'Jam (WIB)', // Asumsi timestamp di JSON adalah UTC, kita format ke WIB
                'Suhu Rata-rata (°C)',
                'Kelembapan Rata-rata (%)',
                'Asap Rata-rata (PPM)',
                // 'Data Points', // Opsional
            ]);
            // Data Per Jam
            foreach ($hourlyDataArray as $row) {
                // Asumsikan 'hour_timestamp' dalam JSON adalah UTC atau sudah sesuai
                // Jika perlu konversi zona waktu, lakukan di sini
                $timestampJam = isset($row['hour_timestamp']) ? Carbon::parse($row['hour_timestamp'])->setTimezone('Asia/Jakarta')->format('H:00') : 'N/A';
                fputcsv($file, [
                    $timestampJam,
                    $row['avg_suhu'] ?? 'N/A', // Menggunakan data rata-rata dari JSON
                    $row['avg_kelembapan'] ?? 'N/A',
                    $row['avg_asap'] ?? 'N/A',
                    // $row['data_points'] ?? 'N/A', // Opsional
                ]);
            }
            fclose($file);
        };
        try {
            return Response::stream($callback, 200, $headers);
        } catch (\Exception $e) {
            Log::error("Gagal membuat file CSV untuk tanggal {$reportDate}: " . $e->getMessage());
            return redirect()->route('reports.index')->with('error', 'Gagal membuat file CSV. Silakan coba lagi.');
        }
    }
}
