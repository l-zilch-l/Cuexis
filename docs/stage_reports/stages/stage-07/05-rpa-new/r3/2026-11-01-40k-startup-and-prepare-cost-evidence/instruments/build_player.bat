@echo off
rem Rebuild one target inside a Visual Studio developer environment. The plain shell has no MSVC
rem INCLUDE/LIB, so cl.exe cannot even find the standard library headers there.
setlocal
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d D:\Cuexis-redesign
cmake --build out\build\rpa-candidate-release --target cuexis_player
