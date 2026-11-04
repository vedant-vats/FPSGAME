@echo off
rem Two game windows with fake lag: 75 ms each way = ~150 ms round trip.
rem Change LAG below to test other pings (value = one way, ms).
set LAG=75
set UE="C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe"
set PROJ="%~dp0FPS.uproject"
start "Host" %UE% %PROJ% -game -windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=40 -log -ABSLOG="%~dp0Saved\Logs\Host.log" -ExecCmds="NetEmulation.PktLag %LAG%"
timeout /t 3 >nul
start "Client" %UE% %PROJ% -game -windowed -ResX=1280 -ResY=720 -WinX=1290 -WinY=40 -log -ABSLOG="%~dp0Saved\Logs\Client.log" -ExecCmds="NetEmulation.PktLag %LAG%"
