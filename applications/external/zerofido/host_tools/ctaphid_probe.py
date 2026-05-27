"""CTAPHID/FIDO probe helpers and CLI.

This module is both an executable diagnostic tool and a small reusable library
for conformance scenarios: it owns HID discovery, CTAPHID channel allocation,
packet fragmentation/reassembly, CTAP2/U2F/ClientPIN request builders, and
response decoders used by the host-side suite.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
import os
import socket
import statistics
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes, hmac, padding
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

import cbor2
import hid


FIDO_USAGE_PAGE = 0xF1D0
FIDO_USAGE = 0x01
FLIPPER_VENDOR_ID = 0x0483
FLIPPER_PRODUCT_ID = 0x5741

BROADCAST_CID = 0xFFFFFFFF
PACKET_SIZE = 64
INIT = 0x86
PING = 0x81
MSG = 0x83
CBOR = 0x90
CANCEL = 0x91
KEEPALIVE = 0xBB
ERROR = 0xBF
WINK = 0x88

CTAP_GET_INFO = 0x04
CTAP_MAKE_CREDENTIAL = 0x01
CTAP_GET_ASSERTION = 0x02
CTAP_GET_NEXT_ASSERTION = 0x08
CTAP_CLIENT_PIN = 0x06

CLIENT_PIN_GET_RETRIES = 0x01
CLIENT_PIN_GET_KEY_AGREEMENT = 0x02
CLIENT_PIN_SET_PIN = 0x03
CLIENT_PIN_CHANGE_PIN = 0x04
CLIENT_PIN_GET_PIN_TOKEN = 0x05

U2F_REGISTER = 0x01
U2F_AUTHENTICATE = 0x02
U2F_VERSION = 0x03
U2F_AUTH_CHECK_ONLY = 0x07
U2F_AUTH_ENFORCE = 0x03
U2F_AUTH_DONT_ENFORCE = 0x08
U2F_SW_NO_ERROR = bytes.fromhex("9000")
U2F_SW_CONDITIONS_NOT_SATISFIED = bytes.fromhex("6985")
U2F_SW_WRONG_LENGTH = bytes.fromhex("6700")
U2F_SW_WRONG_DATA = bytes.fromhex("6a80")
U2F_SW_INS_NOT_SUPPORTED = bytes.fromhex("6d00")
U2F_SW_CLA_NOT_SUPPORTED = bytes.fromhex("6e00")


@dataclass
class HidDeviceInfo:
    path: bytes
    vendor_id: int
    product_id: int
    usage_page: int
    usage: int
    product_string: str | None
    serial_number: str | None


def trace_packet(trace: list[dict[str, str]] | None, direction: str, packet: bytes) -> None:
    if trace is None:
        return
    trace.append({"direction": direction, "packet_hex": packet.hex()})


def build_frames(cid: int, cmd: int, payload: bytes) -> list[bytes]:
    frames: list[bytes] = []
    init = bytearray(PACKET_SIZE)
    struct.pack_into("<I", init, 0, cid)
    init[4] = cmd
    init[5] = (len(payload) >> 8) & 0xFF
    init[6] = len(payload) & 0xFF
    init[7 : 7 + min(len(payload), 57)] = payload[:57]
    frames.append(bytes(init))

    offset = 57
    seq = 0
    while offset < len(payload):
        cont = bytearray(PACKET_SIZE)
        struct.pack_into("<I", cont, 0, cid)
        cont[4] = seq
        chunk = payload[offset : offset + 59]
        cont[5 : 5 + len(chunk)] = chunk
        frames.append(bytes(cont))
        seq += 1
        offset += len(chunk)
    return frames


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Probe a FIDO HID device over CTAPHID")
    parser.add_argument(
        "--cmd",
        choices=[
            "list",
            "init",
            "ping",
            "getinfo",
            "makecredential",
            "getassertion",
            "getnextassertion",
            "cancel",
            "resync",
            "invalidcid",
            "exhaustcids",
            "shortinit",
            "bench",
            "u2fversion",
            "u2fregister",
            "u2fauthinvalid",
            "u2finvalidcla",
            "u2fversiondata",
        ],
        default="getinfo",
        help="Command to send to the first matching FIDO HID device",
    )
    parser.add_argument(
        "--path",
        help="Explicit hidapi path to open instead of auto-selecting the first matching device",
    )
    parser.add_argument(
        "--timeout-ms",
        type=int,
        default=3000,
        help="Read timeout for each packet",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print TX/RX packets as they are exchanged",
    )
    parser.add_argument(
        "--rp-id",
        default="zerofido.local",
        help="RP ID for makecredential/getassertion requests",
    )
    parser.add_argument(
        "--credential-id",
        help="Base64url credential ID or comma-separated IDs for getassertion allowCredentials",
    )
    parser.add_argument(
        "--allow-list-count",
        type=int,
        default=0,
        help="Force an allowCredentials array of this size; extra synthetic IDs are generated as needed",
    )
    parser.add_argument(
        "--user-verification",
        choices=["discouraged", "required"],
        default="discouraged",
        help="Requested user verification policy for getassertion probes",
    )
    parser.add_argument(
        "--cancel-target",
        choices=["makecredential", "getassertion"],
        default="makecredential",
        help="Approval-requiring CTAP command to start before injecting CTAPHID_CANCEL",
    )
    parser.add_argument(
        "--empty-pin-auth",
        action="store_true",
        help="Include an empty pinAuth byte string in MakeCredential/GetAssertion",
    )
    parser.add_argument(
        "--omit-client-data-hash",
        action="store_true",
        help="Omit the required clientDataHash field from MakeCredential/GetAssertion",
    )
    parser.add_argument(
        "--silent",
        action="store_true",
        help="Request GetAssertion with options.up=false",
    )
    parser.add_argument(
        "--include-rk-option",
        action="store_true",
        help="Include the unsupported options.rk field in GetAssertion",
    )
    parser.add_argument(
        "--append-trailing-hex",
        default="",
        help="Append raw trailing hex bytes after the top-level CBOR item",
    )
    parser.add_argument(
        "--cid-count",
        type=int,
        default=9,
        help="Number of broadcast INIT allocations to attempt for exhaustcids",
    )
    parser.add_argument(
        "--short-init-len",
        type=int,
        default=6,
        help="Short CTAPHID INIT frame length to send in shortinit mode (5 or 6)",
    )
    parser.add_argument(
        "--iterations",
        type=int,
        default=10,
        help="Number of timing samples to collect in bench mode",
    )
    parser.add_argument(
        "--u2f-cert-out",
        help="Write the attestation certificate from a u2fregister response to this DER file",
    )
    parser.add_argument(
        "--fido2-cert-out",
        help="Write the packed attestation certificate from a makecredential response to this DER file",
    )
    parser.add_argument(
        "--u2f-invalid-cla",
        type=lambda value: int(value, 0),
        default=0x6D,
        help="CLA byte for u2finvalidcla; defaults to the FIDO conformance F-2 value 0x6d",
    )
    return parser.parse_args()


def discover_devices() -> list[HidDeviceInfo]:
    devices: list[HidDeviceInfo] = []
    for raw in hid.enumerate():
        usage_page = raw.get("usage_page") or 0
        usage = raw.get("usage") or 0
        vendor_id = raw.get("vendor_id") or 0
        product_id = raw.get("product_id") or 0
        product_string = raw.get("product_string")
        path = raw.get("path")
        if not path:
            continue
        if usage_page == FIDO_USAGE_PAGE and usage == FIDO_USAGE:
            devices.append(
                HidDeviceInfo(
                    path=path,
                    vendor_id=vendor_id,
                    product_id=product_id,
                    usage_page=usage_page,
                    usage=usage,
                    product_string=product_string,
                    serial_number=raw.get("serial_number"),
                )
            )
            continue
        if vendor_id == FLIPPER_VENDOR_ID and product_id == FLIPPER_PRODUCT_ID:
            devices.append(
                HidDeviceInfo(
                    path=path,
                    vendor_id=vendor_id,
                    product_id=product_id,
                    usage_page=usage_page,
                    usage=usage,
                    product_string=product_string,
                    serial_number=raw.get("serial_number"),
                )
            )
    return devices


def print_devices(devices: list[HidDeviceInfo]) -> int:
    if not devices:
        print("no matching FIDO HID devices found")
        return 1

    for index, device in enumerate(devices):
        print(
            f"[{index}] path={device.path!r} vid=0x{device.vendor_id:04x} pid=0x{device.product_id:04x} "
            f"usage_page=0x{device.usage_page:04x} usage=0x{device.usage:02x} "
            f"product={device.product_string!r} serial={device.serial_number!r}"
        )
    return 0


def normalize_rx_packet(packet: bytes) -> bytes:
    if len(packet) == PACKET_SIZE:
        return packet
    if len(packet) == PACKET_SIZE + 1 and packet[0] == 0:
        return packet[1:]
    raise ValueError(f"unexpected packet length {len(packet)}")


def write_frame(
    device: hid.device, frame: bytes, verbose: bool, trace: list[dict[str, str]] | None = None
) -> None:
    report = b"\x00" + frame
    written = device.write(report)
    if written != len(report):
        raise RuntimeError(f"short write: wrote {written} bytes, expected {len(report)}")
    trace_packet(trace, "tx", frame)
    if verbose:
        print(f"tx {frame.hex()}")


def read_frame(
    device: hid.device,
    timeout_ms: int,
    verbose: bool,
    trace: list[dict[str, str]] | None = None,
) -> bytes:
    packet = bytes(device.read(PACKET_SIZE + 1, timeout_ms))
    if not packet:
        raise TimeoutError("timed out waiting for HID response")
    packet = normalize_rx_packet(packet)
    trace_packet(trace, "rx", packet)
    if verbose:
        print(f"rx {packet.hex()}")
    return packet


def transact(
    device: hid.device,
    cid: int,
    cmd: int,
    payload: bytes,
    timeout_ms: int,
    verbose: bool,
    trace: list[dict[str, str]] | None = None,
    timing: dict[str, Any] | None = None,
) -> tuple[int, int, bytes]:
    frames = build_frames(cid, cmd, payload)
    total_start = time.perf_counter()
    write_elapsed_ms = 0.0
    read_elapsed_ms = 0.0
    for frame in frames:
        write_start = time.perf_counter()
        write_frame(device, frame, verbose, trace)
        write_elapsed_ms += (time.perf_counter() - write_start) * 1000.0

    read_start = time.perf_counter()
    first = read_ctaphid_response_frame(device, timeout_ms, verbose, trace)
    read_elapsed_ms += (time.perf_counter() - read_start) * 1000.0
    response_cid = struct.unpack_from("<I", first, 0)[0]
    response_cmd = first[4]
    response_len = (first[5] << 8) | first[6]
    response = bytearray(first[7 : 7 + min(response_len, 57)])

    seq = 0
    while len(response) < response_len:
        read_start = time.perf_counter()
        packet = read_ctaphid_response_frame(device, timeout_ms, verbose, trace)
        read_elapsed_ms += (time.perf_counter() - read_start) * 1000.0
        packet_cid = struct.unpack_from("<I", packet, 0)[0]
        packet_seq = packet[4]
        if packet_cid != response_cid:
            raise RuntimeError(
                f"unexpected continuation cid 0x{packet_cid:08x}, expected 0x{response_cid:08x}"
            )
        if packet_seq != seq:
            raise RuntimeError(f"unexpected continuation seq {packet_seq}, expected {seq}")
        response.extend(packet[5 : 5 + min(response_len - len(response), 59)])
        seq += 1

    if timing is not None:
        timing.update(
            {
                "cid": f"0x{cid:08x}",
                "command_hex": f"0x{cmd:02x}",
                "tx_frames": len(frames),
                "rx_frames": seq + 1,
                "write_ms": round(write_elapsed_ms, 3),
                "read_ms": round(read_elapsed_ms, 3),
                "total_ms": round((time.perf_counter() - total_start) * 1000.0, 3),
            }
        )

    return response_cid, response_cmd, bytes(response)


def read_response(
    device: hid.device,
    timeout_ms: int,
    verbose: bool,
    trace: list[dict[str, str]] | None = None,
) -> tuple[int, int, bytes]:
    first = read_ctaphid_response_frame(device, timeout_ms, verbose, trace)
    response_cid = struct.unpack_from("<I", first, 0)[0]
    response_cmd = first[4]
    response_len = (first[5] << 8) | first[6]
    response = bytearray(first[7 : 7 + min(response_len, 57)])

    seq = 0
    while len(response) < response_len:
        packet = read_ctaphid_response_frame(device, timeout_ms, verbose, trace)
        packet_cid = struct.unpack_from("<I", packet, 0)[0]
        packet_seq = packet[4]
        if packet_cid != response_cid:
            raise RuntimeError(
                f"unexpected continuation cid 0x{packet_cid:08x}, expected 0x{response_cid:08x}"
            )
        if packet_seq != seq:
            raise RuntimeError(f"unexpected continuation seq {packet_seq}, expected {seq}")
        response.extend(packet[5 : 5 + min(response_len - len(response), 59)])
        seq += 1

    return response_cid, response_cmd, bytes(response)


def read_ctaphid_response_frame(
    device: hid.device,
    timeout_ms: int,
    verbose: bool,
    trace: list[dict[str, str]] | None = None,
) -> bytes:
    while True:
        packet = read_frame(device, timeout_ms, verbose, trace)
        if packet[4] != KEEPALIVE:
            return packet


def expect_init_response(
    cid: int,
    cmd: int,
    payload: bytes,
    nonce: bytes,
    *,
    expected_response_cid: int = BROADCAST_CID,
) -> int:
    if cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during INIT: 0x{payload[0]:02x}")
    if cmd != INIT:
        raise RuntimeError(f"unexpected INIT response cmd 0x{cmd:02x}")
    if cid != expected_response_cid:
        raise RuntimeError(f"unexpected INIT response cid 0x{cid:08x}")
    if len(payload) < 17:
        raise RuntimeError(f"unexpected INIT response length {len(payload)}")
    if payload[:8] != nonce:
        raise RuntimeError("INIT nonce mismatch")
    allocated_cid = struct.unpack_from("<I", payload, 8)[0]
    return allocated_cid


def run_init(device: hid.device, timeout_ms: int, verbose: bool) -> int:
    nonce = os.urandom(8)
    timing: dict[str, Any] = {}
    response_cid, response_cmd, response_payload = transact(
        device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose, timing=timing
    )
    allocated_cid = expect_init_response(response_cid, response_cmd, response_payload, nonce)
    print(f"allocated_cid=0x{allocated_cid:08x}")
    print(f"init_payload={response_payload.hex()}")
    print(f"timing={json.dumps(timing, sort_keys=True)}")
    return 0


def run_ping(device: hid.device, timeout_ms: int, verbose: bool) -> int:
    nonce = os.urandom(8)
    _, _, init_payload = transact(device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose)
    cid = expect_init_response(BROADCAST_CID, INIT, init_payload, nonce)
    payload = b"zerofido"
    response_cid, response_cmd, response_payload = transact(device, cid, PING, payload, timeout_ms, verbose)
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during PING: 0x{response_payload[0]:02x}")
    if response_cmd != PING:
        raise RuntimeError(f"unexpected PING response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected PING response cid 0x{response_cid:08x}")
    print(f"ping_payload={response_payload!r}")
    return 0


def run_get_info(device: hid.device, timeout_ms: int, verbose: bool) -> int:
    nonce = os.urandom(8)
    _, _, init_payload = transact(device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose)
    cid = expect_init_response(BROADCAST_CID, INIT, init_payload, nonce)
    response_cid, response_cmd, response_payload = transact(
        device, cid, CBOR, bytes([CTAP_GET_INFO]), timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during GetInfo transport: 0x{response_payload[0]:02x}")
    if response_cmd != CBOR:
        raise RuntimeError(f"unexpected CBOR response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected CBOR response cid 0x{response_cid:08x}")
    if not response_payload:
        raise RuntimeError("empty GetInfo response")

    print_ctap_response(response_payload)
    return 0


def print_ctap_response(response_payload: bytes) -> None:
    print(f"ctap_status=0x{response_payload[0]:02x}")
    print(f"cbor_body={response_payload[1:].hex()}")
    if len(response_payload) == 1:
        return

    try:
        decoded = cbor2.loads(response_payload[1:])
    except Exception as exc:
        print(f"cbor_decode_error={exc}")
        return

    print(f"cbor_decoded={decoded!r}")


def b64url_decode(value: str) -> bytes:
    padding = "=" * ((4 - (len(value) % 4)) % 4)
    return base64.urlsafe_b64decode(value + padding)


def parse_credential_id_values(value: str | None) -> list[bytes]:
    if not value:
        return []

    return [b64url_decode(item.strip()) for item in value.split(",") if item.strip()]


def parse_trailing_bytes(value: str) -> bytes:
    compact = "".join(value.split())
    if not compact:
        return b""
    return bytes.fromhex(compact)


def build_allow_list(credential_ids: list[bytes], allow_list_count: int) -> list[dict[str, object]]:
    entries = [{"type": "public-key", "id": credential_id} for credential_id in credential_ids]
    target_count = max(len(entries), allow_list_count)
    for index in range(len(entries), target_count):
        synthetic_id = hashlib.sha256(f"zerofido-allow-list-{index}".encode()).digest()[:16]
        entries.append({"type": "public-key", "id": synthetic_id})
    return entries


def build_make_credential_request(
    rp_id: str, *, empty_pin_auth: bool, omit_client_data_hash: bool, trailing_bytes: bytes = b""
) -> bytes:
    request: dict[object, object] = {
        2: {
            "id": rp_id,
            "name": "ZeroFIDO Debug",
        },
        3: {
            "id": b"debug-user-01",
            "name": "debugger",
            "displayName": "ZeroFIDO Debug User",
        },
        4: [
            {
                "type": "public-key",
                "alg": -7,
            }
        ],
        7: {
            "rk": True,
            "uv": False,
        },
    }
    if not omit_client_data_hash:
        request[1] = hashlib.sha256(b"zerofido-makecredential-debug").digest()
    if empty_pin_auth:
        request[8] = b""
        request[9] = 1
    return cbor2.dumps(request) + trailing_bytes


def build_get_assertion_request(
    rp_id: str,
    credential_ids: list[bytes],
    user_verification: str,
    *,
    empty_pin_auth: bool,
    omit_client_data_hash: bool,
    silent: bool,
    include_rk_option: bool,
    allow_list_count: int = 0,
    trailing_bytes: bytes = b"",
) -> bytes:
    request: dict[object, object] = {
        1: rp_id,
        5: {
            "up": not silent,
            "uv": user_verification == "required",
        },
    }
    if not omit_client_data_hash:
        request[2] = hashlib.sha256(b"zerofido-getassertion-debug").digest()

    allow_list = build_allow_list(credential_ids, allow_list_count)
    if allow_list:
        request[3] = allow_list
    if empty_pin_auth:
        request[6] = b""
        request[7] = 1
    if include_rk_option:
        options = dict(request[5])
        options["rk"] = True
        request[5] = options

    return cbor2.dumps(request) + trailing_bytes


def decode_single_cbor_item(payload: bytes) -> tuple[object, bytes]:
    stream = io.BytesIO(payload)
    decoded = cbor2.load(stream)
    return decoded, stream.read()


def allocate_cid(
    device: hid.device,
    timeout_ms: int,
    verbose: bool,
    timing: dict[str, Any] | None = None,
) -> int:
    nonce = os.urandom(8)
    init_timing: dict[str, Any] = {}
    response_cid, response_cmd, response_payload = transact(
        device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose, timing=init_timing
    )
    allocated_cid = expect_init_response(response_cid, response_cmd, response_payload, nonce)
    if timing is not None:
        timing.update(init_timing)
        timing["allocated_cid"] = f"0x{allocated_cid:08x}"
    return allocated_cid


def summarize_samples(samples: list[float]) -> dict[str, Any]:
    return {
        "iterations": len(samples),
        "samples_ms": [round(sample, 3) for sample in samples],
        "min_ms": round(min(samples), 3),
        "median_ms": round(statistics.median(samples), 3),
        "max_ms": round(max(samples), 3),
    }


def run_bench(device: hid.device, timeout_ms: int, verbose: bool, iterations: int) -> int:
    init_samples: list[float] = []
    ping_samples: list[float] = []
    get_info_samples: list[float] = []
    reused_cid = allocate_cid(device, timeout_ms, verbose)

    for _ in range(iterations):
        init_timing: dict[str, Any] = {}
        allocate_cid(device, timeout_ms, verbose, timing=init_timing)
        init_samples.append(float(init_timing["total_ms"]))

        ping_timing: dict[str, Any] = {}
        response_cid, response_cmd, response_payload = transact(
            device, reused_cid, PING, b"zerofido-bench", timeout_ms, verbose, timing=ping_timing
        )
        if response_cmd != PING or response_cid != reused_cid or response_payload != b"zerofido-bench":
            raise RuntimeError("unexpected PING response during bench run")
        ping_samples.append(float(ping_timing["total_ms"]))

        get_info_timing: dict[str, Any] = {}
        response_cid, response_cmd, response_payload = transact(
            device, reused_cid, CBOR, bytes([CTAP_GET_INFO]), timeout_ms, verbose, timing=get_info_timing
        )
        if response_cmd != CBOR or response_cid != reused_cid or not response_payload:
            raise RuntimeError("unexpected GetInfo response during bench run")
        get_info_samples.append(float(get_info_timing["total_ms"]))

    print(
        json.dumps(
            {
                "broadcast_init": summarize_samples(init_samples),
                "reused_ping": summarize_samples(ping_samples),
                "reused_cbor_get_info": summarize_samples(get_info_samples),
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


def write_raw_packet(
    device: hid.device, packet: bytes, verbose: bool, trace: list[dict[str, str]] | None = None
) -> None:
    report = b"\x00" + packet
    written = device.write(report)
    if written != len(report):
        raise RuntimeError(f"short raw write: wrote {written} bytes, expected {len(report)}")
    trace_packet(trace, "tx", packet)
    if verbose:
        print(f"tx-raw {packet.hex()}")


def run_make_credential(device: hid.device, timeout_ms: int, verbose: bool, args: argparse.Namespace) -> int:
    cid = allocate_cid(device, timeout_ms, verbose)
    trailing_bytes = parse_trailing_bytes(args.append_trailing_hex)
    request = bytes([CTAP_MAKE_CREDENTIAL]) + build_make_credential_request(
        args.rp_id,
        empty_pin_auth=args.empty_pin_auth,
        omit_client_data_hash=args.omit_client_data_hash,
        trailing_bytes=trailing_bytes,
        resident_key=not args.fido2_cert_out,
        attestation_formats=["packed"] if args.fido2_cert_out else None,
    )
    response_cid, response_cmd, response_payload = transact(
        device, cid, CBOR, request, timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(
            f"CTAPHID error during MakeCredential transport: 0x{response_payload[0]:02x}"
        )
    if response_cmd != CBOR:
        raise RuntimeError(f"unexpected CBOR response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected CBOR response cid 0x{response_cid:08x}")
    if not response_payload:
        raise RuntimeError("empty MakeCredential response")

    if args.fido2_cert_out:
        cert_der = extract_make_credential_attestation_certificate(response_payload)
        cert_out = Path(args.fido2_cert_out)
        cert_out.parent.mkdir(parents=True, exist_ok=True)
        cert_out.write_bytes(cert_der)
        print(f"attestation_certificate_path={cert_out}")
        print(f"attestation_certificate_len={len(cert_der)}")
    print_ctap_response(response_payload)
    return 0


def run_get_assertion(
    device: hid.device,
    timeout_ms: int,
    verbose: bool,
    args: argparse.Namespace,
    credential_ids: list[bytes],
) -> int:
    cid = allocate_cid(device, timeout_ms, verbose)
    trailing_bytes = parse_trailing_bytes(args.append_trailing_hex)
    request = bytes([CTAP_GET_ASSERTION]) + build_get_assertion_request(
        args.rp_id,
        credential_ids,
        args.user_verification,
        empty_pin_auth=args.empty_pin_auth,
        omit_client_data_hash=args.omit_client_data_hash,
        silent=args.silent,
        include_rk_option=args.include_rk_option,
        allow_list_count=args.allow_list_count,
        trailing_bytes=trailing_bytes,
    )
    response_cid, response_cmd, response_payload = transact(
        device, cid, CBOR, request, timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(
            f"CTAPHID error during GetAssertion transport: 0x{response_payload[0]:02x}"
        )
    if response_cmd != CBOR:
        raise RuntimeError(f"unexpected CBOR response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected CBOR response cid 0x{response_cid:08x}")
    if not response_payload:
        raise RuntimeError("empty GetAssertion response")

    print_ctap_response(response_payload)
    return 0


def run_get_next_assertion(
    device: hid.device, timeout_ms: int, verbose: bool, args: argparse.Namespace, credential_ids: list[bytes]
) -> int:
    cid = allocate_cid(device, timeout_ms, verbose)
    trailing_bytes = parse_trailing_bytes(args.append_trailing_hex)

    first_request = bytes([CTAP_GET_ASSERTION]) + build_get_assertion_request(
        args.rp_id,
        credential_ids,
        args.user_verification,
        empty_pin_auth=args.empty_pin_auth,
        omit_client_data_hash=args.omit_client_data_hash,
        silent=args.silent,
        include_rk_option=args.include_rk_option,
        allow_list_count=args.allow_list_count,
        trailing_bytes=trailing_bytes,
    )
    first_cid, first_cmd, first_payload = transact(device, cid, CBOR, first_request, timeout_ms, verbose)
    if first_cmd == ERROR:
        raise RuntimeError(
            f"CTAPHID error during initial GetAssertion transport: 0x{first_payload[0]:02x}"
        )
    print("initial_get_assertion:")
    print_ctap_response(first_payload)

    next_cid, next_cmd, next_payload = transact(
        device, cid, CBOR, bytes([CTAP_GET_NEXT_ASSERTION]), timeout_ms, verbose
    )
    if next_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during GetNextAssertion transport: 0x{next_payload[0]:02x}")
    if next_cid != first_cid:
        raise RuntimeError(f"unexpected GetNextAssertion cid 0x{next_cid:08x}")

    print("next_assertion:")
    print_ctap_response(next_payload)
    return 0


def send_ctaphid_cancel(device: hid.device, cid: int, verbose: bool) -> None:
    frame = bytearray(PACKET_SIZE)
    struct.pack_into("<I", frame, 0, cid)
    frame[4] = CANCEL
    write_frame(device, bytes(frame), verbose)


def run_cancel(
    device: hid.device,
    timeout_ms: int,
    verbose: bool,
    rp_id: str,
    credential_ids: list[bytes],
    user_verification: str,
    cancel_target: str,
) -> int:
    cid = allocate_cid(device, timeout_ms, verbose)

    if cancel_target == "makecredential":
        request = bytes([CTAP_MAKE_CREDENTIAL]) + build_make_credential_request(
            rp_id, empty_pin_auth=False, omit_client_data_hash=False
        )
    else:
        request = bytes([CTAP_GET_ASSERTION]) + build_get_assertion_request(
            rp_id,
            credential_ids,
            user_verification,
            empty_pin_auth=False,
            omit_client_data_hash=False,
            silent=False,
            include_rk_option=False,
        )

    for frame in build_frames(cid, CBOR, request):
        write_frame(device, frame, verbose)

    keepalive_statuses: list[int] = []
    while True:
        packet = read_frame(device, timeout_ms, verbose)
        packet_cid = struct.unpack_from("<I", packet, 0)[0]
        if packet_cid != cid:
            raise RuntimeError(f"unexpected packet cid 0x{packet_cid:08x}, expected 0x{cid:08x}")
        if packet[4] != KEEPALIVE:
            raise RuntimeError("expected KEEPALIVE before sending CTAPHID_CANCEL")

        keepalive_statuses.append(packet[7])
        break

    send_ctaphid_cancel(device, cid, verbose)
    response_cid, response_cmd, response_payload = read_response(device, timeout_ms, verbose)
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error after CANCEL: 0x{response_payload[0]:02x}")
    if response_cmd != CBOR:
        raise RuntimeError(f"unexpected response cmd 0x{response_cmd:02x} after CANCEL")
    if response_cid != cid:
        raise RuntimeError(f"unexpected response cid 0x{response_cid:08x} after CANCEL")
    if not response_payload:
        raise RuntimeError("empty CBOR response after CANCEL")
    if response_payload[0] != 0x2D:
        raise RuntimeError(
            f"expected CTAP2_ERR_KEEPALIVE_CANCEL (0x2d), got 0x{response_payload[0]:02x}"
        )

    print(f"cancel_target={cancel_target}")
    print(f"keepalive_statuses={keepalive_statuses}")
    print_ctap_response(response_payload)
    return 0


def run_resync(
    device: hid.device,
    timeout_ms: int,
    verbose: bool,
    args: argparse.Namespace,
    credential_ids: list[bytes],
) -> int:
    cid = allocate_cid(device, timeout_ms, verbose)

    if args.cancel_target == "makecredential":
        request = bytes([CTAP_MAKE_CREDENTIAL]) + build_make_credential_request(
            args.rp_id, empty_pin_auth=False, omit_client_data_hash=False
        )
    else:
        request = bytes([CTAP_GET_ASSERTION]) + build_get_assertion_request(
            args.rp_id,
            credential_ids,
            args.user_verification,
            empty_pin_auth=False,
            omit_client_data_hash=False,
            silent=False,
            include_rk_option=False,
        )

    for frame in build_frames(cid, CBOR, request):
        write_frame(device, frame, verbose)

    while True:
        packet = read_frame(device, timeout_ms, verbose)
        packet_cid = struct.unpack_from("<I", packet, 0)[0]
        if packet_cid != cid:
            raise RuntimeError(f"unexpected packet cid 0x{packet_cid:08x}, expected 0x{cid:08x}")
        if packet[4] == KEEPALIVE:
            break

    resync_nonce = os.urandom(8)
    response_cid, response_cmd, response_payload = transact(
        device, cid, INIT, resync_nonce, timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during same-CID INIT resync: 0x{response_payload[0]:02x}")
    assigned_cid = expect_init_response(
        response_cid,
        response_cmd,
        response_payload,
        resync_nonce,
        expected_response_cid=cid,
    )
    ping_payload = b"resync-ok"
    ping_cid, ping_cmd, ping_response = transact(device, cid, PING, ping_payload, timeout_ms, verbose)
    if ping_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error after same-CID INIT resync: 0x{ping_response[0]:02x}")
    if ping_cmd != PING or ping_cid != cid or ping_response != ping_payload:
        raise RuntimeError("same-CID INIT resync did not leave the channel ready for a new request")
    print(f"resync_target={args.cancel_target}")
    print(f"resync_cid=0x{assigned_cid:08x}")
    print(f"resync_payload={response_payload.hex()}")
    print(f"post_resync_ping={ping_response.hex()}")
    return 0


def run_invalid_cid(device: hid.device, timeout_ms: int, verbose: bool) -> int:
    invalid_cid = 0x01020304
    if invalid_cid in (0, BROADCAST_CID):
        raise RuntimeError("chosen invalid CID is reserved")

    response_cid, response_cmd, response_payload = transact(
        device, invalid_cid, CBOR, bytes([CTAP_GET_INFO]), timeout_ms, verbose
    )
    if response_cmd != ERROR:
        raise RuntimeError(f"expected CTAPHID error, got cmd 0x{response_cmd:02x}")
    if response_cid != invalid_cid:
        raise RuntimeError(f"unexpected error response cid 0x{response_cid:08x}")
    print(f"invalid_cid=0x{invalid_cid:08x}")
    print(f"hid_error=0x{response_payload[0]:02x}")
    return 0


def run_exhaust_cids(device: hid.device, timeout_ms: int, verbose: bool, count: int) -> int:
    allocated: list[int] = []
    failure_cmd: int | None = None
    failure_payload = b""

    for _ in range(count):
        nonce = os.urandom(8)
        response_cid, response_cmd, response_payload = transact(
            device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose
        )
        if response_cmd == ERROR:
            failure_cmd = response_cmd
            failure_payload = response_payload
            break

        allocated.append(expect_init_response(response_cid, response_cmd, response_payload, nonce))

    print(f"allocated_cids={[f'0x{cid:08x}' for cid in allocated]}")
    if failure_cmd is None:
        print("allocation_failure=none")
        return 0

    print(f"allocation_failure_cmd=0x{failure_cmd:02x}")
    print(f"allocation_failure_payload={failure_payload.hex()}")
    if not allocated:
        raise RuntimeError("CID allocation failed before any channel was established")

    payload = b"cid-still-live"
    response_cid, response_cmd, response_payload = transact(
        device, allocated[0], PING, payload, timeout_ms, verbose
    )
    if response_cmd != PING or response_cid != allocated[0] or response_payload != payload:
        raise RuntimeError("previously allocated CID stopped working after allocator failure")

    print(f"post_failure_ping_cid=0x{response_cid:08x}")
    print(f"post_failure_ping_payload={response_payload!r}")
    return 0


def run_short_init(device: hid.device, timeout_ms: int, verbose: bool, short_init_len: int) -> int:
    if short_init_len not in (5, 6):
        raise RuntimeError("--short-init-len must be 5 or 6")

    active_cid = allocate_cid(device, timeout_ms, verbose)
    other_cid = allocate_cid(device, timeout_ms, verbose)
    ping_payload = bytes(range(80))
    ping_frames = build_frames(active_cid, PING, ping_payload)
    write_frame(device, ping_frames[0], verbose)

    short_packet = bytearray(struct.pack("<I", other_cid))
    short_packet.append(INIT)
    if short_init_len == 6:
        short_packet.append(0)
    write_raw_packet(device, bytes(short_packet), verbose)

    error_cid, error_cmd, error_payload = read_response(device, timeout_ms, verbose)
    if error_cmd != ERROR:
        raise RuntimeError(f"expected CTAPHID error after short init, got cmd 0x{error_cmd:02x}")
    if error_cid != other_cid:
        raise RuntimeError(f"unexpected short-init error cid 0x{error_cid:08x}")

    for frame in ping_frames[1:]:
        write_frame(device, frame, verbose)

    response_cid, response_cmd, response_payload = read_response(device, timeout_ms, verbose)
    if response_cmd != PING or response_cid != active_cid or response_payload != ping_payload:
        raise RuntimeError("short init disturbed the in-flight transaction")

    print(f"short_init_error=0x{error_payload[0]:02x}")
    print(f"surviving_ping_cid=0x{response_cid:08x}")
    print(f"surviving_ping_payload_len={len(response_payload)}")
    return 0


def device_info_to_dict(device: HidDeviceInfo) -> dict[str, Any]:
    path_display = device.path.decode("utf-8", errors="replace") if isinstance(device.path, bytes) else str(device.path)
    return {
        "path": path_display,
        "vendor_id": device.vendor_id,
        "product_id": device.product_id,
        "usage_page": device.usage_page,
        "usage": device.usage,
        "product_string": device.product_string,
        "serial_number": device.serial_number,
    }


def select_matching_device(
    *,
    path: str | bytes | None = None,
    path_contains: str = "",
    serial_contains: str = "",
    vendor_id: int | None = None,
    product_id: int | None = None,
) -> HidDeviceInfo:
    if path:
        raw_path = path.encode() if isinstance(path, str) else path
        return HidDeviceInfo(
            path=raw_path,
            vendor_id=vendor_id or 0,
            product_id=product_id or 0,
            usage_page=FIDO_USAGE_PAGE,
            usage=FIDO_USAGE,
            product_string=None,
            serial_number=None,
        )

    devices = discover_devices()
    for device in devices:
        if vendor_id is not None and device.vendor_id != vendor_id:
            continue
        if product_id is not None and device.product_id != product_id:
            continue
        if path_contains:
            candidate = (
                device.path.decode("utf-8", errors="replace")
                if isinstance(device.path, bytes)
                else str(device.path)
            )
            if path_contains not in candidate:
                continue
        if serial_contains and serial_contains not in (device.serial_number or ""):
            continue
        return device

    raise RuntimeError("no matching FIDO HID device found")


def open_device_for_info(device_info: HidDeviceInfo) -> hid.device:
    device = hid.device()
    device.open_path(device_info.path)
    device.set_nonblocking(False)
    return device


def receive_timeout(
    device: hid.device, timeout_ms: int, verbose: bool, trace: list[dict[str, str]] | None = None
) -> bool:
    try:
        read_frame(device, timeout_ms, verbose, trace)
    except TimeoutError:
        return True
    return False


def ctap_status_name(status: int) -> str:
    names = {
        0x00: "success",
        0x01: "invalid_command",
        0x02: "invalid_parameter",
        0x03: "invalid_length",
        0x0B: "invalid_channel",
        0x11: "cbor_unexpected_type",
        0x12: "invalid_cbor",
        0x14: "missing_parameter",
        0x19: "credential_excluded",
        0x26: "unsupported_algorithm",
        0x27: "operation_denied",
        0x28: "key_store_full",
        0x2B: "unsupported_option",
        0x2C: "invalid_option",
        0x2D: "keepalive_cancel",
        0x2E: "no_credentials",
        0x2F: "user_action_timeout",
        0x30: "not_allowed",
        0x31: "pin_invalid",
        0x32: "pin_blocked",
        0x33: "pin_auth_invalid",
        0x34: "pin_auth_blocked",
        0x35: "pin_not_set",
        0x36: "pin_required",
        0x37: "pin_policy_violation",
        0x3E: "invalid_subcommand",
        0x7F: "other",
    }
    return names.get(status, f"0x{status:02x}")


def decode_ctap_response_payload(payload: bytes) -> dict[str, Any]:
    decoded: Any | None = None
    decode_error: str | None = None
    if len(payload) > 1:
        try:
            decoded = cbor2.loads(payload[1:])
        except Exception as exc:  # pragma: no cover - best-effort evidence
            decode_error = str(exc)
    return {
        "ctap_status": payload[0] if payload else None,
        "ctap_status_name": ctap_status_name(payload[0]) if payload else None,
        "cbor_body_hex": payload[1:].hex() if len(payload) > 1 else "",
        "decoded": decoded,
        "decode_error": decode_error,
    }


def decode_u2f_response_payload(payload: bytes) -> dict[str, Any]:
    status_words = payload[-2:] if len(payload) >= 2 else payload
    status_word = int.from_bytes(status_words, "big") if status_words else None
    parsed = {
        "status_words_hex": status_words.hex(),
        "status_word": status_word,
        "status_word_name": u2f_status_word_name(status_words),
        "payload_hex": payload[:-2].hex() if len(payload) >= 2 else "",
    }
    if len(payload) < 2:
        return parsed
    return {
        **parsed,
    }


def u2f_status_word_name(status_words: bytes) -> str | None:
    names = {
        U2F_SW_NO_ERROR: "SW_NO_ERROR",
        U2F_SW_CONDITIONS_NOT_SATISFIED: "SW_CONDITIONS_NOT_SATISFIED",
        U2F_SW_WRONG_LENGTH: "SW_WRONG_LENGTH",
        U2F_SW_WRONG_DATA: "SW_WRONG_DATA",
        U2F_SW_INS_NOT_SUPPORTED: "SW_INS_NOT_SUPPORTED",
        U2F_SW_CLA_NOT_SUPPORTED: "SW_CLA_NOT_SUPPORTED",
    }
    return names.get(status_words)


def _read_der_tlv_length(data: bytes, offset: int) -> tuple[int, int]:
    if offset >= len(data) or data[offset] != 0x30:
        raise RuntimeError("U2F register response does not contain an attestation certificate")
    if offset + 2 > len(data):
        raise RuntimeError("U2F attestation certificate is truncated")
    first_len = data[offset + 1]
    if first_len < 0x80:
        return offset + 2, first_len
    len_len = first_len & 0x7F
    if len_len == 0 or len_len > 4 or offset + 2 + len_len > len(data):
        raise RuntimeError("U2F attestation certificate has invalid DER length")
    value_len = int.from_bytes(data[offset + 2 : offset + 2 + len_len], "big")
    return offset + 2 + len_len, value_len


def extract_u2f_attestation_certificate(response_payload: bytes) -> bytes:
    if len(response_payload) < 70:
        raise RuntimeError("U2F register response too short")
    status_words = response_payload[-2:]
    if status_words != U2F_SW_NO_ERROR:
        raise RuntimeError(f"U2F register failed with status {status_words.hex()}")
    key_handle_len = response_payload[66]
    cert_offset = 67 + key_handle_len
    value_offset, value_len = _read_der_tlv_length(response_payload, cert_offset)
    cert_end = value_offset + value_len
    if cert_end > len(response_payload) - 2:
        raise RuntimeError("U2F attestation certificate overruns register response")
    return response_payload[cert_offset:cert_end]


def extract_make_credential_attestation_certificate(response_payload: bytes) -> bytes:
    if not response_payload:
        raise RuntimeError("MakeCredential response is empty")
    if response_payload[0] != 0:
        raise RuntimeError(f"MakeCredential failed with CTAP status 0x{response_payload[0]:02x}")
    try:
        decoded = cbor2.loads(response_payload[1:])
    except Exception as exc:
        raise RuntimeError(f"MakeCredential response is not valid CBOR: {exc}") from exc
    if not isinstance(decoded, dict):
        raise RuntimeError("MakeCredential response body is not a CBOR map")
    att_stmt = decoded.get(3)
    if not isinstance(att_stmt, dict):
        raise RuntimeError("MakeCredential response does not contain attStmt")
    x5c = att_stmt.get("x5c")
    if not isinstance(x5c, list) or not x5c:
        raise RuntimeError("MakeCredential attStmt does not contain x5c")
    cert_der = x5c[0]
    if not isinstance(cert_der, bytes) or not cert_der:
        raise RuntimeError("MakeCredential x5c[0] is not a DER certificate")
    value_offset, value_len = _read_der_tlv_length(cert_der, 0)
    if value_offset + value_len != len(cert_der):
        raise RuntimeError("MakeCredential x5c[0] has trailing or truncated DER data")
    return cert_der


def parse_auth_data(auth_data: bytes) -> dict[str, Any]:
    if len(auth_data) < 37:
        raise RuntimeError("authData too short")

    flags = auth_data[32]
    sign_count = int.from_bytes(auth_data[33:37], "big")
    parsed: dict[str, Any] = {
        "flags": flags,
        "user_present": bool(flags & 0x01),
        "user_verified": bool(flags & 0x04),
        "sign_count": sign_count,
        "auth_data_hex": auth_data.hex(),
    }
    if flags & 0x40 and len(auth_data) >= 55:
        parsed["aaguid"] = auth_data[37:53].hex()
        credential_id_length = int.from_bytes(auth_data[53:55], "big")
        start = 55
        end = start + credential_id_length
        parsed["credential_id"] = auth_data[start:end].hex()
        parsed["credential_id_length"] = credential_id_length
    return parsed


def parse_make_credential_ctap_payload(payload: bytes) -> dict[str, Any]:
    parsed = decode_ctap_response_payload(payload)
    decoded = parsed.get("decoded")
    if isinstance(decoded, dict):
        auth_data = decoded.get(2)
        if isinstance(auth_data, bytes):
            parsed["auth_data"] = parse_auth_data(auth_data)
        try:
            cert_der = extract_make_credential_attestation_certificate(payload)
        except RuntimeError:
            pass
        else:
            parsed["attestation_certificate_len"] = len(cert_der)
            parsed["attestation_certificate_sha256"] = hashlib.sha256(cert_der).hexdigest()
    return parsed


def parse_get_assertion_ctap_payload(payload: bytes) -> dict[str, Any]:
    parsed = decode_ctap_response_payload(payload)
    decoded = parsed.get("decoded")
    if isinstance(decoded, dict):
        auth_data = decoded.get(2)
        if isinstance(auth_data, bytes):
            parsed["auth_data"] = parse_auth_data(auth_data)
    return parsed


def b64url_encode(value: bytes) -> str:
    return base64.urlsafe_b64encode(value).rstrip(b"=").decode("ascii")


def build_make_credential_request(
    rp_id: str,
    *,
    empty_pin_auth: bool,
    omit_client_data_hash: bool,
    trailing_bytes: bytes = b"",
    resident_key: bool = True,
    user_suffix: str = "01",
    exclude_credential_ids: list[bytes] | None = None,
    attestation_formats: list[str] | None = None,
    pin_auth: bytes | None = None,
    pin_protocol: int | None = None,
) -> bytes:
    user_id = f"debug-user-{user_suffix}".encode("utf-8")
    username = f"debugger-{user_suffix}"
    request: dict[object, object] = {
        2: {
            "id": rp_id,
            "name": "ZeroFIDO Debug",
        },
        3: {
            "id": user_id,
            "name": username,
            "displayName": f"ZeroFIDO Debug User {user_suffix}",
        },
        4: [
            {
                "type": "public-key",
                "alg": -7,
            }
        ],
        7: {
            "rk": resident_key,
            "uv": False,
        },
    }
    if not omit_client_data_hash:
        challenge_label = f"zerofido-makecredential-debug-{rp_id}-{user_suffix}".encode("utf-8")
        request[1] = hashlib.sha256(challenge_label).digest()
    if exclude_credential_ids:
        request[5] = [{"type": "public-key", "id": credential_id} for credential_id in exclude_credential_ids]
    if attestation_formats:
        request[11] = attestation_formats
    if pin_auth is not None:
        request[8] = pin_auth
        request[9] = pin_protocol or 1
    elif empty_pin_auth:
        request[8] = b""
        request[9] = 1
    return cbor2.dumps(request) + trailing_bytes


def build_get_assertion_request(
    rp_id: str,
    credential_ids: list[bytes],
    user_verification: str,
    *,
    empty_pin_auth: bool,
    omit_client_data_hash: bool,
    silent: bool,
    include_rk_option: bool,
    allow_list_count: int = 0,
    trailing_bytes: bytes = b"",
    pin_auth: bytes | None = None,
    pin_protocol: int | None = None,
) -> bytes:
    request: dict[object, object] = {
        1: rp_id,
        5: {
            "up": not silent,
            "uv": user_verification == "required",
        },
    }
    if not omit_client_data_hash:
        challenge_label = f"zerofido-getassertion-debug-{rp_id}-{user_verification}".encode("utf-8")
        request[2] = hashlib.sha256(challenge_label).digest()

    allow_list = build_allow_list(credential_ids, allow_list_count)
    if allow_list:
        request[3] = allow_list
    if pin_auth is not None:
        request[6] = pin_auth
        request[7] = pin_protocol or 1
    elif empty_pin_auth:
        request[6] = b""
        request[7] = 1
    if include_rk_option:
        options = dict(request[5])
        options["rk"] = True
        request[5] = options

    return cbor2.dumps(request) + trailing_bytes


def _sha256(data: bytes) -> bytes:
    digest = hashes.Hash(hashes.SHA256(), backend=default_backend())
    digest.update(data)
    return digest.finalize()


def _aes_cbc_zero_iv_encrypt(key: bytes, payload: bytes) -> bytes:
    cipher = Cipher(algorithms.AES(key), modes.CBC(bytes(16)), backend=default_backend())
    encryptor = cipher.encryptor()
    return encryptor.update(payload) + encryptor.finalize()


def _aes_cbc_zero_iv_decrypt(key: bytes, payload: bytes) -> bytes:
    cipher = Cipher(algorithms.AES(key), modes.CBC(bytes(16)), backend=default_backend())
    decryptor = cipher.decryptor()
    return decryptor.update(payload) + decryptor.finalize()


def _hmac_first16(key: bytes, payload: bytes) -> bytes:
    mac = hmac.HMAC(key, hashes.SHA256(), backend=default_backend())
    mac.update(payload)
    return mac.finalize()[:16]


def build_client_pin_cose_key(private_key: ec.EllipticCurvePrivateKey) -> dict[int, Any]:
    public_key = private_key.public_key().public_numbers()
    return {
        1: 2,
        3: -25,
        -1: 1,
        -2: public_key.x.to_bytes(32, "big"),
        -3: public_key.y.to_bytes(32, "big"),
    }


def derive_client_pin_shared_secret(
    peer_key_agreement: dict[int, Any], private_key: ec.EllipticCurvePrivateKey
) -> bytes:
    public_numbers = ec.EllipticCurvePublicNumbers(
        int.from_bytes(peer_key_agreement[-2], "big"),
        int.from_bytes(peer_key_agreement[-3], "big"),
        ec.SECP256R1(),
    )
    shared_secret = private_key.exchange(ec.ECDH(), public_numbers.public_key(default_backend()))
    return _sha256(shared_secret)


def build_client_pin_request(
    subcommand: int,
    *,
    key_agreement: dict[int, Any] | None = None,
    pin_auth: bytes | None = None,
    new_pin_enc: bytes | None = None,
    pin_hash_enc: bytes | None = None,
    include_permissions: bool = False,
    include_rp_id: bool = False,
) -> bytes:
    request: dict[int, Any] = {
        1: 1,
        2: subcommand,
    }
    if key_agreement is not None:
        request[3] = key_agreement
    if pin_auth is not None:
        request[4] = pin_auth
    if new_pin_enc is not None:
        request[5] = new_pin_enc
    if pin_hash_enc is not None:
        request[6] = pin_hash_enc
    if include_permissions:
        request[9] = 1
    if include_rp_id:
        request[10] = "zerofido.local"
    return cbor2.dumps(request)


def pad_new_pin(pin: str) -> bytes:
    raw = pin.encode("utf-8")
    block = raw + bytes(64 - len(raw))
    return block


def pin_hash16(pin: str) -> bytes:
    return _sha256(pin.encode("utf-8"))[:16]


def pin_auth_for_client_data(pin_token: bytes, client_data_hash: bytes) -> bytes:
    return _hmac_first16(pin_token, client_data_hash)


def build_u2f_version_apdu() -> bytes:
    return bytes([0x00, U2F_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00])


def build_u2f_invalid_cla_version_apdu(cla: int = 0x6D) -> bytes:
    if cla < 0 or cla > 0xFF:
        raise ValueError("U2F invalid CLA must fit in one byte")
    return bytes([cla, U2F_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00])


def build_u2f_version_with_data_apdu(data: bytes) -> bytes:
    if len(data) > 0xFFFF:
        raise ValueError("U2F VERSION data buffer too long")
    return (
        bytes([0x00, U2F_VERSION, 0x00, 0x00, 0x00, (len(data) >> 8) & 0xFF, len(data) & 0xFF])
        + data
    )


def build_u2f_register_apdu(challenge: bytes, app_id: bytes) -> bytes:
    if len(challenge) != 32 or len(app_id) != 32:
        raise ValueError("U2F register challenge and app_id must be 32 bytes")
    return bytes([0x00, U2F_REGISTER, 0x00, 0x00, 0x00, 0x00, 0x40]) + challenge + app_id


def build_u2f_authenticate_apdu(
    challenge: bytes, app_id: bytes, key_handle: bytes, *, mode: int = U2F_AUTH_ENFORCE
) -> bytes:
    if len(challenge) != 32 or len(app_id) != 32:
        raise ValueError("U2F authenticate challenge and app_id must be 32 bytes")
    if len(key_handle) > 255:
        raise ValueError("U2F key handle too long")
    lc = 32 + 32 + 1 + len(key_handle)
    return (
        bytes([0x00, U2F_AUTHENTICATE, mode, 0x00, (lc >> 16) & 0xFF, (lc >> 8) & 0xFF, lc & 0xFF])
        + challenge
        + app_id
        + bytes([len(key_handle)])
        + key_handle
    )


def extract_u2f_key_handle(response_payload: bytes) -> bytes:
    if len(response_payload) < 69:
        raise RuntimeError("U2F register response too short")
    status_words = response_payload[-2:]
    if status_words != U2F_SW_NO_ERROR:
        raise RuntimeError(f"U2F register failed with status {status_words.hex()}")
    key_handle_len = response_payload[66]
    key_handle_start = 67
    key_handle_end = key_handle_start + key_handle_len
    if key_handle_end > len(response_payload) - 2:
        raise RuntimeError("U2F register key handle length is invalid")
    return response_payload[key_handle_start:key_handle_end]


def run_u2f_register(device: hid.device, timeout_ms: int, verbose: bool, args: argparse.Namespace) -> int:
    nonce = os.urandom(8)
    _, _, init_payload = transact(device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose)
    cid = expect_init_response(BROADCAST_CID, INIT, init_payload, nonce)
    challenge = hashlib.sha256(b"zerofido-probe-u2f-register").digest()
    app_id = hashlib.sha256(b"zerofido-probe-u2f-app").digest()
    response_cid, response_cmd, response_payload = transact(
        device, cid, MSG, build_u2f_register_apdu(challenge, app_id), timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during U2F register: 0x{response_payload[0]:02x}")
    if response_cmd != MSG:
        raise RuntimeError(f"unexpected U2F response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected U2F response cid 0x{response_cid:08x}")

    cert_der = extract_u2f_attestation_certificate(response_payload)
    decoded = decode_u2f_response_payload(response_payload)
    decoded["key_handle_hex"] = extract_u2f_key_handle(response_payload).hex()
    decoded["attestation_certificate_len"] = len(cert_der)
    if args.u2f_cert_out:
        cert_out = Path(args.u2f_cert_out)
        cert_out.parent.mkdir(parents=True, exist_ok=True)
        cert_out.write_bytes(cert_der)
        decoded["attestation_certificate_path"] = str(cert_out)
    print(json.dumps(decoded, indent=2, sort_keys=True))
    return 0


def run_u2f_version(device: hid.device, timeout_ms: int, verbose: bool) -> int:
    nonce = os.urandom(8)
    _, _, init_payload = transact(device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose)
    cid = expect_init_response(BROADCAST_CID, INIT, init_payload, nonce)
    response_cid, response_cmd, response_payload = transact(
        device, cid, MSG, build_u2f_version_apdu(), timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during U2F VERSION: 0x{response_payload[0]:02x}")
    if response_cmd != MSG:
        raise RuntimeError(f"unexpected U2F VERSION response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected U2F VERSION response cid 0x{response_cid:08x}")
    print(json.dumps(decode_u2f_response_payload(response_payload), indent=2, sort_keys=True))
    return 0


def run_u2f_invalid_cla(device: hid.device, timeout_ms: int, verbose: bool, cla: int) -> int:
    nonce = os.urandom(8)
    _, _, init_payload = transact(device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose)
    cid = expect_init_response(BROADCAST_CID, INIT, init_payload, nonce)
    response_cid, response_cmd, response_payload = transact(
        device, cid, MSG, build_u2f_invalid_cla_version_apdu(cla), timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during invalid-CLA U2F VERSION: 0x{response_payload[0]:02x}")
    if response_cmd != MSG:
        raise RuntimeError(f"unexpected invalid-CLA U2F VERSION response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected invalid-CLA U2F VERSION response cid 0x{response_cid:08x}")
    print(json.dumps(decode_u2f_response_payload(response_payload), indent=2, sort_keys=True))
    return 0


def run_u2f_version_data(device: hid.device, timeout_ms: int, verbose: bool) -> int:
    nonce = os.urandom(8)
    _, _, init_payload = transact(device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose)
    cid = expect_init_response(BROADCAST_CID, INIT, init_payload, nonce)
    data = bytes.fromhex("5938e99cc9695e756177e67137b588d2e54a5ecaef4a291e7ba1115d0ca5e270ad92")
    response_cid, response_cmd, response_payload = transact(
        device, cid, MSG, build_u2f_version_with_data_apdu(data), timeout_ms, verbose
    )
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during U2F VERSION-with-data: 0x{response_payload[0]:02x}")
    if response_cmd != MSG:
        raise RuntimeError(f"unexpected U2F VERSION-with-data response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected U2F VERSION-with-data response cid 0x{response_cid:08x}")
    print(json.dumps(decode_u2f_response_payload(response_payload), indent=2, sort_keys=True))
    return 0


def run_u2f_auth_invalid(device: hid.device, timeout_ms: int, verbose: bool) -> int:
    nonce = os.urandom(8)
    _, _, init_payload = transact(device, BROADCAST_CID, INIT, nonce, timeout_ms, verbose)
    cid = expect_init_response(BROADCAST_CID, INIT, init_payload, nonce)
    challenge = hashlib.sha256(b"zerofido-probe-u2f-invalid-auth-challenge").digest()
    app_id = hashlib.sha256(b"zerofido-probe-u2f-invalid-auth-app").digest()
    key_handle = bytes(range(64))
    response_cid, response_cmd, response_payload = transact(
        device,
        cid,
        MSG,
        build_u2f_authenticate_apdu(challenge, app_id, key_handle, mode=U2F_AUTH_CHECK_ONLY),
        timeout_ms,
        verbose,
    )
    if response_cmd == ERROR:
        raise RuntimeError(f"CTAPHID error during invalid U2F authenticate: 0x{response_payload[0]:02x}")
    if response_cmd != MSG:
        raise RuntimeError(f"unexpected invalid U2F authenticate response cmd 0x{response_cmd:02x}")
    if response_cid != cid:
        raise RuntimeError(f"unexpected invalid U2F authenticate response cid 0x{response_cid:08x}")
    print(json.dumps(decode_u2f_response_payload(response_payload), indent=2, sort_keys=True))
    return 0


def summarize_exchange(
    *,
    scenario_id: str,
    trace: list[dict[str, str]],
    response_cid: int,
    response_cmd: int,
    response_payload: bytes,
    decoder: str = "ctap",
    extra: dict[str, Any] | None = None,
) -> dict[str, Any]:
    decoded = (
        decode_ctap_response_payload(response_payload)
        if decoder == "ctap"
        else decode_u2f_response_payload(response_payload)
    )
    result = {
        "scenario_id": scenario_id,
        "trace": trace,
        "response": {
            "cid": f"0x{response_cid:08x}",
            "cmd": f"0x{response_cmd:02x}",
            "payload_hex": response_payload.hex(),
            "decoded": decoded,
        },
    }
    if extra:
        result.update(extra)
    return result


def get_local_hostname() -> str:
    try:
        return socket.gethostname()
    except Exception:
        return "localhost"


def open_device(args: argparse.Namespace) -> hid.device:
    device = hid.device()
    if args.path:
        raw_path = args.path.encode() if isinstance(args.path, str) else args.path
        try:
            device.open_path(raw_path)
        except Exception as exc:
            raise RuntimeError(f"could not open FIDO HID device at {raw_path!r}: {exc}") from exc
        device.set_nonblocking(False)
        return device

    devices = discover_devices()
    if not devices:
        raise RuntimeError("no matching FIDO HID device found")
    if args.verbose:
        print_devices(devices)
    try:
        device.open_path(devices[0].path)
    except Exception as exc:
        raise RuntimeError(
            f"could not open FIDO HID device at {devices[0].path!r}: {exc}. "
            "Close other FIDO clients and reconnect or restart the authenticator if macOS still "
            "holds the interface."
        ) from exc
    device.set_nonblocking(False)
    return device


def main() -> int:
    args = parse_args()
    if args.cmd == "list":
        return print_devices(discover_devices())

    try:
        device = open_device(args)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    try:
        if args.cmd == "init":
            return run_init(device, args.timeout_ms, args.verbose)
        if args.cmd == "ping":
            return run_ping(device, args.timeout_ms, args.verbose)
        if args.cmd == "getinfo":
            return run_get_info(device, args.timeout_ms, args.verbose)
        credential_ids = parse_credential_id_values(args.credential_id)
        if args.cmd == "makecredential":
            return run_make_credential(device, args.timeout_ms, args.verbose, args)
        if args.cmd == "getnextassertion":
            return run_get_next_assertion(
                device, args.timeout_ms, args.verbose, args, credential_ids
            )
        if args.cmd == "cancel":
            return run_cancel(
                device,
                args.timeout_ms,
                args.verbose,
                args.rp_id,
                credential_ids,
                args.user_verification,
                args.cancel_target,
            )
        if args.cmd == "resync":
            return run_resync(device, args.timeout_ms, args.verbose, args, credential_ids)
        if args.cmd == "invalidcid":
            return run_invalid_cid(device, args.timeout_ms, args.verbose)
        if args.cmd == "exhaustcids":
            return run_exhaust_cids(device, args.timeout_ms, args.verbose, args.cid_count)
        if args.cmd == "shortinit":
            return run_short_init(device, args.timeout_ms, args.verbose, args.short_init_len)
        if args.cmd == "bench":
            return run_bench(device, args.timeout_ms, args.verbose, args.iterations)
        if args.cmd == "u2fversion":
            return run_u2f_version(device, args.timeout_ms, args.verbose)
        if args.cmd == "u2fregister":
            return run_u2f_register(device, args.timeout_ms, args.verbose, args)
        if args.cmd == "u2fauthinvalid":
            return run_u2f_auth_invalid(device, args.timeout_ms, args.verbose)
        if args.cmd == "u2finvalidcla":
            return run_u2f_invalid_cla(device, args.timeout_ms, args.verbose, args.u2f_invalid_cla)
        if args.cmd == "u2fversiondata":
            return run_u2f_version_data(device, args.timeout_ms, args.verbose)
        return run_get_assertion(
            device,
            args.timeout_ms,
            args.verbose,
            args,
            credential_ids,
        )
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    finally:
        device.close()


if __name__ == "__main__":
    raise SystemExit(main())
