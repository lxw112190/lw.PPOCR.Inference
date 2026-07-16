@echo off
setlocal
start "" /b powershell -NoProfile -WindowStyle Hidden -Command ^
  "$url='http://127.0.0.1:8787/health'; for($i=0;$i -lt 100;$i++){try{Invoke-WebRequest -UseBasicParsing $url -TimeoutSec 1|Out-Null; Start-Process 'http://127.0.0.1:8787/'; break}catch{Start-Sleep -Milliseconds 200}}"
"%~dp0lw.PPOCR.HttpService.exe" --console --config "%~dp0http-service.json"
if errorlevel 1 pause
