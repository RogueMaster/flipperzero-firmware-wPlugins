from __future__ import annotations

import json
import unittest
from dataclasses import replace
from pathlib import Path

from scripts.fibp_codec import (
    Capability,
    Frame,
    FrameDecodeError,
    FrameFlags,
    HeaderField,
    Hello,
    HelloAck,
    MAX_FRAME_PAYLOAD,
    MIN_NEGOTIATED_PAYLOAD,
    MessageType,
    ParseIssueCode,
    RequestStart,
    ResponseEnd,
    ResponseStart,
    StreamDecoder,
    crc32,
    decode_frame,
    decode_header,
    decode_hello,
    decode_hello_ack,
    decode_request_start,
    decode_response_end,
    decode_response_start,
    encode_frame,
    encode_header,
    encode_hello,
    encode_hello_ack,
    encode_ping_token,
    encode_request_start,
    encode_response_end,
    encode_response_start,
    fragment_bytes,
)


ROOT = Path(__file__).resolve().parents[1]
VECTOR_PATH = ROOT / "protocol" / "test-vectors" / "v1.json"


class FibpCodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.vectors = json.loads(VECTOR_PATH.read_text(encoding="utf-8"))["vectors"]

    def test_crc_reference(self) -> None:
        self.assertEqual(crc32(b"123456789"), 0xCBF43926)

    def test_normative_complete_vectors_round_trip(self) -> None:
        for vector in self.vectors:
            if "expectedError" in vector:
                continue
            with self.subTest(vector=vector["name"]):
                raw = bytes.fromhex(vector["hex"])
                frame = decode_frame(raw)
                self.assertEqual(encode_frame(frame), raw)

        ping = decode_frame(bytes.fromhex(self.vectors[0]["hex"]))
        self.assertEqual(ping.message_type, MessageType.PING)
        self.assertEqual(ping.request_id, 0)
        self.assertEqual(ping.sequence, 1)
        self.assertEqual(ping.payload, bytes.fromhex("0807060504030201"))

        request_end = decode_frame(bytes.fromhex(self.vectors[2]["hex"]))
        self.assertEqual(request_end.message_type, MessageType.REQUEST_END)
        self.assertEqual(request_end.flags, FrameFlags.FINAL)

    def test_normative_oversized_header_is_rejected_immediately(self) -> None:
        raw = bytes.fromhex(self.vectors[-1]["hex"])
        decoder = StreamDecoder()
        self.assertEqual(decoder.feed(raw), [])
        self.assertEqual(len(decoder.issues), 1)
        self.assertEqual(decoder.issues[0].code, ParseIssueCode.PAYLOAD_TOO_LARGE)
        self.assertLessEqual(decoder.buffered_bytes, 3)

    def test_every_split_and_bytewise(self) -> None:
        raw = bytes.fromhex(self.vectors[0]["hex"])
        for split in range(len(raw) + 1):
            with self.subTest(split=split):
                decoder = StreamDecoder()
                frames = decoder.feed(raw[:split])
                frames.extend(decoder.feed(raw[split:]))
                self.assertEqual(len(frames), 1)
                self.assertEqual(frames[0].message_type, MessageType.PING)
                self.assertEqual(decoder.issues, [])

        decoder = StreamDecoder()
        frames = []
        for byte in raw:
            frames.extend(decoder.feed(bytes((byte,))))
        self.assertEqual(len(frames), 1)
        self.assertEqual(decoder.buffered_bytes, 0)

    def test_fragment_pattern_and_concatenated_frames(self) -> None:
        ping = bytes.fromhex(self.vectors[0]["hex"])
        pong = bytes.fromhex(self.vectors[1]["hex"])
        chunks = list(fragment_bytes(b"garbage" + ping + pong, (1, 3, 7, 64)))
        decoder = StreamDecoder()
        frames = []
        for chunk in chunks:
            frames.extend(decoder.feed(chunk))
        self.assertEqual([frame.message_type for frame in frames], [MessageType.PING, MessageType.PONG])
        self.assertEqual(decoder.issues, [])

    def test_bad_frame_crc_does_not_dispatch_and_recovers(self) -> None:
        damaged = bytearray.fromhex(self.vectors[0]["hex"])
        damaged[28] ^= 0x01
        valid = bytes.fromhex(self.vectors[1]["hex"])
        decoder = StreamDecoder()
        frames = decoder.feed(bytes(damaged) + valid)
        self.assertEqual([frame.message_type for frame in frames], [MessageType.PONG])
        self.assertEqual(decoder.issues[0].code, ParseIssueCode.BAD_FRAME_CRC)

    def test_bad_header_crc_does_not_dispatch_and_recovers(self) -> None:
        damaged = bytearray.fromhex(self.vectors[0]["hex"])
        damaged[24] ^= 0x01
        valid = bytes.fromhex(self.vectors[0]["hex"])
        decoder = StreamDecoder()
        frames = decoder.feed(bytes(damaged) + valid)
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].message_type, MessageType.PING)
        self.assertEqual(decoder.issues[0].code, ParseIssueCode.BAD_HEADER_CRC)

    def test_exact_decoder_rejects_trailing_and_missing_bytes(self) -> None:
        raw = bytes.fromhex(self.vectors[0]["hex"])
        for candidate in (raw[:-1], raw + b"\x00"):
            with self.subTest(length=len(candidate)):
                with self.assertRaises(FrameDecodeError) as caught:
                    decode_frame(candidate)
                self.assertEqual(caught.exception.code, ParseIssueCode.TRUNCATED_FRAME)

    def test_finish_reports_partial_frame(self) -> None:
        decoder = StreamDecoder()
        decoder.feed(b"FIBP\x01")
        issues = decoder.finish()
        self.assertEqual(issues[0].code, ParseIssueCode.TRUNCATED_FRAME)
        self.assertEqual(decoder.buffered_bytes, 0)

    def test_maximum_payload_round_trip(self) -> None:
        frame = Frame(MessageType.REQUEST_BODY_CHUNK, bytes(range(256)) * 2, request_id=1)
        raw = encode_frame(frame)
        self.assertEqual(len(raw), 28 + MAX_FRAME_PAYLOAD + 4)
        self.assertEqual(decode_frame(raw), frame)
        with self.assertRaises(ValueError):
            Frame(MessageType.REQUEST_BODY_CHUNK, b"x" * (MAX_FRAME_PAYLOAD + 1))

    def test_hello_and_ack_payload_round_trip(self) -> None:
        hello = Hello(
            1,
            0,
            1,
            0,
            int(Capability.HTTPS_GET | Capability.CANCELLATION),
            512,
            16384,
            0x0102030405060708,
            "Flipper Zero",
            "Mico",
            1,
            bytes.fromhex("0102030405060708"),
            "0.1.0",
        )
        self.assertEqual(decode_hello(encode_hello(hello)), hello)

        ack = HelloAck(1, 0, hello.capabilities, 512, 16384, hello.client_nonce, 42)
        self.assertEqual(decode_hello_ack(encode_hello_ack(ack)), ack)

        minimum_hello = replace(hello, maximum_rx_payload=MIN_NEGOTIATED_PAYLOAD)
        self.assertEqual(decode_hello(encode_hello(minimum_hello)), minimum_hello)
        minimum_ack = replace(ack, maximum_payload=MIN_NEGOTIATED_PAYLOAD)
        self.assertEqual(decode_hello_ack(encode_hello_ack(minimum_ack)), minimum_ack)
        with self.assertRaises(ValueError):
            encode_hello(replace(hello, maximum_rx_payload=MIN_NEGOTIATED_PAYLOAD - 1))
        with self.assertRaises(ValueError):
            encode_hello_ack(replace(ack, maximum_payload=MIN_NEGOTIATED_PAYLOAD - 1))

    def test_request_header_and_response_payload_round_trip(self) -> None:
        request = RequestStart(1, 15000, "https://example.com/demo", 0, 1)
        self.assertEqual(decode_request_start(encode_request_start(request)), request)

        header = HeaderField("accept", "text/plain")
        self.assertEqual(decode_header(encode_header(header)), header)

        response = ResponseStart(200, 1, 123)
        self.assertEqual(decode_response_start(encode_response_start(response)), response)
        end = ResponseEnd(0, 123)
        self.assertEqual(decode_response_end(encode_response_end(end)), end)

    def test_text_and_http_header_injection_are_rejected(self) -> None:
        for header in (
            HeaderField("bad header", "value"),
            HeaderField("accept", "ok\r\ninjected: yes"),
        ):
            with self.subTest(header=header):
                with self.assertRaises(ValueError):
                    encode_header(header)

    def test_request_end_final_flag_is_preserved(self) -> None:
        frame = Frame(
            MessageType.REQUEST_END,
            request_id=0x11223344,
            sequence=3,
            flags=FrameFlags.FINAL,
        )
        self.assertEqual(decode_frame(encode_frame(frame)).flags, FrameFlags.FINAL)

    def test_ping_encoder_matches_normative_vector(self) -> None:
        frame = Frame(
            MessageType.PING,
            encode_ping_token(0x0102030405060708),
            sequence=1,
        )
        self.assertEqual(encode_frame(frame).hex(), self.vectors[0]["hex"])


if __name__ == "__main__":
    unittest.main()
