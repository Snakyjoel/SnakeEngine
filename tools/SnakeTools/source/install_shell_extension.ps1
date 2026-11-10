param(
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

$clsid = "{A7F013BD-DE2B-4A1C-9E5C-FCE0CC89B663}"
$extensions = @(".rawtex", ".t3x", ".snaky")

if ($Uninstall) {
    Write-Host "Uninstalling Snake Tools Shell Extension & Associations..." -ForegroundColor Yellow
    Remove-Item "HKCU:\Software\Classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "HKCU:\Software\Classes\SnakeTools.ThumbnailProvider" -Recurse -Force -ErrorAction SilentlyContinue
    foreach ($ext in $extensions) {
        $progId = "SnakeTools$ext"
        Remove-Item "HKCU:\Software\Classes\$ext" -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item "HKCU:\Software\Classes\SystemFileAssociations\$ext" -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item "HKCU:\Software\Classes\$progId" -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\$ext" -Recurse -Force -ErrorAction SilentlyContinue
    }
    Remove-Item "HKCU:\Software\Classes\.adp" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "HKCU:\Software\Classes\SnakeTools.adp" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.adp" -Recurse -Force -ErrorAction SilentlyContinue

    $sig = @'
[DllImport("shell32.dll")]
public static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
'@
    try {
        $shell32 = Add-Type -MemberDefinition $sig -Name "ShellNotifyWinUninst" -Namespace "WinShell" -PassThru -ErrorAction SilentlyContinue
        $shell32::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)
    } catch {}

    Write-Host "Shell extension and associations successfully removed!" -ForegroundColor Green
    exit 0
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
if ([string]::IsNullOrEmpty($scriptDir)) {
    $scriptDir = Get-Location
}

# 1. Locate SnakeEngineShellExt.dll
$searchDirs = @(
    $scriptDir,
    (Join-Path $scriptDir "source"),
    (Join-Path $scriptDir ".."),
    (Join-Path $scriptDir "..\source"),
    (Join-Path $scriptDir "tools\SnakeTools"),
    (Join-Path $scriptDir "tools\SnakeTools\source")
)

$dllPath = $null
foreach ($d in $searchDirs) {
    $candidate = Join-Path $d "SnakeEngineShellExt.dll"
    if (Test-Path $candidate) {
        $dllPath = (Resolve-Path $candidate).Path
        break
    }
}

if (-not $dllPath) {
    Write-Error "Could not find SnakeEngineShellExt.dll in any expected directories."
    exit 1
}

# 2. Locate SnakeTools.exe
$snakeToolsPath = $null
foreach ($d in $searchDirs) {
    $candidate = Join-Path $d "SnakeTools.exe"
    if (Test-Path $candidate) {
        $snakeToolsPath = (Resolve-Path $candidate).Path
        break
    }
}
if (-not $snakeToolsPath) {
    $snakeToolsPath = Join-Path $scriptDir "SnakeTools.exe"
}

Write-Host "Registering Snake Tools Shell Extension & File Associations..." -ForegroundColor Cyan
Write-Host "  DLL : $dllPath" -ForegroundColor DarkGray
Write-Host "  EXE : $snakeToolsPath" -ForegroundColor DarkGray

# 3. Register CLSID
$clsid = "{A7F013BD-DE2B-4A1C-9E5C-FCE0CC89B663}"
$providerGuid = "{E357FCCD-A995-4576-B01F-234630154E96}"
$codebaseUri = "file:///$($dllPath.Replace('\', '/'))"

$clsidPath = "HKCU:\Software\Classes\CLSID\$clsid"
if (Test-Path $clsidPath) { Remove-Item $clsidPath -Recurse -Force -ErrorAction SilentlyContinue }
$null = New-Item $clsidPath -Force
Set-ItemProperty $clsidPath -Name "(Default)" -Value "SnakeEngine Rawtex/T3x/Snaky Thumbnail Provider"
Set-ItemProperty $clsidPath -Name "DisableProcessIsolation" -Value 1 -Type DWord

$inprocPath = Join-Path $clsidPath "InprocServer32"
$null = New-Item $inprocPath -Force
Set-ItemProperty $inprocPath -Name "(Default)" -Value "mscoree.dll"
Set-ItemProperty $inprocPath -Name "ThreadingModel" -Value "Both"
Set-ItemProperty $inprocPath -Name "Class" -Value "SnakeEngineShellExt.RawtexThumbnailProvider"
Set-ItemProperty $inprocPath -Name "Assembly" -Value "SnakeEngineShellExt, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null"
Set-ItemProperty $inprocPath -Name "RuntimeVersion" -Value "v4.0.30319"
Set-ItemProperty $inprocPath -Name "CodeBase" -Value $codebaseUri

$inprocVerPath = Join-Path $inprocPath "1.0.0.0"
$null = New-Item $inprocVerPath -Force
Set-ItemProperty $inprocVerPath -Name "Class" -Value "SnakeEngineShellExt.RawtexThumbnailProvider"
Set-ItemProperty $inprocVerPath -Name "Assembly" -Value "SnakeEngineShellExt, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null"
Set-ItemProperty $inprocVerPath -Name "RuntimeVersion" -Value "v4.0.30319"
Set-ItemProperty $inprocVerPath -Name "CodeBase" -Value $codebaseUri

$catPath = Join-Path $clsidPath "Implemented Categories\{62C8FE65-4EBB-45E7-B440-6E39B2CDBF29}"
$null = New-Item $catPath -Force

$progClsidPath = "HKCU:\Software\Classes\SnakeTools.ThumbnailProvider\CLSID"
$null = New-Item $progClsidPath -Force
Set-ItemProperty $progClsidPath -Name "(Default)" -Value $clsid

# 4. Register Extensions, SystemFileAssociations and Thumbnail Providers
$extensions = @(".rawtex", ".t3x", ".snaky")
foreach ($ext in $extensions) {
    $progId = "SnakeTools$ext"
    $perceived = if ($ext -eq ".snaky") { "video" } else { "image" }
    $treatment = if ($ext -eq ".snaky") { 3 } else { 2 }

    # Extension key
    $extPath = "HKCU:\Software\Classes\$ext"
    if (-not (Test-Path $extPath)) { $null = New-Item $extPath -Force }
    Set-ItemProperty $extPath -Name "(Default)" -Value $progId
    Set-ItemProperty $extPath -Name "PerceivedType" -Value $perceived

    $shellexPath = Join-Path $extPath "ShellEx\$providerGuid"
    if (Test-Path $shellexPath) { Remove-Item $shellexPath -Recurse -Force -ErrorAction SilentlyContinue }
    $null = New-Item $shellexPath -Force
    Set-ItemProperty $shellexPath -Name "(Default)" -Value $clsid

    # SystemFileAssociations
    $sfaPath = "HKCU:\Software\Classes\SystemFileAssociations\$ext"
    if (-not (Test-Path $sfaPath)) { $null = New-Item $sfaPath -Force }
    Set-ItemProperty $sfaPath -Name "PerceivedType" -Value $perceived
    Set-ItemProperty $sfaPath -Name "Treatment" -Value $treatment -Type DWord

    $sfaShellexPath = Join-Path $sfaPath "ShellEx\$providerGuid"
    if (Test-Path $sfaShellexPath) { Remove-Item $sfaShellexPath -Recurse -Force -ErrorAction SilentlyContinue }
    $null = New-Item $sfaShellexPath -Force
    Set-ItemProperty $sfaShellexPath -Name "(Default)" -Value $clsid

    # ProgID
    $progPath = "HKCU:\Software\Classes\$progId"
    if (Test-Path $progPath) { Remove-Item $progPath -Recurse -Force -ErrorAction SilentlyContinue }
    $null = New-Item $progPath -Force
    Set-ItemProperty $progPath -Name "(Default)" -Value "Snake Tools Asset ($ext)"
    Set-ItemProperty $progPath -Name "FriendlyTypeName" -Value "Snake Tools Asset ($ext)"

    $progShellexPath = Join-Path $progPath "ShellEx\$providerGuid"
    $null = New-Item $progShellexPath -Force
    Set-ItemProperty $progShellexPath -Name "(Default)" -Value $clsid

    $iconPath = Join-Path $progPath "DefaultIcon"
    $null = New-Item $iconPath -Force
    Set-ItemProperty $iconPath -Name "(Default)" -Value "$snakeToolsPath,0"

    $cmdPath = Join-Path $progPath "shell\open\command"
    $null = New-Item $cmdPath -Force
    Set-ItemProperty $cmdPath -Name "(Default)" -Value "`"$snakeToolsPath`" `"%1`""

    # Clear stale UserChoice in Explorer\FileExts if any
    $ucPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\$ext\UserChoice"
    if (Test-Path $ucPath) { Remove-Item $ucPath -Recurse -Force -ErrorAction SilentlyContinue }
    
    $openWithProgids = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\$ext\OpenWithProgids"
    if (-not (Test-Path $openWithProgids)) { $null = New-Item $openWithProgids -Force }
    Set-ItemProperty $openWithProgids -Name $progId -Value ([byte[]]@()) -Type None
}

# 5. .adp audio association
$adpExtPath = "HKCU:\Software\Classes\.adp"
if (-not (Test-Path $adpExtPath)) { $null = New-Item $adpExtPath -Force }
Set-ItemProperty $adpExtPath -Name "(Default)" -Value "SnakeTools.adp"
Set-ItemProperty $adpExtPath -Name "PerceivedType" -Value "audio"

$progAdpPath = "HKCU:\Software\Classes\SnakeTools.adp"
if (Test-Path $progAdpPath) { Remove-Item $progAdpPath -Recurse -Force -ErrorAction SilentlyContinue }
$null = New-Item $progAdpPath -Force
Set-ItemProperty $progAdpPath -Name "(Default)" -Value "Snake Tools ADP Audio"

$iconAdpPath = Join-Path $progAdpPath "DefaultIcon"
$null = New-Item $iconAdpPath -Force
Set-ItemProperty $iconAdpPath -Name "(Default)" -Value "$snakeToolsPath,0"

$cmdAdpPath = Join-Path $progAdpPath "shell\open\command"
$null = New-Item $cmdAdpPath -Force
Set-ItemProperty $cmdAdpPath -Name "(Default)" -Value "`"$snakeToolsPath`" `"%1`""

# 6. Notify Windows Shell immediately
Write-Host "Broadcasting shell association updates..." -ForegroundColor Yellow
$sig = @'
[DllImport("shell32.dll")]
public static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
'@
try {
    $shell32 = Add-Type -MemberDefinition $sig -Name "ShellNotifyWin" -Namespace "WinShell" -PassThru -ErrorAction SilentlyContinue
    $shell32::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero) # SHCNE_ASSOCCHANGED
} catch {
}

Write-Host "Shell extension and associations successfully registered!" -ForegroundColor Green
