namespace Lw.PPOCR.Demo;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
#if NETFRAMEWORK
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
#else
        ApplicationConfiguration.Initialize();
#endif
        string? packagedBackend = ReadPackagedBackend();
        if ((args.Length == 2 || args.Length == 3) &&
            (args[0] == "--screenshot" || args[0] == "--smoke-screenshot"))
        {
            using var form = new MainForm(packagedBackend);
            form.Show();
            Application.DoEvents();
            if (args[0] == "--smoke-screenshot")
            {
                string backend = args.Length == 3 ? args[2] : "opencv";
                Task smokeTest = form.RunExperienceSmokeAsync(backend);
                while (!smokeTest.IsCompleted)
                {
                    Application.DoEvents();
                    Thread.Sleep(10);
                }
                smokeTest.GetAwaiter().GetResult();
            }
            using var bitmap = new Bitmap(form.Bounds.Width, form.Bounds.Height);
            form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, bitmap.Size));
            bitmap.Save(args[1], System.Drawing.Imaging.ImageFormat.Png);
            return;
        }

        string? requestedBackend = args.Length == 2 && args[0] == "--backend"
            ? args[1]
            : null;
        Application.Run(new MainForm(requestedBackend ?? packagedBackend));
    }

    private static string? ReadPackagedBackend()
    {
        string path = Path.Combine(AppContext.BaseDirectory, "demo-backend.txt");
        if (!File.Exists(path))
        {
            return null;
        }

        string backend = File.ReadAllText(path).Trim();
        return string.IsNullOrWhiteSpace(backend) ? null : backend;
    }
}
