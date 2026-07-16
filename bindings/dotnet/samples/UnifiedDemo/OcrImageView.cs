using System.Drawing.Drawing2D;
using Lw.PPOCRSharp;

namespace Lw.PPOCR.Demo;

internal sealed class ImageRegionSelectedEventArgs : EventArgs
{
    internal ImageRegionSelectedEventArgs(Rectangle region) => Region = region;
    internal Rectangle Region { get; }
}

internal sealed class OcrImageView : Control
{
    private Bitmap? image;
    private IReadOnlyList<OcrTextRegion> regions = Array.Empty<OcrTextRegion>();
    private bool dragging;
    private Point dragStart;
    private Point dragEnd;
    private Rectangle? selectedImageRegion;

    internal OcrImageView()
    {
        DoubleBuffered = true;
        BackColor = Color.FromArgb(36, 42, 48);
        Dock = DockStyle.Fill;
        SetStyle(ControlStyles.ResizeRedraw, true);
    }

    internal event EventHandler<ImageRegionSelectedEventArgs>? RegionSelected;

    internal bool SelectionEnabled { get; set; } = true;

    internal void SetContent(Bitmap? bitmap, IReadOnlyList<OcrTextRegion>? value)
    {
        image = bitmap;
        regions = value ?? Array.Empty<OcrTextRegion>();
        selectedImageRegion = null;
        dragging = false;
        Invalidate();
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (!SelectionEnabled || image is null || e.Button != MouseButtons.Left)
        {
            return;
        }
        RectangleF target = GetImageTarget();
        if (!target.Contains(e.Location))
        {
            return;
        }
        dragging = true;
        dragStart = ClampPoint(e.Location, target);
        dragEnd = dragStart;
        Capture = true;
        Invalidate();
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (image is null)
        {
            Cursor = Cursors.Default;
            return;
        }
        RectangleF target = GetImageTarget();
        Cursor = dragging || target.Contains(e.Location) ? Cursors.Cross : Cursors.Default;
        if (!dragging)
        {
            return;
        }
        dragEnd = ClampPoint(e.Location, target);
        Invalidate();
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        if (!dragging || image is null)
        {
            return;
        }
        dragging = false;
        Capture = false;
        RectangleF target = GetImageTarget();
        dragEnd = ClampPoint(e.Location, target);
        Rectangle selection = NormalizedRectangle(dragStart, dragEnd);
        if (selection.Width < 4 || selection.Height < 4)
        {
            Invalidate();
            return;
        }

        float scaleX = image.Width / target.Width;
        float scaleY = image.Height / target.Height;
        int left = Clamp((int)Math.Floor((selection.Left - target.Left) * scaleX), 0, image.Width - 1);
        int top = Clamp((int)Math.Floor((selection.Top - target.Top) * scaleY), 0, image.Height - 1);
        int right = Clamp((int)Math.Ceiling((selection.Right - target.Left) * scaleX), left + 1, image.Width);
        int bottom = Clamp((int)Math.Ceiling((selection.Bottom - target.Top) * scaleY), top + 1, image.Height);
        selectedImageRegion = Rectangle.FromLTRB(left, top, right, bottom);
        Invalidate();
        RegionSelected?.Invoke(this,
            new ImageRegionSelectedEventArgs(selectedImageRegion.Value));
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        if (image is null)
        {
            TextRenderer.DrawText(e.Graphics,
                "选择或拖入一张图片开始体验 OCR", Font, ClientRectangle,
                Color.FromArgb(194, 201, 207),
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            return;
        }

        RectangleF target = GetImageTarget();
        e.Graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
        e.Graphics.DrawImage(image, target,
            new RectangleF(0, 0, image.Width, image.Height), GraphicsUnit.Pixel);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        PaintOcrRegions(e.Graphics, target);
        PaintSelection(e.Graphics, target);
    }

    private void PaintOcrRegions(Graphics graphics, RectangleF target)
    {
        if (image is null || regions.Count == 0)
        {
            return;
        }
        float scaleX = target.Width / image.Width;
        float scaleY = target.Height / image.Height;
        using var pen = new Pen(Color.FromArgb(32, 211, 142), Math.Max(1.5f, 2f * scaleX));
        using var fill = new SolidBrush(Color.FromArgb(220, 20, 28, 32));
        using var textBrush = new SolidBrush(Color.White);
        using var labelFont = new Font(Font.FontFamily, 8.5f, FontStyle.Bold);
        for (int index = 0; index < regions.Count; index++)
        {
            PointF[] points = regions[index].Box.Select(point => new PointF(
                target.X + point.X * scaleX,
                target.Y + point.Y * scaleY)).ToArray();
            if (points.Length < 3) continue;
            graphics.DrawPolygon(pen, points);
            string label = (index + 1).ToString();
            SizeF size = graphics.MeasureString(label, labelFont);
            var badge = new RectangleF(points[0].X,
                Math.Max(target.Top, points[0].Y - size.Height - 3),
                size.Width + 7, size.Height + 2);
            graphics.FillRectangle(fill, badge);
            graphics.DrawString(label, labelFont, textBrush, badge.X + 3, badge.Y + 1);
        }
    }

    private void PaintSelection(Graphics graphics, RectangleF target)
    {
        RectangleF selection;
        if (dragging)
        {
            selection = NormalizedRectangle(dragStart, dragEnd);
        }
        else if (selectedImageRegion.HasValue && image is not null)
        {
            Rectangle region = selectedImageRegion.Value;
            selection = new RectangleF(
                target.Left + region.Left * target.Width / image.Width,
                target.Top + region.Top * target.Height / image.Height,
                region.Width * target.Width / image.Width,
                region.Height * target.Height / image.Height);
        }
        else
        {
            return;
        }
        using var fill = new SolidBrush(Color.FromArgb(45, 255, 165, 35));
        using var pen = new Pen(Color.FromArgb(255, 177, 55), 2f)
            { DashStyle = DashStyle.Dash };
        graphics.FillRectangle(fill, selection);
        graphics.DrawRectangle(pen, selection.X, selection.Y,
            selection.Width, selection.Height);
    }

    private RectangleF GetImageTarget()
    {
        if (image is null) return RectangleF.Empty;
        return FitRectangle(image.Size, Rectangle.Inflate(ClientRectangle, -12, -12));
    }

    private static Point ClampPoint(Point point, RectangleF bounds) => new(
        (int)Math.Max(bounds.Left, Math.Min(bounds.Right, point.X)),
        (int)Math.Max(bounds.Top, Math.Min(bounds.Bottom, point.Y)));

    private static int Clamp(int value, int minimum, int maximum) =>
        Math.Max(minimum, Math.Min(maximum, value));

    private static Rectangle NormalizedRectangle(Point first, Point second) =>
        Rectangle.FromLTRB(Math.Min(first.X, second.X), Math.Min(first.Y, second.Y),
            Math.Max(first.X, second.X), Math.Max(first.Y, second.Y));

    private static RectangleF FitRectangle(Size imageSize, Rectangle bounds)
    {
        if (bounds.Width <= 0 || bounds.Height <= 0) return RectangleF.Empty;
        float scale = Math.Min((float)bounds.Width / imageSize.Width,
            (float)bounds.Height / imageSize.Height);
        float width = imageSize.Width * scale;
        float height = imageSize.Height * scale;
        return new RectangleF(bounds.Left + (bounds.Width - width) / 2f,
            bounds.Top + (bounds.Height - height) / 2f, width, height);
    }
}
