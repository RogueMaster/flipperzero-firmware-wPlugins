"""Accumulating readings per sensor."""

from __future__ import annotations

import csv
import time
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

from .flipper_link import Reading

HISTORY_LIMIT = 2000

PRESSURE_UNITS = {
    "kPa": (1.0, 1),
    "bar": (0.01, 3),
    "PSI": (0.1450377, 1),
}


def convert_pressure(kpa: float, unit: str) -> float:
    factor, digits = PRESSURE_UNITS[unit]
    return round(kpa * factor, digits)


@dataclass
class SensorStats:
    """Everything known about a single sensor during the session."""

    sensor_id: str
    first_seen: float
    last_seen: float
    frames: int = 0
    last_reading: Reading | None = None
    min_pressure_kpa: float = field(default=float("inf"))
    max_pressure_kpa: float = field(default=float("-inf"))
    min_temperature_c: int = field(default=10**6)
    max_temperature_c: int = field(default=-(10**6))
    history: deque = field(default_factory=lambda: deque(maxlen=HISTORY_LIMIT))

    def update(self, reading: Reading) -> None:
        frame = reading.frame
        self.frames += 1
        self.last_seen = reading.host_time
        self.last_reading = reading

        # Not every protocol reports both, and a couple report neither.
        if frame.pressure_kpa is not None:
            self.min_pressure_kpa = min(self.min_pressure_kpa, frame.pressure_kpa)
            self.max_pressure_kpa = max(self.max_pressure_kpa, frame.pressure_kpa)
        if frame.temperature_c is not None:
            self.min_temperature_c = min(self.min_temperature_c, frame.temperature_c)
            self.max_temperature_c = max(self.max_temperature_c, frame.temperature_c)

        self.history.append(
            (reading.host_time, frame.pressure_kpa, frame.temperature_c)
        )

    @property
    def age_s(self) -> float:
        return max(0.0, time.time() - self.last_seen)


class SensorModel:
    """Readings of every sensor seen during the session."""

    def __init__(self) -> None:
        self._sensors: dict[str, SensorStats] = {}
        self._readings: list[Reading] = []

    def update(self, reading: Reading) -> SensorStats:
        sensor_id = reading.sensor_id
        stats = self._sensors.get(sensor_id)
        if stats is None:
            stats = SensorStats(
                sensor_id=sensor_id,
                first_seen=reading.host_time,
                last_seen=reading.host_time,
            )
            self._sensors[sensor_id] = stats

        stats.update(reading)
        self._readings.append(reading)
        return stats

    def sensors(self) -> list[SensorStats]:
        """Sensors in the order they first appeared."""
        return sorted(self._sensors.values(), key=lambda s: s.first_seen)

    def get(self, sensor_id: str) -> SensorStats | None:
        return self._sensors.get(sensor_id)

    @property
    def total_frames(self) -> int:
        return len(self._readings)

    def clear(self) -> None:
        self._sensors.clear()
        self._readings.clear()

    def export_csv(self, path: str | Path) -> int:
        """Export every reading. Returns the number of rows."""
        rows = 0
        with open(path, "w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(
                [
                    "timestamp_iso",
                    "host_time",
                    "device_tick",
                    "sensor_id",
                    "protocol",
                    "pressure_kpa",
                    "pressure_bar",
                    "pressure_psi",
                    "temperature_c",
                    "flags",
                    "rssi_dbm",
                    "raw",
                ]
            )
            for reading in self._readings:
                frame = reading.frame
                writer.writerow(
                    [
                        time.strftime(
                            "%Y-%m-%d %H:%M:%S", time.localtime(reading.host_time)
                        ),
                        f"{reading.host_time:.3f}",
                        reading.device_tick,
                        frame.id_hex,
                        frame.proto,
                        (
                            ""
                            if frame.pressure_kpa is None
                            else f"{frame.pressure_kpa:.2f}"
                        ),
                        (
                            ""
                            if frame.pressure_bar is None
                            else f"{frame.pressure_bar:.3f}"
                        ),
                        (
                            ""
                            if frame.pressure_psi is None
                            else f"{frame.pressure_psi:.2f}"
                        ),
                        "" if frame.temperature_c is None else frame.temperature_c,
                        frame.flags,
                        "" if reading.rssi_dbm is None else f"{reading.rssi_dbm:.1f}",
                        frame.raw_hex,
                    ]
                )
                rows += 1
        return rows

    def extend(self, readings: Iterable[Reading]) -> None:
        for reading in readings:
            self.update(reading)
