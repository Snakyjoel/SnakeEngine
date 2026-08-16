import sys
import struct
import zlib
import os

def optimize_opcodes(data):
    # Decode RLE
    i = 0
    n = len(data)
    pixels = bytearray(400 * 240 * 2) # max size
    ptr = 0
    
    ops = []
    
    while i < n:
        op = data[i]
        i += 1
        if op == 0x00: break
        
        count = data[i] | (data[i+1] << 8)
        i += 2
        
        if op == 0x01:
            ops.append(('SKIP', count))
            ptr += count * 2
        elif op == 0x02:
            ops.append(('COPY', count, data[i:i+count*2]))
            i += count * 2
            ptr += count * 2
            
    # Merge small SKIPs
    new_ops = []
    for op in ops:
        if not new_ops:
            new_ops.append(op)
            continue
            
        last_op = new_ops[-1]
        
        if op[0] == 'SKIP':
            if last_op[0] == 'SKIP':
                new_ops[-1] = ('SKIP', last_op[1] + op[1])
            else:
                new_ops.append(op)
        elif op[0] == 'COPY':
            if last_op[0] == 'COPY':
                new_ops[-1] = ('COPY', last_op[1] + op[1], last_op[2] + op[2])
            elif last_op[0] == 'SKIP' and last_op[1] < 16 and len(new_ops) > 1:
                # Merge small SKIP into previous COPY and this COPY
                prev_copy = new_ops[-2]
                skip = last_op
                
                # Create fake pixels for the SKIP
                fake_pixels = bytearray(skip[1] * 2)
                
                merged_copy_data = prev_copy[2] + fake_pixels + op[2]
                merged_count = prev_copy[1] + skip[1] + op[1]
                
                new_ops.pop() # remove skip
                new_ops[-1] = ('COPY', merged_count, merged_copy_data)
            else:
                new_ops.append(op)
                
    # Re-encode
    out = bytearray()
    for op in new_ops:
        if op[0] == 'SKIP':
            c = op[1]
            while c > 0:
                sc = min(c, 65535)
                out.append(0x01)
                out.extend(sc.to_bytes(2, 'little'))
                c -= sc
        elif op[0] == 'COPY':
            c = op[1]
            d = op[2]
            d_idx = 0
            while c > 0:
                cc = min(c, 65535)
                out.append(0x02)
                out.extend(cc.to_bytes(2, 'little'))
                out.extend(d[d_idx:d_idx+cc*2])
                c -= cc
                d_idx += cc*2
    out.append(0x00)
    return out

def update_snaky(in_path, out_path):
    print(f"Updating {in_path} to zlib format...")
    with open(in_path, "rb") as f:
        data = f.read()

    if len(data) < 32:
        print("File too small.")
        return

    magic = data[0:4]
    if magic != b"SNKY":
        print("Invalid magic.")
        return

    version = struct.unpack('<H', data[4:6])[0]
    if version >= 0x0200:
        print(f"File {in_path} is already version {hex(version)}.")
        return

    out_data = bytearray()
    
    # Update version to 0x0200
    header = bytearray(data[0:32])
    header[4:6] = struct.pack('<H', 0x0200)
    out_data.extend(header)

    offset = 32
    size_len = len(data)

    while offset < size_len:
        chunk_type = data[offset]
        offset += 1
        
        size = data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16)
        offset += 3

        chunk_data = data[offset:offset+size]
        offset += size

        if chunk_type == 0x03:
            # Audio, write as is
            out_data.append(0x03)
            out_data.extend(size.to_bytes(3, byteorder='little'))
            out_data.extend(chunk_data)
        elif chunk_type in (0x01, 0x02):
            # Video Key/Delta
            optimized_data = optimize_opcodes(chunk_data)
            comp = zlib.compress(optimized_data, level=9)
            out_data.append(chunk_type)
            out_data.extend(len(comp).to_bytes(3, byteorder='little'))
            out_data.extend(len(optimized_data).to_bytes(3, byteorder='little'))
            out_data.extend(comp)
        else:
            print(f"Unknown chunk type {chunk_type}")
            break

    with open(out_path, "wb") as f:
        f.write(out_data)

    old_size = os.path.getsize(in_path)
    new_size = len(out_data)
    print(f"Done! Reduced size from {old_size//1024} KB to {new_size//1024} KB ({(1 - new_size/old_size)*100:.2f}% reduction)")

if __name__ == "__main__":
    import glob
    videos = glob.glob("assets/preload/videos/*.snaky")
    for v in videos:
        # Check if already zlib
        with open(v, "rb") as f:
            if f.read(6)[4:6] == b'\x00\x02':
                continue
        tmp_out = v + ".tmp"
        update_snaky(v, tmp_out)
        os.replace(tmp_out, v)
    print("All videos updated!")
