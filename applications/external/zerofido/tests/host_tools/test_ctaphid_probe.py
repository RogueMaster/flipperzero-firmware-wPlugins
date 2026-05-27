"""Tests for CTAPHID frame construction, parsing, and response decoding helpers."""

from __future__ import annotations

import io
import unittest

import cbor2

from host_tools import ctaphid_probe


class CtaphidProbeTests(unittest.TestCase):
    def test_transact_populates_optional_timing(self) -> None:
        cid = 0x01020304
        response_payload = b"\x00\xa0"
        response_frames = ctaphid_probe.build_frames(cid, ctaphid_probe.CBOR, response_payload)
        timing: dict[str, object] = {}

        class FakeDevice:
            def __init__(self) -> None:
                self.writes: list[bytes] = []
                self._reads = [b"\x00" + frame for frame in response_frames]

            def write(self, report: bytes) -> int:
                self.writes.append(report)
                return len(report)

            def read(self, size: int, timeout_ms: int) -> list[int]:
                self.assert_size = size
                self.assert_timeout_ms = timeout_ms
                return list(self._reads.pop(0))

        device = FakeDevice()

        response_cid, response_cmd, response = ctaphid_probe.transact(
            device,
            cid,
            ctaphid_probe.CBOR,
            bytes([ctaphid_probe.CTAP_GET_INFO]),
            3000,
            False,
            timing=timing,
        )

        self.assertEqual(response_cid, cid)
        self.assertEqual(response_cmd, ctaphid_probe.CBOR)
        self.assertEqual(response, response_payload)
        self.assertEqual(timing["cid"], "0x01020304")
        self.assertEqual(timing["command_hex"], "0x90")
        self.assertEqual(timing["tx_frames"], 1)
        self.assertEqual(timing["rx_frames"], 1)
        self.assertGreaterEqual(float(timing["total_ms"]), 0.0)

    def test_parse_credential_id_values_supports_multiple_ids(self) -> None:
        encoded = "AQID,BAUG"

        decoded = ctaphid_probe.parse_credential_id_values(encoded)

        self.assertEqual(decoded, [b"\x01\x02\x03", b"\x04\x05\x06"])

    def test_build_allow_list_extends_to_requested_count(self) -> None:
        allow_list = ctaphid_probe.build_allow_list([b"\x01\x02"], 3)

        self.assertEqual(len(allow_list), 3)
        self.assertEqual(allow_list[0]["id"], b"\x01\x02")
        self.assertTrue(all(entry["type"] == "public-key" for entry in allow_list))

    def test_build_get_assertion_request_keeps_trailing_bytes(self) -> None:
        trailing = b"\xaa\xbb"
        payload = ctaphid_probe.build_get_assertion_request(
            "zerofido.local",
            [b"\x11\x22"],
            "discouraged",
            empty_pin_auth=False,
            omit_client_data_hash=False,
            silent=False,
            include_rk_option=False,
            allow_list_count=33,
            trailing_bytes=trailing,
        )

        decoded = cbor2.load(io.BytesIO(payload[:-len(trailing)]))
        self.assertEqual(len(decoded[3]), 33)
        self.assertEqual(payload[-len(trailing) :], trailing)

    def test_build_make_credential_request_keeps_trailing_bytes(self) -> None:
        trailing = b"\x00\xff"
        payload = ctaphid_probe.build_make_credential_request(
            "zerofido.local",
            empty_pin_auth=True,
            omit_client_data_hash=False,
            trailing_bytes=trailing,
        )

        decoded, leftover = ctaphid_probe.decode_single_cbor_item(payload)
        self.assertIn(1, decoded)
        self.assertEqual(decoded[8], b"")
        self.assertEqual(leftover, trailing)

    def test_build_make_credential_request_can_prefer_packed_attestation(self) -> None:
        payload = ctaphid_probe.build_make_credential_request(
            "zerofido.local",
            empty_pin_auth=False,
            omit_client_data_hash=False,
            resident_key=False,
            attestation_formats=["packed"],
        )

        decoded = cbor2.loads(payload)

        self.assertEqual(decoded[7]["rk"], False)
        self.assertEqual(decoded[11], ["packed"])

    def test_build_frames_fragments_long_payload(self) -> None:
        payload = bytes(range(80))

        frames = ctaphid_probe.build_frames(0x01020304, ctaphid_probe.PING, payload)

        self.assertEqual(len(frames), 2)
        self.assertEqual(frames[0][4], ctaphid_probe.PING)
        self.assertEqual(frames[1][4], 0)

    def test_build_u2f_version_apdu(self) -> None:
        self.assertEqual(
            ctaphid_probe.build_u2f_version_apdu(),
            bytes([0x00, ctaphid_probe.U2F_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00]),
        )

    def test_build_u2f_invalid_cla_version_apdu(self) -> None:
        self.assertEqual(
            ctaphid_probe.build_u2f_invalid_cla_version_apdu(),
            bytes([0x6D, ctaphid_probe.U2F_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00]),
        )
        self.assertEqual(
            ctaphid_probe.build_u2f_invalid_cla_version_apdu(0xC3),
            bytes([0xC3, ctaphid_probe.U2F_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00]),
        )

    def test_build_u2f_version_with_data_apdu(self) -> None:
        self.assertEqual(
            ctaphid_probe.build_u2f_version_with_data_apdu(b"\x12\x34"),
            bytes([0x00, ctaphid_probe.U2F_VERSION, 0x00, 0x00, 0x00, 0x00, 0x02, 0x12, 0x34]),
        )

    def test_decode_u2f_response_payload_exposes_status_word_metadata(self) -> None:
        parsed = ctaphid_probe.decode_u2f_response_payload(b"U2F_V2\x90\x00")

        self.assertEqual(parsed["payload_hex"], b"U2F_V2".hex())
        self.assertEqual(parsed["status_words_hex"], "9000")
        self.assertEqual(parsed["status_word"], 0x9000)
        self.assertEqual(parsed["status_word_name"], "SW_NO_ERROR")

    def test_decode_u2f_response_payload_handles_status_only_errors(self) -> None:
        parsed = ctaphid_probe.decode_u2f_response_payload(bytes.fromhex("6e00"))

        self.assertEqual(parsed["payload_hex"], "")
        self.assertEqual(parsed["status_words_hex"], "6e00")
        self.assertEqual(parsed["status_word"], 0x6E00)
        self.assertEqual(parsed["status_word_name"], "SW_CLA_NOT_SUPPORTED")

    def test_decode_u2f_response_payload_names_wrong_data(self) -> None:
        parsed = ctaphid_probe.decode_u2f_response_payload(bytes.fromhex("6a80"))

        self.assertEqual(parsed["status_word"], 0x6A80)
        self.assertEqual(parsed["status_word_name"], "SW_WRONG_DATA")

    def test_extract_make_credential_attestation_certificate_reads_x5c_leaf(self) -> None:
        cert_der = bytes.fromhex("3003020101")
        response_payload = b"\x00" + cbor2.dumps(
            {
                1: "packed",
                2: b"auth-data",
                3: {
                    "alg": -7,
                    "sig": b"signature",
                    "x5c": [cert_der],
                },
            }
        )

        self.assertEqual(
            ctaphid_probe.extract_make_credential_attestation_certificate(response_payload),
            cert_der,
        )

    def test_build_u2f_authenticate_apdu_sets_length(self) -> None:
        challenge = bytes(range(32))
        app_id = bytes(range(32, 64))
        key_handle = bytes(range(65))

        payload = ctaphid_probe.build_u2f_authenticate_apdu(challenge, app_id, key_handle)

        self.assertEqual(payload[1], ctaphid_probe.U2F_AUTHENTICATE)
        self.assertEqual(payload[2], ctaphid_probe.U2F_AUTH_ENFORCE)
        self.assertEqual(payload[4:7], bytes([0x00, 0x00, 0x82]))
        self.assertEqual(payload[7:39], challenge)
        self.assertEqual(payload[39:71], app_id)
        self.assertEqual(payload[71], len(key_handle))

    def test_build_u2f_authenticate_apdu_supports_dont_enforce_mode(self) -> None:
        challenge = bytes(range(32))
        app_id = bytes(range(32, 64))
        key_handle = bytes(range(65))

        payload = ctaphid_probe.build_u2f_authenticate_apdu(
            challenge, app_id, key_handle, mode=ctaphid_probe.U2F_AUTH_DONT_ENFORCE
        )

        self.assertEqual(payload[2], ctaphid_probe.U2F_AUTH_DONT_ENFORCE)

    def test_build_u2f_register_apdu_sets_expected_length(self) -> None:
        challenge = bytes(range(32))
        app_id = bytes(range(32, 64))

        payload = ctaphid_probe.build_u2f_register_apdu(challenge, app_id)

        self.assertEqual(len(payload), 71)
        self.assertEqual(payload[1], ctaphid_probe.U2F_REGISTER)
        self.assertEqual(payload[4:7], bytes([0x00, 0x00, 0x40]))
        self.assertEqual(payload[7:39], challenge)
        self.assertEqual(payload[39:71], app_id)

    def test_pin_hash16_matches_expected_length(self) -> None:
        self.assertEqual(len(ctaphid_probe.pin_hash16("2468")), 16)

    def test_expect_init_response_supports_same_cid_resync(self) -> None:
        nonce = b"12345678"
        payload = nonce + bytes([0x04, 0x03, 0x02, 0x01, 0x02, 0x01, 0x00, 0x03, 0x05])

        allocated = ctaphid_probe.expect_init_response(
            0x01020304,
            ctaphid_probe.INIT,
            payload,
            nonce,
            expected_response_cid=0x01020304,
        )

        self.assertEqual(allocated, 0x01020304)

    def test_expect_init_response_accepts_extended_payloads(self) -> None:
        nonce = b"12345678"
        payload = nonce + bytes([0x04, 0x03, 0x02, 0x01, 0x02, 0x01, 0x00, 0x03, 0x05, 0xAA])

        allocated = ctaphid_probe.expect_init_response(
            ctaphid_probe.BROADCAST_CID,
            ctaphid_probe.INIT,
            payload,
            nonce,
        )

        self.assertEqual(allocated, 0x01020304)

    def test_summarize_samples_reports_median(self) -> None:
        summary = ctaphid_probe.summarize_samples([4.0, 1.0, 9.0])

        self.assertEqual(summary["iterations"], 3)
        self.assertEqual(summary["min_ms"], 1.0)
        self.assertEqual(summary["median_ms"], 4.0)
        self.assertEqual(summary["max_ms"], 9.0)


if __name__ == "__main__":
    unittest.main()
