using System;
using System.Collections.Generic;

namespace Lw.PPOCRSharp
{
    public enum OcrBackend
    {
        OpenCvDnn = 1,
        DirectML = 2,
        OpenVINO = 3,
        TensorRT = 4
    }

    public enum OcrPixelFormat
    {
        Gray8 = 1,
        Bgr24 = 2,
        Rgb24 = 3,
        Bgra32 = 4,
        Rgba32 = 5
    }

    public sealed class OcrOptions
    {
        public OcrBackend Backend { get; set; } = OcrBackend.OpenCvDnn;
        public int DeviceId { get; set; }
        public string RuntimeRoot { get; set; }
        public string RuntimeLibrary { get; set; }
        public string ModelManifest { get; set; }
        public string BackendOptionsJson { get; set; }
        public bool EnableClassifier { get; set; }
        public int LimitSideLength { get; set; } = 960;
        public float DetectionThreshold { get; set; } = 0.3f;
        public float DetectionBoxThreshold { get; set; } = 0.6f;
        public float DetectionUnclipRatio { get; set; } = 1.6f;
        public bool UseDetectionDilation { get; set; }
        public float ClassifierThreshold { get; set; } = 0.9f;
        public int ClassifierBatchSize { get; set; } = 8;
        public int RecognitionBatchSize { get; set; } = 1;
        public int RecognitionConcurrency { get; set; } = 1;
    }

    public sealed class OcrCapabilities
    {
        internal OcrCapabilities(
            OcrBackend backend,
            string backendName,
            bool supportsCpu,
            bool supportsGpu,
            bool supportsFp16,
            bool supportsInt8,
            bool supportsClassifier)
        {
            Backend = backend;
            BackendName = backendName;
            SupportsCpu = supportsCpu;
            SupportsGpu = supportsGpu;
            SupportsFp16 = supportsFp16;
            SupportsInt8 = supportsInt8;
            SupportsClassifier = supportsClassifier;
        }

        public OcrBackend Backend { get; }
        public string BackendName { get; }
        public bool SupportsCpu { get; }
        public bool SupportsGpu { get; }
        public bool SupportsFp16 { get; }
        public bool SupportsInt8 { get; }
        public bool SupportsClassifier { get; }
    }

    public struct OcrPoint
    {
        public OcrPoint(float x, float y)
        {
            X = x;
            Y = y;
        }

        public float X { get; }
        public float Y { get; }
    }

    public sealed class OcrTextRegion
    {
        internal OcrTextRegion(
            OcrPoint[] box,
            string text,
            float score,
            int classifierLabel,
            float classifierScore)
        {
            Box = box;
            Text = text;
            Score = score;
            ClassifierLabel = classifierLabel;
            ClassifierScore = classifierScore;
        }

        public IReadOnlyList<OcrPoint> Box { get; }
        public string Text { get; }
        public float Score { get; }
        public int ClassifierLabel { get; }
        public float ClassifierScore { get; }
    }

    public struct OcrTiming
    {
        internal OcrTiming(double preprocess, double inference, double postprocess, double total)
        {
            PreprocessMilliseconds = preprocess;
            InferenceMilliseconds = inference;
            PostprocessMilliseconds = postprocess;
            TotalMilliseconds = total;
        }

        public double PreprocessMilliseconds { get; }
        public double InferenceMilliseconds { get; }
        public double PostprocessMilliseconds { get; }
        public double TotalMilliseconds { get; }
    }

    public sealed class OcrResult
    {
        internal OcrResult(
            IReadOnlyList<OcrTextRegion> regions,
            OcrTiming detector,
            OcrTiming classifier,
            OcrTiming recognizer,
            OcrTiming pipeline)
        {
            Regions = regions;
            Detector = detector;
            Classifier = classifier;
            Recognizer = recognizer;
            Pipeline = pipeline;
        }

        public IReadOnlyList<OcrTextRegion> Regions { get; }
        public OcrTiming Detector { get; }
        public OcrTiming Classifier { get; }
        public OcrTiming Recognizer { get; }
        public OcrTiming Pipeline { get; }
    }

    public sealed class OcrImage
    {
        public OcrImage(byte[] pixels, int width, int height, int stride,
            OcrPixelFormat pixelFormat = OcrPixelFormat.Bgr24)
        {
            Pixels = pixels ?? throw new ArgumentNullException(nameof(pixels));
            if (width <= 0 || height <= 0 || stride <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(width),
                    "Image dimensions and stride must be positive.");
            }
            if ((long)stride * height > pixels.LongLength)
            {
                throw new ArgumentException(
                    "The image buffer is smaller than stride times height.",
                    nameof(pixels));
            }
            Width = width;
            Height = height;
            Stride = stride;
            PixelFormat = pixelFormat;
        }

        public byte[] Pixels { get; }
        public int Width { get; }
        public int Height { get; }
        public int Stride { get; }
        public OcrPixelFormat PixelFormat { get; }
    }

    public sealed class OcrRecognition
    {
        internal OcrRecognition(long sourceIndex, string text, float score,
            int classifierLabel, float classifierScore)
        {
            SourceIndex = sourceIndex;
            Text = text;
            Score = score;
            ClassifierLabel = classifierLabel;
            ClassifierScore = classifierScore;
        }

        public long SourceIndex { get; }
        public string Text { get; }
        public float Score { get; }
        public int ClassifierLabel { get; }
        public float ClassifierScore { get; }
    }

    public sealed class OcrRecognitionResult
    {
        internal OcrRecognitionResult(IReadOnlyList<OcrRecognition> items,
            OcrTiming classifier, OcrTiming recognizer, OcrTiming pipeline)
        {
            Items = items;
            Classifier = classifier;
            Recognizer = recognizer;
            Pipeline = pipeline;
        }

        public IReadOnlyList<OcrRecognition> Items { get; }
        public OcrTiming Classifier { get; }
        public OcrTiming Recognizer { get; }
        public OcrTiming Pipeline { get; }
    }

    public sealed class OcrException : Exception
    {
        internal OcrException(int status, string message)
            : base(message)
        {
            Status = status;
        }

        public int Status { get; }
    }
}
