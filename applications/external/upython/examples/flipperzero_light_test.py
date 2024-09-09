import time
import flipperzero

is_red = True

for i in range(0, 25, 1):
  brightness = i * 10
  is_red = not is_red

  flipperzero.light_set(flipperzero.LIGHT_RED, brightness if is_red else 0)
  flipperzero.light_set(flipperzero.LIGHT_GREEN, brightness if not is_red else 0)
  flipperzero.light_set(flipperzero.LIGHT_BLUE, 0)
  flipperzero.light_set(flipperzero.LIGHT_BACKLIGHT, brightness)

  time.sleep_ms(200)

flipperzero.light_set(flipperzero.LIGHT_RED, 0)
flipperzero.light_set(flipperzero.LIGHT_GREEN, 0)
flipperzero.light_set(flipperzero.LIGHT_BLUE, 0)
flipperzero.light_set(flipperzero.LIGHT_BACKLIGHT, 0)

time.sleep_ms(500)

flipperzero.light_blink_start(flipperzero.LIGHT_RED, 200, 100, 200)

time.sleep(1)

flipperzero.light_blink_set_color(flipperzero.LIGHT_BLUE)

time.sleep(1)

flipperzero.light_blink_stop()
