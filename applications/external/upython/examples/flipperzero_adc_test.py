import flipperzero as f0
import time

f0.gpio_init_pin(f0.GPIO_PIN_PC1, f0.GPIO_MODE_ANALOG)

for _ in range(1, 1000):
    raw_value = f0.adc_read_pin_value(f0.GPIO_PIN_PC1)
    raw_voltage = f0.adc_read_pin_voltage(f0.GPIO_PIN_PC1)

    value = "{value} #".format(value=raw_value)
    voltage = "{value} mV".format(value=raw_voltage)

    f0.canvas_clear()

    f0.canvas_set_text(10, 32, value)
    f0.canvas_set_text(70, 32, voltage)

    f0.canvas_update()

    time.sleep_ms(10)
