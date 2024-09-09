import time
import flipperzero

color = False

def draw_action():
  print('on draw')

  global color

  for x in range(0, 128):
    color = not color

    for y in range(0, 64):
      flipperzero.canvas_set_color(flipperzero.CANVAS_BLACK if color else flipperzero.CANVAS_WHITE)
      flipperzero.canvas_draw_dot(x, y)

      color = not color
  
  color = not color

  flipperzero.canvas_set_text(64, 32, "Test")

  flipperzero.canvas_update()

print('start')

draw_action()

for _ in range(1, 5):
  time.sleep(1)

  draw_action()
