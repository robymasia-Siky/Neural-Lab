@echo off
title NEURAL LAB - AI Engine (GPU 8GB)
echo ========================================================
echo  Avvio NEURAL LAB AI Engine...
echo ========================================================
call .venv\Scripts\activate.bat
python ai_engine\server.py
pause
