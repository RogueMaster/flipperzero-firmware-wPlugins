"""Tests for conformance-suite command construction and fixture handling."""

from __future__ import annotations

import copy
import threading
import time
import unittest
from unittest import mock

from host_tools import ctaphid_probe
from host_tools.conformance_suite import ConformanceSuiteService, MatchedDevice


class ConformanceSuiteTests(unittest.TestCase):
    def test_manifest_rows_index_scenario_ids(self) -> None:
        service = ConformanceSuiteService()

        self.assertIn("transport.init", service.scenario_to_rows["transport_init"])
        self.assertIn("clientpin.get_retries", service.scenario_to_rows["clientpin_get_retries"])

    def test_build_report_fails_closed_for_required_failures(self) -> None:
        service = ConformanceSuiteService()
        matched = MatchedDevice(
            ctaphid_probe.HidDeviceInfo(
                path=b"dummy",
                vendor_id=0x0483,
                product_id=0x5741,
                usage_page=0xF1D0,
                usage=0x01,
                product_string="ZeroFIDO",
                serial_number="fixture-1",
            )
        )
        report = service._build_report(
            "run-1",
            "unit_test",
            matched,
            [
                {
                    "id": "transport_init",
                    "phase": "transport",
                    "rows": ["transport.init"],
                    "status": "failed",
                    "summary": "boom",
                    "error": "boom",
                    "details": {},
                    "evidence": {},
                    "started_at": "2026-04-22T00:00:00Z",
                    "ended_at": "2026-04-22T00:00:01Z",
                    "duration_ms": 1,
                }
            ],
            None,
        )

        self.assertEqual(report["gate_result"], "failed")

    def test_build_report_marks_missing_required_rows_blocked(self) -> None:
        service = ConformanceSuiteService()
        matched = MatchedDevice(
            ctaphid_probe.HidDeviceInfo(
                path=b"dummy",
                vendor_id=0x0483,
                product_id=0x5741,
                usage_page=0xF1D0,
                usage=0x01,
                product_string="ZeroFIDO",
                serial_number="fixture-1",
            )
        )
        report = service._build_report("run-2", "unit_test", matched, [], None)

        self.assertEqual(report["gate_result"], "blocked")

    def test_build_report_fails_for_unmapped_scenario_failure(self) -> None:
        service = ConformanceSuiteService()
        matched = MatchedDevice(
            ctaphid_probe.HidDeviceInfo(
                path=b"dummy",
                vendor_id=0x0483,
                product_id=0x5741,
                usage_page=0xF1D0,
                usage=0x01,
                product_string="ZeroFIDO",
                serial_number="fixture-1",
            )
        )
        report = service._build_report(
            "run-3",
            "unit_test",
            matched,
            [
                {
                    "id": "unmapped_failure",
                    "phase": "unit",
                    "rows": [],
                    "status": "failed",
                    "summary": "unmapped failed",
                    "error": "boom",
                    "details": {},
                    "evidence": {},
                    "started_at": "2026-04-22T00:00:00Z",
                    "ended_at": "2026-04-22T00:00:01Z",
                    "duration_ms": 1,
                }
            ],
            None,
        )

        self.assertEqual(report["gate_result"], "failed")

    def test_validate_static_metadata_rejects_built_in_uv_claims(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        broken = copy.deepcopy(metadata)
        broken["matcherProtection"] = ["software"]

        with self.assertRaisesRegex(RuntimeError, "matcherProtection"):
            service._validate_static_metadata(broken)

    def test_validate_get_info_response_accepts_omitted_uv_for_clientpin_only_authenticator(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        decoded = {
            1: metadata["authenticatorGetInfo"]["versions"],
            2: metadata["authenticatorGetInfo"]["extensions"],
            3: bytes.fromhex(metadata["aaguid"].replace("-", "")),
            4: {
                "rk": True,
                "up": True,
                "plat": False,
                "clientPin": False,
            },
            5: metadata["authenticatorGetInfo"]["maxMsgSize"],
            6: metadata["authenticatorGetInfo"]["pinUvAuthProtocols"],
        }

        validated = service._validate_get_info_response(decoded, metadata)
        self.assertNotIn("uv", validated["options"])

    def test_validate_get_info_response_rejects_unexpected_enterprise_attestation_option(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        decoded = {
            1: metadata["authenticatorGetInfo"]["versions"],
            2: metadata["authenticatorGetInfo"]["extensions"],
            3: bytes.fromhex(metadata["aaguid"].replace("-", "")),
            4: {
                "rk": True,
                "up": True,
                "plat": False,
                "clientPin": False,
                "uv": False,
                "ep": True,
            },
            5: metadata["authenticatorGetInfo"]["maxMsgSize"],
            6: metadata["authenticatorGetInfo"]["pinUvAuthProtocols"],
        }

        with self.assertRaisesRegex(RuntimeError, "unsupported option 'ep'"):
            service._validate_get_info_response(decoded, metadata)

    def test_validate_get_info_response_rejects_unexpected_min_pin_length(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        decoded = {
            1: metadata["authenticatorGetInfo"]["versions"],
            2: metadata["authenticatorGetInfo"]["extensions"],
            3: bytes.fromhex(metadata["aaguid"].replace("-", "")),
            4: {
                "rk": True,
                "up": True,
                "plat": False,
                "clientPin": False,
                "uv": False,
            },
            5: metadata["authenticatorGetInfo"]["maxMsgSize"],
            6: metadata["authenticatorGetInfo"]["pinUvAuthProtocols"],
            13: 4,
        }

        with self.assertRaisesRegex(RuntimeError, "minPINLength"):
            service._validate_get_info_response(decoded, metadata)

    def test_validate_get_info_response_rejects_unexpected_pin_uv_auth_token_option(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        decoded = {
            1: metadata["authenticatorGetInfo"]["versions"],
            2: metadata["authenticatorGetInfo"]["extensions"],
            3: bytes.fromhex(metadata["aaguid"].replace("-", "")),
            4: {
                "rk": True,
                "up": True,
                "plat": False,
                "clientPin": False,
                "pinUvAuthToken": True,
                "uv": False,
            },
            5: metadata["authenticatorGetInfo"]["maxMsgSize"],
            6: metadata["authenticatorGetInfo"]["pinUvAuthProtocols"],
        }

        with self.assertRaisesRegex(RuntimeError, "unsupported option 'pinUvAuthToken'"):
            service._validate_get_info_response(decoded, metadata)

    def test_validate_get_info_response_accepts_experimental_2_1_options_when_metadata_matches(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        metadata["authenticatorGetInfo"]["versions"] = ["FIDO_2_1", "FIDO_2_0", "U2F_V2"]
        metadata["authenticatorGetInfo"]["options"]["pinUvAuthToken"] = True
        metadata["authenticatorGetInfo"]["options"]["makeCredUvNotRqd"] = True
        metadata["authenticatorGetInfo"]["options"]["clientPin"] = True
        metadata["authenticatorGetInfo"]["pinUvAuthProtocols"] = [2, 1]
        metadata["authenticatorGetInfo"]["transports"] = ["usb"]
        metadata["authenticatorGetInfo"]["algorithms"] = [{"type": "public-key", "alg": -7}]
        metadata["authenticatorGetInfo"]["minPINLength"] = 4
        metadata["authenticatorGetInfo"]["firmwareVersion"] = 10000
        decoded = {
            1: metadata["authenticatorGetInfo"]["versions"],
            2: metadata["authenticatorGetInfo"]["extensions"],
            3: bytes.fromhex(metadata["aaguid"].replace("-", "")),
            4: {
                "rk": True,
                "up": True,
                "plat": False,
                "clientPin": True,
                "pinUvAuthToken": True,
                "makeCredUvNotRqd": True,
            },
            5: metadata["authenticatorGetInfo"]["maxMsgSize"],
            6: metadata["authenticatorGetInfo"]["pinUvAuthProtocols"],
            9: metadata["authenticatorGetInfo"]["transports"],
            10: metadata["authenticatorGetInfo"]["algorithms"],
            13: metadata["authenticatorGetInfo"]["minPINLength"],
            14: metadata["authenticatorGetInfo"]["firmwareVersion"],
        }

        validated = service._validate_get_info_response(decoded, metadata)
        self.assertTrue(validated["options"]["pinUvAuthToken"])

    def test_validate_browser_register_result_rejects_none_attestation_x5c_leak(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        result = {
            "status": "passed",
            "summary": "ok",
            "details": {
                "attestationObject": {"fmt": "none", "x5cCount": 1},
                "parsedAuthData": {"aaguid": metadata["aaguid"].replace("-", "")},
                "credentialId": "cred-1",
            },
        }

        with self.assertRaisesRegex(RuntimeError, "unexpectedly exposed x5cCount"):
            service._validate_browser_register_result(result, attestation="none", metadata=metadata)

    def test_validate_browser_register_result_accepts_direct_attestation_leaf_only(self) -> None:
        service = ConformanceSuiteService()
        metadata = service._load_static_metadata()
        result = {
            "status": "passed",
            "summary": "ok",
            "details": {
                "attestationObject": {"fmt": "packed", "x5cCount": 1},
                "parsedAuthData": {"aaguid": metadata["aaguid"].replace("-", "")},
                "credentialId": "cred-1",
            },
        }

        validated = service._validate_browser_register_result(
            result, attestation="direct", metadata=metadata
        )

        self.assertEqual(validated["fmt"], "packed")
        self.assertEqual(validated["x5cCount"], 1)

    def test_validate_browser_auth_result_rejects_wrong_credential(self) -> None:
        service = ConformanceSuiteService()
        result = {
            "status": "passed",
            "summary": "ok",
            "details": {
                "credentialId": "returned",
                "authData": {"userPresent": True, "userVerified": False, "signCount": 1},
            },
        }

        with self.assertRaisesRegex(RuntimeError, "expected 'expected'"):
            service._validate_browser_auth_result(result, expected_credential_id="expected")

    def test_browser_scenario_waits_for_dashboard_result(self) -> None:
        service = ConformanceSuiteService()
        captured: dict[str, object] = {}

        def run_browser_scenario() -> None:
            captured["result"] = service._run_browser_scenario(
                "browser_register",
                {"slot": "browser_direct", "attestation": "direct"},
            )

        worker = threading.Thread(target=run_browser_scenario)
        worker.start()

        request = None
        for _ in range(100):
            request = service.get_status()["run"].get("browser_request")
            if request:
                break
            time.sleep(0.01)

        self.assertIsNotNone(request)
        claim = service.run_browser_scenario_api(
            "browser_register",
            {"requestId": request["id"], "clientId": "client-a", "claim": True},
        )
        response = service.run_browser_scenario_api(
            "browser_register",
            {
                "requestId": request["id"],
                "clientId": "client-a",
                "result": {
                    "status": "passed",
                    "summary": "ok",
                    "details": {},
                    "evidence": {},
                },
            },
        )

        self.assertTrue(claim["accepted"])
        self.assertTrue(response["accepted"])
        worker.join(timeout=1)
        self.assertFalse(worker.is_alive())
        self.assertEqual(captured["result"]["status"], "passed")
        self.assertIsNone(service.get_status()["run"].get("browser_request"))

    def test_browser_scenario_claims_single_dashboard_client(self) -> None:
        service = ConformanceSuiteService()

        def run_browser_scenario() -> None:
            service._run_browser_scenario(
                "browser_register",
                {"slot": "browser_direct", "attestation": "direct"},
            )

        worker = threading.Thread(target=run_browser_scenario)
        worker.start()

        request = None
        for _ in range(100):
            request = service.get_status()["run"].get("browser_request")
            if request:
                break
            time.sleep(0.01)

        self.assertIsNotNone(request)
        first_claim = service.run_browser_scenario_api(
            "browser_register",
            {"requestId": request["id"], "clientId": "client-a", "claim": True},
        )
        second_claim = service.run_browser_scenario_api(
            "browser_register",
            {"requestId": request["id"], "clientId": "client-b", "claim": True},
        )
        wrong_result = service.run_browser_scenario_api(
            "browser_register",
            {
                "requestId": request["id"],
                "clientId": "client-b",
                "result": {"status": "passed", "summary": "wrong"},
            },
        )
        final_result = service.run_browser_scenario_api(
            "browser_register",
            {
                "requestId": request["id"],
                "clientId": "client-a",
                "result": {"status": "passed", "summary": "ok", "details": {}, "evidence": {}},
            },
        )

        self.assertTrue(first_claim["accepted"])
        self.assertFalse(second_claim["accepted"])
        self.assertFalse(wrong_result["accepted"])
        self.assertTrue(final_result["accepted"])
        worker.join(timeout=1)
        self.assertFalse(worker.is_alive())

    def test_get_status_normalizes_bytes_for_json(self) -> None:
        service = ConformanceSuiteService()
        service._state["run"]["scenarios"].append(
            {
                "id": "ctap_get_info",
                "phase": "ctap",
                "status": "passed",
                "summary": "ok",
                "details": {
                    "decoded": {
                        3: b"\x01\x02\x03",
                    }
                },
                "evidence": {},
            }
        )

        status = service.get_status()
        decoded = status["run"]["scenarios"][0]["details"]["decoded"]["3"]
        self.assertEqual(decoded, {"type": "bytes", "hex": "010203"})

    def test_with_device_reuses_hid_handle_until_closed(self) -> None:
        service = ConformanceSuiteService()
        matched = MatchedDevice(
            ctaphid_probe.HidDeviceInfo(
                path=b"dummy",
                vendor_id=0x0483,
                product_id=0x5741,
                usage_page=0xF1D0,
                usage=0x01,
                product_string="ZeroFIDO",
                serial_number="fixture-1",
            )
        )
        fake_device = mock.Mock()

        with mock.patch.object(ctaphid_probe, "open_device_for_info", return_value=fake_device) as open_device:
            first = service._with_device(matched, lambda device: device)
            second = service._with_device(matched, lambda device: device)

            self.assertIs(first, fake_device)
            self.assertIs(second, fake_device)
            self.assertEqual(open_device.call_count, 1)

            service._close_run_device()

        fake_device.close.assert_called_once()

    def test_get_run_cid_reuses_channel_per_transport_kind(self) -> None:
        service = ConformanceSuiteService()
        fake_device = mock.Mock()

        with mock.patch.object(ctaphid_probe, "allocate_cid", side_effect=[0x11111111, 0x22222222]) as allocate_cid:
            first_cbor = service._get_run_cid(fake_device, "cbor", [])
            second_cbor = service._get_run_cid(fake_device, "cbor", [])
            first_msg = service._get_run_cid(fake_device, "msg", [])
            second_msg = service._get_run_cid(fake_device, "msg", [])

        self.assertEqual(first_cbor, 0x11111111)
        self.assertEqual(second_cbor, 0x11111111)
        self.assertEqual(first_msg, 0x22222222)
        self.assertEqual(second_msg, 0x22222222)
        self.assertEqual(allocate_cid.call_count, 2)

    def test_manual_checkpoint_closes_cached_device_before_waiting(self) -> None:
        service = ConformanceSuiteService()
        fake_device = mock.Mock()
        service._run_device = fake_device
        service._run_cids = {"cbor": 0x11111111, "msg": 0x22222222}

        def resolve_checkpoint() -> None:
            for _ in range(100):
                checkpoint = service.get_status()["run"].get("manual_checkpoint")
                if checkpoint:
                    service.resume_manual_checkpoint(checkpoint["id"])
                    return
                time.sleep(0.01)

        worker = threading.Thread(target=resolve_checkpoint)
        worker.start()
        service._wait_for_manual_checkpoint("Restart Device App", "resume")
        worker.join(timeout=1)

        self.assertFalse(worker.is_alive())
        fake_device.close.assert_called_once()
        self.assertIsNone(service._run_device)
        self.assertEqual(service._run_cids, {})

    def test_run_one_scenario_attaches_collected_timing(self) -> None:
        service = ConformanceSuiteService()
        matched = MatchedDevice(
            ctaphid_probe.HidDeviceInfo(
                path=b"dummy",
                vendor_id=0x0483,
                product_id=0x5741,
                usage_page=0xF1D0,
                usage=0x01,
                product_string="ZeroFIDO",
                serial_number="fixture-1",
            )
        )
        fake_device = mock.Mock()

        def scenario() -> dict[str, object]:
            service._with_device(matched, lambda device: device)
            service._scenario_timing = service._scenario_timing or {}
            service._scenario_timing["broadcast_init"] = {"total_ms": 12.3}
            return {"status": "passed", "summary": "ok", "details": {}, "evidence": {"trace": []}}

        with mock.patch.object(ctaphid_probe, "open_device_for_info", return_value=fake_device):
            result = service._run_one_scenario("transport", "transport_init", scenario, {})

        self.assertIn("timing", result["evidence"])
        self.assertIn("handle_open_ms", result["evidence"]["timing"])
        self.assertEqual(result["evidence"]["timing"]["broadcast_init"]["total_ms"], 12.3)

    def test_get_run_cid_records_allocation_timing(self) -> None:
        service = ConformanceSuiteService()
        fake_device = mock.Mock()
        trace: list[dict[str, str]] = []
        service._scenario_timing = {}

        with mock.patch.object(
            ctaphid_probe,
            "allocate_cid",
            side_effect=lambda device, timeout_ms, verbose, timing=None: timing.update(
                {"total_ms": 8.5, "allocated_cid": "0x11111111"}
            )
            or 0x11111111,
        ):
            cid = service._get_run_cid(fake_device, "cbor", trace)

        self.assertEqual(cid, 0x11111111)
        self.assertEqual(service._scenario_timing["channel_allocations"][0]["transport_kind"], "cbor")
        self.assertEqual(service._scenario_timing["channel_allocations"][0]["total_ms"], 8.5)


if __name__ == "__main__":
    unittest.main()
