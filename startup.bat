@echo off
title IoT Digital Clock Simulator Server
cd /d "%~dp0"
echo =========================================================
echo  KHOI DONG TRINH GIA LAP DONG HO DIEN TU - IoT102
echo =========================================================
echo [*] Dang mo trinh duyet...
start http://localhost:6009/digital_clock_simulator.html
echo [*] Dang chay HTTP Server tren cong 6009...
echo [*] Nhan Ctrl+C trong cua so nay de dung server.
echo =========================================================
python -m http.server 6009
