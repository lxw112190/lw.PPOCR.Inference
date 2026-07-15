using Lw.PPOCRSharp;

string runtimePath = Path.Combine(
    AppContext.BaseDirectory,
    "lw.PPOCR.Runtime.Stub.dll");

using var engine = new OcrEngine(new OcrOptions
{
    Backend = OcrBackend.OpenCvDnn,
    RuntimeLibrary = runtimePath,
    ModelManifest = "stub://model-manifest"
});

byte[] pixels = new byte[12];
OcrResult result = engine.Run(
    pixels,
    width: 2,
    height: 2,
    stride: 6,
    pixelFormat: OcrPixelFormat.Bgr24);

Console.WriteLine($"C# binding smoke test passed, regions: {result.Regions.Count}");

