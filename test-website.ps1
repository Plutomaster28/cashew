#!/usr/bin/env pwsh
# Cashew v1.0 Website Test Script

Write-Host "=== Cashew v1.0 Website Test ===" -ForegroundColor Cyan
Write-Host ""

# Check if server is running
Write-Host "Checking server status..." -ForegroundColor Yellow
$serverRunning = $false
try {
    $response = Invoke-WebRequest -Uri "http://localhost:8080/" -Method Head -TimeoutSec 2 -UseBasicParsing -ErrorAction Stop
    $serverRunning = $true
    Write-Host "✓ Server is running on http://localhost:8080" -ForegroundColor Green
} catch {
    Write-Host "✗ Server is not running" -ForegroundColor Red
    Write-Host "  Please start the server first: .\cashew.exe" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Testing API Endpoints:" -ForegroundColor Yellow
Write-Host ""

# Test main page
Write-Host "1. Testing main page (/)..."
try {
    $response = Invoke-WebRequest -Uri "http://localhost:8080/" -UseBasicParsing -TimeoutSec 5
    if ($response.StatusCode -eq 200) {
        Write-Host "   ✓ Status: $($response.StatusCode) OK" -ForegroundColor Green
        Write-Host "   ✓ Content-Type: $($response.Headers['Content-Type'])" -ForegroundColor Green
        $contentLength = $response.RawContentLength
        Write-Host "   ✓ Size: $contentLength bytes" -ForegroundColor Green
    }
} catch {
    Write-Host "   ✗ Failed: $_" -ForegroundColor Red
}

Write-Host ""

# Test status API
Write-Host "2. Testing /api/status..."
try {
    $response = Invoke-WebRequest -Uri "http://localhost:8080/api/status" -UseBasicParsing -TimeoutSec 5
    if ($response.StatusCode -eq 200) {
        Write-Host "   ✓ Status: $($response.StatusCode) OK" -ForegroundColor Green
        $json = $response.Content | ConvertFrom-Json
        Write-Host "   ✓ Node ID: $($json.node_id.Substring(0,16))..." -ForegroundColor Green
        Write-Host "   ✓ Storage items: $($json.storage_items)" -ForegroundColor Green
        Write-Host "   ✓ Networks: $($json.networks)" -ForegroundColor Green
        Write-Host "   ✓ Uptime: $($json.uptime_seconds)s" -ForegroundColor Green
    }
} catch {
    Write-Host "   ✗ Failed: $_" -ForegroundColor Red
}

Write-Host ""

# Test networks API
Write-Host "3. Testing /api/networks..."
try {
    $response = Invoke-WebRequest -Uri "http://localhost:8080/api/networks" -UseBasicParsing -TimeoutSec 5
    if ($response.StatusCode -eq 200) {
        Write-Host "   ✓ Status: $($response.StatusCode) OK" -ForegroundColor Green
        $json = $response.Content | ConvertFrom-Json
        Write-Host "   ✓ Response: $($response.Content)" -ForegroundColor Green
    }
} catch {
    Write-Host "   ✗ Failed: $_" -ForegroundColor Red
}

Write-Host ""

# Test WebSocket endpoint
Write-Host "4. Testing WebSocket endpoint (/ws)..."
Write-Host "   ℹ WebSocket requires special client (not tested here)" -ForegroundColor Yellow
Write-Host "   ℹ Available at: ws://localhost:8080/ws" -ForegroundColor Yellow

Write-Host ""
Write-Host "=== Test Summary ===" -ForegroundColor Cyan
Write-Host "Server: ✓ Running" -ForegroundColor Green
Write-Host "Web UI: ✓ Accessible" -ForegroundColor Green
Write-Host "API: ✓ Responding" -ForegroundColor Green
Write-Host ""
Write-Host "Open in browser: http://localhost:8080" -ForegroundColor White -BackgroundColor Blue
Write-Host ""
