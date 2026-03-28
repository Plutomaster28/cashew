param(
    [string]$AssetDir = "test-site",
    [string]$Gateway = "http://localhost:8080",
    [string]$ConfigPath = "cashew.conf",
    [string]$ManifestPath = "festival-links.csv",
    [switch]$SkipBuild,
    [switch]$SkipNode
)

$ErrorActionPreference = "Stop"

function Resolve-CashewBinary {
    $candidates = @(
        "build/src/cashew.exe",
        "build/src/cashew"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Cashew binary not found under build/src. Run build first."
}

function Invoke-Cashew {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Binary,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = & $Binary @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    return @{ Output = $output; ExitCode = $exitCode }
}

function Get-RelativePathCompat {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,
        [Parameter(Mandatory = $true)]
        [string]$TargetPath
    )

    $base = (Resolve-Path $BasePath).Path
    $target = (Resolve-Path $TargetPath).Path

    if ($target.StartsWith($base, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $target.Substring($base.Length).TrimStart([char[]]@('\', '/'))
    }

    return [System.IO.Path]::GetFileName($target)
}

if (-not (Test-Path $AssetDir)) {
    throw "Asset directory not found: $AssetDir"
}

if (-not $SkipBuild) {
    Write-Host "[1/4] Configuring build..."
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

    Write-Host "[2/4] Building Cashew..."
    cmake --build build
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

$cashew = Resolve-CashewBinary

$nodeProcess = $null
if (-not $SkipNode) {
    Write-Host "[3/4] Starting Cashew node in background..."
    $argList = @("node", $ConfigPath)
    $nodeProcess = Start-Process -FilePath $cashew -ArgumentList $argList -PassThru
    Start-Sleep -Seconds 2
    Write-Host "Node started (PID: $($nodeProcess.Id))."
}

Write-Host "[4/4] Uploading assets from '$AssetDir'..."
$files = Get-ChildItem -Path $AssetDir -File -Recurse | Sort-Object FullName
if ($files.Count -eq 0) {
    throw "No files found in asset directory: $AssetDir"
}

$records = @()
foreach ($file in $files) {
    $result = Invoke-Cashew -Binary $cashew -Arguments @("content", "add", $file.FullName)
    if ($result.ExitCode -ne 0) {
        throw "Failed to add file: $($file.FullName)`n$($result.Output -join "`n")"
    }

    $text = ($result.Output -join "`n")
    $match = [regex]::Match($text, "Hash:\s*([0-9a-fA-F]{64})")
    if (-not $match.Success) {
        throw "Could not parse hash for file: $($file.FullName)`n$text"
    }

    $hash = $match.Groups[1].Value.ToLowerInvariant()
    $relative = Get-RelativePathCompat -BasePath $AssetDir -TargetPath $file.FullName
    $url = "$Gateway/api/thing/$hash"

    $records += [PSCustomObject]@{
        File = $relative
        Hash = $hash
        Url  = $url
    }
}

$records | Export-Csv -Path $ManifestPath -NoTypeInformation -Encoding UTF8

Write-Host ""
Write-Host "Festival links generated: $ManifestPath"
$records | Format-Table -AutoSize
Write-Host ""
if ($nodeProcess) {
    Write-Host "Tip: stop the node later with 'Stop-Process -Id $($nodeProcess.Id)'" -ForegroundColor Yellow
}
