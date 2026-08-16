# SnakeEngine Shell Extension & Viewer Installer
# Non-Admin, 100% User-Level Installation Script

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
if ([string]::IsNullOrEmpty($scriptDir)) {
    $scriptDir = Get-Location
}

# Paths
$cscPath = "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
$dllPath = Join-Path $scriptDir "SnakeEngineShellExt_v5.dll"
$viewerPath = Join-Path $scriptDir "SnakeEngineViewer.exe"

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Installing SnakeEngine .rawtex / .t3x Extension" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

# 1. Compilation
Write-Host "1. Compiling Shell Extension and Viewer..." -ForegroundColor Yellow
if (-not (Test-Path $cscPath)) {
    throw "C# compiler not found at $cscPath. Please install .NET Framework 4.0 or newer."
}

# Compile DLL
Write-Host "   Compiling SnakeEngineShellExt.dll..."
& $cscPath /target:library /r:System.Drawing.dll /out:$dllPath (Join-Path $scriptDir "SnakeEngineShellExt.cs")

# Compile Viewer EXE
Write-Host "   Compiling SnakeEngineViewer.exe launcher..."
& $cscPath /target:winexe /r:System.Windows.Forms.dll /out:$viewerPath (Join-Path $scriptDir "SnakeEngineViewer.cs")

Write-Host "   Compilation SUCCESSFUL!" -ForegroundColor Green

# 2. Registry Registration (HKCU - No Admin Required!)
Write-Host "2. Registering File Associations and Thumbnail Provider..." -ForegroundColor Yellow

$clsid = "{A7F013BD-DE2B-4A1C-9E5C-FCE0CC89B663}"
$providerGuid = "{E357FCCD-A995-4576-B01F-234630154E96}" # IThumbnailProvider interface GUID

# Create CLSID
$clsidPath = "HKCU:\Software\Classes\CLSID\$clsid"
if (Test-Path $clsidPath) { Remove-Item $clsidPath -Recurse -Force -ErrorAction SilentlyContinue }
$null = New-Item $clsidPath -Force
Set-ItemProperty $clsidPath -Name "(Default)" -Value "SnakeEngine Rawtex/T3x Thumbnail Provider"

$inprocPath = Join-Path $clsidPath "InprocServer32"
$null = New-Item $inprocPath -Force
Set-ItemProperty $inprocPath -Name "(Default)" -Value "C:\Windows\System32\mscoree.dll"
Set-ItemProperty $inprocPath -Name "ThreadingModel" -Value "Both"
Set-ItemProperty $inprocPath -Name "Class" -Value "SnakeEngineShellExt.RawtexThumbnailProvider"
Set-ItemProperty $inprocPath -Name "Assembly" -Value "SnakeEngineShellExt, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null"
Set-ItemProperty $inprocPath -Name "RuntimeVersion" -Value "v4.0.30319"
Set-ItemProperty $inprocPath -Name "CodeBase" -Value "file:///$($dllPath.Replace('\', '/'))"

# IMPORTANT: DisableProcessIsolation allows managed COM shell extensions to run in Explorer if dllhost fails
Set-ItemProperty $clsidPath -Name "DisableProcessIsolation" -Value 1 -Type DWord

# File Associations and Thumbnail Handlers
$extensions = @(".rawtex", ".t3x", ".snaky")
foreach ($ext in $extensions) {
    # HKCU:\Software\Classes\.ext
    $extPath = "HKCU:\Software\Classes\$ext"
    if (-not (Test-Path $extPath)) { $null = New-Item $extPath -Force }
    Set-ItemProperty $extPath -Name "(Default)" -Value "SnakeEngine$ext"

    if ($ext -eq ".snaky") {
        Set-ItemProperty $extPath -Name "PerceivedType" -Value "video"
    } else {
        Set-ItemProperty $extPath -Name "PerceivedType" -Value "image"
    }
    
    # ShellEx Thumbnail Provider GUID registration
    $shellexPath = Join-Path $extPath "ShellEx\$providerGuid"
    if (Test-Path $shellexPath) { Remove-Item $shellexPath -Recurse -Force -ErrorAction SilentlyContinue }
    $null = New-Item $shellexPath -Force
    Set-ItemProperty $shellexPath -Name "(Default)" -Value $clsid
}

# Association for .adp audio files
$adpExtPath = "HKCU:\Software\Classes\.adp"
if (-not (Test-Path $adpExtPath)) { $null = New-Item $adpExtPath -Force }
Set-ItemProperty $adpExtPath -Name "(Default)" -Value "SnakeEngine.adp"

# ProgID Details for .rawtex
$progRawPath = "HKCU:\Software\Classes\SnakeEngine.rawtex"
if (Test-Path $progRawPath) { Remove-Item $progRawPath -Recurse -Force -ErrorAction SilentlyContinue }
$null = New-Item $progRawPath -Force
Set-ItemProperty $progRawPath -Name "(Default)" -Value "SnakeEngine Rawtex Texture File"

$iconRawPath = Join-Path $progRawPath "DefaultIcon"
$null = New-Item $iconRawPath -Force
Set-ItemProperty $iconRawPath -Name "(Default)" -Value "$viewerPath,0"

$cmdRawPath = Join-Path $progRawPath "shell\open\command"
$null = New-Item $cmdRawPath -Force
Set-ItemProperty $cmdRawPath -Name "(Default)" -Value "`"$viewerPath`" `"%1`""

# ProgID Details for .t3x
$progT3xPath = "HKCU:\Software\Classes\SnakeEngine.t3x"
if (Test-Path $progT3xPath) { Remove-Item $progT3xPath -Recurse -Force -ErrorAction SilentlyContinue }
$null = New-Item $progT3xPath -Force
Set-ItemProperty $progT3xPath -Name "(Default)" -Value "SnakeEngine T3X Texture File"

$iconT3xPath = Join-Path $progT3xPath "DefaultIcon"
$null = New-Item $iconT3xPath -Force
Set-ItemProperty $iconT3xPath -Name "(Default)" -Value "$viewerPath,0"

$cmdT3xPath = Join-Path $progT3xPath "shell\open\command"
$null = New-Item $cmdT3xPath -Force
Set-ItemProperty $cmdT3xPath -Name "(Default)" -Value "`"$viewerPath`" `"%1`""

# ProgID Details for .adp
$progAdpPath = "HKCU:\Software\Classes\SnakeEngine.adp"
if (Test-Path $progAdpPath) { Remove-Item $progAdpPath -Recurse -Force -ErrorAction SilentlyContinue }
$null = New-Item $progAdpPath -Force
Set-ItemProperty $progAdpPath -Name "(Default)" -Value "SnakeEngine ADP Audio File"

$iconAdpPath = Join-Path $progAdpPath "DefaultIcon"
$null = New-Item $iconAdpPath -Force
Set-ItemProperty $iconAdpPath -Name "(Default)" -Value "$viewerPath,0"

$cmdAdpPath = Join-Path $progAdpPath "shell\open\command"
$null = New-Item $cmdAdpPath -Force
Set-ItemProperty $cmdAdpPath -Name "(Default)" -Value "`"$viewerPath`" `"%1`""

# ProgID Details for .snaky
$progSnakyPath = "HKCU:\Software\Classes\SnakeEngine.snaky"
if (Test-Path $progSnakyPath) { Remove-Item $progSnakyPath -Recurse -Force -ErrorAction SilentlyContinue }
$null = New-Item $progSnakyPath -Force
Set-ItemProperty $progSnakyPath -Name "(Default)" -Value "SnakeEngine Snaky Video"

$iconSnakyPath = Join-Path $progSnakyPath "DefaultIcon"
$null = New-Item $iconSnakyPath -Force
Set-ItemProperty $iconSnakyPath -Name "(Default)" -Value "$viewerPath,0"

$cmdSnakyPath = Join-Path $progSnakyPath "shell\open\command"
$null = New-Item $cmdSnakyPath -Force
Set-ItemProperty $cmdSnakyPath -Name "(Default)" -Value "`"$viewerPath`" `"%1`""

Write-Host "   Registry Registration SUCCESSFUL!" -ForegroundColor Green

# 3. Explorer Cache Flush
Write-Host "3. Restarting Windows Explorer and clearing Thumbcache..." -ForegroundColor Yellow
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Stop-Process -Name dllhost -Force -ErrorAction SilentlyContinue

Start-Sleep -Seconds 2
$cacheDir = "$env:LOCALAPPDATA\Microsoft\Windows\Explorer"
Get-ChildItem -Path $cacheDir -Filter "thumbcache_*.db" | Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $cacheDir -Filter "iconcache_*.db" | Remove-Item -Force -ErrorAction SilentlyContinue

Start-Process explorer

Write-Host "=============================================" -ForegroundColor Green
Write-Host "INSTALLATION COMPLETED SUCCESSFULLY!" -ForegroundColor Green
Write-Host "Double-click any .rawtex, .t3x, or .adp file to view/play it." -ForegroundColor Green
Write-Host "Explorer will now display thumbnails automatically!" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green
