from __future__ import annotations

import subprocess
import sys
import textwrap
import unittest
from pathlib import Path

from tests.harness import ROOT, load_module, missing_fixture_paths, stage_temp_repo as stage_repo

SCRIPT_PATH = ROOT / "tools" / "verify_current_state_audit_frame.py"
FIXTURE_PATHS = [
    "README.md",
    "application.fam",
    "pyproject.toml",
    "docs/09-milestone-8-clientpin.md",
    "docs/10-release-criteria.md",
    "docs/11-attestation.md",
    "docs/12-metadata.md",
    "docs/12-metadata-statement.json",
    "docs/13-fido-audit-matrix.md",
    "docs/14-protocol-audit-report.md",
    "docs/15-milestone-10-protocol-remediation.md",
    "docs/16-current-state-claim-inventory.md",
    "docs/17-current-state-proof-taxonomy.md",
    "docs/18-current-state-revalidation-map.md",
    "host_tools/conformance_manifest.json",
    "host_tools/conformance_suite.py",
    "src/u2f/apdu.c",
    "src/u2f/adapter.c",
    "src/u2f/session.c",
    "src/zerofido_attestation.c",
    "src/zerofido_ctap_dispatch.c",
    "src/ctap/parse/get_assertion.c",
    "src/ctap/parse/make_credential.c",
    "src/ctap/response.c",
    "src/pin/command.c",
    "src/pin/flow.c",
    "src/transport/dispatch.c",
    "src/transport/usb_hid_session.c",
    "src/transport/usb_hid_worker.c",
    "src/ui/status.c",
    "src/ui/views.c",
    "tests/native/protocol/runner.c",
    "tests/host_tools/test_ctaphid_probe.py",
]


def load_verifier_module():
    return load_module("verify_current_state_audit_frame", SCRIPT_PATH)


def stage_temp_repo():
    return stage_repo(FIXTURE_PATHS)


class CurrentStateAuditFrameTests(unittest.TestCase):
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

    def test_inventory_verifier_passes_for_repo(self) -> None:
        result = self.run_verifier("--inventory")

        self.assertEqual(result.returncode, 0, msg=result.stderr or result.stdout)
        self.assertIn("inventory: ok", result.stdout)

    def test_proof_taxonomy_verifier_passes_for_repo(self) -> None:
        result = self.run_verifier("--proof-taxonomy")

        self.assertEqual(result.returncode, 0, msg=result.stderr or result.stdout)
        self.assertIn("proof-taxonomy: ok", result.stdout)

    def test_revalidation_map_verifier_passes_for_repo(self) -> None:
        result = self.run_verifier("--revalidation-map")

        self.assertEqual(result.returncode, 0, msg=result.stderr or result.stdout)
        self.assertIn("revalidation-map: ok", result.stdout)

    def test_matrix_parser_normalizes_backticks_and_escaped_pipes(self) -> None:
        rows = self.verifier.load_matrix_rows(ROOT)
        row_keys = {(row["section"], row["surface"]) for row in rows}

        self.assertIn(("CTAPHID and U2F transport", "CTAPHID_INIT"), row_keys)
        self.assertIn(("CTAPHID and U2F transport", "Capability bits"), row_keys)
        self.assertEqual(len(rows), 32)

    def test_manifest_inventory_handles_empty_scenarios_explicitly(self) -> None:
        inventory_tables = self.verifier.load_inventory_tables(ROOT)
        manifest_rows = {
            self.verifier.normalize_markdown_cell(row["Row ID"]): row
            for row in inventory_tables["manifest"]
        }

        chooser_row = manifest_rows["ctap.account_chooser"]
        self.assertEqual(self.verifier.parse_scenario_cell(chooser_row["Scenario IDs"]), [])
        self.assertEqual(self.verifier.normalize_markdown_cell(chooser_row["Required"]), "no")

    def test_historical_findings_parser_extracts_all_15_rows(self) -> None:
        findings = self.verifier.extract_historical_findings(
            (ROOT / "docs/14-protocol-audit-report.md").read_text(encoding="utf-8"),
            "docs/14-protocol-audit-report.md",
        )

        self.assertEqual(len(findings), 15)
        self.assertEqual(
            findings[1],
            'authenticatorMakeCredential accepts missing clientDataHash',
        )
        self.assertEqual(
            findings[15],
            'Metadata version identity drifts from the shipped app version surface',
        )

    def test_revalidation_map_uses_distinct_boundary_buckets(self) -> None:
        rows = self.verifier.extract_table_after_heading(
            (ROOT / "docs/18-current-state-revalidation-map.md").read_text(encoding="utf-8"),
            "Historical finding ledger",
            "docs/18-current-state-revalidation-map.md",
        )
        verdicts = {
            self.verifier.normalize_markdown_cell(row["Current verdict bucket"])
            for row in rows
        }

        self.assertIn("partially re-proven", verdicts)
        self.assertIn("hardware needed", verdicts)
        self.assertIn("claim drift closed", verdicts)

    def test_rejects_malformed_manifest_json(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        (temp_root / "host_tools/conformance_manifest.json").write_text("{\n", encoding="utf-8")

        result = self.run_verifier("--inventory", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Malformed JSON in host_tools/conformance_manifest.json", result.stderr)

    def test_rejects_missing_matrix_row_with_manifest_row_id(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        matrix_path = temp_root / "docs/13-fido-audit-matrix.md"
        original = matrix_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| `CTAPHID_INIT` | allocates unique non-reserved channels on broadcast requests, requires allocated channels for subsequent traffic, serializes requests while a message is assembling or approval-bound processing, accepts same-CID resync during assembly and during the blocked approval path, reclaims the least-recently-used inactive allocated CID when the bounded table fills, and resets channel allocation state on USB reconnect | matches the serialized transport model for the implemented surface | acceptable | aligned |\n",
            "",
        )
        matrix_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--inventory", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("transport.init", result.stderr)
        self.assertIn("Missing matrix row", result.stderr)

    def test_rejects_duplicate_inventory_ids(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        inventory_path = temp_root / "docs/16-current-state-claim-inventory.md"
        original = inventory_path.read_text(encoding="utf-8")
        duplicated = original.replace(
            "| `claim.readme` | `README.md` | Top-level shipped-surface summary, build/validation framing, dev-suite positioning, attestation boundary, and protocol-audit framing | non-manifest document inventory ID |\n",
            textwrap.dedent(
                """\
                | `claim.readme` | `README.md` | Top-level shipped-surface summary, build/validation framing, dev-suite positioning, attestation boundary, and protocol-audit framing | non-manifest document inventory ID |
                | `claim.readme` | `README.md` | Duplicate row injected by unit test | non-manifest document inventory ID |
                """
            ),
            1,
        )
        inventory_path.write_text(duplicated, encoding="utf-8")

        result = self.run_verifier("--inventory", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Duplicate inventory ID", result.stderr)
        self.assertIn("claim.readme", result.stderr)

    def test_rejects_missing_claim_source_file(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        (temp_root / "docs/11-attestation.md").unlink()

        result = self.run_verifier("--inventory", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing source path: docs/11-attestation.md", result.stderr)

    def test_rejects_blank_manifest_owner_files(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        inventory_path = temp_root / "docs/16-current-state-claim-inventory.md"
        original = inventory_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| `transport.init` | CTAPHID and U2F transport | CTAPHID_INIT | `docs/13-fido-audit-matrix.md` + `host_tools/conformance_manifest.json` | `aligned` | yes | `transport_init`, `transport_exhaust_cids`, `transport_resync` | `src/transport/usb_hid_session.c` | transport code read + manifest scenario replay | `S02 Protocol conformance re-audit` |\n",
            "| `transport.init` | CTAPHID and U2F transport | CTAPHID_INIT | `docs/13-fido-audit-matrix.md` + `host_tools/conformance_manifest.json` | `aligned` | yes | `transport_init`, `transport_exhaust_cids`, `transport_resync` |  | transport code read + manifest scenario replay | `S02 Protocol conformance re-audit` |\n",
            1,
        )
        inventory_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--inventory", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing Owner files for manifest-backed protocol surfaces row transport.init", result.stderr)

    def test_rejects_blank_version_proof_source_and_downstream_slice(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        inventory_path = temp_root / "docs/16-current-state-claim-inventory.md"
        original = inventory_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| `version.pyproject` | `pyproject.toml` | Python tooling semantic version | `1.0.0` | source-of-truth version for Python package metadata | `pyproject.toml` | package metadata read + release/docs cross-check | `S04 Release-claims and documentation truthfulness audit` |\n",
            "| `version.pyproject` | `pyproject.toml` | Python tooling semantic version | `1.0.0` | source-of-truth version for Python package metadata | `pyproject.toml` |  |  |\n",
            1,
        )
        inventory_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--inventory", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing Proof source for version and identity surfaces row version.pyproject", result.stderr)
        self.assertIn("Missing Downstream slices for version and identity surfaces row version.pyproject", result.stderr)

    def test_rejects_unsupported_proof_label(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        taxonomy_path = temp_root / "docs/17-current-state-proof-taxonomy.md"
        original = taxonomy_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| `source-proven` | The claim is limited to what the tracked source tree, shipped docs, metadata, or static assets currently say or encode. | Cite the current files, functions, tables, or assets that were read in the current tree. | Do not use it to claim live browser interoperability, hardware semantics, or physical security properties that were not exercised. |\n",
            "| `lab-proven` | The claim is limited to what the tracked source tree, shipped docs, metadata, or static assets currently say or encode. | Cite the current files, functions, tables, or assets that were read in the current tree. | Do not use it to claim live browser interoperability, hardware semantics, or physical security properties that were not exercised. |\n",
            1,
        )
        taxonomy_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--proof-taxonomy", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsupported proof label 'lab-proven'", result.stderr)
        self.assertIn("Missing proof label 'source-proven'", result.stderr)

    def test_rejects_missing_required_template_field(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        taxonomy_path = temp_root / "docs/17-current-state-proof-taxonomy.md"
        original = taxonomy_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| `External reference` | Anchors the finding against a spec rule, release-policy statement, metadata boundary, or historical remediation note instead of private interpretation. |\n",
            "",
            1,
        )
        taxonomy_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--proof-taxonomy", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing template field 'External reference'", result.stderr)

    def test_rejects_missing_grounding_reference(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        taxonomy_path = temp_root / "docs/17-current-state-proof-taxonomy.md"
        original = taxonomy_path.read_text(encoding="utf-8")
        mutated = original.replace("Decision `D004`", "Decision `DX04`", 1)
        taxonomy_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--proof-taxonomy", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing grounding reference 'D004'", result.stderr)

    def test_rejects_missing_revalidation_map_row(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        map_path = temp_root / "docs/18-current-state-revalidation-map.md"
        original = map_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| 15 | Metadata version identity drifts from the shipped app version surface | `no batch — version-surface drift is already closed in the current tree` | `docs/12-metadata.md`, `docs/12-metadata-statement.json`, `application.fam`, `pyproject.toml` | `rg -n \"fap_version|version = \\\"1.0.0\\\"|authenticatorVersion|firmwareVersion|10000\" application.fam pyproject.toml docs/12-metadata.md docs/12-metadata-statement.json` | `application.fam`, `pyproject.toml`, `docs/12-metadata.md`, and `docs/12-metadata-statement.json` all now align on `1.0` / `1.0.0` / `10000` | `claim drift closed` | The current version and metadata identity surfaces are now aligned, so this historical drift should only re-open if those tracked files diverge again. |\n",
            "",
            1,
        )
        map_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--revalidation-map", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing revalidation-map row for finding 15", result.stderr)

    def test_rejects_duplicate_revalidation_finding_number(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        map_path = temp_root / "docs/18-current-state-revalidation-map.md"
        original = map_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| 15 | Metadata version identity drifts from the shipped app version surface |",
            "| 14 | Metadata version identity drifts from the shipped app version surface |",
            1,
        )
        map_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--revalidation-map", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Duplicate finding number 14", result.stderr)

    def test_rejects_unsupported_revalidation_verdict_bucket(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        map_path = temp_root / "docs/18-current-state-revalidation-map.md"
        original = map_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| 8 | U2F authentication counter advances in memory even if persistence fails | `Batch 5` | `src/u2f/session.c` | `uv run python -m ufbt` | `docs/15-milestone-10-protocol-remediation.md` Batch 5 notes and the current counter-update logic in `src/u2f/session.c` | `not re-proven` | Batch 5 says the durability work landed, but M001 does not yet include a tracked torn-write or reboot-oriented regression that directly retires the original counter-failure mode. |\n",
            "| 8 | U2F authentication counter advances in memory even if persistence fails | `Batch 5` | `src/u2f/session.c` | `uv run python -m ufbt` | `docs/15-milestone-10-protocol-remediation.md` Batch 5 notes and the current counter-update logic in `src/u2f/session.c` | `fixed` | Batch 5 says the durability work landed, but M001 does not yet include a tracked torn-write or reboot-oriented regression that directly retires the original counter-failure mode. |\n",
            1,
        )
        map_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--revalidation-map", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsupported current verdict bucket 'fixed'", result.stderr)

    def test_rejects_blank_revalidation_owner_files(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        map_path = temp_root / "docs/18-current-state-revalidation-map.md"
        original = map_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| `src/zerofido_ctap_dispatch.c` | `uv run python tools/run_protocol_regressions.py` |",
            "|  | `uv run python tools/run_protocol_regressions.py` |",
            1,
        )
        map_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--revalidation-map", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing current owner files for revalidation-map finding 3", result.stderr)

    def test_rejects_blank_revalidation_recheck_command(self) -> None:
        tempdir, temp_root = stage_temp_repo()
        self.addCleanup(tempdir.cleanup)
        map_path = temp_root / "docs/18-current-state-revalidation-map.md"
        original = map_path.read_text(encoding="utf-8")
        mutated = original.replace(
            "| `src/zerofido_ctap_dispatch.c`, `src/ctap/response.c` | `uv run python host_tools/ctaphid_probe.py --cmd getassertion --silent` and `uv run python host_tools/ctaphid_probe.py --cmd getnextassertion` |",
            "| `src/zerofido_ctap_dispatch.c`, `src/ctap/response.c` |  |",
            1,
        )
        map_path.write_text(mutated, encoding="utf-8")

        result = self.run_verifier("--revalidation-map", root=temp_root)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing fresh re-check command for revalidation-map finding 6", result.stderr)


if __name__ == "__main__":
    unittest.main()
