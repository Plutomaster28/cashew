$ErrorActionPreference = "Stop"

$deployDir = $PSScriptRoot
$runDir = Join-Path $deployDir "run"

$names = @("cashew", "guestbook", "caddy")

foreach ($name in $names) {
    $pidFile = Join-Path $runDir ("{0}.pid" -f $name)
    if (Test-Path $pidFile) {
        $stackPid = Get-Content $pidFile -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($stackPid -match '^\d+$') {
            try {
                Stop-Process -Id ([int]$stackPid) -Force -ErrorAction Stop
                Write-Host ("Stopped {0} (PID {1})" -f $name, $stackPid)
            } catch {
                Write-Host ("{0}: PID {1} already stopped" -f $name, $stackPid)
            }
        }
        Remove-Item $pidFile -Force
    }
}

Get-Process cashew -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process perl -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process caddy -ErrorAction SilentlyContinue | Stop-Process -Force

Write-Host "Stack stop complete."
