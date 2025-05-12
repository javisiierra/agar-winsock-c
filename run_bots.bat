@echo off
setlocal enabledelayedexpansion

set BOTS=5

for /L %%i in (1,1,%BOTS%) do (
    start python bot_client.py
    timeout /t 1 >nul
)