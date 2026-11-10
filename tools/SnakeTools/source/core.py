import os
import sys
import struct
import zlib
import shutil
import tempfile
import subprocess
import threading
import numpy as np
from PIL import Image

def get_resource_path(rel_path):
    if hasattr(sys, '_MEIPASS'):
        return os.path.join(sys._MEIPASS, rel_path)
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), rel_path)

def get_bin(name):
    p1 = get_resource_path(os.path.join("bin", name))
    if os.path.exists(p1):
        return p1
    p2 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bin", name)
    if os.path.exists(p2):
        return p2
    w = shutil.which(name)
    if w:
        return w
    if "tex3ds" in name and os.path.exists(r"C:\devkitPro\tools\bin\tex3ds.exe"):
        return r"C:\devkitPro\tools\bin\tex3ds.exe"
    return None

FFMPEG = get_bin("ffmpeg.exe")
TEX3DS = get_bin("tex3ds.exe")

def morton_unswizzle(x, y):
    i = (x & 7) | ((y & 7) << 8)
    i = (i ^ (i << 2)) & 0x1313
    i = (i ^ (i << 1)) & 0x1515
    return (i & 0xFF) | (((i >> 8) & 0xFF) << 1)

def decompress_lz11(data):
    if not data or data[0] != 0x11:
        return data
    decomp_size = data[1] | (data[2] << 8) | (data[3] << 16)
    out = bytearray()
    idx = 4
    if decomp_size == 0:
        if len(data) < 8: return data
        decomp_size = struct.unpack('<I', data[4:8])[0]
        idx = 8

    while len(out) < decomp_size and idx < len(data):
        flags = data[idx]
        idx += 1
        for b in range(8):
            if len(out) >= decomp_size or idx >= len(data): break
            if (flags & (0x80 >> b)) == 0:
                out.append(data[idx])
                idx += 1
            else:
                if idx + 1 >= len(data): break
                b0 = data[idx]
                b1 = data[idx+1]
                idx += 2
                indicator = b0 >> 4
                if indicator == 0:
                    if idx >= len(data): break
                    b2 = data[idx]
                    idx += 1
                    length = (((b0 & 0x0F) << 4) | (b1 >> 4)) + 0x11
                    disp = (((b1 & 0x0F) << 8) | b2) + 1
                elif indicator == 1:
                    if idx + 1 >= len(data): break
                    b2 = data[idx]
                    b3 = data[idx+1]
                    idx += 2
                    length = (((b0 & 0x0F) << 12) | (b1 << 4) | (b2 >> 4)) + 0x111
                    disp = (((b2 & 0x0F) << 8) | b3) + 1
                else:
                    length = indicator + 1
                    disp = (((b0 & 0x0F) << 8) | b1) + 1

                for _ in range(length):
                    if disp <= len(out):
                        out.append(out[-disp])
                    else:
                        out.append(0)
    return bytes(out)

def decompress_rle(data):
    if not data or data[0] != 0x30:
        return data
    sz = data[1] | (data[2] << 8) | (data[3] << 16)
    idx = 4
    if sz == 0:
        if len(data) < 8: return data
        sz = struct.unpack('<I', data[4:8])[0]
        idx = 8
    out = bytearray()
    while len(out) < sz and idx < len(data):
        flag = data[idx]
        idx += 1
        if (flag & 0x80) == 0:
            cnt = (flag & 0x7F) + 1
            out.extend(data[idx:idx+cnt])
            idx += cnt
        else:
            cnt = (flag & 0x7F) + 3
            if idx >= len(data): break
            val = data[idx]
            idx += 1
            out.extend([val] * cnt)
    return bytes(out[:sz])

def decompress_huff(data):
    if not data or (data[0] not in (0x28, 0x24)):
        return data
    sz = data[1] | (data[2] << 8) | (data[3] << 16)
    idx = 4
    if sz == 0:
        if len(data) < 8: return data
        sz = struct.unpack('<I', data[4:8])[0]
        idx = 8
    if idx >= len(data): return data
    tree_size = data[idx]
    tree_len = 1 + (tree_size * 2 + 1)
    if idx + tree_len > len(data): return data
    tree = data[idx : idx + tree_len]
    idx += tree_len

    out = bytearray()
    r4 = 1
    while len(out) < sz and idx < len(data):
        word = struct.unpack('<I', data[idx:idx+4])[0] if idx + 4 <= len(data) else 0
        idx += 4
        mask = 0x80000000
        while mask > 0 and len(out) < sz:
            bit = (word & mask) != 0
            mask >>= 1
            if r4 >= len(tree):
                r4 = 1
                break
            node = tree[r4]
            offset = node & 0x3F
            child0_is_leaf = (node & 0x80) != 0
            child1_is_leaf = (node & 0x40) != 0
            base = (r4 & ~1) + (offset << 1)

            if not bit:
                r4 = base + 2
                if child0_is_leaf:
                    if r4 < len(tree):
                        out.append(tree[r4])
                    r4 = 1
            else:
                r4 = base + 3
                if child1_is_leaf:
                    if r4 < len(tree):
                        out.append(tree[r4])
                    r4 = 1
    return bytes(out[:sz])

# --- IMA-ADPCM Tables ---
IMA_INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8]
IMA_STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]

def encode_adp(pcm_samples, sample_rate):
    if pcm_samples.ndim == 1:
        channels = 1
        num_samples = len(pcm_samples)
        channels_data = [pcm_samples]
    else:
        channels = pcm_samples.shape[1]
        num_samples = pcm_samples.shape[0]
        channels_data = [pcm_samples[:, ch] for ch in range(channels)]

    encoded_channels = []
    for ch in range(channels):
        samples = channels_data[ch]
        pred = 0
        step_idx = 0
        nibbles = []
        for sample in samples:
            diff = int(sample) - pred
            sign = 0
            if diff < 0:
                sign = 8
                diff = -diff

            step = IMA_STEP_TABLE[step_idx]
            delta = 0
            vpdiff = step >> 3

            if diff >= step:
                delta |= 4
                diff -= step
                vpdiff += step
            step >>= 1
            if diff >= step:
                delta |= 2
                diff -= step
                vpdiff += step
            step >>= 1
            if diff >= step:
                delta |= 1
                vpdiff += step

            if sign:
                pred -= vpdiff
            else:
                pred += vpdiff
            pred = max(-32768, min(32767, pred))

            step_idx += IMA_INDEX_TABLE[delta]
            step_idx = max(0, min(88, step_idx))
            nibbles.append(sign | delta)

        if len(nibbles) % 2 != 0:
            nibbles.append(0)
        ba = bytearray(len(nibbles) // 2)
        for i in range(0, len(nibbles), 2):
            ba[i // 2] = (nibbles[i] & 0x0F) | ((nibbles[i + 1] & 0x0F) << 4)
        encoded_channels.append(ba)

    if channels == 1:
        raw_adp = bytes(encoded_channels[0])
    else:
        raw_adp = bytearray(len(encoded_channels[0]) * channels)
        ptr = 0
        for i in range(len(encoded_channels[0])):
            for ch in range(channels):
                raw_adp[ptr] = encoded_channels[ch][i]
                ptr += 1
        raw_adp = bytes(raw_adp)

    header = struct.pack('<4sIIB7s', b'SADP', int(sample_rate), int(num_samples), int(channels), b'\x00' * 7)
    return header + raw_adp

def decode_adp(adp_bytes):
    if len(adp_bytes) < 20 or adp_bytes[:4] != b'SADP':
        raise ValueError("Invalid SADP header.")

    magic, sample_rate, num_samples, channels, _ = struct.unpack('<4sIIB7s', adp_bytes[:20])
    raw_data = adp_bytes[20:]

    if channels not in (1, 2):
        channels = 1

    nibbles_per_channel = [[] for _ in range(channels)]
    idx = 0
    while idx < len(raw_data):
        for ch in range(channels):
            if idx < len(raw_data):
                b = raw_data[idx]
                idx += 1
                nibbles_per_channel[ch].append(b & 0x0F)
                nibbles_per_channel[ch].append((b >> 4) & 0x0F)

    decoded_channels = []
    for ch in range(channels):
        nibs = nibbles_per_channel[ch][:num_samples]
        pred = 0
        step_idx = 0
        pcm = np.zeros(len(nibs), dtype=np.int16)
        for i, delta in enumerate(nibs):
            step = IMA_STEP_TABLE[step_idx]
            step_idx += IMA_INDEX_TABLE[delta & 7]
            step_idx = max(0, min(88, step_idx))

            vpdiff = step >> 3
            if delta & 4: vpdiff += step
            if delta & 2: vpdiff += step >> 1
            if delta & 1: vpdiff += step >> 2

            if delta & 8:
                pred -= vpdiff
            else:
                pred += vpdiff
            pred = max(-32768, min(32767, pred))
            pcm[i] = pred
        decoded_channels.append(pcm)

    if channels == 1:
        pcm_out = decoded_channels[0]
    else:
        pcm_out = np.column_stack(decoded_channels)

    return sample_rate, channels, pcm_out

def get_adp_info(adp_path):
    with open(adp_path, 'rb') as f:
        data = f.read(20)
    if len(data) < 20 or data[:4] != b'SADP':
        raise ValueError("Not a valid Snake Engine .adp file.")
    magic, sample_rate, num_samples, channels, _ = struct.unpack('<4sIIB7s', data)
    duration = num_samples / sample_rate if sample_rate > 0 else 0
    return {
        "sample_rate": sample_rate,
        "channels": channels,
        "samples": num_samples,
        "duration": duration,
        "size": os.path.getsize(adp_path)
    }

def convert_audio_to_adp(in_path, out_path, sample_rate=32000, channels=2, progress_cb=None):
    if not FFMPEG:
        raise RuntimeError("ffmpeg binary not found.")
    if progress_cb: progress_cb(20, "Decoding audio...")

    cmd = [
        FFMPEG, '-y', '-i', in_path,
        '-ar', str(sample_rate),
        '-ac', str(channels),
        '-f', 's16le', '-'
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    raw_pcm, err = proc.communicate()
    if proc.returncode != 0:
        raise RuntimeError(f"Audio decode error: {err.decode('utf-8', errors='ignore')}")

    if progress_cb: progress_cb(60, "Encoding to IMA-ADPCM...")
    samples = np.frombuffer(raw_pcm, dtype=np.int16)
    if channels > 1:
        valid_samples = (len(samples) // channels) * channels
        samples = samples[:valid_samples].reshape(-1, channels)

    adp_data = encode_adp(samples, sample_rate)
    with open(out_path, 'wb') as f:
        f.write(adp_data)

    if progress_cb: progress_cb(100, "Done.")

def convert_adp_to_audio(in_path, out_path, format_type='ogg', progress_cb=None):
    if not FFMPEG:
        raise RuntimeError("ffmpeg binary not found.")
    if progress_cb: progress_cb(20, "Reading .adp file...")

    with open(in_path, 'rb') as f:
        adp_bytes = f.read()

    sample_rate, channels, pcm = decode_adp(adp_bytes)

    if progress_cb: progress_cb(60, f"Exporting {format_type.upper()}...")
    cmd = [
        FFMPEG, '-y',
        '-f', 's16le',
        '-ar', str(sample_rate),
        '-ac', str(channels),
        '-i', '-',
        out_path
    ]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc.communicate(input=pcm.tobytes())
    if proc.returncode != 0:
        raise RuntimeError("Export error.")

    if progress_cb: progress_cb(100, "Done.")

def render_audio_preview_wav(in_path, sample_rate=32000, channels=2, is_adp=False):
    tmp_wav = tempfile.mktemp(suffix=".wav")
    import wave
    if is_adp:
        with open(in_path, 'rb') as f:
            data = f.read()
        sr, ch, pcm = decode_adp(data)
        with wave.open(tmp_wav, 'wb') as wf:
            wf.setnchannels(ch)
            wf.setsampwidth(2)
            wf.setframerate(sr)
            wf.writeframes(pcm.tobytes())
    else:
        cmd = [
            FFMPEG, '-y', '-i', in_path,
            '-ar', str(sample_rate),
            '-ac', str(channels),
            '-f', 's16le', '-'
        ]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        raw_pcm, _ = proc.communicate()
        samples = np.frombuffer(raw_pcm, dtype=np.int16)
        if channels > 1:
            valid_samples = (len(samples) // channels) * channels
            samples = samples[:valid_samples].reshape(-1, channels)
        adp_bytes = encode_adp(samples, sample_rate)
        sr, ch, pcm_dec = decode_adp(adp_bytes)

        with wave.open(tmp_wav, 'wb') as wf:
            wf.setnchannels(ch)
            wf.setsampwidth(2)
            wf.setframerate(sr)
            wf.writeframes(pcm_dec.tobytes())
    return tmp_wav

def convert_original_to_temp_wav(in_path):
    tmp_wav = tempfile.mktemp(suffix=".wav")
    cmd = [FFMPEG, '-y', '-i', in_path, '-f', 'wav', tmp_wav]
    subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    return tmp_wav

# --- Images (.t3x and .rawtex) ---
def decode_t3x_image(file_path):
    with open(file_path, 'rb') as f:
        data = f.read()

    if len(data) < 5:
        raise ValueError("File is too small to be a .t3x texture.")

    num_sub = struct.unpack('<H', data[0:2])[0]
    log_dims = data[2]
    fmt = data[3] & 0x0F
    
    # 3DS GPU power-of-two texture dimensions (verified from libcitro3d Tex3DSi_ImportCommon)
    pw = 1 << (3 + (log_dims & 0x07))
    ph = 1 << (3 + ((log_dims >> 3) & 0x07))

    w, h = pw, ph
    is_rotated = False
    if num_sub > 0 and len(data) >= 17:
        w, h, left, top, right, bottom = struct.unpack('<HHHHHH', data[5:17])
        # In tex3ds UV mapping, top < bottom signifies 1/4 turn counter-clockwise rotation in atlas
        is_rotated = (top < bottom)

    # Decompress pixel payload (starts at 5 + num_sub * 12)
    sub_off = 5 + num_sub * 12
    raw_slice = data[sub_off:]
    if len(raw_slice) > 0:
        magic_b = raw_slice[0]
        if magic_b == 0x11:
            pixel_data = decompress_lz11(raw_slice)
        elif magic_b in (0x28, 0x24):
            pixel_data = decompress_huff(raw_slice)
        elif magic_b == 0x30:
            pixel_data = decompress_rle(raw_slice)
        elif magic_b == 0x00 and len(raw_slice) > 4:
            pixel_data = raw_slice[4:]
        else:
            pixel_data = raw_slice
    else:
        pixel_data = raw_slice

    # Decode full atlas into an image of size (pw, ph)
    # Formats: 0: RGBA8 (4B), 1: RGB8 (3B), 2: RGBA5551 (2B), 3: RGB565 (2B), 4: RGBA4 (2B)
    # 5: LA8 (2B), 6: HILO8 (2B), 7: L8 (1B), 8: A8 (1B), 9: LA4 (1B), 10: L4 (0.5B), 11: A4 (0.5B)
    atlas = Image.new('RGBA', (pw, ph))
    at_pixels = atlas.load()

    for y in range(ph):
        for x in range(pw):
            local_idx = morton_unswizzle(x, y)
            tx = x >> 3
            ty = y >> 3
            tile_start = (ty * (pw >> 3) + tx) << 6

            if fmt == 0: # RGBA8 (4 bytes, ABGR order)
                src_idx = (tile_start + local_idx) * 4
                if src_idx + 4 <= len(pixel_data):
                    a, b, g, r = pixel_data[src_idx:src_idx+4]
                    at_pixels[x, y] = (r, g, b, a)
            elif fmt == 1: # RGB8 (3 bytes, BGR order)
                src_idx = (tile_start + local_idx) * 3
                if src_idx + 3 <= len(pixel_data):
                    b, g, r = pixel_data[src_idx:src_idx+3]
                    at_pixels[x, y] = (r, g, b, 255)
            elif fmt == 2: # RGBA5551 (2 bytes)
                src_idx = (tile_start + local_idx) * 2
                if src_idx + 2 <= len(pixel_data):
                    val = struct.unpack('<H', pixel_data[src_idx:src_idx+2])[0]
                    r = ((val >> 11) & 0x1F) * 255 // 31
                    g = ((val >> 6) & 0x1F) * 255 // 31
                    b = ((val >> 1) & 0x1F) * 255 // 31
                    a = 255 if (val & 0x01) else 0
                    at_pixels[x, y] = (r, g, b, a)
            elif fmt == 3: # RGB565 (2 bytes)
                src_idx = (tile_start + local_idx) * 2
                if src_idx + 2 <= len(pixel_data):
                    val = struct.unpack('<H', pixel_data[src_idx:src_idx+2])[0]
                    r = ((val >> 11) & 0x1F) * 255 // 31
                    g = ((val >> 5) & 0x3F) * 255 // 63
                    b = (val & 0x1F) * 255 // 31
                    at_pixels[x, y] = (r, g, b, 255)
            elif fmt == 4: # RGBA4 (2 bytes)
                src_idx = (tile_start + local_idx) * 2
                if src_idx + 2 <= len(pixel_data):
                    b0, b1 = pixel_data[src_idx], pixel_data[src_idx+1]
                    a = (b0 & 0x0F) * 17
                    b = ((b0 >> 4) & 0x0F) * 17
                    g = (b1 & 0x0F) * 17
                    r = ((b1 >> 4) & 0x0F) * 17
                    at_pixels[x, y] = (r, g, b, a)
            elif fmt == 5: # LA8 (2 bytes: Lum8, Alpha8)
                src_idx = (tile_start + local_idx) * 2
                if src_idx + 2 <= len(pixel_data):
                    a = pixel_data[src_idx]
                    l = pixel_data[src_idx+1]
                    at_pixels[x, y] = (l, l, l, a)
            elif fmt == 6: # HILO8 (2 bytes)
                src_idx = (tile_start + local_idx) * 2
                if src_idx + 2 <= len(pixel_data):
                    hi, lo = pixel_data[src_idx], pixel_data[src_idx+1]
                    at_pixels[x, y] = (hi, lo, 0, 255)
            elif fmt == 7: # L8 (1 byte: Lum8)
                src_idx = tile_start + local_idx
                if src_idx < len(pixel_data):
                    l = pixel_data[src_idx]
                    at_pixels[x, y] = (l, l, l, 255)
            elif fmt == 8: # A8 (1 byte: Alpha8)
                src_idx = tile_start + local_idx
                if src_idx < len(pixel_data):
                    a = pixel_data[src_idx]
                    at_pixels[x, y] = (255, 255, 255, a)
            elif fmt == 9: # LA4 (1 byte: Lum4, Alpha4)
                src_idx = tile_start + local_idx
                if src_idx < len(pixel_data):
                    b0 = pixel_data[src_idx]
                    l = (b0 >> 4) * 17
                    a = (b0 & 0x0F) * 17
                    at_pixels[x, y] = (l, l, l, a)
            elif fmt == 10: # L4 (4-bit Lum)
                elem = tile_start + local_idx
                byte_idx = elem >> 1
                if byte_idx < len(pixel_data):
                    b0 = pixel_data[byte_idx]
                    val = (b0 & 0x0F) if (elem & 1) == 0 else (b0 >> 4)
                    l = val * 17
                    at_pixels[x, y] = (l, l, l, 255)
            elif fmt == 11: # A4 (4-bit Alpha)
                elem = tile_start + local_idx
                byte_idx = elem >> 1
                if byte_idx < len(pixel_data):
                    b0 = pixel_data[byte_idx]
                    val = (b0 & 0x0F) if (elem & 1) == 0 else (b0 >> 4)
                    a = val * 17
                    at_pixels[x, y] = (255, 255, 255, a)
            else:
                at_pixels[x, y] = (255, 0, 255, 255)

    if is_rotated:
        # Rotated 90 deg CCW in atlas with dimensions (h, w)
        sub_img = atlas.crop((0, 0, min(h, pw), min(w, ph)))
        img = sub_img.transpose(Image.ROTATE_270)
    else:
        img = atlas.crop((0, 0, min(w, pw), min(h, ph)))

    fmt_names = {0: "RGBA8", 1: "RGB8", 2: "RGBA5551", 3: "RGB565", 4: "RGBA4", 5: "LA8", 6: "HILO8", 7: "L8", 8: "A8", 9: "LA4", 10: "L4", 11: "A4"}
    fmt_str = fmt_names.get(fmt, f"Format {fmt}")
    return img, {"width": w, "height": h, "padded_w": pw, "padded_h": ph, "format": f"t3x ({fmt_str})", "format_id": fmt}

def decode_rawtex_image(file_path):
    with open(file_path, 'rb') as f:
        hdr = f.read(12)
        if len(hdr) < 12:
            raise ValueError("File is too small to be a .rawtex texture.")
        magic, pw, ph, w, h = struct.unpack('<4sHHHH', hdr)
        raw_data = f.read()

    magic_str = magic.decode('latin1', errors='ignore')
    if not magic_str.startswith('RWT'):
        raise ValueError("Invalid .rawtex magic header.")

    magic_byte = magic_str[3] if len(magic_str) > 3 else 'X'
    bpp = 4 if magic_byte == 'X' else 2

    img = Image.new('RGBA', (w, h))
    pixels = img.load()

    for y in range(h):
        for x in range(w):
            local_idx = morton_unswizzle(x, y)
            tx = x >> 3
            ty = y >> 3
            tile_start = (ty * (pw >> 3) + tx) << 6
            src_idx = (tile_start + local_idx) * bpp

            if src_idx + bpp <= len(raw_data):
                if magic_byte == 'X': # RGBA8
                    a = raw_data[src_idx]
                    b = raw_data[src_idx + 1]
                    g = raw_data[src_idx + 2]
                    r = raw_data[src_idx + 3]
                    pixels[x, y] = (r, g, b, a)
                elif magic_byte == '4': # RGBA4
                    b0 = raw_data[src_idx]
                    b1 = raw_data[src_idx + 1]
                    a = (b0 & 0x0F) * 17
                    b = ((b0 >> 4) & 0x0F) * 17
                    g = (b1 & 0x0F) * 17
                    r = ((b1 >> 4) & 0x0F) * 17
                    pixels[x, y] = (r, g, b, a)
                elif magic_byte == '5': # RGB565
                    val = struct.unpack('<H', raw_data[src_idx:src_idx+2])[0]
                    r = ((val >> 11) & 0x1F) * 255 // 31
                    g = ((val >> 5) & 0x3F) * 255 // 63
                    b = (val & 0x1F) * 255 // 31
                    pixels[x, y] = (r, g, b, 255)

    return img, {"width": w, "height": h, "padded_w": pw, "padded_h": ph, "type": magic_str}

def convert_image_to_t3x(in_path, out_path, format_str="rgba8888", compression="none"):
    if not TEX3DS or not os.path.exists(TEX3DS):
        raise RuntimeError("tex3ds tool not found.")

    format_arg = f"-f{format_str.lower()}"
    in_abs = os.path.abspath(in_path)
    out_abs = os.path.abspath(out_path)
    cmd = [TEX3DS, in_abs, format_arg, "-z", compression, "-o", out_abs]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if res.returncode != 0:
        err = res.stderr.decode('utf-8', errors='ignore')
        raise RuntimeError(f"tex3ds failed: {err}")

def get_t3x_preview(in_path, format_str="rgba8888", compression="none"):
    tmp_t3x = tempfile.mktemp(suffix=".t3x")
    try:
        convert_image_to_t3x(in_path, tmp_t3x, format_str, compression)
        size_bytes = os.path.getsize(tmp_t3x)
        img, _ = decode_t3x_image(tmp_t3x)
        return img, size_bytes
    finally:
        if os.path.exists(tmp_t3x):
            try: os.remove(tmp_t3x)
            except Exception: pass

def read_3ds_image_as_pil(path):
    ext = os.path.splitext(path)[1].lower()
    if ext == '.t3x':
        img, details = decode_t3x_image(path)
        info = {
            "format": f"t3x GPU (Format {details.get('format_id', 0)})",
            "width": details["width"],
            "height": details["height"],
            "size": os.path.getsize(path)
        }
        return img, info
    elif ext == '.rawtex':
        img, details = decode_rawtex_image(path)
        info = {
            "format": f"rawtex ({details.get('type', 'RWTX')})",
            "width": details["width"],
            "height": details["height"],
            "size": os.path.getsize(path)
        }
        return img, info
    else:
        img = Image.open(path)
        info = {
            "format": ext.upper().replace(".", ""),
            "width": img.width,
            "height": img.height,
            "size": os.path.getsize(path)
        }
        return img, info

# --- Videos (.snaky) ---
def rgb_to_rgb444_packed(frame):
    # frame is (height, width, 3) uint8
    h, w, _ = frame.shape
    total_pixels = h * w
    flat = frame.reshape(-1, 3)

    r = (flat[:, 0] >> 4).astype(np.uint8)
    g = (flat[:, 1] >> 4).astype(np.uint8)
    b = (flat[:, 2] >> 4).astype(np.uint8)

    # Pack 2 pixels into 3 bytes: (R0<<4 | G0), (B0<<4 | R1), (G1<<4 | B1)
    n_pairs = total_pixels // 2
    b0 = (r[0:n_pairs*2:2] << 4) | g[0:n_pairs*2:2]
    b1 = (b[0:n_pairs*2:2] << 4) | r[1:n_pairs*2:2]
    b2 = (g[1:n_pairs*2:2] << 4) | b[1:n_pairs*2:2]

    packed = np.empty((n_pairs, 3), dtype=np.uint8)
    packed[:, 0] = b0
    packed[:, 1] = b1
    packed[:, 2] = b2
    out = bytearray(packed.tobytes())

    if total_pixels % 2 != 0:
        last = total_pixels - 1
        out.append((r[last] << 4) | g[last])
        out.append(b[last] << 4)

    return out

def encode_rle_delta_rgb444(prev_flat, curr_flat, w, h):
    # prev_flat and curr_flat are (N, 3) RGB values
    n = len(curr_flat)
    diff = np.any(prev_flat != curr_flat, axis=1)

    i = 0
    while i < n:
        if not diff[i]:
            start = i
            while i < n and not diff[i]:
                i += 1
            length = i - start
            if length < 16 and start > 0 and i < n:
                diff[start:i] = True
        else:
            i += 1

    opcodes = bytearray()
    i = 0
    while i < n:
        if not diff[i]:
            start = i
            while i < n and not diff[i] and (i - start) < 65535:
                i += 1
            skip_count = i - start
            opcodes.append(0x01)
            opcodes.extend(struct.pack('<H', skip_count))
        else:
            start = i
            while i < n and diff[i] and (i - start) < 65535:
                i += 1
            copy_count = i - start
            opcodes.append(0x02)
            opcodes.extend(struct.pack('<H', copy_count))
            slice_pixels = curr_flat[start:i]
            packed_slice = rgb_to_rgb444_packed(slice_pixels.reshape(-1, 1, 3))
            opcodes.extend(packed_slice)
    opcodes.append(0x00)
    return opcodes

def encode_snaky(in_path, out_path, fps=24, target_w=400, target_h=240, mode="fit", audio_rate=32000, audio_codec="ADPCM", target_channels=2, include_audio=True, progress_cb=None):
    from moviepy import VideoFileClip, vfx

    if progress_cb: progress_cb(5, "Loading video...")
    clip = VideoFileClip(in_path)
    clip = clip.with_fps(fps)

    orig_w, orig_h = clip.size
    target_ratio = target_w / target_h
    current_ratio = orig_w / orig_h

    if mode.lower() == "crop":
        # Crop: fill entire screen without black bars
        if current_ratio > target_ratio:
            new_w = orig_h * target_ratio
            clip = clip.with_effects([vfx.Crop(x_center=orig_w/2, y_center=orig_h/2, width=new_w, height=orig_h)])
        else:
            new_h = orig_w / target_ratio
            clip = clip.with_effects([vfx.Crop(x_center=orig_w/2, y_center=orig_h/2, width=orig_w, height=new_h)])
        clip = clip.resized((target_w, target_h))
    else:
        # Fit: keep aspect ratio, pad with black bars if needed
        if current_ratio > target_ratio:
            scaled_w = target_w
            scaled_h = int(target_w / current_ratio)
        else:
            scaled_h = target_h
            scaled_w = int(target_h * current_ratio)
        scaled_w = max(2, (scaled_w // 2) * 2)
        scaled_h = max(2, (scaled_h // 2) * 2)
        clip = clip.resized((scaled_w, scaled_h))

    width, height = clip.size

    has_audio = False
    channels = 2
    audio_data = b""
    codec_id = 2 if audio_codec.upper() == "ADPCM" else 1

    if include_audio and clip.audio is not None:
        if progress_cb: progress_cb(15, "Processing audio track...")
        temp_wav = tempfile.mktemp(suffix=".wav")
        clip.audio.write_audiofile(temp_wav, fps=audio_rate, nbytes=2, codec='pcm_s16le', logger=None)

        if target_channels in (1, 2) and FFMPEG:
            conv_wav = tempfile.mktemp(suffix=".wav")
            subprocess.run([FFMPEG, '-y', '-i', temp_wav, '-ar', str(audio_rate), '-ac', str(target_channels), conv_wav],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if os.path.exists(temp_wav):
                try: os.remove(temp_wav)
                except Exception: pass
            temp_wav = conv_wav

        import wave
        with wave.open(temp_wav, "rb") as wf:
            channels = wf.getnchannels()
            raw_pcm = wf.readframes(wf.getnframes())
        if os.path.exists(temp_wav):
            try: os.remove(temp_wav)
            except Exception: pass

        if codec_id == 2: # IMA-ADPCM (SnakeEngine audioFormat 2)
            samples = np.frombuffer(raw_pcm, dtype=np.int16)
            if channels > 1:
                valid_samples = (len(samples) // channels) * channels
                samples = samples[:valid_samples].reshape(-1, channels)
            full_adp = encode_adp(samples, audio_rate)
            audio_data = full_adp[20:]
        else: # Raw PCM16 (SnakeEngine audioFormat 1)
            audio_data = raw_pcm
        has_audio = True

    frames = list(clip.iter_frames())
    total_frames = len(frames)

    out_file = open(out_path, "wb")
    out_file.write(b"SNKY")
    out_file.write(struct.pack('<H', 0x0200))
    out_file.write(struct.pack('<H', width))
    out_file.write(struct.pack('<H', height))
    out_file.write(struct.pack('<B', int(fps)))
    out_file.write(struct.pack('<B', 1)) # format 1: standard RGB444 packed (SnakeEngine native format)
    out_file.write(struct.pack('<I', total_frames))
    out_file.write(struct.pack('<H', audio_rate))
    out_file.write(struct.pack('<B', channels))
    out_file.write(struct.pack('<B', codec_id if has_audio else 0))
    out_file.write(struct.pack('<I', 0))
    out_file.write(struct.pack('<I', 0))
    out_file.write(struct.pack('<I', 0))

    prev_flat = np.zeros((width * height, 3), dtype=np.uint8)

    total_audio_bytes = len(audio_data)
    audio_offset = 0
    if codec_id == 2:
        align_bytes = 2 if channels == 2 else 1
        bytes_per_second = (audio_rate * channels) // 2
    else:
        align_bytes = 2 * channels
        bytes_per_second = audio_rate * channels * 2

    for i, frame in enumerate(frames):
        if has_audio:
            ideal_end = int(round((i + 1) * bytes_per_second / float(fps)))
            ideal_end = (ideal_end // align_bytes) * align_bytes
            ideal_end = min(ideal_end, total_audio_bytes)
            chunk = audio_data[audio_offset:ideal_end]
            audio_offset = ideal_end
            if len(chunk) > 0:
                out_file.write(struct.pack('<B', 0x03))
                out_file.write(struct.pack('<I', len(chunk))[:3])
                out_file.write(chunk)

        # Quantize to 4-bit per channel
        curr_flat = ((frame.reshape(-1, 3) >> 4) << 4).astype(np.uint8)

        if i == 0 or i % 30 == 0:
            opcodes = encode_rle_delta_rgb444(np.zeros((width * height, 3), dtype=np.uint8), curr_flat, width, height)
            chunk_type = 0x01
        else:
            opcodes = encode_rle_delta_rgb444(prev_flat, curr_flat, width, height)
            chunk_type = 0x02

        comp = zlib.compress(opcodes, level=9)
        out_file.write(struct.pack('<B', chunk_type))
        out_file.write(struct.pack('<I', len(comp))[:3])
        out_file.write(struct.pack('<I', len(opcodes))[:3])
        out_file.write(comp)

        prev_flat = curr_flat
        if i % 10 == 0 and progress_cb:
            pct = 20 + int((i / total_frames) * 75)
            progress_cb(pct, f"Encoding frame {i}/{total_frames}...")

    out_file.close()
    try: clip.close()
    except Exception: pass
    if progress_cb: progress_cb(100, "Done.")

# --- Real-Time Snaky Video Player Engine ---
class SnakyVideoFile:
    def __init__(self, path):
        self.path = path
        self.f = open(path, 'rb')
        magic = self.f.read(4)
        if magic != b'SNKY':
            self.f.close()
            raise ValueError("Not a valid .snaky file.")
        self.version = struct.unpack('<H', self.f.read(2))[0]
        self.width, self.height = struct.unpack('<HH', self.f.read(4))
        self.fps = struct.unpack('<B', self.f.read(1))[0]
        self.format = struct.unpack('<B', self.f.read(1))[0]
        self.total_frames = struct.unpack('<I', self.f.read(4))[0]
        self.audio_rate = struct.unpack('<H', self.f.read(2))[0]
        self.channels = struct.unpack('<B', self.f.read(1))[0]
        self.audio_format = struct.unpack('<B', self.f.read(1))[0]
        self.has_audio = (self.audio_format in (1, 2))
        self.f.read(12)

        self.frame_pixels = np.zeros((self.height, self.width, 3), dtype=np.uint8)
        self.frame_idx = 0
        self.audio_wav_path = None

    def _extract_audio_to_wav(self):
        if not self.has_audio:
            return
        cur_pos = self.f.tell()
        self.f.seek(32)
        audio_chunks = []
        while True:
            tag_b = self.f.read(1)
            if not tag_b: break
            tag = tag_b[0]
            sz_b = self.f.read(3)
            if len(sz_b) < 3: break
            sz = sz_b[0] | (sz_b[1] << 8) | (sz_b[2] << 16)
            if tag == 0x03:
                audio_chunks.append(self.f.read(sz))
            elif tag in (0x01, 0x02):
                self.f.read(3)
                self.f.seek(sz, 1)
            else:
                self.f.seek(sz, 1)

        self.f.seek(cur_pos)
        if audio_chunks:
            all_aud = b"".join(audio_chunks)
            tmp_wav = tempfile.mktemp(suffix=".wav")
            import wave
            if self.audio_format == 1:
                # Format 1 is raw PCM 16-bit audio (native SnakeEngine format, e.g. 2hotCutscene.snaky)
                try:
                    with wave.open(tmp_wav, 'wb') as wf:
                        wf.setnchannels(self.channels)
                        wf.setsampwidth(2)
                        wf.setframerate(self.audio_rate)
                        wf.writeframes(all_aud)
                    self.audio_wav_path = tmp_wav
                except Exception:
                    pass
            elif self.audio_format == 2:
                # Format 2 is IMA-ADPCM audio
                sadp_wrapped = struct.pack('<4sIIB7s', b'SADP', int(self.audio_rate), len(all_aud)*2, self.channels, b'\x00'*7) + all_aud
                try:
                    _, _, pcm_samples = decode_adp(sadp_wrapped)
                    with wave.open(tmp_wav, 'wb') as wf:
                        wf.setnchannels(self.channels)
                        wf.setsampwidth(2)
                        wf.setframerate(self.audio_rate)
                        wf.writeframes(pcm_samples.tobytes())
                    self.audio_wav_path = tmp_wav
                except Exception:
                    try:
                        with wave.open(tmp_wav, 'wb') as wf:
                            wf.setnchannels(self.channels)
                            wf.setsampwidth(2)
                            wf.setframerate(self.audio_rate)
                            wf.writeframes(all_aud)
                        self.audio_wav_path = tmp_wav
                    except Exception:
                        pass
            else:
                # Fallback to PCM
                try:
                    with wave.open(tmp_wav, 'wb') as wf:
                        wf.setnchannels(self.channels)
                        wf.setsampwidth(2)
                        wf.setframerate(self.audio_rate)
                        wf.writeframes(all_aud)
                    self.audio_wav_path = tmp_wav
                except Exception:
                    pass

    def reset(self):
        self.f.seek(32)
        self.frame_pixels.fill(0)
        self.frame_idx = 0

    def next_frame(self):
        w, h = self.width, self.height
        total_pixels = w * h
        flat_view = self.frame_pixels.reshape(-1, 3)

        while True:
            tag_b = self.f.read(1)
            if not tag_b:
                return None
            tag = tag_b[0]
            sz_b = self.f.read(3)
            if len(sz_b) < 3:
                return None
            sz = sz_b[0] | (sz_b[1] << 8) | (sz_b[2] << 16)

            if tag == 0x03:
                self.f.seek(sz, 1)
            elif tag in (0x01, 0x02):
                uncomp_b = self.f.read(3)
                comp_data = self.f.read(sz)
                decomp = zlib.decompress(comp_data)

                if tag == 0x01:
                    flat_view.fill(0)

                pixel_idx = 0
                idx = 0
                n = len(decomp)

                if self.format == 1: # RGB444 packed (official SnakeEngine format)
                    while idx < n and pixel_idx < total_pixels:
                        op = decomp[idx]
                        idx += 1
                        if op == 0x00: break
                        if idx + 2 > n: break
                        cnt = decomp[idx] | (decomp[idx+1] << 8)
                        idx += 2
                        if op == 0x01:
                            pixel_idx += cnt
                        elif op == 0x02:
                            for k in range(0, cnt, 2):
                                if k + 1 < cnt:
                                    if idx + 2 >= n: break
                                    b0, b1, b2 = decomp[idx], decomp[idx+1], decomp[idx+2]
                                    idx += 3
                                    r0 = (b0 >> 4) * 17
                                    g0 = (b0 & 0x0F) * 17
                                    b0_c = (b1 >> 4) * 17
                                    r1 = (b1 & 0x0F) * 17
                                    g1 = (b2 >> 4) * 17
                                    b1_c = (b2 & 0x0F) * 17

                                    px0 = pixel_idx
                                    px1 = pixel_idx + 1
                                    pixel_idx += 2
                                    if px0 < total_pixels: flat_view[px0] = [r0, g0, b0_c]
                                    if px1 < total_pixels: flat_view[px1] = [r1, g1, b1_c]
                                else:
                                    if idx + 1 >= n: break
                                    b0, b1 = decomp[idx], decomp[idx+1]
                                    idx += 2
                                    r0 = (b0 >> 4) * 17
                                    g0 = (b0 & 0x0F) * 17
                                    b0_c = (b1 >> 4) * 17
                                    px0 = pixel_idx
                                    pixel_idx += 1
                                    if px0 < total_pixels: flat_view[px0] = [r0, g0, b0_c]
                else: # RGB565 raw 16-bit
                    while idx < n and pixel_idx < total_pixels:
                        op = decomp[idx]
                        idx += 1
                        if op == 0x00: break
                        if idx + 2 > n: break
                        cnt = decomp[idx] | (decomp[idx+1] << 8)
                        idx += 2
                        if op == 0x01:
                            pixel_idx += cnt
                        elif op == 0x02:
                            to_copy = min(cnt, (n - idx) // 2)
                            to_copy = min(to_copy, total_pixels - pixel_idx)
                            if to_copy > 0:
                                words = np.frombuffer(decomp[idx:idx+to_copy*2], dtype=np.uint16)
                                r = ((words >> 11) & 0x1F) * 255 // 31
                                g = ((words >> 5) & 0x3F) * 255 // 63
                                b = (words & 0x1F) * 255 // 31
                                flat_view[pixel_idx:pixel_idx+to_copy, 0] = r
                                flat_view[pixel_idx:pixel_idx+to_copy, 1] = g
                                flat_view[pixel_idx:pixel_idx+to_copy, 2] = b
                                pixel_idx += to_copy
                            idx += cnt * 2

                self.frame_idx += 1
                return Image.fromarray(self.frame_pixels, 'RGB')

    def extract_audio_to_temp_wav(self):
        return self.audio_wav_path

    def seek_frame(self, target_idx):
        if target_idx <= 0:
            self.reset()

    @property
    def current_frame_idx(self):
        return self.frame_idx

    def close(self):
        try: self.f.close()
        except Exception: pass
        if self.audio_wav_path and os.path.exists(self.audio_wav_path):
            try: os.remove(self.audio_wav_path)
            except Exception: pass

def decode_snaky_first_frame(snaky_path):
    vid = SnakyVideoFile(snaky_path)
    meta = {
        "width": vid.width,
        "height": vid.height,
        "fps": vid.fps,
        "total_frames": vid.total_frames,
        "audio_rate": vid.audio_rate,
        "channels": vid.channels,
        "has_audio": vid.has_audio,
        "audio_codec": "ADPCM",
        "size": os.path.getsize(snaky_path)
    }
    img = vid.next_frame()
    vid.close()
    if img is None:
        raise RuntimeError("Could not decode video frame.")
    return img, meta

def convert_snaky_to_mp4(in_path, out_path, progress_cb=None):
    if not FFMPEG:
        raise RuntimeError("ffmpeg binary not found.")

    vid = SnakyVideoFile(in_path)
    temp_dir = tempfile.mkdtemp()
    raw_vid_pipe = [
        FFMPEG, '-y',
        '-f', 'rawvideo',
        '-pix_fmt', 'rgb24',
        '-s', f"{vid.width}x{vid.height}",
        '-r', str(vid.fps),
        '-i', '-',
        '-c:v', 'libx264',
        '-pix_fmt', 'yuv420p',
        '-crf', '18',
        os.path.join(temp_dir, 'video.mp4')
    ]
    proc_v = subprocess.Popen(raw_vid_pipe, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    frame_count = 0
    while True:
        img = vid.next_frame()
        if img is None: break
        rgb_data = img.tobytes()
        proc_v.stdin.write(rgb_data)
        frame_count += 1
        if vid.total_frames > 0 and frame_count % 10 == 0 and progress_cb:
            pct = int(10 + (frame_count / vid.total_frames) * 75)
            progress_cb(pct, f"Exporting frame {frame_count}/{vid.total_frames}...")

    proc_v.stdin.close()
    proc_v.wait()

    final_video = os.path.join(temp_dir, 'video.mp4')
    if vid.audio_wav_path and os.path.exists(vid.audio_wav_path):
        cmd_mux = [
            FFMPEG, '-y',
            '-i', final_video,
            '-i', vid.audio_wav_path,
            '-c:v', 'copy', '-c:a', 'aac', '-b:a', '192k', out_path
        ]
        subprocess.run(cmd_mux, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    else:
        shutil.copyfile(final_video, out_path)

    vid.close()
    shutil.rmtree(temp_dir, ignore_errors=True)
    if progress_cb: progress_cb(100, "Done.")
