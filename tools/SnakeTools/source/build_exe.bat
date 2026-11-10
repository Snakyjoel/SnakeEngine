@echo off
echo ========================================================
echo   Compilador de Snake Tools (Snake Engine - 3DS)
echo ========================================================

py -m PyInstaller -v >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set PYTHON_CMD=py
    goto compile
)

python -m PyInstaller -v >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set PYTHON_CMD=python
    goto compile
)

echo [ERROR] No se encontro Python en el sistema.
pause
exit /b 1

:compile
echo Usando: %PYTHON_CMD%
echo Instalando dependencias necesarias...
%PYTHON_CMD% -m pip install -r requirements.txt

echo.
echo Compilando SnakeTools.exe con herramientas integradas...
%PYTHON_CMD% -m PyInstaller --noconfirm --onefile --windowed --name "SnakeTools" --icon "icon.ico" --add-binary "bin/ffmpeg.exe;bin" --add-binary "bin/tex3ds.exe;bin" --add-binary "SnakeEngineShellExt.dll;." --add-data "install_shell_extension.ps1;." --copy-metadata imageio --copy-metadata moviepy main.py

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] La compilacion ha fallado.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Moviendo ejecutable a la carpeta principal...
copy /Y "dist\SnakeTools.exe" "..\SnakeTools.exe"

echo Limpiando temporales de compilacion...
if exist "build" rmdir /S /Q "build"
if exist "dist" rmdir /S /Q "dist"
if exist "SnakeTools.spec" del /Q "SnakeTools.spec"

echo ========================================================
echo   SnakeTools.exe generado correctamente en tools\SnakeTools\
echo ========================================================
pause
