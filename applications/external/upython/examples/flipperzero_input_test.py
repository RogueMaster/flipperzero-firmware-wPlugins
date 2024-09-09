import time
import flipperzero

@flipperzero.on_input
def on_input(button, type):
  print('{button} - {type}'.format(button=button, type=type))

for _ in range(1,1000):
  time.sleep_ms(10)
