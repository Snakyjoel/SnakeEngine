using System;
using System.Drawing;

namespace TestShellExt
{
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length == 0) return;
            try
            {
                byte[] fileBytes = System.IO.File.ReadAllBytes(args[0]);
                using (Bitmap bmp = SnakeEngineShellExt.RawtexThumbnailProvider.DecodeFile(fileBytes))
                {
                    if (bmp != null)
                    {
                        Console.WriteLine(string.Format("SUCCESS: Decoded {0}x{1}", bmp.Width, bmp.Height));
                    }
                    else
                    {
                        Console.WriteLine("FAILED: DecodeFile returned null.");
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("EXCEPTION: " + ex.ToString());
            }
        }
    }
}
