@echo off
title NEURAL LAB - Sample Library Indexer (CLAP)
call .venv\Scripts\activate.bat
set /p SAMPLE_DIR="Trascina qui la tua cartella dei Sample/Loop e premi INVIO: "
python ai_engine\audio_indexer.py %SAMPLE_DIR%
pause
