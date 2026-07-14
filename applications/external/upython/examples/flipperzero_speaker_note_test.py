import time
import flipperzero as f0


def play_frequency(frequency: float):
    volume = 0.8

    f0.speaker_start(frequency, volume)

    for _ in range(0, 150):
        volume *= 0.9945679

        f0.speaker_set_volume(volume)

        time.sleep_ms(1)

    f0.speaker_stop()


play_frequency(f0.SPEAKER_NOTE_C5)
play_frequency(f0.SPEAKER_NOTE_D5)
play_frequency(f0.SPEAKER_NOTE_E5)
play_frequency(f0.SPEAKER_NOTE_F5)
play_frequency(f0.SPEAKER_NOTE_G5)
play_frequency(f0.SPEAKER_NOTE_A5)
play_frequency(f0.SPEAKER_NOTE_B5)
play_frequency(f0.SPEAKER_NOTE_C6)
