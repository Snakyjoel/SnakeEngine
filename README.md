<p align="center"><img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/Logo.png" alt="Snake Engine Logo" width="900"></p>

<h1 align="center">Friday Night Funkin': Snake Engine</h1>

<h2 align="center">An engine for the Nintendo 3DS</h2>

---

# IMPORTANT

This engine uses some assets taken from other engines, such as [Psych Engine](https://github.com/ShadowMario/FNF-PsychEngine) and [Codename Engine](https://github.com/CodenameCrew/CodenameEngine); credit goes to them for the assets used.

---

## What the hell is this

Snake Engine is a full *Friday Night Funkin'* demake built from scratch in C++ (using tools such as devkitARM, libctru, citro2d, and several others) for the entire Nintendo 3DS family of systems—meaning, yes, it also runs on the original (OLD) 3DS.

Furthermore, you will be able to modify this engine in a way very similar to Psych Engine, as the Snake Engine API is quite similar to that of Psych Engine—though there are some differences, whether due to how the menus are structured or certain limitations.

> [!NOTE]
> There is more information further down.

## (Some photos of the engine)

<img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/_08.11.26_11.41.48.986.png" alt="Tttile" width="900">

<img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/_08.11.26_11.42.08.183.png" alt="Menu" width="900">

<img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/_08.11.26_11.42.17.437.png" alt="Story" width="900">

<img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/_08.11.26_11.42.26.583.png" alt="Freeplay" width="900">

<img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/_08.11.26_11.42.42.023.png" alt="Mods" width="900">

<img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/_08.11.26_11.43.06.647.png" alt="Options" width="900">

<img src="https://raw.githubusercontent.com/Snakyjoel/SnakeEngine/refs/heads/main/art/_08.11.26_11.43.40.784.png" alt="Game" width="900">

## HOW TO INSTALL

First, you need to download the CIA of your choice; there are two to pick from:

> [!NOTE]
> Don't worry; the only difference is in the game assets—the way they function is the same in both.
> 
> And also... (Version 3.7.9 will be used as a reference)

|  | Default | Lite |
| --- | --- | --- |
| Base game videos | ✅ | ❌ |
| Size | 800 Mb approximate | 600 Mb approximate |

Once you have downloaded the CIA file, you must copy the file to a folder on your console's SD card (the root directory or a folder dedicated to your CIA files is recommended). Use also a tool that allows you to install it on your console, such as [FBI](https://github.com/steveice10/FBI), [3dsDL](https://ankush.one/3dsDL/), [GodMode9](https://github.com/d0k3/GodMode9) or via a  [Custom Install](https://github.com/ihaveamac/custom-install)

**FBI:** Use the first option (SD) and navigate to the folder containing your CIA file. Once there, move your selection until it highlights the CIA file, then select it. After that, choose "Install CIA" or "Install and delete CIA." This may take some time, depending on your SD card, but once it’s finished, it will be installed on your console.

**3dsDL:** Select the "Files" option and navigate to the folder where your CIA file is located. Once you find it, select it and choose either "Install" or "Install and Delete File." After the process finishes, it will be installed on your console.

**GM9 (GodMode9):** To install it using GM9, start with the console powered off. To enter GM9, hold down the Start button while turning on the console (keep holding Start). Once it powers on, it will boot into GM9.

In the GM9 menu, select the first option ([0:] SDCARD) and navigate to the folder containing your CIA file. Select the file, choose "CIA image options...", and then select "Install game image". Once the installation is complete, simply turn off or restart the console, and the game will be installed.

**CI (Custom Install):** If the first three options seem too slow, you can use CI, which reduces the wait time to just a few seconds. There are two methods for installing a CIA file using this tool, both of which are well-documented on its [GitHub page.](https://github.com/ihaveamac/custom-install)

## HOW TO INSTALL MODS

To install mods, you must create a folder at: `*(your sd)*/SnakeEngine/mods`

They cannot be in a compressed format—such as .rar, .zip, or .7z—otherwise, the mod will not be detected.

## HOW TO BUILD

**SETUP (PART 1):** Install devkitPro...

**Windows:** 
1. Download the official **devkitPro** Windows installer: [devkitPro Installer Releases](https://github.com/devkitPro/installer/releases).

2. Run the installer and select the **3DS Development** workload (this installs `devkitARM`, `libctru`, `citro2d`, `citro3d`, `tex3ds`, etc.).

<br>

**Linux/MacOS:**

Follow the [devkitPro Pacman setup guide](https://devkitpro.org/wiki/devkitPro_pacman) for your distribution, then install the 3DS toolchain:
`sudo dkp-pacman -S 3ds-dev`

<br>
<br>

**SETUP (PART 2): Install required 3DS Portlibs...**

Open your **devkitPro MSYS2** terminal (or system terminal on Linux) and run: `dkp-pacman -S 3ds-dev 3ds-liblua51 3ds-zlib 3ds-libogg 3ds-libvorbisidec 3ds-jansson`

*(Press `Y` to confirm the installation if prompted).*

<br>
<br>

**SETUP (PART 3):** Tools for building the CIA file...

You need to ensure `makerom` and `bannertool` are available in your `PATH` or in `/opt/devkitpro/tools/bin/`. On Windows with devkitPro, these are typically bundled or can be installed via `dkp-pacman -S 3ds-tools`.

<br>
<br>

**SETUP (FINAL PART):** Verify Environment Variables... *(This part is entirely optional, as it is only for verification purposes, but it is still recommended.)*

Ensure the following environment variables are set in your environment:

- `DEVKITPRO` = `/opt/devkitpro` (or `C:\devkitPro` on Windows)
- `DEVKITARM` = `$DEVKITPRO/devkitARM` (or `C:\devkitPro\devkitARM`)

## BUILDING

Open a terminal or PowerShell in the project's root directory and run the command of your choice:

| Command | Result |
| --- | --- |
| make | 3dsx |
| make cia | .cia |
| make cia-lite | .cia (Lite version) |

> [!WARNING]
> This project was created with some help from AI tools (this doesn’t mean it’s "Vibecoded" in the sense of being entirely AI-produced; rather, there were parts where AI provided assistance. Im currently working on rewriting the code for the sections where AI was used).

---

<sub>Built with [README Builder](https://ofershap.github.io/readme-builder/) — visual drag-and-drop editor for GitHub READMEs</sub>
