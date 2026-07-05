# Build script - uses PowerShell-native approach
# Run from PowerShell directly (not from Git Bash)

$ErrorActionPreference = "Continue"
$env:MSYSTEM = ""
$env:MSYSCON = ""
$env:MINGW_PREFIX = ""
$env:MINGW_CHOST = ""
$env:CHERE_INVOKING = ""
$env:MSYS2_PATH_TYPE = ""

$IDF_PATH = "E:\ESP\.espressif\v6.0.1\esp-idf"
$IDF_Python = "C:\Users\MR\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe"
$ProjectDir = "E:\Downloads\Compressed\esp32_esurfing"

Set-Location $ProjectDir

# Clean
Remove-Item -Force sdkconfig -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force managed_components -ErrorAction SilentlyContinue

Write-Host "=== Build ESurfingClient for ESP32-C3 ===" -ForegroundColor Cyan

# Set target
Write-Host "[1/2] set-target esp32c3..." -ForegroundColor Yellow
& $IDF_Python $IDF_PATH\tools\idf.py -C $ProjectDir set-target esp32c3 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] set-target failed" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "[2/2] Building..." -ForegroundColor Yellow
& $IDF_Python $IDF_PATH\tools\idf.py -C $ProjectDir build 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -eq 0) {
    Write-Host "`n============================================" -ForegroundColor Green
    Write-Host "BUILD SUCCESS!" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Get-ChildItem $ProjectDir\build\*.bin | ForEach-Object { Write-Host "  $($_.Name) - $([math]::Round($_.Length/1024)) KB" }
    Write-Host "`nFlash:" -ForegroundColor Cyan
    Write-Host "  . E:\ESP\.espressif\v6.0.1\esp-idf\export.ps1" -ForegroundColor White
    Write-Host "  idf.py -p COM14 flash monitor`n" -ForegroundColor White
} else {
    Write-Host "`nBUILD FAILED" -ForegroundColor Red
}
