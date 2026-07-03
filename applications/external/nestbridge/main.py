import bluetooth
import network
import urequests
import json
import time
from micropython import const

# ── CONFIG ─────────────────────────────────────────────────────────────────────
WIFI_SSID = " "
WIFI_PASSWORD = " "
NLE_API_KEY = " "
DEVICE_ID = " "
SERIAL = " "
NLE_BASE = "https://nolongerevil.com/api/v1"
# ───────────────────────────────────────────────────────────────────────────────

MFR_ID = bytes([0xE5, 0x02])
APP_MARK = bytes([ord("N"), ord("B")])

CMD_IDLE = 0x00
CMD_SET_TEMP = 0xF0
CMD_MODE_HEAT = 0x04
CMD_MODE_COOL = 0x05
CMD_MODE_HC = 0x06
CMD_MODE_OFF = 0x07
CMD_FAN_15 = 0xA0
CMD_FAN_30 = 0xA1
CMD_FAN_45 = 0xA2
CMD_FAN_1H = 0xA3
CMD_FAN_2H = 0xA4
CMD_FAN_4H = 0xA5
CMD_FAN_8H = 0xA6
CMD_FAN_12H = 0xA7
CMD_FAN_OFF = 0xA8

FAN_DURATIONS = {
    CMD_FAN_15: 900,
    CMD_FAN_30: 1800,
    CMD_FAN_45: 2700,
    CMD_FAN_1H: 3600,
    CMD_FAN_2H: 7200,
    CMD_FAN_4H: 14400,
    CMD_FAN_8H: 28800,
    CMD_FAN_12H: 43200,
}

_IRQ_SCAN_RESULT = const(5)
_IRQ_SCAN_DONE = const(6)


# ── WIFI ───────────────────────────────────────────────────────────────────────
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print("Connecting to WiFi...")
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)
        for _ in range(20):
            if wlan.isconnected():
                break
            time.sleep(1)
    if wlan.isconnected():
        print("WiFi connected:", wlan.ifconfig()[0])
        return True
    print("WiFi failed")
    return False


# ── HELPERS ────────────────────────────────────────────────────────────────────
def c_to_f(c):
    return round(c * 9 / 5 + 32, 1)


HEADERS = {"Authorization": NLE_API_KEY, "Content-Type": "application/json"}


# ── NLE API ────────────────────────────────────────────────────────────────────
def set_temperature(value_f):
    try:
        r = urequests.get(f"{NLE_BASE}/thermostat/{DEVICE_ID}/status", headers=HEADERS)
        data = r.json()
        r.close()
        shared = data["state"][f"shared.{SERIAL}"]["value"]
        mode = shared["target_temperature_type"]
        body = json.dumps({"value": value_f, "mode": mode, "scale": "F"})
        r = urequests.post(
            f"{NLE_BASE}/thermostat/{DEVICE_ID}/temperature", headers=HEADERS, data=body
        )
        data = r.json()
        r.close()
        print(f"SET TEMP {value_f}F: {'OK' if data.get('success') else 'FAIL'}")
    except Exception as e:
        print(f"SET TEMP ERR: {e}")


def set_mode(mode):
    try:
        body = json.dumps({"mode": mode})
        r = urequests.post(
            f"{NLE_BASE}/thermostat/{DEVICE_ID}/mode", headers=HEADERS, data=body
        )
        data = r.json()
        r.close()
        print(f"MODE {mode}: {'OK' if data.get('success') else 'FAIL'}")
    except Exception as e:
        print(f"MODE ERR: {e}")


def set_fan_timer(duration_seconds):
    try:
        body = json.dumps({"mode": "on", "duration": duration_seconds})
        r = urequests.post(
            f"{NLE_BASE}/thermostat/{DEVICE_ID}/fan", headers=HEADERS, data=body
        )
        data = r.json()
        r.close()
        print(
            f"FAN {duration_seconds//60}min: {'OK' if data.get('success') else 'FAIL'}"
        )
    except Exception as e:
        print(f"FAN ERR: {e}")


def set_fan_off():
    try:
        body = json.dumps({"mode": "off"})
        r = urequests.post(
            f"{NLE_BASE}/thermostat/{DEVICE_ID}/fan", headers=HEADERS, data=body
        )
        data = r.json()
        r.close()
        print(f"FAN OFF: {'OK' if data.get('success') else 'FAIL'}")
    except Exception as e:
        print(f"FAN OFF ERR: {e}")


# ── COMMAND HANDLER ────────────────────────────────────────────────────────────
def handle_command(cmd, temp):
    print(f"CMD=0x{cmd:02X} temp={temp}")
    if cmd == CMD_SET_TEMP:
        set_temperature(temp)
    elif cmd == CMD_MODE_HEAT:
        set_mode("heat")
    elif cmd == CMD_MODE_COOL:
        set_mode("cool")
    elif cmd == CMD_MODE_HC:
        set_mode("heat-cool")
    elif cmd == CMD_MODE_OFF:
        set_mode("off")
    elif cmd == CMD_FAN_OFF:
        set_fan_off()
    elif cmd in FAN_DURATIONS:
        set_fan_timer(FAN_DURATIONS[cmd])


# ── BLE SCANNER ────────────────────────────────────────────────────────────────
last_cmd = CMD_IDLE
last_temp = 0


def parse_adv(adv_data):
    i = 0
    while i < len(adv_data):
        length = adv_data[i]
        if length == 0 or i + length >= len(adv_data):
            break
        if adv_data[i + 1] == 0xFF and length >= 7:
            mfr = bytes(adv_data[i + 2 : i + 4])
            mark = bytes(adv_data[i + 4 : i + 6])
            if mfr == MFR_ID and mark == APP_MARK:
                return adv_data[i + 6], adv_data[i + 7] if length >= 7 else 0
        i += 1 + length
    return None, None


ble = bluetooth.BLE()
ble.active(True)


def irq(event, data):
    global last_cmd, last_temp
    if event == _IRQ_SCAN_RESULT:
        addr_type, addr, adv_type, rssi, adv_data = data
        cmd, temp = parse_adv(adv_data)
        if cmd is not None:
            if cmd == CMD_IDLE:
                # Broadcast ended - reset dedup so the same command can fire again
                last_cmd = CMD_IDLE
                last_temp = 0
            elif cmd != last_cmd or temp != last_temp:
                last_cmd = cmd
                last_temp = temp
                handle_command(cmd, temp)
    elif event == _IRQ_SCAN_DONE:
        ble.gap_scan(0, 30000, 30000)


ble.irq(irq)


# ── MAIN ───────────────────────────────────────────────────────────────────────
def main():
    connect_wifi()
    print("NestBridge ready, scanning...")
    ble.gap_scan(0, 30000, 30000)
    while True:
        time.sleep(10)


main()
