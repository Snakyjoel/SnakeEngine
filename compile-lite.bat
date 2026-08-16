@echo off
set "ROOT=%~dp0"
echo --- Compilando Version LITE (Sin Videos) ---

cd /d "%ROOT%"

echo Buscando archivos PNG sin definicion T3S...
for /r "%ROOT%assets" %%F in (*.png) do (
    if not exist "%%~dpnF.t3s" (
        echo [AUTO] Generando definicion T3S para %%~nxF...
        (
            echo --atlas -f rgba4444
            echo %%~nxF
        ) > "%%~dpnF.t3s"
    )
)

if exist "romfs-lite" (
echo Eliminando carpeta romfs-lite...
rmdir /s /q "romfs-lite"
)

echo Limpiando cache de compilacion...
if exist "build" (
    del /s /q "build\*.o" >nul 2>&1
    del /s /q "build\*.d" >nul 2>&1
)

echo Compilando codigo fuente y assets Lite...
make cia-lite

if %ERRORLEVEL% NEQ 0 (
echo [ERROR] La compilacion ha fallado.
pause
exit /b %ERRORLEVEL%
)

echo --- Compilacion terminada ---
pause
