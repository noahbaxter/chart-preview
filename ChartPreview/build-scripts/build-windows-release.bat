@echo off
REM Chart Preview - Windows Release Build Script
REM Builds Windows VST3 plugin for release

setlocal enabledelayedexpansion

REM Configuration
set PLUGIN_NAME=ChartPreview
set VERSION=%1
if "%VERSION%"=="" set VERSION=dev
set RELEASE_DIR=..\releases
set BUILD_CONFIG=Release
set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

echo Building Chart Preview v%VERSION% for Windows...

REM Check if MSBuild exists
if not exist %MSBUILD_PATH% (
    set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
)
if not exist %MSBUILD_PATH% (
    set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
)
if not exist %MSBUILD_PATH% (
    echo ERROR: MSBuild not found. Please install Visual Studio 2022.
    exit /b 1
)

REM Create release directory
if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"

REM Navigate to project directory
cd /d "%~dp0"

REM Clean previous builds
echo Cleaning previous builds...
if exist "Builds\VisualStudio2022\x64" rd /s /q "Builds\VisualStudio2022\x64"

REM Regenerate project files if needed
echo Checking Projucer files...
if exist "ChartPreview.jucer" (
    for %%i in (ChartPreview.jucer) do set JUCER_TIME=%%~ti
    for %%i in (Builds\VisualStudio2022\ChartPreview.sln) do set SLN_TIME=%%~ti
    REM Note: Time comparison in batch is complex, skipping for now
    REM User can manually regenerate if needed
)

REM Build Windows VST3
echo Building Windows VST3...
cd Builds\VisualStudio2022

%MSBUILD_PATH% ChartPreview.sln -p:Configuration=%BUILD_CONFIG% -p:Platform=x64 -target:ChartPreview_VST3 -verbosity:minimal -nologo

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

REM Create release bundle
cd ..\..
echo Creating release bundle...

set WINDOWS_BUNDLE=%RELEASE_DIR%\ChartPreview-v%VERSION%-Windows
if exist "%WINDOWS_BUNDLE%" rd /s /q "%WINDOWS_BUNDLE%"
mkdir "%WINDOWS_BUNDLE%"

echo   Packaging Windows bundle...
xcopy "Builds\VisualStudio2022\x64\%BUILD_CONFIG%\VST3\ChartPreview.vst3" "%WINDOWS_BUNDLE%\ChartPreview.vst3\" /E /I /Y

REM Create install script for Windows
echo @echo off > "%WINDOWS_BUNDLE%\install-windows.bat"
echo echo Installing Chart Preview VST3... >> "%WINDOWS_BUNDLE%\install-windows.bat"
echo set VST3_DIR=%%COMMONPROGRAMFILES%%\VST3 >> "%WINDOWS_BUNDLE%\install-windows.bat"
echo if not exist "%%VST3_DIR%%" mkdir "%%VST3_DIR%%" >> "%WINDOWS_BUNDLE%\install-windows.bat"
echo xcopy ChartPreview.vst3 "%%VST3_DIR%%\ChartPreview.vst3\" /E /I /Y >> "%WINDOWS_BUNDLE%\install-windows.bat"
echo echo Installation complete! >> "%WINDOWS_BUNDLE%\install-windows.bat"
echo echo VST3 installed to: %%VST3_DIR%%\ChartPreview.vst3\ >> "%WINDOWS_BUNDLE%\install-windows.bat"
echo pause >> "%WINDOWS_BUNDLE%\install-windows.bat"

REM Create README file
echo Chart Preview v%VERSION% - Windows Release > "%WINDOWS_BUNDLE%\README.txt"
echo. >> "%WINDOWS_BUNDLE%\README.txt"
echo This bundle contains: >> "%WINDOWS_BUNDLE%\README.txt"
echo - ChartPreview.vst3 (VST3 plugin) >> "%WINDOWS_BUNDLE%\README.txt"
echo - install-windows.bat (automatic installer script) >> "%WINDOWS_BUNDLE%\README.txt"
echo. >> "%WINDOWS_BUNDLE%\README.txt"
echo Installation: >> "%WINDOWS_BUNDLE%\README.txt"
echo 1. Run install-windows.bat as Administrator for automatic installation >> "%WINDOWS_BUNDLE%\README.txt"
echo 2. Or manually copy ChartPreview.vst3 to: >> "%WINDOWS_BUNDLE%\README.txt"
echo    %%COMMONPROGRAMFILES%%\VST3\ (usually C:\Program Files\Common Files\VST3\) >> "%WINDOWS_BUNDLE%\README.txt"
echo. >> "%WINDOWS_BUNDLE%\README.txt"
echo Requirements: Windows 10 or later (x64) >> "%WINDOWS_BUNDLE%\README.txt"
echo Built: %date% %time% >> "%WINDOWS_BUNDLE%\README.txt"

REM Create ZIP archive
echo Creating ZIP archive...
powershell -command "Compress-Archive -Path '%WINDOWS_BUNDLE%' -DestinationPath '%RELEASE_DIR%\ChartPreview-v%VERSION%-Windows.zip' -Force"

REM Summary
echo.
echo ✅ Build complete!
echo 📦 Release bundle created:
echo    Windows: %RELEASE_DIR%\ChartPreview-v%VERSION%-Windows.zip
echo.
echo Bundle contents:
echo    Windows: VST3 plugin with installer
pause