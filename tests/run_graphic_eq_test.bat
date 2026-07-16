@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /I. /INeuralAmpModeler /IAudioDSPTools /Ieigen tests\graphic_eq_test.cpp /Fe:%TEMP%\danger-graphic-eq-test.exe
if errorlevel 1 exit /b %errorlevel%
%TEMP%\danger-graphic-eq-test.exe
