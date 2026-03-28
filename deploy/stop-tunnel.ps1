$ErrorActionPreference = "Stop"

$deployDir = $PSScriptRoot
$pidFile = Join-Path $deployDir "run\tunnel.pid"

if (Test-Path $pidFile) {
    $tunnelPid = Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($tunnelPid -match '^\d+$') {
        try {
            Stop-Process -Id ([int]$tunnelPid) -Force -ErrorAction Stop
            Write-Host ("Stopped tunnel process PID {0}" -f $tunnelPid)
        } catch {
            Write-Host ("Tunnel PID {0} already stopped" -f $tunnelPid)
        }
    }
    Remove-Item $pidFile -Force
}

Get-Process cloudflared -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Host "Tunnel stop complete."
