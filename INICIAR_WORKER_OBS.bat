@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
    echo Ambiente .venv nao encontrado. Prepare-o conforme DESENVOLVIMENTO.md.
    pause
    exit /b 1
)
".venv\Scripts\python.exe" -m teleport_cam.obs_worker
if errorlevel 1 pause
