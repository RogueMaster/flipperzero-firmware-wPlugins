from __future__ import annotations

import os
import select
import tempfile
import threading
import time
import unittest
from pathlib import Path

try:
    import pty
    import tty

    import serial  # noqa: F401
except ImportError:
    pty = None

from host.fibp_host.cli import BridgeHost, serve_port
from host.fibp_host.permissions import PermissionStore
from host.fibp_host.session import PermissionDecision
from scripts.fibp_codec import StreamDecoder, encode_frame
from scripts.fibp_simulator import FlipperRoleEngine


@unittest.skipIf(pty is None, "PTY serial integration is POSIX-only")
class HostSerialIntegrationTests(unittest.TestCase):
    def test_hello_permission_and_ping_pong_over_pyserial_pty(self) -> None:
        master_fd, slave_fd = pty.openpty()
        tty.setraw(slave_fd)
        slave_path = os.ttyname(slave_fd)
        os.close(slave_fd)

        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        permission_path = Path(temporary_directory.name) / "permissions.json"
        host = BridgeHost(
            PermissionStore(permission_path),
            PermissionDecision.ALLOW_ONCE,
            verbose=False,
        )

        def run_host() -> None:
            try:
                serve_port(slave_path, host, handshake_timeout=3.0)
            except OSError:
                # Closing the PTY below intentionally simulates USB removal.
                pass

        worker = threading.Thread(target=run_host, daemon=True)
        worker.start()
        time.sleep(0.15)

        flipper = FlipperRoleEngine(auto_request=False)
        for frame in flipper.initial_frames():
            os.write(master_fd, encode_frame(frame))

        decoder = StreamDecoder()
        deadline = time.monotonic() + 4.0
        while not flipper.complete and time.monotonic() < deadline:
            readable, _, _ = select.select([master_fd], [], [], 0.2)
            if not readable:
                continue
            for incoming in decoder.feed(os.read(master_fd, 4096)):
                for outgoing in flipper.on_frame(incoming):
                    os.write(master_fd, encode_frame(outgoing))

        os.close(master_fd)
        worker.join(timeout=2.0)
        self.assertTrue(flipper.handshaken)
        self.assertTrue(flipper.ready)
        self.assertTrue(flipper.pong_received)
        self.assertTrue(flipper.complete)
        self.assertFalse(worker.is_alive())


if __name__ == "__main__":
    unittest.main()
