@echo off
setlocal DisableDelayedExpansion
set "GEOSITH_HOME=%~dp0"
set "JKROOT=%~dp0JK"
if exist "%~dp0game-path.txt" set /p JKROOT=<"%~dp0game-path.txt"
if not exist "%JKROOT%\editor-resources.jkl" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup GeoSith.ps1"
  if errorlevel 1 (
    pause
    exit /b 1
  )
  set /p JKROOT=<"%~dp0game-path.txt"
)
set "OPENJKDF2_ROOT=%JKROOT%"
set "PATH=%GEOSITH_HOME%;%PATH%"
cd /d "%JKROOT%"
"%GEOSITH_HOME%GeoSith.exe" %*
