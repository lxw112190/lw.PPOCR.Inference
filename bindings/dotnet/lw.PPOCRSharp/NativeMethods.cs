using Microsoft.Win32.SafeHandles;
using System;
using System.Runtime.InteropServices;

namespace Lw.PPOCRSharp
{
    internal static class NativeMethods
    {
        internal const uint ApiVersion = 1;
        internal const int StatusOk = 0;
        private const string DllName = "lw.PPOCR.dll";

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeConfig
        {
            internal uint StructSize;
            internal uint ApiVersion;
            internal int Backend;
            internal int DeviceId;
            internal IntPtr RuntimeRoot;
            internal IntPtr RuntimeLibrary;
            internal IntPtr ModelManifest;
            internal IntPtr BackendOptionsJson;
            internal int EnableCls;
            internal int LimitSideLength;
            internal float DetectionThreshold;
            internal float DetectionBoxThreshold;
            internal float DetectionUnclipRatio;
            internal int UseDetectionDilation;
            internal float ClassifierThreshold;
            internal int ClassifierBatchSize;
            internal int RecognitionBatchSize;
            internal int RecognitionConcurrency;
            internal int LogLevel;
            internal IntPtr LogCallback;
            internal IntPtr LogUserData;
            internal int ReservedI0;
            internal int ReservedI1;
            internal int ReservedI2;
            internal int ReservedI3;
            internal int ReservedI4;
            internal int ReservedI5;
            internal int ReservedI6;
            internal int ReservedI7;
            internal IntPtr ReservedP0;
            internal IntPtr ReservedP1;
            internal IntPtr ReservedP2;
            internal IntPtr ReservedP3;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeImage
        {
            internal uint StructSize;
            internal uint ApiVersion;
            internal IntPtr Data;
            internal ulong DataSize;
            internal int Width;
            internal int Height;
            internal int Stride;
            internal int PixelFormat;
            internal int ReservedI0;
            internal int ReservedI1;
            internal int ReservedI2;
            internal int ReservedI3;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativePoint
        {
            internal float X;
            internal float Y;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeQuad
        {
            internal NativePoint TopLeft;
            internal NativePoint TopRight;
            internal NativePoint BottomRight;
            internal NativePoint BottomLeft;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeTextRegion
        {
            internal NativeQuad Box;
            internal IntPtr Text;
            internal float Score;
            internal int ClassifierLabel;
            internal float ClassifierScore;
            internal int ReservedI0;
            internal int ReservedI1;
            internal int ReservedI2;
            internal int ReservedI3;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeTiming
        {
            internal double Preprocess;
            internal double Inference;
            internal double Postprocess;
            internal double Total;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeResult
        {
            internal uint StructSize;
            internal uint ApiVersion;
            internal ulong RegionCount;
            internal IntPtr Regions;
            internal NativeTiming Detector;
            internal NativeTiming Classifier;
            internal NativeTiming Recognizer;
            internal NativeTiming Pipeline;
            internal int ReservedI0;
            internal int ReservedI1;
            internal int ReservedI2;
            internal int ReservedI3;
            internal int ReservedI4;
            internal int ReservedI5;
            internal int ReservedI6;
            internal int ReservedI7;
            internal IntPtr ReservedP0;
            internal IntPtr ReservedP1;
            internal IntPtr ReservedP2;
            internal IntPtr ReservedP3;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct NativeCapabilities
        {
            internal uint StructSize;
            internal uint ApiVersion;
            internal int Backend;
            internal IntPtr BackendName;
            internal int SupportsCpu;
            internal int SupportsGpu;
            internal int SupportsFp16;
            internal int SupportsInt8;
            internal int SupportsClassifier;
            internal int ReservedI0;
            internal int ReservedI1;
            internal int ReservedI2;
            internal int ReservedI3;
            internal int ReservedI4;
            internal int ReservedI5;
            internal int ReservedI6;
            internal int ReservedI7;
        }

        [DllImport(DllName, EntryPoint = "lw_ppocr_create", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Create(ref NativeConfig config, out IntPtr handle);

        [DllImport(DllName, EntryPoint = "lw_ppocr_run", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Run(IntPtr handle, ref NativeImage image, out IntPtr result);

        [DllImport(DllName, EntryPoint = "lw_ppocr_result_free", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void ResultFree(IntPtr handle, IntPtr result);

        [DllImport(DllName, EntryPoint = "lw_ppocr_get_capabilities", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int GetCapabilities(
            IntPtr handle,
            ref NativeCapabilities capabilities);

        [DllImport(DllName, EntryPoint = "lw_ppocr_get_last_error", CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong GetLastError(IntPtr handle, IntPtr buffer, ulong capacity);

        [DllImport(DllName, EntryPoint = "lw_ppocr_destroy", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Destroy(ref IntPtr handle);
    }

    internal sealed class OcrSafeHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        internal OcrSafeHandle(IntPtr handle)
            : base(true)
        {
            SetHandle(handle);
        }

        protected override bool ReleaseHandle()
        {
            IntPtr value = handle;
            NativeMethods.Destroy(ref value);
            SetHandle(IntPtr.Zero);
            return true;
        }
    }
}
