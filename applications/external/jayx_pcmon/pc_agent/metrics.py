"""Collect host metrics for JAYX (with stable GPU / FPS hold)."""

from __future__ import annotations

import ctypes
import struct
import time
from ctypes import wintypes

import psutil

try:
    import pynvml

    _HAS_NVML = True
except Exception:
    pynvml = None  # type: ignore
    _HAS_NVML = False

_NVML_HANDLE = None
_NVML_READY = False

UNITS = ["B", "KB", "MB", "GB", "TB"]
UNIT_BASE = 1024

GPU_HOLD_S = 15.0
FPS_HOLD_S = 3.0

_last_gpu: dict | None = None  # util, vram_total, vram_used, temp, ts
_last_fps: tuple[int, float] = (0, 0.0)  # fps, ts

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

user32.GetForegroundWindow.restype = wintypes.HWND
user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
user32.GetWindowTextLengthW.restype = ctypes.c_int
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.restype = ctypes.c_int
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD

kernel32.OpenFileMappingW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
kernel32.OpenFileMappingW.restype = wintypes.HANDLE
kernel32.MapViewOfFile.argtypes = [
    wintypes.HANDLE,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    ctypes.c_size_t,
]
kernel32.MapViewOfFile.restype = ctypes.c_void_p
kernel32.UnmapViewOfFile.argtypes = [ctypes.c_void_p]
kernel32.UnmapViewOfFile.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL

RTSS_MAP_NAME = "RTSSSharedMemoryV2"
RTSS_SIGNATURE = 0x52545353
FILE_MAP_READ = 0x0004
MAX_PATH = 260
APP_ENTRY_PROCESS_ID_OFFSET = 0
APP_ENTRY_FRAMETIME_OFFSET = 4 + MAX_PATH + 4 + 4 + 4 + 4


def get_exp(num_bytes: int) -> int:
    for exp in (4, 3, 2, 1):
        if num_bytes > UNIT_BASE**exp:
            return exp
    return 0


def scaled_value_and_unit(num_bytes: int) -> tuple[int, bytes]:
    exp = get_exp(num_bytes)
    scaled = int(num_bytes / (UNIT_BASE**exp) * 10)
    unit = UNITS[exp].encode("ascii")
    return min(scaled, 0xFFFF), unit


def _pick_nvml_handle():
    """Prefer discrete GPU with most VRAM."""
    global _NVML_HANDLE, _NVML_READY
    if not _HAS_NVML:
        _NVML_HANDLE = None
        _NVML_READY = False
        return

    try:
        try:
            pynvml.nvmlInit()
        except Exception:
            pass

        count = pynvml.nvmlDeviceGetCount()
        best = None
        best_mem = -1
        for i in range(count):
            h = pynvml.nvmlDeviceGetHandleByIndex(i)
            try:
                mem = pynvml.nvmlDeviceGetMemoryInfo(h).total
            except Exception:
                mem = 0
            try:
                name = pynvml.nvmlDeviceGetName(h)
                if isinstance(name, bytes):
                    name = name.decode("utf-8", errors="replace")
            except Exception:
                name = ""
            # Prefer non-zero mem; boost names that look discrete
            score = mem
            if any(k in name.upper() for k in ("RTX", "GTX", "QUADRO", "TESLA", "GEFORCE")):
                score += 1
            if score > best_mem:
                best_mem = score
                best = h
        _NVML_HANDLE = best
        _NVML_READY = best is not None
    except Exception:
        _NVML_HANDLE = None
        _NVML_READY = False


def _ensure_nvml(reinit: bool = False) -> bool:
    global _NVML_READY
    if reinit or not _NVML_READY or _NVML_HANDLE is None:
        _pick_nvml_handle()
    return _NVML_HANDLE is not None


_ensure_nvml()


def nvml_gpu_name() -> str | None:
    if not _ensure_nvml():
        return None
    try:
        name = pynvml.nvmlDeviceGetName(_NVML_HANDLE)
        if isinstance(name, bytes):
            name = name.decode("utf-8", errors="replace")
        return str(name).strip()
    except Exception:
        return None


def nvml_vram_total_bytes() -> int | None:
    if not _ensure_nvml():
        return None
    try:
        return int(pynvml.nvmlDeviceGetMemoryInfo(_NVML_HANDLE).total)
    except Exception:
        return None


def _read_gpu_raw() -> tuple[int | None, int | None, int | None, int | None]:
    """util%, vram_total, vram_used, temp — independent fields may be None."""
    if not _ensure_nvml():
        return None, None, None, None

    util = None
    total = None
    used = None
    temp = None

    try:
        rates = pynvml.nvmlDeviceGetUtilizationRates(_NVML_HANDLE)
        util = int(rates.gpu)
    except Exception:
        pass

    try:
        mem = pynvml.nvmlDeviceGetMemoryInfo(_NVML_HANDLE)
        total = int(mem.total)
        used = int(mem.used)
    except Exception:
        pass

    try:
        temp = int(
            pynvml.nvmlDeviceGetTemperature(_NVML_HANDLE, pynvml.NVML_TEMPERATURE_GPU)
        )
    except Exception:
        pass

    if util is None and total is None:
        return None, None, None, None
    return util, total, used, temp


def read_gpu_stats() -> tuple[int | None, int | None, int | None, int | None]:
    """Stable GPU stats with last-good hold and one re-init retry."""
    global _last_gpu

    util, total, used, temp = _read_gpu_raw()

    if util is None and total is None:
        # Transient failure — re-init once
        _ensure_nvml(reinit=True)
        util, total, used, temp = _read_gpu_raw()

    now = time.monotonic()

    if util is not None or total is not None:
        prev = _last_gpu or {}
        entry = {
            "util": util if util is not None else prev.get("util"),
            "vram_total": total if total is not None else prev.get("vram_total"),
            "vram_used": used if used is not None else prev.get("vram_used"),
            "temp": temp if temp is not None else prev.get("temp"),
            "ts": now,
        }
        # Only stamp ts if we got something useful
        if util is not None or total is not None:
            _last_gpu = entry
        return entry["util"], entry["vram_total"], entry["vram_used"], entry["temp"]

    # Full failure — hold last good
    if _last_gpu and (now - _last_gpu["ts"]) < GPU_HOLD_S:
        g = _last_gpu
        return g.get("util"), g.get("vram_total"), g.get("vram_used"), g.get("temp")

    return None, None, None, None


def read_cpu_temp() -> int | None:
    try:
        import wmi  # type: ignore

        for namespace in ("root\\LibreHardwareMonitor", "root\\OpenHardwareMonitor"):
            try:
                client = wmi.WMI(namespace=namespace)
                best = None
                for sensor in client.Sensor():
                    if getattr(sensor, "SensorType", None) != "Temperature":
                        continue
                    name = (getattr(sensor, "Name", "") or "").lower()
                    value = getattr(sensor, "Value", None)
                    if value is None:
                        continue
                    if "package" in name or "cpu package" in name or name == "cpu":
                        return max(0, min(int(round(float(value))), 254))
                    if "cpu" in name and best is None:
                        best = max(0, min(int(round(float(value))), 254))
                if best is not None:
                    return best
            except Exception:
                continue
    except Exception:
        pass

    try:
        temps = psutil.sensors_temperatures()  # type: ignore[attr-defined]
        if temps:
            for _name, entries in temps.items():
                for entry in entries:
                    if entry.current is not None:
                        return max(0, min(int(round(entry.current)), 254))
    except Exception:
        pass

    return None


def _fps_from_frametime_us(frame_time_us: int) -> int:
    if frame_time_us <= 0:
        return 0
    return min(round(1_000_000 / frame_time_us), 0xFFFF)


def _read_rtss_entries() -> list[tuple[int, int]]:
    """List of (pid, fps) from RTSS shared memory."""
    h_map = kernel32.OpenFileMappingW(FILE_MAP_READ, False, RTSS_MAP_NAME)
    if not h_map:
        return []

    results: list[tuple[int, int]] = []
    try:
        view = kernel32.MapViewOfFile(h_map, FILE_MAP_READ, 0, 0, 0)
        if not view:
            return []
        try:
            header = ctypes.string_at(view, 20)
            dw_signature, _, app_entry_size, app_arr_offset, app_arr_size = struct.unpack(
                "<IIIII", header
            )
            if dw_signature != RTSS_SIGNATURE:
                return []

            for i in range(app_arr_size):
                entry_addr = view + app_arr_offset + i * app_entry_size
                entry_pid = struct.unpack(
                    "<I", ctypes.string_at(entry_addr + APP_ENTRY_PROCESS_ID_OFFSET, 4)
                )[0]
                if not entry_pid:
                    continue
                frame_time_us = struct.unpack(
                    "<I", ctypes.string_at(entry_addr + APP_ENTRY_FRAMETIME_OFFSET, 4)
                )[0]
                fps = _fps_from_frametime_us(frame_time_us)
                if fps > 0:
                    results.append((entry_pid, fps))
            return results
        finally:
            kernel32.UnmapViewOfFile(view)
    finally:
        kernel32.CloseHandle(h_map)


def read_fps() -> int:
    global _last_fps

    entries = _read_rtss_entries()
    now = time.monotonic()
    fps = 0

    hwnd = user32.GetForegroundWindow()
    fg_pid = 0
    if hwnd:
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        fg_pid = int(pid.value or 0)

    if fg_pid and entries:
        for pid, f in entries:
            if pid == fg_pid:
                fps = f
                break

    # Fallback: highest FPS among RTSS apps (exclusive fullscreen focus quirks)
    if fps == 0 and entries:
        fps = max(f for _pid, f in entries)

    if fps > 0:
        _last_fps = (fps, now)
        return fps

    held, held_ts = _last_fps
    if held > 0 and (now - held_ts) < FPS_HOLD_S:
        return held
    return 0


def read_game_name() -> str:
    hwnd = user32.GetForegroundWindow()
    if not hwnd:
        return ""

    length = user32.GetWindowTextLengthW(hwnd)
    if length <= 0:
        return _process_name_for_hwnd(hwnd)

    buf = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(hwnd, buf, length + 1)
    title = buf.value.strip()
    if not title:
        return _process_name_for_hwnd(hwnd)

    for sep in (" - ", " — ", " | "):
        if sep in title:
            left = title.split(sep)[0].strip()
            if left:
                title = left
                break
    return title[:64]


def _process_name_for_hwnd(hwnd) -> str:
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    if not pid.value:
        return ""
    try:
        return psutil.Process(pid.value).name()
    except Exception:
        return ""


def collect() -> dict:
    cpu_usage = round(psutil.cpu_percent(interval=1))
    mem = psutil.virtual_memory()
    ram_max, ram_unit = scaled_value_and_unit(mem.total)
    ram_usage = round(mem.percent)
    cpu_temp = read_cpu_temp()

    gpu_util, vram_total, vram_used, gpu_temp = read_gpu_stats()

    if gpu_util is None and vram_total is None:
        gpu_usage, vram_max, vram_usage, vram_unit = 0xFF, 0, 0xFF, b"B"
        gpu_temp_c = 0xFF
    else:
        gpu_usage = 0xFF if gpu_util is None else max(0, min(int(gpu_util), 100))
        if vram_total and vram_total > 0:
            vram_max, vram_unit = scaled_value_and_unit(vram_total)
            used = vram_used or 0
            vram_usage = max(0, min(round(used / vram_total * 100), 100))
        else:
            vram_max, vram_usage, vram_unit = 0, 0xFF, b"B"
        gpu_temp_c = 0xFF if gpu_temp is None else max(0, min(int(gpu_temp), 254))

    return {
        "cpu_usage": cpu_usage,
        "cpu_temp_c": 0xFF if cpu_temp is None else cpu_temp,
        "ram_max": ram_max,
        "ram_usage": ram_usage,
        "ram_unit": ram_unit,
        "gpu_usage": gpu_usage,
        "gpu_temp_c": gpu_temp_c,
        "vram_max": vram_max,
        "vram_usage": vram_usage,
        "vram_unit": vram_unit,
        "fps": read_fps(),
        "game_name": read_game_name(),
    }
