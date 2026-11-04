@echo off
setlocal
cd /d "%~dp0JK"
set "OPENJKDF2_ROOT=%~dp0JK"
set "PATH=%~dp0build;%PATH%"
if not exist "%~dp0build\Release\openjkdf2-64.exe" (
  echo Build GeoSith first using tools\build.ps1.
  pause
  exit /b 1
)
"%~dp0build\Release\openjkdf2-64.exe" %*
