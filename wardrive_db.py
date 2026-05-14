"""
wardrive_db.py — SQLite session database for wardrive captures.
"""

import math
import sqlite3
from collections import Counter
from datetime import datetime, timezone


class WardriveDB:
    CREATE_SQL = """
    CREATE TABLE IF NOT EXISTS captures (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        captured_at   TEXT NOT NULL,
        filename      TEXT NOT NULL,
        frequency     INTEGER NOT NULL,
        te_us         REAL NOT NULL,
        classification TEXT NOT NULL,
        confidence    TEXT NOT NULL,
        lat           REAL NOT NULL DEFAULT 0.0,
        lon           REAL NOT NULL DEFAULT 0.0,
        payload_hex   TEXT NOT NULL DEFAULT '',
        signal_quality REAL NOT NULL DEFAULT 0.0,
        sub_protocol  TEXT NOT NULL DEFAULT ''
    );
    """

    def __init__(self, db_path: str):
        self.db_path = db_path
        self._conn = sqlite3.connect(db_path)
        self._conn.row_factory = sqlite3.Row
        self._conn.execute(self.CREATE_SQL)
        self._conn.commit()

    def add_capture(
        self,
        filename: str,
        frequency: int,
        te_us: float,
        classification: str,
        confidence: str,
        lat: float,
        lon: float,
        payload_hex: str,
        signal_quality: float,
        sub_protocol: str,
    ) -> int:
        """Insert a capture record. Returns the new row id."""
        now = datetime.now(timezone.utc).isoformat()
        cur = self._conn.execute(
            """INSERT INTO captures
               (captured_at, filename, frequency, te_us, classification,
                confidence, lat, lon, payload_hex, signal_quality, sub_protocol)
               VALUES (?,?,?,?,?,?,?,?,?,?,?)""",
            (now, filename, frequency, te_us, classification,
             confidence, lat, lon, payload_hex, signal_quality, sub_protocol),
        )
        self._conn.commit()
        return cur.lastrowid

    def get_all(self) -> list:
        """Return all captures as list of dicts."""
        rows = self._conn.execute(
            "SELECT * FROM captures ORDER BY captured_at DESC"
        ).fetchall()
        return [dict(r) for r in rows]

    def summary_stats(self) -> dict:
        """Return totals and per-class counts."""
        rows = self.get_all()
        by_class = {}
        for r in rows:
            cls = r["classification"]
            by_class[cls] = by_class.get(cls, 0) + 1
        return {"total": len(rows), "by_class": by_class}

    def export_geojson(self) -> dict:
        """Return GeoJSON FeatureCollection; skips rows with lat=0 AND lon=0."""
        features = []
        for r in self.get_all():
            if r["lat"] == 0.0 and r["lon"] == 0.0:
                continue
            features.append({
                "type": "Feature",
                "geometry": {
                    "type": "Point",
                    "coordinates": [r["lon"], r["lat"]],
                },
                "properties": {
                    "file": r["filename"],
                    "classification": r["classification"],
                    "confidence": r["confidence"],
                    "frequency_hz": r["frequency"],
                    "te_us": r["te_us"],
                    "signal_quality": r["signal_quality"],
                    "sub_protocol": r["sub_protocol"],
                    "payload_hex": r["payload_hex"],
                    "captured_at": r["captured_at"],
                },
            })
        return {"type": "FeatureCollection", "features": features}

    def print_summary(self) -> None:
        stats = self.summary_stats()
        print(f"\nWARDRIVE SESSION SUMMARY  ({stats['total']} captures)")
        print("-" * 40)
        for cls, count in sorted(stats["by_class"].items(), key=lambda x: -x[1]):
            print(f"  {cls:<25} {count}")
        print()

    def cluster_by_location(self, radius_m: float) -> dict:
        """Single-linkage greedy clustering of geolocated captures.
        Returns GeoJSON FeatureCollection with one Point per cluster.
        Properties: count, dominant_class, classifications (dict)."""
        rows = [r for r in self.get_all() if not (r["lat"] == 0.0 and r["lon"] == 0.0)]
        if not rows:
            return {"type": "FeatureCollection", "features": []}

        clusters: list = []
        for r in rows:
            placed = False
            for c in clusters:
                if _haversine_m(r["lat"], r["lon"], c["lat"], c["lon"]) <= radius_m:
                    n = c["count"]
                    c["lat"] = (c["lat"] * n + r["lat"]) / (n + 1)
                    c["lon"] = (c["lon"] * n + r["lon"]) / (n + 1)
                    c["count"] = n + 1
                    c["classes"][r["classification"]] += 1
                    placed = True
                    break
            if not placed:
                clusters.append({
                    "lat": r["lat"],
                    "lon": r["lon"],
                    "count": 1,
                    "classes": Counter({r["classification"]: 1}),
                })

        features = []
        for c in clusters:
            dominant = c["classes"].most_common(1)[0][0]
            features.append({
                "type": "Feature",
                "geometry": {"type": "Point", "coordinates": [c["lon"], c["lat"]]},
                "properties": {
                    "count": c["count"],
                    "dominant_class": dominant,
                    "classifications": dict(c["classes"]),
                    "cluster_radius_m": radius_m,
                },
            })
        return {"type": "FeatureCollection", "features": features}

    def close(self) -> None:
        self._conn.close()


def _haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """Great-circle distance in metres between two WGS84 coordinates."""
    r = 6_371_000.0
    p1 = math.radians(lat1)
    p2 = math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))
