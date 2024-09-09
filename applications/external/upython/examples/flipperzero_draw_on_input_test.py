import time
import flipperzero

@flipperzero.on_input
def on_input(button, type):
  flipperzero.canvas_clear()
  flipperzero.canvas_set_color(flipperzero.CANVAS_BLACK)
  flipperzero.canvas_set_text(64, 32, '{button} - {type}'.format(button=button, type=type))
  flipperzero.canvas_update()

for _ in range(1,1000):
  time.sleep_ms(10)
