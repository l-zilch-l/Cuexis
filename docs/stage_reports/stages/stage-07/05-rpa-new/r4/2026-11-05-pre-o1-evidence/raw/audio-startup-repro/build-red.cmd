@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
set "VCPKG_ROOT=D:\vcpkg"
cmake --build D:\Cuexis-worktree\out\build\r4\pre-o1\msvc-on-debug --target cuexis_execution_host_tests --parallel 4
exit /b %errorlevel%
