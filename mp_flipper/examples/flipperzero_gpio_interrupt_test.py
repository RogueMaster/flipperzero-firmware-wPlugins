import flipperzero as f0
import time

# init pins
f0.gpio_init_pin(f0.GPIO_PIN_PA7, f0.GPIO_MODE_OUTPUT_PUSH_PULL)
f0.gpio_init_pin(
    f0.GPIO_PIN_PC0,
    f0.GPIO_MODE_INTERRUPT_RISE,
    f0.GPIO_PULL_UP,
    f0.GPIO_SPEED_VERY_HIGH,
)
f0.gpio_init_pin(
    f0.GPIO_PIN_PC1,
    f0.GPIO_MODE_INTERRUPT_RISE,
    f0.GPIO_PULL_UP,
    f0.GPIO_SPEED_VERY_HIGH,
)


@f0.on_gpio
def on_gpio(pin):
    if pin == f0.GPIO_PIN_PC0:
        f0.gpio_set_pin(f0.GPIO_PIN_PA7, True)
    if pin == f0.GPIO_PIN_PC1:
        f0.gpio_set_pin(f0.GPIO_PIN_PA7, False)


for _ in range(1, 1500):
    time.sleep_ms(10)

# reset pins
f0.gpio_init_pin(f0.GPIO_PIN_PA7, f0.GPIO_MODE_ANALOG)
f0.gpio_init_pin(f0.GPIO_PIN_PC0, f0.GPIO_MODE_ANALOG)
f0.gpio_init_pin(f0.GPIO_PIN_PC1, f0.GPIO_MODE_ANALOG)
