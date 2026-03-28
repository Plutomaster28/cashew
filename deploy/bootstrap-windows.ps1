param(
    [switch]$SkipBuild,
    [switch]$SkipFirewall,
    [ValidateSet("http", "https")]
    [string]$Mode = "http",
    [switch]$SkipHealthCheck,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$deployDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $deployDir "..")).Path

function Assert-Command {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [string]$Hint = ""
    )

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        $msg = "Required command '$Name' not found in PATH."
        if ($Hint) {
            $msg += " $Hint"
        }
        throw $msg
    }
}

function Test-IsAdmin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Ensure-FirewallRule {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [int]$Port
    )

    $existing = Get-NetFirewallRule -DisplayName $Name -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host "Firewall rule exists: $Name"
        return
    }

    New-NetFirewallRule -DisplayName $Name -Direction Inbound -Action Allow -Protocol TCP -LocalPort $Port -Profile Private | Out-Null
    Write-Host "Created firewall rule: $Name (TCP/$Port, Private profile)"
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    Write-Host ""
    Write-Host "== $Label =="

    if ($DryRun) {
        Write-Host "[dry-run] skipped"
        return
    }

    & $Action
}

Set-Location $repoRoot

Assert-Command -Name cmake -Hint "Install CMake and ensure it is available in PATH."
Assert-Command -Name perl -Hint "Install Strawberry Perl or equivalent."
Assert-Command -Name caddy -Hint "Install via: winget install --id CaddyServer.Caddy"

Invoke-Step -Label "Build (configure + compile)" -Action {
    if (-not $SkipBuild) {
        cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

        cmake --build build --parallel
        if ($LASTEXITCODE -ne 0) { throw "Build failed." }
    } else {
        Write-Host "Build skipped via -SkipBuild."
    }
}

Invoke-Step -Label "Firewall rules" -Action {
    if ($SkipFirewall) {
        Write-Host "Firewall step skipped via -SkipFirewall."
        return
    }

    if (-not (Test-IsAdmin)) {
        Write-Warning "Not running as Administrator. Skipping firewall rule creation."
        Write-Warning "Run an elevated PowerShell and re-run without -SkipFirewall to open ports automatically."
        return
    }

    Ensure-FirewallRule -Name "Cashew LAN HTTP 80" -Port 80
    Ensure-FirewallRule -Name "Cashew LAN HTTPS 443" -Port 443
    Ensure-FirewallRule -Name "Cashew App 8080" -Port 8080
    Ensure-FirewallRule -Name "Cashew Guestbook 8000" -Port 8000
}

Invoke-Step -Label "Restart stack" -Action {
    & (Join-Path $deployDir "stop-stack.ps1")
    & (Join-Path $deployDir "start-stack.ps1") -Mode $Mode
}

Invoke-Step -Label "Health check" -Action {
    if (-not $SkipHealthCheck) {
        & (Join-Path $deployDir "health-check.ps1")
    } else {
        Write-Host "Health check skipped via -SkipHealthCheck."
    }
}

Write-Host ""
Write-Host "Bootstrap complete."
