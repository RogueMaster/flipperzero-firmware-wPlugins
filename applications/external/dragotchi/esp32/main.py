# Dragotchi WiFi reporter — ESP32-S2 Flipper devboard
# Scans nearby WiFi access points and reports density to the Flipper over the
# GPIO UART so the game can trigger a "signal storm" (richer hunting).
#
# Protocol (line, 115200 8N1):  DRAGO wifi=<ap_count> rssi=<strongest_dbm>
#
# WiFi scanning blocks for seconds, so we DECOUPLE scanning from reporting:
# rescan on a slow timer, but transmit the cached result every ~200 ms. This
# keeps fresh data continuously on the wire, so the Flipper's short listen
# window during a Forage always catches several lines.
#
# Output goes on UART1 mapped to the ESP32-S2 UART0 pins (GPIO43 TX / GPIO44
# RX), which the Flipper WiFi devboard wires to the Flipper's expansion USART
# (pins 13/14). Receive-only WiFi scanning; we transmit nothing on WiFi.

import machine
import network
import time

uart = machine.UART(1, baudrate=115200, tx=43, rx=44)

wlan = network.WLAN(network.STA_IF)
wlan.active(True)
try:
    wlan.disconnect()
except Exception:
    pass

try:
    led = machine.Pin(15, machine.Pin.OUT)
except Exception:
    led = None

SCAN_EVERY_MS = 5000
REPORT_EVERY_MS = 200

def do_scan():
    try:
        nets = wlan.scan()  # (ssid, bssid, channel, RSSI, authmode, hidden)
    except Exception:
        return (0, 0)
    count = len(nets)
    best = 0
    for n in nets:
        r = n[3]
        if r < 0 and (best == 0 or r > best):
            best = r
    return (count, best)

cache = do_scan()
last_scan = time.ticks_ms()

while True:
    now = time.ticks_ms()
    if time.ticks_diff(now, last_scan) >= SCAN_EVERY_MS:
        cache = do_scan()
        last_scan = time.ticks_ms()
        print("DRAGO wifi=%d rssi=%d" % cache)  # occasional USB debug

    uart.write("DRAGO wifi=%d rssi=%d\n" % cache)

    if uart.any():          # drain any nudge from the Flipper
        try:
            uart.read()
        except Exception:
            pass
    if led:
        led.value(not led.value())
    time.sleep_ms(REPORT_EVERY_MS)
