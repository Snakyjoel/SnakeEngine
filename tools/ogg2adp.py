import os
import sys
import struct
import subprocess
import wave
import io
import array

IMA_INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8)
IMA_STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
)

def find_ffmpeg():
    # 1. Local tool in repository
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_ffmpeg = os.path.join(script_dir, "SnakeTools", "source", "bin", "ffmpeg.exe")
    if os.path.exists(repo_ffmpeg):
        return repo_ffmpeg
    repo_ffmpeg_alt = os.path.join(script_dir, "..", "tools", "SnakeTools", "source", "bin", "ffmpeg.exe")
    if os.path.exists(repo_ffmpeg_alt):
        return os.path.abspath(repo_ffmpeg_alt)
    return "ffmpeg"

def encode_channel(samples):
    pred = 0
    step_idx = 0
    step_tbl = IMA_STEP_TABLE
    idx_tbl = IMA_INDEX_TABLE
    out_bytes = bytearray((len(samples) + 1) // 2)
    b_idx = 0
    has_low = False
    low_nibble = 0

    for s in samples:
        diff = s - pred
        sign = 8 if diff < 0 else 0
        if sign:
            diff = -diff

        step = step_tbl[step_idx]
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
        if pred > 32767:
            pred = 32767
        elif pred < -32768:
            pred = -32768

        step_idx += idx_tbl[delta]
        if step_idx < 0:
            step_idx = 0
        elif step_idx > 88:
            step_idx = 88

        nibble = sign | delta
        if not has_low:
            low_nibble = nibble
            has_low = True
        else:
            out_bytes[b_idx] = low_nibble | (nibble << 4)
            b_idx += 1
            has_low = False

    if has_low:
        out_bytes[b_idx] = low_nibble
        b_idx += 1

    return out_bytes[:b_idx]

def convert_ogg_to_adp(in_path, out_path, target_sr=44100):
    ffmpeg = find_ffmpeg()
    cmd = [
        ffmpeg, '-y', '-v', 'error',
        '-i', in_path,
        '-ar', str(target_sr),
        '-f', 'wav', '-'
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    wav_bytes, err = proc.communicate()
    if proc.returncode != 0:
        raise RuntimeError(f"FFmpeg decode error: {err.decode('utf-8', errors='ignore')}")

    with wave.open(io.BytesIO(wav_bytes), 'rb') as wf:
        channels = wf.getnchannels()
        actual_sr = wf.getframerate()
        raw_pcm = wf.readframes(100000000)

    samples = array.array('h')
    samples.frombytes(raw_pcm)

    if channels == 1:
        num_samples = len(samples)
        raw_adp = encode_channel(samples)
    elif channels == 2:
        left_samples = samples[0::2]
        right_samples = samples[1::2]
        num_samples = len(left_samples)
        encoded_l = encode_channel(left_samples)
        encoded_r = encode_channel(right_samples)

        # Interleave byte by byte
        total_len = len(encoded_l)
        raw_adp = bytearray(total_len * 2)
        raw_adp[0::2] = encoded_l
        raw_adp[1::2] = encoded_r
    else:
        # Downmix to stereo if > 2 channels
        # (FFmpeg can do -ac 2)
        cmd[6:6] = ['-ac', '2']
        channels = 2
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        wav_bytes, err = proc.communicate()
        with wave.open(io.BytesIO(wav_bytes), 'rb') as wf:
            raw_pcm = wf.readframes(100000000)
        samples = array.array('h')
        samples.frombytes(raw_pcm)
        left_samples = samples[0::2]
        right_samples = samples[1::2]
        num_samples = len(left_samples)
        encoded_l = encode_channel(left_samples)
        encoded_r = encode_channel(right_samples)
        raw_adp = bytearray(len(encoded_l) * 2)
        raw_adp[0::2] = encoded_l
        raw_adp[1::2] = encoded_r

    header = struct.pack('<4sIIB7s', b'SADP', int(actual_sr), int(num_samples), int(channels), b'\x00' * 7)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(header)
        f.write(raw_adp)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python ogg2adp.py <input.ogg> <output.adp> [sample_rate]")
        sys.exit(1)
    in_file = sys.argv[1]
    out_file = sys.argv[2]
    sr = int(sys.argv[3]) if len(sys.argv) > 3 else 44100
    convert_ogg_to_adp(in_file, out_file, sr)

