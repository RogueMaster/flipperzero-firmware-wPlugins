import flipperzero as f0
import time

f0.gpio_init_pin(f0.GPIO_PIN_PA7, f0.GPIO_MODE_OUTPUT_PUSH_PULL)

f0.gpio_set_pin(f0.GPIO_PIN_PA7, True)
time.sleep(1)
f0.gpio_set_pin(f0.GPIO_PIN_PA7, False)
time.sleep(1)
f0.gpio_set_pin(f0.GPIO_PIN_PA7, True)
time.sleep(1)
f0.gpio_set_pin(f0.GPIO_PIN_PA7, False)
