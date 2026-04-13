import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))
from analyze import (
    decode_manchester, detect_rolling_code, compute_signal_quality,
    classify_alarm_sensor, classify_doorbell, classify_outlet_switch,
    classify_keyfob, classify,
    format_geojson, ClassificationResult,
    SubFile, FeatureVector, PWMParams, PreambleInfo, extract_features,
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
