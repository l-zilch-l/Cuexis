@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d D:\Cuexis-redesign
cmake --build out\build\rpa-candidate-release --target cuexis_execution_device_probe
