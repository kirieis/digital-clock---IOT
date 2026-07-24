@echo off
title IoT Digital Clock Simulator Server
cd /d "%~dp0"
echo =========================================================
echo  KHOI DONG TRINH GIA LAP DONG HO DIEN TU - IoT102
echo =========================================================

:: Kiem tra Python thuc su co chay duoc khong (tranh bi lua boi Windows App Execution Alias)
python -c "import sys" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Tim thay Python. Dang mo trinh duyet va chay HTTP Server...
    start http://localhost:6009/digital_clock_simulator.html
    echo [*] Dang chay HTTP Server tren cong 6009...
    echo [*] Nhan Ctrl+C trong cua so nay de dung server.
    echo =========================================================
    python -m http.server 6009
    goto END
)

py -c "import sys" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Tim thay Python Launcher 'py'. Dang mo trinh duyet va chay HTTP Server...
    start http://localhost:6009/digital_clock_simulator.html
    echo [*] Dang chay HTTP Server tren cong 6009...
    echo [*] Nhan Ctrl+C trong cua so nay de dung server.
    echo =========================================================
    py -m http.server 6009
    goto END
)

:: Neu khong co Python thuc su, tu dong mo truc tiep file HTML bang trinh duyet
echo [!] May tinh chua cai dat Python (hoac Python chua duoc them vao PATH).
echo [*] Tu dong mo truc tiep file digital_clock_simulator.html tren trinh duyet...
echo =========================================================
start "" "%~dp0digital_clock_simulator.html"
echo [OK] Da mo giao dien Web Simulator thanh cong!
echo.
pause

:END
