"""Accurate multi-section PC identity for JAYX Specs page."""

from __future__ import annotations

import platform
import re
import time

import psutil

try:
    from metrics import nvml_gpu_name, nvml_vram_total_bytes
except Exception:

    def nvml_gpu_name() -> str | None:
        return None

    def nvml_vram_total_bytes() -> int | None:
        return None

_cache: list[dict] | None = None
_cache_ts = 0.0
SPECS_TTL_S = 45.0


def _wmi():
    try:
        import wmi  # type: ignore

        return wmi.WMI()
    except Exception:
        return None


def _clean_light(name: str) -> str:
    name = re.sub(r"\s+", " ", (name or "")).strip()
    name = name.replace("(R)", "").replace("(TM)", "").replace("(C)", "")
    return re.sub(r"\s+", " ", name).strip()


def _arch() -> str:
    m = (platform.machine() or "").lower()
    if m in ("amd64", "x86_64"):
        return "x64"
    if m in ("arm64", "aarch64"):
        return "ARM64"
    if m in ("x86", "i386", "i686"):
        return "x86"
    return platform.machine() or "?"


def _section_system(client) -> dict:
    mfr = model = host = ""
    if client:
        try:
            for cs in client.Win32_ComputerSystem():
                mfr = _clean_light(cs.Manufacturer or "")
                model = _clean_light(cs.Model or "")
                host = _clean_light(cs.Name or "")
                break
        except Exception:
            pass
    if not host:
        host = platform.node() or ""

    for junk in ("Inc.", "Incorporated", "Corporation", "Corp.", "Ltd.", "Co."):
        mfr = mfr.replace(junk, "").strip()

    if model and mfr and model.lower().startswith(mfr.lower()):
        identity = model
    elif mfr and model:
        identity = f"{mfr} {model}"
    else:
        identity = mfr or model or "PC"

    lines = [
        identity[:36],
        f"Host: {host}"[:36] if host else "",
        "",
    ]
    return {"title": "SYSTEM", "lines": lines}


def _section_os() -> dict:
    product = ""
    edition = ""
    display = ""
    build = 0
    ubr = 0

    try:
        import winreg

        key = winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Windows NT\CurrentVersion",
        )
        try:
            product = winreg.QueryValueEx(key, "ProductName")[0]
        except OSError:
            product = ""
        try:
            edition = winreg.QueryValueEx(key, "EditionID")[0]
        except OSError:
            edition = ""
        try:
            display = winreg.QueryValueEx(key, "DisplayVersion")[0]
        except OSError:
            display = ""
        try:
            build = int(winreg.QueryValueEx(key, "CurrentBuild")[0])
        except Exception:
            build = 0
        try:
            ubr = int(winreg.QueryValueEx(key, "UBR")[0])
        except Exception:
            ubr = 0
        winreg.CloseKey(key)
    except Exception:
        pass

    # ProductName often still says "Windows 10" on Win11 — use build threshold
    if build >= 22000:
        major = "Windows 11"
    elif "Windows 11" in (product or ""):
        major = "Windows 11"
    elif "Windows 10" in (product or ""):
        major = "Windows 10"
    elif product:
        major = product.replace("Windows ", "Win ")[:16]
    else:
        major = f"Windows {platform.release()}"

    # Edition: prefer EditionID mapped lightly
    ed = edition or ""
    if not ed and product:
        for token in ("Pro", "Home", "Enterprise", "Education", "SE"):
            if token in product:
                ed = token
                break
    ed_map = {
        "Professional": "Pro",
        "Core": "Home",
        "Enterprise": "Enterprise",
        "Education": "Education",
    }
    ed = ed_map.get(ed, ed)

    line0 = major
    if ed:
        line0 = f"{major} {ed}"
    if display:
        line0 = f"{line0} | {display}"
    line0 = line0[:36]

    build_s = f"Build {build}.{ubr}" if build else "Build ?"
    line1 = f"{build_s} | {_arch()}"[:36]

    return {"title": "OS", "lines": [line0, line1, ""]}


def _section_cpu(client) -> dict:
    name = ""
    cores = threads = 0
    max_mhz = 0

    if client:
        try:
            for cpu in client.Win32_Processor():
                name = _clean_light(cpu.Name or "")
                try:
                    cores = int(cpu.NumberOfCores or 0)
                except Exception:
                    cores = 0
                try:
                    threads = int(cpu.NumberOfLogicalProcessors or 0)
                except Exception:
                    threads = 0
                try:
                    max_mhz = int(cpu.MaxClockSpeed or 0)
                except Exception:
                    max_mhz = 0
                break
        except Exception:
            pass

    if not name:
        name = _clean_light(platform.processor() or "CPU")

    # Shorten common prefixes for line length, keep model identity
    name = re.sub(r"^\s*Intel\s+", "", name, flags=re.I)
    name = re.sub(r"^\s*AMD\s+", "", name, flags=re.I)
    name = re.sub(r"\s*CPU\s*", " ", name)
    name = re.sub(r"\s*@\s*.*$", "", name).strip()
    name = re.sub(r"\s+", " ", name)

    line0 = name[:36]
    parts = []
    if cores and threads:
        parts.append(f"{cores}C/{threads}T")
    elif cores:
        parts.append(f"{cores} cores")
    if max_mhz:
        parts.append(f"{max_mhz / 1000.0:.1f} GHz")
    line1 = " | ".join(parts)[:36] if parts else ""

    return {"title": "CPU", "lines": [line0, line1, ""]}


def _section_memory(client) -> dict:
    total = psutil.virtual_memory().total
    total_gb = round(total / (1024**3))
    line0 = f"{total_gb} GB total"

    sticks = []
    speeds = []
    if client:
        try:
            for mem in client.Win32_PhysicalMemory():
                try:
                    cap = int(mem.Capacity or 0)
                except Exception:
                    cap = 0
                if cap > 0:
                    sticks.append(round(cap / (1024**3)))
                try:
                    sp = int(mem.Speed or 0)
                    if sp > 0:
                        speeds.append(sp)
                except Exception:
                    pass
        except Exception:
            pass

    line1 = ""
    if sticks:
        # e.g. 2x8 GB
        from collections import Counter

        c = Counter(sticks)
        parts = [f"{n}x{sz} GB" for sz, n in sorted(c.items(), reverse=True)]
        line1 = " + ".join(parts)
        if speeds:
            # most common speed
            sp = Counter(speeds).most_common(1)[0][0]
            line1 = f"{line1} | {sp} MT/s"
        line1 = line1[:36]

    return {"title": "MEMORY", "lines": [line0, line1, ""]}


def _section_gpu() -> dict:
    name = nvml_gpu_name()
    vram_b = nvml_vram_total_bytes()
    vram_gb = round(vram_b / (1024**3)) if vram_b else None

    if not name:
        client = _wmi()
        best = ""
        best_ram = -1
        if client:
            try:
                for ctl in client.Win32_VideoController():
                    n = _clean_light(ctl.Name or "")
                    if not n or "Microsoft Basic" in n:
                        continue
                    try:
                        ram = int(ctl.AdapterRAM or 0)
                        # AdapterRAM is often signed 32-bit wrong for >2GB; still use for ranking
                        if ram < 0:
                            ram = ram & 0xFFFFFFFF
                    except Exception:
                        ram = 0
                    if ram >= best_ram:
                        best_ram = ram
                        best = n
            except Exception:
                pass
        name = best or "Unknown GPU"

    name = _clean_light(name)
    # Drop only redundant vendor prefix for length
    for prefix in ("NVIDIA GeForce ", "NVIDIA ", "AMD Radeon ", "AMD "):
        if name.upper().startswith(prefix.upper()):
            name = name[len(prefix) :].strip()
            break

    line0 = name[:36]
    line1 = f"VRAM {vram_gb} GB" if vram_gb is not None else ""
    return {"title": "GPU", "lines": [line0, line1, ""]}


def collect_spec_sections(force: bool = False) -> list[dict]:
    """Return list of {title, lines[3]} for protocol packing."""
    global _cache, _cache_ts
    now = time.monotonic()
    if not force and _cache and (now - _cache_ts) < SPECS_TTL_S:
        return _cache

    client = _wmi()
    sections = [
        _section_system(client),
        _section_os(),
        _section_cpu(client),
        _section_memory(client),
        _section_gpu(),
    ]
    # Normalize lines to 3
    for s in sections:
        lines = list(s.get("lines") or [])
        while len(lines) < 3:
            lines.append("")
        s["lines"] = [str(x)[:36] for x in lines[:3]]
        s["title"] = str(s.get("title") or "?")[:12]

    _cache = sections
    _cache_ts = now
    return sections


def collect_specs(force: bool = False) -> dict:
    """Legacy flat dict (tests / debug)."""
    sections = collect_spec_sections(force=force)
    flat = {}
    for s in sections:
        flat[s["title"].lower()] = " | ".join(x for x in s["lines"] if x)
    return flat
