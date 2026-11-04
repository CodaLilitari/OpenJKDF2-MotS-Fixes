@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup GeoSith.ps1" %*
if errorlevel 1 (
  echo Setup failed. Check the message above and use a writable Jedi Knight folder.
  pause
  exit /b 1
)
pause
