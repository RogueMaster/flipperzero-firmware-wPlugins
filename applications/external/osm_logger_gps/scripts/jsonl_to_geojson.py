#!/usr/bin/env python3
import json, sys

features = []
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    obj = json.loads(line)
    lat, lon = obj.get("lat"), obj.get("lon")
    props = {k: v for k, v in obj.items() if k not in {"lat", "lon"}}
    features.append({
        "type": "Feature",
        "geometry": {"type": "Point", "coordinates": [lon, lat]},
        "properties": props,
    })

out = {"type": "FeatureCollection", "features": features}
print(json.dumps(out, ensure_ascii=False))
