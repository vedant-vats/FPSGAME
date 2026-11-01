@echo off
rem Launches two separate game windows for multiplayer testing (no editor).
set UE="C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe"
set PROJ="%~dp0FPS.uproject"
start "Host" %UE% %PROJ% -game -windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=40 -log -ABSLOG="%~dp0Saved\Logs\Host.log"
timeout /t 3 >nul
start "Client" %UE% %PROJ% -game -windowed -ResX=1280 -ResY=720 -WinX=1290 -WinY=40 -log -ABSLOG="%~dp0Saved\Logs\Client.log"
