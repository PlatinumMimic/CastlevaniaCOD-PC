@echo off
rem A timing report of the game on this machine. Start the game with this file instead of the launcher, play for
rem two or three minutes in the places that run slowly, then quit the game as usual: the report is written to
rem logs\perf-report.log next to this file (how long each part of a frame takes on the graphics card and on the
rem processor, once a second). Send that file with a performance report.
set XBE_PERF=1
cd /d "%~dp0"
"%~dp0Castlevania.exe"
copy /y "%~dp0logs\castlevania.log" "%~dp0logs\perf-report.log" >nul
echo The report is in logs\perf-report.log
pause
