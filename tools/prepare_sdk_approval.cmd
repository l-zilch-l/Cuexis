@echo off
setlocal
if defined CUEXIS_APPROVAL_WINDOW goto run
set "CUEXIS_APPROVAL_WINDOW=1"
"%ComSpec%" /d /k ""%~f0" %*"
exit /b %ERRORLEVEL%

:run
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare_sdk_approval.ps1" %*
set "approval_exit=%ERRORLEVEL%"
echo.
echo Assistant exit code: %approval_exit%
echo This window stays open. Close it yourself when finished.
exit /b %approval_exit%
