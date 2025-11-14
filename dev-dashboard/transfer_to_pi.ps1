# PowerShell script to transfer dev-dashboard files to Raspberry Pi
# Run this script to copy all files to the Pi

$piUser = "cex"
$piHost = "192.168.31.28"
$piPath = "/home/cex/dev-dashboard"
$localPath = "C:\Users\cemfi\seismograph-system\dev-dashboard"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "QSafe Dev Dashboard - File Transfer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Transferring files to:" -ForegroundColor Yellow
Write-Host "  ${piUser}@${piHost}:${piPath}" -ForegroundColor White
Write-Host ""
Write-Host "You will be prompted for SSH password: orhint35" -ForegroundColor Yellow
Write-Host ""

# Create directory on Pi
Write-Host "[1/3] Creating directory on Pi..." -ForegroundColor Green
ssh ${piUser}@${piHost} "mkdir -p ${piPath}/templates"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Failed to connect to Pi. Check SSH connection." -ForegroundColor Red
    exit 1
}

# Transfer files
Write-Host "[2/3] Transferring files..." -ForegroundColor Green

$files = @(
    "setup_pi.sh",
    "dashboard_server.py",
    "requirements.txt",
    "README.md",
    "SETUP_INSTRUCTIONS.md",
    "FILES_SUMMARY.txt",
    "QUICK_REFERENCE.md",
    "templates/dashboard.html"
)

foreach ($file in $files) {
    $source = Join-Path $localPath $file
    $dest = "${piUser}@${piHost}:${piPath}/$file"

    Write-Host "  Copying $file..." -ForegroundColor Gray
    scp $source $dest

    if ($LASTEXITCODE -ne 0) {
        Write-Host "  Warning: Failed to copy $file" -ForegroundColor Yellow
    }
}

# Make setup script executable
Write-Host "[3/3] Making setup script executable..." -ForegroundColor Green
ssh ${piUser}@${piHost} "chmod +x ${piPath}/setup_pi.sh"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Transfer Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. SSH to Pi: ssh ${piUser}@${piHost}" -ForegroundColor White
Write-Host "2. Run setup: cd ${piPath} && ./setup_pi.sh" -ForegroundColor White
Write-Host ""
