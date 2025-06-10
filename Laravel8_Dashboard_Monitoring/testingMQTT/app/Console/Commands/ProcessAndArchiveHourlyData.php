<?php
namespace App\Console\Commands;

use Illuminate\Console\Command;
use App\Models\HourlySensorData;
use App\Models\DailySensorReport;
use Illuminate\Support\Facades\DB;
use Carbon\Carbon;
use Illuminate\Support\Facades\Log;

class ProcessAndArchiveHourlyData extends Command
{
    protected $signature = 'sensor:process-archive-hourly';
    protected $description = 'Processes hourly sensor data, archives it as daily JSON report, and resets the hourly table.';

    public function __construct()
    {
        parent::__construct();
    }

    public function handle()
    {
        Log::channel('scheduler')->info('Memulai Proses Laporan Sensor Harian...');
        $this->info('Memulai Proses Laporan Sensor Harian...');

        $reportDate = Carbon::yesterday();
        $reportDateString = $reportDate->toDateString();

        $hourlyData = HourlySensorData::orderBy('hour_timestamp', 'asc')->get();

        if ($hourlyData->isEmpty()) {
            Log::channel('scheduler')->info('Tidak ada data per jam untuk diproses pada tanggal: ' . $reportDateString);
            $this->info('Tidak ada data per jam untuk diproses pada tanggal: ' . $reportDateString);
            
            try {
                DB::table('hourly_sensor_data')->truncate();
                Log::channel('scheduler')->info('Tabel hourly_sensor_data telah direset (meskipun kosong).');
                $this->info('Tabel hourly_sensor_data telah direset (meskipun kosong).');
            } catch (\Exception $e) {
                Log::channel('scheduler')->error('Gagal mereset tabel hourly_sensor_data (kosong): ' . $e->getMessage());
                $this->error('Gagal mereset tabel hourly_sensor_data (kosong): ' . $e->getMessage());
            }
            return 0;
        }

        $reportDataForJson = $hourlyData->map(function ($item) {
            return [
                'hour_timestamp' => $item->hour_timestamp->format('Y-m-d H:00:00'),
                'min_suhu' => $item->min_suhu,
                'max_suhu' => $item->max_suhu,
                'min_kelembapan' => $item->min_kelembapan,
                'max_kelembapan' => $item->max_kelembapan,
                'min_asap' => $item->min_asap,
                'max_asap' => $item->max_asap,
                'data_points' => $item->data_points_count,
            ];
        });
        
        $jsonData = json_encode($reportDataForJson->values()->all(), JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);

        try {
            DailySensorReport::updateOrCreate(
                ['report_date' => $reportDateString],
                ['json_data' => $jsonData]
            );
            Log::channel('scheduler')->info('Laporan JSON harian berhasil disimpan untuk tanggal: ' . $reportDateString);
            $this->info('Laporan JSON harian berhasil disimpan untuk tanggal: ' . $reportDateString);

            DB::table('hourly_sensor_data')->truncate();
            Log::channel('scheduler')->info('Tabel hourly_sensor_data telah direset.');
            $this->info('Tabel hourly_sensor_data telah direset.');

        } catch (\Exception $e) {
            Log::channel('scheduler')->error('Gagal menyimpan laporan JSON harian atau mereset tabel: ' . $e->getMessage());
            $this->error('Gagal menyimpan laporan JSON harian atau mereset tabel: ' . $e->getMessage());
            return 1;
        }

        $this->info('Proses Laporan Sensor Harian Selesai.');
        Log::channel('scheduler')->info('Proses Laporan Sensor Harian Selesai.');
        return 0;
    }
}
