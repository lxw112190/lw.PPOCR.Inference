using System.Drawing.Drawing2D;
using Lw.PPOCRSharp;

namespace Lw.PPOCR.Demo;

internal sealed class OcrImageView : Control
{
    private Bitmap? image;
    private IReadOnlyList<OcrTextRegion> regions = Array.Empty<OcrTextRegion>();

    internal OcrImageView()
    {
        DoubleBuffered = true;
        BackColor = Color.FromArgb(36, 42, 48);
        Dock = DockStyle.Fill;
        SetStyle(ControlStyles.ResizeRedraw, true);
    }

    internal void SetContent(Bitmap? bitmap, IReadOnlyList<OcrTextRegion>? value)
    {
        image = bitmap;
        regions = value ?? Array.Empty<OcrTextRegion>();
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        if (image is null)
        {
            const string message = "选择或拖入一张图片开始体验 OCR";
            TextRenderer.DrawText(
                e.Graphics, message, Font, ClientRectangle,
                Color.FromArgb(194, 201, 207),
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            return;
        }

        Rectangle imageBounds = Rectangle.Inflate(ClientRectangle, -12, -12);
        RectangleF target = FitRectangle(image.Size, imageBounds);
        e.Graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
        e.Graphics.DrawImage(
            image,
            target,
            new RectangleF(0, 0, image.Width, image.Height),
            GraphicsUnit.Pixel);
        if (regions.Count == 0)
        {
            return;
        }

        float scaleX = target.Width / image.Width;
        float scaleY = target.Height / image.Height;
        using var pen = new Pen(Color.FromArgb(32, 211, 142), Math.Max(1.5f, 2f * scaleX));
        using var fill = new SolidBrush(Color.FromArgb(220, 20, 28, 32));
        using var textBrush = new SolidBrush(Color.White);
        using var labelFont = new Font(Font.FontFamily, 8.5f, FontStyle.Bold);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

        for (int index = 0; index < regions.Count; index++)
        {
            PointF[] points = regions[index].Box
                .Select(point => new PointF(
                    target.X + point.X * scaleX,
                    target.Y + point.Y * scaleY))
                .ToArray();
            if (points.Length < 3)
            {
                continue;
            }
            e.Graphics.DrawPolygon(pen, points);

            string label = (index + 1).ToString();
            SizeF size = e.Graphics.MeasureString(label, labelFont);
            var badge = new RectangleF(
                points[0].X, Math.Max(target.Top, points[0].Y - size.Height - 3),
                size.Width + 7, size.Height + 2);
            e.Graphics.FillRectangle(fill, badge);
            e.Graphics.DrawString(label, labelFont, textBrush, badge.X + 3, badge.Y + 1);
        }
    }

    private static RectangleF FitRectangle(Size imageSize, Rectangle bounds)
    {
        if (bounds.Width <= 0 || bounds.Height <= 0)
        {
            return RectangleF.Empty;
        }
        float scale = Math.Min(
            (float)bounds.Width / imageSize.Width,
            (float)bounds.Height / imageSize.Height);
        float width = imageSize.Width * scale;
        float height = imageSize.Height * scale;
        return new RectangleF(
            bounds.Left + (bounds.Width - width) / 2f,
            bounds.Top + (bounds.Height - height) / 2f,
            width,
            height);
    }
}
