using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace Lw.PPOCRSharp
{
    public sealed class OcrEngine : IDisposable
    {
        private OcrSafeHandle handle;

        public OcrCapabilities Capabilities { get; private set; }

        public OcrEngine(OcrOptions options)
        {
            if (options == null)
            {
                throw new ArgumentNullException(nameof(options));
            }
            if (string.IsNullOrWhiteSpace(options.ModelManifest))
            {
                throw new ArgumentException("A model manifest is required.", nameof(options));
            }

            using (var runtimeRoot = new Utf8NativeString(options.RuntimeRoot))
            using (var runtimeLibrary = new Utf8NativeString(options.RuntimeLibrary))
            using (var modelManifest = new Utf8NativeString(options.ModelManifest))
            using (var backendOptions = new Utf8NativeString(options.BackendOptionsJson))
            {
                var config = new NativeMethods.NativeConfig
                {
                    StructSize = (uint)Marshal.SizeOf<NativeMethods.NativeConfig>(),
                    ApiVersion = NativeMethods.ApiVersion,
                    Backend = (int)options.Backend,
                    DeviceId = options.DeviceId,
                    RuntimeRoot = runtimeRoot.Pointer,
                    RuntimeLibrary = runtimeLibrary.Pointer,
                    ModelManifest = modelManifest.Pointer,
                    BackendOptionsJson = backendOptions.Pointer,
                    EnableCls = options.EnableClassifier ? 1 : 0,
                    LimitSideLength = options.LimitSideLength,
                    DetectionThreshold = options.DetectionThreshold,
                    DetectionBoxThreshold = options.DetectionBoxThreshold,
                    DetectionUnclipRatio = options.DetectionUnclipRatio,
                    UseDetectionDilation = options.UseDetectionDilation ? 1 : 0,
                    ClassifierThreshold = options.ClassifierThreshold,
                    ClassifierBatchSize = options.ClassifierBatchSize,
                    RecognitionBatchSize = options.RecognitionBatchSize,
                    RecognitionConcurrency = options.RecognitionConcurrency
                };

                IntPtr nativeHandle;
                int status = NativeMethods.Create(ref config, out nativeHandle);
                if (status != NativeMethods.StatusOk)
                {
                    throw CreateException(status, IntPtr.Zero);
                }
                handle = new OcrSafeHandle(nativeHandle);
                try
                {
                    Capabilities = ReadCapabilities(nativeHandle);
                }
                catch
                {
                    handle.Dispose();
                    handle = null;
                    throw;
                }
            }
        }

        public OcrResult Run(
            byte[] pixels,
            int width,
            int height,
            int stride,
            OcrPixelFormat pixelFormat)
        {
            OcrSafeHandle localHandle = GetHandle();
            if (pixels == null)
            {
                throw new ArgumentNullException(nameof(pixels));
            }
            if (width <= 0 || height <= 0 || stride <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(width), "Image dimensions and stride must be positive.");
            }
            if ((long)stride * height > pixels.LongLength)
            {
                throw new ArgumentException("The image buffer is smaller than stride times height.", nameof(pixels));
            }

            bool handleAdded = false;
            GCHandle pinned = default(GCHandle);
            try
            {
                localHandle.DangerousAddRef(ref handleAdded);
                pinned = GCHandle.Alloc(pixels, GCHandleType.Pinned);
                var image = new NativeMethods.NativeImage
                {
                    StructSize = (uint)Marshal.SizeOf<NativeMethods.NativeImage>(),
                    ApiVersion = NativeMethods.ApiVersion,
                    Data = pinned.AddrOfPinnedObject(),
                    DataSize = (ulong)pixels.LongLength,
                    Width = width,
                    Height = height,
                    Stride = stride,
                    PixelFormat = (int)pixelFormat
                };

                IntPtr nativeResult;
                IntPtr nativeHandle = localHandle.DangerousGetHandle();
                int status = NativeMethods.Run(nativeHandle, ref image, out nativeResult);
                if (status != NativeMethods.StatusOk)
                {
                    throw CreateException(status, nativeHandle);
                }

                try
                {
                    return CopyResult(nativeResult);
                }
                finally
                {
                    NativeMethods.ResultFree(nativeHandle, nativeResult);
                }
            }
            finally
            {
                if (pinned.IsAllocated)
                {
                    pinned.Free();
                }
                if (handleAdded)
                {
                    localHandle.DangerousRelease();
                }
            }
        }

        public OcrRecognitionResult Recognize(
            byte[] pixels, int width, int height, int stride,
            OcrPixelFormat pixelFormat = OcrPixelFormat.Bgr24)
        {
            return RecognizeBatch(new[] {
                new OcrImage(pixels, width, height, stride, pixelFormat)
            });
        }

        public OcrRecognitionResult RecognizeBatch(
            IReadOnlyList<OcrImage> images)
        {
            if (images == null)
            {
                throw new ArgumentNullException(nameof(images));
            }
            if (images.Count == 0)
            {
                throw new ArgumentException("At least one cropped image is required.",
                    nameof(images));
            }

            OcrSafeHandle localHandle = GetHandle();
            bool handleAdded = false;
            var pins = new GCHandle[images.Count];
            var nativeImages = new NativeMethods.NativeImage[images.Count];
            try
            {
                localHandle.DangerousAddRef(ref handleAdded);
                for (int index = 0; index < images.Count; index++)
                {
                    OcrImage image = images[index] ??
                        throw new ArgumentException("The image list contains null.", nameof(images));
                    pins[index] = GCHandle.Alloc(image.Pixels, GCHandleType.Pinned);
                    nativeImages[index] = new NativeMethods.NativeImage
                    {
                        StructSize = (uint)Marshal.SizeOf<NativeMethods.NativeImage>(),
                        ApiVersion = NativeMethods.ApiVersion,
                        Data = pins[index].AddrOfPinnedObject(),
                        DataSize = (ulong)image.Pixels.LongLength,
                        Width = image.Width,
                        Height = image.Height,
                        Stride = image.Stride,
                        PixelFormat = (int)image.PixelFormat
                    };
                }

                IntPtr nativeHandle = localHandle.DangerousGetHandle();
                IntPtr nativeResult;
                int status = NativeMethods.RecognizeBatch(nativeHandle,
                    nativeImages, (ulong)nativeImages.Length, out nativeResult);
                if (status != NativeMethods.StatusOk)
                {
                    throw CreateException(status, nativeHandle);
                }
                try
                {
                    return CopyRecognitionResult(nativeResult);
                }
                finally
                {
                    NativeMethods.RecognitionResultFree(nativeHandle, nativeResult);
                }
            }
            finally
            {
                for (int index = 0; index < pins.Length; index++)
                {
                    if (pins[index].IsAllocated)
                    {
                        pins[index].Free();
                    }
                }
                if (handleAdded)
                {
                    localHandle.DangerousRelease();
                }
            }
        }

        public void Dispose()
        {
            if (handle != null)
            {
                handle.Dispose();
                handle = null;
            }
        }

        private static OcrResult CopyResult(IntPtr pointer)
        {
            if (pointer == IntPtr.Zero)
            {
                throw new InvalidOperationException("The native Runtime returned a null result.");
            }

            NativeMethods.NativeResult native =
                Marshal.PtrToStructure<NativeMethods.NativeResult>(pointer);
            if (native.RegionCount > int.MaxValue)
            {
                throw new InvalidOperationException("The native Runtime returned too many text regions.");
            }

            var regions = new List<OcrTextRegion>((int)native.RegionCount);
            int itemSize = Marshal.SizeOf<NativeMethods.NativeTextRegion>();
            for (int index = 0; index < (int)native.RegionCount; index++)
            {
                IntPtr itemPointer = IntPtr.Add(native.Regions, checked(index * itemSize));
                NativeMethods.NativeTextRegion item =
                    Marshal.PtrToStructure<NativeMethods.NativeTextRegion>(itemPointer);
                regions.Add(new OcrTextRegion(
                    new[]
                    {
                        ToPoint(item.Box.TopLeft),
                        ToPoint(item.Box.TopRight),
                        ToPoint(item.Box.BottomRight),
                        ToPoint(item.Box.BottomLeft)
                    },
                    Utf8FromPointer(item.Text),
                    item.Score,
                    item.ClassifierLabel,
                    item.ClassifierScore));
            }

            return new OcrResult(
                regions,
                ToTiming(native.Detector),
                ToTiming(native.Classifier),
                ToTiming(native.Recognizer),
                ToTiming(native.Pipeline));
        }

        private static OcrRecognitionResult CopyRecognitionResult(IntPtr pointer)
        {
            if (pointer == IntPtr.Zero)
            {
                throw new InvalidOperationException(
                    "The native Runtime returned a null recognition result.");
            }
            NativeMethods.NativeRecognitionResult native =
                Marshal.PtrToStructure<NativeMethods.NativeRecognitionResult>(pointer);
            if (native.ItemCount > int.MaxValue)
            {
                throw new InvalidOperationException(
                    "The native Runtime returned too many recognition items.");
            }
            var items = new List<OcrRecognition>((int)native.ItemCount);
            int itemSize = Marshal.SizeOf<NativeMethods.NativeRecognition>();
            for (int index = 0; index < (int)native.ItemCount; index++)
            {
                IntPtr itemPointer = IntPtr.Add(native.Items, checked(index * itemSize));
                NativeMethods.NativeRecognition item =
                    Marshal.PtrToStructure<NativeMethods.NativeRecognition>(itemPointer);
                items.Add(new OcrRecognition(
                    checked((long)item.SourceIndex), Utf8FromPointer(item.Text),
                    item.Score, item.ClassifierLabel, item.ClassifierScore));
            }
            return new OcrRecognitionResult(items, ToTiming(native.Classifier),
                ToTiming(native.Recognizer), ToTiming(native.Pipeline));
        }

        private static OcrCapabilities ReadCapabilities(IntPtr nativeHandle)
        {
            var native = new NativeMethods.NativeCapabilities
            {
                StructSize = (uint)Marshal.SizeOf<NativeMethods.NativeCapabilities>()
            };
            int status = NativeMethods.GetCapabilities(nativeHandle, ref native);
            if (status != NativeMethods.StatusOk)
            {
                throw CreateException(status, nativeHandle);
            }
            return new OcrCapabilities(
                (OcrBackend)native.Backend,
                Utf8FromPointer(native.BackendName),
                native.SupportsCpu != 0,
                native.SupportsGpu != 0,
                native.SupportsFp16 != 0,
                native.SupportsInt8 != 0,
                native.SupportsClassifier != 0);
        }

        private static OcrPoint ToPoint(NativeMethods.NativePoint point)
        {
            return new OcrPoint(point.X, point.Y);
        }

        private static OcrTiming ToTiming(NativeMethods.NativeTiming timing)
        {
            return new OcrTiming(
                timing.Preprocess,
                timing.Inference,
                timing.Postprocess,
                timing.Total);
        }

        private static OcrException CreateException(int status, IntPtr nativeHandle)
        {
            ulong required = NativeMethods.GetLastError(nativeHandle, IntPtr.Zero, 0);
            if (required <= 1 || required > int.MaxValue)
            {
                return new OcrException(status, "Native OCR call failed with status " + status + ".");
            }

            IntPtr buffer = Marshal.AllocHGlobal((int)required);
            try
            {
                NativeMethods.GetLastError(nativeHandle, buffer, required);
                return new OcrException(status, Utf8FromPointer(buffer));
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        private static string Utf8FromPointer(IntPtr pointer)
        {
            if (pointer == IntPtr.Zero)
            {
                return string.Empty;
            }

            int length = 0;
            while (Marshal.ReadByte(pointer, length) != 0)
            {
                length++;
            }
            var bytes = new byte[length];
            Marshal.Copy(pointer, bytes, 0, length);
            return Encoding.UTF8.GetString(bytes);
        }

        private OcrSafeHandle GetHandle()
        {
            OcrSafeHandle value = handle;
            if (value == null || value.IsClosed || value.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(OcrEngine));
            }
            return value;
        }

        private sealed class Utf8NativeString : IDisposable
        {
            internal Utf8NativeString(string value)
            {
                if (string.IsNullOrEmpty(value))
                {
                    return;
                }
                byte[] bytes = Encoding.UTF8.GetBytes(value + "\0");
                Pointer = Marshal.AllocHGlobal(bytes.Length);
                Marshal.Copy(bytes, 0, Pointer, bytes.Length);
            }

            internal IntPtr Pointer { get; private set; }

            public void Dispose()
            {
                if (Pointer != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(Pointer);
                    Pointer = IntPtr.Zero;
                }
            }
        }
    }
}
