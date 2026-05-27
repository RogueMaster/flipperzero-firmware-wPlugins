"""Validate the protocol-conformance audit ledger.

The checker ties ledger rows to the claim inventory, proof taxonomy,
revalidation map, and conformance manifest so protocol evidence cannot drift
from the scenario IDs and slice ownership it claims.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

LEDGER_PATH = "docs/19-current-state-protocol-conformance.md"
INVENTORY_PATH = "docs/16-current-state-claim-inventory.md"
MANIFEST_PATH = "host_tools/conformance_manifest.json"
TAXONOMY_PATH = "docs/17-current-state-proof-taxonomy.md"
SLICE_NAME = "S02 Protocol conformance re-audit"

TRANSPORT_SECTION = "Transport and U2F rows"
CTAP2_SECTION = "CTAP2 rows"
BOUNDARY_SECTION = "Protocol-boundary rows"
AUX_SECTION = "Auxiliary protocol checkpoints"
SUMMARY_SECTION = "End-of-doc summary"
SUMMARY_LABELS = (
    "Supported current-state rows",
    "Rows that still stop at local/source proof",
    "Open browser/hardware gaps",
    "Current audit failures still open",
)
SUMMARY_ROW_IDS = (
    "ctap.multiple_assertions",
    "ctap.account_chooser",
    "attestation.none",
    "attestation.enterprise",
)

BOUNDARY_ROW_IDS = {
    "ctap.multiple_assertions",
    "ctap.account_chooser",
    "clientpin.empty_pin_auth_probe",
    "attestation.none",
    "attestation.enterprise",
}

CTAP2_ROW_IDS = {
    "ctap.get_info",
    "ctap.make_credential_rk",
    "ctap.make_credential_nonresident",
    "ctap.exclude_list",
    "ctap.get_assertion_allow_list",
    "ctap.get_assertion_discoverable",
    "ctap.get_assertion_unsupported",
}

AUX_CHECKPOINTS = {
    "aux.proof-taxonomy-boundary": "Proof-label boundary",
    "aux.revalidation-map-linkage": "Historical-bucket linkage",
    "aux.live-proof-boundary": "Browser and hardware proof boundary",
}

REQUIRED_FINDING_FIELDS = (
    "Local claim",
    "Verdict",
    "Proof label",
    "Current code/test evidence",
    "External reference",
    "Impact",
    "Revalidation / next check",
)
TAXONOMY_REQUIRED_FIELDS = (
    "Local claim",
    "Proof label",
    "Current code/test evidence",
    "External reference",
    "Impact",
)

DISALLOWED_PLACEHOLDERS = {
    "",
    "tbd",
    "todo",
    "none",
    "n/a",
    "unknown",
    "same",
    "same as above",
    "see above",
}

TRANSPORT_CITATIONS = ("CTAP 2.0", "CTAP 2.2", "U2F HID Protocol v1.1", "U2F Raw Message Formats v1.1")
CTAP_CITATIONS = ("CTAP 2.0", "CTAP 2.2")
ATTESTATION_CITATIONS = ("CTAP 2.0", "CTAP 2.2", "docs/11-attestation.md", "docs/12-metadata.md")
CHECKPOINT_CITATIONS = (
    "docs/10-release-criteria.md",
    "docs/17-current-state-proof-taxonomy.md",
    "docs/18-current-state-revalidation-map.md",
)


class VerificationError(RuntimeError):
    pass


@dataclass
class LedgerEntry:
    kind: str
    identifier: str
    title: str
    section: str
    fields: dict[str, str]



def repo_root(explicit_root: str | None) -> Path:
    if explicit_root:
        return Path(explicit_root).resolve()
    return Path(__file__).resolve().parents[1]



def read_text(root: Path, relative_path: str) -> str:
    path = root / relative_path
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError as exc:
        raise VerificationError(f"Missing source path: {relative_path}") from exc



def read_json(root: Path, relative_path: str) -> Any:
    path = root / relative_path
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise VerificationError(f"Missing source path: {relative_path}") from exc
    except json.JSONDecodeError as exc:
        raise VerificationError(
            f"Malformed JSON in {relative_path}: line {exc.lineno}, column {exc.colno}: {exc.msg}"
        ) from exc



def collapse_whitespace(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()



def strip_code_ticks(value: str) -> str:
    return value.replace("`", "")



def normalize_markdown_cell(value: str) -> str:
    return collapse_whitespace(strip_code_ticks(value).replace("\\|", "|"))



def split_markdown_row(line: str) -> list[str]:
    stripped = line.strip()
    if not stripped.startswith("|"):
        raise VerificationError(f"Malformed markdown row: {line}")
    if stripped.endswith("|"):
        stripped = stripped[:-1]
    stripped = stripped[1:]

    cells: list[str] = []
    current: list[str] = []
    in_code = False
    escape = False
    for ch in stripped:
        if escape:
            current.append(ch)
            escape = False
            continue
        if ch == "\\":
            current.append(ch)
            escape = True
            continue
        if ch == "`":
            current.append(ch)
            in_code = not in_code
            continue
        if ch == "|" and not in_code:
            cells.append("".join(current).strip())
            current = []
            continue
        current.append(ch)
    cells.append("".join(current).strip())
    return cells



def is_separator_row(cells: list[str]) -> bool:
    if not cells:
        return False
    for cell in cells:
        normalized = cell.replace(":", "").replace(" ", "")
        if not normalized or any(ch != "-" for ch in normalized):
            return False
    return True



def extract_section_after_heading(text: str, heading: str, path: str) -> str:
    lines = text.splitlines()
    heading_line = f"## {heading}"
    try:
        start = lines.index(heading_line)
    except ValueError as exc:
        raise VerificationError(f"Missing heading '{heading}' in {path}") from exc

    section_lines: list[str] = []
    for line in lines[start + 1 :]:
        if line.startswith("## "):
            break
        section_lines.append(line)
    return "\n".join(section_lines).strip()



def extract_table_after_heading(text: str, heading: str, path: str) -> list[dict[str, str]]:
    section_text = extract_section_after_heading(text, heading, path)
    table_lines = [line for line in section_text.splitlines() if line.strip().startswith("|")]
    if len(table_lines) < 2:
        raise VerificationError(f"Missing markdown table under '{heading}' in {path}")

    header = split_markdown_row(table_lines[0])
    separator = split_markdown_row(table_lines[1])
    if len(header) != len(separator) or not is_separator_row(separator):
        raise VerificationError(f"Malformed markdown table under '{heading}' in {path}")

    rows: list[dict[str, str]] = []
    for line in table_lines[2:]:
        cells = split_markdown_row(line)
        if len(cells) != len(header):
            raise VerificationError(
                f"Malformed markdown row under '{heading}' in {path}: expected {len(header)} cells, got {len(cells)}"
            )
        rows.append(dict(zip(header, cells, strict=True)))
    return rows



def extract_summary_section(root: Path) -> str:
    text = read_text(root, LEDGER_PATH)
    return extract_section_after_heading(text, SUMMARY_SECTION, LEDGER_PATH)


def parse_summary_row_ids(summary_text: str, label: str) -> set[str]:
    pattern = rf"^- {re.escape(label)}:(.*)$"
    match = re.search(pattern, summary_text, flags=re.MULTILINE)
    if match is None:
        raise VerificationError(f"Missing summary label '{label}' in {LEDGER_PATH}")

    suffix = collapse_whitespace(match.group(1))
    if not suffix:
        return set()

    normalized_suffix = suffix.rstrip(".").strip().lower()
    if normalized_suffix == "none":
        return set()

    return set(re.findall(r"`([a-z0-9_.]+)`", suffix))



def extract_first_fenced_code_block(section_text: str, *, heading: str, path: str) -> str:
    match = re.search(r"```(?:[A-Za-z0-9_-]+)?\n(.*?)\n```", section_text, flags=re.DOTALL)
    if match is None:
        raise VerificationError(f"Missing fenced code block under '{heading}' in {path}")
    return match.group(1)



def load_manifest_rows(root: Path) -> dict[str, dict[str, Any]]:
    manifest = read_json(root, MANIFEST_PATH)
    if not isinstance(manifest, dict):
        raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: top level must be an object")

    rows = manifest.get("rows")
    if not isinstance(rows, list):
        raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: 'rows' must be a list")

    normalized: dict[str, dict[str, Any]] = {}
    required_keys = {"id", "section", "surface", "required", "classification", "scenario_ids"}
    for entry in rows:
        if not isinstance(entry, dict):
            raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: each row must be an object")
        missing = required_keys - entry.keys()
        if missing:
            missing_keys = ", ".join(sorted(missing))
            raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: row missing keys {missing_keys}")
        row_id = entry["id"]
        if not isinstance(row_id, str) or not row_id:
            raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: row id must be a non-empty string")
        if row_id in normalized:
            raise VerificationError(f"Duplicate manifest row ID in {MANIFEST_PATH}: {row_id}")
        scenario_ids = entry["scenario_ids"]
        if not isinstance(scenario_ids, list) or any(not isinstance(item, str) or not item for item in scenario_ids):
            raise VerificationError(
                f"Malformed JSON in {MANIFEST_PATH}: scenario_ids must be a string list for row {row_id}"
            )
        normalized[row_id] = {
            "id": row_id,
            "section": collapse_whitespace(str(entry["section"])),
            "surface": normalize_markdown_cell(str(entry["surface"])),
            "required": bool(entry["required"]),
            "classification": collapse_whitespace(str(entry["classification"])),
            "scenario_ids": scenario_ids,
        }
    return normalized



def load_supported_values(root: Path) -> tuple[set[str], set[str]]:
    text = read_text(root, TAXONOMY_PATH)
    proof_rows = extract_table_after_heading(text, "Allowed proof labels", TAXONOMY_PATH)
    verdict_rows = extract_table_after_heading(text, "Allowed verdict vocabulary", TAXONOMY_PATH)
    field_rows = extract_table_after_heading(text, "Required finding fields", TAXONOMY_PATH)
    template_section = extract_section_after_heading(text, "Reusable finding template", TAXONOMY_PATH)
    template_code_block = extract_first_fenced_code_block(
        template_section,
        heading="Reusable finding template",
        path=TAXONOMY_PATH,
    )

    proof_labels = {
        normalize_markdown_cell(row.get("Proof label", ""))
        for row in proof_rows
        if normalize_markdown_cell(row.get("Proof label", ""))
    }
    verdicts = {
        normalize_markdown_cell(row.get("Verdict", ""))
        for row in verdict_rows
        if normalize_markdown_cell(row.get("Verdict", ""))
    }
    required_fields = {
        normalize_markdown_cell(row.get("Field", ""))
        for row in field_rows
        if normalize_markdown_cell(row.get("Field", ""))
    }
    if not proof_labels:
        raise VerificationError(f"Missing proof labels in {TAXONOMY_PATH}")
    if not verdicts:
        raise VerificationError(f"Missing verdict vocabulary in {TAXONOMY_PATH}")
    for field in TAXONOMY_REQUIRED_FIELDS:
        if field not in required_fields:
            raise VerificationError(f"Missing required finding field '{field}' in {TAXONOMY_PATH}")
    for field in REQUIRED_FINDING_FIELDS:
        if re.search(rf"{re.escape(field)}\s*:", template_code_block) is None:
            raise VerificationError(
                f"Missing reusable finding template field '{field}' in {TAXONOMY_PATH}"
            )
    return proof_labels, verdicts



def inventory_rows_for_slice(root: Path) -> dict[str, dict[str, str]]:
    text = read_text(root, INVENTORY_PATH)
    rows = extract_table_after_heading(text, "Manifest-backed protocol surfaces", INVENTORY_PATH)
    manifest_rows = load_manifest_rows(root)

    selected: dict[str, dict[str, str]] = {}
    for row in rows:
        row_id = normalize_markdown_cell(row.get("Row ID", ""))
        if not row_id:
            raise VerificationError(f"Malformed manifest inventory row in {INVENTORY_PATH}: missing Row ID")
        downstream = normalize_markdown_cell(row.get("Downstream slices", ""))
        if SLICE_NAME not in downstream:
            continue
        if row_id not in manifest_rows:
            raise VerificationError(f"Missing manifest row for S02-owned inventory row {row_id}")
        manifest_row = manifest_rows[row_id]
        section = normalize_markdown_cell(row.get("Matrix section", ""))
        surface = normalize_markdown_cell(row.get("Matrix surface", ""))
        if section != manifest_row["section"]:
            raise VerificationError(
                f"Inventory section drift for S02-owned row {row_id}: expected {manifest_row['section']}, found {section}"
            )
        if surface != manifest_row["surface"]:
            raise VerificationError(
                f"Inventory surface drift for S02-owned row {row_id}: expected {manifest_row['surface']}, found {surface}"
            )
        selected[row_id] = {
            "section": section,
            "surface": surface,
        }

    if not selected:
        raise VerificationError(f"No S02-owned rows found in {INVENTORY_PATH}")
    return selected



def expected_section_for_row(row_id: str) -> str:
    if row_id.startswith("transport."):
        return TRANSPORT_SECTION
    if row_id in CTAP2_ROW_IDS:
        return CTAP2_SECTION
    if row_id in BOUNDARY_ROW_IDS:
        return BOUNDARY_SECTION
    raise VerificationError(f"Unmapped S02 row ID in verifier grouping: {row_id}")



def parse_ledger_entries(root: Path) -> list[LedgerEntry]:
    text = read_text(root, LEDGER_PATH)
    entries: list[LedgerEntry] = []
    current_section = ""
    current_entry: LedgerEntry | None = None
    current_field: str | None = None

    def finalize_entry() -> None:
        nonlocal current_entry, current_field
        if current_entry is None:
            return
        current_entry.fields = {
            key: collapse_whitespace(value)
            for key, value in current_entry.fields.items()
        }
        entries.append(current_entry)
        current_entry = None
        current_field = None

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        if line.startswith("## "):
            finalize_entry()
            current_section = line[3:].strip()
            continue
        if line.startswith("### Finding "):
            finalize_entry()
            match = re.match(r"^### Finding ([^:]+):\s*(.+)$", line)
            if match is None:
                raise VerificationError(f"Malformed finding heading in {LEDGER_PATH}: {line}")
            current_entry = LedgerEntry(
                kind="finding",
                identifier=match.group(1).strip(),
                title=normalize_markdown_cell(match.group(2)),
                section=current_section,
                fields={},
            )
            continue
        if line.startswith("### Checkpoint "):
            finalize_entry()
            match = re.match(r"^### Checkpoint ([^:]+):\s*(.+)$", line)
            if match is None:
                raise VerificationError(f"Malformed checkpoint heading in {LEDGER_PATH}: {line}")
            current_entry = LedgerEntry(
                kind="checkpoint",
                identifier=match.group(1).strip(),
                title=normalize_markdown_cell(match.group(2)),
                section=current_section,
                fields={},
            )
            continue
        if current_entry is None:
            continue
        if line.startswith("- "):
            if ":" not in line:
                raise VerificationError(
                    f"Malformed field line in {LEDGER_PATH} for {current_entry.identifier}: {line}"
                )
            name, value = line[2:].split(":", 1)
            current_field = name.strip()
            current_entry.fields[current_field] = value.strip()
            continue
        if line.startswith("  ") and current_field is not None:
            current_entry.fields[current_field] += f" {line.strip()}"
            continue
        if line.strip() and current_field is not None and not line.startswith("### "):
            current_entry.fields[current_field] += f" {line.strip()}"

    finalize_entry()
    return entries



def normalize_inline_code(value: str) -> str:
    return normalize_markdown_cell(value)



def has_required_citation(entry: LedgerEntry, field_value: str) -> bool:
    if entry.kind == "checkpoint":
        citations = CHECKPOINT_CITATIONS
    elif entry.identifier.startswith("transport."):
        citations = TRANSPORT_CITATIONS
    elif entry.identifier.startswith("attestation."):
        citations = ATTESTATION_CITATIONS
    else:
        citations = CTAP_CITATIONS
    return any(token in field_value for token in citations)



def validate_non_placeholder(value: str) -> bool:
    normalized = normalize_inline_code(value).lower()
    return normalized not in DISALLOWED_PLACEHOLDERS



def verify_ledger(root: Path) -> list[str]:
    errors: list[str] = []
    try:
        expected_rows = inventory_rows_for_slice(root)
        proof_labels, verdicts = load_supported_values(root)
        entries = parse_ledger_entries(root)
        summary_text = extract_summary_section(root)
    except VerificationError as exc:
        return [str(exc)]

    section_names = {entry.section for entry in entries}
    for required_section in {TRANSPORT_SECTION, CTAP2_SECTION, BOUNDARY_SECTION, AUX_SECTION}:
        if required_section not in section_names:
            errors.append(f"Missing section '{required_section}' in {LEDGER_PATH}")

    for label in SUMMARY_LABELS:
        if label not in summary_text:
            errors.append(f"Missing summary label '{label}' in {LEDGER_PATH}")
    for row_id in SUMMARY_ROW_IDS:
        if row_id not in summary_text:
            errors.append(f"Missing summary row reference '{row_id}' in {LEDGER_PATH}")

    try:
        documented_failures = parse_summary_row_ids(summary_text, "Current audit failures still open")
    except VerificationError as exc:
        errors.append(str(exc))
        documented_failures = set()

    seen_rows: set[str] = set()
    seen_checkpoints: set[str] = set()
    audit_failures: set[str] = set()
    for entry in entries:
        if entry.kind == "finding":
            if entry.identifier in seen_rows:
                errors.append(f"Duplicate row ID in {LEDGER_PATH}: {entry.identifier}")
                continue
            seen_rows.add(entry.identifier)
            expected = expected_rows.get(entry.identifier)
            if expected is None:
                errors.append(f"Unexpected row ID in {LEDGER_PATH}: {entry.identifier}")
                continue
            expected_section = expected_section_for_row(entry.identifier)
            if entry.section != expected_section:
                errors.append(
                    f"Section mismatch for row {entry.identifier} in {LEDGER_PATH}: expected '{expected_section}', found '{entry.section}'"
                )
            if entry.title != expected["surface"]:
                errors.append(
                    f"Surface/title mismatch for row {entry.identifier} in {LEDGER_PATH}: expected '{expected['surface']}', found '{entry.title}'"
                )
        elif entry.kind == "checkpoint":
            if entry.identifier in seen_checkpoints:
                errors.append(f"Duplicate auxiliary checkpoint ID in {LEDGER_PATH}: {entry.identifier}")
                continue
            seen_checkpoints.add(entry.identifier)
            expected_title = AUX_CHECKPOINTS.get(entry.identifier)
            if expected_title is None:
                errors.append(f"Unexpected auxiliary checkpoint ID in {LEDGER_PATH}: {entry.identifier}")
                continue
            if entry.section != AUX_SECTION:
                errors.append(
                    f"Section mismatch for auxiliary checkpoint {entry.identifier} in {LEDGER_PATH}: expected '{AUX_SECTION}', found '{entry.section}'"
                )
            if entry.title != expected_title:
                errors.append(
                    f"Title mismatch for auxiliary checkpoint {entry.identifier} in {LEDGER_PATH}: expected '{expected_title}', found '{entry.title}'"
                )
        else:
            errors.append(f"Unsupported ledger entry kind in {LEDGER_PATH}: {entry.kind}")
            continue

        for field in REQUIRED_FINDING_FIELDS:
            value = entry.fields.get(field, "")
            if not normalize_inline_code(value):
                errors.append(f"Missing field '{field}' for {entry.identifier} in {LEDGER_PATH}")
                continue
            if field in {"Current code/test evidence", "External reference", "Impact", "Revalidation / next check", "Local claim"} and not validate_non_placeholder(value):
                errors.append(f"Blank or vague field '{field}' for {entry.identifier} in {LEDGER_PATH}")

        verdict_value = normalize_inline_code(entry.fields.get("Verdict", ""))
        if verdict_value and verdict_value not in verdicts:
            errors.append(f"Unsupported verdict '{verdict_value}' for {entry.identifier} in {LEDGER_PATH}")
        elif verdict_value == "audit-failure":
            audit_failures.add(entry.identifier)

        proof_value = normalize_inline_code(entry.fields.get("Proof label", ""))
        if proof_value and proof_value not in proof_labels:
            errors.append(f"Unsupported proof label '{proof_value}' for {entry.identifier} in {LEDGER_PATH}")

        external_reference = entry.fields.get("External reference", "")
        if external_reference and not has_required_citation(entry, external_reference):
            errors.append(f"Missing required citation for {entry.identifier} in {LEDGER_PATH}: External reference")

        if entry.kind == "finding":
            revalidation_note = f"{entry.fields.get('External reference', '')} {entry.fields.get('Revalidation / next check', '')}"
            if "docs/18-current-state-revalidation-map.md" not in revalidation_note:
                errors.append(
                    f"Missing historical-bucket link for {entry.identifier} in {LEDGER_PATH}: docs/18-current-state-revalidation-map.md"
                )

    missing_rows = sorted(set(expected_rows) - seen_rows)
    for row_id in missing_rows:
        errors.append(f"Missing S02 row ID in {LEDGER_PATH}: {row_id}")

    missing_checkpoints = sorted(set(AUX_CHECKPOINTS) - seen_checkpoints)
    for checkpoint_id in missing_checkpoints:
        errors.append(f"Missing auxiliary checkpoint ID in {LEDGER_PATH}: {checkpoint_id}")

    if documented_failures != audit_failures:
        expected = ", ".join(sorted(audit_failures)) if audit_failures else "none"
        documented = ", ".join(sorted(documented_failures)) if documented_failures else "none"
        errors.append(
            "Summary/current verdict drift in "
            f"{LEDGER_PATH}: 'Current audit failures still open' lists [{documented}] "
            f"but row verdicts require [{expected}]"
        )

    return errors



def run_selected_checks(root: Path, *, strict: bool) -> list[str]:
    if not strict:
        return ["select --strict"]
    return verify_ledger(root)



def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify the S02 protocol conformance ledger and its fail-closed structure.")
    parser.add_argument("--strict", action="store_true", help="run the protocol conformance audit checks")
    parser.add_argument("--root", help="override the repository root for tests")
    args = parser.parse_args(argv)
    if not args.strict:
        parser.error("select --strict")
    return args



def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    root = repo_root(args.root)
    errors = run_selected_checks(root, strict=args.strict)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print("strict: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
