"""Keep RX Callsigns button-paddle keying confined to its answer phase."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INPUT = (ROOT / "src/firmware/morse_flipper_input.c").read_text()
HOST = (ROOT / "src/firmware/morse_flipper_rx_practice_host.c").read_text()


def main() -> None:
    required_gate = """if(app->screen == MorseFlipperScreenRxPractice && !g.live) return false;"""
    if required_gate not in INPUT:
        raise AssertionError("RX Callsigns must decline button keying outside Answer")

    forbidden_bypass = """if(app->screen == MorseFlipperScreenRxPractice &&
       event->key == InputKeyBack && g.back_key)"""
    if forbidden_bypass in INPUT:
        raise AssertionError("RX Callsigns Back must not bypass its Answer phase gate")

    transition_release = """if(result.phase != MfRxPracticePhaseAnswer) {
        morse_flipper_drop_live_keying_for_playback(app, now_ms);
        morse_flipper_release_all_notes(app);"""
    if transition_release not in HOST:
        raise AssertionError("RX Callsigns must release live notes on non-Answer transitions")

    print("rx practice phase gating: passed")


if __name__ == "__main__":
    main()
