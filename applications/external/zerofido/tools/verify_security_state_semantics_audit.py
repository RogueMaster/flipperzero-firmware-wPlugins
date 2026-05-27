"""Validate the security/state-semantics audit ledger.

This focuses on ClientPIN, credential state, counters, storage semantics, and
metadata rows, ensuring each ledger entry cites the expected inventory claims,
proof IDs, manifest scenarios, and slice ownership.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

LEDGER_PATH = "docs/20-current-state-security-state-semantics.md"
INVENTORY_PATH = "docs/16-current-state-claim-inventory.md"
MANIFEST_PATH = "host_tools/conformance_manifest.json"
TAXONOMY_PATH = "docs/17-current-state-proof-taxonomy.md"
REVALIDATION_MAP_PATH = "docs/18-current-state-revalidation-map.md"
SLICE_NAME = "S03 Security and state-semantics audit"

CLIENTPIN_SECTION = "ClientPIN and UV rows"
QUEUE_SECTION = "Queue and approval ownership rows"
COUNTERS_SECTION = "Counters and persistence checkpoints"
ATTESTATION_SECTION = "Attestation and metadata trust-boundary rows"
AUX_SECTION = "Auxiliary security/state checkpoints"
SUMMARY_SECTION = "End-of-doc summary"
SUMMARY_LABELS = (
    "Rows currently settled by current evidence",
    "Rows that still stop at source/local proof",
    "Historical revalidation anchors still open",
    "Open browser/hardware gaps",
)
REQUIRED_VERSION_SURFACES = {
    "identity.aaguid": "ZeroFIDO AAGUID",
    "identity.attestation_subjects": "Public attestation subject names",
}

CHECKPOINTS = {
    "aux.ctap-sign-count-ordering": {
        "title": "CTAP sign-count persistence ordering",
        "section": COUNTERS_SECTION,
    },
    "aux.credential-store-recovery": {
        "title": "Credential-store seal and temp/backup recovery",
        "section": COUNTERS_SECTION,
    },
    "aux.u2f-counter-durability": {
        "title": "U2F counter durability gap",
        "section": COUNTERS_SECTION,
    },
    "aux.proof-taxonomy-boundary": {
        "title": "Proof-label boundary",
        "section": AUX_SECTION,
    },
    "aux.revalidation-map-linkage": {
        "title": "Historical-bucket linkage",
        "section": AUX_SECTION,
    },
    "aux.s02-handoff-boundary": {
        "title": "S02 handoff consumption boundary",
        "section": AUX_SECTION,
    },
    "aux.live-proof-boundary": {
        "title": "Browser and hardware proof boundary",
        "section": AUX_SECTION,
    },
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
REQUIRED_HISTORICAL_LINK_ROWS = {
    "transport.cancel",
    "ctap.make_credential_rk",
    "ctap.make_credential_nonresident",
    "ctap.exclude_list",
    "ctap.get_assertion_allow_list",
    "ctap.get_assertion_discoverable",
    "ctap.multiple_assertions",
    "clientpin.get_key_agreement",
    "clientpin.set_pin",
    "clientpin.change_pin",
    "clientpin.get_pin_token",
    "clientpin.retry_lockout",
    "attestation.format",
    "attestation.aaguid",
    "metadata.clientpin",
    "identity.aaguid",
}


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



def extract_first_fenced_code_block(section_text: str, *, heading: str, path: str) -> str:
    match = re.search(r"```(?:[A-Za-z0-9_-]+)?\n(.*?)\n```", section_text, flags=re.DOTALL)
    if match is None:
        raise VerificationError(f"Missing fenced code block under '{heading}' in {path}")
    return match.group(1)



def extract_summary_section(root: Path) -> str:
    text = read_text(root, LEDGER_PATH)
    return extract_section_after_heading(text, SUMMARY_SECTION, LEDGER_PATH)



def load_manifest_rows(root: Path) -> dict[str, dict[str, str]]:
    manifest = read_json(root, MANIFEST_PATH)
    if not isinstance(manifest, dict):
        raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: top level must be an object")
    rows = manifest.get("rows")
    if not isinstance(rows, list):
        raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: 'rows' must be a list")

    normalized: dict[str, dict[str, str]] = {}
    required_keys = {"id", "section", "surface"}
    for entry in rows:
        if not isinstance(entry, dict):
            raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: each row must be an object")
        missing = required_keys - entry.keys()
        if missing:
            raise VerificationError(
                f"Malformed JSON in {MANIFEST_PATH}: row missing keys {', '.join(sorted(missing))}"
            )
        row_id = entry["id"]
        if not isinstance(row_id, str) or not row_id:
            raise VerificationError(f"Malformed JSON in {MANIFEST_PATH}: row id must be a non-empty string")
        if row_id in normalized:
            raise VerificationError(f"Duplicate manifest row ID in {MANIFEST_PATH}: {row_id}")
        normalized[row_id] = {
            "section": collapse_whitespace(str(entry["section"])),
            "surface": normalize_markdown_cell(str(entry["surface"])),
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
            raise VerificationError(f"Missing reusable finding template field '{field}' in {TAXONOMY_PATH}")
    return proof_labels, verdicts



def inventory_rows_for_slice(root: Path) -> dict[str, dict[str, str]]:
    text = read_text(root, INVENTORY_PATH)
    manifest_rows = extract_table_after_heading(text, "Manifest-backed protocol surfaces", INVENTORY_PATH)
    version_rows = extract_table_after_heading(text, "Version and identity surfaces", INVENTORY_PATH)
    manifest_by_id = load_manifest_rows(root)

    selected: dict[str, dict[str, str]] = {}
    for row in manifest_rows:
        row_id = normalize_markdown_cell(row.get("Row ID", ""))
        if not row_id:
            raise VerificationError(f"Malformed manifest inventory row in {INVENTORY_PATH}: missing Row ID")
        downstream = normalize_markdown_cell(row.get("Downstream slices", ""))
        if SLICE_NAME not in downstream:
            continue
        manifest_row = manifest_by_id.get(row_id)
        if manifest_row is None:
            raise VerificationError(f"Missing manifest row for S03-owned inventory row {row_id}")
        section = normalize_markdown_cell(row.get("Matrix section", ""))
        surface = normalize_markdown_cell(row.get("Matrix surface", ""))
        if section != manifest_row["section"]:
            raise VerificationError(
                f"Inventory section drift for S03-owned row {row_id}: expected {manifest_row['section']}, found {section}"
            )
        if surface != manifest_row["surface"]:
            raise VerificationError(
                f"Inventory surface drift for S03-owned row {row_id}: expected {manifest_row['surface']}, found {surface}"
            )
        selected[row_id] = {"surface": surface}

    for row in version_rows:
        row_id = normalize_markdown_cell(row.get("Inventory ID", ""))
        if not row_id:
            raise VerificationError(f"Malformed version/identity inventory row in {INVENTORY_PATH}: missing Inventory ID")
        downstream = normalize_markdown_cell(row.get("Downstream slices", ""))
        if SLICE_NAME not in downstream:
            continue
        surface = normalize_markdown_cell(row.get("Surface", ""))
        if not surface:
            raise VerificationError(f"Malformed version/identity inventory row in {INVENTORY_PATH}: missing Surface for {row_id}")
        expected_surface = REQUIRED_VERSION_SURFACES.get(row_id)
        if expected_surface is not None and surface != expected_surface:
            raise VerificationError(
                f"Inventory surface drift for S03-owned row {row_id}: expected {expected_surface}, found {surface}"
            )
        if row_id in selected:
            raise VerificationError(f"Duplicate S03-owned inventory row ID in {INVENTORY_PATH}: {row_id}")
        selected[row_id] = {"surface": surface}

    if not selected:
        raise VerificationError(f"No S03-owned rows found in {INVENTORY_PATH}")
    return selected



def expected_section_for_row(row_id: str) -> str:
    if row_id.startswith("clientpin."):
        return CLIENTPIN_SECTION
    if row_id in {
        "transport.cancel",
        "ctap.make_credential_rk",
        "ctap.make_credential_nonresident",
        "ctap.exclude_list",
        "ctap.get_assertion_allow_list",
        "ctap.get_assertion_discoverable",
        "ctap.multiple_assertions",
    }:
        return QUEUE_SECTION
    if row_id.startswith("attestation.") or row_id.startswith("metadata.") or row_id.startswith("identity."):
        return ATTESTATION_SECTION
    raise VerificationError(f"Unmapped S03 row ID in verifier grouping: {row_id}")



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
        current_entry.fields = {key: collapse_whitespace(value) for key, value in current_entry.fields.items()}
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
                raise VerificationError(f"Malformed field line in {LEDGER_PATH} for {current_entry.identifier}: {line}")
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



def validate_non_placeholder(value: str) -> bool:
    normalized = normalize_markdown_cell(value).lower()
    return normalized not in DISALLOWED_PLACEHOLDERS



def extract_backticked_items(value: str) -> list[str]:
    return [item.strip() for item in re.findall(r"`([^`]+)`", value) if item.strip()]



def extract_path_references(value: str) -> list[str]:
    references: list[str] = []
    for item in extract_backticked_items(value):
        if item in {"README.md", "application.fam", "pyproject.toml"}:
            references.append(item)
            continue
        if item.startswith(("docs/", "src/", "tests/", "tools/", "host_tools/")):
            references.append(item)
    return references



def require_existing_paths_in_field(value: str, *, identifier: str, field: str, root: Path, errors: list[str]) -> None:
    for relative_path in extract_path_references(value):
        if not (root / relative_path).exists():
            errors.append(
                f"Missing source path referenced by field '{field}' for {identifier} in {LEDGER_PATH}: {relative_path}"
            )



def verify_ledger(root: Path) -> list[str]:
    try:
        expected_rows = inventory_rows_for_slice(root)
        proof_labels, verdicts = load_supported_values(root)
        entries = parse_ledger_entries(root)
        summary_text = extract_summary_section(root)
    except VerificationError as exc:
        return [str(exc)]

    errors: list[str] = []

    section_names = {entry.section for entry in entries}
    for required_section in {CLIENTPIN_SECTION, QUEUE_SECTION, COUNTERS_SECTION, ATTESTATION_SECTION, AUX_SECTION}:
        if required_section not in section_names:
            errors.append(f"Missing section '{required_section}' in {LEDGER_PATH}")

    for label in SUMMARY_LABELS:
        if label not in summary_text:
            errors.append(f"Missing summary label '{label}' in {LEDGER_PATH}")

    seen_rows: set[str] = set()
    seen_checkpoints: set[str] = set()
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
            expected_checkpoint = CHECKPOINTS.get(entry.identifier)
            if expected_checkpoint is None:
                errors.append(f"Unexpected auxiliary checkpoint ID in {LEDGER_PATH}: {entry.identifier}")
                continue
            if entry.section != expected_checkpoint["section"]:
                errors.append(
                    f"Section mismatch for auxiliary checkpoint {entry.identifier} in {LEDGER_PATH}: expected '{expected_checkpoint['section']}', found '{entry.section}'"
                )
            if entry.title != expected_checkpoint["title"]:
                errors.append(
                    f"Title mismatch for auxiliary checkpoint {entry.identifier} in {LEDGER_PATH}: expected '{expected_checkpoint['title']}', found '{entry.title}'"
                )
        else:
            errors.append(f"Unsupported ledger entry kind in {LEDGER_PATH}: {entry.kind}")
            continue

        for field in REQUIRED_FINDING_FIELDS:
            value = entry.fields.get(field, "")
            if not normalize_markdown_cell(value):
                errors.append(f"Missing field '{field}' for {entry.identifier} in {LEDGER_PATH}")
                continue
            if field in {"Local claim", "Current code/test evidence", "External reference", "Impact", "Revalidation / next check"}:
                if not validate_non_placeholder(value):
                    errors.append(f"Blank or vague field '{field}' for {entry.identifier} in {LEDGER_PATH}")
            require_existing_paths_in_field(value, identifier=entry.identifier, field=field, root=root, errors=errors)

        verdict_value = normalize_markdown_cell(entry.fields.get("Verdict", ""))
        if verdict_value and verdict_value not in verdicts:
            errors.append(f"Unsupported verdict '{verdict_value}' for {entry.identifier} in {LEDGER_PATH}")

        proof_value = normalize_markdown_cell(entry.fields.get("Proof label", ""))
        if proof_value and proof_value not in proof_labels:
            errors.append(f"Unsupported proof label '{proof_value}' for {entry.identifier} in {LEDGER_PATH}")

        if entry.kind == "finding" and entry.identifier in REQUIRED_HISTORICAL_LINK_ROWS:
            external_reference = entry.fields.get("External reference", "")
            if REVALIDATION_MAP_PATH not in external_reference:
                errors.append(f"Missing historical revalidation citation for {entry.identifier} in {LEDGER_PATH}")

    missing_rows = sorted(set(expected_rows) - seen_rows)
    for row_id in missing_rows:
        errors.append(f"Missing S03 row ID in {LEDGER_PATH}: {row_id}")

    missing_checkpoints = sorted(set(CHECKPOINTS) - seen_checkpoints)
    for checkpoint_id in missing_checkpoints:
        errors.append(f"Missing auxiliary checkpoint ID in {LEDGER_PATH}: {checkpoint_id}")

    return errors



def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify the S03 security/state-semantics ledger and fail-closed structure.")
    parser.add_argument("--strict", action="store_true", help="run the security/state-semantics audit checks")
    parser.add_argument("--root", help="override the repository root for tests")
    args = parser.parse_args(argv)
    if not args.strict:
        parser.error("select --strict")
    return args



def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    root = repo_root(args.root)
    errors = verify_ledger(root)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print("strict: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
