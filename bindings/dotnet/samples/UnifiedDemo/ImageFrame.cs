using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using Lw.PPOCRSharp;

namespace Lw.PPOCR.Demo;

internal sealed class ImageFrame : IDisposable
{
    private ImageFrame(Bitmap bitmap, byte[] pixels, int stride)
    {
        Bitmap = bitmap;
        Pixels = pixels;
        Stride = stride;
    }

    internal Bitmap Bitmap { get; }
    internal byte[] Pixels { get; }
    internal int Width => Bitmap.Width;
    internal int Height => Bitmap.Height;
    internal int Stride { get; }
    internal OcrPixelFormat PixelFormat => OcrPixelFormat.Bgr24;

    internal static ImageFrame Load(string path)
    {
        using var source = new Bitmap(path);
        var bitmap = new Bitmap(
            source.Width,
            source.Height,
            System.Drawing.Imaging.PixelFormat.Format24bppRgb);
        using (Graphics graphics = Graphics.FromImage(bitmap))
        {
            graphics.Clear(Color.White);
            graphics.DrawImage(
                source,
                new Rectangle(0, 0, bitmap.Width, bitmap.Height),
                new Rectangle(0, 0, source.Width, source.Height),
                GraphicsUnit.Pixel);
        }

        var rectangle = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
        BitmapData data = bitmap.LockBits(
            rectangle,
            ImageLockMode.ReadOnly,
            System.Drawing.Imaging.PixelFormat.Format24bppRgb);
        try
        {
            int stride = Math.Abs(data.Stride);
            byte[] pixels = new byte[checked(stride * bitmap.Height)];
            for (int row = 0; row < bitmap.Height; row++)
            {
                IntPtr sourceRow = IntPtr.Add(data.Scan0, row * data.Stride);
                Marshal.Copy(sourceRow, pixels, row * stride, stride);
            }
            return new ImageFrame(bitmap, pixels, stride);
        }
        catch
        {
            bitmap.Dispose();
            throw;
        }
        finally
        {
            bitmap.UnlockBits(data);
        }
    }

    internal OcrImage Crop(Rectangle region)
    {
        Rectangle clipped = Rectangle.Intersect(
            new Rectangle(0, 0, Width, Height), region);
        if (clipped.Width <= 0 || clipped.Height <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(region),
                "框选区域不在图片范围内。");
        }
        int cropStride = checked(clipped.Width * 3);
        byte[] crop = new byte[checked(cropStride * clipped.Height)];
        for (int row = 0; row < clipped.Height; row++)
        {
            Buffer.BlockCopy(Pixels,
                checked((clipped.Top + row) * Stride + clipped.Left * 3),
                crop, row * cropStride, cropStride);
        }
        return new OcrImage(crop, clipped.Width, clipped.Height,
            cropStride, OcrPixelFormat.Bgr24);
    }

    public void Dispose() => Bitmap.Dispose();
}
