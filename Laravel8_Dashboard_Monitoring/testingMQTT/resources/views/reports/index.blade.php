<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Laporan Sensor Harian</title>
    <link rel="stylesheet" href="{{ asset('css/dashboard.css') }}"> <!-- Anda bisa menggunakan CSS yang sama atau buat baru -->
    <style>
        body { font-family: 'Segoe UI', sans-serif; background-color: #f4f7f6; margin: 0; padding: 20px; color: #333; }
        .report-container { max-width: 600px; margin: 50px auto; padding: 30px; background: #fff; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); text-align: center; }
        h1 { color: #2c3e50; margin-bottom: 25px;}
        .form-group { margin-bottom: 20px; text-align: left;}
        .form-group label { display: block; margin-bottom: 8px; font-weight: 500; color: #34495e; }
        .form-group select, .form-group button { width: 100%; padding: 12px; border-radius: 5px; border: 1px solid #ced4da; box-sizing: border-box; font-size: 16px; }
        .form-group select { appearance: none; -webkit-appearance: none; background-image: url('data:image/svg+xml;charset=US-ASCII,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%22292.4%22%20height%3D%22292.4%22%3E%3Cpath%20fill%3D%22%23007bff%22%20d%3D%22M287%2069.4a17.6%2017.6%200%200%200-13-5.4H18.4c-5%200-9.3%201.8-12.9%205.4A17.6%2017.6%200%200%200%200%2082.2c0%205%201.8%209.3%205.4%2012.9l128%20127.9c3.6%203.6%207.8%205.4%2012.8%205.4s9.2-1.8%2012.8-5.4L287%2095c3.5-3.5%205.4-7.8%205.4-12.8%200-5-1.9-9.2-5.4-12.8z%22%2F%3E%3C%2Fsvg%3E'); background-repeat: no-repeat; background-position: right .7em top 50%; background-size: .65em auto; padding-right: 2.5em;}
        .form-group button { background-color: #5cb85c; color: white; cursor: pointer; border: none; }
        .form-group button:hover { background-color: #4cae4c; }
        .form-group button:disabled { background-color: #cccccc; cursor: not-allowed;}
        .alert { padding: 15px; margin-bottom: 20px; border: 1px solid transparent; border-radius: 4px; text-align: left; }
        .alert-error { color: #a94442; background-color: #f2dede; border-color: #ebccd1; }
        .alert-success { color: #3c763d; background-color: #dff0d8; border-color: #d6e9c6; }
        .nav-link { display: inline-block; margin-top: 20px; padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; font-size: 1em;}
        .nav-link:hover { background-color: #0056b3;}
    </style>
</head>
<body>
    <div class="report-container">
        <h1>Download Laporan Sensor Harian</h1>

        @if(session('error'))
            <div class="alert alert-error">{{ session('error') }}</div>
        @endif
        @if(session('success')) {{-- Tidak digunakan saat ini, tapi bisa untuk notif lain --}}
            <div class="alert alert-success">{{ session('success') }}</div>
        @endif

        <form action="{{ route('reports.export.csv') }}" method="POST">
            @csrf
            <div class="form-group">
                <label for="report_date">Pilih Tanggal Laporan:</label>
                <select name="report_date" id="report_date" required>
                    @if($availableReports->isEmpty())
                        <option value="" disabled selected>Tidak ada laporan tersedia</option>
                    @else
                        @foreach($availableReports as $date)
                            <option value="{{ $date->format('Y-m-d') }}">{{ $date->format('d F Y') }}</option>
                        @endforeach
                    @endif
                </select>
            </div>
            <div class="form-group">
                <button type="submit" @if($availableReports->isEmpty()) disabled @endif>Download Laporan CSV</button>
            </div>
        </form>
        <a href="{{ url('/') }}" class="nav-link">Kembali ke Dashboard</a>
    </div>
</body>
</html>
