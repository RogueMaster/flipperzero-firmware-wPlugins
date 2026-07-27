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

    cleanup_start = HOST.find("if(result.phase != MfRxPracticePhaseAnswer) {")
    cleanup_end = HOST.find("    }", cleanup_start)
    cleanup = HOST[cleanup_start:cleanup_end]
    drop = cleanup.find("morse_flipper_drop_live_keying_for_playback(app, now_ms);")
    release = cleanup.find("morse_flipper_release_all_notes(app);")
    refresh = cleanup.find("morse_flipper_refresh_keyer(app, now_ms);")
    if cleanup_start < 0 or not (0 <= drop < release < refresh):
        raise AssertionError("RX Callsigns must release sources and flush queued keyer tails")

    print("rx practice phase gating: passed")


if __name__ == "__main__":
    main()
