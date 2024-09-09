import time
import flipperzero as f0

def init_grid():
  return [
    [' ', ' ', ' '],
    [' ', ' ', ' '],
    [' ', ' ', ' '],
  ]

m_exit = False

m_grid = init_grid()

m_x = 1
m_y = 1

m_is_cross = True

@f0.on_input
def input_handler(button, type):
  global m_exit
  global m_grid
  global m_x
  global m_y
  global m_is_cross

  if button == f0.INPUT_BUTTON_BACK and type == f0.INPUT_TYPE_LONG:
    m_exit = True
  elif button == f0.INPUT_BUTTON_BACK and type == f0.INPUT_TYPE_SHORT:
    m_grid = init_grid()
  elif button == f0.INPUT_BUTTON_LEFT and type == f0.INPUT_TYPE_SHORT:
    m_x = (m_x - 1) % 3
  elif button == f0.INPUT_BUTTON_RIGHT and type == f0.INPUT_TYPE_SHORT:
    m_x = (m_x + 1) % 3
  elif button == f0.INPUT_BUTTON_UP and type == f0.INPUT_TYPE_SHORT:
    m_y = (m_y - 1) % 3
  elif button == f0.INPUT_BUTTON_DOWN and type == f0.INPUT_TYPE_SHORT:
    m_y = (m_y + 1) % 3
  elif button == f0.INPUT_BUTTON_OK and type == f0.INPUT_TYPE_SHORT:
    m_grid[m_x][m_y] = 'X' if m_is_cross else 'O'
    m_is_cross = not m_is_cross

def draw_grid():
  global m_grid
  global m_x
  global m_y

  f0.canvas_clear()
  f0.canvas_draw_frame(2, 2, 60, 60)
  
  f0.canvas_draw_line(22, 2, 22, 62)
  f0.canvas_draw_line(42, 2, 42, 62)
  
  f0.canvas_draw_line(2, 22, 62, 22)
  f0.canvas_draw_line(2, 42, 62, 42)

  px = m_x * 20 + 4
  py = m_y * 20 + 4

  f0.canvas_draw_frame(px, py, 16, 16)
  
  f0.canvas_set_text_align(f0.ALIGN_CENTER, f0.ALIGN_CENTER)

  for x in range(0, 3):
    for y in range(0, 3):
      px = x * 20 + 10 + 2
      py = y * 20 + 10 + 2
      f0.canvas_set_text(px, py, m_grid[x][y])
  
  f0.canvas_update()

while not m_exit:
  draw_grid()
  time.sleep_ms(25)
