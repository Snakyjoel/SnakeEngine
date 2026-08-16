# Snake Engine

A Friday Night Funkin' engine tailored for the **Nintendo 3DS**, built natively in C++ with **devkitARM**, **libctru**, and **citro2d**.

---

## Features

- **Full Lua Scripting**: Psych Engine-compatible Lua callbacks, tweens, timers, and custom events.
- **Custom Assets**: Support for custom character sheets, stages, note skins, rating skins, and soundfonts.
- **Mod Support**: Load mods dynamically from the SD card (`sdmc:/SnakeEngine/mods/`).
- **High Performance**: Optimized memory and VRAM management with texture caching, `.rawtex` fallbacks, and hardware-accelerated rendering.
- **Dual-Screen UI**: Bottom screen touch hitboxes, pause menu, customizable combo HUD, and visualizers.
- **Custom Shaders**: Hardware-accelerated shader post-processing pipeline.

---

## Setup — Building from Source

### 1. Install devkitPro

#### Windows
1. Download the official **devkitPro** Windows installer: [devkitPro Installer Releases](https://github.com/devkitPro/installer/releases).
2. Run the installer and select the **3DS Development** workload (this installs `devkitARM`, `libctru`, `citro2d`, `citro3d`, `tex3ds`, etc.).

#### Linux / macOS
Follow the [devkitPro Pacman setup guide](https://devkitpro.org/wiki/devkitPro_pacman) for your distribution, then install the 3DS toolchain:
```bash
sudo dkp-pacman -S 3ds-dev
```

---

### 2. Install Required 3DS Portlibs

Open your **devkitPro MSYS2** terminal (or system terminal on Linux) and run:

```bash
dkp-pacman -S 3ds-dev 3ds-liblua51 3ds-zlib 3ds-libogg 3ds-libvorbisidec 3ds-jansson
```

*(Press `Y` to confirm the installation if prompted).*

---

### 3. (Optional) Tools for Building `.cia` Files

If you want to build installable `.cia` packages:
- Ensure `makerom` and `bannertool` are available in your `PATH` or in `/opt/devkitpro/tools/bin/`.
- On Windows with devkitPro, these are typically bundled or can be installed via `dkp-pacman -S 3ds-tools`.

---

### 4. Verify Environment Variables

Ensure the following environment variables are set in your environment:

- `DEVKITPRO` = `/opt/devkitpro` (or `C:\devkitPro` on Windows)
- `DEVKITARM` = `$DEVKITPRO/devkitARM` (or `C:\devkitPro\devkitARM`)

---

## Building

Open a terminal or PowerShell in the root directory of the project:

### Standard Build (Outputs `.3dsx`)
```bash
make -j4
```

### Build Installable `.cia` Package
```bash
make cia
```

### Lite Build (Optimized for Old 3DS RAM limits)
```bash
make LITE=1 -j4
```

### Clean Build
```bash
make clean
```

---

## Running on 3DS / Emulator

- **Citra / Azahar / Lime3DS**: Open `SnakeEngine.3dsx` directly from the project root.
- **Real 3DS Hardware**: 
  - Copy `SnakeEngine.3dsx` and `SnakeEngine.smdh` to `sdmc:/3ds/SnakeEngine/`.
  - Launch using **Homebrew Launcher** (or install the generated `.cia` via **FBI**).

---

## Credits & Licenses

- **Snake Engine** by **Snakyjoel** and contributors.
- **Friday Night Funkin'** by The Funkin' Crew.
- Powered by [devkitPro](https://devkitpro.org/), [libctru](https://github.com/devkitPro/libctru), and [citro2d](https://github.com/devkitPro/citro2d).
- Distributed under the terms of the project licenses (see `SNAKE ENGINE LICENSE.md` and `FRIDAY NIGHT FUNKIN LICENSE.md`).

