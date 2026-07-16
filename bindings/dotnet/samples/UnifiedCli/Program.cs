using Lw.PPOCRSharp;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

Console.OutputEncoding = Encoding.UTF8;
if (args.Length < 3 || args.Length > 8)
{
    PrintUsage();
    return 2;
}

try
{
    OcrBackend backend = ParseBackend(args[0]);
    string modelManifest = Path.GetFullPath(args[1]);
    string imagePath = Path.GetFullPath(args[2]);
    string runtimeRoot = args.Length >= 4
        ? Path.GetFullPath(args[3])
        : Path.Combine(AppContext.BaseDirectory, "runtimes", "win-x64");
    int deviceId = args.Length >= 5 ? int.Parse(args[4]) : 0;
    bool enableClassifier = args.Length >= 6 && ParseBoolean(args[5]);
    int warmupRuns = args.Length >= 7 ? int.Parse(args[6]) : 0;
    int measuredRuns = args.Length >= 8 ? int.Parse(args[7]) : 1;
    if (warmupRuns < 0 || measuredRuns < 1)
    {
        throw new ArgumentOutOfRangeException(
            nameof(measuredRuns), "Warmup must be >= 0 and runs must be >= 1.");
    }

    var options = CreateOptions(
        backend, modelManifest, runtimeRoot, deviceId, enableClassifier);
    using var engine = new OcrEngine(options);
    DecodedImage image = DecodeImage(imagePath);
    for (int index = 0; index < warmupRuns; ++index)
    {
        _ = Run(engine, image);
    }

    var results = new List<OcrResult>(measuredRuns);
    var wallSamples = new List<double>(measuredRuns);
    for (int index = 0; index < measuredRuns; ++index)
    {
        long startedAt = Stopwatch.GetTimestamp();
        results.Add(Run(engine, image));
        wallSamples.Add(Stopwatch.GetElapsedTime(startedAt).TotalMilliseconds);
    }
    OcrResult result = results[^1];

    OcrCapabilities capabilities = engine.Capabilities;
    Console.WriteLine($"Backend: {capabilities.BackendName}");
    Console.WriteLine($"Device: {(capabilities.SupportsGpu ? $"GPU {deviceId}" : "CPU")}");
    Console.WriteLine($"Regions: {result.Regions.Count}");
    for (int index = 0; index < result.Regions.Count; ++index)
    {
        OcrTextRegion region = result.Regions[index];
        Console.WriteLine($"[{index}] {region.Text} ({region.Score:F4})");
    }
    PrintTiming("det", result.Detector);
    if (enableClassifier)
    {
        PrintTiming("cls", result.Classifier);
    }
    PrintTiming("rec", result.Recognizer);
    PrintTiming("total", result.Pipeline);
    if (warmupRuns > 0 || measuredRuns > 1)
    {
        PrintBenchmark(results, wallSamples, warmupRuns);
    }
    return 0;
}
catch (Exception exception)
{
    Console.Error.WriteLine(exception.Message);
    return 1;
}

static OcrResult Run(OcrEngine engine, DecodedImage image)
{
    return engine.Run(
        image.Pixels, image.Width, image.Height, image.Stride,
        OcrPixelFormat.Bgr24);
}

static OcrOptions CreateOptions(
    OcrBackend backend,
    string modelManifest,
    string runtimeRoot,
    int deviceId,
    bool enableClassifier)
{
    return new OcrOptions
    {
        Backend = backend,
        DeviceId = deviceId,
        RuntimeRoot = runtimeRoot,
        ModelManifest = modelManifest,
        BackendOptionsJson = backend switch
        {
            OcrBackend.OpenVINO => "{\"device\":\"CPU\"}",
            OcrBackend.OnnxRuntime => "{\"device\":\"cpu\"}",
            _ => null
        },
        EnableClassifier = enableClassifier,
        RecognitionBatchSize = backend == OcrBackend.OpenVINO ? 1
            : (backend == OcrBackend.DirectML || backend == OcrBackend.OnnxRuntime ||
               backend == OcrBackend.TensorRT ? 4 : 1),
        RecognitionConcurrency = backend switch
        {
            OcrBackend.DirectML => 4,
            OcrBackend.OnnxRuntime => 4,
            OcrBackend.OpenVINO => 8,
            OcrBackend.TensorRT => 4,
            _ => 2
        }
    };
}

static OcrBackend ParseBackend(string value)
{
    return value.ToLowerInvariant() switch
    {
        "opencv" or "opencv-dnn" => OcrBackend.OpenCvDnn,
        "directml" or "dml" => OcrBackend.DirectML,
        "openvino" => OcrBackend.OpenVINO,
        "tensorrt" or "trt" => OcrBackend.TensorRT,
        "onnxruntime" or "ort" => OcrBackend.OnnxRuntime,
        _ => throw new ArgumentException("Backend must be opencv, directml, openvino, tensorrt, or onnxruntime.")
    };
}

static bool ParseBoolean(string value)
{
    return value.Equals("1", StringComparison.OrdinalIgnoreCase) ||
        value.Equals("true", StringComparison.OrdinalIgnoreCase) ||
        value.Equals("cls", StringComparison.OrdinalIgnoreCase);
}

static DecodedImage DecodeImage(string path)
{
    using var source = new Bitmap(path);
    using var bitmap = new Bitmap(
        source.Width, source.Height, PixelFormat.Format24bppRgb);
    using (Graphics graphics = Graphics.FromImage(bitmap))
    {
        graphics.DrawImageUnscaled(source, 0, 0);
    }

    var rectangle = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
    BitmapData data = bitmap.LockBits(
        rectangle, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
    try
    {
        int stride = Math.Abs(data.Stride);
        byte[] pixels = new byte[checked(stride * bitmap.Height)];
        for (int row = 0; row < bitmap.Height; ++row)
        {
            int sourceRow = data.Stride >= 0 ? row : bitmap.Height - 1 - row;
            IntPtr rowPointer = IntPtr.Add(data.Scan0, sourceRow * data.Stride);
            Marshal.Copy(rowPointer, pixels, row * stride, stride);
        }
        return new DecodedImage(pixels, bitmap.Width, bitmap.Height, stride);
    }
    finally
    {
        bitmap.UnlockBits(data);
    }
}

static void PrintTiming(string name, OcrTiming timing)
{
    Console.WriteLine(
        $"{name}: preprocess={timing.PreprocessMilliseconds:F3} ms, " +
        $"inference={timing.InferenceMilliseconds:F3} ms, " +
        $"postprocess={timing.PostprocessMilliseconds:F3} ms, " +
        $"total={timing.TotalMilliseconds:F3} ms");
}

static void PrintBenchmark(
    IReadOnlyList<OcrResult> results,
    IReadOnlyList<double> wallSamples,
    int warmupRuns)
{
    Console.WriteLine($"Benchmark: warmup={warmupRuns}, runs={results.Count}");
    PrintStatistics("det-total", results.Select(result =>
        result.Detector.TotalMilliseconds));
    PrintStatistics("cls-total", results.Select(result =>
        result.Classifier.TotalMilliseconds));
    PrintStatistics("rec-total", results.Select(result =>
        result.Recognizer.TotalMilliseconds));
    PrintStatistics("pipeline", results.Select(result =>
        result.Pipeline.TotalMilliseconds));
    PrintStatistics("wall", wallSamples);
}

static void PrintStatistics(string name, IEnumerable<double> samples)
{
    double[] values = samples.Order().ToArray();
    double median = values.Length % 2 == 0
        ? (values[values.Length / 2 - 1] + values[values.Length / 2]) / 2.0
        : values[values.Length / 2];
    int p95Index = Math.Max(0, (int)Math.Ceiling(values.Length * 0.95) - 1);
    Console.WriteLine(
        $"benchmark {name}: mean={values.Average():F3} ms, " +
        $"median={median:F3} ms, p95={values[p95Index]:F3} ms, " +
        $"min={values[0]:F3} ms, max={values[^1]:F3} ms");
}

static void PrintUsage()
{
    Console.WriteLine(
        "Usage: lw.PPOCR.Cli <backend> <model.json> <image> " +
        "[runtime-root] [device-id] [cls] [warmup] [runs]");
    Console.WriteLine("Backends: opencv, directml, openvino, tensorrt, onnxruntime");
}

internal sealed record DecodedImage(
    byte[] Pixels,
    int Width,
    int Height,
    int Stride);
