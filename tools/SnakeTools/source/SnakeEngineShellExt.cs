using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

[assembly: AssemblyTitle("SnakeEngineShellExt")]
[assembly: AssemblyProduct("Snake Engine Tools")]
[assembly: AssemblyVersion("1.0.0.0")]
[assembly: AssemblyFileVersion("1.0.0.0")]

namespace SnakeEngineShellExt
{
    [ComVisible(true)]
    [Guid("E357FCCD-A995-4576-B01F-234630154E96")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IThumbnailProvider
    {
        void GetThumbnail(uint cx, out IntPtr phbmp, out uint pdwAlpha);
    }

    [ComVisible(true)]
    [Guid("b7a88764-929f-4111-b49d-509769da7363")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IInitializeWithFile
    {
        void Initialize([MarshalAs(UnmanagedType.LPWStr)] string pszFilePath, uint grfMode);
    }

    [ComVisible(true)]
    [Guid("b824b49d-22ac-4161-ac8a-9916e8fa3f7f")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IInitializeWithStream
    {
        void Initialize(System.Runtime.InteropServices.ComTypes.IStream pstream, uint grfMode);
    }

    [ComVisible(true)]
    [Guid("7f7370d9-1179-4836-bd32-82ede4e660ac")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IInitializeWithItem
    {
        void Initialize(IntPtr psi, uint grfMode);
    }

    [ComVisible(true)]
    [Guid("A7F013BD-DE2B-4A1C-9E5C-FCE0CC89B663")]
    [ClassInterface(ClassInterfaceType.None)]
    [ProgId("SnakeTools.ThumbnailProvider")]
    public class RawtexThumbnailProvider : IThumbnailProvider, IInitializeWithFile, IInitializeWithStream, IInitializeWithItem
    {
        private byte[] _fileBytes;

        public void Initialize(string pszFilePath, uint grfMode)
        {
            try
            {
                if (!string.IsNullOrEmpty(pszFilePath) && File.Exists(pszFilePath))
                {
                    _fileBytes = File.ReadAllBytes(pszFilePath);
                }
            }
            catch
            {
            }
        }

        public void Initialize(System.Runtime.InteropServices.ComTypes.IStream pstream, uint grfMode)
        {
            if (pstream == null) return;
            try
            {
                using (var ms = new MemoryStream())
                {
                    byte[] buffer = new byte[16384];
                    IntPtr pcbRead = Marshal.AllocCoTaskMem(Marshal.SizeOf(typeof(int)));
                    try
                    {
                        while (true)
                        {
                            pstream.Read(buffer, buffer.Length, pcbRead);
                            int bytesRead = Marshal.ReadInt32(pcbRead);
                            if (bytesRead <= 0) break;
                            ms.Write(buffer, 0, bytesRead);
                        }
                        _fileBytes = ms.ToArray();
                    }
                    finally
                    {
                        Marshal.FreeCoTaskMem(pcbRead);
                    }
                }
            }
            catch
            {
            }
        }

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate int GetDisplayNameDelegate(IntPtr thisPtr, uint sigdnName, out IntPtr ppszName);

        public void Initialize(IntPtr psi, uint grfMode)
        {
            try
            {
                if (psi != IntPtr.Zero)
                {
                    IntPtr pVtbl = Marshal.ReadIntPtr(psi);
                    IntPtr pGetDisplayName = Marshal.ReadIntPtr(pVtbl, 5 * IntPtr.Size);
                    var getDisplayNameFunc = (GetDisplayNameDelegate)Marshal.GetDelegateForFunctionPointer(pGetDisplayName, typeof(GetDisplayNameDelegate));
                    IntPtr pStr;
                    if (getDisplayNameFunc(psi, 0x80058000, out pStr) == 0 && pStr != IntPtr.Zero)
                    {
                        string path = Marshal.PtrToStringUni(pStr);
                        Marshal.FreeCoTaskMem(pStr);
                        if (!string.IsNullOrEmpty(path) && File.Exists(path))
                        {
                            _fileBytes = File.ReadAllBytes(path);
                        }
                    }
                }
            }
            catch
            {
            }
        }

        public void GetThumbnail(uint cx, out IntPtr phbmp, out uint pdwAlpha)
        {
            phbmp = IntPtr.Zero;
            pdwAlpha = 0; // WTSAT_UNKNOWN (allows Explorer to render both transparent & opaque surfaces)

            try
            {
                if (_fileBytes == null || _fileBytes.Length < 5)
                    return;

                using (Bitmap bmp = DecodeFile(_fileBytes))
                {
                    if (bmp != null)
                    {
                        phbmp = bmp.GetHbitmap();
                    }
                }
            }
            catch
            {
            }
        }

        private static int MortonUnswizzle(int x, int y)
        {
            int i = (x & 7) | ((y & 7) << 8);
            i = (i ^ (i << 2)) & 0x1313;
            i = (i ^ (i << 1)) & 0x1515;
            return (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
        }

        private static byte[] DecompressLZ11(byte[] data, int startOffset)
        {
            if (data == null || startOffset >= data.Length) return null;
            if (data[startOffset] == 0x00 && startOffset + 4 <= data.Length)
            {
                byte[] uncomp = new byte[data.Length - (startOffset + 4)];
                Array.Copy(data, startOffset + 4, uncomp, 0, uncomp.Length);
                return uncomp;
            }
            if (data[startOffset] != 0x11)
            {
                byte[] uncomp = new byte[data.Length - startOffset];
                Array.Copy(data, startOffset, uncomp, 0, uncomp.Length);
                return uncomp;
            }

            int decompSize = data[startOffset + 1] | (data[startOffset + 2] << 8) | (data[startOffset + 3] << 16);
            int idx = startOffset + 4;
            if (decompSize == 0 && startOffset + 8 <= data.Length)
            {
                decompSize = BitConverter.ToInt32(data, startOffset + 4);
                idx = startOffset + 8;
            }

            byte[] outBuf = new byte[decompSize];
            int outPtr = 0;

            while (outPtr < decompSize && idx < data.Length)
            {
                byte flags = data[idx++];
                for (int b = 0; b < 8; b++)
                {
                    if (outPtr >= decompSize || idx >= data.Length) break;
                    if ((flags & (0x80 >> b)) == 0)
                    {
                        outBuf[outPtr++] = data[idx++];
                    }
                    else
                    {
                        if (idx + 1 >= data.Length) break;
                        byte b0 = data[idx++];
                        byte b1 = data[idx++];
                        int indicator = b0 >> 4;
                        int length;
                        int disp;

                        if (indicator == 0)
                        {
                            if (idx >= data.Length) break;
                            byte b2 = data[idx++];
                            length = (((b0 & 0x0F) << 4) | (b1 >> 4)) + 0x11;
                            disp = (((b1 & 0x0F) << 8) | b2) + 1;
                        }
                        else if (indicator == 1)
                        {
                            if (idx + 1 >= data.Length) break;
                            byte b2 = data[idx++];
                            byte b3 = data[idx++];
                            length = (((b0 & 0x0F) << 12) | (b1 << 4) | (b2 >> 4)) + 0x111;
                            disp = (((b2 & 0x0F) << 8) | b3) + 1;
                        }
                        else
                        {
                            length = indicator + 1;
                            disp = (((b0 & 0x0F) << 8) | b1) + 1;
                        }

                        for (int k = 0; k < length && outPtr < decompSize; k++)
                        {
                            int copyIdx = outPtr - disp;
                            outBuf[outPtr++] = (copyIdx >= 0) ? outBuf[copyIdx] : (byte)0;
                        }
                    }
                }
            }
            return outBuf;
        }

        private static byte[] DecompressRLE(byte[] data, int startOffset)
        {
            if (data == null || startOffset + 4 > data.Length) return null;
            int sz = data[startOffset + 1] | (data[startOffset + 2] << 8) | (data[startOffset + 3] << 16);
            int idx = startOffset + 4;
            if (sz == 0 && idx + 4 <= data.Length)
            {
                sz = BitConverter.ToInt32(data, idx);
                idx += 4;
            }
            byte[] outBuf = new byte[sz];
            int outPtr = 0;
            while (outPtr < sz && idx < data.Length)
            {
                byte flag = data[idx++];
                if ((flag & 0x80) == 0)
                {
                    int cnt = (flag & 0x7F) + 1;
                    for (int k = 0; k < cnt && outPtr < sz && idx < data.Length; k++)
                        outBuf[outPtr++] = data[idx++];
                }
                else
                {
                    int cnt = (flag & 0x7F) + 3;
                    if (idx >= data.Length) break;
                    byte val = data[idx++];
                    for (int k = 0; k < cnt && outPtr < sz; k++)
                        outBuf[outPtr++] = val;
                }
            }
            return outBuf;
        }

        private static byte[] DecompressHuffman(byte[] data, int startOffset)
        {
            if (data == null || startOffset + 4 > data.Length) return null;
            int sz = data[startOffset + 1] | (data[startOffset + 2] << 8) | (data[startOffset + 3] << 16);
            int idx = startOffset + 4;
            if (sz == 0 && idx + 4 <= data.Length)
            {
                sz = BitConverter.ToInt32(data, idx);
                idx += 4;
            }
            if (idx >= data.Length) return null;
            int treeSize = data[idx];
            int treeLen = 1 + (treeSize * 2 + 1);
            if (idx + treeLen > data.Length) return null;
            byte[] tree = new byte[treeLen];
            Array.Copy(data, idx, tree, 0, treeLen);
            idx += treeLen;

            byte[] outBuf = new byte[sz];
            int outPtr = 0;
            int r4 = 1;

            while (outPtr < sz && idx < data.Length)
            {
                uint word = 0;
                if (idx + 4 <= data.Length)
                {
                    word = BitConverter.ToUInt32(data, idx);
                    idx += 4;
                }
                else
                {
                    int rem = data.Length - idx;
                    for (int r = 0; r < rem; r++)
                        word |= ((uint)data[idx + r]) << (r * 8);
                    idx = data.Length;
                }

                uint mask = 0x80000000;
                while (mask > 0 && outPtr < sz)
                {
                    bool bit = (word & mask) != 0;
                    mask >>= 1;

                    if (r4 >= tree.Length) { r4 = 1; break; }
                    byte node = tree[r4];
                    int offset = node & 0x3F;
                    bool child0IsLeaf = (node & 0x80) != 0;
                    bool child1IsLeaf = (node & 0x40) != 0;
                    int baseIdx = (r4 & ~1) + (offset << 1);

                    if (!bit)
                    {
                        r4 = baseIdx + 2;
                        if (child0IsLeaf)
                        {
                            if (r4 < tree.Length) outBuf[outPtr++] = tree[r4];
                            r4 = 1;
                        }
                    }
                    else
                    {
                        r4 = baseIdx + 3;
                        if (child1IsLeaf)
                        {
                            if (r4 < tree.Length) outBuf[outPtr++] = tree[r4];
                            r4 = 1;
                        }
                    }
                }
            }
            return outBuf;
        }

        private static byte[] DecompressPayload(byte[] data, int startOffset)
        {
            if (data == null || startOffset >= data.Length) return null;
            byte magic = data[startOffset];
            if (magic == 0x11)
                return DecompressLZ11(data, startOffset);
            if (magic == 0x28 || magic == 0x24)
                return DecompressHuffman(data, startOffset);
            if (magic == 0x30)
                return DecompressRLE(data, startOffset);
            if (magic == 0x00 && startOffset + 4 <= data.Length)
            {
                byte[] uncomp = new byte[data.Length - (startOffset + 4)];
                Array.Copy(data, startOffset + 4, uncomp, 0, uncomp.Length);
                return uncomp;
            }
            byte[] raw = new byte[data.Length - startOffset];
            Array.Copy(data, startOffset, raw, 0, raw.Length);
            return raw;
        }

        public static Bitmap DecodeFile(byte[] fileBytes)
        {
            if (fileBytes == null || fileBytes.Length < 5)
                return null;

            // 1. .snaky video (extract frame 0)
            if (fileBytes.Length > 32 && fileBytes[0] == 'S' && fileBytes[1] == 'N' && fileBytes[2] == 'K' && fileBytes[3] == 'Y')
            {
                int w = BitConverter.ToUInt16(fileBytes, 6);
                int h = BitConverter.ToUInt16(fileBytes, 8);
                byte fmt = fileBytes[11];
                int offset = 32;

                while (offset + 4 < fileBytes.Length)
                {
                    byte chunk_type = fileBytes[offset];
                    if (chunk_type == 0x03)
                    {
                        int sz = fileBytes[offset + 1] | (fileBytes[offset + 2] << 8) | (fileBytes[offset + 3] << 16);
                        offset += 4 + sz;
                    }
                    else if (chunk_type == 0x01 || chunk_type == 0x02)
                    {
                        int comp_size = fileBytes[offset + 1] | (fileBytes[offset + 2] << 8) | (fileBytes[offset + 3] << 16);
                        int uncomp_size = fileBytes[offset + 4] | (fileBytes[offset + 5] << 8) | (fileBytes[offset + 6] << 16);

                        byte[] uncomp_data = new byte[uncomp_size];
                        int read = 0;
                        using (var ms = new MemoryStream(fileBytes, offset + 7 + 2, comp_size - 2))
                        using (var ds = new System.IO.Compression.DeflateStream(ms, System.IO.Compression.CompressionMode.Decompress))
                        {
                            while (read < uncomp_size)
                            {
                                int r = ds.Read(uncomp_data, read, uncomp_size - read);
                                if (r == 0) break;
                                read += r;
                            }
                        }

                        Bitmap bmp = new Bitmap(w, h);
                        int totalPixels = w * h;
                        int pixelIdx = 0;
                        int idx = 0;

                        if (fmt == 1) // RGB444 packed (SnakeEngine official)
                        {
                            while (idx < uncomp_data.Length && pixelIdx < totalPixels)
                            {
                                byte op = uncomp_data[idx++];
                                if (op == 0x00) break;
                                if (idx + 1 >= uncomp_data.Length) break;
                                int cnt = uncomp_data[idx] | (uncomp_data[idx + 1] << 8);
                                idx += 2;

                                if (op == 0x01)
                                {
                                    pixelIdx += cnt;
                                }
                                else if (op == 0x02)
                                {
                                    for (int k = 0; k < cnt; k += 2)
                                    {
                                        if (k + 1 < cnt)
                                        {
                                            if (idx + 2 >= uncomp_data.Length) break;
                                            byte b0 = uncomp_data[idx++];
                                            byte b1 = uncomp_data[idx++];
                                            byte b2 = uncomp_data[idx++];

                                            int r0 = (b0 >> 4) * 17;
                                            int g0 = (b0 & 0x0F) * 17;
                                            int b0_c = (b1 >> 4) * 17;

                                            int r1 = (b1 & 0x0F) * 17;
                                            int g1 = (b2 >> 4) * 17;
                                            int b1_c = (b2 & 0x0F) * 17;

                                            int px0 = pixelIdx++;
                                            int px1 = pixelIdx++;
                                            if (px0 < totalPixels) bmp.SetPixel(px0 % w, px0 / w, Color.FromArgb(r0, g0, b0_c));
                                            if (px1 < totalPixels) bmp.SetPixel(px1 % w, px1 / w, Color.FromArgb(r1, g1, b1_c));
                                        }
                                        else
                                        {
                                            if (idx + 1 >= uncomp_data.Length) break;
                                            byte b0 = uncomp_data[idx++];
                                            byte b1 = uncomp_data[idx++];
                                            int r0 = (b0 >> 4) * 17;
                                            int g0 = (b0 & 0x0F) * 17;
                                            int b0_c = (b1 >> 4) * 17;

                                            int px0 = pixelIdx++;
                                            if (px0 < totalPixels) bmp.SetPixel(px0 % w, px0 / w, Color.FromArgb(r0, g0, b0_c));
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            while (idx < uncomp_data.Length && pixelIdx < totalPixels)
                            {
                                byte op = uncomp_data[idx++];
                                if (op == 0x00) break;
                                if (idx + 1 >= uncomp_data.Length) break;
                                int cnt = uncomp_data[idx] | (uncomp_data[idx + 1] << 8);
                                idx += 2;
                                if (op == 0x01) pixelIdx += cnt;
                                else if (op == 0x02)
                                {
                                    for (int k = 0; k < cnt && idx + 1 < uncomp_data.Length && pixelIdx < totalPixels; k++)
                                    {
                                        ushort color = (ushort)(uncomp_data[idx] | (uncomp_data[idx + 1] << 8));
                                        idx += 2;
                                        int b = (color & 0x1F) * 255 / 31;
                                        int g = ((color >> 5) & 0x3F) * 255 / 63;
                                        int r = ((color >> 11) & 0x1F) * 255 / 31;
                                        bmp.SetPixel(pixelIdx % w, pixelIdx / w, Color.FromArgb(r, g, b));
                                        pixelIdx++;
                                    }
                                }
                            }
                        }
                        return bmp;
                    }
                    else break;
                }
                return null;
            }

            // 2. .rawtex texture
            if (fileBytes.Length >= 12 && fileBytes[0] == 'R' && fileBytes[1] == 'W' && fileBytes[2] == 'T')
            {
                byte magicByte = fileBytes[3];
                int bpp = (magicByte == 'X') ? 4 : 2;

                ushort pw = BitConverter.ToUInt16(fileBytes, 4);
                ushort ph = BitConverter.ToUInt16(fileBytes, 6);
                ushort w = BitConverter.ToUInt16(fileBytes, 8);
                ushort h = BitConverter.ToUInt16(fileBytes, 10);

                byte[] pixelData = new byte[fileBytes.Length - 12];
                Array.Copy(fileBytes, 12, pixelData, 0, pixelData.Length);

                Bitmap bmp = new Bitmap(w, h);
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        int local_idx = MortonUnswizzle(x, y);
                        int tx = x >> 3;
                        int ty = y >> 3;
                        int tile_start = (ty * (pw >> 3) + tx) << 6;
                        int src_idx = (tile_start + local_idx) * bpp;

                        if (src_idx + bpp <= pixelData.Length)
                        {
                            if (magicByte == 'X')
                            {
                                byte a = pixelData[src_idx];
                                byte b = pixelData[src_idx + 1];
                                byte g = pixelData[src_idx + 2];
                                byte r = pixelData[src_idx + 3];
                                bmp.SetPixel(x, y, Color.FromArgb(a, r, g, b));
                            }
                            else if (magicByte == '4')
                            {
                                byte b0 = pixelData[src_idx];
                                byte b1 = pixelData[src_idx + 1];
                                byte a = (byte)((b0 & 0x0F) * 17);
                                byte b = (byte)(((b0 >> 4) & 0x0F) * 17);
                                byte g = (byte)((b1 & 0x0F) * 17);
                                byte r = (byte)(((b1 >> 4) & 0x0F) * 17);
                                bmp.SetPixel(x, y, Color.FromArgb(a, r, g, b));
                            }
                            else if (magicByte == '5')
                            {
                                ushort rgb = (ushort)(pixelData[src_idx] | (pixelData[src_idx + 1] << 8));
                                byte b = (byte)((rgb & 0x1F) * 255 / 31);
                                byte g = (byte)(((rgb >> 5) & 0x3F) * 255 / 63);
                                byte r = (byte)(((rgb >> 11) & 0x1F) * 255 / 31);
                                bmp.SetPixel(x, y, Color.FromArgb(255, r, g, b));
                            }
                        }
                    }
                }
                return bmp;
            }

            // 3. .t3x texture
            if (fileBytes.Length >= 5)
            {
                ushort numSubTextures = BitConverter.ToUInt16(fileBytes, 0);
                byte logDims = fileBytes[2];
                byte format = (byte)(fileBytes[3] & 0x0F);

                int widthLog2 = logDims & 0x07;
                int heightLog2 = (logDims >> 3) & 0x07;
                int pw = 1 << (widthLog2 + 3);
                int ph = 1 << (heightLog2 + 3);

                int w = pw;
                int h = ph;
                bool isRotated = false;
                if (numSubTextures > 0 && fileBytes.Length >= 17)
                {
                    w = BitConverter.ToUInt16(fileBytes, 5);
                    h = BitConverter.ToUInt16(fileBytes, 7);
                    ushort left = BitConverter.ToUInt16(fileBytes, 9);
                    ushort top = BitConverter.ToUInt16(fileBytes, 11);
                    ushort right = BitConverter.ToUInt16(fileBytes, 13);
                    ushort bottom = BitConverter.ToUInt16(fileBytes, 15);
                    isRotated = (top < bottom);
                }

                int subOffset = 5 + numSubTextures * 12;
                byte[] pixelData = DecompressPayload(fileBytes, subOffset);
                if (pixelData != null)
                {
                    Bitmap atlasBmp = new Bitmap(pw, ph);
                    for (int y = 0; y < ph; y++)
                    {
                        for (int x = 0; x < pw; x++)
                        {
                            int local_idx = MortonUnswizzle(x, y);
                            int tx = x >> 3;
                            int ty = y >> 3;
                            int tile_start = (ty * (pw >> 3) + tx) << 6;

                            if (format == 0) // RGBA8
                            {
                                int src_idx = (tile_start + local_idx) * 4;
                                if (src_idx + 4 <= pixelData.Length)
                                {
                                    byte a = pixelData[src_idx];
                                    byte b = pixelData[src_idx + 1];
                                    byte g = pixelData[src_idx + 2];
                                    byte r = pixelData[src_idx + 3];
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(a, r, g, b));
                                }
                            }
                            else if (format == 1) // RGB8
                            {
                                int src_idx = (tile_start + local_idx) * 3;
                                if (src_idx + 3 <= pixelData.Length)
                                {
                                    byte b = pixelData[src_idx];
                                    byte g = pixelData[src_idx + 1];
                                    byte r = pixelData[src_idx + 2];
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(255, r, g, b));
                                }
                            }
                            else if (format == 2) // RGBA5551
                            {
                                int src_idx = (tile_start + local_idx) * 2;
                                if (src_idx + 2 <= pixelData.Length)
                                {
                                    ushort val = (ushort)(pixelData[src_idx] | (pixelData[src_idx + 1] << 8));
                                    int r = ((val >> 11) & 0x1F) * 255 / 31;
                                    int g = ((val >> 6) & 0x1F) * 255 / 31;
                                    int b = ((val >> 1) & 0x1F) * 255 / 31;
                                    int a = (val & 0x01) != 0 ? 255 : 0;
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(a, r, g, b));
                                }
                            }
                            else if (format == 3) // RGB565
                            {
                                int src_idx = (tile_start + local_idx) * 2;
                                if (src_idx + 2 <= pixelData.Length)
                                {
                                    ushort rgb = (ushort)(pixelData[src_idx] | (pixelData[src_idx + 1] << 8));
                                    byte b = (byte)((rgb & 0x1F) * 255 / 31);
                                    byte g = (byte)(((rgb >> 5) & 0x3F) * 255 / 63);
                                    byte r = (byte)(((rgb >> 11) & 0x1F) * 255 / 31);
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(255, r, g, b));
                                }
                            }
                            else if (format == 4) // RGBA4
                            {
                                int src_idx = (tile_start + local_idx) * 2;
                                if (src_idx + 2 <= pixelData.Length)
                                {
                                    byte b0 = pixelData[src_idx];
                                    byte b1 = pixelData[src_idx + 1];
                                    byte a = (byte)((b0 & 0x0F) * 17);
                                    byte b = (byte)(((b0 >> 4) & 0x0F) * 17);
                                    byte g = (byte)((b1 & 0x0F) * 17);
                                    byte r = (byte)(((b1 >> 4) & 0x0F) * 17);
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(a, r, g, b));
                                }
                            }
                            else if (format == 5) // LA8
                            {
                                int src_idx = (tile_start + local_idx) * 2;
                                if (src_idx + 2 <= pixelData.Length)
                                {
                                    byte a = pixelData[src_idx];
                                    byte l = pixelData[src_idx + 1];
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(a, l, l, l));
                                }
                            }
                            else if (format == 7) // L8
                            {
                                int src_idx = tile_start + local_idx;
                                if (src_idx < pixelData.Length)
                                {
                                    byte l = pixelData[src_idx];
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(255, l, l, l));
                                }
                            }
                            else if (format == 8) // A8
                            {
                                int src_idx = tile_start + local_idx;
                                if (src_idx < pixelData.Length)
                                {
                                    byte a = pixelData[src_idx];
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(a, 255, 255, 255));
                                }
                            }
                            else if (format == 9) // LA4
                            {
                                int src_idx = tile_start + local_idx;
                                if (src_idx < pixelData.Length)
                                {
                                    byte b0 = pixelData[src_idx];
                                    byte l = (byte)((b0 >> 4) * 17);
                                    byte a = (byte)((b0 & 0x0F) * 17);
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(a, l, l, l));
                                }
                            }
                            else if (format == 10) // L4
                            {
                                int elem = tile_start + local_idx;
                                int byte_idx = elem >> 1;
                                if (byte_idx < pixelData.Length)
                                {
                                    byte b0 = pixelData[byte_idx];
                                    int val = (elem & 1) == 0 ? (b0 & 0x0F) : (b0 >> 4);
                                    byte l = (byte)(val * 17);
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(255, l, l, l));
                                }
                            }
                            else if (format == 11) // A4
                            {
                                int elem = tile_start + local_idx;
                                int byte_idx = elem >> 1;
                                if (byte_idx < pixelData.Length)
                                {
                                    byte b0 = pixelData[byte_idx];
                                    int val = (elem & 1) == 0 ? (b0 & 0x0F) : (b0 >> 4);
                                    byte a = (byte)(val * 17);
                                    atlasBmp.SetPixel(x, y, Color.FromArgb(a, 255, 255, 255));
                                }
                            }
                        }
                    }

                    Bitmap finalBmp;
                    if (isRotated)
                    {
                        int cropW = Math.Min(h, pw);
                        int cropH = Math.Min(w, ph);
                        Bitmap cropped = atlasBmp.Clone(new Rectangle(0, 0, cropW, cropH), atlasBmp.PixelFormat);
                        cropped.RotateFlip(RotateFlipType.Rotate270FlipNone);
                        finalBmp = cropped;
                        atlasBmp.Dispose();
                    }
                    else
                    {
                        int cropW = Math.Min(w, pw);
                        int cropH = Math.Min(h, ph);
                        finalBmp = atlasBmp.Clone(new Rectangle(0, 0, cropW, cropH), atlasBmp.PixelFormat);
                        atlasBmp.Dispose();
                    }
                    return finalBmp;
                }
            }

            return null;
        }
    }
}
