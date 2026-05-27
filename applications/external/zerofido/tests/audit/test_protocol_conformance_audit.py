"""Tests for the protocol-conformance audit verifier."""

from __future__ import annotations

import re
import subprocess
import sys
import unittest
from pathlib import Path

from tests.harness import ROOT, load_module, missing_fixture_paths, stage_temp_repo as stage_repo

SCRIPT_PATH = ROOT / "tools" / "verify_protocol_conformance_audit.py"
FIXTURE_PATHS = [
    "docs/10-release-criteria.md",
    "docs/11-attestation.md",
    "docs/12-metadata.md",
    "docs/16-current-state-claim-inventory.md",
    "docs/17-current-state-proof-taxonomy.md",
    "docs/18-current-state-revalidation-map.md",
    "docs/19-current-state-protocol-conformance.md",
    "host_tools/conformance_manifest.json",
]


def load_verifier_module():
    return load_module("verify_protocol_conformance_audit", SCRIPT_PATH)


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


class ProtocolConformanceAuditTests(unittest.TestCase):
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

    def test_inventory_derivation_finds_all_21_s02_rows(self) -> None:
        rows = self.verifier.inventory_rows_for_slice(ROOT)

        self.assertEqual(len(rows), 21)
        self.assertIn("transport.init", rows)
        self.assertIn("ctap.get_info", rows)
        self.assertIn("attestation.enterprise", rows)

    def test_ledger_parser_finds_21_rows_and_3_auxiliary_checkpoints(self) -> None:
        entries = self.verifier.parse_ledger_entries(ROOT)
        finding_ids = [entry.identifier for entry in entries if entry.kind == "finding"]
        checkpoint_ids = [entry.identifier for entry in entries if entry.kind == "checkpoint"]

        self.assertEqual(len(finding_ids), 21)
        self.assertEqual(len(checkpoint_ids), 3)
        self.assertIn("transport.u2f_apdu_validation", finding_ids)
        self.assertIn("aux.live-proof-boundary", checkpoint_ids)

    def test_rejects_missing_end_of_doc_summary_section(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(original, "## End-of-doc summary", "## Final notes")
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing heading 'End-of-doc summary'", result.stderr)

    def test_rejects_summary_that_omits_browser_gap_label(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(original, "- Open browser/hardware gaps:", "- Open verification gaps:")
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing summary label 'Open browser/hardware gaps'", result.stderr)

    def test_rejects_summary_open_failures_that_disagree_with_verdicts(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- Current audit failures still open: none.",
            "- Current audit failures still open: `transport.init`.",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Summary/current verdict drift", result.stderr)

    def test_rejects_malformed_manifest_json(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        (temp_root / "host_tools/conformance_manifest.json").write_text("{\n", encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Malformed JSON in host_tools/conformance_manifest.json", result.stderr)

    def test_rejects_missing_required_s02_row(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        ledger_path.write_text(remove_entry(original, "transport.cancel"), encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing S02 row ID in docs/19-current-state-protocol-conformance.md: transport.cancel", result.stderr)

    def test_rejects_duplicate_row_ids(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "### Finding transport.ping: CTAPHID_PING",
            "### Finding transport.init: CTAPHID_PING",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Duplicate row ID in docs/19-current-state-protocol-conformance.md: transport.init", result.stderr)

    def test_rejects_unsupported_proof_label(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- Proof label: `source-proven`",
            "- Proof label: `lab-proven`",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsupported proof label 'lab-proven' for transport.init", result.stderr)

    def test_rejects_missing_current_evidence_field(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- Current code/test evidence: `src/transport/usb_hid_session.c` reserves `0xffffffff` and `0x00000000`, allocates fresh CIDs through `zf_transport_allocate_cid()`, rejects direct `INIT` on unallocated non-broadcast CIDs, special-cases same-CID resync both in `zf_transport_handle_processing_control()` and in the `transport->processing` path of `zf_handle_packet()`, and reclaims the least-recently-used inactive slot when `ZF_MAX_ALLOCATED_CIDS` (8) is full. Fresh local evidence in this slice is limited to `uv run python -m unittest tests.host_tools.test_ctaphid_probe`, plus `uv run python tools/run_protocol_regressions.py`, which keeps the native transport regressions current; the attached-device `transport_exhaust_cids` / `transport_resync` probes were not rerun.",
            "- Current code/test evidence:",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Missing field 'Current code/test evidence' for transport.init in docs/19-current-state-protocol-conformance.md",
            result.stderr,
        )

    def test_rejects_missing_required_citation(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        ledger_path = temp_root / "docs/19-current-state-protocol-conformance.md"
        original = ledger_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "- External reference: U2F HID Protocol v1.1 `U2FHID_INIT` and reserved-CID rules; CTAP 2.2 USB HID channel allocation and same-channel resynchronization guidance; `docs/18-current-state-revalidation-map.md` findings 4 and 5.",
            "- External reference: local note only.",
        )
        ledger_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Missing required citation for transport.init in docs/19-current-state-protocol-conformance.md: External reference",
            result.stderr,
        )

    def test_rejects_inventory_surface_drift_for_s02_row(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        inventory_path = temp_root / "docs/16-current-state-claim-inventory.md"
        original = inventory_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "| `transport.init` | CTAPHID and U2F transport | CTAPHID_INIT | `docs/13-fido-audit-matrix.md` + `host_tools/conformance_manifest.json` | `aligned` | yes | `transport_init`, `transport_exhaust_cids`, `transport_resync` | `src/transport/usb_hid_session.c` | transport code read + manifest scenario replay | `S02 Protocol conformance re-audit` |",
            "| `transport.init` | CTAPHID and U2F transport | CTAPHID_INIT_DRIFT | `docs/13-fido-audit-matrix.md` + `host_tools/conformance_manifest.json` | `aligned` | yes | `transport_init`, `transport_exhaust_cids`, `transport_resync` | `src/transport/usb_hid_session.c` | transport code read + manifest scenario replay | `S02 Protocol conformance re-audit` |",
        )
        inventory_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Inventory surface drift for S02-owned row transport.init", result.stderr)

    def test_rejects_taxonomy_drift_that_removes_required_field(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        taxonomy_path = temp_root / "docs/17-current-state-proof-taxonomy.md"
        original = taxonomy_path.read_text(encoding="utf-8")
        mutated = replace_once(
            original,
            "| `External reference` | Anchors the finding against a spec rule, release-policy statement, metadata boundary, or historical remediation note instead of private interpretation. |\n",
            "",
        )
        taxonomy_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--strict", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing required finding field 'External reference' in docs/17-current-state-proof-taxonomy.md", result.stderr)


if __name__ == "__main__":
    unittest.main()
