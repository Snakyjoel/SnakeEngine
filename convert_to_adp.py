import os
import sys
import struct
import glob
import subprocess

INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8]
STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]

def encode_ima_adpcm(samples):
    num_samples = len(samples)
    adpcm = bytearray(num_samples // 2 + 1)
    
    predictor = 0
    step_index = 0
    
    pending_nibble = 0
    has_pending = False
    
    out_idx = 0
    
    for i in range(num_samples):
        sample = samples[i]
        diff = sample - predictor
        step = STEP_TABLE[step_index]
        
        nibble = 0
        if diff < 0:
            nibble = 8
            diff = -diff
            
        mask = 4
        temp_diff = step
        for _ in range(3):
            if diff >= temp_diff:
                nibble |= mask
                diff -= temp_diff
            temp_diff >>= 1
            mask >>= 1
            
        pred_diff = step >> 3
        if (nibble & 4): pred_diff += step
        if (nibble & 2): pred_diff += (step >> 1)
        if (nibble & 1): pred_diff += (step >> 2)
        
        if (nibble & 8):
            predictor -= pred_diff
        else:
            predictor += pred_diff
            
        if predictor < -32768: predictor = -32768
        elif predictor > 32767: predictor = 32767
        
        step_index += INDEX_TABLE[nibble & 7]
        if step_index < 0: step_index = 0
        elif step_index > 88: step_index = 88
        
        if not has_pending:
            pending_nibble = nibble
            has_pending = True
        else:
            adpcm[out_idx] = pending_nibble | (nibble << 4)
            out_idx += 1
            has_pending = False
            
    if has_pending:
        adpcm[out_idx] = pending_nibble
        out_idx += 1
        
    return adpcm[:out_idx]

def convert_to_sadp(input_file, output_file, target_hz=44100):
    # Try running ffmpeg to decode OGG to raw PCM
    ffmpeg_cmd = ['ffmpeg', '-y', '-i', input_file, '-f', 's16le', '-ac', '1', '-ar', str(target_hz), '-']
    # Check if local ffmpeg.exe exists in current directory
    if os.path.exists('ffmpeg.exe'):
        ffmpeg_cmd[0] = './ffmpeg.exe'
        
    try:
        process = subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        pcm_data, err = process.communicate()
    except Exception as e:
        print(f"Error executing ffmpeg: {e}")
        print("Please ensure 'ffmpeg' is installed or place 'ffmpeg.exe' in this directory.")
        return False

    if process.returncode != 0:
        print(f"ffmpeg error on {input_file}: {err.decode(errors='ignore')}")
        return False

    num_samples = len(pcm_data) // 2
    if num_samples == 0:
        print(f"No audio samples decoded from {input_file}")
        return False

    print(f"  Decoding OK. Encoding {num_samples} samples...")
    samples = struct.unpack(f"<{num_samples}h", pcm_data)
    encoded = encode_ima_adpcm(samples)
    
    with open(output_file, "wb") as f:
        f.write(b"SADP")
        f.write(struct.pack("<IIB", target_hz, num_samples, 1))
        f.write(b"\x00" * 7) # Reserved
        f.write(encoded)
    return True

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Convert OGG to SADP for SnakeEngine")
    parser.add_argument("--dir", type=str, default="assets/songs", help="Directory to scan")
    parser.add_argument("--hz", type=int, default=44100, help="Target sample rate (44100 or 22050)")
    args = parser.parse_args()

    # Search for all oggs
    files = glob.glob(os.path.join(args.dir, "**", "*.ogg"), recursive=True)
    if not files:
        print(f"No OGG files found in {args.dir}")
        sys.exit(0)
        
    print(f"Found {len(files)} OGG files. Converting to ADP...")
    
    success_count = 0
    for file in files:
        out_file = os.path.splitext(file)[0] + ".adp"
        print(f"[{file}] -> [{out_file}]")
        if convert_to_sadp(file, out_file, target_hz=args.hz):
            success_count += 1
        
    print(f"Done! Successfully converted {success_count}/{len(files)} files.")
