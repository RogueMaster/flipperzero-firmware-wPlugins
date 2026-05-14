import sys, pathlib, tempfile, os
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))
from wardrive_db import WardriveDB

def test_create_and_insert():
    with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
        db_path = f.name
    try:
        db = WardriveDB(db_path)
        db.add_capture(
            filename="test.sub",
            frequency=433_920_000,
            te_us=174.0,
            classification="GARAGE_REMOTE",
            confidence="HIGH",
            lat=52.3702,
            lon=4.8952,
            payload_hex="25 6F 31 0D",
            signal_quality=0.88,
            sub_protocol="PT2262 fixed-code",
        )
        rows = db.get_all()
        assert len(rows) == 1
        assert rows[0]["classification"] == "GARAGE_REMOTE"
        assert rows[0]["frequency"] == 433_920_000
    finally:
        os.unlink(db_path)

def test_geojson_export_skips_zero_coords():
    with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
        db_path = f.name
    try:
        db = WardriveDB(db_path)
        db.add_capture("a.sub", 315_000_000, 97.0, "TPMS", "HIGH",
                       lat=0.0, lon=0.0, payload_hex="", signal_quality=0.5, sub_protocol="")
        db.add_capture("b.sub", 433_920_000, 174.0, "GARAGE_REMOTE", "HIGH",
                       lat=52.3702, lon=4.8952, payload_hex="AB CD", signal_quality=0.9, sub_protocol="")
        gj = db.export_geojson()
        assert gj["type"] == "FeatureCollection"
        assert len(gj["features"]) == 1  # only b.sub has real coords
        assert gj["features"][0]["properties"]["classification"] == "GARAGE_REMOTE"
    finally:
        os.unlink(db_path)

def test_summary_stats():
    with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
        db_path = f.name
    try:
        db = WardriveDB(db_path)
        for cls in ["NOISE", "TPMS", "TPMS", "GARAGE_REMOTE"]:
            db.add_capture("x.sub", 433_920_000, 100.0, cls, "HIGH",
                           0.0, 0.0, "", 0.5, "")
        stats = db.summary_stats()
        assert stats["total"] == 4
        assert stats["by_class"]["TPMS"] == 2
        assert stats["by_class"]["NOISE"] == 1
    finally:
        os.unlink(db_path)


def test_cluster_by_location_merges_nearby():
    with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
        db_path = f.name
    try:
        db = WardriveDB(db_path)
        # Three captures within ~20m of each other in Amsterdam, plus one in NYC
        db.add_capture("a.sub", 433_920_000, 174.0, "GARAGE_REMOTE", "HIGH",
                       52.3702, 4.8952, "", 0.9, "")
        db.add_capture("b.sub", 433_920_000, 174.0, "GARAGE_REMOTE", "HIGH",
                       52.37021, 4.89521, "", 0.9, "")
        db.add_capture("c.sub", 433_920_000, 174.0, "TPMS", "HIGH",
                       52.37022, 4.89523, "", 0.9, "")
        db.add_capture("d.sub", 315_000_000, 100.0, "TPMS", "HIGH",
                       40.7128, -74.0060, "", 0.9, "")
        gj = db.cluster_by_location(radius_m=100.0)
        assert gj["type"] == "FeatureCollection"
        assert len(gj["features"]) == 2
        counts = sorted(f["properties"]["count"] for f in gj["features"])
        assert counts == [1, 3]
        big = next(f for f in gj["features"] if f["properties"]["count"] == 3)
        assert big["properties"]["dominant_class"] == "GARAGE_REMOTE"
    finally:
        os.unlink(db_path)


def test_cluster_by_location_skips_zero_coords():
    with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
        db_path = f.name
    try:
        db = WardriveDB(db_path)
        db.add_capture("a.sub", 433_920_000, 174.0, "GARAGE_REMOTE", "HIGH",
                       0.0, 0.0, "", 0.9, "")
        gj = db.cluster_by_location(radius_m=50.0)
        assert gj["features"] == []
    finally:
        os.unlink(db_path)
