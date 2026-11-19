from __future__ import annotations

import dataclasses
import asyncio
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest.mock import patch

from host.fibp_host.desktop_backend import (
    ConnectionSnapshot,
    ConnectionWorker,
    DesktopController,
    DesktopHost,
    PromptBroker,
    bridge_ports,
)
from host.fibp_host.permissions import PermissionStore
from host.fibp_host.session import PermissionDecision
from scripts.fibp_codec import (
    Frame,
    FrameFlags,
    MessageType,
    RequestStart,
    decode_hello,
    encode_hello,
    encode_request_start,
)
from tests.test_host_session import hello_frame
from tests.test_bluetooth_host import Vault
from host.fibp_host.bluetooth_pairing import PairingStore


class DesktopBackendTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.permissions = PermissionStore(Path(self.temp.name) / "permissions.json")
        self.pairs = PairingStore(Path(self.temp.name) / "pairs.json", Vault())
        self.controller = DesktopController(
            self.permissions, lambda: [], lambda: self.pairs
        )
        self.addCleanup(self.controller.close)

    def worker(self, link):
        worker = ConnectionWorker(self.controller, link)
        self.controller.workers[link] = worker
        host = DesktopHost(self.controller, worker, bluetooth=link == "bluetooth")
        worker.host = host
        self.addCleanup(host.close)
        return worker, host

    def test_dialog_cancel_and_stale_answer_never_grant_another_owner(self):
        broker = PromptBroker()
        old = broker.request("old", "usb", "permission", "Mico")
        fresh = broker.request("new", "bluetooth", "permission", "Mico")
        broker.cancel("old")
        broker.answer(old.id, PermissionDecision.ALLOW_ALWAYS)
        self.assertIsNone(old.future.result())
        self.assertFalse(fresh.future.done())
        broker.answer(fresh.id, PermissionDecision.ALLOW_ONCE)
        self.assertEqual(fresh.future.result(), PermissionDecision.ALLOW_ONCE)

    def test_usb_consent_is_nonblocking_and_no_network_before_allow(self):
        worker, host = self.worker("usb")
        requests = []
        host.session.on_request = requests.append
        host.session.on_frame(hello_frame())
        self.assertEqual(host.resolve_permission(), [])
        prompt = self.controller.prompts.pending()[0]
        start = Frame(
            MessageType.REQUEST_START,
            encode_request_start(RequestStart(1, 1000, "https://example.com")),
            request_id=50,
        )
        self.assertEqual(
            host.session.on_frame(start)[0].message_type, MessageType.ERROR
        )
        self.assertFalse(requests)
        self.controller.prompts.answer(prompt.id, PermissionDecision.ALLOW_ONCE)
        self.assertEqual(host.resolve_permission()[0].payload, bytes((1, 0)))
        self.assertFalse(self.permissions.contains(host.session.hello))

    def test_denied_and_closed_prompts_fail_closed(self):
        _, host = self.worker("usb")
        host.session.on_frame(hello_frame())
        host.resolve_permission()
        self.controller.prompts.answer(self.controller.prompts.pending()[0].id, None)
        self.assertEqual(host.resolve_permission()[0].payload, bytes((0, 0)))
        self.assertEqual(host.session.permission, PermissionDecision.DENY)

    def test_persistent_usb_grant_does_not_grant_bluetooth(self):
        _, usb = self.worker("usb")
        usb.session.on_frame(hello_frame())
        usb.resolve_permission()
        self.controller.prompts.answer(
            self.controller.prompts.pending()[0].id, PermissionDecision.ALLOW_ALWAYS
        )
        usb.resolve_permission()
        self.assertTrue(self.permissions.contains(usb.session.hello))
        _, bluetooth = self.worker("bluetooth")
        bluetooth.session.on_frame(hello_frame())
        self.assertTrue(bluetooth.session.permission_pending)
        self.assertEqual(bluetooth.session.permission, PermissionDecision.DENY)

    def test_access_revoke_is_not_pairing_revoke_and_cancels_request(self):
        worker, host = self.worker("usb")
        host.session.on_frame(hello_frame())
        worker.command("allow")
        self.assertEqual(worker.tick(host)[0].payload, bytes((2, 0)))
        self.pairs.establish(host.session.hello, b"x" * 32)
        requests = []
        host.session.on_request = requests.append
        host.session.on_frame(
            Frame(
                MessageType.REQUEST_START,
                encode_request_start(RequestStart(1, 1000, "https://example.com")),
                request_id=7,
            )
        )
        host.session.on_frame(
            Frame(
                MessageType.REQUEST_END,
                request_id=7,
                sequence=1,
                flags=FrameFlags.FINAL,
            )
        )
        self.assertEqual(len(requests), 1)
        worker.command("revoke")
        replies = worker.tick(host)
        self.assertEqual(replies[0].payload, bytes((0, 2)))
        self.assertTrue(requests[0].cancel.is_set())
        self.assertFalse(self.permissions.contains(host.session.hello))
        self.assertEqual(len(self.pairs.records()), 1)

    def test_nonce_change_discards_old_consent(self):
        _, host = self.worker("usb")
        host.session.on_frame(hello_frame())
        host.resolve_permission()
        old = self.controller.prompts.pending()[0]
        hello = dataclasses.replace(
            decode_hello(hello_frame().payload), client_nonce=999
        )
        host.session.on_frame(Frame(MessageType.HELLO, encode_hello(hello)))
        host.resolve_permission()
        self.controller.prompts.answer(old.id, PermissionDecision.ALLOW_ALWAYS)
        self.assertTrue(host.session.permission_pending)
        self.assertEqual(len(self.controller.prompts.pending()), 1)

    def test_generation_guard_does_not_publish_old_connection(self):
        old, _ = self.worker("usb")
        new, _ = self.worker("usb")
        self.controller.publish(new, ConnectionSnapshot("usb", True, status="new"))
        self.controller.publish(old, ConnectionSnapshot("usb", True, status="old"))
        self.assertEqual(self.controller.snapshot("usb").status, "new")

    def test_pairing_revoke_removes_row_and_token_not_usb_permission(self):
        hello = decode_hello(hello_frame().payload)
        self.permissions.grant(hello)
        self.pairs.establish(hello, b"x" * 32)
        key = self.pairs.key(hello)
        self.controller.revoke_pairing(key)
        deadline = time.monotonic() + 2
        while self.pairs.records() and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertFalse(self.pairs.records())
        self.assertFalse(self.pairs.vault.values)
        self.assertTrue(self.permissions.contains(hello))

    def test_port_discovery_uses_official_id_shared_by_both_cdc_ports(self):
        from types import SimpleNamespace

        ports = [
            SimpleNamespace(vid=0x0483, pid=0x5740, device="cli"),
            SimpleNamespace(
                vid=0x0483, pid=0x5740, device="/dev/cu.usbmodemflip_Mico3"
            ),
            SimpleNamespace(vid=0x1234, pid=0x5740, device="unrelated"),
        ]
        with patch("serial.tools.list_ports.comports", return_value=ports):
            self.assertEqual(bridge_ports(), ["/dev/cu.usbmodemflip_Mico3", "cli"])

    def test_rapid_off_on_waits_for_the_still_closing_serial_owner(self):
        entered, release, second = (
            threading.Event(),
            threading.Event(),
            threading.Event(),
        )
        calls = []

        def delayed_close(_port, _host, **_hooks):
            calls.append(1)
            (entered if len(calls) == 1 else second).set()
            release.wait(2)

        self.controller.port_provider = lambda: ["fake"]
        with patch("host.fibp_host.desktop_backend.serve_port", delayed_close):
            try:
                self.controller.set_enabled("usb", True)
                self.assertTrue(entered.wait(1))
                self.controller.set_enabled("usb", False)
                self.controller.set_enabled("usb", True)
                self.assertFalse(
                    second.wait(0.1), "A second owner opened before the old port closed"
                )
                release.set()
                self.assertTrue(second.wait(1))
            finally:
                release.set()
                self.controller.close()


class DesktopPTYTests(unittest.TestCase):
    @unittest.skipUnless(__import__("os").name == "posix", "PTY is POSIX-only")
    def test_usb_worker_handshake_async_consent_and_disconnect(self):
        import os, pty, select, tty
        from scripts.fibp_codec import StreamDecoder, encode_frame

        master, slave = pty.openpty()
        tty.setraw(slave)
        port = os.ttyname(slave)
        os.close(slave)
        self.addCleanup(lambda: os.close(master))
        with tempfile.TemporaryDirectory() as temp:
            controller = DesktopController(
                PermissionStore(Path(temp) / "permissions.json"), lambda: [port]
            )
            self.addCleanup(controller.close)
            controller.set_enabled("usb", True)
            time.sleep(0.1)
            os.write(master, encode_frame(hello_frame()))
            deadline = time.monotonic() + 2
            while not controller.prompts.pending() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertEqual(
                len(controller.prompts.pending()),
                1,
                (controller.snapshot("usb"), tuple(controller.logs)),
            )
            controller.prompts.answer(
                controller.prompts.pending()[0].id, PermissionDecision.ALLOW_ONCE
            )
            decoder, received = StreamDecoder(), []
            while time.monotonic() < deadline and not any(
                f.message_type == MessageType.PERMISSION_STATUS for f in received
            ):
                if select.select([master], [], [], 0.05)[0]:
                    received.extend(decoder.feed(os.read(master, 4096)))
            self.assertTrue(
                any(
                    f.message_type == MessageType.PERMISSION_STATUS
                    and f.payload == b"\1\0"
                    for f in received
                )
            )
            controller.set_enabled("usb", False)
            self.assertFalse(controller.snapshot("usb").enabled)
            self.assertFalse(controller.prompts.pending())


class DesktopBluetoothTests(unittest.IsolatedAsyncioTestCase):
    async def exercise_consent(self, decision):
        from tests.test_bluetooth_host import hello
        from host.fibp_host.bluetooth_pairing import WAITING, NEEDED, RESULT
        from host.fibp_host.cli import BridgeHost
        from scripts.fibp_codec import encode_frame

        with tempfile.TemporaryDirectory() as temp:
            store = PairingStore(Path(temp) / "pairs.json", Vault())
            controller = DesktopController(
                PermissionStore(Path(temp) / "permissions.json"),
                lambda: [],
                lambda: store,
            )
            worker = ConnectionWorker(controller, "bluetooth")
            controller.workers["bluetooth"] = worker
            prefix = decode_hello(hello().payload).client_nonce.to_bytes(8, "little")
            incoming = [
                hello(),
                Frame(WAITING, prefix + b"\3"),
                Frame(NEEDED, prefix + b"\2"),
            ]
            requests, prompts = [], []

            class Peer:
                def __init__(self):
                    self.sent = []

                async def send(self, frame):
                    self.sent.append(frame)
                    if frame.message_type == 0x41:
                        incoming.append(Frame(RESULT, prefix + b"\1"))
                    elif frame.message_type == MessageType.PERMISSION_STATUS:
                        incoming.extend(
                            [
                                Frame(
                                    MessageType.REQUEST_START,
                                    encode_request_start(
                                        RequestStart(1, 1000, "https://example.com")
                                    ),
                                    request_id=45,
                                ),
                                Frame(
                                    MessageType.REQUEST_END,
                                    request_id=45,
                                    sequence=1,
                                    flags=FrameFlags.FINAL,
                                ),
                                Frame(MessageType.DISCONNECT),
                            ]
                        )

                async def read(self):
                    await asyncio.sleep(0.001)
                    return encode_frame(incoming.pop(0)) if incoming else b""

            peer = Peer()

            async def fake_connect(serve):
                task = asyncio.create_task(serve(peer))
                try:
                    while not task.done():
                        for item in controller.prompts.pending():
                            prompts.append(item.kind)
                            self.assertFalse(
                                requests, "Network started before explicit consent"
                            )
                            controller.prompts.answer(
                                item.id, "123456" if item.kind == "code" else decision
                            )
                        await asyncio.sleep(0.001)
                    await task
                finally:
                    if not task.done():
                        task.cancel()

            try:
                with patch(
                    "host.fibp_host.bluetooth_transport.connect_demo", fake_connect
                ), patch.object(
                    BridgeHost,
                    "_start_request",
                    lambda _host, request: requests.append(request),
                ):
                    await asyncio.wait_for(worker.ble(), 2)
                self.assertEqual(prompts, ["code", "permission"])
                self.assertEqual(
                    len(store.records()), 1, "Deny must not remove bridge recognition"
                )
                self.assertEqual(
                    len(requests), int(decision == PermissionDecision.ALLOW_ONCE)
                )
                if requests:
                    self.assertTrue(
                        requests[0].cancel.is_set(), "Disconnect must cancel network"
                    )
                self.assertFalse(controller.prompts.pending())
            finally:
                worker.task = None
                controller.close()

    async def test_gui_bluetooth_code_then_deny_never_runs_network(self):
        await self.exercise_consent(PermissionDecision.DENY)

    async def test_gui_bluetooth_code_then_allow_and_disconnect(self):
        await self.exercise_consent(PermissionDecision.ALLOW_ONCE)


if __name__ == "__main__":
    unittest.main()
