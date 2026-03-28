param(
    [string]$BaseUrl = "http://127.0.0.1",
    [switch]$WriteTest,
    [int]$TimeoutSec = 8,
    [int]$Retries = 3,
    [int]$RetryDelayMs = 700
)

$ErrorActionPreference = "Stop"

function Test-Endpoint {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [string]$Method = "GET",
        [string]$ContentType = "",
        [string]$Body = "",
        [scriptblock]$Validate
    )

    for ($attempt = 1; $attempt -le $Retries; $attempt++) {
        try {
            if ($Method -eq "POST") {
                if ($ContentType) {
                    $resp = Invoke-WebRequest -UseBasicParsing -TimeoutSec $TimeoutSec -Uri $Url -Method POST -ContentType $ContentType -Body $Body
                } else {
                    $resp = Invoke-WebRequest -UseBasicParsing -TimeoutSec $TimeoutSec -Uri $Url -Method POST -Body $Body
                }
            } else {
                $resp = Invoke-WebRequest -UseBasicParsing -TimeoutSec $TimeoutSec -Uri $Url
            }

            $ok = $true
            if ($Validate) {
                $ok = & $Validate $resp
            }

            return [PSCustomObject]@{
                Name = $Name
                Url = $Url
                Status = $resp.StatusCode
                Success = [bool]$ok
                Detail = if ($ok) { "OK" } else { "Validation failed" }
            }
        } catch {
            if ($attempt -lt $Retries) {
                Start-Sleep -Milliseconds $RetryDelayMs
                continue
            }

            return [PSCustomObject]@{
                Name = $Name
                Url = $Url
                Status = "ERR"
                Success = $false
                Detail = $_.Exception.Message
            }
        }
    }
}

$results = @()

$siteUrl = "$BaseUrl/miyamii/"
$apiUrl = "$BaseUrl/health"
$guestbookListUrl = "$BaseUrl/miyamii/cgi-bin/guestbook.pl?action=list"
$guestbookPostUrl = "$BaseUrl/miyamii/cgi-bin/guestbook.pl"

$results += Test-Endpoint -Name "Site" -Url $siteUrl -Validate {
    param($resp)
    ($resp.StatusCode -eq 200) -and ($resp.Content -match "miyamii.net")
}

$results += Test-Endpoint -Name "Health API" -Url $apiUrl -Validate {
    param($resp)
    ($resp.StatusCode -eq 200) -and ($resp.Content -match "healthy|cashew-gateway")
}

$results += Test-Endpoint -Name "Guestbook List" -Url $guestbookListUrl -Validate {
    param($resp)
    ($resp.StatusCode -eq 200) -and ($resp.Content -match "posts")
}

if ($WriteTest) {
    $stamp = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $formBody = "name=healthcheck&subject=probe&body=health+probe+$stamp"

    $postResult = Test-Endpoint -Name "Guestbook Post" -Url $guestbookPostUrl -Method "POST" -ContentType "application/x-www-form-urlencoded" -Body $formBody -Validate {
        param($resp)
        ($resp.StatusCode -eq 200) -and ($resp.Content -match '"ok"\s*:\s*true')
    }
    $results += $postResult

    $verifyResult = Test-Endpoint -Name "Guestbook Verify" -Url $guestbookListUrl -Validate {
        param($resp)
        ($resp.StatusCode -eq 200) -and ($resp.Content -match "health probe $stamp")
    }
    $results += $verifyResult
}

Write-Host ""
Write-Host "Cashew Stack Health Check"
Write-Host "Base URL: $BaseUrl"
Write-Host ""
$results | Format-Table Name,Status,Success,Detail -AutoSize
Write-Host ""

$failed = @($results | Where-Object { -not $_.Success })
if ($failed.Count -gt 0) {
    Write-Host "FAILED checks: $($failed.Count)" -ForegroundColor Red
    exit 1
}

Write-Host "All checks passed." -ForegroundColor Green

try {
    $publicIp = (Invoke-RestMethod -Uri "https://api.ipify.org?format=json" -TimeoutSec 6).ip
    if ($publicIp) {
        Write-Host "Share URL (public IP): http://$publicIp/miyamii/"
    }
} catch {
    Write-Host "Public IP lookup skipped."
}
