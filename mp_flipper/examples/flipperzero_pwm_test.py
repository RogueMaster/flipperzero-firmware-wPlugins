import flipperzero as f0
import time

f0.pwm_start(f0.GPIO_PIN_PA7, 4, 50)
time.sleep(3)

f0.pwm_start(f0.GPIO_PIN_PA7, 1, 25)
time.sleep(3)
