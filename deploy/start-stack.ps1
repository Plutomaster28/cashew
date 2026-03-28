param(
    [ValidateSet("http", "https")]
    [string]$Mode = "http",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Get-CaddyBinary {
    $cmd = Get-Command caddy -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $wingetLink = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\caddy.exe"
    if (Test-Path $wingetLink) {
        return $wingetLink
    }

    throw "Caddy not found. Install with: winget install --id CaddyServer.Caddy"
}

function Get-Msys2UcrtBin {
    $candidates = @()

    if ($env:MSYS2_ROOT) {
        $candidates += (Join-Path $env:MSYS2_ROOT "ucrt64\bin")
    }

    $candidates += @(
        "C:\msys64\ucrt64\bin",
        "D:\msys64\ucrt64\bin"
    )

    foreach ($tool in @("gcc", "cc", "c++")) {
        $cmd = Get-Command $tool -ErrorAction SilentlyContinue
        if ($cmd) {
            $toolDir = Split-Path $cmd.Source -Parent
            if ($toolDir) {
                $candidates += $toolDir
            }
        }
    }

    foreach ($path in ($candidates | Select-Object -Unique)) {
        if (-not $path) { continue }
        if ((Test-Path $path) -and (Test-Path (Join-Path $path "libsodium-26.dll"))) {
            return $path
        }
    }

    return $null
}

function Start-LoggedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,
        [Parameter(Mandatory = $true)]
        [string]$RunDir,
        [switch]$DryRun
    )

    $outLog = Join-Path $RunDir ("{0}.out.log" -f $Name)
    $errLog = Join-Path $RunDir ("{0}.err.log" -f $Name)
    $pidFile = Join-Path $RunDir ("{0}.pid" -f $Name)

    if ($DryRun) {
        Write-Host ("[dry-run] {0}: {1} {2}" -f $Name, $FilePath, ($Arguments -join " "))
        return
    }

    if (Test-Path $outLog) { Remove-Item $outLog -Force }
    if (Test-Path $errLog) { Remove-Item $errLog -Force }

    $startParams = @{
        FilePath = $FilePath
        WorkingDirectory = $WorkingDirectory
        PassThru = $true
        RedirectStandardOutput = $outLog
        RedirectStandardError = $errLog
    }
    if ($Arguments -and $Arguments.Count -gt 0) {
        $startParams.ArgumentList = $Arguments
    }

    $proc = Start-Process @startParams
    $proc.Id | Set-Content -Path $pidFile -Encoding ASCII
    Write-Host ("Started {0} (PID {1})" -f $Name, $proc.Id)
}

$deployDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $deployDir "..")).Path
$runDir = Join-Path $deployDir "run"

New-Item -Path $runDir -ItemType Directory -Force | Out-Null

$cashewBin = Join-Path $repoRoot "build\src\cashew.exe"
if (-not (Test-Path $cashewBin)) {
    throw "Cashew binary not found at build/src/cashew.exe. Build first with cmake --build build"
}

$ucrtBin = Get-Msys2UcrtBin
if ($ucrtBin) {
    $pathParts = @($env:Path -split ';' | Where-Object { $_ -and $_.Trim().Length -gt 0 })
    $alreadyPresent = $false
    foreach ($part in $pathParts) {
        if ($part.TrimEnd('\\').ToLowerInvariant() -eq $ucrtBin.TrimEnd('\\').ToLowerInvariant()) {
            $alreadyPresent = $true
            break
        }
    }

    if (-not $alreadyPresent) {
        $env:Path = "$ucrtBin;$env:Path"
        Write-Host "Prepended MSYS2 runtime path: $ucrtBin"
    }
} else {
    Write-Warning "MSYS2 UCRT runtime path not found. If cashew fails with missing DLLs, install MSYS2 UCRT64 and add its bin folder to PATH."
}

$caddyBin = Get-CaddyBinary

$caddyConfig = Join-Path $deployDir "Caddyfile"
if ($Mode -eq "https") {
    $httpsConfig = Join-Path $deployDir "Caddyfile.https"
    $httpsExample = Join-Path $deployDir "Caddyfile.https.example"
    if (-not (Test-Path $httpsConfig)) {
        if (-not (Test-Path $httpsExample)) {
            throw "Missing deploy/Caddyfile.https.example"
        }
        Copy-Item $httpsExample $httpsConfig -Force
        Write-Host "Created deploy/Caddyfile.https from example. Edit domains before public use."
    }
    $caddyConfig = $httpsConfig
}

# Stop stale instances from previous runs to avoid port conflicts.
Get-Process cashew -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process perl -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process caddy -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

Start-LoggedProcess -Name "cashew" -FilePath $cashewBin -Arguments @() -WorkingDirectory $repoRoot -RunDir $runDir -DryRun:$DryRun
Start-LoggedProcess -Name "guestbook" -FilePath "perl" -Arguments @(".\examples\perl-cgi\server.pl", "8000") -WorkingDirectory $repoRoot -RunDir $runDir -DryRun:$DryRun
Start-LoggedProcess -Name "caddy" -FilePath $caddyBin -Arguments @("run", "--config", $caddyConfig) -WorkingDirectory $repoRoot -RunDir $runDir -DryRun:$DryRun

if ($DryRun) {
    return
}

Start-Sleep -Seconds 1

$lanIp = (Get-NetIPAddress -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -notmatch '^127\.' -and $_.IPAddress -notmatch '^169\.254\.' -and $_.InterfaceAlias -eq 'Wi-Fi' } |
    Select-Object -First 1 -ExpandProperty IPAddress)

if (-not $lanIp) {
    $lanIp = (Get-NetIPAddress -AddressFamily IPv4 |
        Where-Object { $_.IPAddress -notmatch '^127\.' -and $_.IPAddress -notmatch '^169\.254\.' } |
        Select-Object -First 1 -ExpandProperty IPAddress)
}

Write-Host ""
Write-Host "Stack is up."
Write-Host "Local: http://127.0.0.1/miyamii/"
if ($lanIp) {
    Write-Host ("LAN:   http://{0}/miyamii/" -f $lanIp)
}
Write-Host "Logs: deploy/run/*.log"
Write-Host "Stop: .\\deploy\\stop-stack.ps1"
