using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Windows.Forms;

[assembly: AssemblyProduct("AMPEDUP!")]
[assembly: AssemblyDescription("AMPEDUP! for Windows")]
[assembly: AssemblyVersion("0.7.0.0")]
[assembly: AssemblyFileVersion("0.7.0.0")]

internal static class Bootstrap
{
    private const string SetupResource = "AMPEDUP.Setup.zip";

    private static string QuotePath(string path)
    {
        string trimmed = path.TrimEnd('\\');
        return "\"" + trimmed + new string('\\', 2 * (path.Length - trimmed.Length)) + "\"";
    }

    [STAThread]
    private static int Main(string[] args)
    {
        string temporary = null;
        try
        {
            string root = AppDomain.CurrentDomain.BaseDirectory;
            bool setup;
            using (Stream bundle = Assembly.GetExecutingAssembly().GetManifestResourceStream(SetupResource))
            {
                setup = bundle != null;
                if (setup)
                {
                    temporary = Path.Combine(Path.GetTempPath(), "AMPEDUP-" + Guid.NewGuid().ToString("N"));
                    Directory.CreateDirectory(temporary);
                    using (var archive = new ZipArchive(bundle, ZipArchiveMode.Read))
                        ZipFileExtensions.ExtractToDirectory(archive, temporary);
                    root = temporary;
                }
            }
            bool managed = !setup && File.Exists(Path.Combine(root, "recomplauncher.json"));
            bool prepare = !setup && args.Length > 0 && args[0] == "--prepare";
            bool play = !setup && args.Length == 1 && args[0] == "--play";
            if (prepare && (!managed || args.Length != 3 || args[1] != "--game"))
                throw new ArgumentException("Use the Windows ZIP and pass --prepare --game <path>.");
            string scriptName = setup ? "setup-wizard.ps1" : prepare ? "prepare-for-launcher.ps1"
                : play ? "launch.ps1" : "launcher-wizard.ps1";
            string script = Path.Combine(root, scriptName);
            if (!File.Exists(script)) throw new FileNotFoundException("The launcher files are incomplete.", script);
            string ps = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), @"WindowsPowerShell\v1.0\powershell.exe");
            var start = new ProcessStartInfo(ps, "-NoProfile -ExecutionPolicy Bypass -STA -File \"" + script + "\"");
            start.WorkingDirectory = root;
            start.UseShellExecute = false;
            start.CreateNoWindow = true;
            // PowerShell 7 may pass a module path that omits Windows PowerShell
            // 5.1's built-in cmdlets. The packaged scripts intentionally use 5.1.
            string windowsModules = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), @"WindowsPowerShell\v1.0\Modules");
            string sharedModules = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), @"WindowsPowerShell\Modules");
            start.EnvironmentVariables["PSModulePath"] = windowsModules + ";" + sharedModules;
            if (prepare)
                start.Arguments += " -Game " + QuotePath(Path.GetFullPath(args[2]));
            else if (managed)
                start.Arguments += " -Managed";
            if (play) start.Arguments += " -Headless";
            if (prepare || play)
            {
                start.RedirectStandardOutput = true;
                start.RedirectStandardError = true;
            }
            if (setup)
            {
                start.RedirectStandardError = true;
                for (int i = 0; i < args.Length; ++i)
                {
                    string option = args[i];
                    if (option == "-SelfTestNoShortcut") start.Arguments += " " + option;
                    else if (option == "-SelfTestImage" || option == "-SelfTestDestination" || option == "-SelfTestDesktop")
                    {
                        if (++i >= args.Length || args[i].IndexOf('"') >= 0 || args[i].EndsWith("\\"))
                            throw new ArgumentException("Expected a full path for " + option);
                        start.Arguments += " " + option + " \"" + args[i] + "\"";
                    }
                    else throw new ArgumentException("Unknown setup argument: " + option);
                }
            }
            using (Process process = Process.Start(start))
            {
                if (prepare || play)
                {
                    process.OutputDataReceived += (sender, line) => { if (line.Data != null) Console.WriteLine(line.Data); };
                    process.ErrorDataReceived += (sender, line) => { if (line.Data != null) Console.Error.WriteLine(line.Data); };
                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();
                    process.WaitForExit();
                    return process.ExitCode;
                }
                if (setup)
                {
                    string error = process.StandardError.ReadToEnd();
                    process.WaitForExit();
                    if (process.ExitCode != 0)
                    {
                        if (String.IsNullOrWhiteSpace(error)) error = "Setup stopped unexpectedly (exit code " + process.ExitCode + ").";
                        if (error.Length > 1800) error = error.Substring(0, 1800);
                        if (args.Length == 0)
                            MessageBox.Show(error, "AMPEDUP! setup failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    return process.ExitCode;
                }
            }
            return 0;
        }
        catch (Exception ex)
        {
            if (args.Length == 0)
                MessageBox.Show(ex.Message, "AMPEDUP!", MessageBoxButtons.OK, MessageBoxIcon.Error);
            else
                Console.Error.WriteLine(ex.Message);
            return 1;
        }
        finally
        {
            if (temporary != null)
            {
                try { Directory.Delete(temporary, true); }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
        }
    }
}
