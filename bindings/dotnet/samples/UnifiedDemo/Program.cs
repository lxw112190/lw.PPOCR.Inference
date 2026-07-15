namespace Lw.PPOCR.Demo;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        if ((args.Length == 2 || args.Length == 3) &&
            (args[0] == "--screenshot" || args[0] == "--smoke-screenshot"))
        {
            using var form = new MainForm();
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
        Application.Run(new MainForm());
    }
}
