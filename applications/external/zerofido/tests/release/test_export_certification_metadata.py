"""Tests for metadata export normalization and profile generation."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
import unittest.mock
from base64 import b64encode
from datetime import datetime, timedelta, timezone
from hashlib import sha1
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID

from host_tools.export_certification_metadata import (
    DEFAULT_CANONICAL_STATEMENT,
    ICON_DATA_URL,
    MDS3_LEGAL_HEADER,
    build_certification_metadata,
    load_or_create_statement,
    main,
)


class ExportCertificationMetadataTests(unittest.TestCase):
    def load_statement(self) -> dict:
        return {
            "legalHeader": "https://fidoalliance.org/metadata/metadata-statement-legal-header/",
            "aaguid": "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a",
            "upv": [{"major": 1, "minor": 0}],
            "attestationTypes": ["basic_full"],
            "attestationRootCertificates": ["dGVzdC1yb290"],
            "authenticationAlgorithms": ["secp256r1_ecdsa_sha256_raw"],
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
        }

    def build_test_u2f_cert_der(self) -> tuple[bytes, bytes]:
        key = ec.generate_private_key(ec.SECP256R1())
        subject = x509.Name(
            [
                x509.NameAttribute(NameOID.COUNTRY_NAME, "BG"),
                x509.NameAttribute(NameOID.ORGANIZATION_NAME, "ZeroFIDO Test"),
                x509.NameAttribute(NameOID.COMMON_NAME, "ZeroFIDO Test U2F"),
            ]
        )
        cert = (
            x509.CertificateBuilder()
            .subject_name(subject)
            .issuer_name(subject)
            .public_key(key.public_key())
            .serial_number(1)
            .not_valid_before(datetime.now(timezone.utc) - timedelta(days=1))
            .not_valid_after(datetime.now(timezone.utc) + timedelta(days=1))
            .sign(key, hashes.SHA256())
        )
        cert_der = cert.public_bytes(serialization.Encoding.DER)
        public_key_bytes = key.public_key().public_bytes(
            serialization.Encoding.X962,
            serialization.PublicFormat.UncompressedPoint,
        )
        return cert_der, public_key_bytes

    def build_test_u2f_chain_der(self) -> tuple[bytes, bytes, bytes]:
        root_key = ec.generate_private_key(ec.SECP256R1())
        leaf_key = ec.generate_private_key(ec.SECP256R1())
        root_subject = x509.Name(
            [
                x509.NameAttribute(NameOID.COUNTRY_NAME, "BG"),
                x509.NameAttribute(NameOID.ORGANIZATION_NAME, "ZeroFIDO Test"),
                x509.NameAttribute(NameOID.COMMON_NAME, "ZeroFIDO Test Root"),
            ]
        )
        leaf_subject = x509.Name(
            [
                x509.NameAttribute(NameOID.COUNTRY_NAME, "BG"),
                x509.NameAttribute(NameOID.ORGANIZATION_NAME, "ZeroFIDO Test"),
                x509.NameAttribute(NameOID.COMMON_NAME, "ZeroFIDO Test U2F"),
            ]
        )
        root_cert = (
            x509.CertificateBuilder()
            .subject_name(root_subject)
            .issuer_name(root_subject)
            .public_key(root_key.public_key())
            .serial_number(1)
            .not_valid_before(datetime.now(timezone.utc) - timedelta(days=1))
            .not_valid_after(datetime.now(timezone.utc) + timedelta(days=1))
            .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
            .sign(root_key, hashes.SHA256())
        )
        leaf_cert = (
            x509.CertificateBuilder()
            .subject_name(leaf_subject)
            .issuer_name(root_subject)
            .public_key(leaf_key.public_key())
            .serial_number(2)
            .not_valid_before(datetime.now(timezone.utc) - timedelta(days=1))
            .not_valid_after(datetime.now(timezone.utc) + timedelta(days=1))
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .sign(root_key, hashes.SHA256())
        )
        leaf_public_key_bytes = leaf_key.public_key().public_bytes(
            serialization.Encoding.X962,
            serialization.PublicFormat.UncompressedPoint,
        )
        return (
            root_cert.public_bytes(serialization.Encoding.DER),
            leaf_cert.public_bytes(serialization.Encoding.DER),
            leaf_public_key_bytes,
        )

    def test_build_certification_metadata_defaults_to_ctap20_legal_header_and_upv(self) -> None:
        statement = self.load_statement()

        exported = build_certification_metadata(statement)

        self.assertEqual(exported["legalHeader"], MDS3_LEGAL_HEADER)
        self.assertEqual(exported["description"], "ZeroFIDO")
        self.assertEqual(exported["authenticatorVersion"], 700)
        self.assertEqual(exported["protocolFamily"], "fido2")
        self.assertEqual(exported["schema"], 3)
        self.assertEqual(exported["upv"], [{"major": 1, "minor": 0}])
        self.assertEqual(exported["publicKeyAlgAndEncodings"], ["cose"])
        self.assertEqual(exported["keyProtection"], ["software"])
        self.assertEqual(exported["tcDisplay"], [])
        self.assertNotIn("FIDO_2_1", exported["authenticatorGetInfo"]["versions"])
        self.assertFalse(exported["authenticatorGetInfo"]["options"]["clientPin"])
        self.assertEqual(exported["attestationTypes"], ["basic_full"])
        self.assertIn("attestationRootCertificates", exported)

    def test_build_certification_metadata_fido2_uses_self_issued_leaf_as_root(self) -> None:
        statement = self.load_statement()
        cert_der, _ = self.build_test_u2f_cert_der()

        exported = build_certification_metadata(
            statement,
            fido2_attestation_cert_der=cert_der,
        )

        self.assertEqual(exported["attestationTypes"], ["basic_full"])
        self.assertEqual(exported["attestationRootCertificates"], [b64encode(cert_der).decode("ascii")])
        self.assertNotIn("attestationCertificateKeyIdentifiers", exported)

    def test_build_certification_metadata_fido2_preserves_roots_for_subordinate_leaf(self) -> None:
        statement = self.load_statement()
        root_der, leaf_der, _ = self.build_test_u2f_chain_der()
        root_b64 = b64encode(root_der).decode("ascii")
        statement["attestationRootCertificates"] = [root_b64]

        exported = build_certification_metadata(
            statement,
            fido2_attestation_cert_der=leaf_der,
        )

        self.assertEqual(exported["attestationTypes"], ["basic_full"])
        self.assertEqual(exported["attestationRootCertificates"], [root_b64])
        self.assertNotIn("attestationCertificateKeyIdentifiers", exported)

    def test_build_certification_metadata_normalizes_stale_ctap20_metadata_fields(self) -> None:
        statement = {
            "legalHeader": "https://fidoalliance.org/metadata/metadata-statement-legal-header/",
            "aaguid": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
            "upv": [{"major": 1, "minor": 1}, {"major": 1, "minor": 0}],
            "authenticatorGetInfo": {
                "aaguid": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                "versions": ["FIDO_2_1", "FIDO_2_0", "U2F_V2", "FIDO_2_0"],
                "options": {
                    "rk": True,
                    "up": True,
                    "plat": False,
                    "pinUvAuthToken": True,
                    "makeCredUvNotRqd": True,
                },
                "pinUvAuthProtocols": [1, 2],
            },
            "userVerificationDetails": [[{"userVerificationMethod": "presence_internal"}]],
        }

        exported = build_certification_metadata(statement)

        self.assertEqual(exported["legalHeader"], MDS3_LEGAL_HEADER)
        self.assertEqual(exported["upv"], [{"major": 1, "minor": 0}])
        self.assertEqual(
            exported["authenticatorGetInfo"]["versions"],
            ["FIDO_2_0", "U2F_V2"],
        )
        self.assertEqual(exported["authenticatorGetInfo"]["pinUvAuthProtocols"], [1])
        self.assertNotIn("pinUvAuthToken", exported["authenticatorGetInfo"]["options"])
        self.assertNotIn("makeCredUvNotRqd", exported["authenticatorGetInfo"]["options"])
        self.assertFalse(exported["authenticatorGetInfo"]["options"]["clientPin"])
        self.assertNotIn("minPINLength", exported["authenticatorGetInfo"])
        self.assertNotIn("firmwareVersion", exported["authenticatorGetInfo"])

    def test_build_certification_metadata_experimental_ctap21_matches_profile_claims(self) -> None:
        statement = self.load_statement()

        exported = build_certification_metadata(statement, profile="fido2-2.1-experimental")

        self.assertEqual(exported["upv"], [{"major": 1, "minor": 1}, {"major": 1, "minor": 0}])
        self.assertEqual(
            exported["authenticatorGetInfo"]["versions"],
            ["FIDO_2_1", "FIDO_2_0", "U2F_V2"],
        )
        self.assertTrue(exported["authenticatorGetInfo"]["options"]["pinUvAuthToken"])
        self.assertTrue(exported["authenticatorGetInfo"]["options"]["makeCredUvNotRqd"])
        self.assertTrue(exported["authenticatorGetInfo"]["options"]["clientPin"])
        self.assertEqual(exported["authenticatorGetInfo"]["pinUvAuthProtocols"], [2, 1])
        self.assertEqual(exported["authenticatorGetInfo"]["transports"], ["usb"])
        self.assertEqual(exported["authenticatorGetInfo"]["algorithms"], [{"type": "public-key", "alg": -7}])
        self.assertEqual(exported["authenticatorGetInfo"]["minPINLength"], 4)
        self.assertEqual(exported["authenticatorGetInfo"]["firmwareVersion"], 700)

    def test_build_certification_metadata_can_export_set_or_omitted_client_pin_state(self) -> None:
        statement = self.load_statement()

        pin_set = build_certification_metadata(statement, client_pin_state="set")
        omitted = build_certification_metadata(statement, client_pin_state="omit")

        self.assertTrue(pin_set["authenticatorGetInfo"]["options"]["clientPin"])
        self.assertNotIn("clientPin", omitted["authenticatorGetInfo"]["options"])

    def test_build_certification_metadata_uses_surrogate_when_statement_has_no_attestation_type(self) -> None:
        statement = {
            "aaguid": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
            "authenticatorGetInfo": {
                "aaguid": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                "versions": ["FIDO_2_0"],
                "options": {"rk": True, "up": True, "plat": False},
            },
            "userVerificationDetails": [[{"userVerificationMethod": "presence_internal"}]],
        }

        exported = build_certification_metadata(statement)

        self.assertEqual(exported["attestationTypes"], ["basic_surrogate"])

    def test_build_certification_metadata_normalizes_statement_aaguids(self) -> None:
        statement = self.load_statement()

        exported = build_certification_metadata(statement)

        self.assertEqual(exported["aaguid"], "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a")
        self.assertEqual(
            exported["authenticatorGetInfo"]["aaguid"],
            "b51a976a0b0240aa9d8a36c8b91bbd1a",
        )
        self.assertEqual(exported["authenticationAlgorithms"], ["secp256r1_ecdsa_sha256_raw"])
        self.assertEqual(exported["matcherProtection"], ["software"])
        self.assertEqual(exported["icon"], ICON_DATA_URL)
        self.assertNotIn("friendlyNames", exported)
        self.assertNotIn("attestationCertificateKeyIdentifiers", exported)
        self.assertNotIn("isSecondFactorOnly", exported)

    def test_build_certification_metadata_prefers_live_get_info(self) -> None:
        statement = {
            "aaguid": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
            "authenticatorGetInfo": {
                "aaguid": "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                "versions": ["FIDO_2_0", "U2F_V2"],
            },
            "userVerificationDetails": [[{"userVerificationMethod": "presence_internal"}]],
        }
        live_get_info = {
            1: ["FIDO_2_0", "U2F_V2"],
            3: bytes.fromhex("b51a976a0b0240aa9d8a36c8b91bbd1a"),
            4: {"rk": True, "up": True, "plat": False, "clientPin": False},
            5: 1024,
            6: [1],
            9: ["usb"],
            10: [{"alg": -7, "type": "public-key"}],
            13: 4,
            14: 700,
        }

        exported = build_certification_metadata(statement, live_get_info=live_get_info)

        self.assertEqual(exported["aaguid"], "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa")
        self.assertEqual(
            exported["authenticatorGetInfo"]["aaguid"],
            "b51a976a0b0240aa9d8a36c8b91bbd1a",
        )
        self.assertEqual(
            exported["authenticatorGetInfo"]["versions"],
            ["FIDO_2_0", "U2F_V2"],
        )
        self.assertEqual(exported["userVerificationDetails"], [[{"userVerificationMethod": "presence_internal"}]])

    def test_build_certification_metadata_u2f_profile_removes_fido2_only_fields(self) -> None:
        statement = self.load_statement()

        exported = build_certification_metadata(statement, profile="u2f")

        self.assertEqual(exported["protocolFamily"], "u2f")
        self.assertEqual(exported["description"], "ZeroFIDO")
        self.assertEqual(exported["authenticatorVersion"], 700)
        self.assertEqual(exported["schema"], 3)
        self.assertEqual(exported["upv"], [{"major": 1, "minor": 2}])
        self.assertEqual(exported["authenticationAlgorithms"], ["secp256r1_ecdsa_sha256_raw"])
        self.assertEqual(exported["publicKeyAlgAndEncodings"], ["ecc_x962_raw"])
        self.assertEqual(exported["keyProtection"], ["software"])
        self.assertEqual(exported["matcherProtection"], ["software"])
        self.assertEqual(exported["tcDisplay"], [])
        self.assertEqual(exported["icon"], ICON_DATA_URL)
        self.assertEqual(exported["userVerificationDetails"], [[{"userVerificationMethod": "presence_internal"}]])
        self.assertNotIn("aaguid", exported)
        self.assertNotIn("aaid", exported)
        self.assertNotIn("authenticatorGetInfo", exported)
        self.assertNotIn("isSecondFactorOnly", exported)
        self.assertNotIn("friendlyNames", exported)

    def test_build_certification_metadata_u2f_profile_includes_attestation_cert_fields(self) -> None:
        statement = self.load_statement()
        cert_der, public_key_bytes = self.build_test_u2f_cert_der()

        exported = build_certification_metadata(
            statement,
            profile="u2f",
            u2f_attestation_cert_der=cert_der,
        )

        self.assertEqual(exported["attestationTypes"], ["basic_full"])
        self.assertEqual(exported["attestationRootCertificates"], [b64encode(cert_der).decode("ascii")])
        self.assertEqual(exported["attestationCertificateKeyIdentifiers"], [sha1(public_key_bytes).hexdigest()])

    def test_build_certification_metadata_u2f_profile_preserves_statement_roots_for_subordinate_cert(
        self,
    ) -> None:
        statement = self.load_statement()
        root_der, leaf_der, leaf_public_key_bytes = self.build_test_u2f_chain_der()
        root_b64 = b64encode(root_der).decode("ascii")
        statement["attestationRootCertificates"] = [root_b64]

        exported = build_certification_metadata(
            statement,
            profile="u2f",
            u2f_attestation_cert_der=leaf_der,
        )

        self.assertEqual(exported["attestationTypes"], ["basic_full"])
        self.assertEqual(exported["attestationRootCertificates"], [root_b64])
        self.assertEqual(
            exported["attestationCertificateKeyIdentifiers"],
            [sha1(leaf_public_key_bytes).hexdigest()],
        )

    def test_load_or_create_statement_creates_missing_parent_and_statement(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            statement_path = Path(tmpdir) / "metadata" / "statement.json"

            statement = load_or_create_statement(statement_path)

            self.assertEqual(statement, DEFAULT_CANONICAL_STATEMENT)
            self.assertEqual(json.loads(statement_path.read_text()), DEFAULT_CANONICAL_STATEMENT)

    def test_main_reports_missing_fido2_attestation_certificate_without_traceback(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            statement_path = Path(tmpdir) / "metadata" / "statement.json"
            missing_cert_path = Path(tmpdir) / "metadata" / "fido2-attestation.der"
            argv = [
                "export_certification_metadata.py",
                "--statement",
                str(statement_path),
                "--profile",
                "fido2-2.0",
                "--fido2-attestation-cert",
                str(missing_cert_path),
            ]

            with unittest.mock.patch.object(sys, "argv", argv), self.assertRaises(SystemExit) as ctx:
                main()

        self.assertIn("attestation certificate not found", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
