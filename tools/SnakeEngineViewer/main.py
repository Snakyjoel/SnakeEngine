import os
import sys
import struct
import tempfile
import threading
import subprocess
import time
import zlib
from PIL import Image, ImageTk
import numpy as np
import customtkinter as ctk
import winsound # Native Windows API for audio playback without dependencies

try:
    import imageio_ffmpeg
    FFMPEG_PATH = imageio_ffmpeg.get_ffmpeg_exe()
except ImportError:
    FFMPEG_PATH = "ffmpeg"

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

MAX_SIZE = 4096

# -------------------------------------------------------------
# ADP DECODER
# -------------------------------------------------------------
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8]
STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]

def create_wav_file(pcm_data, sample_rate, num_samples, channels, out_path):
    import wave
    with wave.open(out_path, 'wb') as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(pcm_data)

def decode_adp(file_path):
    with open(file_path, "rb") as f:
        header = f.read(20)
        if len(header) < 20: return None
        magic, sample_rate, num_samples, channels = struct.unpack("<4sIIB", header[:13])
        if magic != b"SADP": return None
        
        adpcm_data = f.read()
    
    predictor = 0
    step_index = 0
    out_samples = bytearray()
    
    for byte in adpcm_data:
        # Low nibble
        nibble = byte & 0x0F
        step = STEP_TABLE[step_index]
        diff = step >> 3
        if nibble & 4: diff += step
        if nibble & 2: diff += (step >> 1)
        if nibble & 1: diff += (step >> 2)
        if nibble & 8: predictor -= diff
        else: predictor += diff
        predictor = max(-32768, min(32767, predictor))
        step_index = max(0, min(88, step_index + INDEX_TABLE[nibble & 7]))
        out_samples += struct.pack("<h", predictor)
        
        # High nibble
        nibble = (byte >> 4) & 0x0F
        step = STEP_TABLE[step_index]
        diff = step >> 3
        if nibble & 4: diff += step
        if nibble & 2: diff += (step >> 1)
        if nibble & 1: diff += (step >> 2)
        if nibble & 8: predictor -= diff
        else: predictor += diff
        predictor = max(-32768, min(32767, predictor))
        step_index = max(0, min(88, step_index + INDEX_TABLE[nibble & 7]))
        out_samples += struct.pack("<h", predictor)
        
    temp_wav = tempfile.mktemp(suffix=".wav")
    create_wav_file(out_samples[:num_samples*channels*2], sample_rate, num_samples, channels, temp_wav)
    return temp_wav, sample_rate, num_samples, channels

# -------------------------------------------------------------
# IMAGE DECODER
# -------------------------------------------------------------
def morton_unswizzle(x, y):
    i = (x & 7) | ((y & 7) << 8)
    i = (i ^ (i << 2)) & 0x1313
    i = (i ^ (i << 1)) & 0x1515
    return (i & 0xFF) | (((i >> 8) & 0xFF) << 1)

def decode_rawtex(path):
    with open(path, "rb") as f:
        fileBytes = f.read()
    
    if len(fileBytes) >= 12 and fileBytes[:3] == b'RWT':
        magicByte = chr(fileBytes[3])
        if magicByte not in ['X', '4', '5']: return None
        
        pw, ph, w, h = struct.unpack('<HHHH', fileBytes[4:12])
        pixelData = fileBytes[12:]
        
        rawBpp = 4 if magicByte == 'X' else 2
        img = Image.new("RGBA", (w, h))
        pixels = img.load()
        
        for y in range(h):
            for x in range(w):
                local_idx = morton_unswizzle(x, y)
                tx = x >> 3
                ty = y >> 3
                tile_start = (ty * (pw >> 3) + tx) << 6
                src_idx = (tile_start + local_idx) * rawBpp
                
                if src_idx + (rawBpp - 1) < len(pixelData):
                    if magicByte == 'X':
                        a, b, g, r = pixelData[src_idx], pixelData[src_idx+1], pixelData[src_idx+2], pixelData[src_idx+3]
                    elif magicByte == '4':
                        byte0, byte1 = pixelData[src_idx], pixelData[src_idx+1]
                        a = (byte0 & 0x0F) * 17
                        b = ((byte0 >> 4) & 0x0F) * 17
                        g = (byte1 & 0x0F) * 17
                        r = ((byte1 >> 4) & 0x0F) * 17
                    elif magicByte == '5':
                        byte0, byte1 = pixelData[src_idx], pixelData[src_idx+1]
                        rgb = byte0 | (byte1 << 8)
                        b = (rgb & 0x1F) * 255 // 31
                        g = ((rgb >> 5) & 0x3F) * 255 // 63
                        r = ((rgb >> 11) & 0x1F) * 255 // 31
                        a = 255
                    pixels[x, y] = (r, g, b, a)
        return img, f"RAWTEX ({magicByte})"
        
    # T3X check
    if len(fileBytes) > 5:
        numSubTextures = struct.unpack('<H', fileBytes[0:2])[0]
        logDims = fileBytes[2]
        formatByte = fileBytes[3]
        heightLog2 = logDims & 0x07
        widthLog2 = (logDims >> 3) & 0x07
        pwT3x = 1 << (widthLog2 + 3)
        phT3x = 1 << (heightLog2 + 3)
        
        pixelOffset = 5 + numSubTextures * 20
        if pixelOffset < len(fileBytes):
            wT3x, hT3x = pwT3x, phT3x
            if numSubTextures > 0:
                wT3x, hT3x = struct.unpack('<HH', fileBytes[5:9])
            
            t3xPixelData = fileBytes[pixelOffset:]
            bpp = 4
            if formatByte == 1: bpp = 3
            elif formatByte == 4: bpp = 2
            
            img = Image.new("RGBA", (wT3x, hT3x))
            pixels = img.load()
            
            for y in range(hT3x):
                for x in range(wT3x):
                    local_idx = morton_unswizzle(x, y)
                    tx = x >> 3
                    ty = y >> 3
                    tile_start = (ty * (pwT3x >> 3) + tx) << 6
                    src_idx = (tile_start + local_idx) * bpp
                    
                    if src_idx + (bpp - 1) < len(t3xPixelData):
                        if formatByte == 0:
                            a, b, g, r = t3xPixelData[src_idx:src_idx+4]
                        elif formatByte == 1:
                            b, g, r = t3xPixelData[src_idx:src_idx+3]
                            a = 255
                        elif formatByte == 4:
                            byte0, byte1 = t3xPixelData[src_idx], t3xPixelData[src_idx+1]
                            a = (byte0 & 0x0F) * 17
                            b = ((byte0 >> 4) & 0x0F) * 17
                            g = (byte1 & 0x0F) * 17
                            r = ((byte1 >> 4) & 0x0F) * 17
                        else:
                            r=g=b=0; a=255
                        pixels[x, y] = (r, g, b, a)
            return img, f"T3X (Format {formatByte})"
            
    return None, None

# -------------------------------------------------------------
# APP
# -------------------------------------------------------------
class SnakeEngineViewer(ctk.CTk):
    def __init__(self, target_file):
        super().__init__()
        self.title("SnakeEngine Viewer")
        self.geometry("800x600")
        self.target_file = target_file
        self.temp_wav = None
        self.is_playing = False
        
        # Audio/Video info
        self.media_duration = 0
        self.media_start_time = 0
        
        # Image view state
        self.pil_img = None
        self.scale = 1.0
        self.pan_x = 0
        self.pan_y = 0
        self.last_mouse_x = 0
        self.last_mouse_y = 0
        
        # Video state
        self.video_thread = None
        self.stop_video = False
        
        self.build_ui()
        self.load_file()
        
    def build_ui(self):
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)
        
        self.top_frame = ctk.CTkFrame(self)
        self.top_frame.grid(row=0, column=0, sticky="ew", padx=10, pady=10)
        
        self.info_lbl = ctk.CTkLabel(self.top_frame, text="Loading...", font=("Arial", 16, "bold"))
        self.info_lbl.pack(side="left", padx=10)
        
        self.action_btn = ctk.CTkButton(self.top_frame, text="Action", command=self.do_action)
        self.action_btn.pack(side="right", padx=10)
        self.action_btn.configure(state="disabled")
        
        self.time_frame = ctk.CTkFrame(self)
        self.time_lbl = ctk.CTkLabel(self.time_frame, text="00:00 / 00:00")
        self.time_lbl.pack(side="left", padx=10)
        self.progress_bar = ctk.CTkProgressBar(self.time_frame)
        self.progress_bar.pack(side="left", fill="x", expand=True, padx=10)
        self.progress_bar.set(0)
        
        self.canvas_frame = ctk.CTkFrame(self)
        self.canvas_frame.grid(row=1, column=0, sticky="nsew", padx=10, pady=(0, 10))
        self.canvas_frame.grid_rowconfigure(0, weight=1)
        self.canvas_frame.grid_columnconfigure(0, weight=1)
        
        self.img_lbl = ctk.CTkLabel(self.canvas_frame, text="")
        self.img_lbl.grid(row=0, column=0)
        
        # Bind mouse events
        self.img_lbl.bind("<MouseWheel>", self.on_mouse_wheel)
        self.img_lbl.bind("<ButtonPress-1>", self.on_mouse_press)
        self.img_lbl.bind("<B1-Motion>", self.on_mouse_drag)
        
    def load_file(self):
        if not os.path.exists(self.target_file):
            self.info_lbl.configure(text="File not found!")
            return
            
        ext = os.path.splitext(self.target_file)[1].lower()
        size_kb = os.path.getsize(self.target_file) / 1024.0
        
        if ext == ".adp":
            self.geometry("500x150")
            self.canvas_frame.grid_forget()
            self.time_frame.grid(row=1, column=0, sticky="ew", padx=10, pady=(0, 10))
            self.info_lbl.configure(text=f"ADP Audio | Size: {size_kb:.1f} KB")
            
            # Setup Audio UI buttons
            self.action_btn.pack_forget()
            self.play_btn = ctk.CTkButton(self.top_frame, text="Stop", command=self.toggle_audio, width=80)
            self.play_btn.pack(side="right", padx=5)
            self.export_btn = ctk.CTkButton(self.top_frame, text="Export to OGG", command=self.export_audio, width=120)
            self.export_btn.pack(side="right", padx=5)
            
            # Decode ADP
            res = decode_adp(self.target_file)
            if res:
                self.temp_wav, sr, ns, ch = res
                self.media_duration = ns / sr
                winsound.PlaySound(self.temp_wav, winsound.SND_FILENAME | winsound.SND_ASYNC)
                self.is_playing = True
                self.media_start_time = time.time()
                self._update_audio_timebar()
        
        elif ext == ".snaky":
            self.info_lbl.configure(text=f"Snaky Video | Size: {size_kb:.1f} KB")
            self.time_frame.grid(row=2, column=0, sticky="ew", padx=10, pady=(0, 10))
            self.action_btn.configure(text="Play Video", state="normal")
            self.play_snaky_video()
            
        elif ext in [".rawtex", ".t3x"]:
            self.pil_img, format_str = decode_rawtex(self.target_file)
            if self.pil_img:
                w, h = self.pil_img.size
                self.info_lbl.configure(text=f"{format_str} | {w}x{h} | {size_kb:.1f} KB")
                self.action_btn.configure(text="Export to PNG", state="normal")
                
                # Fit image nicely on open
                if w > 750 or h > 500:
                    self.scale = min(750/w, 500/h)
                self.update_image_view()
            else:
                self.info_lbl.configure(text="Failed to decode texture!")
                
        elif ext == ".png":
            try:
                self.pil_img = Image.open(self.target_file)
                w, h = self.pil_img.size
                self.info_lbl.configure(text=f"PNG Image | {w}x{h} | {size_kb:.1f} KB")
                self.action_btn.configure(text="Convert to RAWTEX", state="normal")
                if w > 750 or h > 500:
                    self.scale = min(750/w, 500/h)
                self.update_image_view()
            except Exception as e:
                self.info_lbl.configure(text=f"Failed to load PNG: {e}")
                
    def on_mouse_wheel(self, event):
        if not self.pil_img: return
        old_scale = self.scale
        if event.delta > 0:
            self.scale *= 1.1
        else:
            self.scale /= 1.1
            
        self.scale = max(0.1, min(10.0, self.scale))
        
        # Center zoom on mouse pos
        w, h = self.pil_img.size
        # (This is simplified, standard center zoom math can be complex)
        self.update_image_view()

    def on_mouse_press(self, event):
        self.last_mouse_x = event.x
        self.last_mouse_y = event.y

    def on_mouse_drag(self, event):
        if not self.pil_img: return
        dx = event.x - self.last_mouse_x
        dy = event.y - self.last_mouse_y
        self.pan_x += dx
        self.pan_y += dy
        self.last_mouse_x = event.x
        self.last_mouse_y = event.y
        self.update_image_view()

    def update_image_view(self):
        if not self.pil_img: return
        w, h = self.pil_img.size
        new_w = max(1, int(w * self.scale))
        new_h = max(1, int(h * self.scale))
        
        # Only resize if reasonably small enough, else use original and crop
        if new_w * new_h < 10000000:
            resized = self.pil_img.resize((new_w, new_h), Image.Resampling.NEAREST if self.scale >= 1.0 else Image.Resampling.LANCZOS)
            
            # Simple panning (crop viewport)
            bg = Image.new("RGBA", (750, 500), (0,0,0,0))
            draw_x = (750 - new_w) // 2 + self.pan_x
            draw_y = (500 - new_h) // 2 + self.pan_y
            bg.paste(resized, (draw_x, draw_y))
            
            ctk_img = ctk.CTkImage(light_image=bg, dark_image=bg, size=(750, 500))
            self.img_lbl.configure(image=ctk_img, text="")
            self.img_lbl.image = ctk_img

    def _update_audio_timebar(self):
        if not self.is_playing: return
        elapsed = time.time() - self.media_start_time
        if elapsed >= self.media_duration:
            self.is_playing = False
            self.play_btn.configure(text="Play")
            self.progress_bar.set(1.0)
            return
            
        self.progress_bar.set(elapsed / self.media_duration)
        em = int(elapsed // 60)
        es = int(elapsed % 60)
        tm = int(self.media_duration // 60)
        ts = int(self.media_duration % 60)
        self.time_lbl.configure(text=f"{em:02d}:{es:02d} / {tm:02d}:{ts:02d}")
        self.after(100, self._update_audio_timebar)

    def toggle_audio(self):
        if self.is_playing:
            winsound.PlaySound(None, winsound.SND_PURGE)
            self.is_playing = False
            self.play_btn.configure(text="Play")
        else:
            if self.temp_wav:
                winsound.PlaySound(self.temp_wav, winsound.SND_FILENAME | winsound.SND_ASYNC)
                self.is_playing = True
                self.media_start_time = time.time()
                self.play_btn.configure(text="Stop")
                self._update_audio_timebar()
                
    def export_audio(self):
        if not self.temp_wav: return
        out_path = os.path.splitext(self.target_file)[0] + ".ogg"
        try:
            self.info_lbl.configure(text="Exporting...")
            self.update()
            subprocess.run([FFMPEG_PATH, "-y", "-i", self.temp_wav, out_path], 
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
            self.info_lbl.configure(text=f"Exported to {os.path.basename(out_path)}!")
        except Exception as e:
            self.info_lbl.configure(text=f"FFMpeg error: {e}")

    def do_action(self):
        ext = os.path.splitext(self.target_file)[1].lower()
        if ext == ".snaky":
            if self.is_playing: return
            self.stop_video = True
            if self.video_thread and self.video_thread.is_alive():
                self.video_thread.join(timeout=0.5)
            self.play_snaky_video()
        elif ext in [".rawtex", ".t3x"]:
            out_path = os.path.splitext(self.target_file)[0] + ".png"
            self.pil_img.save(out_path)
            self.info_lbl.configure(text=f"Exported to {os.path.basename(out_path)}!")
        elif ext == ".png":
            # Convert to RAWTEX using converter script logic
            sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "converters")))
            try:
                import convert_assets
                convert_assets.process_file(self.target_file, force=True)
                self.info_lbl.configure(text="Converted successfully!")
            except Exception as e:
                self.info_lbl.configure(text=f"Convert error: {e}")

    def play_snaky_video(self):
        self.stop_video = False
        self.is_playing = True
        self.action_btn.configure(text="Playing...", state="disabled")
        self.video_thread = threading.Thread(target=self._snaky_decoder_loop, daemon=True)
        self.video_thread.start()
        
    def _snaky_decoder_loop(self):
        with open(self.target_file, "rb") as f:
            magic = f.read(4)
            if magic != b"SNKY": return
            
            f.read(2) # ver
            w, h = struct.unpack('<HH', f.read(4))
            fps = struct.unpack('<B', f.read(1))[0]
            fmt = struct.unpack('<B', f.read(1))[0]
            total_frames = struct.unpack('<I', f.read(4))[0]
            audio_rate = struct.unpack('<H', f.read(2))[0]
            channels = struct.unpack('<B', f.read(1))[0]
            has_audio = struct.unpack('<B', f.read(1))[0]
            f.read(12) # padding
            
            # Read all frames and audio
            audio_bytes = bytearray()
            frame_chunks = []
            
            while True:
                chunk_header = f.read(1)
                if not chunk_header: break
                chunk_type = chunk_header[0]
                
                if chunk_type == 0x03:
                    # audio
                    size = struct.unpack('<I', f.read(3) + b'\x00')[0]
                    audio_bytes.extend(f.read(size))
                elif chunk_type in [0x01, 0x02]:
                    # video
                    comp_size = struct.unpack('<I', f.read(3) + b'\x00')[0]
                    uncomp_size = struct.unpack('<I', f.read(3) + b'\x00')[0]
                    comp_data = f.read(comp_size)
                    frame_chunks.append((chunk_type, comp_data, uncomp_size))
            
            # play audio async
            if has_audio and len(audio_bytes) > 0:
                self.temp_wav = tempfile.mktemp(suffix=".wav")
                create_wav_file(audio_bytes, audio_rate, len(audio_bytes)//(2*channels), channels, self.temp_wav)
                winsound.PlaySound(self.temp_wav, winsound.SND_FILENAME | winsound.SND_ASYNC)
                
            frame_buffer = np.zeros(w * h, dtype=np.uint16)
            
            start_time = time.time()
            frame_duration = 1.0 / fps
            
            for i, (ctype, comp_data, uncomp_size) in enumerate(frame_chunks):
                if self.stop_video: break
                
                # Zlib decompress
                opcodes = zlib.decompress(comp_data)
                
                # RLE Decode
                idx = 0
                op_idx = 0
                while op_idx < len(opcodes):
                    op = opcodes[op_idx]
                    op_idx += 1
                    if op == 0x00: break
                    elif op == 0x01:
                        skip = struct.unpack('<H', opcodes[op_idx:op_idx+2])[0]
                        op_idx += 2
                        idx += skip
                    elif op == 0x02:
                        copy = struct.unpack('<H', opcodes[op_idx:op_idx+2])[0]
                        op_idx += 2
                        raw_copy = opcodes[op_idx:op_idx+(copy*2)]
                        # Fast assign
                        arr = np.frombuffer(raw_copy, dtype=np.uint16)
                        frame_buffer[idx:idx+copy] = arr
                        op_idx += (copy*2)
                        idx += copy
                        
                # Time sync
                expected_time = i * frame_duration
                elapsed = time.time() - start_time
                
                # Frame skip
                if elapsed > expected_time + frame_duration:
                    continue # Skip rendering this frame
                    
                # RGB565 to RGB888
                r = (frame_buffer >> 11) & 0x1F
                g = (frame_buffer >> 5) & 0x3F
                b = frame_buffer & 0x1F
                r = (r * 255) // 31
                g = (g * 255) // 63
                b = (b * 255) // 31
                
                rgb888 = np.stack((r, g, b), axis=1).astype(np.uint8)
                img = Image.frombuffer("RGB", (w, h), rgb888.tobytes())
                
                # Update UI
                if w > 750 or h > 500:
                    scale = min(750/w, 500/h)
                    img = img.resize((int(w*scale), int(h*scale)), Image.Resampling.NEAREST)
                
                ctk_img = ctk.CTkImage(light_image=img, dark_image=img, size=img.size)
                self.img_lbl.configure(image=ctk_img)
                self.img_lbl.image = ctk_img
                
                # Update timebar
                self.progress_bar.set(i / total_frames)
                em = int(elapsed // 60)
                es = int(elapsed % 60)
                tm = int((total_frames / fps) // 60)
                ts = int((total_frames / fps) % 60)
                self.time_lbl.configure(text=f"{em:02d}:{es:02d} / {tm:02d}:{ts:02d}")
                
                sleep_time = expected_time - (time.time() - start_time)
                if sleep_time > 0:
                    time.sleep(sleep_time)
            
            self.is_playing = False
            self.action_btn.configure(text="Play Video", state="normal")

    def on_closing(self):
        self.stop_video = True
        if self.is_playing:
            winsound.PlaySound(None, winsound.SND_PURGE)
        if self.temp_wav and os.path.exists(self.temp_wav):
            try:
                os.remove(self.temp_wav)
            except:
                pass
        self.destroy()

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        target = sys.argv[1]
    else:
        target = "No file provided"
        
    app = SnakeEngineViewer(target)
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
