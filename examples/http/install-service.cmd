@echo off
setlocal
net session >nul 2>&1
if errorlevel 1 (
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)
"%~dp0lw.PPOCR.HttpService.exe" --install --config "%~dp0http-service.json"
pause
