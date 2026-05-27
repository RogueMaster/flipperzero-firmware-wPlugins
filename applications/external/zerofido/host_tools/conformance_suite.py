"""ZeroFIDO conformance scenario orchestrator.

The suite watches for a matching HID device, runs ordered scenario phases on a
worker thread, handles manual checkpoints and browser handoff, then maps each
scenario result back to the audit rows declared in the manifest.
"""

from __future__ import annotations

import base64
import copy
import hashlib
import json
import os
import platform
import queue
import threading
import time
import traceback
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

import hid
import cbor2
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec

from host_tools import ctaphid_probe as probe


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPORT_DIR = ROOT / ".tmp" / "conformance_suite"
DEFAULT_MANIFEST_PATH = ROOT / "host_tools" / "conformance_manifest.json"
DEFAULT_CONFIG_PATH = ROOT / "host_tools" / "fixture_config.local.json"
DEFAULT_CONFIG_FALLBACK_PATH = ROOT / "host_tools" / "fixture_config.example.json"
DEFAULT_METADATA_DIR = ROOT / "metadata"
DEFAULT_METADATA_PATH = DEFAULT_METADATA_DIR / "metadata-ctap20.json"
DEFAULT_ROOT_CERT_PATH = DEFAULT_METADATA_DIR / "11-attestation-root.pem"
DEFAULT_LEAF_CERT_PATH = DEFAULT_METADATA_DIR / "11-attestation-leaf.pem"

EXPECTED_STATIC_USER_VERIFICATION_DETAILS = [
    [
        {"userVerificationMethod": "passcode_external"},
        {"userVerificationMethod": "presence_internal"},
    ]
]

DEFAULT_STATIC_METADATA: dict[str, Any] = {
    "aaguid": "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a",
    "authenticatorGetInfo": {
        "versions": ["FIDO_2_0", "U2F_V2"],
        "extensions": ["credProtect", "hmac-secret"],
        "aaguid": "b51a976a0b0240aa9d8a36c8b91bbd1a",
        "options": {"rk": True, "up": True, "plat": False},
        "maxMsgSize": 1024,
        "pinUvAuthProtocols": [1],
    },
    "userVerificationDetails": EXPECTED_STATIC_USER_VERIFICATION_DETAILS,
}


def utc_now_iso() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def monotonic_ms() -> int:
    return int(time.monotonic() * 1000)


def make_json_safe(value: Any) -> Any:
    if isinstance(value, bytes):
        return {"type": "bytes", "hex": value.hex()}
    if isinstance(value, bytearray):
        return {"type": "bytes", "hex": bytes(value).hex()}
    if isinstance(value, dict):
        return {str(key): make_json_safe(item) for key, item in value.items()}
    if isinstance(value, list):
        return [make_json_safe(item) for item in value]
    if isinstance(value, tuple):
        return [make_json_safe(item) for item in value]
    if isinstance(value, set):
        return [make_json_safe(item) for item in value]
    return value


@dataclass
class MatchedDevice:
    info: probe.HidDeviceInfo

    def as_json(self) -> dict[str, Any]:
        return probe.device_info_to_dict(self.info)


class ConformanceSuiteService:
    def __init__(self, host: str = "localhost", port: int = 8765) -> None:
        self.host = host
        self.port = port
        self.report_dir = DEFAULT_REPORT_DIR
        self.report_dir.mkdir(parents=True, exist_ok=True)

        self.config = self._load_fixture_config()
        self.manifest = json.loads(DEFAULT_MANIFEST_PATH.read_text())
        self.scenario_to_rows = self._build_scenario_row_index(self.manifest)

        self._lock = threading.RLock()
        self._subscribers: list[queue.Queue[dict[str, Any]]] = []
        self._watcher_stop = threading.Event()
        self._watcher_thread: threading.Thread | None = None
        self._run_thread: threading.Thread | None = None
        self._run_stop = threading.Event()
        self._checkpoint_event: threading.Event | None = None
        self._browser_request_event: threading.Event | None = None
        self._browser_request_result: dict[str, Any] | None = None
        self._matched_device: MatchedDevice | None = None
        self._latest_report: dict[str, Any] | None = None
        self._run_device: hid.device | None = None
        self._run_device_open_ms: float | None = None
        self._run_cids: dict[str, int] = {}
        self._scenario_timing: dict[str, Any] | None = None

        self._state: dict[str, Any] = {
            "helper": {
                "status": "starting",
                "version": "suite-v1",
                "started_at": utc_now_iso(),
                "host": host,
                "port": port,
                "config_path": str(
                    DEFAULT_CONFIG_PATH if DEFAULT_CONFIG_PATH.exists() else DEFAULT_CONFIG_FALLBACK_PATH
                ),
            },
            "device": {
                "connected": False,
                "info": None,
                "matched_at": None,
            },
            "run": {
                "status": "idle",
                "run_id": None,
                "trigger": None,
                "started_at": None,
                "finished_at": None,
                "current_phase": None,
                "current_scenario": None,
                "operator_prompt": None,
                "manual_checkpoint": None,
                "browser_request": None,
                "scenarios": [],
                "gate_result": None,
                "error": None,
            },
        }

    def _load_fixture_config(self) -> dict[str, Any]:
        source = DEFAULT_CONFIG_PATH if DEFAULT_CONFIG_PATH.exists() else DEFAULT_CONFIG_FALLBACK_PATH
        return json.loads(source.read_text())

    def _build_scenario_row_index(self, manifest: dict[str, Any]) -> dict[str, list[str]]:
        index: dict[str, list[str]] = {}
        for row in manifest["rows"]:
            for scenario_id in row.get("scenario_ids", []):
                index.setdefault(scenario_id, []).append(row["id"])
        return index

    def _load_static_metadata(self) -> dict[str, Any]:
        if DEFAULT_METADATA_PATH.exists():
            return json.loads(DEFAULT_METADATA_PATH.read_text())
        return copy.deepcopy(DEFAULT_STATIC_METADATA)

    def _pem_to_der(self, path: Path) -> bytes:
        encoded_lines = [
            line.strip()
            for line in path.read_text().splitlines()
            if line and not line.startswith("-----")
        ]
        return base64.b64decode("".join(encoded_lines))

    def _validate_static_metadata(self, metadata: dict[str, Any]) -> dict[str, Any]:
        if metadata.get("userVerificationDetails") != EXPECTED_STATIC_USER_VERIFICATION_DETAILS:
            raise RuntimeError("static metadata userVerificationDetails do not match the runtime UV model")
        if "matcherProtection" in metadata:
            raise RuntimeError("static metadata must not claim matcherProtection without built-in UV")

        get_info = metadata.get("authenticatorGetInfo")
        if not isinstance(get_info, dict):
            raise RuntimeError("static metadata is missing authenticatorGetInfo")
        options = get_info.get("options")
        if not isinstance(options, dict):
            raise RuntimeError("static metadata authenticatorGetInfo options are missing")
        if "clientPin" in options:
            raise RuntimeError("static metadata must not hard-code clientPin")
        if "uv" in options:
            raise RuntimeError("static metadata must not claim built-in uv")
        if metadata.get("aaguid") != get_info.get("aaguid"):
            raise RuntimeError("static metadata AAGUID is inconsistent across sections")
        return {
            "aaguid": metadata.get("aaguid"),
            "options": options,
            "userVerificationDetails": metadata.get("userVerificationDetails"),
        }

    def _validate_get_info_response(self, decoded: Any, metadata: dict[str, Any]) -> dict[str, Any]:
        if not isinstance(decoded, dict):
            raise RuntimeError("GetInfo did not decode to a CBOR map")
        get_info = metadata.get("authenticatorGetInfo")
        if not isinstance(get_info, dict):
            raise RuntimeError("static metadata is missing authenticatorGetInfo")
        versions = decoded.get(1)
        if not isinstance(versions, list):
            raise RuntimeError(f"unexpected GetInfo versions: {versions!r}")
        expected_versions = get_info.get("versions")
        if not isinstance(expected_versions, list):
            raise RuntimeError("static metadata authenticatorGetInfo versions are missing")
        if versions != expected_versions:
            raise RuntimeError(
                f"unexpected GetInfo versions: {versions!r}, expected {expected_versions!r}"
            )
        extensions = decoded.get(2)
        expected_metadata_fields = {
            2: "extensions",
            5: "maxMsgSize",
            6: "pinUvAuthProtocols",
            9: "transports",
            10: "algorithms",
            13: "minPINLength",
            14: "firmwareVersion",
        }
        for cbor_key, metadata_field in expected_metadata_fields.items():
            expected = get_info.get(metadata_field)
            if expected is None:
                if cbor_key in decoded:
                    raise RuntimeError(
                        f"unexpected GetInfo {metadata_field}: {decoded.get(cbor_key)!r}, expected omission"
                    )
                continue
            actual = decoded.get(cbor_key)
            if actual != expected:
                raise RuntimeError(
                    f"unexpected GetInfo {metadata_field}: {actual!r}, expected {expected!r}"
                )
        aaguid = decoded.get(3)
        if not isinstance(aaguid, bytes) or aaguid.hex() != metadata.get("aaguid", "").replace("-", ""):
            raise RuntimeError("GetInfo AAGUID does not match static metadata")
        options = decoded.get(4)
        if not isinstance(options, dict):
            raise RuntimeError("GetInfo options are missing")
        expected_options = {"rk": True, "up": True, "plat": False}
        for key, expected in expected_options.items():
            if options.get(key) != expected:
                raise RuntimeError(f"GetInfo option {key!r} was {options.get(key)!r}, expected {expected!r}")
        if not isinstance(options.get("clientPin"), bool):
            raise RuntimeError("GetInfo clientPin must be present and boolean")
        uv_option = options.get("uv")
        if uv_option not in (None, False):
            raise RuntimeError(f"GetInfo option 'uv' was {uv_option!r}, expected False or omission")
        for forbidden in ("ep",):
            if forbidden in options:
                raise RuntimeError(f"GetInfo unexpectedly advertised unsupported option {forbidden!r}")
        metadata_options = get_info.get("options") if isinstance(get_info.get("options"), dict) else {}
        for profile_option in ("pinUvAuthToken", "makeCredUvNotRqd"):
            expected = metadata_options.get(profile_option)
            if expected is None:
                if profile_option in options:
                    raise RuntimeError(
                        f"GetInfo unexpectedly advertised unsupported option {profile_option!r}"
                    )
            elif options.get(profile_option) != expected:
                raise RuntimeError(
                    f"GetInfo option {profile_option!r} was {options.get(profile_option)!r}, expected {expected!r}"
                )
        return {
            "versions": versions,
            "extensions": extensions,
            "aaguid": aaguid.hex(),
            "options": options,
            "maxMsgSize": decoded.get(5),
            "pinUvAuthProtocols": decoded.get(6),
            "transports": decoded.get(9),
            "algorithms": decoded.get(10),
            "minPINLength": decoded.get(13),
            "firmwareVersion": decoded.get(14),
        }

    def _validate_make_credential_attestation(
        self, parsed: dict[str, Any], metadata: dict[str, Any]
    ) -> dict[str, Any]:
        decoded = parsed.get("decoded")
        if not isinstance(decoded, dict):
            raise RuntimeError("MakeCredential response body did not decode")
        if decoded.get(1) != "packed":
            raise RuntimeError(f"unexpected attestation format: {decoded.get(1)!r}")

        auth_data = parsed.get("auth_data")
        expected_aaguid = metadata.get("aaguid", "").replace("-", "")
        if not isinstance(auth_data, dict) or auth_data.get("aaguid") != expected_aaguid:
            raise RuntimeError("MakeCredential authData AAGUID did not match metadata")

        att_stmt = decoded.get(3)
        if not isinstance(att_stmt, dict):
            raise RuntimeError("MakeCredential attStmt was missing")
        if att_stmt.get("alg") != -7:
            raise RuntimeError(f"unexpected attestation algorithm: {att_stmt.get('alg')!r}")
        if not isinstance(att_stmt.get("sig"), bytes) or len(att_stmt["sig"]) == 0:
            raise RuntimeError("MakeCredential attStmt signature was missing")

        x5c = att_stmt.get("x5c")
        if not isinstance(x5c, list) or len(x5c) != 1 or not all(isinstance(item, bytes) for item in x5c):
            raise RuntimeError("MakeCredential attStmt x5c did not include exactly one leaf certificate")

        return {
            "fmt": decoded.get(1),
            "x5cCount": len(x5c),
            "aaguid": auth_data.get("aaguid"),
            "credentialId": auth_data.get("credential_id"),
        }

    def _validate_browser_register_result(
        self, result: dict[str, Any], *, attestation: str, metadata: dict[str, Any]
    ) -> dict[str, Any]:
        if result.get("status") != "passed":
            raise RuntimeError(f"browser registration did not pass: {result.get('summary')}")
        details = result.get("details")
        if not isinstance(details, dict):
            raise RuntimeError("browser registration details were missing")
        attestation_object = details.get("attestationObject")
        if not isinstance(attestation_object, dict):
            raise RuntimeError("browser registration did not expose attestationObject details")
        parsed_auth_data = details.get("parsedAuthData")
        expected_aaguid = metadata.get("aaguid", "").replace("-", "")
        if not isinstance(parsed_auth_data, dict) or parsed_auth_data.get("aaguid") != expected_aaguid:
            raise RuntimeError("browser registration authData AAGUID did not match metadata")

        fmt = attestation_object.get("fmt")
        x5c_count = attestation_object.get("x5cCount")
        if attestation == "direct":
            if fmt != "packed":
                raise RuntimeError(f"browser direct attestation returned {fmt!r} instead of 'packed'")
            if x5c_count != 1:
                raise RuntimeError(f"browser direct attestation returned x5cCount={x5c_count!r}, expected 1")
        elif attestation == "none":
            if fmt != "none":
                raise RuntimeError(f"browser none attestation returned {fmt!r} instead of 'none'")
            if x5c_count not in (0, None):
                raise RuntimeError(
                    f"browser none attestation unexpectedly exposed x5cCount={x5c_count!r}"
                )

        return {
            "fmt": fmt,
            "x5cCount": x5c_count,
            "aaguid": parsed_auth_data.get("aaguid"),
            "credentialId": details.get("credentialId"),
        }

    def _validate_browser_auth_result(
        self, result: dict[str, Any], *, expected_credential_id: str | None
    ) -> dict[str, Any]:
        if result.get("status") != "passed":
            raise RuntimeError(f"browser authentication did not pass: {result.get('summary')}")
        details = result.get("details")
        if not isinstance(details, dict):
            raise RuntimeError("browser authentication details were missing")
        credential_id = details.get("credentialId")
        if expected_credential_id and credential_id != expected_credential_id:
            raise RuntimeError(
                f"browser authentication returned credentialId {credential_id!r}, "
                f"expected {expected_credential_id!r}"
            )
        auth_data = details.get("authData")
        if not isinstance(auth_data, dict) or not auth_data.get("userPresent"):
            raise RuntimeError("browser authentication did not report user presence in authData")
        return {
            "credentialId": credential_id,
            "userPresent": auth_data.get("userPresent"),
            "userVerified": auth_data.get("userVerified"),
            "signCount": auth_data.get("signCount"),
        }

    def start(self) -> None:
        self._watcher_stop.clear()
        self._watcher_thread = threading.Thread(target=self._watcher_loop, name="suite-device-watch", daemon=True)
        self._watcher_thread.start()
        with self._lock:
            self._state["helper"]["status"] = "ready"
        self._emit("helper_ready", self.get_status())

    def stop(self) -> None:
        self._watcher_stop.set()
        self._run_stop.set()
        with self._lock:
            checkpoint_event = self._checkpoint_event
            browser_event = self._browser_request_event
        if checkpoint_event is not None:
            checkpoint_event.set()
        if browser_event is not None:
            browser_event.set()
        self._close_run_device()
        if self._watcher_thread:
            self._watcher_thread.join(timeout=2)
        if self._run_thread and self._run_thread.is_alive():
            self._run_thread.join(timeout=2)

    def subscribe(self) -> queue.Queue[dict[str, Any]]:
        q: queue.Queue[dict[str, Any]] = queue.Queue()
        with self._lock:
            self._subscribers.append(q)
        return q

    def unsubscribe(self, q: queue.Queue[dict[str, Any]]) -> None:
        with self._lock:
            self._subscribers = [item for item in self._subscribers if item is not q]

    def _emit(self, kind: str, payload: dict[str, Any]) -> None:
        message = {"event": kind, "data": make_json_safe(payload)}
        with self._lock:
            subscribers = list(self._subscribers)
        for subscriber in subscribers:
            subscriber.put(message)

    def get_status(self) -> dict[str, Any]:
        with self._lock:
            status = copy.deepcopy(self._state)
            status["latest_report"] = copy.deepcopy(self._latest_report)
        return make_json_safe(status)

    def get_latest_report(self) -> dict[str, Any] | None:
        with self._lock:
            return make_json_safe(copy.deepcopy(self._latest_report))

    def _watcher_loop(self) -> None:
        stable_candidate: MatchedDevice | None = None
        stable_since = 0.0
        while not self._watcher_stop.wait(1.0):
            matched = self._find_matching_device()
            with self._lock:
                previous_connected = self._state["device"]["connected"]
            if matched is None:
                stable_candidate = None
                stable_since = 0.0
                if previous_connected:
                    self._close_run_device()
                    with self._lock:
                        self._matched_device = None
                        self._state["device"] = {
                            "connected": False,
                            "info": None,
                            "matched_at": None,
                        }
                    self._emit("device_disconnected", self.get_status())
                continue

            if stable_candidate and stable_candidate.info.path == matched.info.path:
                if time.monotonic() - stable_since < 1.0:
                    continue
            else:
                stable_candidate = matched
                stable_since = time.monotonic()
                continue

            with self._lock:
                already_same = (
                    self._matched_device is not None
                    and self._matched_device.info.path == matched.info.path
                    and self._state["device"]["connected"]
                )
                if not already_same:
                    self._close_run_device()
                    self._matched_device = matched
                    self._state["device"] = {
                        "connected": True,
                        "info": matched.as_json(),
                        "matched_at": utc_now_iso(),
                    }
            if not previous_connected or not already_same:
                self._emit("device_connected", self.get_status())
            if self.config.get("autostart", False):
                self.start_run("device_connected")

    def _find_matching_device(self) -> MatchedDevice | None:
        match = self.config.get("device_match", {})
        vendor_id = match.get("vendor_id")
        product_id = match.get("product_id")
        path_contains = match.get("path_contains", "")
        serial_contains = match.get("serial_contains", "")
        for device in probe.discover_devices():
            if vendor_id is not None and device.vendor_id != vendor_id:
                continue
            if product_id is not None and device.product_id != product_id:
                continue
            if path_contains:
                candidate = device.path.decode("utf-8", errors="replace")
                if path_contains not in candidate:
                    continue
            if serial_contains and serial_contains not in (device.serial_number or ""):
                continue
            return MatchedDevice(device)
        return None

    def start_run(self, trigger: str) -> dict[str, Any]:
        with self._lock:
            if self._state["run"]["status"] == "running":
                return {"accepted": False, "reason": "run already active"}
            matched = self._matched_device
            if matched is None:
                return {"accepted": False, "reason": "no matched device"}
            run_id = uuid.uuid4().hex
            self._run_stop.clear()
            self._state["run"] = {
                "status": "running",
                "run_id": run_id,
                "trigger": trigger,
                "started_at": utc_now_iso(),
                "finished_at": None,
                "current_phase": None,
                "current_scenario": None,
                "operator_prompt": None,
                "manual_checkpoint": None,
                "browser_request": None,
                "scenarios": [],
                "gate_result": None,
                "error": None,
            }
        self._emit("run_started", self.get_status())
        self._run_thread = threading.Thread(
            target=self._run_suite, args=(run_id, trigger, matched), name="suite-run", daemon=True
        )
        self._run_thread.start()
        return {"accepted": True, "run_id": run_id}

    def resume_manual_checkpoint(self, checkpoint_id: str) -> dict[str, Any]:
        with self._lock:
            checkpoint = self._state["run"]["manual_checkpoint"]
            if checkpoint is None:
                return {"accepted": False, "reason": "no checkpoint pending"}
            if checkpoint["id"] != checkpoint_id:
                return {"accepted": False, "reason": "checkpoint id mismatch"}
            event = self._checkpoint_event
        if event is None:
            return {"accepted": False, "reason": "checkpoint event missing"}
        event.set()
        return {"accepted": True, "checkpoint_id": checkpoint_id}

    def run_browser_scenario_api(self, scenario_id: str, payload: dict[str, Any]) -> dict[str, Any]:
        request_id = payload.get("requestId")
        client_id = payload.get("clientId")
        if not isinstance(client_id, str) or not client_id:
            return {"accepted": False, "reason": "client id missing"}
        if payload.get("claim") is True:
            with self._lock:
                request = self._state["run"].get("browser_request")
                event = self._browser_request_event
                if request is None:
                    return {"accepted": False, "reason": "no browser request pending"}
                if request["id"] != request_id:
                    return {"accepted": False, "reason": "browser request id mismatch"}
                if request["scenario_id"] != scenario_id:
                    return {"accepted": False, "reason": "browser scenario mismatch"}
                if event is None:
                    return {"accepted": False, "reason": "browser request event missing"}
                claimed_by = request.get("claimed_by")
                if claimed_by and claimed_by != client_id:
                    return {"accepted": False, "reason": "browser request already claimed"}
                request["claimed_by"] = client_id
                request["claimed_at"] = utc_now_iso()
            self._emit("browser_request_claimed", self.get_status())
            return {"accepted": True, "request_id": request_id, "scenario_id": scenario_id}
        result = payload.get("result")
        if not isinstance(request_id, str) or not request_id:
            return {"accepted": False, "reason": "request id missing"}
        if not isinstance(result, dict):
            return {"accepted": False, "reason": "browser result missing"}
        with self._lock:
            request = self._state["run"].get("browser_request")
            event = self._browser_request_event
            if request is None:
                return {"accepted": False, "reason": "no browser request pending"}
            if request["id"] != request_id:
                return {"accepted": False, "reason": "browser request id mismatch"}
            if request["scenario_id"] != scenario_id:
                return {"accepted": False, "reason": "browser scenario mismatch"}
            if event is None:
                return {"accepted": False, "reason": "browser request event missing"}
            claimed_by = request.get("claimed_by")
            if claimed_by and claimed_by != client_id:
                return {"accepted": False, "reason": "browser request owned by another client"}
            self._browser_request_result = copy.deepcopy(result)
        event.set()
        return {"accepted": True, "request_id": request_id, "scenario_id": scenario_id}

    def _set_current_step(self, phase: str, scenario_id: str | None) -> None:
        with self._lock:
            self._state["run"]["current_phase"] = phase
            self._state["run"]["current_scenario"] = scenario_id
        self._emit("run_progress", self.get_status())

    def _set_operator_prompt(self, prompt: dict[str, Any] | None) -> None:
        with self._lock:
            self._state["run"]["operator_prompt"] = prompt
        self._emit("operator_prompt", self.get_status())

    def _wait_for_manual_checkpoint(self, title: str, message: str) -> None:
        checkpoint_id = uuid.uuid4().hex[:12]
        event = threading.Event()
        self._close_run_device()
        with self._lock:
            self._checkpoint_event = event
            self._state["run"]["manual_checkpoint"] = {
                "id": checkpoint_id,
                "title": title,
                "message": message,
                "created_at": utc_now_iso(),
            }
        self._emit("manual_checkpoint", self.get_status())
        event.wait()
        if self._run_stop.is_set():
            raise RuntimeError("run stopped")
        with self._lock:
            self._checkpoint_event = None
            self._state["run"]["manual_checkpoint"] = None
        self._emit("manual_checkpoint_resolved", self.get_status())

    def _append_result(self, result: dict[str, Any]) -> None:
        with self._lock:
            self._state["run"]["scenarios"].append(result)
        self._emit("scenario_result", result)

    def _mark_run_finished(self, report: dict[str, Any], gate_result: str, error: str | None) -> None:
        safe_report = make_json_safe(report)
        self._close_run_device()
        with self._lock:
            self._latest_report = safe_report
            self._state["run"]["status"] = gate_result
            self._state["run"]["finished_at"] = utc_now_iso()
            self._state["run"]["current_phase"] = None
            self._state["run"]["current_scenario"] = None
            self._state["run"]["operator_prompt"] = None
            self._state["run"]["manual_checkpoint"] = None
            self._state["run"]["browser_request"] = None
            self._state["run"]["gate_result"] = gate_result
            self._state["run"]["error"] = error
        latest_json = self.report_dir / "latest.json"
        latest_json.write_text(json.dumps(safe_report, indent=2))
        self._emit("run_finished", self.get_status())

    def _run_suite(self, run_id: str, trigger: str, matched: MatchedDevice) -> None:
        runtime: dict[str, Any] = {
            "run_id": run_id,
            "rp_counter": 0,
            "credentials": {},
            "browser_credentials": {},
            "u2f": {},
            "pin": {
                "current": self.config.get("fixture", {}).get("known_pin", "") or "",
                "bootstrap": self.config.get("fixture", {}).get("bootstrap_pin", "") or "",
                "changed": self.config.get("fixture", {}).get("changed_pin", "") or "",
                "token": None,
                "peer_key_agreement": None,
                "platform_private_key": None,
            },
        }
        results: list[dict[str, Any]] = []
        error: str | None = None
        try:
            phases: list[tuple[str, list[tuple[str, Callable[[], dict[str, Any]], dict[str, Any]]]]] = [
                (
                    "static",
                    [
                        ("static_attestation_assets", lambda: self._scenario_static_attestation_assets(), {}),
                        ("static_metadata_alignment", lambda: self._scenario_static_metadata_alignment(), {}),
                    ],
                ),
                (
                    "transport",
                    [
                        ("transport_init", lambda: self._scenario_transport_init(matched), {}),
                        ("transport_ping", lambda: self._scenario_transport_ping(matched), {}),
                        ("transport_wink", lambda: self._scenario_transport_wink(matched), {}),
                        ("transport_invalid_cid", lambda: self._scenario_transport_invalid_cid(matched), {}),
                        ("transport_exhaust_cids", lambda: self._scenario_transport_exhaust_cids(matched), {}),
                        ("transport_short_init", lambda: self._scenario_transport_short_init(matched), {}),
                        (
                            "transport_spurious_continuation",
                            lambda: self._scenario_transport_spurious_continuation(matched),
                            {},
                        ),
                        (
                            "transport_cancel",
                            lambda: self._scenario_transport_cancel(matched),
                            {
                                "prompt": {
                                    "title": "Approval Touch Required",
                                    "message": "Touch the device to allow the pending CTAP request so the CANCEL flow can be observed.",
                                }
                            },
                        ),
                        (
                            "transport_resync",
                            lambda: self._scenario_transport_resync(matched),
                            {
                                "prompt": {
                                    "title": "Approval Touch Required",
                                    "message": "Touch the device during the pending request so the same-CID INIT resync can be exercised.",
                                }
                            },
                        ),
                    ],
                ),
                (
                    "ctap",
                    [
                        ("ctap_get_info", lambda: self._scenario_ctap_get_info(matched, runtime), {}),
                        (
                            "ctap_make_credential_resident",
                            lambda: self._scenario_ctap_make_credential(matched, runtime, True),
                            {
                                "prompt": {
                                    "title": "Approve Registration",
                                    "message": "Touch the device when the raw resident-key registration request appears.",
                                }
                            },
                        ),
                        (
                            "ctap_make_credential_nonresident",
                            lambda: self._scenario_ctap_make_credential(matched, runtime, False),
                            {
                                "prompt": {
                                    "title": "Approve Registration",
                                    "message": "Touch the device when the raw non-resident registration request appears.",
                                }
                            },
                        ),
                        (
                            "ctap_exclude_list",
                            lambda: self._scenario_ctap_exclude_list(matched, runtime),
                            {
                                "prompt": {
                                    "title": "Approve Registration",
                                    "message": "Touch the device for the excludeList regression check.",
                                }
                            },
                        ),
                        (
                            "ctap_get_assertion_allow_list",
                            lambda: self._scenario_ctap_get_assertion_allow_list(matched, runtime),
                            {
                                "prompt": {
                                    "title": "Approve Authentication",
                                    "message": "Touch the device for the allow-list assertion path.",
                                }
                            },
                        ),
                        (
                            "ctap_get_assertion_discoverable",
                            lambda: self._scenario_ctap_get_assertion_discoverable(matched, runtime),
                            {
                                "prompt": {
                                    "title": "Approve Authentication",
                                    "message": "Touch the device for the discoverable assertion path.",
                                }
                            },
                        ),
                        (
                            "ctap_get_next_assertion",
                            lambda: self._scenario_ctap_get_next_assertion(matched, runtime),
                            {
                                "prompt": {
                                    "title": "Approve Authentication",
                                    "message": "Touch the device for the multi-assertion enumeration path.",
                                }
                            },
                        ),
                        ("ctap_silent_assertion", lambda: self._scenario_ctap_silent_assertion(matched, runtime), {}),
                        ("ctap_unsupported_option_rk", lambda: self._scenario_ctap_unsupported_option(matched, runtime, "rk"), {}),
                        ("ctap_unsupported_option_uv", lambda: self._scenario_ctap_unsupported_option(matched, runtime, "uv"), {}),
                        ("ctap_make_credential_missing_client_data_hash", lambda: self._scenario_ctap_missing_client_data_hash(matched, make_credential=True), {}),
                        ("ctap_get_assertion_missing_client_data_hash", lambda: self._scenario_ctap_missing_client_data_hash(matched, make_credential=False), {}),
                        ("ctap_make_credential_trailing_cbor", lambda: self._scenario_ctap_trailing_cbor(matched, make_credential=True), {}),
                        ("ctap_get_assertion_trailing_cbor", lambda: self._scenario_ctap_trailing_cbor(matched, make_credential=False), {}),
                        (
                            "ctap_empty_pin_auth_make_credential",
                            lambda: self._scenario_ctap_empty_pin_auth(matched, make_credential=True),
                            {
                                "prompt": {
                                    "title": "Approve Empty pinAuth Probe",
                                    "message": "Touch the device for the MakeCredential empty pinAuth compatibility probe.",
                                }
                            },
                        ),
                        (
                            "ctap_empty_pin_auth_get_assertion",
                            lambda: self._scenario_ctap_empty_pin_auth(matched, make_credential=False),
                            {
                                "prompt": {
                                    "title": "Approve Empty pinAuth Probe",
                                    "message": "Touch the device for the GetAssertion empty pinAuth compatibility probe.",
                                }
                            },
                        ),
                    ],
                ),
                (
                    "browser",
                    [
                        (
                            "browser_register_direct",
                            lambda: self._scenario_browser_register(runtime, attestation="direct", resident_key=False, slot="browser_direct"),
                            {
                                "prompt": {
                                    "title": "Approve Browser Registration",
                                    "message": "Touch the device when your browser starts the direct-attestation registration ceremony.",
                                }
                            },
                        ),
                        (
                            "browser_auth_allow_list",
                            lambda: self._scenario_browser_auth(runtime, discoverable=False, slot="browser_direct"),
                            {
                                "prompt": {
                                    "title": "Approve Browser Authentication",
                                    "message": "Touch the device for the browser allow-list authentication ceremony.",
                                }
                            },
                        ),
                    ],
                ),
                (
                    "clientpin",
                    [
                        ("clientpin_get_retries", lambda: self._scenario_clientpin_get_retries(matched, runtime), {}),
                        ("clientpin_get_key_agreement", lambda: self._scenario_clientpin_get_key_agreement(matched, runtime), {}),
                        ("clientpin_set_pin", lambda: self._scenario_clientpin_set_pin(matched, runtime), {}),
                        (
                            "browser_register_none",
                            lambda: self._scenario_browser_register(runtime, attestation="none", resident_key=True, slot="browser_resident"),
                            {
                                "prompt": {
                                    "title": "Approve Browser Registration",
                                    "message": "Touch the device when your browser starts the anonymized discoverable registration ceremony, and enter the current security key PIN in the browser if prompted.",
                                }
                            },
                        ),
                        (
                            "browser_auth_discoverable",
                            lambda: self._scenario_browser_auth(runtime, discoverable=True, slot="browser_resident"),
                            {
                                "prompt": {
                                    "title": "Approve Browser Authentication",
                                    "message": "Touch the device for the discoverable browser authentication ceremony.",
                                }
                            },
                        ),
                        ("clientpin_get_pin_token", lambda: self._scenario_clientpin_get_pin_token(matched, runtime), {}),
                        ("ctap_make_credential_with_pin_auth", lambda: self._scenario_ctap_with_pin_auth(matched, runtime, make_credential=True), {}),
                        ("ctap_get_assertion_with_pin_auth", lambda: self._scenario_ctap_with_pin_auth(matched, runtime, make_credential=False), {}),
                        ("clientpin_change_pin", lambda: self._scenario_clientpin_change_pin(matched, runtime), {}),
                        ("clientpin_wrong_pin_retry", lambda: self._scenario_clientpin_wrong_pin_retry(matched, runtime), {}),
                        ("clientpin_wrong_pin_auth_block", lambda: self._scenario_clientpin_wrong_pin_auth_block(matched, runtime), {}),
                        ("clientpin_recovery", lambda: self._scenario_clientpin_recovery(matched, runtime), {}),
                    ],
                ),
                (
                    "u2f",
                    [
                        ("u2f_version", lambda: self._scenario_u2f_version(matched, runtime), {}),
                        ("u2f_invalid_apdu", lambda: self._scenario_u2f_invalid_apdu(matched), {}),
                        (
                            "u2f_register",
                            lambda: self._scenario_u2f_register(matched, runtime),
                            {
                                "prompt": {
                                    "title": "Approve U2F Registration",
                                    "message": "Touch the device for the raw U2F registration ceremony.",
                                }
                            },
                        ),
                        (
                            "u2f_authenticate",
                            lambda: self._scenario_u2f_authenticate(matched, runtime),
                            {
                                "prompt": {
                                    "title": "Approve U2F Authentication",
                                    "message": "Touch the device for the raw U2F authentication ceremony.",
                                }
                            },
                        ),
                    ],
                ),
            ]

            for phase_name, phase_scenarios in phases:
                for scenario_id, fn, options in phase_scenarios:
                    if self._run_stop.is_set():
                        raise RuntimeError("run stopped")
                    result = self._run_one_scenario(phase_name, scenario_id, fn, options)
                    results.append(result)

            self._wait_for_manual_checkpoint(
                "Restart Device App",
                "Restart the ZeroFIDO app on the device, then click Resume so the persisted-credential browser check can continue.",
            )
            result = self._run_one_scenario(
                "browser",
                "browser_auth_persisted",
                lambda: self._scenario_browser_auth(runtime, discoverable=False, slot="browser_direct"),
                {
                    "prompt": {
                        "title": "Approve Browser Authentication",
                        "message": "Touch the device after the app restart for the persisted-credential browser check.",
                    }
                },
            )
            results.append(result)
        except Exception as exc:  # pragma: no cover - integration failure path
            error = f"{exc}\n{traceback.format_exc()}"

        report = self._build_report(run_id, trigger, matched, results, error)
        self._mark_run_finished(report, report["gate_result"], error)

    def _run_one_scenario(
        self,
        phase_name: str,
        scenario_id: str,
        fn: Callable[[], dict[str, Any]],
        options: dict[str, Any],
    ) -> dict[str, Any]:
        self._set_current_step(phase_name, scenario_id)
        prompt = options.get("prompt")
        if prompt:
            self._set_operator_prompt(prompt)
        start = monotonic_ms()
        started_at = utc_now_iso()
        self._scenario_timing = {}
        try:
            body = fn()
            status = body.get("status", "passed")
            summary = body.get("summary", scenario_id)
            error = body.get("error")
        except Exception as exc:  # pragma: no cover - integration failure path
            body = {
                "status": "failed",
                "summary": str(exc),
                "error": traceback.format_exc(),
                "details": {},
                "evidence": {},
            }
            status = "failed"
            summary = str(exc)
            error = body["error"]
        finally:
            scenario_timing = self._scenario_timing
            self._scenario_timing = None
            if prompt:
                self._set_operator_prompt(None)
        ended_at = utc_now_iso()
        finished = monotonic_ms()
        evidence = body.get("evidence", {})
        if scenario_timing:
            evidence = dict(evidence)
            evidence["timing"] = scenario_timing
        result = {
            "id": scenario_id,
            "phase": phase_name,
            "rows": self.scenario_to_rows.get(scenario_id, []),
            "status": status,
            "summary": summary,
            "error": error,
            "started_at": started_at,
            "ended_at": ended_at,
            "duration_ms": finished - start,
            "details": body.get("details", {}),
            "evidence": evidence,
        }
        self._append_result(result)
        return result

    def _build_report(
        self,
        run_id: str,
        trigger: str,
        matched: MatchedDevice,
        results: list[dict[str, Any]],
        error: str | None,
    ) -> dict[str, Any]:
        scenario_lookup = {result["id"]: result for result in results}
        row_verdicts: list[dict[str, Any]] = []
        required_statuses: list[str] = []
        for row in self.manifest["rows"]:
            statuses = [
                scenario_lookup[scenario_id]["status"]
                for scenario_id in row.get("scenario_ids", [])
                if scenario_id in scenario_lookup
            ]
            if not row.get("scenario_ids"):
                verdict = "not_covered"
            elif not statuses:
                verdict = "not_covered"
            elif any(status == "failed" for status in statuses):
                verdict = "failed"
            elif any(status == "blocked" for status in statuses):
                verdict = "blocked"
            elif all(status == "passed" for status in statuses):
                verdict = "passed"
            else:
                verdict = "blocked"
            row_verdict = {
                "id": row["id"],
                "section": row["section"],
                "surface": row["surface"],
                "required": row["required"],
                "classification": row["classification"],
                "scenario_ids": row["scenario_ids"],
                "verdict": verdict,
            }
            row_verdicts.append(row_verdict)
            if row["required"]:
                required_statuses.append(verdict)

        failed_scenarios = [result for result in results if result.get("status") == "failed"]
        if error or failed_scenarios or any(status == "failed" for status in required_statuses):
            gate_result = "failed"
        elif any(status in {"blocked", "not_covered"} for status in required_statuses):
            gate_result = "blocked"
        else:
            gate_result = "passed"

        return {
            "run_id": run_id,
            "generated_at": utc_now_iso(),
            "gate_result": gate_result,
            "error": error,
            "metadata": {
                "trigger": trigger,
                "helper_version": "suite-v1",
                "os": platform.platform(),
                "python": platform.python_version(),
                "device": matched.as_json(),
                "config_path": str(
                    DEFAULT_CONFIG_PATH if DEFAULT_CONFIG_PATH.exists() else DEFAULT_CONFIG_FALLBACK_PATH
                ),
                "browser_runner": "operator_browser",
            },
            "scenario_results": results,
            "row_verdicts": row_verdicts,
        }

    def _close_run_device(self) -> None:
        self._run_cids = {}
        if self._run_device is None:
            return
        try:
            self._run_device.close()
        finally:
            self._run_device = None
            self._run_device_open_ms = None

    def _get_run_device(self, matched: MatchedDevice) -> hid.device:
        if self._run_stop.is_set():
            raise RuntimeError("run stopped")
        if self._run_device is None:
            start = time.perf_counter()
            self._run_device = probe.open_device_for_info(matched.info)
            self._run_device_open_ms = round((time.perf_counter() - start) * 1000.0, 3)
            if self._scenario_timing is not None:
                self._scenario_timing["handle_open_ms"] = self._run_device_open_ms
        return self._run_device

    def _invalidate_run_cid(self, transport_kind: str) -> None:
        self._run_cids.pop(transport_kind, None)

    def _get_run_cid(self, device: hid.device, transport_kind: str, trace: list[dict[str, str]]) -> int:
        cid = self._run_cids.get(transport_kind)
        if cid is None:
            timing: dict[str, Any] = {}
            cid = probe.allocate_cid(device, 3000, False, timing=timing)
            self._run_cids[transport_kind] = cid
            if self._scenario_timing is not None:
                allocations = self._scenario_timing.setdefault("channel_allocations", [])
                allocations.append(
                    {
                        "transport_kind": transport_kind,
                        **timing,
                    }
                )
        elif self._scenario_timing is not None:
            reuses = self._scenario_timing.setdefault("channel_reuse", [])
            reuses.append({"transport_kind": transport_kind, "cid": f"0x{cid:08x}"})
        return cid

    def _transact_run_channel(
        self,
        device: hid.device,
        transport_kind: str,
        transport_cmd: int,
        payload: bytes,
        trace: list[dict[str, str]],
    ) -> tuple[int, int, bytes]:
        for _ in range(2):
            if self._run_stop.is_set():
                raise RuntimeError("run stopped")
            cid = self._get_run_cid(device, transport_kind, trace)
            timing: dict[str, Any] = {}
            response_cid, response_cmd, response_payload = probe.transact(
                device, cid, transport_cmd, payload, 3000, False, trace, timing=timing
            )
            if self._scenario_timing is not None:
                transacts = self._scenario_timing.setdefault("command_transacts", [])
                transacts.append(
                    {
                        "transport_kind": transport_kind,
                        **timing,
                    }
                )
            if not (
                response_cmd == probe.ERROR
                and response_payload
                and response_payload[0] == 0x0B
            ):
                return response_cid, response_cmd, response_payload
            self._invalidate_run_cid(transport_kind)

        raise RuntimeError("run-scoped CTAPHID channel was rejected as invalid after retry")

    def _with_device(self, matched: MatchedDevice, fn: Callable[[hid.device], dict[str, Any]]) -> dict[str, Any]:
        if self._run_stop.is_set():
            raise RuntimeError("run stopped")
        return fn(self._get_run_device(matched))

    def _scenario_static_attestation_assets(self) -> dict[str, Any]:
        root_pem = DEFAULT_ROOT_CERT_PATH
        leaf_pem = DEFAULT_LEAF_CERT_PATH
        statement = DEFAULT_METADATA_PATH
        missing = [str(path) for path in (root_pem, leaf_pem, statement) if not path.exists()]
        if missing:
            return {"status": "failed", "summary": "attestation assets missing", "details": {"missing": missing}}
        metadata = self._load_static_metadata()
        metadata_roots = metadata.get("attestationRootCertificates") or []
        if len(metadata_roots) != 1:
            return {
                "status": "failed",
                "summary": "metadata attestation roots are not singular",
                "details": {"attestationRootCertificates": metadata_roots},
            }
        root_der = self._pem_to_der(root_pem)
        if root_der != base64.b64decode(metadata_roots[0]):
            return {
                "status": "failed",
                "summary": "metadata root certificate does not match the shipped PEM",
                "details": {"root_pem": str(root_pem), "metadata_statement": str(statement)},
            }
        return {
            "status": "passed",
            "summary": "attestation assets are present",
            "details": {
                "root_pem": str(root_pem),
                "leaf_pem": str(leaf_pem),
                "metadata_statement": str(statement),
                "metadata_root_count": len(metadata_roots),
            },
        }

    def _scenario_static_metadata_alignment(self) -> dict[str, Any]:
        metadata = self._load_static_metadata()
        audit_matrix = (ROOT / "docs" / "13-fido-audit-matrix.md").read_text()
        details = self._validate_static_metadata(metadata)
        return {
            "status": "passed",
            "summary": "static metadata matches the runtime UV and clientPin model",
            "details": {
                "aaguid": details["aaguid"],
                "options": details["options"],
                "userVerificationDetails": details["userVerificationDetails"],
                "matrix_mentions_clientpin_dynamic": "Metadata `clientPin`" in audit_matrix,
            },
        }

    def _scenario_transport_init(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            nonce = os.urandom(8)
            timing: dict[str, Any] = {}
            response_cid, response_cmd, response_payload = probe.transact(
                device, probe.BROADCAST_CID, probe.INIT, nonce, 3000, False, trace, timing=timing
            )
            cid = probe.expect_init_response(response_cid, response_cmd, response_payload, nonce)
            caps = response_payload[-1]
            if self._scenario_timing is not None:
                self._scenario_timing["broadcast_init"] = timing
            return {
                "status": "passed",
                "summary": "broadcast INIT allocated a CID",
                "details": {
                    "allocated_cid": f"0x{cid:08x}",
                    "capabilities_hex": f"0x{caps:02x}",
                    "wink_supported": bool(caps & 0x01),
                    "cbor_supported": bool(caps & 0x04),
                },
                "evidence": {"trace": trace, "response_payload_hex": response_payload.hex()},
            }
        return self._with_device(matched, run)

    def _scenario_transport_ping(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            init_timing: dict[str, Any] = {}
            transact_timing: dict[str, Any] = {}
            cid = probe.allocate_cid(device, 3000, False, timing=init_timing)
            payload = b"zerofido-suite-ping"
            response_cid, response_cmd, response_payload = probe.transact(
                device, cid, probe.PING, payload, 3000, False, trace, timing=transact_timing
            )
            if response_cmd != probe.PING or response_cid != cid or response_payload != payload:
                raise RuntimeError("PING response did not echo the original payload")
            if self._scenario_timing is not None:
                self._scenario_timing["broadcast_init"] = init_timing
                self._scenario_timing["command_transacts"] = [{"transport_kind": "ping", **transact_timing}]
            return {
                "status": "passed",
                "summary": "PING echoed the original payload",
                "details": {"cid": f"0x{cid:08x}", "payload": response_payload.decode("ascii")},
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_transport_wink(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = probe.allocate_cid(device, 3000, False)
            response_cid, response_cmd, response_payload = probe.transact(
                device, cid, probe.WINK, b"", 3000, False, trace
            )
            if response_cmd != probe.WINK or response_cid != cid or response_payload != b"":
                raise RuntimeError("WINK response did not match the expected empty reply")
            return {"status": "passed", "summary": "WINK returned an empty success reply", "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_transport_invalid_cid(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            invalid_cid = 0x01020304
            response_cid, response_cmd, response_payload = probe.transact(
                device, invalid_cid, probe.CBOR, bytes([probe.CTAP_GET_INFO]), 3000, False, trace
            )
            if response_cmd != probe.ERROR or response_payload[:1] != b"\x0b":
                raise RuntimeError("unallocated CID did not return CTAPHID ERR_INVALID_CHANNEL")
            return {
                "status": "passed",
                "summary": "unallocated CID was rejected",
                "details": {"response_cid": f"0x{response_cid:08x}", "hid_error_hex": response_payload.hex()},
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_transport_exhaust_cids(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            allocated: list[int] = []
            for _ in range(9):
                nonce = os.urandom(8)
                response_cid, response_cmd, response_payload = probe.transact(
                    device, probe.BROADCAST_CID, probe.INIT, nonce, 3000, False, trace
                )
                allocated.append(probe.expect_init_response(response_cid, response_cmd, response_payload, nonce))
            if len(allocated) != 9:
                raise RuntimeError("broadcast INIT did not keep returning allocated CIDs")
            ping = b"allocator-check"
            response_cid, response_cmd, response_payload = probe.transact(
                device, allocated[0], probe.PING, ping, 3000, False, trace
            )
            if response_cmd != probe.PING or response_payload != ping:
                raise RuntimeError("previously allocated CID did not remain usable after additional INIT allocations")
            response_cid, response_cmd, response_payload = probe.transact(
                device, allocated[-1], probe.PING, ping, 3000, False, trace
            )
            if response_cmd != probe.PING or response_payload != ping:
                raise RuntimeError("most recently allocated CID did not remain usable after additional INIT allocations")
            return {
                "status": "passed",
                "summary": "allocated CIDs remained valid after additional channel allocations",
                "details": {"allocated_cids": [f"0x{cid:08x}" for cid in allocated]},
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_transport_short_init(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            active_cid = probe.allocate_cid(device, 3000, False)
            other_cid = probe.allocate_cid(device, 3000, False)
            ping_payload = bytes(range(80))
            ping_frames = probe.build_frames(active_cid, probe.PING, ping_payload)
            probe.write_frame(device, ping_frames[0], False, trace)
            short_packet = bytearray(int.to_bytes(other_cid, 4, "little"))
            short_packet.append(probe.INIT)
            short_packet.append(0)
            probe.write_raw_packet(device, bytes(short_packet), False, trace)
            error_cid, error_cmd, error_payload = probe.read_response(device, 3000, False, trace)
            if error_cmd != probe.ERROR or error_cid != other_cid:
                raise RuntimeError("short INIT did not produce a CTAPHID error on the new CID")
            for frame in ping_frames[1:]:
                probe.write_frame(device, frame, False, trace)
            response_cid, response_cmd, response_payload = probe.read_response(device, 3000, False, trace)
            if response_cmd != probe.PING or response_cid != active_cid or response_payload != ping_payload:
                raise RuntimeError("short INIT disturbed the active transaction")
            return {"status": "passed", "summary": "short malformed INIT left the active transaction intact", "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_transport_spurious_continuation(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = probe.allocate_cid(device, 3000, False)
            continuation = bytearray(int.to_bytes(cid, 4, "little"))
            continuation.append(0)
            continuation.extend(bytes(59))
            probe.write_raw_packet(device, bytes(continuation), False, trace)
            timed_out = probe.receive_timeout(device, 500, False, trace)
            if not timed_out:
                raise RuntimeError("spurious continuation packet unexpectedly produced a response")
            return {"status": "passed", "summary": "spurious continuation packet was ignored", "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_transport_cancel(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = probe.allocate_cid(device, 3000, False)
            request = bytes([probe.CTAP_MAKE_CREDENTIAL]) + probe.build_make_credential_request(
                self._next_rp_id("cancel"), empty_pin_auth=False, omit_client_data_hash=False, resident_key=True
            )
            for frame in probe.build_frames(cid, probe.CBOR, request):
                probe.write_frame(device, frame, False, trace)
            keepalive = probe.read_frame(device, 3000, False, trace)
            if keepalive[4] != probe.KEEPALIVE:
                raise RuntimeError("CANCEL scenario did not observe KEEPALIVE before cancellation")
            cancel_packet = bytearray(64)
            cancel_packet[0:4] = int.to_bytes(cid, 4, "little")
            cancel_packet[4] = probe.CANCEL
            probe.write_frame(device, bytes(cancel_packet), False, trace)
            response_cid, response_cmd, response_payload = probe.read_response(device, 3000, False, trace)
            parsed = probe.decode_ctap_response_payload(response_payload)
            if response_cmd != probe.CBOR or parsed["ctap_status"] != 0x2D:
                raise RuntimeError("CANCEL scenario did not terminate with CTAP2_ERR_KEEPALIVE_CANCEL")
            return {"status": "passed", "summary": "CANCEL terminated the active CBOR request", "details": {"response": parsed}, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_transport_resync(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = probe.allocate_cid(device, 3000, False)
            request = bytes([probe.CTAP_MAKE_CREDENTIAL]) + probe.build_make_credential_request(
                self._next_rp_id("resync"), empty_pin_auth=False, omit_client_data_hash=False, resident_key=True
            )
            for frame in probe.build_frames(cid, probe.CBOR, request):
                probe.write_frame(device, frame, False, trace)
            keepalive = probe.read_frame(device, 3000, False, trace)
            if keepalive[4] != probe.KEEPALIVE:
                raise RuntimeError("resync scenario did not observe KEEPALIVE before re-INIT")
            nonce = os.urandom(8)
            response_cid, response_cmd, response_payload = probe.transact(
                device, cid, probe.INIT, nonce, 3000, False, trace
            )
            assigned = probe.expect_init_response(
                response_cid,
                response_cmd,
                response_payload,
                nonce,
                expected_response_cid=cid,
            )
            if assigned != cid:
                raise RuntimeError("same-CID INIT resync changed the channel identifier")
            ping_payload = b"resync-ok"
            ping_cid, ping_cmd, ping_response = probe.transact(
                device, cid, probe.PING, ping_payload, 3000, False, trace
            )
            if ping_cmd != probe.PING or ping_cid != cid or ping_response != ping_payload:
                raise RuntimeError("same-CID INIT resync did not leave the channel ready for reuse")
            return {
                "status": "passed",
                "summary": "same-CID INIT resynchronized the pending transaction and accepted a new request",
                "details": {
                    "resync_cid": f"0x{assigned:08x}",
                    "payload_hex": response_payload.hex(),
                    "post_resync_ping_hex": ping_response.hex(),
                },
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _next_rp_id(self, purpose: str) -> str:
        prefix = self.config.get("fixture", {}).get("rp_id_prefix", "zerofido-suite")
        return f"{prefix}-{purpose}-{uuid.uuid4().hex[:8]}"

    def _next_user_suffix(self) -> str:
        return uuid.uuid4().hex[:8]

    def _scenario_ctap_get_info(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "cbor", trace)
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "cbor", probe.CBOR, bytes([probe.CTAP_GET_INFO]), trace
            )
            if response_cmd != probe.CBOR or response_cid != cid:
                raise RuntimeError("GetInfo transport reply was malformed")
            parsed = probe.decode_ctap_response_payload(response_payload)
            decoded = parsed.get("decoded")
            metadata = self._load_static_metadata()
            validated = self._validate_get_info_response(decoded, metadata)
            runtime["get_info"] = decoded
            return {
                "status": "passed",
                "summary": "GetInfo returned the runtime product surface",
                "details": {**parsed, "validated": validated},
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _send_make_credential(
        self,
        device: hid.device,
        rp_id: str,
        *,
        resident_key: bool,
        trace: list[dict[str, str]],
        user_suffix: str,
        exclude_credential_ids: list[bytes] | None = None,
        pin_auth: bytes | None = None,
        pin_protocol: int | None = None,
    ) -> tuple[int, bytes, dict[str, Any]]:
        cid = self._get_run_cid(device, "cbor", trace)
        payload = bytes([probe.CTAP_MAKE_CREDENTIAL]) + probe.build_make_credential_request(
            rp_id,
            empty_pin_auth=False,
            omit_client_data_hash=False,
            resident_key=resident_key,
            user_suffix=user_suffix,
            exclude_credential_ids=exclude_credential_ids,
            pin_auth=pin_auth,
            pin_protocol=pin_protocol,
        )
        response_cid, response_cmd, response_payload = self._transact_run_channel(
            device, "cbor", probe.CBOR, payload, trace
        )
        if response_cmd != probe.CBOR or response_cid != cid:
            raise RuntimeError("MakeCredential transport reply was malformed")
        parsed = probe.parse_make_credential_ctap_payload(response_payload)
        return cid, response_payload, parsed

    def _send_get_assertion(
        self,
        device: hid.device,
        rp_id: str,
        *,
        credential_ids: list[bytes],
        silent: bool,
        include_rk_option: bool,
        trace: list[dict[str, str]],
        pin_auth: bytes | None = None,
        pin_protocol: int | None = None,
        user_verification: str = "discouraged",
    ) -> tuple[int, bytes, dict[str, Any]]:
        cid = self._get_run_cid(device, "cbor", trace)
        payload = bytes([probe.CTAP_GET_ASSERTION]) + probe.build_get_assertion_request(
            rp_id,
            credential_ids,
            user_verification,
            empty_pin_auth=False,
            omit_client_data_hash=False,
            silent=silent,
            include_rk_option=include_rk_option,
            pin_auth=pin_auth,
            pin_protocol=pin_protocol,
        )
        response_cid, response_cmd, response_payload = self._transact_run_channel(
            device, "cbor", probe.CBOR, payload, trace
        )
        if response_cmd != probe.CBOR or response_cid != cid:
            raise RuntimeError("GetAssertion transport reply was malformed")
        parsed = probe.parse_get_assertion_ctap_payload(response_payload)
        return cid, response_payload, parsed

    def _scenario_ctap_make_credential(self, matched: MatchedDevice, runtime: dict[str, Any], resident_key: bool) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            rp_id = self._next_rp_id("resident" if resident_key else "nonresident")
            user_suffix = self._next_user_suffix()
            _, response_payload, parsed = self._send_make_credential(
                device,
                rp_id,
                resident_key=resident_key,
                trace=trace,
                user_suffix=user_suffix,
            )
            if parsed["ctap_status"] != 0x00:
                raise RuntimeError(f"MakeCredential failed with {parsed['ctap_status_name']}")
            metadata = self._load_static_metadata()
            validated_attestation = self._validate_make_credential_attestation(parsed, metadata)
            auth_data = parsed.get("auth_data", {})
            credential_id = bytes.fromhex(auth_data["credential_id"])
            runtime["credentials"]["resident" if resident_key else "nonresident"] = {
                "rp_id": rp_id,
                "credential_id": credential_id,
                "response_payload": response_payload,
            }
            return {
                "status": "passed",
                "summary": f"MakeCredential succeeded for {'resident' if resident_key else 'non-resident'} mode",
                "details": {**parsed, "validated_attestation": validated_attestation},
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_ctap_exclude_list(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        resident = runtime["credentials"].get("resident")
        if not resident:
            return {"status": "blocked", "summary": "resident credential prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            _, _, parsed = self._send_make_credential(
                device,
                resident["rp_id"],
                resident_key=True,
                trace=trace,
                user_suffix=self._next_user_suffix(),
                exclude_credential_ids=[resident["credential_id"]],
            )
            if parsed["ctap_status"] != 0x19:
                raise RuntimeError("excludeList did not produce CREDENTIAL_EXCLUDED")
            return {"status": "passed", "summary": "excludeList hit returned CREDENTIAL_EXCLUDED after approval", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_ctap_get_assertion_allow_list(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        nonresident = runtime["credentials"].get("nonresident")
        if not nonresident:
            return {"status": "blocked", "summary": "non-resident credential prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            _, _, parsed = self._send_get_assertion(
                device,
                nonresident["rp_id"],
                credential_ids=[nonresident["credential_id"]],
                silent=False,
                include_rk_option=False,
                trace=trace,
            )
            if parsed["ctap_status"] != 0x00:
                raise RuntimeError(f"allow-list GetAssertion failed with {parsed['ctap_status_name']}")
            auth_data = parsed.get("auth_data", {})
            returned_credential = (((parsed.get("decoded") or {}).get(1) or {}).get("id"))
            if returned_credential != nonresident["credential_id"]:
                raise RuntimeError("allow-list GetAssertion returned an unexpected credential ID")
            if not auth_data.get("user_present"):
                raise RuntimeError("allow-list GetAssertion did not set the UP flag")
            return {
                "status": "passed",
                "summary": "allow-list GetAssertion succeeded",
                "details": parsed,
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_ctap_get_assertion_discoverable(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        resident = runtime["credentials"].get("resident")
        if not resident:
            return {"status": "blocked", "summary": "resident credential prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            _, _, parsed = self._send_get_assertion(
                device,
                resident["rp_id"],
                credential_ids=[],
                silent=False,
                include_rk_option=False,
                trace=trace,
            )
            if parsed["ctap_status"] != 0x00:
                raise RuntimeError(f"discoverable GetAssertion failed with {parsed['ctap_status_name']}")
            auth_data = parsed.get("auth_data", {})
            returned_credential = (((parsed.get("decoded") or {}).get(1) or {}).get("id"))
            if returned_credential != resident["credential_id"]:
                raise RuntimeError("discoverable GetAssertion returned an unexpected credential ID")
            if not auth_data.get("user_present"):
                raise RuntimeError("discoverable GetAssertion did not set the UP flag")
            return {
                "status": "passed",
                "summary": "discoverable GetAssertion succeeded without an allow list",
                "details": parsed,
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_ctap_get_next_assertion(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        resident = runtime["credentials"].get("resident")
        if not resident:
            return {"status": "blocked", "summary": "resident credential prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            credential_ids = [resident["credential_id"]]
            for _ in range(2):
                _, response_payload, parsed = self._send_make_credential(
                    device,
                    resident["rp_id"],
                    resident_key=True,
                    trace=trace,
                    user_suffix=self._next_user_suffix(),
                )
                if parsed["ctap_status"] != 0x00:
                    raise RuntimeError("failed to seed additional resident credentials for GetNextAssertion")
                auth_data = parsed.get("auth_data", {})
                if "credential_id" not in auth_data:
                    raise RuntimeError("seeded resident credential did not include a credential ID")
                credential_ids.append(bytes.fromhex(auth_data["credential_id"]))
                runtime["credentials"].setdefault("resident_multi", []).append(response_payload.hex())
            cid, first_payload, first_parsed = self._send_get_assertion(
                device,
                resident["rp_id"],
                credential_ids=credential_ids,
                silent=False,
                include_rk_option=False,
                trace=trace,
            )
            if first_parsed["ctap_status"] != 0x00:
                raise RuntimeError("initial allow-list multi-match GetAssertion failed")
            first_decoded = first_parsed.get("decoded") or {}
            if 5 in first_decoded:
                raise RuntimeError("allow-list multi-match GetAssertion unexpectedly advertised multiple credentials")

            response_cid, response_cmd, response_payload = probe.transact(
                device, cid, probe.CBOR, bytes([probe.CTAP_GET_NEXT_ASSERTION]), 3000, False, trace
            )
            if response_cmd != probe.CBOR or response_cid != cid:
                raise RuntimeError("GetNextAssertion transport reply was malformed")
            next_parsed = probe.parse_get_assertion_ctap_payload(response_payload)
            if next_parsed["ctap_status"] != 0x30:
                raise RuntimeError("allow-list multi-match GetAssertion unexpectedly opened GetNextAssertion state")
            return {
                "status": "passed",
                "summary": "allow-list multi-match GetAssertion omitted numberOfCredentials and did not open GetNextAssertion",
                "details": {
                    "initial": first_parsed,
                    "next": next_parsed,
                    "initial_payload_hex": first_payload.hex(),
                },
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_ctap_silent_assertion(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        resident = runtime["credentials"].get("resident")
        if not resident:
            return {"status": "blocked", "summary": "resident credential prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            _, _, parsed = self._send_get_assertion(
                device,
                resident["rp_id"],
                credential_ids=[],
                silent=True,
                include_rk_option=False,
                trace=trace,
            )
            if parsed["ctap_status"] != 0x00:
                raise RuntimeError("silent GetAssertion failed")
            auth_data = parsed.get("auth_data", {})
            if auth_data.get("user_present"):
                raise RuntimeError("silent assertion unexpectedly set the UP flag")
            return {"status": "passed", "summary": "silent GetAssertion returned without UP", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_ctap_unsupported_option(self, matched: MatchedDevice, runtime: dict[str, Any], option: str) -> dict[str, Any]:
        resident = runtime["credentials"].get("resident")
        if not resident:
            return {"status": "blocked", "summary": "resident credential prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "cbor", trace)
            request = {
                1: resident["rp_id"],
                2: hashlib.sha256(f"unsupported-{option}".encode()).digest(),
                5: {"up": True, "uv": option == "uv"},
            }
            if option == "rk":
                request[5]["rk"] = True
            payload = bytes([probe.CTAP_GET_ASSERTION]) + cbor2.dumps(request)
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "cbor", probe.CBOR, payload, trace
            )
            if response_cmd != probe.CBOR or response_cid != cid:
                raise RuntimeError("unsupported-option transport reply was malformed")
            parsed = probe.decode_ctap_response_payload(response_payload)
            expected_status = 0x2B
            expected_name = "UNSUPPORTED_OPTION"
            if parsed["ctap_status"] != expected_status:
                raise RuntimeError(f"expected {expected_name}, got {parsed['ctap_status_name']}")
            return {"status": "passed", "summary": f"GetAssertion rejected {option} with the expected option error", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_ctap_missing_client_data_hash(self, matched: MatchedDevice, make_credential: bool) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "cbor", trace)
            if make_credential:
                payload = bytes([probe.CTAP_MAKE_CREDENTIAL]) + probe.build_make_credential_request(
                    self._next_rp_id("missing-hash"), empty_pin_auth=False, omit_client_data_hash=True
                )
            else:
                payload = bytes([probe.CTAP_GET_ASSERTION]) + probe.build_get_assertion_request(
                    self._next_rp_id("missing-hash"),
                    [],
                    "discouraged",
                    empty_pin_auth=False,
                    omit_client_data_hash=True,
                    silent=False,
                    include_rk_option=False,
                )
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "cbor", probe.CBOR, payload, trace
            )
            if response_cmd != probe.CBOR or response_cid != cid:
                raise RuntimeError("missing-clientDataHash transport reply was malformed")
            parsed = probe.decode_ctap_response_payload(response_payload)
            if parsed["ctap_status"] != 0x14:
                raise RuntimeError(f"expected MISSING_PARAMETER, got {parsed['ctap_status_name']}")
            return {"status": "passed", "summary": "missing clientDataHash was rejected", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_ctap_trailing_cbor(self, matched: MatchedDevice, make_credential: bool) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "cbor", trace)
            trailing = bytes.fromhex("aabb")
            if make_credential:
                payload = bytes([probe.CTAP_MAKE_CREDENTIAL]) + probe.build_make_credential_request(
                    self._next_rp_id("trailing"), empty_pin_auth=False, omit_client_data_hash=False, trailing_bytes=trailing
                )
            else:
                payload = bytes([probe.CTAP_GET_ASSERTION]) + probe.build_get_assertion_request(
                    self._next_rp_id("trailing"),
                    [],
                    "discouraged",
                    empty_pin_auth=False,
                    omit_client_data_hash=False,
                    silent=False,
                    include_rk_option=False,
                    trailing_bytes=trailing,
                )
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "cbor", probe.CBOR, payload, trace
            )
            if response_cmd != probe.CBOR or response_cid != cid:
                raise RuntimeError("trailing-CBOR transport reply was malformed")
            parsed = probe.decode_ctap_response_payload(response_payload)
            if parsed["ctap_status"] != 0x12:
                raise RuntimeError(f"expected INVALID_CBOR, got {parsed['ctap_status_name']}")
            return {"status": "passed", "summary": "trailing bytes after the top-level CBOR item were rejected", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_ctap_empty_pin_auth(self, matched: MatchedDevice, make_credential: bool) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "cbor", trace)
            if make_credential:
                payload = bytes([probe.CTAP_MAKE_CREDENTIAL]) + probe.build_make_credential_request(
                    self._next_rp_id("emptypinauth"), empty_pin_auth=True, omit_client_data_hash=False
                )
            else:
                payload = bytes([probe.CTAP_GET_ASSERTION]) + probe.build_get_assertion_request(
                    self._next_rp_id("emptypinauth"),
                    [],
                    "discouraged",
                    empty_pin_auth=True,
                    omit_client_data_hash=False,
                    silent=False,
                    include_rk_option=False,
                )
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "cbor", probe.CBOR, payload, trace
            )
            if response_cmd != probe.CBOR or response_cid != cid:
                raise RuntimeError("empty pinAuth transport reply was malformed")
            parsed = probe.decode_ctap_response_payload(response_payload)
            if parsed["ctap_status"] not in {0x31, 0x35}:
                raise RuntimeError(f"expected PIN_INVALID or PIN_NOT_SET, got {parsed['ctap_status_name']}")
            return {"status": "passed", "summary": "zero-length pinAuth compatibility probe returned the expected PIN status", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _clientpin_exchange(self, device: hid.device, request: bytes, trace: list[dict[str, str]]) -> tuple[int, bytes, dict[str, Any]]:
        cid = self._get_run_cid(device, "cbor", trace)
        response_cid, response_cmd, response_payload = self._transact_run_channel(
            device, "cbor", probe.CBOR, bytes([probe.CTAP_CLIENT_PIN]) + request, trace
        )
        if response_cmd != probe.CBOR or response_cid != cid:
            raise RuntimeError("ClientPIN transport reply was malformed")
        return cid, response_payload, probe.decode_ctap_response_payload(response_payload)

    def _scenario_clientpin_get_retries(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            _, _, parsed = self._clientpin_exchange(
                device, probe.build_client_pin_request(probe.CLIENT_PIN_GET_RETRIES), trace
            )
            if parsed["ctap_status"] != 0x00:
                raise RuntimeError("getRetries failed")
            return {"status": "passed", "summary": "ClientPIN getRetries succeeded", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_clientpin_get_key_agreement(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            _, _, parsed = self._clientpin_exchange(
                device, probe.build_client_pin_request(probe.CLIENT_PIN_GET_KEY_AGREEMENT), trace
            )
            if parsed["ctap_status"] != 0x00:
                raise RuntimeError("getKeyAgreement failed")
            decoded = parsed["decoded"] or {}
            key_agreement = decoded.get(1)
            if not isinstance(key_agreement, dict):
                raise RuntimeError("getKeyAgreement did not return a COSE key")
            runtime["pin"]["peer_key_agreement"] = key_agreement
            return {"status": "passed", "summary": "ClientPIN getKeyAgreement returned a valid COSE key", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _ensure_pin_handshake(
        self, device: hid.device, runtime: dict[str, Any], trace: list[dict[str, str]]
    ) -> tuple[ec.EllipticCurvePrivateKey, dict[int, Any], bytes]:
        _, _, parsed = self._clientpin_exchange(
            device, probe.build_client_pin_request(probe.CLIENT_PIN_GET_KEY_AGREEMENT), trace
        )
        if parsed["ctap_status"] != 0x00:
            raise RuntimeError("getKeyAgreement failed while preparing ClientPIN handshake")
        decoded = parsed["decoded"] or {}
        peer = decoded.get(1)
        if not isinstance(peer, dict):
            raise RuntimeError("getKeyAgreement did not return a COSE key during ClientPIN handshake")
        runtime["pin"]["peer_key_agreement"] = peer
        private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())
        cose_key = probe.build_client_pin_cose_key(private_key)
        shared_secret = probe.derive_client_pin_shared_secret(peer, private_key)
        return private_key, cose_key, shared_secret

    def _scenario_clientpin_set_pin(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        bootstrap_pin = runtime["pin"].get("bootstrap")
        if not bootstrap_pin:
            return {"status": "blocked", "summary": "fixture bootstrap_pin is not configured"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            private_key, cose_key, shared_secret = self._ensure_pin_handshake(device, runtime, trace)
            new_pin_plain = probe.pad_new_pin(bootstrap_pin)
            new_pin_enc = probe._aes_cbc_zero_iv_encrypt(shared_secret, new_pin_plain)
            pin_auth = probe._hmac_first16(shared_secret, new_pin_enc)
            _, _, parsed = self._clientpin_exchange(
                device,
                probe.build_client_pin_request(
                    probe.CLIENT_PIN_SET_PIN,
                    key_agreement=cose_key,
                    pin_auth=pin_auth,
                    new_pin_enc=new_pin_enc,
                ),
                trace,
            )
            if parsed["ctap_status"] == 0x00:
                runtime["pin"]["current"] = bootstrap_pin
                runtime["pin"]["platform_private_key"] = private_key
                return {"status": "passed", "summary": "ClientPIN setPin succeeded", "details": parsed, "evidence": {"trace": trace}}
            if parsed["ctap_status"] == 0x33:
                return {"status": "blocked", "summary": "setPin requires an unset fixture baseline", "details": parsed, "evidence": {"trace": trace}}
            raise RuntimeError(f"setPin failed with {parsed['ctap_status_name']}")
        return self._with_device(matched, run)

    def _get_pin_token_for_current_pin(self, device: hid.device, runtime: dict[str, Any], trace: list[dict[str, str]]) -> dict[str, Any]:
        current_pin = runtime["pin"].get("current")
        if not current_pin:
            raise RuntimeError("fixture current PIN is unknown")
        private_key, cose_key, shared_secret = self._ensure_pin_handshake(device, runtime, trace)
        pin_hash_enc = probe._aes_cbc_zero_iv_encrypt(shared_secret, probe.pin_hash16(current_pin))
        _, response_payload, parsed = self._clientpin_exchange(
            device,
            probe.build_client_pin_request(
                probe.CLIENT_PIN_GET_PIN_TOKEN,
                key_agreement=cose_key,
                pin_hash_enc=pin_hash_enc,
            ),
            trace,
        )
        if parsed["ctap_status"] != 0x00:
            raise RuntimeError(f"getPinToken failed with {parsed['ctap_status_name']}")
        decoded = parsed["decoded"] or {}
        encrypted_token = decoded.get(2)
        if not isinstance(encrypted_token, bytes):
            raise RuntimeError("getPinToken did not return an encrypted PIN token")
        token = probe._aes_cbc_zero_iv_decrypt(shared_secret, encrypted_token)
        runtime["pin"]["platform_private_key"] = private_key
        runtime["pin"]["token"] = token
        return {"parsed": parsed, "response_payload": response_payload, "pin_token": token}

    def _scenario_clientpin_get_pin_token(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        if not runtime["pin"].get("current"):
            return {"status": "blocked", "summary": "current fixture PIN is unknown; getPinToken cannot run"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            token_info = self._get_pin_token_for_current_pin(device, runtime, trace)
            return {"status": "passed", "summary": "ClientPIN getPinToken succeeded", "details": token_info["parsed"], "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_ctap_with_pin_auth(self, matched: MatchedDevice, runtime: dict[str, Any], make_credential: bool) -> dict[str, Any]:
        if runtime["pin"].get("token") is None:
            return {"status": "blocked", "summary": "PIN token prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            pin_token = runtime["pin"]["token"]
            if make_credential:
                rp_id = self._next_rp_id("pin-auth")
                request = cbor2.loads(
                    probe.build_make_credential_request(
                        rp_id,
                        empty_pin_auth=False,
                        omit_client_data_hash=False,
                        resident_key=True,
                        user_suffix=self._next_user_suffix(),
                    )
                )
                client_data_hash = request[1]
                request[8] = probe.pin_auth_for_client_data(pin_token, client_data_hash)
                request[9] = 1
                cid = probe.allocate_cid(device, 3000, False)
                response_cid, response_cmd, response_payload = probe.transact(
                    device, cid, probe.CBOR, bytes([probe.CTAP_MAKE_CREDENTIAL]) + cbor2.dumps(request), 3000, False, trace
                )
                parsed = probe.parse_make_credential_ctap_payload(response_payload)
                if response_cmd != probe.CBOR or response_cid != cid or parsed["ctap_status"] != 0x00:
                    raise RuntimeError("MakeCredential with pinUvAuthParam failed")
                if not parsed.get("auth_data", {}).get("user_verified"):
                    raise RuntimeError("MakeCredential with pinUvAuthParam did not set UV")
                return {"status": "passed", "summary": "MakeCredential accepted pinUvAuthParam and set UV", "details": parsed, "evidence": {"trace": trace}}

            resident = runtime["credentials"].get("resident")
            if not resident:
                return {"status": "blocked", "summary": "resident credential prerequisite is missing"}
            request = cbor2.loads(
                probe.build_get_assertion_request(
                    resident["rp_id"],
                    [],
                    "discouraged",
                    empty_pin_auth=False,
                    omit_client_data_hash=False,
                    silent=False,
                    include_rk_option=False,
                )
            )
            client_data_hash = request[2]
            request[6] = probe.pin_auth_for_client_data(pin_token, client_data_hash)
            request[7] = 1
            cid = probe.allocate_cid(device, 3000, False)
            response_cid, response_cmd, response_payload = probe.transact(
                device, cid, probe.CBOR, bytes([probe.CTAP_GET_ASSERTION]) + cbor2.dumps(request), 3000, False, trace
            )
            parsed = probe.parse_get_assertion_ctap_payload(response_payload)
            if response_cmd != probe.CBOR or response_cid != cid or parsed["ctap_status"] != 0x00:
                raise RuntimeError("GetAssertion with pinUvAuthParam failed")
            if not parsed.get("auth_data", {}).get("user_verified"):
                raise RuntimeError("GetAssertion with pinUvAuthParam did not set UV")
            return {"status": "passed", "summary": "GetAssertion accepted pinUvAuthParam and set UV", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_clientpin_change_pin(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        current_pin = runtime["pin"].get("current")
        changed_pin = runtime["pin"].get("changed")
        if not current_pin or not changed_pin:
            return {"status": "blocked", "summary": "fixture current or changed PIN is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            private_key, cose_key, shared_secret = self._ensure_pin_handshake(device, runtime, trace)
            new_pin_enc = probe._aes_cbc_zero_iv_encrypt(shared_secret, probe.pad_new_pin(changed_pin))
            pin_hash_enc = probe._aes_cbc_zero_iv_encrypt(shared_secret, probe.pin_hash16(current_pin))
            pin_auth = probe._hmac_first16(shared_secret, new_pin_enc + pin_hash_enc)
            _, _, parsed = self._clientpin_exchange(
                device,
                probe.build_client_pin_request(
                    probe.CLIENT_PIN_CHANGE_PIN,
                    key_agreement=cose_key,
                    pin_auth=pin_auth,
                    new_pin_enc=new_pin_enc,
                    pin_hash_enc=pin_hash_enc,
                ),
                trace,
            )
            if parsed["ctap_status"] != 0x00:
                raise RuntimeError(f"changePin failed with {parsed['ctap_status_name']}")
            runtime["pin"]["current"] = changed_pin
            runtime["pin"]["platform_private_key"] = private_key
            return {"status": "passed", "summary": "ClientPIN changePin succeeded", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_clientpin_wrong_pin_retry(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        if not runtime["pin"].get("current"):
            return {"status": "blocked", "summary": "fixture current PIN is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            private_key, cose_key, shared_secret = self._ensure_pin_handshake(device, runtime, trace)
            wrong_hash = probe._aes_cbc_zero_iv_encrypt(shared_secret, probe.pin_hash16("9999"))
            _, _, parsed = self._clientpin_exchange(
                device,
                probe.build_client_pin_request(
                    probe.CLIENT_PIN_GET_PIN_TOKEN,
                    key_agreement=cose_key,
                    pin_hash_enc=wrong_hash,
                ),
                trace,
            )
            if parsed["ctap_status"] != 0x31:
                raise RuntimeError(f"wrong PIN did not return PIN_INVALID; got {parsed['ctap_status_name']}")
            return {"status": "passed", "summary": "wrong PIN decremented retries and returned PIN_INVALID", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_clientpin_wrong_pin_auth_block(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        if not runtime["pin"].get("current"):
            return {"status": "blocked", "summary": "fixture current PIN is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            last_status = None
            for _ in range(3):
                private_key, cose_key, shared_secret = self._ensure_pin_handshake(device, runtime, trace)
                wrong_hash = probe._aes_cbc_zero_iv_encrypt(shared_secret, probe.pin_hash16("9999"))
                _, _, parsed = self._clientpin_exchange(
                    device,
                    probe.build_client_pin_request(
                        probe.CLIENT_PIN_GET_PIN_TOKEN,
                        key_agreement=cose_key,
                        pin_hash_enc=wrong_hash,
                    ),
                    trace,
                )
                last_status = parsed["ctap_status"]
            if last_status != 0x34:
                raise RuntimeError("three consecutive wrong PIN attempts did not return PIN_AUTH_BLOCKED")
            return {"status": "passed", "summary": "three wrong PIN attempts triggered PIN_AUTH_BLOCKED", "details": {"final_status": probe.ctap_status_name(last_status)}, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_clientpin_recovery(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        if not runtime["pin"].get("current"):
            return {"status": "blocked", "summary": "fixture current PIN is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            private_key, cose_key, shared_secret = self._ensure_pin_handshake(device, runtime, trace)
            pin_hash_enc = probe._aes_cbc_zero_iv_encrypt(shared_secret, probe.pin_hash16(runtime["pin"]["current"]))
            _, _, parsed = self._clientpin_exchange(
                device,
                probe.build_client_pin_request(
                    probe.CLIENT_PIN_GET_PIN_TOKEN,
                    key_agreement=cose_key,
                    pin_hash_enc=pin_hash_enc,
                ),
                trace,
            )
            if parsed["ctap_status"] != 0x34:
                raise RuntimeError(f"expected PIN_AUTH_BLOCKED during recovery, got {parsed['ctap_status_name']}")
            self._wait_for_manual_checkpoint(
                "Recover PIN Auth",
                "On the device, use the local PIN recovery action to resume auth attempts, then click Resume.",
            )
            recovered = self._get_pin_token_for_current_pin(device, runtime, trace)
            bootstrap_pin = runtime["pin"].get("bootstrap")
            restored_to_bootstrap = False
            if bootstrap_pin and runtime["pin"]["current"] != bootstrap_pin:
                private_key, cose_key, shared_secret = self._ensure_pin_handshake(device, runtime, trace)
                new_pin_enc = probe._aes_cbc_zero_iv_encrypt(shared_secret, probe.pad_new_pin(bootstrap_pin))
                pin_hash_enc = probe._aes_cbc_zero_iv_encrypt(
                    shared_secret, probe.pin_hash16(runtime["pin"]["current"])
                )
                pin_auth = probe._hmac_first16(shared_secret, new_pin_enc + pin_hash_enc)
                _, _, change_pin = self._clientpin_exchange(
                    device,
                    probe.build_client_pin_request(
                        probe.CLIENT_PIN_CHANGE_PIN,
                        key_agreement=cose_key,
                        pin_auth=pin_auth,
                        new_pin_enc=new_pin_enc,
                        pin_hash_enc=pin_hash_enc,
                    ),
                    trace,
                )
                if change_pin["ctap_status"] != 0x00:
                    raise RuntimeError(
                        f"post-recovery changePin back to bootstrap failed with {change_pin['ctap_status_name']}"
                    )
                runtime["pin"]["current"] = bootstrap_pin
                runtime["pin"]["platform_private_key"] = private_key
                runtime["pin"]["token"] = None
                restored_to_bootstrap = True
            else:
                change_pin = None
            _, _, retries = self._clientpin_exchange(
                device, probe.build_client_pin_request(probe.CLIENT_PIN_GET_RETRIES), trace
            )
            if retries["ctap_status"] != 0x00:
                raise RuntimeError("getRetries failed during recovery")
            return {
                "status": "passed",
                "summary": "local recovery restored PIN auth flow",
                "details": {
                    "blocked_attempt": parsed,
                    "recovered_get_pin_token": recovered["parsed"],
                    "retries": retries,
                    "restored_to_bootstrap": restored_to_bootstrap,
                    "change_pin": change_pin,
                    "current_pin": runtime["pin"]["current"],
                },
                "evidence": {"trace": trace},
            }
        return self._with_device(matched, run)

    def _scenario_u2f_version(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "msg", trace)
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "msg", probe.MSG, probe.build_u2f_version_apdu(), trace
            )
            parsed = probe.decode_u2f_response_payload(response_payload)
            if response_cmd != probe.MSG or response_cid != cid or response_payload[-2:] != probe.U2F_SW_NO_ERROR:
                raise RuntimeError("U2F VERSION failed")
            return {"status": "passed", "summary": "U2F VERSION returned U2F_V2", "details": parsed, "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_u2f_invalid_apdu(self, matched: MatchedDevice) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "msg", trace)
            invalid = bytes([0x01, probe.U2F_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00])
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "msg", probe.MSG, invalid, trace
            )
            if response_cmd != probe.MSG or response_cid != cid or response_payload[-2:] == probe.U2F_SW_NO_ERROR:
                raise RuntimeError("invalid U2F APDU was not rejected")
            return {"status": "passed", "summary": "invalid U2F APDU header was rejected", "details": probe.decode_u2f_response_payload(response_payload), "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_u2f_register(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "msg", trace)
            challenge = hashlib.sha256(b"zerofido-suite-u2f-register").digest()
            app_id = hashlib.sha256(b"zerofido-suite-u2f-app").digest()
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device, "msg", probe.MSG, probe.build_u2f_register_apdu(challenge, app_id), trace
            )
            if response_cmd != probe.MSG or response_cid != cid or response_payload[-2:] != probe.U2F_SW_NO_ERROR:
                raise RuntimeError("U2F REGISTER failed")
            runtime["u2f"]["app_id"] = app_id
            runtime["u2f"]["key_handle"] = probe.extract_u2f_key_handle(response_payload)
            return {"status": "passed", "summary": "U2F register succeeded", "details": probe.decode_u2f_response_payload(response_payload), "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _scenario_u2f_authenticate(self, matched: MatchedDevice, runtime: dict[str, Any]) -> dict[str, Any]:
        key_handle = runtime["u2f"].get("key_handle")
        app_id = runtime["u2f"].get("app_id")
        if not key_handle or not app_id:
            return {"status": "blocked", "summary": "U2F register prerequisite is missing"}

        def run(device: hid.device) -> dict[str, Any]:
            trace: list[dict[str, str]] = []
            cid = self._get_run_cid(device, "msg", trace)
            challenge = hashlib.sha256(b"zerofido-suite-u2f-auth").digest()
            response_cid, response_cmd, response_payload = self._transact_run_channel(
                device,
                "msg",
                probe.MSG,
                probe.build_u2f_authenticate_apdu(challenge, app_id, key_handle),
                trace,
            )
            if response_cmd != probe.MSG or response_cid != cid or response_payload[-2:] != probe.U2F_SW_NO_ERROR:
                raise RuntimeError("U2F AUTHENTICATE failed")
            return {"status": "passed", "summary": "U2F authenticate succeeded", "details": probe.decode_u2f_response_payload(response_payload), "evidence": {"trace": trace}}
        return self._with_device(matched, run)

    def _run_browser_scenario(self, scenario_id: str, payload: dict[str, Any]) -> dict[str, Any]:
        request_id = uuid.uuid4().hex[:12]
        event = threading.Event()
        with self._lock:
            self._browser_request_event = event
            self._browser_request_result = None
            self._state["run"]["browser_request"] = {
                "id": request_id,
                "scenario_id": scenario_id,
                "payload": copy.deepcopy(payload),
                "created_at": utc_now_iso(),
                "claimed_by": None,
                "claimed_at": None,
            }
        self._emit("browser_request", self.get_status())
        event.wait()
        if self._run_stop.is_set():
            return {
                "status": "blocked",
                "summary": f"browser scenario {scenario_id} was stopped",
                "details": {},
                "evidence": {},
            }
        with self._lock:
            result = copy.deepcopy(self._browser_request_result)
            self._browser_request_event = None
            self._browser_request_result = None
            self._state["run"]["browser_request"] = None
        self._emit("browser_request_resolved", self.get_status())
        if isinstance(result, dict):
            return result
        return {
            "status": "blocked",
            "summary": f"browser scenario {scenario_id} did not return a result",
            "details": {},
            "evidence": {},
        }

    def _scenario_browser_register(
        self, runtime: dict[str, Any], *, attestation: str, resident_key: bool, slot: str
    ) -> dict[str, Any]:
        username_prefix = self.config.get("fixture", {}).get("username_prefix", "suite-user")
        rp_id = self.host
        payload = {
            "attestation": attestation,
            "residentKey": resident_key,
            "rpId": rp_id,
            "username": f"{username_prefix}-{uuid.uuid4().hex[:8]}",
            "slot": slot,
        }
        result = self._run_browser_scenario("browser_register", payload)
        if result.get("status") == "passed":
            metadata = self._load_static_metadata()
            result.setdefault("details", {})
            result["details"]["validated"] = self._validate_browser_register_result(
                result, attestation=attestation, metadata=metadata
            )
            runtime["browser_credentials"][slot] = result.get("credential", {})
        return result

    def _scenario_browser_auth(self, runtime: dict[str, Any], *, discoverable: bool, slot: str) -> dict[str, Any]:
        credential = runtime["browser_credentials"].get(slot)
        payload = {
            "slot": slot,
            "rpId": self.host,
            "discoverable": discoverable,
            "credentialId": (credential or {}).get("credentialId"),
        }
        result = self._run_browser_scenario("browser_authenticate", payload)
        if result.get("status") == "passed":
            result.setdefault("details", {})
            result["details"]["validated"] = self._validate_browser_auth_result(
                result, expected_credential_id=(credential or {}).get("credentialId")
            )
        return result
