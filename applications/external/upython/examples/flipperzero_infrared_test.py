import flipperzero as f0
import time

# receive IR signal
signal = f0.infrared_receive()

time.sleep(3)

# transmit received signal
f0.infrared_transmit(signal)
