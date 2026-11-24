@echo off
rem Load the Visual Studio x64 developer environment, then run the command unchanged.
rem
rem Usage:
rem   tools\with-msvc.cmd
rem   tools\with-msvc.cmd cmake --preset debug --fresh
rem   tools\with-msvc.cmd cmake -S . -B out\build\scratch -G Ninja -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
rem
rem Unlike the PowerShell entry point, this launcher passes the command line through cmd.exe, so
rem arguments that contain a drive letter after '=' survive unchanged.
setlocal
set "CUEXIS_SAVED_VCPKG_ROOT=%VCPKG_ROOT%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo with-msvc: vswhere.exe was not found 1>&2
    exit /b 1
)
set "VSINSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo with-msvc: no Visual Studio installation with the x64 C++ toolchain was found 1>&2
    exit /b 1
)
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 >nul
if errorlevel 1 (
    echo with-msvc: VsDevCmd.bat failed with exit code %errorlevel% 1>&2
    exit /b 1
)
if defined CUEXIS_SAVED_VCPKG_ROOT set "VCPKG_ROOT=%CUEXIS_SAVED_VCPKG_ROOT%"
if "%~1"=="" (
    where cl.exe
    echo VCPKG_ROOT=%VCPKG_ROOT%
    exit /b 0
)
%*
exit /b %errorlevel%
