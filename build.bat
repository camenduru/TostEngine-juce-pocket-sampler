@echo off
REM Build script for TostEngineJucePocketSampler
REM Requires Visual Studio 2019 or later with JUCE

echo Building TostEngineJucePocketSampler...

setlocal

REM Set JUCE path
set JUCE_PATH=C:\Users\PC\Documents\JUCE

REM Check if JUCE exists
if not exist "%JUCE_PATH%\modules" (
    echo Error: JUCE modules not found at %JUCE_PATH%\modules
    echo Please update the JUCE_PATH variable in this script
    pause
    exit /b 1
)

REM Set MSVC environment
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    echo Error: Visual Studio not found
    pause
    exit /b 1
)

REM Build the project
devenv TostEngineJucePocketSampler.sln /Build Release /Project TostEngineJucePocketSampler

if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Output: Release\TostEngineJucePocketSampler.exe
) else (
    echo Build failed!
)

pause
endlocal
