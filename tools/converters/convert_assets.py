import os
import struct
import sys
import xml.etree.ElementTree as ET
from PIL import Image

MAX_SIZE = 2048  # Safe resolution limit for New 3DS PICA200 GPU

def scale_xml_file(xml_path, scale_factor):
    if not os.path.exists(xml_path):
        return False
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        for subtex in root.findall('SubTexture'):
            for attr in ['x', 'y', 'width', 'height', 'frameX', 'frameY', 'frameWidth', 'frameHeight']:
                if attr in subtex.attrib:
                    val = float(subtex.attrib[attr])
                    # Multiply and round to integer
                    subtex.attrib[attr] = str(round(val * scale_factor))
        tree.write(xml_path, encoding='utf-8', xml_declaration=True)
        print(f"  [XML] Scaled coordinates in {os.path.basename(xml_path)} by {scale_factor:.4f}")
        return True
    except Exception as e:
        print(f"  [XML] Error scaling XML {xml_path}: {e}")
        return False

def unswizzle_rawtex_data(data, pw, ph, w, h):
    img = Image.new("RGBA", (w, h))
    pixels = img.load()
    
    for y in range(h):
        for x in range(w):
            i = (x & 7) | ((y & 7) << 8)
            i = (i ^ (i << 2)) & 0x1313
            i = (i ^ (i << 1)) & 0x1515
            
            tx = x >> 3
            ty = y >> 3
            tile_start = (ty * (pw >> 3) + tx) << 6
            local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1)
            
            src_idx = (tile_start + local_idx) * 4
            if src_idx + 3 < len(data):
                a = data[src_idx]
                b = data[src_idx + 1]
                g = data[src_idx + 2]
                r = data[src_idx + 3]
                pixels[x, y] = (r, g, b, a)
    return img

def swizzle_image_data(img):
    w, h = img.size
    
    # Calculate next power of two
    pw = 1
    while pw < w:
        pw *= 2
    ph = 1
    while ph < h:
        ph *= 2

    pixels = img.load()
    out_data = bytearray(pw * ph * 4)

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            
            i = (x & 7) | ((y & 7) << 8)
            i = (i ^ (i << 2)) & 0x1313
            i = (i ^ (i << 1)) & 0x1515
            
            tx = x >> 3
            ty = y >> 3
            tile_start = (ty * (pw >> 3) + tx) << 6
            local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1)
            
            dest_idx = (tile_start + local_idx) * 4
            out_data[dest_idx] = a
            out_data[dest_idx + 1] = b
            out_data[dest_idx + 2] = g
            out_data[dest_idx + 3] = r

    return out_data, pw, ph

def process_file(file_path, force=False):
    is_rawtex = file_path.lower().endswith(".rawtex")
    
    img = None
    if is_rawtex:
        try:
            with open(file_path, "rb") as f:
                header_bytes = f.read(12)
                if len(header_bytes) < 12:
                    return False
                magic, pw, ph, w, h = struct.unpack('<4sHHHH', header_bytes)
                if magic != b'RWTX':
                    return False
                raw_data = f.read()
            
            # Skip if it is already within the safe maximum size limit, unless forced
            if not force and w <= MAX_SIZE and h <= MAX_SIZE:
                return False
            
            print(f"Processing RAWtex: {os.path.basename(file_path)} ({w}x{h}). Unswizzling...")
            img = unswizzle_rawtex_data(raw_data, pw, ph, w, h)
        except Exception as e:
            print(f"Error reading rawtex {file_path}: {e}")
            return False
    else:
        try:
            img = Image.open(file_path)
        except Exception as e:
            print(f"Error opening image {file_path}: {e}")
            return False

    if not img:
        return False

    orig_w, orig_h = img.size
    w, h = orig_w, orig_h
    
    # Calculate scale factor if larger than MAX_SIZE
    scale_factor = 1.0
    if orig_w > MAX_SIZE or orig_h > MAX_SIZE:
        if orig_w > orig_h:
            scale_factor = MAX_SIZE / orig_w
            w = MAX_SIZE
            h = int(orig_h * scale_factor)
        else:
            scale_factor = MAX_SIZE / orig_h
            h = MAX_SIZE
            w = int(orig_w * scale_factor)

    # Resize if needed
    if scale_factor < 1.0:
        print(f"Resizing: {os.path.basename(file_path)} ({orig_w}x{orig_h} -> {w}x{h}) due to safe 3DS limits.")
        img = img.resize((w, h), Image.Resampling.LANCZOS)
        
        # Scale corresponding XML
        base_path, _ = os.path.splitext(file_path)
        xml_path = base_path + ".xml"
        if os.path.exists(xml_path):
            scale_xml_file(xml_path, scale_factor)

    # Swizzle image data
    img_rgba = img.convert('RGBA')
    out_data, pw, ph = swizzle_image_data(img_rgba)

    # Save rawtex
    header = struct.pack('<4sHHHH', b'RWTX', pw, ph, w, h)
    base_path, _ = os.path.splitext(file_path)
    out_path = base_path + ".rawtex"

    try:
        # Write out_path to a temporary file first, then replace (for safety)
        with open(out_path + ".tmp", "wb") as f:
            f.write(header)
            f.write(out_data)
        
        if os.path.exists(out_path):
            os.remove(out_path)
        os.rename(out_path + ".tmp", out_path)
        
        # Remove original file if it was a PNG
        if not is_rawtex and os.path.exists(file_path):
            os.remove(file_path)
            
        print(f"Success! Converted/Resized {os.path.basename(out_path)} ({w}x{h} -> pw2: {pw}x{ph})")
        return True
    except Exception as e:
        print(f"Error saving swizzled image: {e}")
        return False

def scan_and_convert(mod_dir, force=False):
    if not os.path.exists(mod_dir):
        print(f"Error: Path '{mod_dir}' does not exist.")
        return

    converted_count = 0
    # Process both .png and already-converted huge .rawtex files!
    for root, dirs, files in os.walk(mod_dir):
        for file in files:
            ext = file.lower()
            if ext.endswith(".png") or ext.endswith(".rawtex"):
                file_path = os.path.join(root, file)
                if process_file(file_path, force=force):
                    converted_count += 1

    print(f"\nFinished! Converted/Optimized {converted_count} files successfully.")

if __name__ == "__main__":
    force_mode = False
    args = sys.argv[1:]
    if "--force" in args:
        force_mode = True
        args.remove("--force")
        print("Force mode enabled: All .rawtex files will be cleanly re-processed.")

    if len(args) > 0:
        mod_directory = args[0]
    else:
        default_path = r"C:\Users\alvar\AppData\Roaming\Citra\sdmc\SnakeEngine\mods\Nocturnal Protocol (Fan Recreation)"
        if os.path.exists(default_path):
            mod_directory = default_path
        else:
            mod_directory = input("Enter the path to your mod folder: ").strip().strip('"')

    print(f"Scanning directory: {mod_directory}")
    scan_and_convert(mod_directory, force=force_mode)
