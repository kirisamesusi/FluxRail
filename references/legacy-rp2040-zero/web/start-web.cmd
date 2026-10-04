@echo off
cd /d "%~dp0"
echo Open http://127.0.0.1:8000 in Chrome or Edge.
python "%~dp0serve.py"
pause
