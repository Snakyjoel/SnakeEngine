import os
import sys
import time
import threading
import tempfile
import winsound
import subprocess
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageTk

import core

class SnakeToolsApp(tk.Tk):
    def __init__(self, initial_file=None):
        super().__init__()
        self.title("Snake Tools")
        self.geometry("920x660")
        self.minsize(800, 560)

        # Clean native desktop tool styling
        style = ttk.Style(self)
        try:
            style.theme_use('vista')
        except Exception:
            try:
                style.theme_use('winnative')
            except Exception:
                pass

        self.temp_files = []
        self.snaky_player = None
        self.is_video_playing = False
        self.tk_canvas_img = None

        self._build_ui()

        if initial_file and os.path.isfile(initial_file):
            target = os.path.abspath(initial_file)
            self.after(200, lambda: self.open_in_viewer(target))

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self):
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)

        self.tab_viewer = ttk.Frame(self.notebook)
        self.tab_image = ttk.Frame(self.notebook)
        self.tab_audio = ttk.Frame(self.notebook)
        self.tab_video = ttk.Frame(self.notebook)

        self.notebook.add(self.tab_viewer, text="Viewer, Player & Export")
        self.notebook.add(self.tab_image, text="PNG to T3X")
        self.notebook.add(self.tab_audio, text="Audio to ADP")
        self.notebook.add(self.tab_video, text="Video to SNAKY")

        self._build_viewer_tab()
        self._build_image_tab()
        self._build_audio_tab()
        self._build_video_tab()

    # --- Viewer / Player ---
    def _build_viewer_tab(self):
        top_bar = ttk.Frame(self.tab_viewer)
        top_bar.pack(fill=tk.X, padx=8, pady=(8, 4))

        ttk.Button(top_bar, text="Open File (.t3x, .rawtex, .adp, .snaky)...", command=self._viewer_browse).pack(side=tk.LEFT)
        self.lbl_v_path = ttk.Label(top_bar, text="No file loaded", font=("Segoe UI", 9, "bold"))
        self.lbl_v_path.pack(side=tk.LEFT, padx=10)

        # Buttons to install/uninstall Windows Explorer thumbnail extension
        ttk.Button(top_bar, text="Install Thumbnails", command=self._install_shell_ext).pack(side=tk.RIGHT, padx=2)
        ttk.Button(top_bar, text="Uninstall Extension", command=self._uninstall_shell_ext).pack(side=tk.RIGHT, padx=2)

        main_pane = ttk.PanedWindow(self.tab_viewer, orient=tk.HORIZONTAL)
        main_pane.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        # Left side: Display / Player Area
        left_box = ttk.Frame(main_pane)
        main_pane.add(left_box, weight=3)

        self.viewer_canvas = tk.Canvas(left_box, bg="#000000", highlightthickness=1, highlightbackground="#888888")
        self.viewer_canvas.pack(fill=tk.BOTH, expand=True)

        self.player_bar = ttk.Frame(left_box)
        # Hidden by default until an audio/video file is opened

        self.btn_play = ttk.Button(self.player_bar, text="Play", command=self._toggle_playback)
        self.btn_play.pack(side=tk.LEFT, padx=2)

        self.btn_stop = ttk.Button(self.player_bar, text="Stop", command=self._stop_playback)
        self.btn_stop.pack(side=tk.LEFT, padx=2)

        self.lbl_time = ttk.Label(self.player_bar, text="00:00 / 00:00")
        self.lbl_time.pack(side=tk.LEFT, padx=8)

        # Right side: File Info & Adaptive Export Actions
        right_box = ttk.Frame(main_pane)
        main_pane.add(right_box, weight=1)

        ttk.Label(right_box, text="File Information:", font=("Segoe UI", 9, "bold")).pack(anchor="w", padx=4, pady=(4, 2))
        self.txt_info = tk.Text(right_box, width=28, height=12, wrap=tk.WORD, relief=tk.SOLID, borderwidth=1)
        self.txt_info.pack(fill=tk.X, padx=4, pady=4)
        self.txt_info.insert(tk.END, "Open any Snake Engine asset (.t3x, .rawtex, .adp, .snaky) to preview, play or export.")
        self.txt_info.config(state=tk.DISABLED)

        # Adaptive Actions Box: updates based on file type
        self.box_actions = ttk.LabelFrame(right_box, text="Export & Conversion Actions")
        self.box_actions.pack(fill=tk.X, padx=4, pady=8)

        self.lbl_no_action = ttk.Label(self.box_actions, text="No file selected for export.")
        self.lbl_no_action.pack(padx=6, pady=8)

        self.v_prog = ttk.Progressbar(right_box, mode="determinate")
        self.v_prog.pack(fill=tk.X, padx=4, pady=4)

        self.current_loaded_file = None
        self.current_image_obj = None

    def _viewer_browse(self):
        f = filedialog.askopenfilename(filetypes=[("Snake Engine Files", "*.t3x *.rawtex *.adp *.snaky"), ("All Files", "*.*")])
        if f:
            self.open_in_viewer(f)

    def open_in_viewer(self, path):
        if not path or not os.path.exists(path):
            return

        self._stop_playback()
        self.current_loaded_file = os.path.abspath(path)
        self.lbl_v_path.config(text=os.path.basename(path))
        self.viewer_canvas.delete("all")
        self.v_prog['value'] = 0

        # Clear existing actions
        for w in self.box_actions.winfo_children():
            w.destroy()

        ext = os.path.splitext(path)[1].lower()

        if ext in ('.t3x', '.rawtex'):
            # Images do not need player controls
            self.player_bar.pack_forget()
            try:
                img, info = core.read_3ds_image_as_pil(path)
                self.current_image_obj = img
                self._display_image_on_canvas(img)

                info_text = (
                    f"Format: {info['format']}\n"
                    f"Width: {info['width']} px\n"
                    f"Height: {info['height']} px\n"
                    f"File Size: {info['size'] / 1024:.1f} KB\n"
                )
                self._set_info_text(info_text)

                def exp_png():
                    out = filedialog.asksaveasfilename(defaultextension=".png", filetypes=[("PNG Image", "*.png")])
                    if out:
                        img.save(out, "PNG")
                        messagebox.showinfo("Exported", f"Saved PNG image:\n{out}")

                ttk.Label(self.box_actions, text="Export Image:").pack(anchor="w", padx=6, pady=(4, 2))
                ttk.Button(self.box_actions, text="Export as PNG...", command=exp_png).pack(fill=tk.X, padx=6, pady=4)
            except Exception as e:
                self._set_info_text(f"Error loading image:\n{e}")

        elif ext == '.adp':
            self.player_bar.pack(fill=tk.X, pady=(4, 0))
            self.lbl_time.config(text="")
            self.btn_play.config(text="Play Audio")
            try:
                info = core.get_adp_info(path)
                ch_s = "Stereo" if info['channels'] == 2 else "Mono"
                info_text = (
                    f"Format: 3DS SADP Audio\n"
                    f"Sample Rate: {info['sample_rate']} Hz\n"
                    f"Channels: {ch_s}\n"
                    f"Duration: {info['duration']:.2f} s\n"
                    f"Samples: {info['samples']:,}\n"
                    f"File Size: {info['size'] / 1024:.1f} KB\n"
                )
                self._set_info_text(info_text)

                cw = self.viewer_canvas.winfo_width() or 400
                ch = self.viewer_canvas.winfo_height() or 240
                self.viewer_canvas.create_text(cw // 2, ch // 2, text=f"Audio Track: {os.path.basename(path)}\n({ch_s}, {info['sample_rate']} Hz, {info['duration']:.2f}s)", fill="#ffffff", font=("Segoe UI", 11, "bold"), justify=tk.CENTER)

                def exp_audio(target_fmt):
                    out = filedialog.asksaveasfilename(defaultextension=f".{target_fmt}", filetypes=[(f"{target_fmt.upper()} Audio", f"*.{target_fmt}")])
                    if out:
                        def w():
                            def cb(p, m): self.v_prog['value'] = p
                            try:
                                core.convert_adp_to_audio(path, out, format_type=target_fmt, progress_cb=cb)
                                self.after(0, lambda: messagebox.showinfo("Exported", f"Saved {target_fmt.upper()} audio:\n{out}"))
                            except Exception as err:
                                self.after(0, lambda: messagebox.showerror("Error", str(err)))
                        threading.Thread(target=w, daemon=True).start()

                ttk.Label(self.box_actions, text="Export Audio:").pack(anchor="w", padx=6, pady=(4, 2))
                ttk.Button(self.box_actions, text="Export as WAV...", command=lambda: exp_audio('wav')).pack(fill=tk.X, padx=6, pady=2)
                ttk.Button(self.box_actions, text="Export as OGG...", command=lambda: exp_audio('ogg')).pack(fill=tk.X, padx=6, pady=2)

            except Exception as e:
                self._set_info_text(f"Error reading audio:\n{e}")

        elif ext == '.snaky':
            self.player_bar.pack(fill=tk.X, pady=(4, 0))
            self.lbl_time.config(text="00:00 / 00:00")
            self.btn_play.config(text="Play")
            try:
                if self.snaky_player:
                    self.snaky_player.close()
                self.snaky_player = core.SnakyVideoFile(path)

                dur = (self.snaky_player.total_frames / self.snaky_player.fps) if self.snaky_player.fps > 0 else 0
                if getattr(self.snaky_player, 'has_audio', False):
                    codec_s = "IMA-ADPCM" if getattr(self.snaky_player, 'audio_format', 0) == 2 else "PCM16"
                    ch_s = "Stereo" if getattr(self.snaky_player, 'channels', 1) == 2 else "Mono"
                    aud_text = f"{self.snaky_player.audio_rate} Hz ({codec_s}, {ch_s})"
                else:
                    aud_text = "No Audio"

                fmt_name = "RGB444 Packed (Native)" if getattr(self.snaky_player, 'format', 1) == 1 else "RGB565 Raw"
                info_text = (
                    f"Format: Snaky Video (.snaky)\n"
                    f"Color: {fmt_name}\n"
                    f"Resolution: {self.snaky_player.width}x{self.snaky_player.height}\n"
                    f"Framerate: {self.snaky_player.fps} FPS\n"
                    f"Total Frames: {self.snaky_player.total_frames}\n"
                    f"Duration: {dur:.1f} s\n"
                    f"Audio: {aud_text}\n"
                    f"File Size: {os.path.getsize(path) / (1024*1024):.2f} MB\n"
                )
                self._set_info_text(info_text)

                # Show frame 0
                first_frame = self.snaky_player.next_frame()
                if first_frame:
                    self._display_image_on_canvas(first_frame)

                def exp_mp4():
                    out = filedialog.asksaveasfilename(defaultextension=".mp4", filetypes=[("MP4 Video", "*.mp4")])
                    if out:
                        def w():
                            def cb(p, m): self.v_prog['value'] = p
                            try:
                                core.convert_snaky_to_mp4(path, out, progress_cb=cb)
                                self.after(0, lambda: messagebox.showinfo("Exported", f"Saved MP4 video:\n{out}"))
                            except Exception as err:
                                self.after(0, lambda: messagebox.showerror("Error", str(err)))
                        threading.Thread(target=w, daemon=True).start()

                ttk.Label(self.box_actions, text="Export Video:").pack(anchor="w", padx=6, pady=(4, 2))
                ttk.Button(self.box_actions, text="Export as MP4 Video...", command=exp_mp4).pack(fill=tk.X, padx=6, pady=4)

            except Exception as e:
                self._set_info_text(f"Error loading video:\n{e}")

    def _set_info_text(self, text):
        self.txt_info.config(state=tk.NORMAL)
        self.txt_info.delete("1.0", tk.END)
        self.txt_info.insert(tk.END, text)
        self.txt_info.config(state=tk.DISABLED)

    def _display_image_on_canvas(self, pil_img):
        self.viewer_canvas.delete("all")
        cw = self.viewer_canvas.winfo_width()
        ch = self.viewer_canvas.winfo_height()
        if cw < 50 or ch < 50:
            cw, ch = 480, 320

        im = pil_img.copy()
        im.thumbnail((cw - 10, ch - 10), Image.Resampling.LANCZOS)
        tk_im = ImageTk.PhotoImage(im)
        self.viewer_canvas.create_image(cw // 2, ch // 2, image=tk_im)
        self.tk_canvas_img = tk_im

    def _toggle_playback(self):
        if not self.current_loaded_file:
            return

        ext = os.path.splitext(self.current_loaded_file)[1].lower()
        if ext == '.adp':
            self._stop_playback()
            wav = core.render_audio_preview_wav(self.current_loaded_file, is_adp=True)
            self.temp_files.append(wav)
            winsound.PlaySound(wav, winsound.SND_ASYNC | winsound.SND_FILENAME)
            self.btn_play.config(text="Replay")
            return

        if ext == '.snaky' and self.snaky_player:
            if self.is_video_playing:
                self._pause_video()
            else:
                self._start_video_playback()

    def _start_video_playback(self):
        if not self.snaky_player:
            return
        self.is_video_playing = True
        self.btn_play.config(text="Pause")

        # Play audio track if present
        if self.snaky_player.has_audio:
            wav_path = self.snaky_player.extract_audio_to_temp_wav()
            if wav_path:
                self.temp_files.append(wav_path)
                winsound.PlaySound(wav_path, winsound.SND_ASYNC | winsound.SND_FILENAME)

        threading.Thread(target=self._video_playback_loop, daemon=True).start()

    def _pause_video(self):
        self.is_video_playing = False
        self.btn_play.config(text="Play")
        winsound.PlaySound(None, winsound.SND_PURGE)

    def _stop_playback(self):
        self.is_video_playing = False
        self.btn_play.config(text="Play")
        winsound.PlaySound(None, winsound.SND_PURGE)
        if self.snaky_player:
            self.snaky_player.seek_frame(0)
            f = self.snaky_player.next_frame()
            if f:
                self._display_image_on_canvas(f)
            self.lbl_time.config(text="00:00 / 00:00")

    def _video_playback_loop(self):
        fps = self.snaky_player.fps if self.snaky_player.fps > 0 else 24
        frame_interval = 1.0 / fps
        total_sec = self.snaky_player.total_frames / fps

        while self.is_video_playing:
            start_t = time.time()
            f = self.snaky_player.next_frame()
            if f is None:
                self.snaky_player.seek_frame(0)
                break

            curr_sec = self.snaky_player.current_frame_idx / fps
            time_str = f"{int(curr_sec // 60):02d}:{int(curr_sec % 60):02d} / {int(total_sec // 60):02d}:{int(total_sec % 60):02d}"

            self.after(0, lambda img=f, ts=time_str: self._update_video_frame(img, ts))

            elapsed = time.time() - start_t
            sleep_t = frame_interval - elapsed
            if sleep_t > 0:
                time.sleep(sleep_t)

        self.after(0, lambda: self.btn_play.config(text="Play"))
        self.is_video_playing = False

    def _update_video_frame(self, img, time_str):
        self._display_image_on_canvas(img)
        self.lbl_time.config(text=time_str)

    # --- Windows Explorer Shell Extension ---
    def _install_shell_ext(self):
        try:
            import winreg
            import ctypes

            try:
                import shell_ext_data
                embedded_bytes = shell_ext_data.get_dll_bytes()
            except Exception:
                embedded_bytes = None

            # 1. Determine permanent location for SnakeEngineShellExt.dll
            # Use %LOCALAPPDATA%\SnakeTools so it persists regardless of where SnakeTools.exe is run from
            local_app_data = os.environ.get("LOCALAPPDATA")
            if not local_app_data:
                local_app_data = os.path.expanduser("~")
            install_dir = os.path.join(local_app_data, "SnakeTools")
            os.makedirs(install_dir, exist_ok=True)
            dll_path = os.path.join(install_dir, "SnakeEngineShellExt.dll")

            # 2. Get the DLL binary (from embedded base64 first, then fallback to local disk)
            dll_bytes = embedded_bytes
            if not dll_bytes:
                possible_dirs = [
                    os.path.dirname(os.path.abspath(__file__)),
                    os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."),
                    os.path.join(os.path.dirname(os.path.abspath(__file__)), "source"),
                    os.path.dirname(sys.executable),
                    os.path.join(os.path.dirname(sys.executable), "source"),
                ]
                if getattr(sys, 'frozen', False) and hasattr(sys, '_MEIPASS'):
                    possible_dirs.insert(0, sys._MEIPASS)

                for d in possible_dirs:
                    candidate = os.path.join(d, "SnakeEngineShellExt.dll")
                    if os.path.exists(candidate):
                        with open(candidate, "rb") as f:
                            dll_bytes = f.read()
                        break

            if not dll_bytes:
                messagebox.showerror("Error", "Could not load embedded SnakeEngineShellExt.dll binary.")
                return

            # Write the DLL to the permanent directory
            try:
                with open(dll_path, "wb") as f:
                    f.write(dll_bytes)
            except Exception:
                # If file is currently loaded in memory by Explorer/dllhost, that's fine if it exists
                if not os.path.exists(dll_path):
                    messagebox.showerror("Error", f"Failed to write thumbnail extension to {dll_path}")
                    return

            # 3. Locate SnakeTools.exe
            exe_path = os.path.abspath(sys.executable)
            if not exe_path.lower().endswith(".exe") or "python" in os.path.basename(exe_path).lower():
                candidate_exe = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "SnakeTools.exe")
                if os.path.exists(candidate_exe):
                    exe_path = os.path.abspath(candidate_exe)

            clsid = "{A7F013BD-DE2B-4A1C-9E5C-FCE0CC89B663}"
            provider_guid = "{E357FCCD-A995-4576-B01F-234630154E96}"
            codebase = "file:///" + dll_path.replace("\\", "/")

            # Register CLSID in HKCU
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\CLSID\{clsid}") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, "SnakeEngine Rawtex/T3x/Snaky Thumbnail Provider")
                winreg.SetValueEx(k, "DisableProcessIsolation", 0, winreg.REG_DWORD, 1)

            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\CLSID\{clsid}\InprocServer32") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, "mscoree.dll")
                winreg.SetValueEx(k, "ThreadingModel", 0, winreg.REG_SZ, "Both")
                winreg.SetValueEx(k, "Class", 0, winreg.REG_SZ, "SnakeEngineShellExt.RawtexThumbnailProvider")
                winreg.SetValueEx(k, "Assembly", 0, winreg.REG_SZ, "SnakeEngineShellExt, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null")
                winreg.SetValueEx(k, "RuntimeVersion", 0, winreg.REG_SZ, "v4.0.30319")
                winreg.SetValueEx(k, "CodeBase", 0, winreg.REG_SZ, codebase)

            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\CLSID\{clsid}\InprocServer32\1.0.0.0") as k:
                winreg.SetValueEx(k, "Class", 0, winreg.REG_SZ, "SnakeEngineShellExt.RawtexThumbnailProvider")
                winreg.SetValueEx(k, "Assembly", 0, winreg.REG_SZ, "SnakeEngineShellExt, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null")
                winreg.SetValueEx(k, "RuntimeVersion", 0, winreg.REG_SZ, "v4.0.30319")
                winreg.SetValueEx(k, "CodeBase", 0, winreg.REG_SZ, codebase)

            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\CLSID\{clsid}\ProgId") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, "SnakeTools.ThumbnailProvider")

            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\CLSID\{clsid}\Implemented Categories\{{62C8FE65-4EBB-45E7-B440-6E39B2CDBF29}}"):
                pass

            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\SnakeTools.ThumbnailProvider\CLSID") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, clsid)

            # Register extensions, SystemFileAssociations, and ProgIDs
            exts = [".rawtex", ".t3x", ".snaky"]
            for ext in exts:
                prog_id = f"SnakeTools{ext}"
                perceived = "video" if ext == ".snaky" else "image"
                treatment = 3 if ext == ".snaky" else 2

                # 1. Extension key
                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{ext}") as k:
                    winreg.SetValueEx(k, "", 0, winreg.REG_SZ, prog_id)
                    winreg.SetValueEx(k, "PerceivedType", 0, winreg.REG_SZ, perceived)

                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{ext}\ShellEx\{provider_guid}") as k:
                    winreg.SetValueEx(k, "", 0, winreg.REG_SZ, clsid)

                # 2. SystemFileAssociations (Windows 10 / 11 standard thumbnail mapping)
                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\SystemFileAssociations\{ext}") as k:
                    winreg.SetValueEx(k, "PerceivedType", 0, winreg.REG_SZ, perceived)
                    winreg.SetValueEx(k, "Treatment", 0, winreg.REG_DWORD, treatment)

                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\SystemFileAssociations\{ext}\ShellEx\{provider_guid}") as k:
                    winreg.SetValueEx(k, "", 0, winreg.REG_SZ, clsid)

                # 3. ProgID key
                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{prog_id}") as k:
                    winreg.SetValueEx(k, "", 0, winreg.REG_SZ, f"Snake Tools Asset ({ext})")
                    winreg.SetValueEx(k, "FriendlyTypeName", 0, winreg.REG_SZ, f"Snake Tools Asset ({ext})")

                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{prog_id}\ShellEx\{provider_guid}") as k:
                    winreg.SetValueEx(k, "", 0, winreg.REG_SZ, clsid)

                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{prog_id}\DefaultIcon") as k:
                    winreg.SetValueEx(k, "", 0, winreg.REG_SZ, f"{exe_path},0")

                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{prog_id}\shell\open\command") as k:
                    winreg.SetValueEx(k, "", 0, winreg.REG_SZ, f'"{exe_path}" "%1"')

                # 4. Clear any stale UserChoice override in Explorer\FileExts (e.g. if previously opened with Python)
                try:
                    winreg.DeleteKey(winreg.HKEY_CURRENT_USER, rf"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{ext}\UserChoice")
                except Exception:
                    pass

                try:
                    with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{ext}\OpenWithProgids") as k:
                        winreg.SetValueEx(k, prog_id, 0, winreg.REG_NONE, b"")
                except Exception:
                    pass

            # .adp audio
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\.adp") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, "SnakeTools.adp")
                winreg.SetValueEx(k, "PerceivedType", 0, winreg.REG_SZ, "audio")
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\SnakeTools.adp") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, "Snake Tools ADP Audio")
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\SnakeTools.adp\DefaultIcon") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, f"{exe_path},0")
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Classes\SnakeTools.adp\shell\open\command") as k:
                winreg.SetValueEx(k, "", 0, winreg.REG_SZ, f'"{exe_path}" "%1"')

            # Notify shell
            try:
                ctypes.windll.shell32.SHChangeNotify(0x08000000, 0, None, None) # SHCNE_ASSOCCHANGED
            except Exception:
                pass

            msg = (
                "Successfully registered Windows Explorer thumbnail provider and file associations!\n\n"
                "Explorer will now display live thumbnails for .rawtex, .t3x, and .snaky files.\n\n"
                "Would you like to restart Windows Explorer now to flush any existing thumbnail cache?"
            )
            if messagebox.askyesno("Explorer Thumbnails Installed", msg):
                subprocess.Popen(["cmd.exe", "/c", "taskkill /f /im explorer.exe & start explorer.exe"], shell=False)

        except Exception as e:
            messagebox.showerror("Error", f"Failed to register shell extension:\n{e}")

    def _uninstall_shell_ext(self):
        try:
            import winreg
            import ctypes

            clsid = "{A7F013BD-DE2B-4A1C-9E5C-FCE0CC89B663}"

            def delete_key_tree(root, subkey):
                try:
                    with winreg.OpenKey(root, subkey, 0, winreg.KEY_ALL_ACCESS) as k:
                        while True:
                            try:
                                child = winreg.EnumKey(k, 0)
                                delete_key_tree(root, f"{subkey}\\{child}")
                            except OSError:
                                break
                    winreg.DeleteKey(root, subkey)
                except FileNotFoundError:
                    pass
                except Exception:
                    pass

            delete_key_tree(winreg.HKEY_CURRENT_USER, rf"Software\Classes\CLSID\{clsid}")
            delete_key_tree(winreg.HKEY_CURRENT_USER, r"Software\Classes\SnakeTools.ThumbnailProvider")

            exts = [".rawtex", ".t3x", ".snaky"]
            for ext in exts:
                prog_id = f"SnakeTools{ext}"
                delete_key_tree(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{ext}")
                delete_key_tree(winreg.HKEY_CURRENT_USER, rf"Software\Classes\SystemFileAssociations\{ext}")
                delete_key_tree(winreg.HKEY_CURRENT_USER, rf"Software\Classes\{prog_id}")
                delete_key_tree(winreg.HKEY_CURRENT_USER, rf"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{ext}")

            delete_key_tree(winreg.HKEY_CURRENT_USER, r"Software\Classes\.adp")
            delete_key_tree(winreg.HKEY_CURRENT_USER, r"Software\Classes\SnakeTools.adp")
            delete_key_tree(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.adp")

            try:
                local_app_data = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
                dll_path = os.path.join(local_app_data, "SnakeTools", "SnakeEngineShellExt.dll")
                if os.path.exists(dll_path):
                    os.remove(dll_path)
            except Exception:
                pass

            try:
                ctypes.windll.shell32.SHChangeNotify(0x08000000, 0, None, None)
            except Exception:
                pass

            msg = (
                "Shell extension and file associations have been removed successfully.\n\n"
                "Would you like to restart Windows Explorer now to flush the thumbnail cache?"
            )
            if messagebox.askyesno("Extension Uninstalled", msg):
                subprocess.Popen(["cmd.exe", "/c", "taskkill /f /im explorer.exe & start explorer.exe"], shell=False)

        except Exception as e:
            messagebox.showerror("Error", f"Failed to uninstall shell extension:\n{e}")


    # --- Image to T3X Converter ---
    def _build_image_tab(self):
        file_box = ttk.LabelFrame(self.tab_image, text="Source Image")
        file_box.pack(fill=tk.X, padx=10, pady=6)

        self.img_in_var = tk.StringVar()
        ttk.Entry(file_box, textvariable=self.img_in_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4, pady=4)
        ttk.Button(file_box, text="Browse...", command=self._browse_image).pack(side=tk.RIGHT, padx=4, pady=4)

        opt_box = ttk.LabelFrame(self.tab_image, text="Output Format")
        opt_box.pack(fill=tk.X, padx=10, pady=4)

        ttk.Label(opt_box, text="Pixel Format:").pack(side=tk.LEFT, padx=(6, 2), pady=4)
        self.img_fmt_var = tk.StringVar(value="rgba8888")
        cb_fmt = ttk.Combobox(opt_box, textvariable=self.img_fmt_var, state="readonly", width=14,
                              values=["rgba8888", "rgb565", "rgba4444"])
        cb_fmt.pack(side=tk.LEFT, padx=(2, 12), pady=4)
        cb_fmt.bind("<<ComboboxSelected>>", lambda e: self._refresh_image_preview())

        # Dual preview panels
        prev_container = ttk.Frame(self.tab_image)
        prev_container.pack(fill=tk.BOTH, expand=True, padx=10, pady=4)

        p_left = ttk.LabelFrame(prev_container, text="Original Image")
        p_left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 4))
        self.canvas_og = tk.Canvas(p_left, bg="#1a1a1a", highlightthickness=1)
        self.canvas_og.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        self.lbl_og_stat = ttk.Label(p_left, text="No image loaded")
        self.lbl_og_stat.pack(anchor="w", padx=4, pady=2)

        p_right = ttk.LabelFrame(prev_container, text="Converted .t3x Preview")
        p_right.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(4, 0))
        self.canvas_conv = tk.Canvas(p_right, bg="#1a1a1a", highlightthickness=1)
        self.canvas_conv.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        self.lbl_conv_stat = ttk.Label(p_right, text="Select an image to preview conversion")
        self.lbl_conv_stat.pack(anchor="w", padx=4, pady=2)

        btn_row = ttk.Frame(self.tab_image)
        btn_row.pack(fill=tk.X, padx=10, pady=6)
        ttk.Button(btn_row, text="Convert and Save .t3x...", command=self._convert_t3x_save).pack(side=tk.RIGHT)

        self.tk_og = None
        self.tk_conv = None

    def _browse_image(self):
        f = filedialog.askopenfilename(filetypes=[("Images", "*.png *.jpg *.jpeg *.bmp"), ("All Files", "*.*")])
        if f:
            self.img_in_var.set(f)
            self._refresh_image_preview()

    def _refresh_image_preview(self):
        path = self.img_in_var.get()
        if not path or not os.path.exists(path):
            return

        fmt = self.img_fmt_var.get()

        def w():
            try:
                og = Image.open(path)
                og_sz = os.path.getsize(path) / 1024

                conv_img, conv_bytes = core.get_t3x_preview(path, fmt, "none")
                conv_sz = conv_bytes / 1024
                pct = ((conv_sz - og_sz) / og_sz) * 100 if og_sz > 0 else 0

                def update():
                    self._draw_simple_canvas(og, self.canvas_og, is_og=True)
                    self.lbl_og_stat.config(text=f"Resolution: {og.width}x{og.height} | Original Size: {og_sz:.1f} KB")

                    self._draw_simple_canvas(conv_img, self.canvas_conv, is_og=False)
                    self.lbl_conv_stat.config(text=f"Format: {fmt.upper()} (Uncompressed) | T3X Size: {conv_sz:.1f} KB ({pct:+.1f}%)")

                self.after(0, update)
            except Exception as e:
                self.after(0, lambda: self.lbl_conv_stat.config(text=f"Preview error: {e}"))

        threading.Thread(target=w, daemon=True).start()

    def _draw_simple_canvas(self, pil_img, canvas, is_og=True):
        canvas.delete("all")
        cw = canvas.winfo_width()
        ch = canvas.winfo_height()
        if cw < 40 or ch < 40:
            cw, ch = 320, 200
        im = pil_img.copy()
        im.thumbnail((cw - 12, ch - 12), Image.Resampling.LANCZOS)
        tk_im = ImageTk.PhotoImage(im)
        canvas.create_image(cw // 2, ch // 2, image=tk_im)
        if is_og:
            self.tk_og = tk_im
        else:
            self.tk_conv = tk_im

    def _convert_t3x_save(self):
        src = self.img_in_var.get()
        if not src or not os.path.exists(src):
            messagebox.showerror("Error", "Please select a valid image file.")
            return

        out = filedialog.asksaveasfilename(defaultextension=".t3x", filetypes=[("3DS Texture", "*.t3x")])
        if not out:
            return

        try:
            core.convert_image_to_t3x(src, out, self.img_fmt_var.get(), "none")
            messagebox.showinfo("Success", f"Saved .t3x texture:\n{out}")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    # --- Audio to ADP Converter ---
    def _build_audio_tab(self):
        file_box = ttk.LabelFrame(self.tab_audio, text="Source Audio")
        file_box.pack(fill=tk.X, padx=10, pady=6)

        self.aud_in_var = tk.StringVar()
        ttk.Entry(file_box, textvariable=self.aud_in_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4, pady=4)
        ttk.Button(file_box, text="Browse...", command=self._browse_audio).pack(side=tk.RIGHT, padx=4, pady=4)

        opt_box = ttk.LabelFrame(self.tab_audio, text="Encoding Settings")
        opt_box.pack(fill=tk.X, padx=10, pady=4)

        ttk.Label(opt_box, text="Sample Rate:").pack(side=tk.LEFT, padx=(6, 2), pady=4)
        self.aud_rate_var = tk.StringVar(value="32000 Hz")
        cb_rate = ttk.Combobox(opt_box, textvariable=self.aud_rate_var, state="readonly", width=12,
                               values=["44100 Hz", "32000 Hz", "22050 Hz", "16000 Hz", "11025 Hz"])
        cb_rate.pack(side=tk.LEFT, padx=(2, 10), pady=4)

        ttk.Label(opt_box, text="Channels:").pack(side=tk.LEFT, padx=(6, 2), pady=4)
        self.aud_ch_var = tk.StringVar(value="Stereo (2 Channels)")
        cb_ch = ttk.Combobox(opt_box, textvariable=self.aud_ch_var, state="readonly", width=18,
                             values=["Stereo (2 Channels)", "Mono (1 Channel)"])
        cb_ch.pack(side=tk.LEFT, padx=(2, 8), pady=4)

        play_box = ttk.LabelFrame(self.tab_audio, text="Audio Test (A/B Comparison)")
        play_box.pack(fill=tk.X, padx=10, pady=8)

        row_p = ttk.Frame(play_box)
        row_p.pack(fill=tk.X, padx=6, pady=6)

        ttk.Button(row_p, text="Play Original Audio", command=self._play_aud_og).pack(side=tk.LEFT, padx=4)
        ttk.Button(row_p, text="Play Converted (ADP) Preview", command=self._play_aud_conv).pack(side=tk.LEFT, padx=4)
        ttk.Button(row_p, text="Stop Audio", command=self._stop_playback).pack(side=tk.LEFT, padx=4)

        btn_row = ttk.Frame(self.tab_audio)
        btn_row.pack(fill=tk.X, padx=10, pady=10)
        ttk.Button(btn_row, text="Convert and Save .adp...", command=self._convert_adp_save).pack(side=tk.RIGHT)

    def _browse_audio(self):
        f = filedialog.askopenfilename(filetypes=[("Audio Files", "*.wav *.ogg *.mp3 *.flac"), ("All Files", "*.*")])
        if f:
            self.aud_in_var.set(f)

    def _play_aud_og(self):
        path = self.aud_in_var.get()
        if not path or not os.path.exists(path):
            messagebox.showerror("Error", "Select an audio file first.")
            return
        self._stop_playback()
        try:
            if path.lower().endswith(".wav"):
                w = path
            else:
                w = core.convert_original_to_temp_wav(path)
                self.temp_files.append(w)
            winsound.PlaySound(w, winsound.SND_ASYNC | winsound.SND_FILENAME)
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _play_aud_conv(self):
        path = self.aud_in_var.get()
        if not path or not os.path.exists(path):
            messagebox.showerror("Error", "Select an audio file first.")
            return
        self._stop_playback()
        rate = int(self.aud_rate_var.get().split()[0])
        ch = 1 if "Mono" in self.aud_ch_var.get() else 2

        def w():
            try:
                wav = core.render_audio_preview_wav(path, sample_rate=rate, channels=ch, is_adp=False)
                self.temp_files.append(wav)
                winsound.PlaySound(wav, winsound.SND_ASYNC | winsound.SND_FILENAME)
            except Exception as e:
                self.after(0, lambda: messagebox.showerror("Error", str(e)))

        threading.Thread(target=w, daemon=True).start()

    def _convert_adp_save(self):
        src = self.aud_in_var.get()
        if not src or not os.path.exists(src):
            messagebox.showerror("Error", "Please select a valid audio file.")
            return

        out = filedialog.asksaveasfilename(defaultextension=".adp", filetypes=[("3DS Audio", "*.adp")])
        if not out:
            return

        rate = int(self.aud_rate_var.get().split()[0])
        ch = 1 if "Mono" in self.aud_ch_var.get() else 2

        try:
            core.convert_audio_to_adp(src, out, sample_rate=rate, channels=ch)
            messagebox.showinfo("Success", f"Saved .adp audio:\n{out}")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    # --- Video to SNAKY Converter ---
    def _build_video_tab(self):
        file_box = ttk.LabelFrame(self.tab_video, text="Source Video")
        file_box.pack(fill=tk.X, padx=10, pady=6)

        self.vid_in_var = tk.StringVar()
        ttk.Entry(file_box, textvariable=self.vid_in_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4, pady=4)
        ttk.Button(file_box, text="Browse...", command=self._browse_video).pack(side=tk.RIGHT, padx=4, pady=4)

        opt_box = ttk.LabelFrame(self.tab_video, text="Video & Screen Settings")
        opt_box.pack(fill=tk.X, padx=10, pady=4)

        # Framerate row with custom value entry
        row_fps = ttk.Frame(opt_box)
        row_fps.pack(fill=tk.X, padx=6, pady=4)

        ttk.Label(row_fps, text="Framerate (FPS):", width=18).pack(side=tk.LEFT)
        self.vid_fps_var = tk.StringVar(value="24")
        sp_fps = ttk.Spinbox(row_fps, from_=1, to=60, textvariable=self.vid_fps_var, width=6)
        sp_fps.pack(side=tk.LEFT, padx=4)

        ttk.Label(row_fps, text="(Default: 24 FPS, Max recommended for 3DS: 30 FPS)", foreground="#666666").pack(side=tk.LEFT, padx=8)

        # Resolution slider (from 400x240 down to minimum 160x96)
        row_res = ttk.Frame(opt_box)
        row_res.pack(fill=tk.X, padx=6, pady=6)

        ttk.Label(row_res, text="Target Resolution:", width=18).pack(side=tk.LEFT)
        self.res_slider_var = tk.IntVar(value=400)
        self.slider_res = ttk.Scale(row_res, from_=160, to=400, orient=tk.HORIZONTAL, variable=self.res_slider_var, command=self._on_res_slider_change)
        self.slider_res.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=6)

        self.lbl_res_val = ttk.Label(row_res, text="400x240", width=12, font=("Segoe UI", 9, "bold"))
        self.lbl_res_val.pack(side=tk.LEFT, padx=4)

        # Mode row: Fit (letterbox with black bars) vs Crop (fill screen, no bars, lose edge visual info)
        row_mode = ttk.Frame(opt_box)
        row_mode.pack(fill=tk.X, padx=6, pady=4)

        ttk.Label(row_mode, text="Screen Fitting Mode:", width=18).pack(side=tk.LEFT)
        self.vid_mode_var = tk.StringVar(value="fit")

        rb_fit = ttk.Radiobutton(row_mode, text="Fit (may leave black bars, keeps full image)", variable=self.vid_mode_var, value="fit")
        rb_fit.pack(side=tk.LEFT, padx=(4, 12))

        rb_crop = ttk.Radiobutton(row_mode, text="Crop (fills screen, no bars, crops edges)", variable=self.vid_mode_var, value="crop")
        rb_crop.pack(side=tk.LEFT, padx=4)

        # Audio settings
        self.vid_include_audio_var = tk.BooleanVar(value=True)

        row_aud_toggle = ttk.Frame(opt_box)
        row_aud_toggle.pack(fill=tk.X, padx=6, pady=(4, 2))

        cb_inc_aud = ttk.Checkbutton(
            row_aud_toggle,
            text="Include Audio Track",
            variable=self.vid_include_audio_var,
            command=self._on_toggle_audio
        )
        cb_inc_aud.pack(side=tk.LEFT)

        ttk.Label(
            row_aud_toggle,
            text="(Uncheck for background/gameplay videos to save CPU & file size)",
            foreground="#666666"
        ).pack(side=tk.LEFT, padx=6)

        self.row_aud_params = ttk.Frame(opt_box)
        self.row_aud_params.pack(fill=tk.X, padx=6, pady=(2, 4))

        self.lbl_rate = ttk.Label(self.row_aud_params, text="Audio Sample Rate:", width=18)
        self.lbl_rate.pack(side=tk.LEFT)
        self.vid_rate_var = tk.StringVar(value="32000 Hz")
        self.cb_rate = ttk.Combobox(self.row_aud_params, textvariable=self.vid_rate_var, state="readonly", width=12,
                               values=["44100 Hz", "32000 Hz", "22050 Hz", "16000 Hz", "11025 Hz"])
        self.cb_rate.pack(side=tk.LEFT, padx=4)

        self.lbl_ch = ttk.Label(self.row_aud_params, text="Channels:")
        self.lbl_ch.pack(side=tk.LEFT, padx=(10, 2))
        self.vid_ch_var = tk.StringVar(value="Stereo")
        self.cb_vid_ch = ttk.Combobox(self.row_aud_params, textvariable=self.vid_ch_var, state="readonly", width=8,
                                 values=["Stereo", "Mono"])
        self.cb_vid_ch.pack(side=tk.LEFT, padx=4)

        self.lbl_codec = ttk.Label(self.row_aud_params, text="(Audio Codec: IMA-ADPCM)", foreground="#666666")
        self.lbl_codec.pack(side=tk.LEFT, padx=(10, 4))

        self.vid_stat = ttk.Label(self.tab_video, text="Ready.")
        self.vid_stat.pack(anchor="w", padx=10, pady=(10, 2))

        self.vid_prog = ttk.Progressbar(self.tab_video, mode="determinate")
        self.vid_prog.pack(fill=tk.X, padx=10, pady=(0, 10))

        btn_row = ttk.Frame(self.tab_video)
        btn_row.pack(fill=tk.X, padx=10, pady=4)
        ttk.Button(btn_row, text="Encode to .snaky...", command=self._start_video_encode).pack(side=tk.RIGHT)

    def _on_toggle_audio(self):
        st = "readonly" if self.vid_include_audio_var.get() else "disabled"
        lbl_fg = "#000000" if self.vid_include_audio_var.get() else "#888888"
        self.cb_rate.config(state=st)
        self.cb_vid_ch.config(state=st)
        self.lbl_rate.config(foreground=lbl_fg)
        self.lbl_ch.config(foreground=lbl_fg)

    def _on_res_slider_change(self, val):
        w = int(float(val))
        # Snap to even number
        w = (w // 2) * 2
        # Standard 3DS 5:3 top screen ratio (400:240 = 5:3)
        h = int(w * 240 / 400)
        h = (h // 2) * 2
        self.lbl_res_val.config(text=f"{w}x{h}")

    def _browse_video(self):
        f = filedialog.askopenfilename(filetypes=[("Videos", "*.mp4 *.mkv *.avi *.mov *.webm"), ("All Files", "*.*")])
        if f:
            self.vid_in_var.set(f)

    def _start_video_encode(self):
        src = self.vid_in_var.get()
        if not src or not os.path.exists(src):
            messagebox.showerror("Error", "Select a valid video file.")
            return

        out = filedialog.asksaveasfilename(defaultextension=".snaky", filetypes=[("Snaky Video", "*.snaky")])
        if not out:
            return

        try:
            fps = int(self.vid_fps_var.get().strip())
            if fps <= 0 or fps > 60:
                fps = 24
        except ValueError:
            fps = 24

        res_w = int(self.res_slider_var.get())
        res_w = (res_w // 2) * 2
        res_h = (int(res_w * 240 / 400) // 2) * 2

        mode = self.vid_mode_var.get()
        rate = int(self.vid_rate_var.get().split()[0])
        target_ch = 1 if "Mono" in self.vid_ch_var.get() else 2
        inc_audio = self.vid_include_audio_var.get()

        def w():
            def cb(p, msg):
                self.vid_prog['value'] = p
                self.vid_stat.config(text=msg)

            try:
                cb(5, "Encoding...")
                core.encode_snaky(
                    src, out,
                    fps=fps,
                    target_w=res_w,
                    target_h=res_h,
                    mode=mode,
                    audio_rate=rate,
                    audio_codec="ADPCM",
                    target_channels=target_ch,
                    include_audio=inc_audio,
                    progress_cb=cb
                )
                messagebox.showinfo("Success", f"Video encoded successfully:\n{out}")
            except Exception as e:
                messagebox.showerror("Error", str(e))
                cb(0, f"Error: {e}")

        threading.Thread(target=w, daemon=True).start()

    def _on_close(self):
        self._stop_playback()
        if self.snaky_player:
            self.snaky_player.close()
        for f in self.temp_files:
            try:
                if os.path.exists(f):
                    os.remove(f)
            except Exception:
                pass
        self.destroy()

if __name__ == "__main__":
    initial = sys.argv[1] if len(sys.argv) > 1 else None
    app = SnakeToolsApp(initial_file=initial)
    app.mainloop()
