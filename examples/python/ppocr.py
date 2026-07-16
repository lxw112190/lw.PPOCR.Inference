"""
lw.PPOCR.Inference — Python binding via ctypes.

Minimal, zero-dependency wrapper around lw.PPOCR.dll.  Works with any
Python ≥ 3.8 on Windows x64.

Usage:
    import ppocr

    engine = ppocr.OcrEngine(
        backend="opencv",
        model_manifest="models/ppocrv6-tiny/model.json",
    )
    result = engine.run_file("photo.jpg")
    for region in result.regions:
        print(f"[{region.text}] score={region.score:.4f}")
    engine.close()
"""

import ctypes
import os
import sys
from ctypes import (
    CFUNCTYPE, POINTER, Structure, byref, c_char_p, c_double, c_float,
    c_int32, c_uint32, c_uint64, c_void_p, cdll, pointer,
)
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple

_LW_PPOCR_BACKEND_OPENCV_DNN = 1
_LW_PPOCR_BACKEND_DIRECTML   = 2
_LW_PPOCR_BACKEND_OPENVINO   = 3
_LW_PPOCR_BACKEND_TENSORRT   = 4

_BACKEND_MAP = {
    "opencv":   _LW_PPOCR_BACKEND_OPENCV_DNN,
    "directml": _LW_PPOCR_BACKEND_DIRECTML,
    "openvino": _LW_PPOCR_BACKEND_OPENVINO,
    "tensorrt": _LW_PPOCR_BACKEND_TENSORRT,
}


# ---------------------------------------------------------------------------
# ctypes structures (mirror include/lw/ppocr/ppocr_api.h)
# ---------------------------------------------------------------------------

class _LwPoint(Structure):
    _fields_ = [("x", c_float), ("y", c_float)]


class _LwQuad(Structure):
    _fields_ = [
        ("top_left", _LwPoint), ("top_right", _LwPoint),
        ("bottom_right", _LwPoint), ("bottom_left", _LwPoint),
    ]


class _LwTextRegion(Structure):
    _fields_ = [
        ("box", _LwQuad),
        ("text_utf8", c_char_p),
        ("score", c_float),
        ("cls_label", c_int32),
        ("cls_score", c_float),
        ("reserved_i32", c_int32 * 4),
    ]


class _LwTiming(Structure):
    _fields_ = [
        ("preprocess_ms", c_double),
        ("inference_ms", c_double),
        ("postprocess_ms", c_double),
        ("total_ms", c_double),
    ]


class _LwResult(Structure):
    _fields_ = [
        ("struct_size", c_uint32),
        ("api_version", c_uint32),
        ("region_count", c_uint64),
        ("regions", POINTER(_LwTextRegion)),
        ("detector", _LwTiming),
        ("classifier", _LwTiming),
        ("recognizer", _LwTiming),
        ("pipeline", _LwTiming),
        ("reserved_i32", c_int32 * 8),
        ("reserved_ptr", c_void_p * 4),
    ]


class _LwImage(Structure):
    _fields_ = [
        ("struct_size", c_uint32),
        ("api_version", c_uint32),
        ("data", c_void_p),
        ("data_size", c_uint64),
        ("width", c_int32),
        ("height", c_int32),
        ("stride", c_int32),
        ("pixel_format", c_int32),
        ("reserved_i32", c_int32 * 4),
    ]


class _LwConfig(Structure):
    _fields_ = [
        ("struct_size", c_uint32),
        ("api_version", c_uint32),
        ("backend", c_int32),
        ("device_id", c_int32),
        ("runtime_root_utf8", c_char_p),
        ("runtime_library_utf8", c_char_p),
        ("model_manifest_utf8", c_char_p),
        ("backend_options_json_utf8", c_char_p),
        ("enable_cls", c_int32),
        ("limit_side_len", c_int32),
        ("det_db_thresh", c_float),
        ("det_db_box_thresh", c_float),
        ("det_db_unclip_ratio", c_float),
        ("det_use_dilation", c_int32),
        ("cls_threshold", c_float),
        ("cls_batch_size", c_int32),
        ("rec_batch_size", c_int32),
        ("rec_concurrency", c_int32),
        ("log_level", c_int32),
        ("log_callback", c_void_p),
        ("log_user_data", c_void_p),
        ("reserved_i32", c_int32 * 8),
        ("reserved_ptr", c_void_p * 4),
    ]


# ---------------------------------------------------------------------------
# Python data classes
# ---------------------------------------------------------------------------

@dataclass
class OcrPoint:
    x: float
    y: float


@dataclass
class OcrTextRegion:
    text: str
    score: float
    box: List[OcrPoint]
    cls_label: int = -1
    cls_score: float = 0.0


@dataclass
class OcrTiming:
    preprocess_ms: float = 0.0
    inference_ms: float = 0.0
    postprocess_ms: float = 0.0
    total_ms: float = 0.0


@dataclass
class OcrResult:
    regions: List[OcrTextRegion] = field(default_factory=list)
    detector: OcrTiming = field(default_factory=OcrTiming)
    classifier: OcrTiming = field(default_factory=OcrTiming)
    recognizer: OcrTiming = field(default_factory=OcrTiming)
    pipeline: OcrTiming = field(default_factory=OcrTiming)


# ---------------------------------------------------------------------------
# Engine
# ---------------------------------------------------------------------------

class OcrEngine:
    """A single OCR engine instance backed by lw.PPOCR.dll.

    Parameters
    ----------
    backend : str
        One of ``"opencv"``, ``"directml"``, ``"openvino"``, ``"tensorrt"``.
    model_manifest : str
        Absolute or relative path to a ``model.json`` manifest.
    device_id : int
        GPU device index.  Ignored for CPU-only backends.
    runtime_root : Optional[str]
        Root of the ``runtimes/win-x64`` layout.  Defaults to the directory
        containing ``lw.PPOCR.dll``.
    runtime_library : Optional[str]
        Explicit path to the backend Runtime DLL.  Overrides ``runtime_root``
        discovery.
    enable_cls : bool
        Enable direction classification (CLS).
    limit_side_len : int
        Maximum side length for detection preprocessing.
    det_db_thresh : float
        Detection DB threshold (0–1).
    det_db_box_thresh : float
        Detection box score threshold (0–1).
    det_db_unclip_ratio : float
        Detection unclip ratio (typically 1.5–2.0).
    cls_threshold : float
        Classifier confidence threshold (0–1).
    cls_batch_size : int
        Classifier batch size.
    rec_batch_size : int
        Recogniser batch size.
    rec_concurrency : int
        Number of recognition workers.
    """

    def __init__(
        self,
        backend: str = "opencv",
        model_manifest: Optional[str] = None,
        device_id: int = 0,
        runtime_root: Optional[str] = None,
        runtime_library: Optional[str] = None,
        backend_options: Optional[str] = None,
        enable_cls: bool = True,
        limit_side_len: int = 960,
        det_db_thresh: float = 0.3,
        det_db_box_thresh: float = 0.6,
        det_db_unclip_ratio: float = 1.6,
        cls_threshold: float = 0.9,
        cls_batch_size: int = 8,
        rec_batch_size: int = 1,
        rec_concurrency: int = 2,
        dll_path: Optional[str] = None,
    ):
        self._dll = cdll.LoadLibrary(dll_path or "lw.PPOCR.dll")
        self._setup_api()

        backend_id = _BACKEND_MAP.get(backend.lower())
        if backend_id is None:
            raise ValueError(f"unknown backend: {backend}")

        config = _LwConfig()
        config.struct_size         = ctypes.sizeof(_LwConfig)
        config.api_version         = 1
        config.backend             = backend_id
        config.device_id           = device_id
        config.runtime_root_utf8   = _to_bytes(runtime_root)
        config.runtime_library_utf8 = _to_bytes(runtime_library)
        config.model_manifest_utf8 = _to_bytes(model_manifest)
        config.backend_options_json_utf8 = _to_bytes(backend_options)
        config.enable_cls          = 1 if enable_cls else 0
        config.limit_side_len      = limit_side_len
        config.det_db_thresh       = det_db_thresh
        config.det_db_box_thresh   = det_db_box_thresh
        config.det_db_unclip_ratio = det_db_unclip_ratio
        config.cls_threshold       = cls_threshold
        config.cls_batch_size      = cls_batch_size
        config.rec_batch_size      = rec_batch_size
        config.rec_concurrency     = rec_concurrency

        self._handle = c_void_p()
        status = self._dll.lw_ppocr_create(byref(config), byref(self._handle))
        if status != 0 or not self._handle:
            msg = self._last_error()
            raise RuntimeError(f"lw_ppocr_create failed (status={status}): {msg}")

    # -- public API ----------------------------------------------------------

    def run(self, pixels: bytes, width: int, height: int, stride: int,
            pixel_format: int = 2) -> OcrResult:
        """Run OCR on a raw BGR24 (or other) pixel buffer."""
        image = _LwImage()
        image.struct_size  = ctypes.sizeof(_LwImage)
        image.api_version  = 1
        image.data         = (ctypes.c_char * len(pixels)).from_buffer_copy(pixels)
        image.data_size    = len(pixels)
        image.width        = width
        image.height       = height
        image.stride       = stride
        image.pixel_format = pixel_format

        result_ptr = POINTER(_LwResult)()
        status = self._dll.lw_ppocr_run(
            self._handle, byref(image), byref(result_ptr))
        if status != 0 or not result_ptr:
            msg = self._last_error()
            raise RuntimeError(f"lw_ppocr_run failed (status={status}): {msg}")

        native = result_ptr.contents
        result = self._copy_result(native)
        self._dll.lw_ppocr_result_free(self._handle, result_ptr)
        return result

    def run_file(self, path: str) -> OcrResult:
        """Decode a JPEG/PNG image with OpenCV and run OCR.

        Requires ``pip install opencv-python``.
        """
        import cv2  # type: ignore
        import numpy as np

        img: np.ndarray = cv2.imread(path, cv2.IMREAD_COLOR)
        if img is None:
            raise FileNotFoundError(f"cannot read image: {path}")
        return self.run(
            img.tobytes(),
            img.shape[1],
            img.shape[0],
            int(img.strides[0]),
            pixel_format=2,  # BGR24
        )

    def close(self):
        """Destroy the engine.  Not thread-safe with concurrent inference."""
        if self._handle:
            self._dll.lw_ppocr_destroy(byref(self._handle))
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    # -- internals -----------------------------------------------------------

    def _setup_api(self):
        dll = self._dll
        dll.lw_ppocr_create.restype = c_int32
        dll.lw_ppocr_create.argtypes = [POINTER(_LwConfig), POINTER(c_void_p)]
        dll.lw_ppocr_run.restype = c_int32
        dll.lw_ppocr_run.argtypes = [
            c_void_p, POINTER(_LwImage), POINTER(POINTER(_LwResult))]
        dll.lw_ppocr_result_free.restype = None
        dll.lw_ppocr_result_free.argtypes = [c_void_p, POINTER(_LwResult)]
        dll.lw_ppocr_destroy.restype = None
        dll.lw_ppocr_destroy.argtypes = [POINTER(c_void_p)]
        dll.lw_ppocr_get_last_error.restype = c_uint64
        dll.lw_ppocr_get_last_error.argtypes = [c_void_p, c_char_p, c_uint64]

    def _last_error(self) -> str:
        needed = self._dll.lw_ppocr_get_last_error(
            self._handle if self._handle else None, None, 0)
        if needed <= 1:
            return ""
        buf = ctypes.create_string_buffer(needed)
        self._dll.lw_ppocr_get_last_error(
            self._handle if self._handle else None, buf, needed)
        return buf.value.decode("utf-8", errors="replace")

    @staticmethod
    def _copy_result(native: _LwResult) -> OcrResult:
        regions = []
        for i in range(native.region_count):
            r = native.regions[i]
            q = r.box
            regions.append(OcrTextRegion(
                text=r.text_utf8.decode("utf-8") if r.text_utf8 else "",
                score=r.score,
                cls_label=r.cls_label,
                cls_score=r.cls_score,
                box=[
                    OcrPoint(q.top_left.x, q.top_left.y),
                    OcrPoint(q.top_right.x, q.top_right.y),
                    OcrPoint(q.bottom_right.x, q.bottom_right.y),
                    OcrPoint(q.bottom_left.x, q.bottom_left.y),
                ],
            ))
        return OcrResult(
            regions=regions,
            detector=OcrTiming(
                native.detector.preprocess_ms,
                native.detector.inference_ms,
                native.detector.postprocess_ms,
                native.detector.total_ms,
            ),
            classifier=OcrTiming(
                native.classifier.preprocess_ms,
                native.classifier.inference_ms,
                native.classifier.postprocess_ms,
                native.classifier.total_ms,
            ),
            recognizer=OcrTiming(
                native.recognizer.preprocess_ms,
                native.recognizer.inference_ms,
                native.recognizer.postprocess_ms,
                native.recognizer.total_ms,
            ),
            pipeline=OcrTiming(
                native.pipeline.preprocess_ms,
                native.pipeline.inference_ms,
                native.pipeline.postprocess_ms,
                native.pipeline.total_ms,
            ),
        )


def _to_bytes(value: Optional[str]) -> Optional[bytes]:
    return value.encode("utf-8") if value else None
