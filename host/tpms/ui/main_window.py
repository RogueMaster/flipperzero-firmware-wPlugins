"""Главное окно приложения."""

from __future__ import annotations

import queue
import time
from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QComboBox,
    QFileDialog,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from ..flipper_link import DEFAULT_FREQUENCY, FlipperLink, Reading, find_flipper_ports
from ..model import PRESSURE_UNITS, SensorModel, convert_pressure
from ..sub_file import load as load_capture
from .chart import HistoryChart

COLUMNS = ["ID датчика", "Давление", "Температура", "Флаги", "RSSI", "Кадров", "Последний приём"]

LOG_LIMIT = 2000
POLL_INTERVAL_MS = 150
AGE_INTERVAL_MS = 1000

STALE_AFTER_S = 60.0


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("TPMS Renault — Flipper Zero")
        self.resize(980, 720)

        self.model = SensorModel()
        self.link: FlipperLink | None = None
        self._rows: dict[str, int] = {}

        self._build_ui()

        self._poll_timer = QTimer(self)
        self._poll_timer.timeout.connect(self._poll_events)
        self._poll_timer.start(POLL_INTERVAL_MS)

        self._age_timer = QTimer(self)
        self._age_timer.timeout.connect(self._refresh_ages)
        self._age_timer.start(AGE_INTERVAL_MS)

        self._refresh_ports()

    # ------------------------------------------------------------------
    # Сборка интерфейса
    # ------------------------------------------------------------------

    def _build_ui(self) -> None:
        central = QWidget()
        layout = QVBoxLayout(central)

        layout.addLayout(self._build_connection_bar())

        splitter = QSplitter(Qt.Vertical)

        self.table = QTableWidget(0, len(COLUMNS))
        self.table.setHorizontalHeaderLabels(COLUMNS)
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.table.verticalHeader().setVisible(False)
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.itemSelectionChanged.connect(self._refresh_chart)
        splitter.addWidget(self.table)

        self.chart = HistoryChart()
        splitter.addWidget(self.chart)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(LOG_LIMIT)
        self.log.setPlaceholderText("Сообщения устройства и сырые данные")
        splitter.addWidget(self.log)

        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 3)
        splitter.setStretchFactor(2, 2)
        layout.addWidget(splitter)

        layout.addLayout(self._build_action_bar())
        self.setCentralWidget(central)

        self.status = self.statusBar()
        self.status.showMessage("Не подключено")

    def _build_connection_bar(self) -> QHBoxLayout:
        bar = QHBoxLayout()

        bar.addWidget(QLabel("Порт:"))
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(240)
        self.port_combo.setEditable(True)
        bar.addWidget(self.port_combo)

        self.refresh_button = QPushButton("Обновить")
        self.refresh_button.clicked.connect(self._refresh_ports)
        bar.addWidget(self.refresh_button)

        bar.addWidget(QLabel("Частота, Гц:"))
        self.frequency_spin = QSpinBox()
        self.frequency_spin.setRange(300_000_000, 928_000_000)
        self.frequency_spin.setSingleStep(10_000)
        self.frequency_spin.setValue(DEFAULT_FREQUENCY)
        # Разделитель разрядов в локали рисуется точкой и частота начинает
        # читаться как дробное число — лучше без него.
        self.frequency_spin.setGroupSeparatorShown(False)
        bar.addWidget(self.frequency_spin)

        bar.addWidget(QLabel("Режим:"))
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(["json", "raw"])
        self.mode_combo.setToolTip(
            "json — декодированные кадры.\n"
            "raw — сырые тайминги для диагностики, если кадры не ловятся."
        )
        bar.addWidget(self.mode_combo)

        self.wake_check = QCheckBox("Будить")
        self.wake_check.setToolTip(
            "Периодически излучать поле 125 кГц, чтобы разбудить датчик.\n"
            "Датчик надо держать вплотную к задней стороне Flipper — там катушка.\n"
            "В покое датчик молчит и выходит в эфир только от движения колеса."
        )
        bar.addWidget(self.wake_check)

        self.connect_button = QPushButton("Подключить")
        self.connect_button.clicked.connect(self._toggle_connection)
        bar.addWidget(self.connect_button)

        bar.addStretch(1)

        bar.addWidget(QLabel("Единицы:"))
        self.unit_combo = QComboBox()
        self.unit_combo.addItems(list(PRESSURE_UNITS))
        self.unit_combo.currentTextChanged.connect(self._on_unit_changed)
        bar.addWidget(self.unit_combo)

        return bar

    def _build_action_bar(self) -> QHBoxLayout:
        bar = QHBoxLayout()

        self.export_button = QPushButton("Экспорт CSV…")
        self.export_button.clicked.connect(self._export_csv)
        bar.addWidget(self.export_button)

        self.save_log_button = QPushButton("Сохранить лог…")
        self.save_log_button.clicked.connect(self._save_log)
        bar.addWidget(self.save_log_button)

        self.open_button = QPushButton("Открыть .sub / дамп…")
        self.open_button.clicked.connect(self._open_capture)
        bar.addWidget(self.open_button)

        self.clear_button = QPushButton("Очистить")
        self.clear_button.clicked.connect(self._clear)
        bar.addWidget(self.clear_button)

        bar.addStretch(1)

        self.summary_label = QLabel("Кадров: 0   Датчиков: 0")
        bar.addWidget(self.summary_label)

        return bar

    # ------------------------------------------------------------------
    # Подключение
    # ------------------------------------------------------------------

    def _refresh_ports(self) -> None:
        current = self.port_combo.currentText()
        ports = find_flipper_ports()

        self.port_combo.clear()
        self.port_combo.addItems(ports)

        if current and current in ports:
            self.port_combo.setCurrentText(current)
        elif not ports:
            self._append_log("Flipper не найден. Подключите его по USB и нажмите «Обновить».")

    def _toggle_connection(self) -> None:
        if self.link is not None and self.link.running:
            self._disconnect()
        else:
            self._connect()

    def _connect(self) -> None:
        port = self.port_combo.currentText().strip()
        if not port:
            QMessageBox.warning(self, "Нет порта", "Укажите последовательный порт Flipper.")
            return

        self.link = FlipperLink(
            port=port,
            frequency=self.frequency_spin.value(),
            mode=self.mode_combo.currentText(),
            wake=self.wake_check.isChecked(),
        )
        self.link.start()

        self.connect_button.setText("Отключить")
        self.status.showMessage(f"Подключено к {port}")
        self._append_log(
            f"Подключение к {port}. На Flipper должно быть запущено приложение TPMS Bridge."
        )

    def _disconnect(self) -> None:
        if self.link is not None:
            self.link.stop()
            self.link = None
        self.connect_button.setText("Подключить")
        self.status.showMessage("Отключено")

    # ------------------------------------------------------------------
    # События
    # ------------------------------------------------------------------

    def _poll_events(self) -> None:
        if self.link is None:
            return

        updated = False
        while True:
            try:
                kind, payload = self.link.events.get_nowait()
            except queue.Empty:
                break

            if kind == "reading":
                self.model.update(payload)
                self._update_sensor_row(payload)
                updated = True
            elif kind == "raw":
                self._append_log(str(payload))
            elif kind == "error":
                self._append_log(f"ОШИБКА: {payload}")
                self.status.showMessage(f"Ошибка: {payload}")
            elif kind == "closed":
                self._append_log("Соединение закрыто.")
                self.connect_button.setText("Подключить")
            else:
                self._append_log(str(payload))

        if updated:
            self._refresh_summary()
            self._refresh_chart()

    def _update_sensor_row(self, reading: Reading) -> None:
        sensor_id = reading.sensor_id
        stats = self.model.get(sensor_id)
        if stats is None:
            return

        row = self._rows.get(sensor_id)
        if row is None:
            row = self.table.rowCount()
            self.table.insertRow(row)
            self._rows[sensor_id] = row
            for column in range(len(COLUMNS)):
                self.table.setItem(row, column, QTableWidgetItem(""))
            if self.table.currentRow() < 0:
                self.table.selectRow(row)

        unit = self.unit_combo.currentText()
        frame = reading.frame

        values = [
            sensor_id,
            f"{convert_pressure(frame.pressure_kpa, unit)} {unit}",
            f"{frame.temperature_c} °C",
            f"0x{frame.flags:02x}",
            "—" if reading.rssi_dbm is None else f"{reading.rssi_dbm:.1f} dBm",
            str(stats.frames),
            "только что",
        ]
        for column, value in enumerate(values):
            self.table.item(row, column).setText(value)

    def _refresh_ages(self) -> None:
        now = time.time()
        for sensor_id, row in self._rows.items():
            stats = self.model.get(sensor_id)
            if stats is None:
                continue

            age = now - stats.last_seen
            item = self.table.item(row, len(COLUMNS) - 1)
            item.setText("только что" if age < 2 else f"{int(age)} с назад")

            # Молчащий датчик подсвечиваем, чтобы не спутать со свежим.
            stale = age > STALE_AFTER_S
            for column in range(len(COLUMNS)):
                cell = self.table.item(row, column)
                cell.setForeground(
                    QColor(150, 150, 150) if stale else self.table.palette().text().color()
                )

    def _refresh_summary(self) -> None:
        self.summary_label.setText(
            f"Кадров: {self.model.total_frames}   Датчиков: {len(self.model.sensors())}"
        )

    def _refresh_chart(self) -> None:
        sensor_id = self._selected_sensor()
        if sensor_id is None:
            self.chart.clear()
            return

        stats = self.model.get(sensor_id)
        if stats is None or not stats.history:
            self.chart.clear()
            return

        self.chart.set_history(
            list(stats.history),
            unit=self.unit_combo.currentText(),
            title=f"Датчик {sensor_id}",
        )

    def _selected_sensor(self) -> str | None:
        row = self.table.currentRow()
        if row < 0:
            return None
        item = self.table.item(row, 0)
        return item.text() if item else None

    def _on_unit_changed(self) -> None:
        unit = self.unit_combo.currentText()
        for sensor_id, row in self._rows.items():
            stats = self.model.get(sensor_id)
            if stats is None or stats.last_reading is None:
                continue
            pressure = stats.last_reading.frame.pressure_kpa
            self.table.item(row, 1).setText(f"{convert_pressure(pressure, unit)} {unit}")
        self._refresh_chart()

    # ------------------------------------------------------------------
    # Действия
    # ------------------------------------------------------------------

    def _export_csv(self) -> None:
        if not self.model.total_frames:
            QMessageBox.information(self, "Нечего выгружать", "Пока не принято ни одного кадра.")
            return

        path, _ = QFileDialog.getSaveFileName(
            self, "Сохранить показания", "tpms.csv", "CSV (*.csv)"
        )
        if not path:
            return

        rows = self.model.export_csv(path)
        self.status.showMessage(f"Записано строк: {rows} → {path}")

    def _save_log(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Сохранить лог", "tpms_raw.txt", "Текст (*.txt)"
        )
        if not path:
            return

        Path(path).write_text(self.log.toPlainText(), encoding="utf-8")
        self.status.showMessage(f"Лог сохранён → {path}")

    def _open_capture(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Открыть запись",
            "",
            "Записи (*.sub *.txt *.log);;Все файлы (*)",
        )
        if not path:
            return

        try:
            capture = load_capture(path)
        except OSError as exc:
            QMessageBox.critical(self, "Не удалось прочитать", str(exc))
            return

        frames = capture.decode()
        self._append_log(
            f"Файл {Path(path).name}: таймингов {len(capture.timings)}, кадров {len(frames)}"
        )

        now = time.time()
        for index, frame in enumerate(frames):
            reading = Reading(
                host_time=now + index * 0.001,
                device_tick=0,
                frame=frame,
                rssi_dbm=None,
            )
            self.model.update(reading)
            self._update_sensor_row(reading)

        if not frames:
            QMessageBox.information(
                self,
                "Кадров нет",
                "В файле не нашлось валидных кадров Renault.\n\n"
                "Если это запись из Sub-GHz → Read RAW, то так и будет: прошивка "
                "выбрасывает импульсы короче 50 мкс, а у этого протокола длительность "
                "чипа примерно столько и есть. Снимайте дамп режимом raw команды tpms_rx.",
            )

        self._refresh_summary()
        self._refresh_chart()

    def _clear(self) -> None:
        self.model.clear()
        self._rows.clear()
        self.table.setRowCount(0)
        self.log.clear()
        self.chart.clear()
        self._refresh_summary()

    def _append_log(self, message: str) -> None:
        self.log.appendPlainText(f"[{time.strftime('%H:%M:%S')}] {message}")

    # ------------------------------------------------------------------

    def closeEvent(self, event) -> None:  # noqa: N802 — имя из Qt
        self._disconnect()
        super().closeEvent(event)
