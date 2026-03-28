param(
    [string]$Url = "http://127.0.0.1:80"
)

$ErrorActionPreference = "Stop"

function Get-CloudflaredBinary {
    $cmd = Get-Command cloudflared -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\cloudflared.exe"),
        "C:\Program Files\cloudflared\cloudflared.exe",
        "C:\Program Files (x86)\cloudflared\cloudflared.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    throw "cloudflared not found. Install with: winget install --id Cloudflare.cloudflared"
}

$deployDir = $PSScriptRoot
$runDir = Join-Path $deployDir "run"
New-Item -Path $runDir -ItemType Directory -Force | Out-Null

$cloudflared = Get-CloudflaredBinary
$outLog = Join-Path $runDir "tunnel.out.log"
$errLog = Join-Path $runDir "tunnel.err.log"
$pidFile = Join-Path $runDir "tunnel.pid"

Get-Process cloudflared -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 250

if (Test-Path $outLog) { Remove-Item $outLog -Force }
if (Test-Path $errLog) { Remove-Item $errLog -Force }

$args = @(
    "tunnel",
    "--url", $Url,
    "--no-autoupdate",
    "--protocol", "http2",
    "--edge-ip-version", "4"
)

$proc = Start-Process -FilePath $cloudflared -ArgumentList $args -WorkingDirectory $deployDir -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
$proc.Id | Set-Content -Path $pidFile -Encoding ASCII

Write-Host ("Started cloudflared (PID {0})" -f $proc.Id)
Write-Host "Waiting for tunnel URL..."

$tunnelUrl = $null
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 500

    $candidates = @()
    if (Test-Path $outLog) { $candidates += $outLog }
    if (Test-Path $errLog) { $candidates += $errLog }

    foreach ($log in $candidates) {
        $line = Select-String -Path $log -Pattern "https://[a-zA-Z0-9.-]+\\.trycloudflare\\.com" | Select-Object -Last 1
        if ($line) {
            $m = [regex]::Match($line.Line, "https://[a-zA-Z0-9.-]+\\.trycloudflare\\.com")
            if ($m.Success) {
                $tunnelUrl = $m.Value
                break
            }
        }
    }

    if ($tunnelUrl) { break }
}

if (-not $tunnelUrl) {
    Write-Host "Tunnel started but URL not found yet. Check deploy/run/tunnel.out.log" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Quick Tunnel URL:" -ForegroundColor Green
Write-Host "$tunnelUrl/miyamii/"
Write-Host ""
Write-Host "Stop with: powershell -ExecutionPolicy Bypass -File .\\deploy\\stop-tunnel.ps1"
