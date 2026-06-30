import flipperzero as f0
import time

terminate = False
index = 0
signal = []


def draw_signal():
    global index
    global signal

    f0.canvas_clear()

    level = False if index % 2 else True
    x = 0
    y_low = 32
    y_high = 40
    y_level = y_low

    for i in range(100):
        i += index

        if i > len(signal):
            break

        duration = int(signal[i] / 100)

        if level:
            f0.canvas_draw_line(x, y_low, x, y_high)
            y_level = y_high
        else:
            f0.canvas_draw_line(x, y_high, x, y_low)
            y_level = y_low

        f0.canvas_draw_line(x, y_level, x + duration, y_level)

        x += duration

        level = not level

        if x > f0.canvas_width():
            break

    f0.canvas_update()


@f0.on_input
def on_input(button, type):
    global terminate
    global index
    global signal

    # terminate upon back button press
    if button == f0.INPUT_BUTTON_BACK and type == f0.INPUT_TYPE_SHORT:
        terminate = True

        return

    # transmit signal upon ok button
    if button == f0.INPUT_BUTTON_OK and type == f0.INPUT_TYPE_SHORT and len(signal) > 0:
        f0.infrared_transmit(signal)

        return

    # scroll left upon button left
    if button == f0.INPUT_BUTTON_LEFT and type == f0.INPUT_TYPE_SHORT:
        index -= 1 if index > 0 else 0

        draw_signal()

        return

    # scroll right upon button left
    if button == f0.INPUT_BUTTON_RIGHT and type == f0.INPUT_TYPE_SHORT:
        index += 1
        index %= len(signal)

        draw_signal()

        return


f0.canvas_set_text(10, 32, "Waiting for IR signal ...")
f0.canvas_update()

# receive signal
signal = f0.infrared_receive()

if len(signal):
    draw_signal()
else:
    terminate = True

# wait upon termination
while not terminate:
    time.sleep_ms(1)
