#!/usr/bin/env python3
"""
meshlog.py - полевой логгер для тестов MeshCore.

Что делает (всё сразу, руками писать ничего не надо):
  * ПАССИВНО ловит каждый услышанный радио-пакет и пишет его SNR/RSSI в CSV
    (это и есть "карта покрытия" - ходишь, оно само пишет уровень сигнала).
  * ПЕРИОДИЧЕСКИ опрашивает ноду: заряд батареи, шумовой пол, счётчики пакетов
    и ошибок приёма -> в CSV. Отсюда берётся кривая батареи на весь день.
  * ПО ЖЕЛАНИЮ активно пингует заданный контакт и меряет round-trip (RTT).
  * GPS берёт из termux-location (Android/Termux). Нет termux-api - просто
    колонки координат будут пустыми, скрипт всё равно работает (и на ноуте тоже).

Установка:
    pip install meshcore          # нужен Python 3.10+
    # Linux + USB: добавь себя в группу dialout (sudo usermod -a -G dialout $USER, релогин)
    # BLE: сначала спарь ноду через bluetoothctl

Запуск (выбери один способ подключения):
    python meshlog.py --serial /dev/ttyACM0            # companion по USB
    python meshlog.py --tcp 192.168.4.1:5000           # companion на WiFi (AP ноды)
    python meshlog.py --ble C2:2B:A1:D5:3E:B6          # companion по BLE

Полезные флаги:
    --ping BASE --interval 15     # активно пинговать контакт "BASE" каждые 15 с
    --stats-interval 60           # как часто опрашивать батарею/статы, сек
    --outdir ./meshtest           # куда складывать CSV

Стоп: Ctrl-C. Файлы: rx_log.csv, events.csv, telemetry.csv, ping.csv
"""

import argparse
import asyncio
import csv
import json
import os
import subprocess
import time
from datetime import datetime, timezone

from meshcore import MeshCore, EventType


# ------------------------------------------------------------------ GPS (optional)
_gps = {"lat": None, "lon": None, "acc": None}


async def gps_loop(period: float):
    """Фоново тянет позицию из termux-location. Нет termux-api -> тихо пропускаем."""
    while True:
        try:
            r = subprocess.run(
                ["termux-location", "-p", "gps"],
                capture_output=True, text=True, timeout=25,
            )
            d = json.loads(r.stdout)
            _gps.update(lat=d.get("latitude"), lon=d.get("longitude"),
                        acc=d.get("accuracy"))
        except Exception:
            pass
        await asyncio.sleep(period)


def ts() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


# ------------------------------------------------------------------ CSV helper
class Csv:
    def __init__(self, path, header):
        fresh = (not os.path.exists(path)) or os.path.getsize(path) == 0
        self.f = open(path, "a", newline="", encoding="utf-8")
        self.w = csv.writer(self.f)
        if fresh:
            self.w.writerow(header)
            self.f.flush()

    def row(self, *cols):
        self.w.writerow(cols)
        self.f.flush()  # flush сразу: выдернешь кабель - данные не потеряются


def pg(p, *keys):
    """Первый непустой ключ из payload (имена полей в прошивках слегка гуляют)."""
    if not isinstance(p, dict):
        return None
    for k in keys:
        if p.get(k) is not None:
            return p[k]
    return None


async def main():
    ap = argparse.ArgumentParser()
    conn = ap.add_mutually_exclusive_group(required=True)
    conn.add_argument("--serial", metavar="PORT", help="напр. /dev/ttyACM0 или COM5")
    conn.add_argument("--tcp", metavar="HOST[:PORT]", help="companion на WiFi (порт по умолч. 5000)")
    conn.add_argument("--ble", metavar="ADDR", help="MAC/имя BLE-устройства")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--ping", metavar="NAMES",
                    help="контакты для активного пинга через запятую, напр. BASE,REP1,REP2")
    ap.add_argument("--interval", type=float, default=15, help="период пинга, сек")
    ap.add_argument("--stats-interval", type=float, default=60, help="период опроса статов, сек")
    ap.add_argument("--gps-interval", type=float, default=5, help="период опроса GPS, сек")
    ap.add_argument("--outdir", default="./meshtest", help="папка для CSV")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    d = args.outdir
    rx_csv = Csv(os.path.join(d, "rx_log.csv"),
                 ["ts", "snr", "rssi", "lat", "lon", "acc", "raw"])
    ev_csv = Csv(os.path.join(d, "events.csv"),
                 ["ts", "type", "info", "lat", "lon", "acc", "raw"])
    tel_csv = Csv(os.path.join(d, "telemetry.csv"),
                  ["ts", "batt_pct", "voltage", "noise_floor", "rx_total", "tx_total",
                   "recv_errors", "lat", "lon", "acc", "raw_bat", "raw_radio", "raw_pkts"])
    ping_csv = Csv(os.path.join(d, "ping.csv"),
                   ["ts", "target", "seq", "ok", "rtt_ms", "lat", "lon", "acc"])

    # ---- подключение
    print(f"[{ts()}] подключаюсь...")
    if args.serial:
        mc = await MeshCore.create_serial(args.serial, args.baud)
    elif args.tcp:
        host, _, port = args.tcp.partition(":")
        mc = await MeshCore.create_tcp(host, int(port or 5000))
    else:
        mc = await MeshCore.create_ble(args.ble)
    print(f"[{ts()}] подключено. Пишу CSV в {os.path.abspath(d)}. Ctrl-C для остановки.")

    # ---- ПАССИВ: каждый услышанный пакет -> SNR/RSSI
    async def on_rx(ev):
        p = ev.payload or {}
        rx_csv.row(ts(), pg(p, "snr", "SNR"), pg(p, "rssi", "RSSI"),
                   _gps["lat"], _gps["lon"], _gps["acc"], json.dumps(p, default=str))
    mc.subscribe(EventType.RX_LOG_DATA, on_rx)
    mc.subscribe(EventType.RAW_DATA, on_rx)  # запасной канал у части прошивок

    async def on_advert(ev):
        p = ev.payload or {}
        ev_csv.row(ts(), "advert", pg(p, "public_key", "pubkey_prefix"),
                   _gps["lat"], _gps["lon"], _gps["acc"], json.dumps(p, default=str))
    mc.subscribe(EventType.ADVERTISEMENT, on_advert)

    async def on_msg(ev):
        p = ev.payload or {}
        ev_csv.row(ts(), "msg", pg(p, "text"),
                   _gps["lat"], _gps["lon"], _gps["acc"], json.dumps(p, default=str))
    mc.subscribe(EventType.CONTACT_MSG_RECV, on_msg)
    mc.subscribe(EventType.CHANNEL_MSG_RECV, on_msg)
    await mc.start_auto_message_fetching()

    # ---- ПЕРИОДИКА: батарея / шумовой пол / счётчики пакетов
    async def stats_loop():
        while True:
            bat = radio = pkts = {}
            try:
                bat = (await mc.commands.get_bat()).payload or {}
            except Exception:
                pass
            try:
                radio = (await mc.commands.get_stats_radio()).payload or {}
            except Exception:
                pass
            try:
                pkts = (await mc.commands.get_stats_packets()).payload or {}
            except Exception:
                pass
            tel_csv.row(
                ts(),
                pg(bat, "level", "battery", "batt"),
                pg(bat, "voltage", "volt") or pg(radio, "voltage"),
                pg(radio, "noise_floor", "noise"),
                pg(pkts, "rx_total", "rx", "received"),
                pg(pkts, "tx_total", "tx", "sent"),
                pg(pkts, "recv_errors", "errors"),
                _gps["lat"], _gps["lon"], _gps["acc"],
                json.dumps(bat, default=str), json.dumps(radio, default=str),
                json.dumps(pkts, default=str),
            )
            await asyncio.sleep(args.stats_interval)

    # ---- ОПЦИЯ: активный пинг (RTT через msg + ACK), несколько целей по кругу
    async def ping_loop():
        targets = [t.strip() for t in args.ping.split(",") if t.strip()]
        print(f"[{ts()}] ПИНГ целей: {', '.join(targets)}")
        seq = 0
        warned = set()
        while True:
            for name in targets:
                contact = mc.get_contact_by_name(name)
                if not contact:
                    # нода могла ещё не попасть в контакты - перечитаем список
                    try:
                        await mc.commands.get_contacts()
                        contact = mc.get_contact_by_name(name)
                    except Exception:
                        pass
                if not contact:
                    if name not in warned:
                        print(f"[{ts()}] ПИНГ: '{name}' пока не в контактах, "
                              f"продолжаю пробовать (нужен его advert)")
                        warned.add(name)
                    # промах фиксируем: цель вне зоны / ещё не найдена
                    ping_csv.row(ts(), name, "", 0, "",
                                 _gps["lat"], _gps["lon"], _gps["acc"])
                    await asyncio.sleep(args.interval)
                    continue
                warned.discard(name)

                seq += 1
                t0 = time.monotonic()
                ok = False
                try:
                    sent = await mc.commands.send_msg(contact, f"ping {seq}")
                    exp = (sent.payload or {}).get("expected_ack")
                    code = exp.hex() if hasattr(exp, "hex") else str(exp)
                    ack = await mc.wait_for_event(
                        EventType.ACK,
                        attribute_filters={"code": code},
                        timeout=max(2.0, args.interval - 1),
                    )
                    ok = ack is not None
                except Exception:
                    ok = False
                rtt = round((time.monotonic() - t0) * 1000) if ok else ""
                ping_csv.row(ts(), name, seq, int(ok), rtt,
                             _gps["lat"], _gps["lon"], _gps["acc"])
                print(f"[{ts()}] ping {name} #{seq}: "
                      f"{'OK ' + str(rtt) + ' ms' if ok else 'MISS'}")
                await asyncio.sleep(args.interval)

    tasks = [asyncio.create_task(gps_loop(args.gps_interval)),
             asyncio.create_task(stats_loop())]
    if args.ping:
        tasks.append(asyncio.create_task(ping_loop()))

    try:
        await asyncio.Event().wait()  # крутимся, пока не Ctrl-C
    except (KeyboardInterrupt, asyncio.CancelledError):
        pass
    finally:
        for t in tasks:
            t.cancel()
        try:
            await mc.disconnect()
        except Exception:
            pass
        print(f"\n[{ts()}] остановлено. Файлы в {os.path.abspath(d)}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
