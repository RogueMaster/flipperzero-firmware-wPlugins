import time
import flipperzero

def play_frequency(frequency: float):
  volume = 0.8

  flipperzero.speaker_start(frequency, volume)

  for _ in range(0, 150):
    volume *= 0.9945679

    flipperzero.speaker_set_volume(volume)

    time.sleep_ms(1)

  flipperzero.speaker_stop()

play_frequency(100.0)
play_frequency(200.0)
play_frequency(300.0)
play_frequency(500.0)
play_frequency(800.0)
play_frequency(1300.0)
play_frequency(2100.0)
play_frequency(3400.0)
