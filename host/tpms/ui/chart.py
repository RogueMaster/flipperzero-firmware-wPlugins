"""Pressure and temperature over time.

Drawn by hand with QPainter: one dependency less, and all that is needed
here are two polylines with labelled axes.
"""

from __future__ import annotations

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QWidget

from ..model import PRESSURE_UNITS, convert_pressure

PRESSURE_COLOR = QColor(64, 132, 226)
TEMPERATURE_COLOR = QColor(226, 138, 60)

MARGIN_LEFT = 52
MARGIN_RIGHT = 52
MARGIN_TOP = 22
MARGIN_BOTTOM = 26


class HistoryChart(QWidget):
    """History of one sensor: pressure (left axis), temperature (right)."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setMinimumHeight(180)
        self._points: list[tuple[float, float, int]] = []
        self._unit = "kPa"
        self._title = ""

    def set_history(
        self,
        points: list[tuple[float, float, int]],
        unit: str = "kPa",
        title: str = "",
    ) -> None:
        self._points = points
        self._unit = unit if unit in PRESSURE_UNITS else "kPa"
        self._title = title
        self.update()

    def clear(self) -> None:
        self.set_history([], self._unit, "")

    # ------------------------------------------------------------------

    def paintEvent(self, event) -> None:  # noqa: N802 — the name comes from Qt
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        palette = self.palette()
        text_color = palette.windowText().color()
        grid_color = QColor(text_color)
        grid_color.setAlpha(40)

        plot = QRectF(
            MARGIN_LEFT,
            MARGIN_TOP,
            max(1.0, self.width() - MARGIN_LEFT - MARGIN_RIGHT),
            max(1.0, self.height() - MARGIN_TOP - MARGIN_BOTTOM),
        )

        painter.setPen(QPen(grid_color, 1))
        painter.drawRect(plot)

        font = QFont(self.font())
        font.setPointSizeF(max(8.0, font.pointSizeF() - 1))
        painter.setFont(font)

        if len(self._points) < 2:
            painter.setPen(QPen(text_color, 1))
            painter.drawText(plot, Qt.AlignCenter, "Недостаточно данных для графика")
            return

        times = [p[0] for p in self._points]
        pressures = [convert_pressure(p[1], self._unit) for p in self._points]
        temperatures = [float(p[2]) for p in self._points]

        t_min, t_max = min(times), max(times)
        if t_max - t_min < 1e-6:
            t_max = t_min + 1.0

        p_lo, p_hi = _padded_range(pressures)
        t_lo, t_hi = _padded_range(temperatures)

        # Grid and axis labels.
        painter.setPen(QPen(grid_color, 1, Qt.DotLine))
        for i in range(1, 4):
            y = plot.top() + plot.height() * i / 4
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y))

        painter.setPen(QPen(text_color, 1))
        for i in range(5):
            fraction = i / 4
            y = plot.bottom() - plot.height() * fraction

            pressure_label = f"{p_lo + (p_hi - p_lo) * fraction:.{PRESSURE_UNITS[self._unit][1]}f}"
            painter.drawText(
                QRectF(0, y - 8, MARGIN_LEFT - 6, 16),
                Qt.AlignRight | Qt.AlignVCenter,
                pressure_label,
            )

            # Over a narrow range, labels without decimals all come out the same.
            temperature_digits = 1 if (t_hi - t_lo) < 5 else 0
            temperature_label = f"{t_lo + (t_hi - t_lo) * fraction:.{temperature_digits}f}"
            painter.drawText(
                QRectF(plot.right() + 6, y - 8, MARGIN_RIGHT - 8, 16),
                Qt.AlignLeft | Qt.AlignVCenter,
                temperature_label,
            )

        span = t_max - t_min
        painter.drawText(
            QRectF(plot.left(), plot.bottom() + 4, plot.width(), MARGIN_BOTTOM - 6),
            Qt.AlignLeft | Qt.AlignTop,
            f"-{span:.0f} с",
        )
        painter.drawText(
            QRectF(plot.left(), plot.bottom() + 4, plot.width(), MARGIN_BOTTOM - 6),
            Qt.AlignRight | Qt.AlignTop,
            "сейчас",
        )

        header_rect = QRectF(plot.left(), 2, plot.width(), MARGIN_TOP - 4)
        painter.drawText(header_rect, Qt.AlignLeft | Qt.AlignVCenter, self._title or "История")

        # Legend: two labels in a row, widths measured with the font
        # metrics, otherwise they run into each other.
        metrics = painter.fontMetrics()
        pressure_legend = f"давление, {self._unit}"
        temperature_legend = "температура, °C"
        gap = 12.0

        temperature_width = metrics.horizontalAdvance(temperature_legend)
        pressure_width = metrics.horizontalAdvance(pressure_legend)

        temperature_rect = QRectF(
            header_rect.right() - temperature_width, header_rect.top(), temperature_width, header_rect.height()
        )
        pressure_rect = QRectF(
            temperature_rect.left() - gap - pressure_width,
            header_rect.top(),
            pressure_width,
            header_rect.height(),
        )

        painter.setPen(QPen(PRESSURE_COLOR, 1))
        painter.drawText(pressure_rect, Qt.AlignRight | Qt.AlignVCenter, pressure_legend)
        painter.setPen(QPen(TEMPERATURE_COLOR, 1))
        painter.drawText(temperature_rect, Qt.AlignRight | Qt.AlignVCenter, temperature_legend)

        def to_points(values: list[float], lo: float, hi: float) -> list[QPointF]:
            scale = (hi - lo) or 1.0
            return [
                QPointF(
                    plot.left() + plot.width() * (time - t_min) / (t_max - t_min),
                    plot.bottom() - plot.height() * (value - lo) / scale,
                )
                for time, value in zip(times, values)
            ]

        painter.setPen(QPen(TEMPERATURE_COLOR, 1.5))
        painter.drawPolyline(to_points(temperatures, t_lo, t_hi))

        painter.setPen(QPen(PRESSURE_COLOR, 2))
        painter.drawPolyline(to_points(pressures, p_lo, p_hi))


def _padded_range(values: list[float]) -> tuple[float, float]:
    lo, hi = min(values), max(values)
    if hi - lo < 1e-9:
        pad = max(abs(hi) * 0.05, 1.0)
        return lo - pad, hi + pad
    pad = (hi - lo) * 0.1
    return lo - pad, hi + pad
