"""Tests for the security-state semantics audit verifier."""

from __future__ import annotations

import re
import subprocess
import sys
import unittest
from pathlib import Path

from tests.harness import ROOT, load_module, missing_fixture_paths, stage_temp_repo as stage_repo

SCRIPT_PATH = ROOT / "tools" / "verify_security_state_semantics_audit.py"
FIXTURE_PATHS = [
    "README.md",
    "docs/10-release-criteria.md",
    "docs/11-attestation.md",
    "docs/12-metadata.md",
    "docs/12-metadata-statement.json",
    "docs/16-current-state-claim-inventory.md",
    "docs/17-current-state-proof-taxonomy.md",
    "docs/18-current-state-revalidation-map.md",
    "docs/19-current-state-protocol-conformance.md",
    "docs/20-current-state-security-state-semantics.md",
    "host_tools/conformance_manifest.json",
    "src/u2f/apdu.c",
    "src/u2f/adapter.c",
    "src/u2f/session.c",
    "src/u2f/persistence.c",
    "src/zerofido_app_i.h",
    "src/zerofido_attestation.c",
    "src/zerofido_ctap_dispatch.c",
    "src/ctap/response.c",
    "src/pin/command.c",
    "src/pin/flow.c",
    "src/pin/store/state_store.c",
    "src/zerofido_store.c",
    "src/store/record_format.c",
    "src/store/recovery.c",
    "src/transport/dispatch.c",
    "src/transport/usb_hid_session.c",
    "src/transport/usb_hid_worker.c",
    "src/ui/status.c",
    "src/ui/approval_state.c",
    "src/ui/views.c",
    "tests/native/protocol/runner.c",
    "tests/host_tools/test_conformance_suite.py",
    "tools/run_protocol_regressions.py",
]


def load_verifier_module():
    return load_module("verify_security_state_semantics_audit", SCRIPT_PATH)


def stage_temp_repo():
    return stage_repo(FIXTURE_PATHS)



def replace_once(text: str, old: str, new: str) -> str:
    if old not in text:
        raise AssertionError(f"Expected text not found for mutation: {old!r}")
    return text.replace(old, new, 1)



def remove_entry(text: str, identifier: str, kind: str = "Finding") -> str:
    pattern = rf"^### {kind} {re.escape(identifier)}:.*?(?=^### (?:Finding|Checkpoint) |^## |\Z)"
    mutated, count = re.subn(pattern, "", text, count=1, flags=re.MULTILINE | re.DOTALL)
    if count != 1:
        raise AssertionError(f"Failed to remove {kind.lower()} {identifier}")
    return mutated


class SecurityStateSemanticsAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        missing = missing_fixture_paths(FIXTURE_PATHS)
        if missing:
            raise unittest.SkipTest(f"missing audit fixture paths: {', '.join(missing)}")
        cls.verifier = load_verifier_module()

    def run_verifier(self, *args: str, root: Path | None = None) -> subprocess.CompletedProcess[str]:
        command = [sys.executable, str(SCRIPT_PATH), *args]
        if root is not None:
            command.extend(["--root", str(root)])
        return subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)

    def test_strict_verifier_passes_for_repo(self) -> None:
        result = self.run_verifier("--strict")

        self.assertEqual(result.returncode, 0, msg=result.stderr or result.stdout)
        self.assertIn("strict: ok", result.stdout)

    def test_inventory_derivation_finds_all_21_s03_rows(self) -> None:
        rows = self.verifier.inventory_rows_for_slice(ROOT)

        self.assertEqual(len(rows), 21)
        self.assertIn("transport.cancel", rows)
        self.assertIn("clientpin.retry_lockout", rows)
        self.assertIn("identity.attestation_subjects", rows)

    def test_ledger_parser_finds_21_rows_and_7_checkpoints(self) -> None:
        entries = self.verifier.parse_ledger_entries(ROOT)
        finding_ids = [entry.identifier for entry in entries if entry.kind == "finding"]
        checkpoint_ids = [entry.identifier for entry in entries if entry.kind == "checkpoint"]

        self.assertEqual(len(finding_ids), 21)
        self.assertEqual(len(checkpoint_ids), 7)
        self.assertIn("ctap.multiple_assertions", finding_ids)
        self.assertIn("aux.u2f-counter-durability", checkpoint_ids)

    def test_rejects_missing_required_s03_row(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/20-current-state-security-state-semantics.md"
        original = ledger_path.read_text(encoding="utf-8")
        ledger_path.write_text(remove_entry(original, "clientpin.get_pin_token"), encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Missing S03 row ID in docs/20-current-state-security-state-semantics.md: clientpin.get_pin_token",
            result.stderr,
        )

    def test_rejects_missing_required_checkpoint(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/20-current-state-security-state-semantics.md"
        original = ledger_path.read_text(encoding="utf-8")
        ledger_path.write_text(
            remove_entry(original, "aux.credential-store-recovery", kind="Checkpoint"),
            encoding="utf-8",
        )

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Missing auxiliary checkpoint ID in docs/20-current-state-security-state-semantics.md: aux.credential-store-recovery",
            result.stderr,
        )

    def test_rejects_duplicate_checkpoint_ids(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/20-current-state-security-state-semantics.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "### Checkpoint aux.u2f-counter-durability: U2F counter durability gap",
            "### Checkpoint aux.live-proof-boundary: U2F counter durability gap",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Duplicate auxiliary checkpoint ID in docs/20-current-state-security-state-semantics.md: aux.live-proof-boundary",
            result.stderr,
        )

    def test_rejects_unsupported_proof_label(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/20-current-state-security-state-semantics.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- Proof label: `source-proven`",
            "- Proof label: `lab-proven`",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Unsupported proof label 'lab-proven' for transport.cancel in docs/20-current-state-security-state-semantics.md",
            result.stderr,
        )

    def test_rejects_missing_historical_citation_for_historical_row(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/20-current-state-security-state-semantics.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- External reference: `docs/17-current-state-proof-taxonomy.md`; `docs/18-current-state-revalidation-map.md` historical linkage stays required for this scaffold row until later tasks replace the placeholder with row-specific citations.",
            "- External reference: `docs/17-current-state-proof-taxonomy.md` only.",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Missing historical revalidation citation for transport.cancel in docs/20-current-state-security-state-semantics.md",
            result.stderr,
        )

    def test_rejects_missing_summary_label(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/20-current-state-security-state-semantics.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- Open browser/hardware gaps:",
            "- Open verification gaps:",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Missing summary label 'Open browser/hardware gaps' in docs/20-current-state-security-state-semantics.md",
            result.stderr,
        )

    def test_rejects_blank_current_evidence_field(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/20-current-state-security-state-semantics.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- Current code/test evidence: T01 scaffolds this row with inventory-derived ownership only: `docs/16-current-state-claim-inventory.md` assigns the row to S03, and later tasks must replace this placeholder with row-specific code paths, rerun commands, or both before any stronger verdict is allowed.",
            "- Current code/test evidence:",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Missing field 'Current code/test evidence' for transport.cancel in docs/20-current-state-security-state-semantics.md",
            result.stderr,
        )

    def test_rejects_inventory_drift_for_identity_row(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        inventory_path = temp_root / "docs/16-current-state-claim-inventory.md"
        original = inventory_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "| `identity.aaguid` | `docs/11-attestation.md`, `docs/12-metadata.md`, `docs/12-metadata-statement.json` | ZeroFIDO AAGUID | `b51a976a-0b02-40aa-9d8a-36c8b91bbd1a` | stable identity anchor across attestation, metadata, and runtime claim docs | `src/zerofido_attestation.c` (primary), `src/ctap/response.c` (support) | attestation asset review + doc/runtime cross-check | `S03 Security and state-semantics audit`, `S04 Release-claims and documentation truthfulness audit` |",
            "| `identity.aaguid` | `docs/11-attestation.md`, `docs/12-metadata.md`, `docs/12-metadata-statement.json` | ZeroFIDO AAGUID DRIFT | `b51a976a-0b02-40aa-9d8a-36c8b91bbd1a` | stable identity anchor across attestation, metadata, and runtime claim docs | `src/zerofido_attestation.c` (primary), `src/ctap/response.c` (support) | attestation asset review + doc/runtime cross-check | `S03 Security and state-semantics audit`, `S04 Release-claims and documentation truthfulness audit` |",
        )
        inventory_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Inventory surface drift for S03-owned row identity.aaguid", result.stderr)
    def test_clientpin_rows_name_historical_anchors(self) -> None:
        ledger = (ROOT / "docs/20-current-state-security-state-semantics.md").read_text(encoding="utf-8")

        self.assertIn("historical finding 9 in `docs/18-current-state-revalidation-map.md`", ledger)
        self.assertIn("historical finding 13 in `docs/18-current-state-revalidation-map.md`", ledger)
        self.assertIn("Historical finding 12 in `docs/18-current-state-revalidation-map.md`", ledger)
        self.assertIn("explicit local unblock ceremony", ledger)
        self.assertIn("test_get_key_agreement_does_not_rotate_runtime_secrets", ledger)
        self.assertIn("test_set_pin_invalid_new_pin_block_is_rejected", ledger)


if __name__ == "__main__":
    unittest.main()
