<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dashboard Monitoring Sensor</title>
    <meta name="csrf-token" content="{{ csrf_token() }}">
    <link rel="stylesheet" href="{{ asset('css/dashboard.css') }}">
    <script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/moment@2.29.1/moment.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-moment@1.0.0/dist/chartjs-adapter-moment.min.js"></script>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f4f7f6; margin: 0; padding: 20px; color: #333; line-height: 1.6; }
        .container { max-width: 1200px; margin: 20px auto; padding: 0 15px; }
        h1 { text-align: center; color: #2c3e50; margin-bottom: 30px; }
        .sensor-display-grid { display: grid; grid-template-columns: 1fr; gap: 30px; margin-bottom: 30px; }
        .sensor-row { display: grid; grid-template-columns: 1fr; gap: 20px; align-items: stretch; }
        @media (min-width: 992px) { .sensor-row { grid-template-columns: 280px 1fr; align-items: center; } }
        .card { background: #fff; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.08); padding: 25px; text-align: left; min-height: 150px; display: flex; flex-direction: column; justify-content: center; }
        .card h2 { margin-top: 0; margin-bottom: 8px; color: #34495e; font-size: 1.2em; font-weight: 600; }
        .card p { font-size: 2.8em; color: #2c3e50; margin: 0; font-weight: 700; line-height: 1.1; }
        .card.suhu { border-left: 6px solid #e74c3c; }
        .card.kelembapan { border-left: 6px solid #3498db; }
        .card.asap { border-left: 6px solid #f39c12; }
        .chart-container { position: relative; height: 280px; background-color: #fff; padding: 15px; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.08); }
        .nav-link-container { margin-top: 40px; text-align: center; }
        .nav-link { display: inline-block; padding: 12px 25px; background-color: #3498db; color: white; text-decoration: none; border-radius: 5px; font-size: 16px; transition: background-color 0.3s ease; }
        .nav-link:hover { background-color: #2980b9;}
    </style>
</head>
<body>
    <div class="container">
        <h1>Dashboard Monitoring Sensor</h1>
        <div class="sensor-display-grid">
            <div class="sensor-row">
                <div class="card suhu">
                    <h2>Suhu</h2>
                    <p><span id="suhuValue">{{ $realtimeData->suhu ?? '-' }}</span> °C</p>
                </div>
                <div class="chart-container">
                    <canvas id="realtimeSuhuChart"></canvas>
                </div>
            </div>
            <div class="sensor-row">
                <div class="card kelembapan">
                    <h2>Kelembapan</h2>
                    <p><span id="kelembapanValue">{{ $realtimeData->kelembapan ?? '-' }}</span> %</p>
                </div>
                <div class="chart-container">
                    <canvas id="realtimeKelembapanChart"></canvas>
                </div>
            </div>
            <div class="sensor-row">
                <div class="card asap">
                    <h2>Asap (PPM)</h2>
                    <p><span id="asapValue">{{ $realtimeData->asap ?? '-' }}</span> ppm</p>
                </div>
                <div class="chart-container">
                    <canvas id="realtimeAsapChart"></canvas>
                </div>
            </div>
        </div>
        
        <div class="nav-link-container">
            <a href="{{ route('reports.index') }}" class="nav-link">Lihat & Ekspor Laporan Harian</a>
        </div>
    </div>

    <script>
        const initialSuhuTimestamps = @json($initialSuhuData['timestamps'] ?? []);
        const initialSuhuValues = @json($initialSuhuData['values'] ?? []);
        const initialKelembapanTimestamps = @json($initialKelembapanData['timestamps'] ?? []);
        const initialKelembapanValues = @json($initialKelembapanData['values'] ?? []);
        const initialAsapTimestamps = @json($initialAsapData['timestamps'] ?? []);
        const initialAsapValues = @json($initialAsapData['values'] ?? []);

        let realtimeSuhuChart, realtimeKelembapanChart, realtimeAsapChart;
        const MAX_CHART_POINTS = 450; // Sekitar 1 jam data jika interval 8 detik

        function createRealtimeChart(ctx, label, yAxisLabel, borderColor, initialTimestamps = [], initialData = []) {
            return new Chart(ctx, {
                type: 'line',
                data: {
                    labels: initialTimestamps.map(ts => moment(ts)), 
                    datasets: [{
                        label: label, data: initialData, borderColor: borderColor,
                        borderWidth: 1.5, fill: false, tension: 0.3, pointRadius: 1, pointHoverRadius: 3
                    }]
                },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    scales: {
                        x: {
                            type: 'time',
                            time: { unit: 'minute', tooltipFormat: 'HH:mm:ss', displayFormats: { minute: 'HH:mm' }},
                            ticks: { source: 'auto', maxRotation: 0, autoSkip: true, maxTicksLimit: 10 },
                            title: { display: false }
                        },
                        y: {
                            beginAtZero: false, title: { display: true, text: yAxisLabel },
                            ticks: { 
                                stepSize: 0.5,
                                callback: function(value) { return (value % 1 !== 0) ? value.toFixed(1) : value; }
                            }
                        }
                    },
                    animation: { duration: 200 },
                    plugins: { legend: { display: false } }
                }
            });
        }
        
        function addDataToChart(chart, dataValue, timestamp) {
            if (!chart || dataValue === null || dataValue === undefined || timestamp === null) return;
            const timeLabel = moment(timestamp); 
            const lastLabel = chart.data.labels.length > 0 ? chart.data.labels[chart.data.labels.length - 1] : null;
            if (lastLabel && timeLabel.isSameOrBefore(lastLabel)) { return; } 

            chart.data.labels.push(timeLabel);
            chart.data.datasets[0].data.push(dataValue);
            if (chart.data.labels.length > MAX_CHART_POINTS) {
                chart.data.labels.shift();
                chart.data.datasets[0].data.shift();
            }
            chart.update('quiet');
        }

        function loadRealtimeDataAndUpdateCards() {
            $.getJSON("{{ url('/api/sensor-realtime') }}", function(data) {
                if (data) {
                    $('#suhuValue').text(data.suhu !== null ? parseFloat(data.suhu).toFixed(1) : '-');
                    $('#kelembapanValue').text(data.kelembapan !== null ? parseFloat(data.kelembapan).toFixed(1) : '-');
                    $('#asapValue').text(data.asap !== null ? parseInt(data.asap) : '-');
                    const dataTimestamp = data.created_at_iso ? data.created_at_iso : moment().toISOString();
                    
                    addDataToChart(realtimeSuhuChart, data.suhu, dataTimestamp);
                    addDataToChart(realtimeKelembapanChart, data.kelembapan, dataTimestamp);
                    addDataToChart(realtimeAsapChart, data.asap, dataTimestamp);
                }
            }).fail(function(jqXHR, textStatus, errorThrown) {
                console.error("Gagal mengambil data real-time:", textStatus, errorThrown);
            });
        }
        
        $(document).ready(function() {
            realtimeSuhuChart = createRealtimeChart(
                $('#realtimeSuhuChart')[0].getContext('2d'), 
                'Suhu', '°C', 'rgba(255, 99, 132, 1)', 
                initialSuhuTimestamps, 
                initialSuhuValues
            );
            realtimeKelembapanChart = createRealtimeChart(
                $('#realtimeKelembapanChart')[0].getContext('2d'), 
                'Kelembapan', '%', 'rgba(54, 162, 235, 1)',
                initialKelembapanTimestamps,
                initialKelembapanValues
            );
            realtimeAsapChart = createRealtimeChart(
                $('#realtimeAsapChart')[0].getContext('2d'), 
                'Asap', 'PPM', 'rgba(255, 159, 64, 1)',
                initialAsapTimestamps,
                initialAsapValues
            );
            
            setInterval(loadRealtimeDataAndUpdateCards, 8000); 
        });
    </script>
</body>
</html>