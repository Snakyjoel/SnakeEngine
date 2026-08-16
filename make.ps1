# make.ps1 - Wrapper para ejecutar make desde PowerShell usando el MSYS2 de devkitPro
# Uso: .\make.ps1          -> compila el proyecto
#      .\make.ps1 clean    -> limpia el build

$target = if ($args.Count -gt 0) { $args[0] } else { "" }

$bash    = "C:\devkitPro\msys2\usr\bin\bash.exe"
$projDir = Split-Path -Parent $MyInvocation.MyCommand.Path
# Convertir ruta Windows a ruta MSYS2 (C:\ -> /c/)
$msysDir = "/" + ($projDir -replace "\\", "/" -replace "^([A-Za-z]):", '$1').ToLower()[0] + ($projDir -replace "\\", "/" -replace "^[A-Za-z]:", "")

$env_setup = "export PATH=/usr/bin:/opt/devkitpro/devkitARM/bin:/opt/devkitpro/tools/bin:`$PATH"
$env_setup += " && export DEVKITPRO=/opt/devkitpro"
$env_setup += " && export DEVKITARM=/opt/devkitpro/devkitARM"
$cmd = "$env_setup && cd '$msysDir' && make $target"

Write-Host ">>> Ejecutando: make $target" -ForegroundColor Cyan
& $bash -c $cmd
