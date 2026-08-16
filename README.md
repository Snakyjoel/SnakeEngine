# SnakeEngine

A Friday Night Funkin' engine for the Nintendo 3DS, built with devkitARM and libctru.

---

## Setup — Building from Source

### 1. Install devkitPro

- Download the official devkitPro installer for Windows from: https://github.com/devkitPro/installer/releases
- Run the installer and select **"3DS Development"** (this will automatically install `devkitARM`, `libctru`, `citro2d`, `citro3d`, and all required tools).

### 2. Install required libraries

Open the **devkitPro MSYS2** terminal and run:

```bash
dkp-pacman -S 3ds-dev 3ds-liblua51 3ds-zlib 3ds-libogg 3ds-libvorbisidec 3ds-jansson
```

Press `Y` and `Enter` if prompted to confirm.

### 3. Verify environment variables

Make sure `DEVKITPRO` and `DEVKITARM` are set correctly (the installer usually handles this automatically):

- `DEVKITPRO` = `C:\devkitPro`
- `DEVKITARM` = `C:\devkitPro\devkitARM`

---

## Building

Open a **new** PowerShell window in the project root and run:

```powershell
make clean
make
```

> If `make` is not recognized, close and reopen PowerShell — the devkitPro PATH only applies to terminals opened after installation.

Additional targets:

```powershell
make cia       # Build a CIA installable file
make clean     # Clean build artifacts and romfs
```
