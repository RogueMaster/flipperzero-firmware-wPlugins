#!/usr/bin/env python3
"""
analyze.py — Classify Flipper Zero BinRAW .sub captures.
Usage:
    python3 analyze.py <file.sub>
    python3 analyze.py <directory/>
    python3 analyze.py <directory/> --summary-only
    python3 analyze.py <file.sub> --json
"""

import argparse
import json
import math
import os
import pathlib
from collections import Counter
from dataclasses import dataclass, field


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class SubFile:
    path: str
    frequency: int          # Hz (integer)
    te_us: int              # microseconds
    total_bit_header: int   # from "Bit:" header field (informational)
    segments: list          # list[list[int]], one list per Data_RAW line
    lat: float              # from Lat: header (0.0 if absent)
    lon: float              # from Lon: header (0.0 if absent)
    preset: str             # e.g. "FuriHalSubGhzPresetOok650Async"


@dataclass
class PWMParams:
    pulse_width: int        # dominant 1-run length (in TE units = raw bit counts)
    short_gap: int          # shorter 0-run
    long_gap: int           # longer 0-run
    consistency: float      # fraction of 1-runs matching dominant pulse_width


@dataclass
class PreambleInfo:
    found: bool
    length: int             # number of alternating bits
    position: int           # start index in inner bits


@dataclass
class FeatureVector:
    frequency: int
    te_us: float
    bitrate_bps: float
    seg_count: int
    seg_sizes: list         # list[int], raw bit counts per segment
    total_bits: int

    inner_bits_per_seg: list    # list[list[int]]
    inner_sizes: list           # list[int]
    total_inner_bits: int
    mean_inner_size: float

    zero_ratio: float       # computed on raw (all) bits across all segments
    entropy: float          # computed on inner bits across all segments

    dominant_1run: int
    dominant_0run: int
    run_variety: float      # unique run lengths / total runs

    pwm_params: object      # PWMParams or None
    pwm_decoded_bits: list  # list[int]
    pwm_decoded_count: int

    preamble: object        # PreambleInfo

    seg_similarity: object  # float or None (None if seg_count == 1)

    repeating_subpattern_period: object  # int or None
    repeating_subpattern_reps: int

    # Manchester decoding
    manchester_decoded_bits: list   # list[int], decoded from first segment inner bits
    manchester_decoded_count: int
    manchester_error_rate: float    # 0.0 = clean, 1.0 = all errors
    manchester_convention: str      # "G.E.Thomas" or "IEEE 802.3"

    # Rolling code analysis (requires 2+ segments with PWM decode)
    rolling_code: bool              # True if segments differ in consistent bit positions
    fixed_code: bool                # True if all decoded payloads are identical
    diff_positions: list            # list[int], bit positions that change across segments

    # Signal quality
    signal_quality: float           # 0.0–1.0 composite score

    # GPS (forwarded from SubFile)
    lat: float
    lon: float


@dataclass
class ClassificationResult:
    label: str              # NOISE / TPMS / GARAGE_REMOTE / WEATHER_STATION / KEYFOB_REMOTE / UNKNOWN_STRUCTURED
    confidence: str         # HIGH / MEDIUM / LOW
    sub_protocol: list      # list[str], informational hints
    reasons: list           # list[str], reasoning chain
    warnings: list          # list[str]


# ---------------------------------------------------------------------------
# Parsing layer
# ---------------------------------------------------------------------------

def _hex_to_bits(hex_str: str) -> list:
    """Convert hex string (space-separated) to MSB-first bit list."""
    raw_bytes = bytes.fromhex(hex_str.replace(" ", ""))
    bits = []
    for byte in raw_bytes:
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)
    return bits


def parse_sub_file(path: str) -> SubFile:
    """Parse a .sub file, keeping each Data_RAW line as a separate segment."""
    frequency = None
    te_us = None
    total_bit_header = None
    segments = []
    pending_bit_raw = None
    lat = 0.0
    lon = 0.0
    preset = ""

    with open(path, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith("Frequency:"):
                frequency = int(line.split(":", 1)[1].strip())
            elif line.startswith("TE:"):
                te_us = int(line.split(":", 1)[1].strip())
            elif line.startswith("Preset:"):
                preset = line.split(":", 1)[1].strip()
            elif line.startswith("Lat:"):
                try:
                    lat = float(line.split(":", 1)[1].strip())
                except ValueError:
                    pass
            elif line.startswith("Lon:"):
                try:
                    lon = float(line.split(":", 1)[1].strip())
                except ValueError:
                    pass
            elif line.startswith("Bit:") and not line.startswith("Bit_RAW:"):
                total_bit_header = int(line.split(":", 1)[1].strip())
            elif line.startswith("Bit_RAW:"):
                pending_bit_raw = int(line.split(":", 1)[1].strip())
            elif line.startswith("Data_RAW:"):
                hex_data = line.split(":", 1)[1].strip()
                bits = _hex_to_bits(hex_data)
                if pending_bit_raw is not None:
                    # Truncate to declared bit count (removes MSB padding of last byte)
                    bits = bits[:pending_bit_raw]
                    pending_bit_raw = None
                segments.append(bits)

    if frequency is None:
        raise ValueError(f"{path}: missing Frequency field")
    if te_us is None:
        raise ValueError(f"{path}: missing TE field")
    if not segments:
        raise ValueError(f"{path}: no Data_RAW found")

    return SubFile(
        path=path,
        frequency=frequency,
        te_us=te_us,
        total_bit_header=total_bit_header or 0,
        segments=segments,
        lat=lat,
        lon=lon,
        preset=preset,
    )


# ---------------------------------------------------------------------------
# Signal analysis helpers
# ---------------------------------------------------------------------------

def strip_padding(bits: list) -> list:
    """Remove leading and trailing zero bits."""
    if not bits:
        return []
    start = 0
    while start < len(bits) and bits[start] == 0:
        start += 1
    end = len(bits) - 1
    while end > start and bits[end] == 0:
        end -= 1
    if start > end:
        return []
    return bits[start:end + 1]


def compute_runs(bits: list) -> list:
    """Run-length encoding: returns [(value, length), ...]."""
    if not bits:
        return []
    runs = []
    current_val = bits[0]
    count = 1
    for b in bits[1:]:
        if b == current_val:
            count += 1
        else:
            runs.append((current_val, count))
            current_val = b
            count = 1
    runs.append((current_val, count))
    return runs


def compute_entropy(bits: list) -> float:
    """Shannon entropy normalized to [0,1]. Returns 0 for empty/trivial."""
    if not bits:
        return 0.0
    counts = Counter(bits)
    total = len(bits)
    entropy = 0.0
    for count in counts.values():
        p = count / total
        if p > 0:
            entropy -= p * math.log2(p)
    return entropy  # max is 1.0 for perfectly balanced binary


def compute_zero_ratio(bits: list) -> float:
    """Fraction of zero bits."""
    if not bits:
        return 1.0
    return bits.count(0) / len(bits)


def detect_pwm_params(runs: list) -> object:
    """
    Detect PWM encoding: uniform pulse width + 2 distinct gap lengths.
    Returns PWMParams or None.
    """
    if not runs:
        return None

    one_runs = [length for val, length in runs if val == 1]
    zero_runs = [length for val, length in runs if val == 0]

    if len(one_runs) < 4 or len(zero_runs) < 4:
        return None

    # Dominant 1-run
    one_counter = Counter(one_runs)
    dominant_pulse, dominant_count = one_counter.most_common(1)[0]
    pulse_consistency = dominant_count / len(one_runs)
    if pulse_consistency < 0.60:
        return None

    # Two dominant 0-runs
    zero_counter = Counter(zero_runs)
    top_two = zero_counter.most_common(2)
    if len(top_two) < 2:
        return None

    g1_len, g1_count = top_two[0]
    g2_len, g2_count = top_two[1]
    combined_frac = (g1_count + g2_count) / len(zero_runs)
    if combined_frac < 0.65:
        return None

    ratio = max(g1_len, g2_len) / max(min(g1_len, g2_len), 1)
    if ratio < 1.5 or ratio > 8.0:
        return None

    short_gap = min(g1_len, g2_len)
    long_gap = max(g1_len, g2_len)

    return PWMParams(
        pulse_width=dominant_pulse,
        short_gap=short_gap,
        long_gap=long_gap,
        consistency=pulse_consistency,
    )


def decode_pwm_bits(bits: list, pwm: object) -> list:
    """Decode PWM-encoded bits into logical bit stream using pulse/gap pairs."""
    if pwm is None:
        return []
    runs = compute_runs(bits)
    decoded = []
    i = 0
    while i < len(runs) - 1:
        val, length = runs[i]
        next_val, next_length = runs[i + 1]
        if val == 1 and next_val == 0:
            if next_length >= pwm.long_gap * 0.65:
                decoded.append(0)
            elif next_length <= pwm.short_gap * 1.5:
                decoded.append(1)
            # else: ambiguous symbol, skip
            i += 2
        else:
            i += 1
    return decoded


def decode_manchester(bits: list) -> tuple:
    """
    Decode Manchester-encoded bits. Tries both conventions, returns the better one.
    Returns (decoded_bits, convention_name, error_rate).
    Convention G.E.Thomas: 1=10, 0=01
    Convention IEEE 802.3: 1=01, 0=10
    Odd-length input: trailing bit is silently dropped (only complete pairs decoded).
    Invalid pairs (0,0 or 1,1) are counted as errors and not appended to output.
    """
    if len(bits) < 2:
        return [], "G.E.Thomas", 0.0

    def _try_convention(hi_is_one: bool):
        decoded = []
        errors = 0
        total = 0
        for i in range(0, len(bits) - 1, 2):
            a, b = bits[i], bits[i + 1]
            total += 1
            if a == 1 and b == 0:
                decoded.append(1 if hi_is_one else 0)
            elif a == 0 and b == 1:
                decoded.append(0 if hi_is_one else 1)
            else:
                # Invalid pair (0,0 or 1,1): not appended, counted as error
                errors += 1
        error_rate = errors / total  # total >= 1 guaranteed: len(bits) >= 2 checked above
        return decoded, error_rate

    decoded_a, err_a = _try_convention(True)   # G.E.Thomas: 10=1
    decoded_b, err_b = _try_convention(False)  # IEEE 802.3: 01=1

    if err_a < err_b:
        return decoded_a, "G.E.Thomas (1=high-low)", err_a
    if err_b < err_a:
        return decoded_b, "IEEE 802.3 (1=low-high)", err_b
    # tied: use first valid pair to pick convention
    if bits[0] == 1 and bits[1] == 0:
        return decoded_a, "G.E.Thomas (1=high-low)", err_a
    return decoded_b, "IEEE 802.3 (1=low-high)", err_b


def detect_rolling_code(decoded_segs: list) -> dict:
    """
    Compare PWM-decoded bit lists across segments.
    Returns dict with keys: is_rolling, is_fixed, diff_positions, truncated.
    truncated=True when segments differ in length (tail bits of longer segs ignored).
    decoded_segs: list of list[int], one per segment.
    """
    if not decoded_segs or len(decoded_segs) < 2:
        return {"is_rolling": False, "is_fixed": False, "diff_positions": [], "truncated": False}

    lengths = [len(s) for s in decoded_segs]
    min_len = min(lengths)
    truncated = len(set(lengths)) > 1

    if min_len == 0:
        return {"is_rolling": False, "is_fixed": False, "diff_positions": [], "truncated": truncated}

    diff_positions = []
    for pos in range(min_len):
        values = {s[pos] for s in decoded_segs}
        if len(values) > 1:
            diff_positions.append(pos)

    if not diff_positions:
        return {"is_rolling": False, "is_fixed": True, "diff_positions": [], "truncated": truncated}

    return {"is_rolling": True, "is_fixed": False, "diff_positions": diff_positions, "truncated": truncated}


def compute_signal_quality(fv: FeatureVector) -> float:
    """
    Composite signal quality score 0.0–1.0.
    Combines: inner_ratio, PWM consistency, segment similarity, entropy quality.
    """
    components = []

    # Inner content ratio: more signal vs silence = better quality
    inner_ratio = fv.total_inner_bits / max(fv.total_bits, 1)
    components.append(min(inner_ratio * 2.5, 1.0))

    # PWM consistency (if applicable)
    if fv.pwm_params is not None:
        components.append(fv.pwm_params.consistency)
    else:
        components.append(0.5)

    # Segment similarity (multi-segment captures)
    if fv.seg_similarity is not None:
        components.append(fv.seg_similarity)
    else:
        components.append(0.5)

    # Entropy quality (0.7-1.0 is good signal; very low = noise)
    if fv.entropy >= 0.70:
        components.append(1.0)
    elif fv.entropy >= 0.35:
        components.append(fv.entropy / 0.70)
    else:
        components.append(0.15)

    return round(sum(components) / len(components), 3)


def detect_preamble(bits: list) -> object:
    """Find longest alternating run of >= 8 bits."""
    if not bits:
        return PreambleInfo(found=False, length=0, position=0)

    best_start = 0
    best_len = 0
    current_start = 0
    current_len = 1

    for i in range(1, len(bits)):
        if bits[i] != bits[i - 1]:
            current_len += 1
        else:
            if current_len > best_len:
                best_len = current_len
                best_start = current_start
            current_start = i
            current_len = 1

    if current_len > best_len:
        best_len = current_len
        best_start = current_start

    found = best_len >= 8
    return PreambleInfo(
        found=found,
        length=best_len if found else 0,
        position=best_start if found else 0,
    )


def compute_segment_similarity(segs: list) -> object:
    """
    Mean pairwise Hamming similarity of all segments vs segment[0].
    Returns float [0,1] or None if only 1 segment.
    """
    if len(segs) < 2:
        return None

    ref = segs[0]
    similarities = []
    for other in segs[1:]:
        min_len = min(len(ref), len(other))
        if min_len == 0:
            continue
        mismatches = sum(a != b for a, b in zip(ref[:min_len], other[:min_len]))
        similarities.append(1.0 - mismatches / min_len)

    if not similarities:
        return None
    return sum(similarities) / len(similarities)


def find_repeating_subpattern(bits: list, min_len: int = 8, max_len: int = 128):
    """
    Find shortest period p where bits[:p] repeats >= 2 times covering >= 50% of stream.
    Returns (period, count) or (None, 0).
    """
    n = len(bits)
    if n < min_len * 2:
        return None, 0

    for period in range(min_len, min(max_len + 1, n // 2 + 1)):
        pattern = bits[:period]
        count = 1
        pos = period
        while pos + period <= n:
            if bits[pos:pos + period] == pattern:
                count += 1
                pos += period
            else:
                break
        if count >= 2 and (count * period) / n >= 0.50:
            return period, count

    return None, 0


# ---------------------------------------------------------------------------
# Feature extraction
# ---------------------------------------------------------------------------

def extract_features(sub: SubFile) -> FeatureVector:
    inner_bits_per_seg = [strip_padding(seg) for seg in sub.segments]
    inner_sizes = [len(inner) for inner in inner_bits_per_seg]
    total_inner_bits = sum(inner_sizes)
    mean_inner_size = total_inner_bits / len(inner_sizes) if inner_sizes else 0.0

    # Raw stats across ALL bits (including zero-padding)
    all_raw_bits = []
    for seg in sub.segments:
        all_raw_bits.extend(seg)
    total_bits = len(all_raw_bits)
    zero_ratio = compute_zero_ratio(all_raw_bits)

    # Entropy on inner bits only
    all_inner_bits = []
    for inner in inner_bits_per_seg:
        all_inner_bits.extend(inner)
    entropy = compute_entropy(all_inner_bits)

    # Run-length on all inner bits combined
    all_runs = compute_runs(all_inner_bits)
    one_runs_list = [length for val, length in all_runs if val == 1]
    zero_runs_list = [length for val, length in all_runs if val == 0]

    dominant_1run = Counter(one_runs_list).most_common(1)[0][0] if one_runs_list else 0
    dominant_0run = Counter(zero_runs_list).most_common(1)[0][0] if zero_runs_list else 0

    unique_run_lengths = len(set(l for _, l in all_runs))
    run_variety = unique_run_lengths / max(len(all_runs), 1)

    # PWM detection on all inner bits combined, but decode only the first segment
    # so pwm_decoded_count reflects one frame (not N repeats of the same frame)
    pwm_params = detect_pwm_params(all_runs)
    first_inner = inner_bits_per_seg[0] if inner_bits_per_seg else []
    pwm_decoded_bits = decode_pwm_bits(first_inner, pwm_params) if pwm_params else []
    pwm_decoded_count = len(pwm_decoded_bits)

    # Preamble on first segment inner bits
    preamble = (
        detect_preamble(inner_bits_per_seg[0])
        if inner_bits_per_seg
        else PreambleInfo(False, 0, 0)
    )

    seg_similarity = compute_segment_similarity(sub.segments)

    rep_period, rep_count = find_repeating_subpattern(all_inner_bits)

    # Manchester decoding (on first segment inner bits)
    if first_inner:
        manchester_bits, manchester_convention, manchester_error_rate = decode_manchester(first_inner)
    else:
        manchester_bits, manchester_convention, manchester_error_rate = [], "G.E.Thomas", 0.0
    manchester_decoded_count = len(manchester_bits)

    # Rolling code detection (PWM-decode each segment independently, then compare)
    if pwm_params is not None and len(sub.segments) >= 2:
        decoded_segs = [decode_pwm_bits(strip_padding(seg), pwm_params) for seg in sub.segments]
        rc_result = detect_rolling_code(decoded_segs)
    else:
        rc_result = {"is_rolling": False, "is_fixed": False, "diff_positions": [], "truncated": False}

    fv = FeatureVector(
        frequency=sub.frequency,
        te_us=float(sub.te_us),
        bitrate_bps=1e6 / sub.te_us if sub.te_us > 0 else 0.0,
        seg_count=len(sub.segments),
        seg_sizes=[len(seg) for seg in sub.segments],
        total_bits=total_bits,
        inner_bits_per_seg=inner_bits_per_seg,
        inner_sizes=inner_sizes,
        total_inner_bits=total_inner_bits,
        mean_inner_size=mean_inner_size,
        zero_ratio=zero_ratio,
        entropy=entropy,
        dominant_1run=dominant_1run,
        dominant_0run=dominant_0run,
        run_variety=run_variety,
        pwm_params=pwm_params,
        pwm_decoded_bits=pwm_decoded_bits,
        pwm_decoded_count=pwm_decoded_count,
        preamble=preamble,
        seg_similarity=seg_similarity,
        repeating_subpattern_period=rep_period,
        repeating_subpattern_reps=rep_count,
        manchester_decoded_bits=manchester_bits,
        manchester_decoded_count=manchester_decoded_count,
        manchester_error_rate=manchester_error_rate,
        manchester_convention=manchester_convention,
        rolling_code=rc_result["is_rolling"],
        fixed_code=rc_result["is_fixed"],
        diff_positions=rc_result["diff_positions"],
        signal_quality=0.0,
        lat=sub.lat,
        lon=sub.lon,
    )
    fv.signal_quality = compute_signal_quality(fv)
    return fv


# ---------------------------------------------------------------------------
# Classification layer
# ---------------------------------------------------------------------------

ISM_FREQS = {315_000_000, 433_420_000, 433_920_000, 434_420_000, 868_350_000, 915_000_000}
# 433.42MHz: Somfy RTS (shutters/blinds)
# 434.42MHz: EU ISM band upper region (433.05–434.79 MHz)
# 868.35MHz: EU ISM (alarms, LoRa-adjacent OOK)
# 915MHz: US ISM (some AMR meters, Z-Wave adjacent)


def classify_amr_meter(fv: FeatureVector) -> object:
    """
    Automatic Meter Reading (AMR/ERT): utility meters transmitting consumption data.
    US ERT: 315MHz OOK, Manchester encoded, long preamble, ~92 bytes payload.
    EU M-Bus: 868MHz.
    """
    if fv.frequency not in {315_000_000, 868_350_000}:
        return None

    score = 0
    reasons = []

    if fv.preamble.found and fv.preamble.length >= 16:
        score += 3
        reasons.append(f"[AMR1] {fv.preamble.length}-bit alternating preamble — AMR/ERT sync pattern")
    if fv.manchester_decoded_count >= 60 and fv.manchester_error_rate < 0.15:
        score += 3
        reasons.append(
            f"[AMR2] Manchester decode: {fv.manchester_decoded_count} bits,"
            f" {fv.manchester_error_rate:.0%} errors — ERT payload structure"
        )
    if fv.total_inner_bits >= 200:
        score += 2
        reasons.append(f"[AMR3] {fv.total_inner_bits} inner bits — AMR frames are 600–800 bits")
    if 40 <= fv.te_us <= 100:
        score += 1
        reasons.append(f"[AMR4] TE={fv.te_us:.0f}µs — fast OOK typical of ERT (≈50µs)")
    if fv.seg_count == 1:
        score += 1
        reasons.append("[AMR5] Single burst — ERT meters transmit once per interval")

    if score < 4:
        return None

    hints = []
    if fv.frequency == 315_000_000:
        hints.append("315MHz → US ERT (Encoder Receiver Transmitter) meter profile")
    else:
        hints.append("868MHz → EU M-Bus / wireless meter profile")
    if fv.manchester_decoded_count > 0:
        hints.append(f"Manchester payload: {fv.manchester_decoded_count} bits decoded")

    confidence = "MEDIUM" if score >= 7 else "LOW"

    return ClassificationResult(
        label="AMR_METER",
        confidence=confidence,
        sub_protocol=hints,
        reasons=reasons,
        warnings=["AMR identification requires CRC validation against ERT/IRTIS spec"],
    )


def classify_noise(fv: FeatureVector) -> object:
    all_inner = []
    for seg in fv.inner_bits_per_seg:
        all_inner.extend(seg)
    set_bits = sum(all_inner)

    # N0: near-empty signal — single glitch or squelch spike
    if set_bits <= 2:
        return ClassificationResult(
            label="NOISE",
            confidence="HIGH",
            sub_protocol=[],
            reasons=[f"[N0] Only {set_bits} set bit(s) — effectively empty capture"],
            warnings=[],
        )

    # N1: too few total bits
    if fv.total_bits < 50:
        return ClassificationResult(
            label="NOISE",
            confidence="HIGH",
            sub_protocol=[],
            reasons=[f"[N1] Only {fv.total_bits} bits total — too short to be a real signal"],
            warnings=[],
        )

    # N2: nearly all zeros with almost no signal
    if fv.zero_ratio > 0.97 and set_bits < 20:
        return ClassificationResult(
            label="NOISE",
            confidence="HIGH",
            sub_protocol=[],
            reasons=[
                f"[N2] zero_ratio={fv.zero_ratio:.1%}, only {set_bits} set bits"
                " — background noise or squelch artifact"
            ],
            warnings=[],
        )

    # N3: single segment, mostly zeros, very low entropy
    if fv.seg_count == 1 and fv.zero_ratio > 0.95 and fv.entropy < 0.25:
        return ClassificationResult(
            label="NOISE",
            confidence="HIGH",
            sub_protocol=[],
            reasons=[
                f"[N3] Single segment, zero_ratio={fv.zero_ratio:.1%} > 95%,"
                f" entropy={fv.entropy:.3f} < 0.25 — no meaningful signal content"
            ],
            warnings=[],
        )

    return None


def classify_tpms(fv: FeatureVector) -> object:
    if fv.frequency not in ISM_FREQS:
        return None

    score = 0
    reasons = []
    warnings = []

    # Disqualifiers
    if (fv.pwm_params is not None
            and fv.pwm_params.consistency > 0.85
            and fv.pwm_decoded_count < 80):
        return None  # clean PWM with short payload → likely remote, not TPMS
    if fv.seg_similarity is not None and fv.seg_similarity > 0.92:
        return None  # too identical → remote (TPMS rolling counters leave more variance)

    # Scoring
    if 50 <= fv.te_us <= 200:
        score += 2
        reasons.append(f"[T1] TE={fv.te_us:.0f}µs within TPMS timing range (50–200µs)")
    if 2 <= fv.seg_count <= 8:
        score += 2
        reasons.append(f"[T2] {fv.seg_count} segments — TPMS sensors repeat burst 3–8×")
    if 60 <= fv.mean_inner_size <= 200:
        score += 2
        reasons.append(
            f"[T3] Mean inner size {fv.mean_inner_size:.0f} bits"
            " — matches TPMS burst length (60–200)"
        )
    if fv.seg_similarity is not None and 0.70 <= fv.seg_similarity <= 0.99:
        score += 2
        reasons.append(
            f"[T4] Segment similarity {fv.seg_similarity:.1%}"
            " — similar but not identical (rolling counter expected)"
        )
    if 0.80 <= fv.zero_ratio <= 0.97:
        score += 1
        reasons.append(f"[T5] zero_ratio={fv.zero_ratio:.1%} — typical preamble/silence framing")
    if 0.35 <= fv.entropy <= 0.75:
        score += 1
        reasons.append(f"[T6] entropy={fv.entropy:.3f} — structured but not fully random")

    # Single-segment captures need higher score to be convincing TPMS
    min_score = 6 if fv.seg_count == 1 else 4
    if score < min_score:
        return None

    if fv.frequency == 315_000_000:
        hints = ["315MHz → North American TPMS standard"]
        confidence = "HIGH" if score >= 8 else "MEDIUM" if score >= 6 else "LOW"
    else:
        hints = ["433.92MHz → European TPMS standard"]
        confidence = "MEDIUM" if score >= 6 else "LOW"

    if fv.seg_count == 1:
        warnings.append(
            "Single segment — cannot confirm repeat burst pattern; may be partial capture"
        )

    return ClassificationResult(
        label="TPMS",
        confidence=confidence,
        sub_protocol=hints,
        reasons=reasons,
        warnings=warnings,
    )


def classify_alarm_sensor(fv: FeatureVector) -> object:
    """
    Home/car alarm sensor: Honeywell 5800, DSC, Visonic.
    433.92MHz, short single or dual bursts, high entropy, no clean PWM.
    """
    if fv.frequency not in {433_920_000, 868_350_000}:
        return None
    # No clean PWM — alarm sensors use Manchester or proprietary encoding
    if fv.pwm_params is not None and fv.pwm_params.consistency > 0.80:
        return None
    # Must be short single or dual burst (not a repeated remote)
    if fv.seg_count > 3:
        return None
    if fv.seg_similarity is not None and fv.seg_similarity > 0.92:
        return None  # too identical → this is a remote
    # High entropy is a hard requirement (encrypted payload)
    if fv.entropy < 0.90:
        return None

    score = 0
    reasons = []

    if 100 <= fv.te_us <= 400:
        score += 2
        reasons.append(f"[AS1] TE={fv.te_us:.0f}µs — alarm sensor OOK range (100–400µs)")
    if 40 <= fv.mean_inner_size <= 120:
        score += 2
        reasons.append(f"[AS2] Inner size {fv.mean_inner_size:.0f} bits — alarm payload range (40–120)")
    # AS3 always fires here: the entropy gate above guarantees entropy >= 0.90
    if fv.entropy >= 0.90:
        score += 2
        reasons.append(f"[AS3] entropy={fv.entropy:.3f} — high entropy consistent with encrypted payload")
    if 0.40 <= fv.zero_ratio <= 0.75:
        score += 1
        reasons.append(f"[AS4] zero_ratio={fv.zero_ratio:.1%} — dense signal data")
    if fv.seg_count <= 2:
        score += 1
        reasons.append(f"[AS5] {fv.seg_count} segment(s) — alarm sensors transmit 1–2× per event")

    if score < 7:
        return None

    confidence = "MEDIUM" if score >= 8 else "LOW"

    hints = []
    if 130 <= fv.te_us <= 170:
        hints.append("TE ~150µs → possible Honeywell 5800-series profile")
    elif 90 <= fv.te_us <= 130:
        hints.append("TE ~110µs → possible DSC/Visonic profile")
    if fv.manchester_error_rate < 0.20 and fv.manchester_decoded_count > 20:
        hints.append(f"Manchester decode successful ({fv.manchester_decoded_count} bits, "
                     f"{fv.manchester_error_rate:.0%} errors)")
    if fv.frequency == 868_350_000:
        hints.append("868MHz → European alarm sensor band")
    else:
        hints.append("433.92MHz → alarm sensor ISM band")

    return ClassificationResult(
        label="ALARM_SENSOR",
        confidence=confidence,
        sub_protocol=hints,
        reasons=reasons,
        warnings=["Cannot confirm alarm brand without protocol-specific CRC check"],
    )


def classify_shutter_blind(fv: FeatureVector) -> object:
    """
    Motorised blinds/shutters: Somfy RTS (433.42MHz), Nice, Faac.
    Note: Flipper tuned to 433.92MHz may capture 433.42MHz with reduced fidelity.
    """
    # Primary: 433.42MHz (Somfy RTS)
    # Secondary: 433.92MHz may catch it with ±500kHz tolerance
    if fv.frequency not in {433_420_000, 433_920_000}:
        return None
    if not (550 <= fv.te_us <= 700):
        return None  # Somfy RTS TE ≈ 604µs

    reasons = [
        f"[SB1] TE={fv.te_us:.0f}µs — matches Somfy RTS timing (TE≈604µs)",
    ]
    hints = []
    warnings = []

    if 50 <= fv.total_inner_bits <= 80:
        reasons.append(f"[SB2] {fv.total_inner_bits} inner bits — Somfy RTS 56-bit payload range")
    if fv.frequency == 433_420_000:
        hints.append("433.42MHz → Somfy RTS confirmed frequency")
    else:
        hints.append("433.92MHz capture — Somfy RTS is 433.42MHz; signal may be degraded")
        warnings.append("Frequency offset ±500kHz: confirm with Flipper tuned to 433.42MHz")

    if fv.seg_count >= 2:
        hints.append(f"{fv.seg_count} segments — Somfy typically transmits frame 2× with pause")

    confidence = "MEDIUM" if fv.frequency == 433_420_000 else "LOW"

    return ClassificationResult(
        label="SHUTTER_BLIND",
        confidence=confidence,
        sub_protocol=hints,
        reasons=reasons,
        warnings=warnings,
    )


def classify_doorbell(fv: FeatureVector) -> object:
    """Wireless doorbell: PT2262-family, 5–8 repeats, 24-bit fixed code."""
    if fv.frequency not in ISM_FREQS:
        return None
    if not (5 <= fv.seg_count <= 10):
        return None
    if fv.seg_similarity is None or fv.seg_similarity < 0.92:
        return None
    if fv.pwm_params is None or fv.pwm_params.consistency < 0.75:
        return None
    if not (16 <= fv.pwm_decoded_count <= 40):
        return None

    reasons = [
        f"[D1] {fv.seg_count} repeats — doorbells typically transmit 5–8× while button held",
        f"[D2] Segment similarity {fv.seg_similarity:.1%} — identical fixed code",
        f"[D3] {fv.pwm_decoded_count} decoded bits — PT2262 fixed-code family",
    ]
    if 100 <= fv.te_us <= 400:
        reasons.append(f"[D4] TE={fv.te_us:.0f}µs — OOK doorbell range")

    protocol = ["PT2262 fixed-code doorbell"]
    if fv.frequency == 433_920_000:
        protocol.append("433.92MHz → European/universal doorbell")
    else:
        protocol.append("315MHz → North American doorbell")

    return ClassificationResult(
        label="DOORBELL",
        confidence="MEDIUM",
        sub_protocol=protocol,
        reasons=reasons,
        warnings=[],
    )


def classify_outlet_switch(fv: FeatureVector) -> object:
    """Wireless power outlet / smart plug: PT2262-family, 3–4 repeats, 24-bit fixed code."""
    if fv.frequency not in ISM_FREQS:
        return None
    if not (3 <= fv.seg_count <= 4):
        return None
    if fv.seg_similarity is None or fv.seg_similarity < 0.97:
        return None  # outlet codes are perfectly identical
    if fv.pwm_params is None or fv.pwm_params.consistency < 0.80:
        return None
    # Outlet payloads are short fixed codes
    if not (24 <= fv.pwm_decoded_count <= 32):
        return None
    # Must be fixed code (not rolling)
    if fv.rolling_code:
        return None

    reasons = [
        f"[O1] {fv.seg_count} repeats — wireless outlets typically transmit 3–4×",
        f"[O2] Segment similarity {fv.seg_similarity:.1%} — perfectly identical (fixed code)",
        f"[O3] {fv.pwm_decoded_count} decoded bits — PT2262 24-bit fixed code",
        "[O4] No rolling code detected — consistent with static outlet address",
    ]

    return ClassificationResult(
        label="OUTLET_SWITCH",
        confidence="LOW",
        sub_protocol=["PT2262 wireless outlet/switch",
                      "433.92MHz ISM" if fv.frequency == 433_920_000 else "315MHz ISM"],
        reasons=reasons,
        warnings=["Cannot distinguish outlet from short garage/barrier remote without brand code database"],
    )


def classify_garage(fv: FeatureVector) -> object:
    if fv.frequency not in ISM_FREQS:
        return None

    reasons = []
    warnings = []
    sub_protocol = []

    # Required conditions
    if not (2 <= fv.seg_count <= 6):
        return None
    if fv.seg_similarity is None or fv.seg_similarity < 0.92:
        return None

    reasons.append(
        f"[G1] {fv.seg_count} segments — consistent with {fv.seg_count}× repeat transmission"
    )
    reasons.append(
        f"[G2] Segment similarity {fv.seg_similarity:.1%} — near-identical copies"
    )

    # Strong conditions
    has_pwm = fv.pwm_params is not None and fv.pwm_params.consistency >= 0.80
    strong_count = 0

    if has_pwm:
        strong_count += 1
        p = fv.pwm_params
        reasons.append(
            f"[G3] Clean PWM encoding detected"
            f" (pulse={p.pulse_width} TE, short_gap={p.short_gap} TE, long_gap={p.long_gap} TE)"
        )
        reasons.append(f"[G4] PWM consistency {p.consistency:.0%} — well-formed symbols")

    # Moderate conditions
    if 100 <= fv.te_us <= 400:
        reasons.append(f"[G5] TE={fv.te_us:.0f}µs within OOK remote timing range (100–400µs)")
    if has_pwm and 24 <= fv.pwm_decoded_count <= 80:
        reasons.append(
            f"[G6] {fv.pwm_decoded_count} decoded payload bits"
            " — consistent with remote frame size"
        )
    if 0.60 <= fv.zero_ratio <= 0.85:
        reasons.append(f"[G7] zero_ratio={fv.zero_ratio:.1%} — typical OOK framing overhead")

    # Confidence
    if strong_count >= 1:
        confidence = "HIGH"
    else:
        confidence = "LOW"
        warnings.append(
            "Near-identical repeats detected but no clean PWM found — encoding ambiguous"
        )

    # Sub-protocol hints

    # Rolling code vs fixed code annotation (before dc chain so it feeds into sub-protocol labels)
    if fv.rolling_code:
        reasons.append(
            f"[G8] ROLLING CODE (changes at bit positions: "
            f"{fv.diff_positions[:8]}{'...' if len(fv.diff_positions) > 8 else ''})"
        )
    elif fv.fixed_code and fv.seg_count >= 2:
        reasons.append("[G8] FIXED CODE — identical payload in all segments (potentially replayable)")
        warnings.append("Fixed code detected — this transmission may be vulnerable to replay attack")

    dc = fv.pwm_decoded_count
    if dc == 66 and fv.seg_count == 3:
        sub_protocol.append("KeeLoq rolling code (66-bit frame)")
    elif 50 <= dc <= 56 and 280 <= fv.te_us <= 380:
        sub_protocol.append("CAME 52-bit profile")
    elif 24 <= dc <= 40 and fv.fixed_code:
        sub_protocol.append("PT2262/generic fixed-code remote")
    elif 24 <= dc <= 40 and fv.rolling_code:
        sub_protocol.append("Generic rolling-code remote (short frame)")
    elif 24 <= dc <= 40:
        # Single-segment capture: rolling/fixed indeterminate
        sub_protocol.append("PT2262 family / short remote (code type indeterminate — need 2+ segments)")
    elif dc > 0:
        sub_protocol.append(f"Unrecognised frame ({dc} decoded bits)")

    if fv.frequency == 315_000_000:
        sub_protocol.append("315MHz → North American garage/barrier/car remote")
    else:
        sub_protocol.append("433.92MHz → European garage/CAME/Marantec/car remote")

    # Large segment warning
    if fv.seg_count > 1:
        mean_size = sum(fv.seg_sizes) / fv.seg_count
        for idx, size in enumerate(fv.seg_sizes):
            if size > mean_size * 3:
                warnings.append(
                    f"Segment {idx + 1} ({size} bits) is {size / mean_size:.1f}×"
                    " larger than average — may be multiple concatenated captures"
                )

    return ClassificationResult(
        label="GARAGE_REMOTE",
        confidence=confidence,
        sub_protocol=sub_protocol,
        reasons=reasons,
        warnings=warnings,
    )


def classify_weather(fv: FeatureVector) -> object:
    if fv.frequency != 433_920_000:
        return None
    # Hard gate: weather sensors use slow OOK; fast TE is not weather
    if not (150 <= fv.te_us <= 600):
        return None

    reasons = []
    warnings = []
    sub_protocol = []
    rules_passed = 0

    if 150 <= fv.te_us <= 600:
        rules_passed += 1
        reasons.append(f"[W1] TE={fv.te_us:.0f}µs — slow timing typical of weather sensor OOK")
    if 1 <= fv.seg_count <= 3:
        rules_passed += 1
        reasons.append(f"[W2] {fv.seg_count} segment(s) — weather stations send 1–2 repeats")
    if 80 <= fv.mean_inner_size <= 600:
        rules_passed += 1
        reasons.append(
            f"[W3] Mean inner size {fv.mean_inner_size:.0f} bits"
            " — matches weather payload length"
        )
    if 0.40 <= fv.zero_ratio <= 0.75:
        rules_passed += 1
        reasons.append(f"[W4] zero_ratio={fv.zero_ratio:.1%} — dense signal data")
    if fv.entropy >= 0.85:
        rules_passed += 1
        reasons.append(f"[W5] entropy={fv.entropy:.3f} — high information content (sensor data)")

    if fv.pwm_params is not None:
        rules_passed -= 1
        warnings.append(
            "Clean PWM detected — weather stations usually use Manchester or non-uniform OOK"
        )

    if rules_passed >= 5:
        confidence = "MEDIUM"
    elif rules_passed >= 3:
        confidence = "LOW"
    else:
        return None

    if 460 <= fv.te_us <= 520:
        sub_protocol.append("TE ~488µs → Oregon Scientific protocol")
    elif 180 <= fv.te_us <= 220:
        sub_protocol.append("TE ~200µs → AcuRite protocol")
    elif 300 <= fv.te_us <= 370:
        sub_protocol.append("TE ~336µs → generic 433MHz sensor (Lacrosse/similar)")

    return ClassificationResult(
        label="WEATHER_STATION",
        confidence=confidence,
        sub_protocol=sub_protocol,
        reasons=reasons,
        warnings=warnings,
    )


def classify_keyfob(fv: FeatureVector) -> object:
    if fv.frequency not in {315_000_000, 433_920_000}:
        return None

    reasons = []
    rules_passed = 0

    if 80 <= fv.te_us <= 500:
        rules_passed += 1
        reasons.append(f"[K1] TE={fv.te_us:.0f}µs within keyfob OOK range (80–500µs)")
    if fv.pwm_params is not None:
        rules_passed += 1
        reasons.append("[K2] PWM encoding detected")
    if 16 <= fv.pwm_decoded_count <= 48:
        rules_passed += 1
        reasons.append(f"[K3] {fv.pwm_decoded_count} decoded bits — short fixed code typical of keyfob")
    if fv.entropy >= 0.80:
        rules_passed += 1
        reasons.append(f"[K4] entropy={fv.entropy:.3f} — structured signal content")

    # PWM is required — without it, can't distinguish keyfob from weather/other OOK
    if fv.pwm_params is None:
        return None

    if rules_passed >= 4:
        confidence = "MEDIUM"
    elif rules_passed >= 2:
        confidence = "LOW"
    else:
        return None

    freq_hint = (
        "315MHz → North American car keyfob"
        if fv.frequency == 315_000_000
        else "433.92MHz → EU/Asian car keyfob / short-range remote"
    )
    return ClassificationResult(
        label="KEYFOB_REMOTE",
        confidence=confidence,
        sub_protocol=[freq_hint],
        reasons=reasons,
        warnings=[],
    )


def classify_unknown_structured(fv: FeatureVector) -> object:
    """Fallback classifier — always returns a result."""
    structured_indicators = 0
    if fv.entropy >= 0.70:
        structured_indicators += 1
    if fv.total_inner_bits >= 30:
        structured_indicators += 1
    if 0.30 <= fv.zero_ratio <= 0.90:
        structured_indicators += 1
    if fv.run_variety < 0.6:
        structured_indicators += 1

    if structured_indicators >= 3:
        reason = "Signal has structure but does not match any known device profile"
    else:
        reason = "Ambiguous: partial signal or unrecognised modulation"

    return ClassificationResult(
        label="UNKNOWN_STRUCTURED",
        confidence="LOW",
        sub_protocol=[],
        reasons=[f"[U1] {reason}"],
        warnings=[],
    )


def classify(fv: FeatureVector) -> ClassificationResult:
    """Run classifiers in priority order; return first match."""
    for fn in [
        classify_noise,
        classify_amr_meter,      # long preamble + Manchester → distinct early exit
        classify_tpms,
        classify_alarm_sensor,
        classify_shutter_blind,  # TE≈600µs is unique enough to run early
        classify_doorbell,
        classify_outlet_switch,
        classify_garage,
        classify_keyfob,
        classify_weather,
        classify_unknown_structured,
    ]:
        result = fn(fv)
        if result is not None:
            return result
    # Should never reach here
    return ClassificationResult(
        label="UNKNOWN_STRUCTURED",
        confidence="LOW",
        sub_protocol=[],
        reasons=["No classifier matched"],
        warnings=[],
    )


# ---------------------------------------------------------------------------
# Output formatting
# ---------------------------------------------------------------------------

def _freq_label(hz: int) -> str:
    if hz >= 1_000_000:
        mhz = hz / 1_000_000
        # Format cleanly: strip trailing zeros
        if mhz == int(mhz):
            return f"{int(mhz)} MHz"
        return f"{mhz:.3f} MHz".rstrip("0")
    return f"{hz} Hz"


def _bits_to_hex(bits: list) -> str:
    """Pack bits into bytes MSB-first, return hex string."""
    if not bits:
        return ""
    padded = bits + [0] * ((8 - len(bits) % 8) % 8)
    result = []
    for i in range(0, len(padded), 8):
        byte = 0
        for j in range(8):
            byte = (byte << 1) | padded[i + j]
        result.append(f"{byte:02X}")
    return " ".join(result)


def format_report(path: str, sub: SubFile, fv: FeatureVector, result: ClassificationResult) -> str:
    lines = []
    sep = "=" * 60

    lines.append(sep)
    lines.append(f"FILE: {os.path.basename(path)}")
    lines.append(sep)
    lines.append("")

    lines.append(f"CLASSIFICATION : {result.label}")
    lines.append(f"CONFIDENCE     : {result.confidence}")
    if result.sub_protocol:
        first = True
        for hint in result.sub_protocol:
            prefix = "SUB-PROTOCOL   :" if first else "               :"
            lines.append(f"{prefix} {hint}")
            first = False

    lines.append("")
    lines.append("KEY METRICS")
    lines.append(f"  Frequency    : {_freq_label(fv.frequency)}")
    lines.append(f"  TE           : {fv.te_us:.0f} µs  →  bitrate ≈ {fv.bitrate_bps:.0f} bps")

    seg_size_str = ", ".join(str(s) for s in fv.seg_sizes)
    lines.append(f"  Segments     : {fv.seg_count}  (sizes: {seg_size_str} bits)")

    if fv.seg_similarity is not None:
        lines.append(f"  Segment sim  : {fv.seg_similarity:.1%} identical")
    else:
        lines.append("  Segment sim  : n/a (single segment)")

    lines.append(
        f"  Inner bits   : {fv.total_inner_bits} total"
        f"  (mean {fv.mean_inner_size:.0f}/segment)"
    )
    lines.append(f"  Zero ratio   : {fv.zero_ratio:.1%} (of raw bytes)")
    lines.append(f"  Entropy      : {fv.entropy:.3f} bits/symbol")

    if fv.pwm_params is not None:
        p = fv.pwm_params
        lines.append(
            f"  PWM          : pulse={p.pulse_width} TE,"
            f" short_gap={p.short_gap} TE,"
            f" long_gap={p.long_gap} TE"
            f"  [consistency: {p.consistency:.0%}]"
        )
        lines.append(f"  Decoded bits : {fv.pwm_decoded_count}")
        if fv.pwm_decoded_bits:
            lines.append(f"  Payload bits : {''.join(str(b) for b in fv.pwm_decoded_bits)}")
            lines.append(f"  Payload hex  : {_bits_to_hex(fv.pwm_decoded_bits)}")
    else:
        lines.append("  PWM          : not detected")

    if fv.preamble.found:
        lines.append(
            f"  Preamble     : {fv.preamble.length}-bit alternating"
            f" at offset {fv.preamble.position}"
        )
    else:
        lines.append("  Preamble     : not detected")

    lines.append(f"  Signal qual  : {fv.signal_quality:.0%}")

    if fv.manchester_decoded_count > 0 and fv.manchester_error_rate < 0.30:
        lines.append(
            f"  Manchester   : {fv.manchester_decoded_count} bits decoded"
            f" [{fv.manchester_convention},"
            f" {fv.manchester_error_rate:.0%} errors]"
        )
        if fv.manchester_decoded_bits:
            mhex = _bits_to_hex(fv.manchester_decoded_bits)
            lines.append(f"  Manch. hex   : {mhex}")

    if fv.seg_count >= 2:
        if fv.rolling_code:
            lines.append(f"  Code type    : ROLLING (changes at {len(fv.diff_positions)} bit positions)")
        elif fv.fixed_code:
            lines.append(f"  Code type    : FIXED (identical across all {fv.seg_count} segments)")

    if fv.lat != 0.0 or fv.lon != 0.0:
        lines.append(f"  Location     : {fv.lat:.6f}, {fv.lon:.6f}")

    lines.append("")
    lines.append("REASONING CHAIN")
    for reason in result.reasons:
        lines.append(f"  {reason}")

    if result.warnings:
        lines.append("")
        lines.append("WARNINGS")
        for w in result.warnings:
            lines.append(f"  ! {w}")

    lines.append("")
    return "\n".join(lines)


def format_json(path: str, sub: SubFile, fv: FeatureVector, result: ClassificationResult) -> dict:
    return {
        "file": os.path.basename(path),
        "classification": result.label,
        "confidence": result.confidence,
        "sub_protocol": result.sub_protocol,
        "metrics": {
            "frequency_hz": fv.frequency,
            "te_us": fv.te_us,
            "bitrate_bps": round(fv.bitrate_bps),
            "seg_count": fv.seg_count,
            "seg_sizes": fv.seg_sizes,
            "total_bits": fv.total_bits,
            "total_inner_bits": fv.total_inner_bits,
            "mean_inner_size": round(fv.mean_inner_size, 1),
            "zero_ratio": round(fv.zero_ratio, 4),
            "entropy": round(fv.entropy, 4),
            "seg_similarity": (
                round(fv.seg_similarity, 4) if fv.seg_similarity is not None else None
            ),
            "pwm_decoded_count": fv.pwm_decoded_count,
            "pwm_decoded_bits": (
                "".join(str(b) for b in fv.pwm_decoded_bits) if fv.pwm_decoded_bits else None
            ),
            "pwm_decoded_hex": (
                _bits_to_hex(fv.pwm_decoded_bits) if fv.pwm_decoded_bits else None
            ),
            "signal_quality": round(fv.signal_quality, 4),
            "rolling_code": fv.rolling_code,
            "fixed_code": fv.fixed_code,
            "diff_positions": fv.diff_positions[:8],  # cap to match text report
            "manchester_decoded_count": fv.manchester_decoded_count,
            "manchester_decoded_hex": (
                _bits_to_hex(fv.manchester_decoded_bits)
                if fv.manchester_decoded_bits and fv.manchester_error_rate < 0.30
                else None
            ),
            "manchester_error_rate": round(fv.manchester_error_rate, 4),
            "manchester_convention": fv.manchester_convention if fv.manchester_decoded_count > 0 else None,
            "lat": fv.lat,
            "lon": fv.lon,
        },
        "reasoning": result.reasons,
        "warnings": result.warnings,
    }


def format_geojson(records: list) -> dict:
    """
    Build a GeoJSON FeatureCollection from a list of (path, sub, fv, result) tuples.
    Only includes records with non-zero GPS coordinates.
    """
    features = []
    for path, sub, fv, result in records:
        if fv.lat == 0.0 and fv.lon == 0.0:
            continue
        feature = {
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [fv.lon, fv.lat],  # GeoJSON is [lon, lat]
            },
            "properties": {
                "file": os.path.basename(path),
                "classification": result.label,
                "confidence": result.confidence,
                "sub_protocol": result.sub_protocol,
                "frequency_hz": fv.frequency,
                "te_us": fv.te_us,
                "signal_quality": round(fv.signal_quality, 4),
                "rolling_code": fv.rolling_code,
                "fixed_code": fv.fixed_code,
                "pwm_decoded_hex": _bits_to_hex(fv.pwm_decoded_bits) if fv.pwm_decoded_bits else None,
            },
        }
        features.append(feature)

    return {
        "type": "FeatureCollection",
        "features": features,
    }


# ---------------------------------------------------------------------------
# Runners
# ---------------------------------------------------------------------------

def run_single(path: pathlib.Path, json_mode: bool = False, db=None) -> tuple:
    """Process a single file. Returns (label, confidence, error_or_None)."""
    try:
        sub = parse_sub_file(str(path))
        fv = extract_features(sub)
        result = classify(fv)
        if db is not None:
            db.add_capture(
                filename=path.name,
                frequency=fv.frequency,
                te_us=fv.te_us,
                classification=result.label,
                confidence=result.confidence,
                lat=fv.lat,
                lon=fv.lon,
                payload_hex=_bits_to_hex(fv.pwm_decoded_bits),
                signal_quality=fv.signal_quality,
                sub_protocol="; ".join(result.sub_protocol),
            )
        if json_mode:
            print(json.dumps(format_json(str(path), sub, fv, result), indent=2))
        else:
            print(format_report(str(path), sub, fv, result))
        return result.label, result.confidence, None
    except Exception as e:
        msg = f"ERROR: {e}"
        if not json_mode:
            print(f"\n{'=' * 60}\nFILE: {path.name}\n{'=' * 60}\n{msg}\n")
        return "ERROR", "—", str(e)


def run_batch(
    directory: pathlib.Path,
    json_mode: bool = False,
    summary_only: bool = False,
    geojson_out: pathlib.Path = None,
    db=None,
) -> None:
    sub_files = sorted(directory.glob("*.sub"))
    if not sub_files:
        print(f"No .sub files found in {directory}")
        return

    results = []
    batch_json = []
    records = []

    for path in sub_files:
        try:
            sub = parse_sub_file(str(path))
            fv = extract_features(sub)
            result = classify(fv)
            records.append((str(path), sub, fv, result))

            if db is not None:
                db.add_capture(
                    filename=path.name,
                    frequency=fv.frequency,
                    te_us=fv.te_us,
                    classification=result.label,
                    confidence=result.confidence,
                    lat=fv.lat,
                    lon=fv.lon,
                    payload_hex=_bits_to_hex(fv.pwm_decoded_bits),
                    signal_quality=fv.signal_quality,
                    sub_protocol="; ".join(result.sub_protocol),
                )
            if json_mode:
                batch_json.append(format_json(str(path), sub, fv, result))
            elif not summary_only:
                print(format_report(str(path), sub, fv, result))

            results.append((path.name, result.label, result.confidence))
        except Exception as e:
            results.append((path.name, "ERROR", "—"))
            if not summary_only and not json_mode:
                print(f"ERROR processing {path.name}: {e}\n")

    if json_mode:
        print(json.dumps(batch_json, indent=2))
        if geojson_out is not None:
            gj = format_geojson(records)
            with open(geojson_out, "w") as f:
                json.dump(gj, f, indent=2)
            print(f"GeoJSON written to {geojson_out} ({len(gj['features'])} geolocated captures)")
        return

    # Summary table
    print(f"\nBATCH SUMMARY  ({len(sub_files)} files)")
    print("-" * 72)
    print(f"{'FILE':<45} {'CLASS':<22} CONF")
    print("-" * 72)
    for fname, label, conf in results:
        print(f"{fname:<45} {label:<22} {conf}")
    print()

    if geojson_out is not None:
        gj = format_geojson(records)
        with open(geojson_out, "w") as f:
            json.dump(gj, f, indent=2)
        geo_count = len(gj["features"])
        print(f"GeoJSON written to {geojson_out} ({geo_count} geolocated captures)")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Classify Flipper Zero BinRAW .sub captures"
    )
    parser.add_argument("target", nargs="?", help=".sub file or directory containing .sub files")
    parser.add_argument(
        "--json", action="store_true", help="output JSON instead of text report"
    )
    parser.add_argument(
        "--summary-only",
        action="store_true",
        help="batch mode: print only the summary table, no per-file reports",
    )
    parser.add_argument(
        "--geojson",
        metavar="OUTPUT.geojson",
        help="write GeoJSON FeatureCollection of geolocated captures to this file",
    )
    parser.add_argument(
        "--db",
        metavar="SESSION.db",
        help="append all classified captures to this SQLite wardrive database",
    )
    parser.add_argument(
        "--db-summary",
        metavar="SESSION.db",
        help="print summary statistics from an existing wardrive database and exit",
    )
    args = parser.parse_args()

    if args.db_summary:
        from wardrive_db import WardriveDB
        db = WardriveDB(args.db_summary)
        db.print_summary()
        db.close()
        return

    if args.target is None:
        parser.error("target is required unless --db-summary is used")

    db = None
    if args.db:
        from wardrive_db import WardriveDB
        db = WardriveDB(args.db)

    target = pathlib.Path(args.target)
    if target.is_dir():
        run_batch(
            target,
            json_mode=args.json,
            summary_only=args.summary_only,
            geojson_out=pathlib.Path(args.geojson) if args.geojson else None,
            db=db,
        )
    elif target.is_file() and target.suffix == ".sub":
        run_single(target, json_mode=args.json, db=db)
    else:
        parser.error(f"Not a .sub file or directory: {args.target}")

    if db is not None:
        db.close()


if __name__ == "__main__":
    main()
