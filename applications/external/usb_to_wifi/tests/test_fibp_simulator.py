from __future__ import annotations

import os
import select
import threading
import time
import unittest
from collections import deque

from scripts.fibp_codec import (
    Frame,
    MessageType,
    ParseIssueCode,
    StreamDecoder,
    encode_frame,
    encode_ping_token,
    fragment_bytes,
)
from scripts.fibp_simulator import (
    DEFAULT_PING_TOKEN,
    FlipperRoleEngine,
    FrameTransmitter,
    MacRoleEngine,
    PseudoTerminal,
    PtySimulator,
)


class FibpSimulatorTests(unittest.TestCase):
    def test_role_engines_complete_demo_exchange(self) -> None:
        mac = MacRoleEngine(response_text="hello from mac\n")
        flipper = FlipperRoleEngine()
        queue = deque((mac, frame) for frame in flipper.initial_frames())

        steps = 0
        while queue:
            target, frame = queue.popleft()
            peer = flipper if target is mac else mac
            for response in target.on_frame(frame):
                queue.append((peer, response))
            steps += 1
            self.assertLess(steps, 100, "simulated exchange did not converge")

        self.assertTrue(mac.handshaken)
        self.assertTrue(mac.permission_granted)
        self.assertTrue(mac.complete)
        self.assertTrue(flipper.handshaken)
        self.assertTrue(flipper.ready)
        self.assertTrue(flipper.pong_received)
        self.assertTrue(flipper.complete)
        self.assertEqual(bytes(flipper.response_body), b"hello from mac\n")
        self.assertEqual(flipper.response_started.http_status, 200)
        self.assertEqual(flipper.response_end.bytes_sent, len(flipper.response_body))

    def test_transmitter_fragment_pattern_round_trips(self) -> None:
        frame = Frame(
            MessageType.PING, encode_ping_token(DEFAULT_PING_TOKEN), sequence=1
        )
        transmitter = FrameTransmitter((1, 2, 7, 64))
        encoded = transmitter.encoded(frame)
        decoder = StreamDecoder()
        frames = []
        for chunk in fragment_bytes(encoded, transmitter.fragment_sizes):
            frames.extend(decoder.feed(chunk))
        self.assertEqual(frames, [frame])

    def test_transmitter_can_corrupt_header_or_frame_crc(self) -> None:
        frame = Frame(
            MessageType.PING, encode_ping_token(DEFAULT_PING_TOKEN), sequence=1
        )
        for mode, expected in (
            ("header", ParseIssueCode.BAD_HEADER_CRC),
            ("frame", ParseIssueCode.BAD_FRAME_CRC),
        ):
            with self.subTest(mode=mode):
                transmitter = FrameTransmitter((544,), corrupt_crc=mode)
                decoder = StreamDecoder()
                self.assertEqual(decoder.feed(transmitter.encoded(frame)), [])
                self.assertEqual(decoder.issues[0].code, expected)
                # Corruption is one-shot; the next frame is valid.
                self.assertEqual(decoder.feed(transmitter.encoded(frame)), [frame])

    @unittest.skipUnless(os.name == "posix", "PTY support requires POSIX")
    def test_mac_role_over_real_pty(self) -> None:
        mac = MacRoleEngine(response_text="PTY response\n")
        flipper = FlipperRoleEngine()

        with PseudoTerminal.open() as endpoint:
            os.set_blocking(endpoint.slave_fd, False)
            stop = threading.Event()
            simulator = PtySimulator(
                endpoint,
                mac,
                FrameTransmitter((1, 3, 7, 64)),
            )
            thread = threading.Thread(
                target=simulator.run,
                kwargs={"stop_event": stop, "timeout_seconds": 3.0, "once": True},
                daemon=True,
            )
            thread.start()

            for initial in flipper.initial_frames():
                raw = encode_frame(initial)
                for chunk in fragment_bytes(raw, (2, 5, 11)):
                    os.write(endpoint.slave_fd, chunk)

            peer_decoder = StreamDecoder()
            deadline = time.monotonic() + 3.0
            while time.monotonic() < deadline and not flipper.complete:
                readable, _, _ = select.select([endpoint.slave_fd], [], [], 0.05)
                if not readable:
                    continue
                try:
                    data = os.read(endpoint.slave_fd, 4096)
                except BlockingIOError:
                    continue
                for received in peer_decoder.feed(data):
                    for response in flipper.on_frame(received):
                        os.write(endpoint.slave_fd, encode_frame(response))

            stop.set()
            thread.join(timeout=3.0)
            self.assertFalse(thread.is_alive())
            self.assertTrue(flipper.pong_received)
            self.assertTrue(flipper.complete)
            self.assertEqual(bytes(flipper.response_body), b"PTY response\n")
            self.assertEqual(peer_decoder.issues, [])


if __name__ == "__main__":
    unittest.main()
