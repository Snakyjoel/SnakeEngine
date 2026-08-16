@echo off
title SnakeEngine PC Asset Converter
echo Installing required Python libraries (Pillow)...
pip install Pillow
echo.
echo Running conversion script...
python "%~dp0convert_assets.py"
echo.
pause
