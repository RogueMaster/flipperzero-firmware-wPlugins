"""UI smoke tests: the window builds, the table and chart survive data.

They run in Qt's offscreen mode, so no window appears on screen.
"""

from __future__ import annotations

import os
import time

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

pytest.importorskip("PySide6")

from PySide6.QtWidgets import QApplication  # noqa: E402

from tpms.decoder import build_frame, parse_frame  # noqa: E402
from tpms.flipper_link import Reading  # noqa: E402
from tpms.ui.main_window import MainWindow  # noqa: E402


@pytest.fixture(scope="module")
def qt_app():
    app = QApplication.instance() or QApplication([])
    yield app


@pytest.fixture
def window(qt_app):
    window = MainWindow()
    yield window
    window.close()


def _reading(sensor_id: int, kpa: float, temp: int, offset: float = 0.0) -> Reading:
    frame = parse_frame(build_frame(sensor_id, kpa, temp))
    assert frame is not None
    return Reading(
        host_time=time.time() + offset,
        device_tick=1000,
        frame=frame,
        rssi_dbm=-62.5,
    )


def _feed(window: MainWindow, reading: Reading) -> None:
    window.model.update(reading)
    window._update_sensor_row(reading)


def test_window_starts_empty(window):
    assert window.table.rowCount() == 0
    assert window.model.total_frames == 0


def test_readings_fill_table(window):
    _feed(window, _reading(0x7AD779, 243.75, 22))
    _feed(window, _reading(0x7AD779, 245.25, 23, offset=1))
    _feed(window, _reading(0x0AB1C2, 180.0, 18, offset=2))

    assert window.table.rowCount() == 2
    assert window.table.item(0, 0).text() == "7ad779"
    assert window.table.item(0, 1).text() == "245.2 kPa"
    assert window.table.item(0, 2).text() == "23 °C"
    assert window.table.item(0, 5).text() == "2"
    assert window.table.item(1, 0).text() == "0ab1c2"


def test_unit_switch_rewrites_pressure(window):
    _feed(window, _reading(0x7AD779, 243.75, 22))

    window.unit_combo.setCurrentText("bar")
    assert window.table.item(0, 1).text() == "2.438 bar"

    window.unit_combo.setCurrentText("PSI")
    assert window.table.item(0, 1).text() == "35.4 PSI"

    window.unit_combo.setCurrentText("kPa")
    assert window.table.item(0, 1).text() == "243.8 kPa"


def test_chart_renders_with_and_without_data(window):
    # An empty chart must render too.
    window.chart.grab()

    for index in range(10):
        _feed(window, _reading(0x7AD779, 240.0 + index, 20 + index, offset=index))

    window.table.selectRow(0)
    window._refresh_chart()
    pixmap = window.chart.grab()
    assert not pixmap.isNull()


def test_ages_refresh_marks_stale_sensor(window):
    _feed(window, _reading(0x7AD779, 243.75, 22, offset=-300))
    window._refresh_ages()
    assert "назад" in window.table.item(0, 6).text()


def test_clear_empties_everything(window):
    _feed(window, _reading(0x7AD779, 243.75, 22))
    window._clear()

    assert window.table.rowCount() == 0
    assert window.model.total_frames == 0
    assert window.summary_label.text() == "Кадров: 0   Датчиков: 0"


def test_export_csv_from_window_model(window, tmp_path):
    _feed(window, _reading(0x7AD779, 243.75, 22))
    path = tmp_path / "out.csv"
    assert window.model.export_csv(path) == 1
    assert "7ad779" in path.read_text(encoding="utf-8")


def test_open_capture_decodes_file(window, tmp_path):
    from tpms.decoder import chips_to_timings, frame_to_chips

    raw = build_frame(0x123456, 220.5, 21)
    timings = chips_to_timings(frame_to_chips(raw))

    path = tmp_path / "capture.txt"
    path.write_text(" ".join(str(v) for v in timings), encoding="utf-8")

    # Bypass the file dialog by calling the parsing path directly.
    from tpms.sub_file import load

    frames = load(path).decode()
    assert [f.raw for f in frames] == [raw]

    for frame in frames:
        _feed(window, Reading(host_time=time.time(), device_tick=0, frame=frame, rssi_dbm=None))

    assert window.table.item(0, 0).text() == "123456"
