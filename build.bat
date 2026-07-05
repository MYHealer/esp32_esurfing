@echo off
setlocal enabledelayedexpansion

REM Clear MSYS environment variables
set MSYSTEM=
set MSYS=
set MSYSCON=
set MINGW_PREFIX=
set MINGW_CHOST=
set MINGW_PACKAGE_PREFIX=
set CHERE_INVOKING=
set MSYS2_PATH_TYPE=

set PY=C:\Users\MR\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe
set IDF=E:\ESP\.espressif\v6.0.1\esp-idf
set PRJ=E:\Downloads\Compressed\esp32_esurfing
set LOG=%PRJ%\build_output.log

cd /d %PRJ%

echo Build started at %DATE% %TIME% > %LOG%

REM Clean
if exist sdkconfig del /f sdkconfig 2>nul
if exist build rd /s /q build 2>nul

echo [1/2] set-target esp32c3...
%PY% %IDF%\tools\idf.py -C %PRJ% set-target esp32c3 >> %LOG% 2>&1
if errorlevel 1 (
    echo [FAIL] set-target failed >> %LOG%
    type %LOG%
    goto end
)

echo [2/2] build...
%PY% %IDF%\tools\idf.py -C %PRJ% build >> %LOG% 2>&1
if errorlevel 1 (
    echo [FAIL] build failed >> %LOG%
    type %LOG%
    goto end
)

echo BUILD SUCCESS >> %LOG%
echo ============================================
echo BUILD SUCCESS
echo ============================================
dir /b %PRJ%\build\*.bin 2>nul

:end
echo.
echo Full log: %LOG%
pause
