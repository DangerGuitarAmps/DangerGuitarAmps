@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /I. /IAudioDSPTools /Ieigen tests\stereo_ir_test.cpp AudioDSPTools\dsp\dsp.cpp AudioDSPTools\dsp\ImpulseResponse.cpp AudioDSPTools\dsp\RecursiveLinearFilter.cpp AudioDSPTools\dsp\wav.cpp /Fe:%TEMP%\danger-stereo-ir-test.exe
if errorlevel 1 exit /b %errorlevel%
%TEMP%\danger-stereo-ir-test.exe
