using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace SnakeEngineViewerLauncher
{
    static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            try
            {
                string exeDir = AppDomain.CurrentDomain.BaseDirectory;
                string mainPyPath = Path.GetFullPath(Path.Combine(exeDir, @"..\..\SnakeEngineViewer\main.py"));

                if (!File.Exists(mainPyPath))
                {
                    MessageBox.Show("Could not find main.py at: " + mainPyPath, "SnakeEngine Launcher Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                // We use python.exe directly since we verified it works, but hide the console window.
                ProcessStartInfo psi = new ProcessStartInfo();
                psi.FileName = "python.exe";
                psi.Arguments = string.Format("\"{0}\" {1}", mainPyPath, args.Length > 0 ? string.Format("\"{0}\"", args[0]) : "");
                psi.UseShellExecute = false;
                psi.CreateNoWindow = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;

                Process.Start(psi);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to launch viewer: " + ex.Message, "SnakeEngine Launcher Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
