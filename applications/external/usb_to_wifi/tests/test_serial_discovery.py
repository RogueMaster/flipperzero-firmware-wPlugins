"""Hardware-ID discovery and bounded probing; no physical ports or consent."""

import queue
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from host.fibp_host import cli
from host.fibp_host.desktop_backend import bridge_ports
from host.fibp_host.desktop_backend import ConnectionWorker, DesktopController
from host.fibp_host.serial_ports import serial_candidates
from host.fibp_host.permissions import PermissionStore
from scripts.fibp_codec import MessageType, StreamDecoder, encode_frame
from tests.test_host_session import hello_frame
from pathlib import Path
import tempfile


def port(device, vid=0x0483, pid=0x5740, description="USB Serial Device"):
    return SimpleNamespace(
        device=device,
        vid=vid,
        pid=pid,
        description=description,
        manufacturer="Microsoft",
        product=None,
    )


class USBDiscoveryTests(unittest.TestCase):
    def test_gui_accepts_official_single_and_dual_cdc_identity(self):
        # Official firmware 1.4.3 uses the SAME 0483:5740 descriptor for both
        # modes. Interfaces/HELLO, not a fabricated second PID, identify CDC1.
        ports = [
            port("COM7"),
            port("COM8"),
            port("other", vid=0x1234),
            port("not_flipper", pid=0x5741),
        ]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            self.assertEqual(set(bridge_ports()), {"COM7", "COM8"})

    def test_cli_recognizes_generic_windows_description_by_vid_pid(self):
        with patch("serial.tools.list_ports.comports", return_value=[port("COM8")]):
            self.assertTrue(serial_candidates()[0].likely_flipper)

    def test_cli_does_not_trust_name_over_known_other_hardware(self):
        with patch(
            "serial.tools.list_ports.comports",
            return_value=[
                port("COM5", vid=0x1234, description="Flipper"),
                port("COM6", pid=0x5741, description="Flipper"),
            ],
        ):
            self.assertFalse(any(item.likely_flipper for item in serial_candidates()))

    def test_cli_keeps_legacy_name_fallback_only_without_usb_metadata(self):
        with patch(
            "serial.tools.list_ports.comports",
            return_value=[
                port("/dev/cu.usbmodemflip_Test3", vid=None, pid=None),
                port("/dev/cu.unrelated", vid=None, pid=None),
            ],
        ):
            candidates = serial_candidates()
            self.assertTrue(candidates[0].likely_flipper)
            self.assertFalse(candidates[1].likely_flipper)


class HandshakeDeadlineTests(unittest.TestCase):
    def probe(self, noisy):
        elapsed, reads, closed = [0.0], [0], []

        class FakeSerial:
            in_waiting = 16

            def read(self, _size):
                reads[0] += 1
                elapsed[0] += 0.25
                if reads[0] > 12:
                    raise AssertionError("Probe exceeded its deadline without HELLO")
                return b"debug log\r\n" if noisy else b""

            def write(self, data):
                return len(data)

            def close(self):
                closed.append(True)

        host = SimpleNamespace(
            outgoing=queue.Queue(),
            verbose=False,
            session=SimpleNamespace(permission_pending=False),
            debug=lambda _: None,
            close=lambda: None,
        )
        with patch.object(cli, "open_serial", return_value=FakeSerial()), patch.object(
            cli.time, "monotonic", side_effect=lambda: elapsed[0]
        ):
            self.assertFalse(cli.serve_port("test-only", host, handshake_timeout=1))
        self.assertTrue(closed)
        self.assertLessEqual(elapsed[0], 1.25)

    def test_silent_candidate_times_out(self):
        self.probe(False)

    def test_continuous_non_fibp_data_cannot_extend_hello_deadline(self):
        self.probe(True)

    def test_gui_advances_from_noisy_cli_to_real_bridge_without_granting_access(self):
        elapsed, opened, written = [0.0], [], []
        with tempfile.TemporaryDirectory() as directory:
            permissions = PermissionStore(Path(directory) / "permissions.json")
            controller = DesktopController(permissions, lambda: ["COM7", "COM8"])
            self.addCleanup(controller.close)
            worker = ConnectionWorker(controller, "usb")
            controller.workers["usb"] = worker

            class Noisy:
                in_waiting = 16

                def read(self, _size):
                    elapsed[0] += 0.5
                    if elapsed[0] > 20:
                        raise AssertionError("The worker never advanced to the bridge")
                    return b"debug log\r\n"

                def write(self, data):
                    return len(data)

                def close(self):
                    pass

            class Bridge(Noisy):
                sent_hello = False

                def read(self, _size):
                    elapsed[0] += 0.5
                    if not self.sent_hello:
                        self.sent_hello = True
                        return encode_frame(hello_frame())
                    worker.stop_event.set()
                    return b""

                def write(self, data):
                    written.append(data)
                    return len(data)

            def open_port(device, **_options):
                opened.append(device)
                return Noisy() if device == "COM7" else Bridge()

            with patch.object(cli, "open_serial", side_effect=open_port), patch.object(
                cli.time, "monotonic", side_effect=lambda: elapsed[0]
            ), patch.object(cli, "perform_request") as network:
                worker.run()
            self.assertEqual(opened, ["COM7", "COM8"])
            frames = StreamDecoder().feed(b"".join(written))
            self.assertTrue(
                any(frame.message_type == MessageType.HELLO_ACK for frame in frames)
            )
            network.assert_not_called()
            self.assertFalse(permissions.path.exists())


if __name__ == "__main__":
    unittest.main()
