"""Export certification-tool metadata from the canonical statement.

The source metadata statement remains static and product-oriented; this tool
normalizes profile-specific authenticatorGetInfo snapshots and U2F certificate
fields into the JSON shapes expected by certification tooling.
"""

from __future__ import annotations

import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
from typing import Any

from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_METADATA_DIR = ROOT / "metadata"
DEFAULT_STATEMENT_PATH = DEFAULT_METADATA_DIR / "statement.json"
MDS3_LEGAL_HEADER = (
    "Submission of this statement and retrieval and use of this statement indicates acceptance "
    "of the appropriate agreement located at "
    "https://fidoalliance.org/metadata/metadata-legal-terms/."
)
CTAP20_VERSION = {"major": 1, "minor": 0}
CTAP21_VERSION = {"major": 1, "minor": 1}
FIDO20_VERSION_STRING = "FIDO_2_0"
FIDO21_VERSION_STRING = "FIDO_2_1"
U2F_VERSION_STRING = "U2F_V2"
ICON_DATA_URL = (
    "data:image/png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+tm2cAAAAASUVORK5CYII="
)
DEFAULT_CANONICAL_STATEMENT: dict[str, Any] = {
    "legalHeader": "https://fidoalliance.org/metadata/metadata-statement-legal-header/",
    "description": "ZeroFIDO",
    "authenticatorVersion": 700,
    "protocolFamily": "fido2",
    "schema": 3,
    "aaguid": "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a",
    "upv": [{"major": 1, "minor": 0}],
    "authenticationAlgorithms": ["secp256r1_ecdsa_sha256_raw"],
    "publicKeyAlgAndEncodings": ["cose"],
    "keyProtection": ["software"],
    "authenticatorGetInfo": {
        "versions": ["FIDO_2_0", "U2F_V2"],
        "extensions": ["credProtect", "hmac-secret"],
        "aaguid": "b51a976a0b0240aa9d8a36c8b91bbd1a",
        "options": {"rk": True, "up": True, "plat": False},
        "maxMsgSize": 1024,
        "pinUvAuthProtocols": [1],
    },
    "userVerificationDetails": [
        [
            {"userVerificationMethod": "passcode_external"},
            {"userVerificationMethod": "presence_internal"},
        ]
    ],
    "tcDisplay": [],
}


def compact_aaguid(value: str) -> str:
    return value.replace("-", "").lower()


def normalize_aaguid(value: str) -> str:
    compact = compact_aaguid(value)
    if len(compact) != 32:
        return compact
    return (
        f"{compact[0:8]}-{compact[8:12]}-{compact[12:16]}-"
        f"{compact[16:20]}-{compact[20:32]}"
    )


def normalize_live_get_info(decoded: dict[Any, Any]) -> dict[str, Any]:
    exported: dict[str, Any] = {}
    if 1 in decoded:
        exported["versions"] = decoded[1]
    if 2 in decoded:
        exported["extensions"] = decoded[2]
    if 3 in decoded and isinstance(decoded[3], (bytes, bytearray)):
        exported["aaguid"] = compact_aaguid(bytes(decoded[3]).hex())
    if 4 in decoded:
        exported["options"] = decoded[4]
    if 5 in decoded:
        exported["maxMsgSize"] = decoded[5]
    if 6 in decoded:
        exported["pinUvAuthProtocols"] = decoded[6]
    if 7 in decoded:
        exported["maxCredentialCountList"] = decoded[7]
    if 8 in decoded:
        exported["maxCredentialIdLength"] = decoded[8]
    if 9 in decoded:
        exported["transports"] = decoded[9]
    if 10 in decoded:
        exported["algorithms"] = decoded[10]
    if 11 in decoded:
        exported["maxSerializedLargeBlobArray"] = decoded[11]
    if 12 in decoded:
        exported["forcePINChange"] = decoded[12]
    if 13 in decoded:
        exported["minPINLength"] = decoded[13]
    if 14 in decoded:
        exported["firmwareVersion"] = decoded[14]
    if 15 in decoded:
        exported["maxCredBlobLength"] = decoded[15]
    if 16 in decoded:
        exported["maxRPIDsForSetMinPINLength"] = decoded[16]
    if 17 in decoded:
        exported["preferredPlatformUvAttempts"] = decoded[17]
    if 18 in decoded:
        exported["uvModality"] = decoded[18]
    if 19 in decoded:
        exported["certifications"] = decoded[19]
    if 20 in decoded:
        exported["remainingDiscoverableCredentials"] = decoded[20]
    if 21 in decoded:
        exported["vendorPrototypeConfigCommands"] = decoded[21]
    return exported


def normalize_upv(value: Any, profile: str) -> list[dict[str, int]]:
    versions: list[dict[str, int]] = []
    seen: set[tuple[int, int]] = set()
    required = [CTAP20_VERSION]

    if profile == "fido2-2.1-experimental":
        required = [CTAP21_VERSION, CTAP20_VERSION]

    if isinstance(value, list):
        for item in value:
            if not isinstance(item, dict):
                continue
            major = item.get("major")
            minor = item.get("minor")
            if not isinstance(major, int) or not isinstance(minor, int):
                continue
            key = (major, minor)
            if key in seen:
                continue
            seen.add(key)
            versions.append({"major": major, "minor": minor})

    filtered: list[dict[str, int]] = []
    required_keys = {(item["major"], item["minor"]) for item in required}
    for item in versions:
        key = (item["major"], item["minor"])
        if key in required_keys:
            filtered.append(item)
    versions = filtered

    for item in reversed(required):
        key = (item["major"], item["minor"])
        if key not in {(version["major"], version["minor"]) for version in versions}:
            versions.insert(0, dict(item))
    return versions


def normalize_string_sequence_with_required(value: Any, required: list[str]) -> list[str]:
    normalized: list[str] = []
    seen: set[str] = set()
    if isinstance(value, list):
        for item in value:
            if not isinstance(item, str) or item in seen:
                continue
            seen.add(item)
            normalized.append(item)
    for item in required:
        if item not in seen:
            normalized.append(item)
    return normalized


def normalize_statement_get_info(get_info: Any, profile: str) -> dict[str, Any]:
    normalized = copy.deepcopy(get_info) if isinstance(get_info, dict) else {}
    versions = normalized.get("versions")
    if isinstance(versions, list):
        deduped_versions: list[str] = []
        seen_versions: set[str] = set()
        for item in versions:
            if not isinstance(item, str) or item in seen_versions:
                continue
            seen_versions.add(item)
            deduped_versions.append(item)
    else:
        deduped_versions = []

    if profile == "fido2-2.1-experimental":
        desired_versions = [FIDO21_VERSION_STRING, FIDO20_VERSION_STRING, U2F_VERSION_STRING]
    else:
        desired_versions = [FIDO20_VERSION_STRING, U2F_VERSION_STRING]
    normalized["versions"] = [
        version for version in desired_versions if version in deduped_versions or version != U2F_VERSION_STRING
    ]
    if U2F_VERSION_STRING in deduped_versions and U2F_VERSION_STRING not in normalized["versions"]:
        normalized["versions"].append(U2F_VERSION_STRING)

    if "aaguid" in normalized and isinstance(normalized["aaguid"], str):
        normalized["aaguid"] = compact_aaguid(normalized["aaguid"])

    options = normalized.get("options")
    if not isinstance(options, dict):
        options = {}
    options.pop("clientPin", None)
    options.pop("uv", None)
    if profile == "fido2-2.1-experimental":
        options["pinUvAuthToken"] = True
        options["makeCredUvNotRqd"] = True
        normalized["pinUvAuthProtocols"] = [2, 1]
        normalized.setdefault("transports", ["usb"])
        normalized.setdefault("algorithms", [{"type": "public-key", "alg": -7}])
        normalized.setdefault("minPINLength", 4)
        normalized.setdefault("firmwareVersion", 700)
    else:
        options.pop("pinUvAuthToken", None)
        options.pop("makeCredUvNotRqd", None)
        normalized["pinUvAuthProtocols"] = [1]
        for field in ("transports", "algorithms", "minPINLength", "firmwareVersion"):
            normalized.pop(field, None)
    normalized["options"] = options
    return normalized


def load_certificate_der(path: Path) -> bytes:
    try:
        data = path.read_bytes()
    except FileNotFoundError as exc:
        raise ValueError(
            f"attestation certificate not found: {path}. Run ctaphid_probe.py first and make "
            "sure it succeeds before exporting certification metadata."
        ) from exc
    if data.startswith(b"-----BEGIN CERTIFICATE-----"):
        return x509.load_pem_x509_certificate(data).public_bytes(serialization.Encoding.DER)
    x509.load_der_x509_certificate(data)
    return data


def load_or_create_statement(path: Path) -> dict[str, Any]:
    if path.exists():
        return json.loads(path.read_text())

    statement = copy.deepcopy(DEFAULT_CANONICAL_STATEMENT)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(statement, indent=2) + "\n")
    return statement


def compute_u2f_attestation_key_identifier(cert_der: bytes) -> str:
    cert = x509.load_der_x509_certificate(cert_der)
    public_key = cert.public_key()
    if not isinstance(public_key, ec.EllipticCurvePublicKey):
        raise ValueError("U2F attestation certificate must contain an EC public key")
    if public_key.curve.name != "secp256r1":
        raise ValueError(f"U2F attestation certificate must use P-256, got {public_key.curve.name}")
    subject_public_key = public_key.public_bytes(
        serialization.Encoding.X962,
        serialization.PublicFormat.UncompressedPoint,
    )
    return hashlib.sha1(subject_public_key).hexdigest()


def is_self_issued_certificate(cert_der: bytes) -> bool:
    cert = x509.load_der_x509_certificate(cert_der)
    return cert.issuer == cert.subject


def attestation_root_certificates_for_leaf(
    statement: dict[str, Any],
    attestation_cert_der: bytes,
    label: str,
) -> list[str]:
    if is_self_issued_certificate(attestation_cert_der):
        return [base64.b64encode(attestation_cert_der).decode("ascii")]

    roots = statement.get("attestationRootCertificates")
    if not isinstance(roots, list) or not all(isinstance(item, str) and item for item in roots):
        raise ValueError(
            f"{label} attestation certificate is not self-issued and statement has no "
            "attestationRootCertificates"
        )
    return copy.deepcopy(roots)


def build_u2f_metadata(
    statement: dict[str, Any],
    u2f_attestation_cert_der: bytes | None = None,
) -> dict[str, Any]:
    exported = copy.deepcopy(statement)
    exported.pop("aaguid", None)
    exported.pop("aaid", None)
    exported.pop("authenticatorGetInfo", None)
    exported.pop("isSecondFactorOnly", None)
    exported.pop("friendlyNames", None)
    exported["description"] = exported.get("description", "ZeroFIDO")
    exported["authenticatorVersion"] = exported.get("authenticatorVersion", 700)
    exported["schema"] = 3
    exported["protocolFamily"] = "u2f"
    exported["upv"] = [{"major": 1, "minor": 2}]
    exported["authenticationAlgorithms"] = exported.get(
        "authenticationAlgorithms", ["secp256r1_ecdsa_sha256_raw"]
    )
    exported["publicKeyAlgAndEncodings"] = ["ecc_x962_raw"]
    exported["userVerificationDetails"] = [[{"userVerificationMethod": "presence_internal"}]]
    exported["keyProtection"] = ["software"]
    exported["matcherProtection"] = ["software"]
    exported["tcDisplay"] = []
    exported["icon"] = ICON_DATA_URL
    if u2f_attestation_cert_der is not None:
        skid = compute_u2f_attestation_key_identifier(u2f_attestation_cert_der)
        exported["attestationTypes"] = ["basic_full"]
        exported["attestationRootCertificates"] = attestation_root_certificates_for_leaf(
            statement,
            u2f_attestation_cert_der,
            "U2F",
        )
        exported["attestationCertificateKeyIdentifiers"] = [skid]
    return exported


def build_certification_metadata(
    statement: dict[str, Any],
    live_get_info: dict[Any, Any] | None = None,
    profile: str = "fido2",
    fido2_attestation_cert_der: bytes | None = None,
    u2f_attestation_cert_der: bytes | None = None,
    client_pin_state: str = "unset",
) -> dict[str, Any]:
    if profile == "u2f":
        return build_u2f_metadata(statement, u2f_attestation_cert_der)
    if profile == "fido2":
        profile = "fido2-2.0"
    if profile not in ("fido2-2.0", "fido2-2.1-experimental"):
        raise ValueError(f"unsupported metadata profile: {profile}")

    exported = copy.deepcopy(statement)
    exported["legalHeader"] = MDS3_LEGAL_HEADER
    exported["description"] = exported.get("description") or DEFAULT_CANONICAL_STATEMENT["description"]
    exported["authenticatorVersion"] = (
        exported.get("authenticatorVersion") or DEFAULT_CANONICAL_STATEMENT["authenticatorVersion"]
    )
    exported["protocolFamily"] = "fido2"
    exported["schema"] = 3
    exported["upv"] = normalize_upv(exported.get("upv"), profile)
    if "aaguid" in exported and isinstance(exported["aaguid"], str):
        exported["aaguid"] = normalize_aaguid(exported["aaguid"])
    exported.pop("friendlyNames", None)
    exported.pop("aaid", None)
    exported.pop("supportedExtensions", None)
    exported.pop("assertionScheme", None)
    exported.pop("authenticationAlgorithm", None)
    exported.pop("publicKeyAlgAndEncoding", None)
    exported.pop("operatingEnv", None)
    exported.pop("isSecondFactorOnly", None)
    exported.pop("attestationCertificateKeyIdentifiers", None)
    exported["publicKeyAlgAndEncodings"] = normalize_string_sequence_with_required(
        exported.get("publicKeyAlgAndEncodings"),
        ["cose"],
    )
    exported["keyProtection"] = normalize_string_sequence_with_required(
        exported.get("keyProtection"),
        DEFAULT_CANONICAL_STATEMENT["keyProtection"],
    )
    exported["matcherProtection"] = ["software"]
    exported["tcDisplay"] = []
    exported["icon"] = ICON_DATA_URL
    if "attestationTypes" not in exported:
        exported["attestationTypes"] = ["basic_surrogate"]
    if fido2_attestation_cert_der is not None:
        exported["attestationTypes"] = ["basic_full"]
        exported["attestationRootCertificates"] = attestation_root_certificates_for_leaf(
            statement,
            fido2_attestation_cert_der,
            "FIDO2",
        )

    get_info = normalize_statement_get_info(exported.get("authenticatorGetInfo", {}), profile)
    if live_get_info is not None:
        get_info = normalize_live_get_info(live_get_info)
    elif client_pin_state != "omit":
        options = get_info.setdefault("options", {})
        if isinstance(options, dict):
            options["clientPin"] = client_pin_state == "set" or (
                profile == "fido2-2.1-experimental" and client_pin_state == "unset"
            )

    exported["authenticatorGetInfo"] = get_info
    return exported


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export certification-tool metadata from the canonical statement."
    )
    parser.add_argument(
        "--statement",
        default=str(DEFAULT_STATEMENT_PATH),
        help="Path to the canonical metadata statement JSON",
    )
    parser.add_argument(
        "--output",
        help=(
            "Path to write the certification metadata JSON. Defaults to "
            "metadata/metadata-<profile>.json."
        ),
    )
    parser.add_argument(
        "--profile",
        choices=("fido2", "fido2-2.0", "fido2-2.1-experimental", "u2f"),
        default="fido2",
        help="Metadata profile to export for certification tooling. fido2 aliases fido2-2.0.",
    )
    parser.add_argument(
        "--u2f-attestation-cert",
        help="DER or PEM U2F attestation certificate returned by this device's U2F Register response",
    )
    parser.add_argument(
        "--fido2-attestation-cert",
        help=(
            "DER or PEM packed attestation certificate returned by this device's FIDO2 "
            "MakeCredential response"
        ),
    )
    parser.add_argument(
        "--client-pin-state",
        choices=("unset", "set", "omit"),
        default="unset",
        help=(
            "ClientPIN option to put in exported FIDO2 metadata. Use unset for a fresh device, "
            "set for a pre-configured PIN, or omit for the canonical static statement model."
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    statement_path = Path(args.statement)
    profile_name = "ctap20" if args.profile in ("fido2", "fido2-2.0") else args.profile
    if profile_name == "fido2-2.1-experimental":
        profile_name = "ctap21-experimental"
    output_path = Path(args.output) if args.output else DEFAULT_METADATA_DIR / f"metadata-{profile_name}.json"

    statement = load_or_create_statement(statement_path)
    fido2_cert_der = None
    u2f_cert_der = None
    if args.u2f_attestation_cert:
        if args.profile != "u2f":
            raise SystemExit("--u2f-attestation-cert is only valid with --profile u2f")
        try:
            u2f_cert_der = load_certificate_der(Path(args.u2f_attestation_cert))
        except ValueError as exc:
            raise SystemExit(str(exc)) from exc
    if args.fido2_attestation_cert:
        if args.profile == "u2f":
            raise SystemExit("--fido2-attestation-cert is only valid with FIDO2 profiles")
        try:
            fido2_cert_der = load_certificate_der(Path(args.fido2_attestation_cert))
        except ValueError as exc:
            raise SystemExit(str(exc)) from exc
    if args.profile == "u2f" and u2f_cert_der is None:
        raise SystemExit(
            "--profile u2f requires --u2f-attestation-cert. "
            "Use host_tools/ctaphid_probe.py --cmd u2fregister --u2f-cert-out first."
        )

    exported = build_certification_metadata(
        statement,
        profile=args.profile,
        fido2_attestation_cert_der=fido2_cert_der,
        u2f_attestation_cert_der=u2f_cert_der,
        client_pin_state=args.client_pin_state,
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(exported, indent=2) + "\n")
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
