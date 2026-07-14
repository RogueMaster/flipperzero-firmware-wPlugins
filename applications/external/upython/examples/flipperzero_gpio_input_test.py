import flipperzero as f0
import time

f0.gpio_init_pin(f0.GPIO_PIN_PA7, f0.GPIO_MODE_OUTPUT_PUSH_PULL)
f0.gpio_init_pin(
    f0.GPIO_PIN_PC1, f0.GPIO_MODE_INPUT, f0.GPIO_PULL_UP, f0.GPIO_SPEED_HIGH
)

for _ in range(0, 15):
    state = f0.gpio_get_pin(f0.GPIO_PIN_PC1)

    f0.gpio_set_pin(f0.GPIO_PIN_PA7, state)

    time.sleep(1)
