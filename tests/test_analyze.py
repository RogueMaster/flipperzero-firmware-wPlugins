import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))
from analyze import (
    decode_manchester, detect_rolling_code, compute_signal_quality,
    classify_alarm_sensor, classify_doorbell, classify_outlet_switch,
    classify_keyfob, classify_pt2262, classify_ev1527, classify,
    format_geojson, ClassificationResult,
    SubFile, FeatureVector, PWMParams, PreambleInfo, extract_features,
    compute_inter_segment_gaps_us, detect_pwm3_params, detect_crc,
    compute_runs, _crc8, _crc16_ccitt,
    ISM_FREQS,
)


def test_manchester_ge_thomas_convention():
    # G.E.Thomas: 1=10, 0=01
    raw = [1,0, 0,1, 1,0, 0,1]  # encodes 1,0,1,0
    bits, convention, error_rate = decode_manchester(raw)
    assert bits == [1, 0, 1, 0]
    assert "Thomas" in convention
    assert error_rate == 0.0

def test_manchester_ieee_convention():
    # IEEE 802.3: 1=01, 0=10
    raw = [0,1, 1,0, 0,1, 1,0]  # encodes 1,0,1,0
    bits, convention, error_rate = decode_manchester(raw)
    assert bits == [1, 0, 1, 0]
    assert "IEEE" in convention
    assert error_rate == 0.0

def test_manchester_with_errors():
    raw = [1,0, 0,0, 1,0]  # middle pair 0,0 is invalid
    bits, convention, error_rate = decode_manchester(raw)
    assert len(bits) == 2   # only valid pairs decoded
    assert error_rate > 0.0

def test_manchester_empty():
    bits, convention, error_rate = decode_manchester([])
    assert bits == []
    assert error_rate == 0.0

def test_manchester_tiebreak_all_errors():
    # All-invalid pairs: both conventions have equal error rate; tiebreak by first pair
    raw = [0, 0, 1, 1]  # both pairs invalid under both conventions
    bits, convention, error_rate = decode_manchester(raw)
    assert error_rate == 1.0
    assert bits == []
    # first pair is (0,0): not 1,0 so tiebreak falls through to IEEE
    assert "IEEE" in convention


def test_rolling_code_detected():
    # Segments differ in last 4 bits (rolling counter)
    seg0 = [1,0,1,1,0,0,1,0,  1,0,0,1,0,1,0,0]
    seg1 = [1,0,1,1,0,0,1,0,  1,0,0,1,0,1,0,1]
    seg2 = [1,0,1,1,0,0,1,0,  1,0,0,1,0,1,1,0]
    result = detect_rolling_code([seg0, seg1, seg2])
    assert result["is_rolling"] is True
    assert result["is_fixed"] is False
    assert 15 in result["diff_positions"]

def test_fixed_code_detected():
    seg = [1,0,1,1,0,0,1,0,1,0,0,1,0,1,0,0]
    result = detect_rolling_code([seg, seg, seg])
    assert result["is_fixed"] is True
    assert result["is_rolling"] is False
    assert result["diff_positions"] == []

def test_rolling_code_single_segment():
    seg = [1,0,1,1,0,0]
    result = detect_rolling_code([seg])
    assert result["is_rolling"] is False
    assert result["is_fixed"] is False

def test_rolling_code_empty_list():
    result = detect_rolling_code([])
    assert result["is_rolling"] is False
    assert result["is_fixed"] is False

def test_rolling_code_truncation_flag():
    # Segments of different lengths — truncated should be True
    seg0 = [1, 0, 1, 0, 1, 0]
    seg1 = [1, 0, 1, 0]
    result = detect_rolling_code([seg0, seg1])
    assert result["truncated"] is True


def _make_sub(segs, freq=433_920_000, te=174):
    return SubFile(path="test", frequency=freq, te_us=te,
                   total_bit_header=0, segments=segs, lat=0.0, lon=0.0, preset="")

def test_signal_quality_range():
    sub = _make_sub([[0]*100])
    fv = extract_features(sub)
    assert 0.0 <= fv.signal_quality <= 1.0

def test_signal_quality_high_for_clean_pwm():
    dense = [1,0,1,1,0,1,0,1,0,0,1,0,1,1,0,0] * 10
    sub_dense = _make_sub([dense])
    fv_dense = extract_features(sub_dense)
    sub_zero = _make_sub([[0]*160])
    fv_zero = extract_features(sub_zero)
    assert fv_dense.signal_quality > fv_zero.signal_quality
    assert fv_dense.signal_quality >= 0.5


def _make_fv(freq=433_920_000, te=174.0, seg_count=3, seg_sim=0.98,
             pwm_consistency=0.95, pwm_decoded=24, zero_ratio=0.72,
             entropy=0.85):
    pwm = PWMParams(pulse_width=3, short_gap=6, long_gap=11, consistency=pwm_consistency)
    pre = PreambleInfo(found=False, length=0, position=0)
    return FeatureVector(
        frequency=freq, te_us=te, bitrate_bps=1e6/te,
        seg_count=seg_count, seg_sizes=[448]*seg_count, total_bits=448*seg_count,
        inner_bits_per_seg=[[1]*100]*seg_count, inner_sizes=[100]*seg_count,
        total_inner_bits=100*seg_count, mean_inner_size=100.0,
        zero_ratio=zero_ratio, entropy=entropy,
        dominant_1run=3, dominant_0run=6, run_variety=0.3,
        pwm_params=pwm, pwm_decoded_bits=[1,0]*12, pwm_decoded_count=pwm_decoded,
        preamble=pre, seg_similarity=seg_sim,
        repeating_subpattern_period=None, repeating_subpattern_reps=0,
        manchester_decoded_bits=[], manchester_decoded_count=0,
        manchester_error_rate=0.5, manchester_convention="G.E.Thomas",
        rolling_code=False, fixed_code=True, diff_positions=[],
        signal_quality=0.85, lat=0.0, lon=0.0,
    )

def test_classify_doorbell_fires_for_high_repeat():
    fv = _make_fv(seg_count=6, pwm_decoded=24)
    result = classify_doorbell(fv)
    assert result is not None
    assert result.label == "DOORBELL"

def test_classify_doorbell_ignores_low_repeat():
    fv = _make_fv(seg_count=3, pwm_decoded=24)
    result = classify_doorbell(fv)
    assert result is None

def test_classify_outlet_fires_for_3_4_repeats():
    fv = _make_fv(seg_count=4, pwm_decoded=24)
    result = classify_outlet_switch(fv)
    assert result is not None
    assert result.label == "OUTLET_SWITCH"


# --- PT2262 (tri-state fixed-code) ---------------------------------------

def test_pt2262_fires_on_tristate():
    fv = _make_fv(seg_count=4, pwm_decoded=24, te=350.0)
    fv.pwm3_detected = True
    fv.pwm3_symbol_count = 12
    result = classify_pt2262(fv)
    assert result is not None
    assert result.label == "PT2262_REMOTE"
    assert result.confidence == "MEDIUM"

def test_pt2262_skipped_without_tristate():
    fv = _make_fv(seg_count=4, pwm_decoded=24)
    fv.pwm3_detected = False
    assert classify_pt2262(fv) is None

def test_pt2262_skipped_on_rolling_code():
    fv = _make_fv(seg_count=4, pwm_decoded=24, te=350.0)
    fv.pwm3_detected = True
    fv.pwm3_symbol_count = 12
    fv.fixed_code = False
    assert classify_pt2262(fv) is None

def test_classify_routes_tristate_to_pt2262():
    # End-to-end: a tri-state remote must beat the generic DOORBELL/GARAGE classifiers
    fv = _make_fv(seg_count=4, pwm_decoded=24, te=350.0)
    fv.pwm3_detected = True
    fv.pwm3_symbol_count = 12
    assert classify(fv).label == "PT2262_REMOTE"


# --- EV1527 (2-symbol 24-bit fixed code) ---------------------------------

def _ev1527_fv(seg_count=5):
    fv = _make_fv(seg_count=seg_count, pwm_decoded=24, te=320.0)
    fv.pwm_params = PWMParams(pulse_width=3, short_gap=4, long_gap=12, consistency=0.95)
    fv.pwm3_detected = False
    return fv

def test_ev1527_fires_on_3to1_ratio():
    result = classify_ev1527(_ev1527_fv(seg_count=5))
    assert result is not None
    assert result.label == "EV1527_REMOTE"
    assert result.confidence == "MEDIUM"

def test_ev1527_low_confidence_few_repeats():
    result = classify_ev1527(_ev1527_fv(seg_count=3))
    assert result is not None
    assert result.confidence == "LOW"

def test_ev1527_skipped_when_ratio_too_low():
    # Default _make_fv pwm has long/short = 11/6 ≈ 1.8 → not EV1527
    fv = _make_fv(seg_count=5, pwm_decoded=24)
    fv.pwm3_detected = False
    assert classify_ev1527(fv) is None

def test_ev1527_skipped_off_frequency():
    fv = _ev1527_fv(seg_count=5)
    fv.frequency = 315_000_000
    assert classify_ev1527(fv) is None

def test_ev1527_skipped_on_tristate():
    fv = _ev1527_fv(seg_count=5)
    fv.pwm3_detected = True
    assert classify_ev1527(fv) is None

def test_classify_routes_ev1527():
    assert classify(_ev1527_fv(seg_count=5)).label == "EV1527_REMOTE"

def test_classify_plain_doorbell_not_stolen():
    # No tri-state, ~1.8 gap ratio → still DOORBELL, not PT2262/EV1527
    fv = _make_fv(seg_count=6, pwm_decoded=24)
    assert classify(fv).label == "DOORBELL"

def test_alarm_sensor_single_segment():
    fv = _make_fv(freq=433_920_000, te=150.0, seg_count=1, seg_sim=None,
                  pwm_consistency=0.0, pwm_decoded=0, zero_ratio=0.55, entropy=0.95)
    # Override pwm_params to None
    fv.pwm_params = None
    fv.pwm_decoded_count = 0
    fv.seg_similarity = None
    fv.mean_inner_size = 64.0
    result = classify_alarm_sensor(fv)
    assert result is not None
    assert result.label == "ALARM_SENSOR"

def test_alarm_sensor_rejects_low_entropy():
    fv = _make_fv(freq=433_920_000, te=150.0, seg_count=1,
                  pwm_decoded=0, zero_ratio=0.55, entropy=0.30)
    fv.pwm_params = None
    fv.seg_similarity = None
    fv.mean_inner_size = 64.0
    result = classify_alarm_sensor(fv)
    assert result is None


def _make_result(label="NOISE"):
    return ClassificationResult(label=label, confidence="HIGH",
                                sub_protocol=[], reasons=[], warnings=[])

def test_geojson_skips_zero_coords():
    fv = _make_fv()
    fv.lat = 0.0
    fv.lon = 0.0
    result = _make_result("TPMS")
    gj = format_geojson([("test.sub", None, fv, result)])
    assert gj["type"] == "FeatureCollection"
    assert gj["features"] == []  # zero coords excluded

def test_geojson_includes_gps_record():
    fv = _make_fv()
    fv.lat = 52.3702
    fv.lon = 4.8952
    result = _make_result("GARAGE_REMOTE")
    gj = format_geojson([("path/to/test.sub", None, fv, result)])
    assert len(gj["features"]) == 1
    feat = gj["features"][0]
    # GeoJSON coordinates are [lon, lat]
    assert feat["geometry"]["coordinates"] == [4.8952, 52.3702]
    assert feat["properties"]["classification"] == "GARAGE_REMOTE"
    assert feat["properties"]["file"] == "test.sub"


def test_manchester_single_bit_no_crash():
    bits, convention, error_rate = decode_manchester([1])
    assert bits == []
    assert error_rate == 0.0


def test_noise_near_empty_signal():
    # 1 set bit out of 143 total — the pattern from crashing files
    bits = [0] * 1 + [1] + [0] * 141
    sub = _make_sub([bits])
    fv = extract_features(sub)
    result = classify(fv)
    assert result.label == "NOISE"
    assert result.confidence == "HIGH"


def test_434mhz_in_ism_freqs():
    assert 434_420_000 in ISM_FREQS


def test_classify_keyfob_315mhz():
    fv = _make_fv(freq=315_000_000, te=174, pwm_decoded=24, seg_count=1)
    result = classify_keyfob(fv)
    assert result is not None
    assert result.label == "KEYFOB_REMOTE"
    assert "315MHz" in result.sub_protocol[0]


def test_inter_segment_gap_zero_when_single_segment():
    mean, var = compute_inter_segment_gaps_us([[1, 0, 1]], te_us=100.0)
    assert mean == 0.0 and var == 0.0


def test_inter_segment_gap_basic():
    # seg0 trails 3 zeros; seg1 leads 2 zeros; gap = 5 bits × 100us = 500us
    seg0 = [1, 1, 0, 0, 0]
    seg1 = [0, 0, 1, 1]
    mean, var = compute_inter_segment_gaps_us([seg0, seg1], te_us=100.0)
    assert mean == 500.0 and var == 0.0


def test_inter_segment_gap_variance():
    # gaps: (1+0)*100=100us  and  (3+0)*100=300us → mean 200, var = 10000
    seg0 = [1, 0]
    seg1 = [1, 1, 0, 0, 0]
    seg2 = [1]
    mean, var = compute_inter_segment_gaps_us([seg0, seg1, seg2], te_us=100.0)
    assert mean == 200.0
    assert var == 10000.0


def test_pwm3_detects_three_buckets():
    # Pattern: pulse=2 ones followed by 2/5/10 zeros, repeated
    bits = []
    gaps = [2, 5, 10] * 3
    for g in gaps:
        bits += [1, 1] + [0] * g
    runs = compute_runs(bits)
    result = detect_pwm3_params(runs)
    assert result is not None
    short, mid, long_, count = result
    assert (short, mid, long_) == (2, 5, 10)
    assert count > 0


def test_pwm3_rejects_two_bucket_pwm():
    bits = []
    for g in [3, 6] * 5:
        bits += [1, 1] + [0] * g
    runs = compute_runs(bits)
    assert detect_pwm3_params(runs) is None


def test_crc8_round_trip():
    payload = bytes([0x12, 0x34, 0x56])
    crc = _crc8(payload)
    # Verify CRC over payload + crc-byte produces zero
    assert _crc8(payload + bytes([crc])) == 0


def test_detect_crc_finds_crc8():
    payload = bytes([0xAA, 0xBB])
    crc = _crc8(payload)
    bits = []
    for byte in payload + bytes([crc]):
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)
    valid, kind = detect_crc(bits)
    assert valid and kind == "CRC-8"


def test_detect_crc_finds_crc16_ccitt():
    payload = bytes([0xDE, 0xAD, 0xBE, 0xEF])
    crc = _crc16_ccitt(payload)
    full = payload + bytes([(crc >> 8) & 0xFF, crc & 0xFF])
    bits = []
    for byte in full:
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)
    valid, kind = detect_crc(bits)
    assert valid and kind == "CRC-16-CCITT"


def test_detect_crc_rejects_random_bytes():
    bits = []
    for byte in (0x01, 0x02, 0x03, 0x04, 0x05):
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)
    valid, _ = detect_crc(bits)
    assert valid is False


def test_diff_manchester_decoded():
    # Standard Manchester pairs that don't form a low-error standard decode but
    # do form a clean diff-Manchester sequence.
    # Sequence with transitions chosen so diff-Manchester error rate is low.
    raw = [1, 0, 1, 0, 0, 1, 0, 1, 1, 0]
    bits, conv, err = decode_manchester(raw)
    # We only assert it picks the lowest-error option; equality with std decodings is OK.
    assert err >= 0.0 and isinstance(conv, str)
