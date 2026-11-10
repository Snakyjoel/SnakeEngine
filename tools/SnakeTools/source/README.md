# Snake Tools

Dedicated desktop asset converter, inverse extractor, and file viewer for the Nintendo 3DS **Snake Engine**.

---

## Features

### 1. 3DS Asset Viewer ("Open With...")
- Double-click or open any `.t3x`, `.rawtex`, `.adp`, or `.snaky` file from Windows Explorer.
- **Images (`.t3x` / `.rawtex`)**: Displays full image preview and real resolution, format details, and one-click export to PNG.
- **Audio (`.adp`)**: Displays sample rate, duration, channels, with built-in player and direct export to WAV/OGG.
- **Videos (`.snaky`)**: Displays frame 0 color thumbnail, resolution, FPS, total frame count, and one-click export to MP4.

### 2. Windows Explorer Thumbnails
- Registers custom Windows shell thumbnail providers so that `.rawtex`, `.t3x`, and `.snaky` files show their real visual previews directly inside Windows Explorer folders.
- Click **"Install Thumbnails"** or **"Uninstall Extension"** directly inside Snake Tools to register or remove live thumbnail previews and file associations in one click. Alternatively, run `install_shell_extension.ps1` (or `install_shell_extension.ps1 -Uninstall`).

### 3. Image Converter (`.t3x`)
- Convert PNG/JPG/BMP to Citro3D GPU `.t3x` textures.
- Formats: `RGBA8888`, `RGB565`, `RGBA4444`.
- Compression: `Best` (LZ11), `Fast`, `None`.
- **Dual Real-time Preview**: Inspect side-by-side original vs. converted texture with hardware color dithering and file size in KB.

### 4. Audio Converter (`.adp`)
- Convert WAV, OGG, MP3, FLAC to `.adp` (IMA-ADPCM SADP header format).
- Sample rates: 44.1 kHz, 32 kHz (recommended for 3DS), 22.05 kHz, 16 kHz, 11.025 kHz.
- **Live A/B Audio Comparison**: Play the original source audio vs. the converted ADPCM sound in real-time before saving.

### 5. Video Converter (`.snaky`)
- Encode MP4/MKV/AVI to `.snaky`.
- Select framerate (24 FPS, 30 FPS, original) and screen aspect cropping (400x240 Top Screen).
- Configurable audio sample rate and codec (IMA-ADPCM 4:1 compression or PCM 16-bit).

### 6. Inverse Conversions
- `.snaky` -> `.mp4`
- `.adp` -> `.ogg` / `.wav`
- `.t3x` / `.rawtex` -> `.png`

---

## Building the Executable

```batch
build_exe.bat
```

Compiles a zero-dependency standalone `SnakeTools.exe` bundling `ffmpeg` and `tex3ds`.
