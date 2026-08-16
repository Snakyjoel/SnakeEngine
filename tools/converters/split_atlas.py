import os
import struct
import sys
import xml.etree.ElementTree as ET
from PIL import Image

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

def split_and_convert(image_path, xml_path):
    if not os.path.exists(image_path):
        print(f"Error: {image_path} does not exist.")
        return False
    if not os.path.exists(xml_path):
        print(f"Error: {xml_path} does not exist.")
        return False

    # Load image
    img = Image.open(image_path).convert("RGBA")
    width, height = img.size
    print(f"Loaded {image_path} ({width}x{height})")

    # Custom split lines to avoid cutting frames
    xs = [875]
    ys = [804, 1608]

    # Full boundaries (including start and end)
    x_bounds = [0] + xs + [width]
    y_bounds = [0] + ys + [height]

    cols = len(x_bounds) - 1
    rows = len(y_bounds) - 1
    print(f"Splitting into custom grid: cols={cols}, rows={rows} ({cols * rows} textures)...")

    base_dir = os.path.dirname(image_path)
    base_name, _ = os.path.splitext(os.path.basename(image_path))

    # Slice and convert each chunk
    for r in range(rows):
        for c in range(cols):
            idx = c + r * cols
            
            x0 = x_bounds[c]
            y0 = y_bounds[r]
            x1 = x_bounds[c+1]
            y1 = y_bounds[r+1]
            
            print(f"  Cropping chunk {idx}: rect({x0}, {y0}, {x1}, {y1})")
            chunk = img.crop((x0, y0, x1, y1))
            
            # Swizzle and save to .rawtex
            out_data, pw, ph = swizzle_image_data(chunk)
            w, h = chunk.size
            
            header = struct.pack('<4sHHHH', b'RWTX', pw, ph, w, h)
            out_path = os.path.join(base_dir, f"{base_name}_{idx}.rawtex")
            
            # Clean up old file if exists
            if os.path.exists(out_path):
                os.remove(out_path)
                
            with open(out_path, "wb") as f:
                f.write(header)
                f.write(out_data)
            print(f"  Saved {out_path} ({w}x{h} -> pw2: {pw}x{ph})")

    # Update XML with split="true" and split points
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        root.set("split", "true")
        root.set("splitX", ",".join(map(str, xs)))
        root.set("splitY", ",".join(map(str, ys)))
        
        # Write back to XML
        tree.write(xml_path, encoding='utf-8', xml_declaration=True)
        print(f"Successfully updated {xml_path} with custom split attributes")
        return True
    except Exception as e:
        print(f"Error updating XML {xml_path}: {e}")
        return False

if __name__ == "__main__":
    img_path = r"C:\Users\Usuario\Desktop\SnakeEngine\assets\preload\images\gfDanceTitle.png"
    xml_path = r"C:\Users\Usuario\Desktop\SnakeEngine\assets\preload\images\gfDanceTitle.xml"
    split_and_convert(img_path, xml_path)
