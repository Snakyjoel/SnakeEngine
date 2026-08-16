using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.IO;

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
    [Guid("b824b49d-22ac-4161-ac8a-9916e8fa3f7f")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IInitializeWithStream
    {
        void Initialize(System.Runtime.InteropServices.ComTypes.IStream pstream, uint grfMode);
    }

    [ComVisible(true)]
    [Guid("A7F013BD-DE2B-4A1C-9E5C-FCE0CC89B663")]
    [ClassInterface(ClassInterfaceType.None)]
    [ProgId("SnakeEngine.ThumbnailProvider")]
    public class RawtexThumbnailProvider : IThumbnailProvider, IInitializeWithStream
    {
        private byte[] _fileBytes;

        public void Initialize(System.Runtime.InteropServices.ComTypes.IStream pstream, uint grfMode)
        {
            using (var ms = new MemoryStream())
            {
                byte[] buffer = new byte[8192];
                IntPtr pcbRead = Marshal.AllocCoTaskMem(Marshal.SizeOf(typeof(int)));
                try
                {
                    while (true)
                    {
                        pstream.Read(buffer, buffer.Length, pcbRead);
                        int bytesRead = Marshal.ReadInt32(pcbRead);
                        if (bytesRead == 0) break;
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

        public void GetThumbnail(uint cx, out IntPtr phbmp, out uint pdwAlpha)
        {
            phbmp = IntPtr.Zero;
            pdwAlpha = 2; // WTSAT_ARGB

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
                // Fail silently
            }
        }

        private static int MortonUnswizzle(int x, int y)
        {
            int i = (x & 7) | ((y & 7) << 8);
            i = (i ^ (i << 2)) & 0x1313;
            i = (i ^ (i << 1)) & 0x1515;
            return (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
        }

        public static Bitmap DecodeFile(byte[] fileBytes)
        {
            if (fileBytes.Length < 5)
                return null;

            // Check for .snaky magic
            if (fileBytes.Length > 32 && fileBytes[0] == 'S' && fileBytes[1] == 'N' && fileBytes[2] == 'K' && fileBytes[3] == 'Y')
            {
                int w = BitConverter.ToUInt16(fileBytes, 6);
                int h = BitConverter.ToUInt16(fileBytes, 8);
                int offset = 32;
                
                while (offset < fileBytes.Length)
                {
                    byte chunk_type = fileBytes[offset];
                    if (chunk_type == 0x01)
                    {
                        int comp_size = fileBytes[offset+1] | (fileBytes[offset+2] << 8) | (fileBytes[offset+3] << 16);
                        int uncomp_size = fileBytes[offset+4] | (fileBytes[offset+5] << 8) | (fileBytes[offset+6] << 16);
                        
                        byte[] uncomp_data = new byte[uncomp_size];
                        int read = 0;
                        using (var ms = new MemoryStream(fileBytes, offset + 7 + 2, comp_size - 2)) // Skip 2 byte zlib header
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
                        int[] frame_buffer = new int[w * h];
                        int idx = 0;
                        int op_idx = 0;
                        Console.WriteLine(string.Format("w={0} h={1} uncomp_size={2} read={3}", w, h, uncomp_size, read));
                        try {
                            while (op_idx < uncomp_data.Length && idx < frame_buffer.Length)
                            {
                                byte op = uncomp_data[op_idx++];
                                if ((op & 0x80) != 0)
                                {
                                    int len = (op & 0x7F) + 1;
                                    ushort color = (ushort)(uncomp_data[op_idx] | (uncomp_data[op_idx+1] << 8));
                                    op_idx += 2;
                                    for (int i = 0; i < len; i++) {
                                        if (idx >= frame_buffer.Length) break;
                                        frame_buffer[idx++] = color;
                                    }
                                }
                                else
                                {
                                    int len = (op & 0x7F) + 1;
                                    for (int i = 0; i < len; i++)
                                    {
                                        ushort color = (ushort)(uncomp_data[op_idx] | (uncomp_data[op_idx+1] << 8));
                                        op_idx += 2;
                                        if (idx >= frame_buffer.Length) break;
                                        frame_buffer[idx++] = color;
                                    }
                                }
                            }
                        } catch (Exception) {
                            // ignore decode trailing errors
                        }
                        
                        for (int y = 0; y < h; y++)
                        {
                            for (int x = 0; x < w; x++)
                            {
                                int color = frame_buffer[y * w + x];
                                int b = (color & 0x1F) * 255 / 31;
                                int g = ((color >> 5) & 0x3F) * 255 / 63;
                                int r = ((color >> 11) & 0x1F) * 255 / 31;
                                bmp.SetPixel(x, y, Color.FromArgb(255, r, g, b));
                            }
                        }
                        return bmp;
                    }
                    else if (chunk_type == 0x03)
                    {
                        int size = fileBytes[offset+1] | (fileBytes[offset+2] << 8) | (fileBytes[offset+3] << 16);
                        offset += 4 + size;
                    }
                    else
                    {
                        break;
                    }
                }
                return null;
            }

            // Check for .rawtex magic
            if (fileBytes.Length >= 12 &&
                fileBytes[0] == 'R' && fileBytes[1] == 'W' && fileBytes[2] == 'T' && 
                (fileBytes[3] == 'X' || fileBytes[3] == '4' || fileBytes[3] == '5'))
            {
                byte magicByte = fileBytes[3]; // 'X' for RGBA8, '4' for RGBA4, '5' for RGB565
                int rawBpp = (magicByte == 'X') ? 4 : 2;

                ushort pw = BitConverter.ToUInt16(fileBytes, 4);
                ushort ph = BitConverter.ToUInt16(fileBytes, 6);
                ushort w = BitConverter.ToUInt16(fileBytes, 8);
                ushort h = BitConverter.ToUInt16(fileBytes, 10);

                byte[] pixelData = new byte[fileBytes.Length - 12];
                Array.Copy(fileBytes, 12, pixelData, 0, Math.Min(pixelData.Length, fileBytes.Length - 12));

                Bitmap bmp = new Bitmap(w, h);
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        int local_idx = MortonUnswizzle(x, y);
                        int tx = x >> 3;
                        int ty = y >> 3;
                        int tile_start = (ty * (pw >> 3) + tx) << 6;
                        int src_idx = (tile_start + local_idx) * rawBpp;

                        if (src_idx + (rawBpp - 1) < pixelData.Length)
                        {
                            byte r = 0, g = 0, b = 0, a = 255;
                            if (magicByte == 'X')
                            {
                                a = pixelData[src_idx];
                                b = pixelData[src_idx + 1];
                                g = pixelData[src_idx + 2];
                                r = pixelData[src_idx + 3];
                            }
                            else if (magicByte == '4') // RGBA4
                            {
                                byte byte0 = pixelData[src_idx];
                                byte byte1 = pixelData[src_idx + 1];
                                a = (byte)((byte0 & 0x0F) * 17);
                                b = (byte)(((byte0 >> 4) & 0x0F) * 17);
                                g = (byte)((byte1 & 0x0F) * 17);
                                r = (byte)(((byte1 >> 4) & 0x0F) * 17);
                            }
                            else if (magicByte == '5') // RGB565
                            {
                                byte byte0 = pixelData[src_idx];
                                byte byte1 = pixelData[src_idx + 1];
                                // Little-endian read
                                ushort rgb = (ushort)(byte0 | (byte1 << 8));
                                // In memory for 3ds, RGB565 is usually stored such that byte0 and byte1 hold the 565 layout.
                                // Actually, for citro3d, B is lowest 5 bits, G is middle 6 bits, R is top 5 bits.
                                b = (byte)((rgb & 0x1F) * 255 / 31);
                                g = (byte)(((rgb >> 5) & 0x3F) * 255 / 63);
                                r = (byte)(((rgb >> 11) & 0x1F) * 255 / 31);
                                a = 255;
                            }
                            bmp.SetPixel(x, y, Color.FromArgb(a, r, g, b));
                        }
                    }
                }
                return bmp;
            }

            // Check for .t3x format
            // Header is 5 bytes:
            // numSubTextures (u16), log_dims (u8), format (u8), mipmapLevels (u8)
            ushort numSubTextures = BitConverter.ToUInt16(fileBytes, 0);
            byte logDims = fileBytes[2];
            byte format = fileBytes[3];

            int heightLog2 = logDims & 0x07;
            int widthLog2 = (logDims >> 3) & 0x07;
            int pwT3x = 1 << (widthLog2 + 3);
            int phT3x = 1 << (heightLog2 + 3);

            int pixelOffset = 5 + numSubTextures * 20;
            if (pixelOffset > fileBytes.Length)
                return null;

            int wT3x = pwT3x;
            int hT3x = phT3x;

            if (numSubTextures > 0)
            {
                wT3x = BitConverter.ToUInt16(fileBytes, 5);
                hT3x = BitConverter.ToUInt16(fileBytes, 7);
            }

            byte[] t3xPixelData = new byte[fileBytes.Length - pixelOffset];
            Array.Copy(fileBytes, pixelOffset, t3xPixelData, 0, t3xPixelData.Length);

            Bitmap bmpT3x = new Bitmap(wT3x, hT3x);

            int bpp = 4;
            if (format == 1) bpp = 3; // RGB8
            else if (format == 4) bpp = 2; // RGBA4

            for (int y = 0; y < hT3x; y++)
            {
                for (int x = 0; x < wT3x; x++)
                {
                    int local_idx = MortonUnswizzle(x, y);
                    int tx = x >> 3;
                    int ty = y >> 3;
                    int tile_start = (ty * (pwT3x >> 3) + tx) << 6;
                    int src_idx = (tile_start + local_idx) * bpp;

                    if (src_idx + (bpp - 1) < t3xPixelData.Length)
                    {
                        byte r = 0, g = 0, b = 0, a = 255;
                        if (format == 0) // RGBA8 (ABGR in memory)
                        {
                            a = t3xPixelData[src_idx];
                            b = t3xPixelData[src_idx + 1];
                            g = t3xPixelData[src_idx + 2];
                            r = t3xPixelData[src_idx + 3];
                        }
                        else if (format == 1) // RGB8 (BGR in memory)
                        {
                            b = t3xPixelData[src_idx];
                            g = t3xPixelData[src_idx + 1];
                            r = t3xPixelData[src_idx + 2];
                            a = 255;
                        }
                        else if (format == 4) // RGBA4
                        {
                            byte byte0 = t3xPixelData[src_idx];
                            byte byte1 = t3xPixelData[src_idx + 1];
                            a = (byte)( (byte0 & 0x0F) * 17 );
                            b = (byte)( ((byte0 >> 4) & 0x0F) * 17 );
                            g = (byte)( (byte1 & 0x0F) * 17 );
                            r = (byte)( ((byte1 >> 4) & 0x0F) * 17 );
                        }
                        bmpT3x.SetPixel(x, y, Color.FromArgb(a, r, g, b));
                    }
                }
            }

            return bmpT3x;
        }
    }
}
