@echo off
echo ========================================================
echo SNAKE ENGINE - OGG TO ADP CONVERTER (DEPENDENCY-FREE)
echo ========================================================
echo.
echo NOTA: Este conversor requiere "ffmpeg" para decodificar los audios.
echo Si el script te da error, descarga "ffmpeg.exe" para Windows
echo y colocalo en esta misma carpeta (donde esta este archivo .bat).
echo.
echo Convirtiendo carpeta assets\songs...
python convert_to_adp.py --dir assets\songs
echo.
pause
