from __future__ import annotations

import unittest

from host.fibp_host.session import HostSession, PermissionDecision
from scripts.fibp_codec import (
    Frame,
    FrameFlags,
    Hello,
    MessageType,
    RequestStart,
    decode_hello_ack,
    decode_permission_status,
    encode_hello,
    encode_request_start,
)


def hello_frame() -> Frame:
    hello = Hello(
        1,
        0,
        1,
        0,
        0x1F,
        512,
        65536,
        0x1234,
        "Flipper Zero",
        "Mico",
        1,
        b"12345678",
        "0.3",
    )
    return Frame(MessageType.HELLO, encode_hello(hello), sequence=0)


class HostSessionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.requests = []
        self.session = HostSession(self.requests.append, lambda _hello: False)

    def test_handshake_prompts_then_grants_once(self) -> None:
        replies = self.session.on_frame(hello_frame())
        self.assertEqual(
            [item.message_type for item in replies],
            [MessageType.HELLO_ACK, MessageType.PERMISSION_REQUIRED],
        )
        ack = decode_hello_ack(replies[0].payload)
        self.assertEqual(ack.echoed_client_nonce, 0x1234)
        self.assertTrue(self.session.permission_pending)
        granted = self.session.resolve_permission(PermissionDecision.ALLOW_ONCE)
        self.assertEqual(decode_permission_status(granted[0].payload), (1, 0))

    def test_complete_get_is_delivered_to_request_worker(self) -> None:
        self.session.on_frame(hello_frame())
        self.session.resolve_permission(PermissionDecision.ALLOW_ONCE)
        request_id = 42
        start = RequestStart(1, 10000, "https://example.com/")
        self.assertEqual(
            self.session.on_frame(
                Frame(
                    MessageType.REQUEST_START,
                    encode_request_start(start),
                    request_id=request_id,
                    sequence=0,
                )
            ),
            [],
        )
        self.assertEqual(
            self.session.on_frame(
                Frame(
                    MessageType.REQUEST_END,
                    request_id=request_id,
                    sequence=1,
                    flags=FrameFlags.FINAL,
                )
            ),
            [],
        )
        self.assertEqual(len(self.requests), 1)
        self.assertEqual(self.requests[0].start.url, "https://example.com/")

    def test_cancel_sets_worker_event(self) -> None:
        self.test_complete_get_is_delivered_to_request_worker()
        request = self.requests[0]
        self.session.on_frame(
            Frame(
                MessageType.CANCEL, b"\x00", request_id=request.request_id, sequence=2
            )
        )
        self.assertTrue(request.cancel.is_set())

    def test_bad_cancel_sequence_has_no_side_effect(self) -> None:
        self.test_complete_get_is_delivered_to_request_worker()
        request = self.requests[0]
        replies = self.session.on_frame(
            Frame(
                MessageType.CANCEL,
                b"\x00",
                request_id=request.request_id,
                sequence=99,
            )
        )
        self.assertEqual(replies[0].message_type, MessageType.ERROR)
        self.assertFalse(request.cancel.is_set())

    def test_duplicate_request_id_is_rejected(self) -> None:
        self.test_complete_get_is_delivered_to_request_worker()
        self.session.finish_request(42)
        start = RequestStart(1, 10000, "https://example.com/")
        replies = self.session.on_frame(
            Frame(
                MessageType.REQUEST_START,
                encode_request_start(start),
                request_id=42,
                sequence=0,
            )
        )
        self.assertEqual(replies[0].message_type, MessageType.ERROR)

    def test_persistent_grant_skips_prompt(self) -> None:
        session = HostSession(self.requests.append, lambda _hello: True)
        replies = session.on_frame(hello_frame())
        self.assertEqual(replies[1].message_type, MessageType.PERMISSION_STATUS)
        self.assertEqual(decode_permission_status(replies[1].payload), (2, 1))


if __name__ == "__main__":
    unittest.main()
