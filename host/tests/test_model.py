"""Tests for accumulating readings."""

from __future__ import annotations

import csv

from tpms.decoder import build_frame, parse_frame
from tpms.flipper_link import Reading
from tpms.model import SensorModel, convert_pressure


def _reading(sensor_id: int, kpa: float, temp: int, host_time: float) -> Reading:
    frame = parse_frame(build_frame(sensor_id, kpa, temp))
    assert frame is not None
    return Reading(host_time=host_time, device_tick=int(host_time * 1000), frame=frame, rssi_dbm=-62.5)


def test_model_groups_by_sensor():
    model = SensorModel()
    model.update(_reading(0x111111, 200.25, 20, 1000.0))
    model.update(_reading(0x111111, 210.0, 21, 1001.0))
    model.update(_reading(0x222222, 180.0, 19, 1002.0))

    sensors = model.sensors()
    assert [s.sensor_id for s in sensors] == ["111111", "222222"]
    assert sensors[0].frames == 2
    assert model.total_frames == 3


def test_model_tracks_min_max():
    model = SensorModel()
    model.update(_reading(0x111111, 200.25, 20, 1000.0))
    model.update(_reading(0x111111, 249.75, 25, 1001.0))
    model.update(_reading(0x111111, 150.0, 15, 1002.0))

    stats = model.get("111111")
    assert stats is not None
    assert stats.min_pressure_kpa == 150.0
    assert stats.max_pressure_kpa == 249.75
    assert stats.min_temperature_c == 15
    assert stats.max_temperature_c == 25
    assert len(stats.history) == 3


def test_export_csv(tmp_path):
    model = SensorModel()
    model.update(_reading(0x7AD779, 243.75, 22, 1000.0))
    model.update(_reading(0x7AD779, 243.75, 23, 1001.0))

    path = tmp_path / "out.csv"
    assert model.export_csv(path) == 2

    with open(path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    assert len(rows) == 2
    assert rows[0]["sensor_id"] == "7ad779"
    assert rows[0]["pressure_kpa"] == "243.75"
    assert rows[0]["temperature_c"] == "22"
    assert rows[0]["rssi_dbm"] == "-62.5"


def test_clear_resets_everything():
    model = SensorModel()
    model.update(_reading(0x111111, 200.25, 20, 1000.0))
    model.clear()
    assert model.sensors() == []
    assert model.total_frames == 0


def test_pressure_conversion():
    assert convert_pressure(243.75, "kPa") == 243.8
    assert convert_pressure(243.75, "bar") == 2.438
    assert convert_pressure(243.75, "PSI") == 35.4
