@echo off
setlocal

REM Step 1: Run serial.exe
f:\_Work\_Programing\rpi\set_boot_mode\serial_set.exe  -b 1200
if %ERRORLEVEL%==1 (
    echo serial.exe returned 1. Noserial port trying to find RPI drive....
rem    exit /b 1
)

REM Step 2: Find drive with label "RPI-RP2"
for %%D in (A B C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    vol %%D: 2>nul | findstr /C:"RPI-RP2" >nul
    if not errorlevel 1 (
        set "rpidrive=%%D:"
        goto :found
    )
)

echo Drive with label RPI-RP2 not found.
exit /b 1

:found
echo Found RPI-RP2 on drive %rpidrive%
copy /Y cmake-build-debug-system-arm\car_lighting.uf2  %rpidrive%\
if errorlevel 1 (
    echo Failed to copy file.
    exit /b 1
)

echo File copied successfully.
exit /b 0