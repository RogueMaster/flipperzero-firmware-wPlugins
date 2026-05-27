"""Validate the current-state audit frame documents.

Checks that the generated claim inventory, proof taxonomy, and revalidation map
agree on public claim sources, proof IDs, summary fields, and expected document
structure.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

REQUIRED_PUBLIC_CLAIM_SOURCES = {
    "claim.readme": "README.md",
    "claim.release_criteria": "docs/10-release-criteria.md",
    "claim.attestation_policy": "docs/11-attestation.md",
    "claim.metadata_notes": "docs/12-metadata.md",
    "claim.metadata_statement": "docs/12-metadata-statement.json",
    "claim.audit_matrix": "docs/13-fido-audit-matrix.md",
    "claim.manifest_registry": "host_tools/conformance_manifest.json",
}

REQUIRED_NON_MANIFEST_SOURCES = {
    "claim.readme": "README.md",
    "claim.release_criteria": "docs/10-release-criteria.md",
    "claim.attestation_policy": "docs/11-attestation.md",
    "claim.metadata_notes": "docs/12-metadata.md",
    "claim.metadata_statement": "docs/12-metadata-statement.json",
    "version.application_fam": "application.fam",
    "version.pyproject": "pyproject.toml",
}

REQUIRED_VERSION_SURFACES = {
    "identity.aaguid": {"docs/11-attestation.md", "docs/12-metadata.md", "docs/12-metadata-statement.json"},
    "identity.attestation_subjects": {"docs/11-attestation.md"},
    "identity.metadata_description": {"docs/12-metadata-statement.json"},
    "version.application_fam": {"application.fam"},
    "version.pyproject": {"pyproject.toml"},
    "version.metadata_integer": {"docs/12-metadata.md", "docs/12-metadata-statement.json"},
}

REQUIRED_MANIFEST_CLUSTER_FIELDS = ("Owner files", "Proof source", "Downstream slices")
REQUIRED_VERSION_CLUSTER_FIELDS = ("Owner files", "Proof source", "Downstream slices")

SUPPORTED_CLASSIFICATIONS = {
    "aligned",
    "spec violation",
    "interop risk",
    "intentional product choice",
    "intentional deviation",
}

PROOF_TAXONOMY_PATH = "docs/17-current-state-proof-taxonomy.md"
REVALIDATION_MAP_PATH = "docs/18-current-state-revalidation-map.md"
REVALIDATION_REPORT_PATH = "docs/14-protocol-audit-report.md"
REMEDIATION_PLAN_PATH = "docs/15-milestone-10-protocol-remediation.md"

REQUIRED_PROOF_LABELS = {
    "source-proven",
    "test-proven",
    "live-risk hypothesis",
    "not proven without hardware",
}
REQUIRED_PROOF_LABEL_FIELDS = ("Legal when", "Required evidence", "Illegal when")

REQUIRED_VERDICTS = {
    "supported-current-state",
    "audit-failure",
    "needs-revalidation",
    "hardware-gap",
}
REQUIRED_VERDICT_FIELDS = ("Meaning", "Closure rule")

REQUIRED_FINDING_TEMPLATE_FIELDS = {
    "Local claim",
    "Proof label",
    "Current code/test evidence",
    "External reference",
    "Impact",
}
REQUIRED_TEMPLATE_CODE_FIELDS = (
    "Local claim",
    "Verdict",
    "Proof label",
    "Current code/test evidence",
    "External reference",
    "Impact",
)
PROOF_TAXONOMY_REQUIRED_REFERENCES = (
    "10-release-criteria.md",
    "12-metadata.md",
    "15-milestone-10-protocol-remediation.md",
    "D003",
    "D004",
    "D005",
)

REQUIRED_REVALIDATION_BUCKETS = {
    "re-proven",
    "partially re-proven",
    "not re-proven",
    "hardware needed",
    "claim drift closed",
}
REQUIRED_REVALIDATION_BUCKET_FIELDS = ("Use when", "Closure rule")
REQUIRED_REVALIDATION_LEDGER_FIELDS = (
    "Current remediation batch",
    "Current owner files",
    "Fresh re-check command",
    "Fresh evidence source",
    "Current verdict bucket",
    "Conservative checkpoint",
)
REQUIRED_REVALIDATION_BOUNDARY_BUCKETS = {
    "partially re-proven",
    "hardware needed",
    "claim drift closed",
}
DISALLOWED_RECHECK_PLACEHOLDERS = {
    "",
    "tbd",
    "todo",
    "none",
    "n/a",
    "same",
    "same as above",
    "see above",
    "unknown",
}


class VerificationError(RuntimeError):
    pass


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


def load_manifest_rows(root: Path) -> list[dict[str, Any]]:
    manifest = read_json(root, "host_tools/conformance_manifest.json")
    if not isinstance(manifest, dict):
        raise VerificationError("Malformed JSON in host_tools/conformance_manifest.json: top level must be an object")
    rows = manifest.get("rows")
    if not isinstance(rows, list):
        raise VerificationError("Malformed JSON in host_tools/conformance_manifest.json: 'rows' must be a list")

    seen_ids: set[str] = set()
    normalized_rows: list[dict[str, Any]] = []
    for entry in rows:
        if not isinstance(entry, dict):
            raise VerificationError("Malformed JSON in host_tools/conformance_manifest.json: each row must be an object")
        missing_keys = {"id", "section", "surface", "required", "classification", "scenario_ids"} - entry.keys()
        if missing_keys:
            missing = ", ".join(sorted(missing_keys))
            raise VerificationError(
                f"Malformed JSON in host_tools/conformance_manifest.json: row missing keys {missing}"
            )
        row_id = entry["id"]
        if not isinstance(row_id, str) or not row_id:
            raise VerificationError("Malformed JSON in host_tools/conformance_manifest.json: row id must be a non-empty string")
        if row_id in seen_ids:
            raise VerificationError(f"Duplicate manifest row ID in host_tools/conformance_manifest.json: {row_id}")
        seen_ids.add(row_id)

        classification = entry["classification"]
        if classification not in SUPPORTED_CLASSIFICATIONS:
            raise VerificationError(
                f"Unsupported classification '{classification}' in host_tools/conformance_manifest.json for row {row_id}"
            )

        scenario_ids = entry["scenario_ids"]
        if not isinstance(scenario_ids, list) or any(not isinstance(item, str) for item in scenario_ids):
            raise VerificationError(
                f"Malformed JSON in host_tools/conformance_manifest.json: scenario_ids must be a string list for row {row_id}"
            )

        normalized_rows.append(
            {
                "id": row_id,
                "section": collapse_whitespace(str(entry["section"])),
                "surface": normalize_markdown_cell(str(entry["surface"])),
                "required": bool(entry["required"]),
                "classification": classification,
                "scenario_ids": scenario_ids,
            }
        )
    return normalized_rows


def load_matrix_rows(root: Path) -> list[dict[str, str]]:
    path = "docs/13-fido-audit-matrix.md"
    text = read_text(root, path)
    manifest_rows = load_manifest_rows(root)
    sections = []
    seen_sections = set()
    for row in manifest_rows:
        section = row["section"]
        if section not in seen_sections:
            seen_sections.add(section)
            sections.append(section)

    rows: list[dict[str, str]] = []
    seen_keys: dict[tuple[str, str], str] = {}
    for section in sections:
        table_rows = extract_table_after_heading(text, section, path)
        for entry in table_rows:
            if set(entry.keys()) != {"Surface", "Implemented behavior", "Protocol bar", "Interop bar", "Classification"}:
                raise VerificationError(
                    f"Malformed matrix columns in {path} section '{section}': expected Surface/Implemented behavior/Protocol bar/Interop bar/Classification"
                )
            surface = normalize_markdown_cell(entry["Surface"])
            key = (section, surface)
            if key in seen_keys:
                raise VerificationError(f"Duplicate matrix surface in {path}: {section} / {surface}")
            seen_keys[key] = section
            classification = normalize_markdown_cell(entry["Classification"])
            if classification not in SUPPORTED_CLASSIFICATIONS:
                raise VerificationError(
                    f"Unsupported classification '{classification}' in {path} for surface {section} / {surface}"
                )
            rows.append(
                {
                    "section": section,
                    "surface": surface,
                    "classification": classification,
                }
            )
    return rows


def load_inventory_tables(root: Path) -> dict[str, list[dict[str, str]]]:
    path = "docs/16-current-state-claim-inventory.md"
    text = read_text(root, path)
    return {
        "public": extract_table_after_heading(text, "Public claim sources", path),
        "non_manifest": extract_table_after_heading(text, "Explicitly non-manifest-backed sources", path),
        "manifest": extract_table_after_heading(text, "Manifest-backed protocol surfaces", path),
        "version": extract_table_after_heading(text, "Version and identity surfaces", path),
    }


def parse_scenario_cell(value: str) -> list[str]:
    normalized = normalize_markdown_cell(value)
    if normalized == "none":
        return []
    backticked = re.findall(r"`([^`]+)`", value)
    if backticked:
        return backticked
    return [item.strip() for item in normalized.split(",") if item.strip()]


def require_populated_fields(
    row: dict[str, str],
    *,
    identifier: str,
    table_name: str,
    required_fields: tuple[str, ...],
    errors: list[str],
) -> None:
    for field in required_fields:
        if not normalize_markdown_cell(row.get(field, "")):
            errors.append(f"Missing {field} for {table_name} row {identifier}")


def extract_backticked_items(value: str) -> list[str]:
    return [item.strip() for item in re.findall(r"`([^`]+)`", value) if item.strip()]


def extract_path_references(value: str) -> list[str]:
    candidates = extract_backticked_items(value)
    if candidates:
        return [candidate for candidate in candidates if "/" in candidate or candidate.endswith((".md", ".json", ".py", ".c", ".toml", ".fam"))]

    normalized = normalize_markdown_cell(value)
    parts = [part.strip() for part in re.split(r",|\band\b|<br>", normalized) if part.strip()]
    return [part for part in parts if "/" in part or part.endswith((".md", ".json", ".py", ".c", ".toml", ".fam"))]


def require_existing_paths(paths: list[str], *, label: str, finding_number: int, root: Path, errors: list[str]) -> None:
    if not paths:
        errors.append(f"Missing {label} for revalidation-map finding {finding_number}")
        return
    for relative_path in paths:
        if not (root / relative_path).exists():
            errors.append(
                f"Missing source path referenced by {label} for revalidation-map finding {finding_number}: {relative_path}"
            )


def validate_named_recheck_field(value: str, *, label: str, finding_number: int, errors: list[str]) -> None:
    normalized = normalize_markdown_cell(value)
    if not normalized:
        errors.append(f"Missing {label} for revalidation-map finding {finding_number}")
        return
    if normalized.lower() in DISALLOWED_RECHECK_PLACEHOLDERS:
        errors.append(f"Blank or vague {label} for revalidation-map finding {finding_number}: {normalized}")
        return
    if not extract_backticked_items(value):
        errors.append(f"Blank or vague {label} for revalidation-map finding {finding_number}: {normalized}")


def extract_historical_findings(text: str, path: str) -> dict[int, str]:
    findings: dict[int, str] = {}
    for line in text.splitlines():
        match = re.match(r"^####\s+(\d+)\.\s+(.*)$", line.strip())
        if match is None:
            continue
        number = int(match.group(1))
        title = normalize_markdown_cell(match.group(2))
        if number in findings:
            raise VerificationError(f"Duplicate finding number {number} in {path}")
        findings[number] = title

    if not findings:
        raise VerificationError(f"No numbered findings found in {path}")

    expected_numbers = list(range(1, max(findings) + 1))
    actual_numbers = sorted(findings)
    if actual_numbers != expected_numbers:
        missing = [number for number in expected_numbers if number not in findings]
        details = ", ".join(str(number) for number in missing)
        raise VerificationError(f"Missing finding heading/number in {path}: {details}")

    return findings


def extract_remediation_batches(text: str, path: str) -> set[str]:
    batches = {normalize_markdown_cell(match.group(1)) for match in re.finditer(r"^###\s+(Batch\s+\d+):", text, flags=re.MULTILINE)}
    if not batches:
        raise VerificationError(f"No remediation batches found in {path}")
    return batches


def verify_inventory(root: Path) -> list[str]:
    errors: list[str] = []

    for relative_path in {
        *REQUIRED_PUBLIC_CLAIM_SOURCES.values(),
        *REQUIRED_NON_MANIFEST_SOURCES.values(),
        "docs/16-current-state-claim-inventory.md",
    }:
        if not (root / relative_path).exists():
            errors.append(f"Missing source path: {relative_path}")

    if errors:
        return errors

    try:
        manifest_rows = load_manifest_rows(root)
    except VerificationError as exc:
        return [str(exc)]

    try:
        matrix_rows = load_matrix_rows(root)
    except VerificationError as exc:
        return [str(exc)]

    try:
        inventory_tables = load_inventory_tables(root)
    except VerificationError as exc:
        return [str(exc)]

    matrix_by_key = {(row["section"], row["surface"]): row for row in matrix_rows}
    manifest_by_key = {(row["section"], row["surface"]): row for row in manifest_rows}

    for row in manifest_rows:
        key = (row["section"], row["surface"])
        matrix_row = matrix_by_key.get(key)
        if matrix_row is None:
            errors.append(
                f"Missing matrix row for manifest row {row['id']}: {row['section']} / {row['surface']}"
            )
            continue
        if matrix_row["classification"] != row["classification"]:
            errors.append(
                "Classification mismatch for manifest row "
                f"{row['id']}: matrix '{matrix_row['classification']}' vs manifest '{row['classification']}'"
            )

    for row in matrix_rows:
        key = (row["section"], row["surface"])
        if key not in manifest_by_key:
            errors.append(f"Missing manifest row for matrix surface {row['section']} / {row['surface']}")

    public_rows = inventory_tables["public"]
    public_seen: dict[str, str] = {}
    for row in public_rows:
        inventory_id = normalize_markdown_cell(row.get("Inventory ID", ""))
        source_path = normalize_markdown_cell(row.get("Source path", ""))
        if not inventory_id:
            errors.append("Missing Inventory ID in docs/16-current-state-claim-inventory.md Public claim sources table")
            continue
        if inventory_id in public_seen:
            errors.append(f"Duplicate inventory ID in docs/16-current-state-claim-inventory.md: {inventory_id}")
            continue
        public_seen[inventory_id] = source_path

    for inventory_id, source_path in REQUIRED_PUBLIC_CLAIM_SOURCES.items():
        actual = public_seen.get(inventory_id)
        if actual is None:
            errors.append(
                f"Missing inventory row {inventory_id} in docs/16-current-state-claim-inventory.md Public claim sources"
            )
        elif actual != source_path:
            errors.append(
                f"Source path mismatch for inventory row {inventory_id}: expected {source_path}, found {actual}"
            )

    non_manifest_rows = inventory_tables["non_manifest"]
    non_manifest_seen: dict[str, str] = {}
    for row in non_manifest_rows:
        inventory_id = normalize_markdown_cell(row.get("Inventory ID", ""))
        source_path = normalize_markdown_cell(row.get("Source path", ""))
        if not inventory_id:
            errors.append(
                "Missing Inventory ID in docs/16-current-state-claim-inventory.md Explicitly non-manifest-backed sources table"
            )
            continue
        if inventory_id in non_manifest_seen:
            errors.append(f"Duplicate inventory ID in docs/16-current-state-claim-inventory.md: {inventory_id}")
            continue
        non_manifest_seen[inventory_id] = source_path

    for inventory_id, source_path in REQUIRED_NON_MANIFEST_SOURCES.items():
        actual = non_manifest_seen.get(inventory_id)
        if actual is None:
            errors.append(
                f"Missing non-manifest inventory row {inventory_id} in docs/16-current-state-claim-inventory.md"
            )
        elif actual != source_path:
            errors.append(
                f"Source path mismatch for non-manifest row {inventory_id}: expected {source_path}, found {actual}"
            )

    manifest_inventory_rows = inventory_tables["manifest"]
    inventory_manifest_seen: dict[str, dict[str, str]] = {}
    for row in manifest_inventory_rows:
        row_id = normalize_markdown_cell(row.get("Row ID", ""))
        if not row_id:
            errors.append("Missing Row ID in docs/16-current-state-claim-inventory.md Manifest-backed protocol surfaces table")
            continue
        if row_id in inventory_manifest_seen:
            errors.append(f"Duplicate inventory ID in docs/16-current-state-claim-inventory.md: {row_id}")
            continue
        inventory_manifest_seen[row_id] = row

    for manifest_row in manifest_rows:
        row = inventory_manifest_seen.get(manifest_row["id"])
        if row is None:
            errors.append(f"Missing inventory row ID in docs/16-current-state-claim-inventory.md: {manifest_row['id']}")
            continue
        require_populated_fields(
            row,
            identifier=manifest_row["id"],
            table_name="manifest-backed protocol surfaces",
            required_fields=REQUIRED_MANIFEST_CLUSTER_FIELDS,
            errors=errors,
        )
        section = normalize_markdown_cell(row.get("Matrix section", ""))
        surface = normalize_markdown_cell(row.get("Matrix surface", ""))
        classification = normalize_markdown_cell(row.get("Classification", ""))
        required = normalize_markdown_cell(row.get("Required", ""))
        scenarios = parse_scenario_cell(row.get("Scenario IDs", ""))
        if section != manifest_row["section"]:
            errors.append(
                f"Section mismatch for inventory row {manifest_row['id']}: expected {manifest_row['section']}, found {section}"
            )
        if surface != manifest_row["surface"]:
            errors.append(
                f"Surface mismatch for inventory row {manifest_row['id']}: expected {manifest_row['surface']}, found {surface}"
            )
        if classification != manifest_row["classification"]:
            errors.append(
                f"Classification mismatch for inventory row {manifest_row['id']}: expected {manifest_row['classification']}, found {classification}"
            )
        expected_required = "yes" if manifest_row["required"] else "no"
        if required != expected_required:
            errors.append(
                f"Required flag mismatch for inventory row {manifest_row['id']}: expected {expected_required}, found {required}"
            )
        if scenarios != manifest_row["scenario_ids"]:
            errors.append(
                f"Scenario ID mismatch for inventory row {manifest_row['id']}: expected {manifest_row['scenario_ids']}, found {scenarios}"
            )

    manifest_ids = {row["id"] for row in manifest_rows}
    for row_id in inventory_manifest_seen:
        if row_id not in manifest_ids:
            errors.append(f"Inventory row ID is not manifest-backed: {row_id}")

    version_rows = inventory_tables["version"]
    version_seen: dict[str, set[str]] = {}
    for row in version_rows:
        inventory_id = normalize_markdown_cell(row.get("Inventory ID", ""))
        source_paths = {
            normalize_markdown_cell(item)
            for item in row.get("Source path", "").split(",")
            if normalize_markdown_cell(item)
        }
        if not inventory_id:
            errors.append("Missing Inventory ID in docs/16-current-state-claim-inventory.md Version and identity surfaces table")
            continue
        if inventory_id in version_seen:
            errors.append(f"Duplicate inventory ID in docs/16-current-state-claim-inventory.md: {inventory_id}")
            continue
        version_seen[inventory_id] = source_paths

    for inventory_id, source_paths in REQUIRED_VERSION_SURFACES.items():
        row = next(
            (
                candidate
                for candidate in version_rows
                if normalize_markdown_cell(candidate.get("Inventory ID", "")) == inventory_id
            ),
            None,
        )
        actual = version_seen.get(inventory_id)
        if actual is None or row is None:
            errors.append(
                f"Missing version/identity inventory row {inventory_id} in docs/16-current-state-claim-inventory.md"
            )
            continue
        require_populated_fields(
            row,
            identifier=inventory_id,
            table_name="version and identity surfaces",
            required_fields=REQUIRED_VERSION_CLUSTER_FIELDS,
            errors=errors,
        )
        if actual != source_paths:
            errors.append(
                f"Source path mismatch for version/identity row {inventory_id}: expected {sorted(source_paths)}, found {sorted(actual)}"
            )

    return errors


def verify_proof_taxonomy(root: Path) -> list[str]:
    if not (root / PROOF_TAXONOMY_PATH).exists():
        return [f"Missing source path: {PROOF_TAXONOMY_PATH}"]

    try:
        text = read_text(root, PROOF_TAXONOMY_PATH)
        proof_rows = extract_table_after_heading(text, "Allowed proof labels", PROOF_TAXONOMY_PATH)
        verdict_rows = extract_table_after_heading(text, "Allowed verdict vocabulary", PROOF_TAXONOMY_PATH)
        field_rows = extract_table_after_heading(text, "Required finding fields", PROOF_TAXONOMY_PATH)
        template_section = extract_section_after_heading(text, "Reusable finding template", PROOF_TAXONOMY_PATH)
        template_code_block = extract_first_fenced_code_block(
            template_section,
            heading="Reusable finding template",
            path=PROOF_TAXONOMY_PATH,
        )
    except VerificationError as exc:
        return [str(exc)]

    errors: list[str] = []

    for reference in PROOF_TAXONOMY_REQUIRED_REFERENCES:
        if reference not in text:
            errors.append(f"Missing grounding reference '{reference}' in {PROOF_TAXONOMY_PATH}")

    allowed_proof_columns = {"Proof label", *REQUIRED_PROOF_LABEL_FIELDS}
    if proof_rows and set(proof_rows[0].keys()) != allowed_proof_columns:
        return [
            f"Malformed proof taxonomy columns in {PROOF_TAXONOMY_PATH}: expected Proof label/Legal when/Required evidence/Illegal when"
        ]

    seen_proof_labels: set[str] = set()
    for row in proof_rows:
        label = normalize_markdown_cell(row.get("Proof label", ""))
        if not label:
            errors.append(f"Missing proof label in {PROOF_TAXONOMY_PATH}")
            continue
        if label in seen_proof_labels:
            errors.append(f"Duplicate proof label '{label}' in {PROOF_TAXONOMY_PATH}")
            continue
        seen_proof_labels.add(label)
        if label not in REQUIRED_PROOF_LABELS:
            errors.append(f"Unsupported proof label '{label}' in {PROOF_TAXONOMY_PATH}")
        require_populated_fields(
            row,
            identifier=label,
            table_name="proof taxonomy",
            required_fields=REQUIRED_PROOF_LABEL_FIELDS,
            errors=errors,
        )

    for label in sorted(REQUIRED_PROOF_LABELS):
        if label not in seen_proof_labels:
            errors.append(f"Missing proof label '{label}' in {PROOF_TAXONOMY_PATH}")

    allowed_verdict_columns = {"Verdict", *REQUIRED_VERDICT_FIELDS}
    if verdict_rows and set(verdict_rows[0].keys()) != allowed_verdict_columns:
        return [
            f"Malformed verdict vocabulary columns in {PROOF_TAXONOMY_PATH}: expected Verdict/Meaning/Closure rule"
        ]

    seen_verdicts: set[str] = set()
    for row in verdict_rows:
        verdict = normalize_markdown_cell(row.get("Verdict", ""))
        if not verdict:
            errors.append(f"Missing verdict in {PROOF_TAXONOMY_PATH}")
            continue
        if verdict in seen_verdicts:
            errors.append(f"Duplicate verdict '{verdict}' in {PROOF_TAXONOMY_PATH}")
            continue
        seen_verdicts.add(verdict)
        if verdict not in REQUIRED_VERDICTS:
            errors.append(f"Unsupported verdict '{verdict}' in {PROOF_TAXONOMY_PATH}")
        require_populated_fields(
            row,
            identifier=verdict,
            table_name="verdict vocabulary",
            required_fields=REQUIRED_VERDICT_FIELDS,
            errors=errors,
        )

    for verdict in sorted(REQUIRED_VERDICTS):
        if verdict not in seen_verdicts:
            errors.append(f"Missing verdict '{verdict}' in {PROOF_TAXONOMY_PATH}")

    allowed_field_columns = {"Field", "Why it is required"}
    if field_rows and set(field_rows[0].keys()) != allowed_field_columns:
        return [
            f"Malformed required finding fields columns in {PROOF_TAXONOMY_PATH}: expected Field/Why it is required"
        ]

    seen_fields: set[str] = set()
    for row in field_rows:
        field = normalize_markdown_cell(row.get("Field", ""))
        if not field:
            errors.append(f"Missing template field in {PROOF_TAXONOMY_PATH}")
            continue
        if field in seen_fields:
            errors.append(f"Duplicate template field '{field}' in {PROOF_TAXONOMY_PATH}")
            continue
        seen_fields.add(field)
        if not normalize_markdown_cell(row.get("Why it is required", "")):
            errors.append(f"Missing Why it is required for required finding fields row {field}")

    for field in sorted(REQUIRED_FINDING_TEMPLATE_FIELDS):
        if field not in seen_fields:
            errors.append(f"Missing template field '{field}' in {PROOF_TAXONOMY_PATH}")

    for field in REQUIRED_TEMPLATE_CODE_FIELDS:
        if re.search(rf"{re.escape(field)}\s*:", template_code_block) is None:
            errors.append(
                f"Missing template field '{field}' in reusable finding template code block in {PROOF_TAXONOMY_PATH}"
            )

    return errors


def verify_revalidation_map(root: Path) -> list[str]:
    required_paths = {REVALIDATION_MAP_PATH, REVALIDATION_REPORT_PATH, REMEDIATION_PLAN_PATH}
    missing_paths = [relative_path for relative_path in sorted(required_paths) if not (root / relative_path).exists()]
    if missing_paths:
        return [f"Missing source path: {relative_path}" for relative_path in missing_paths]

    try:
        historical_text = read_text(root, REVALIDATION_REPORT_PATH)
        remediation_text = read_text(root, REMEDIATION_PLAN_PATH)
        map_text = read_text(root, REVALIDATION_MAP_PATH)
        bucket_rows = extract_table_after_heading(map_text, "Approved verdict buckets", REVALIDATION_MAP_PATH)
        ledger_rows = extract_table_after_heading(map_text, "Historical finding ledger", REVALIDATION_MAP_PATH)
        historical_findings = extract_historical_findings(historical_text, REVALIDATION_REPORT_PATH)
        remediation_batches = extract_remediation_batches(remediation_text, REMEDIATION_PLAN_PATH)
    except VerificationError as exc:
        return [str(exc)]

    errors: list[str] = []

    allowed_bucket_columns = {"Current verdict bucket", *REQUIRED_REVALIDATION_BUCKET_FIELDS}
    if bucket_rows and set(bucket_rows[0].keys()) != allowed_bucket_columns:
        return [
            f"Malformed revalidation bucket columns in {REVALIDATION_MAP_PATH}: expected Current verdict bucket/Use when/Closure rule"
        ]

    approved_buckets: set[str] = set()
    for row in bucket_rows:
        bucket = normalize_markdown_cell(row.get("Current verdict bucket", ""))
        if not bucket:
            errors.append(f"Missing verdict bucket in {REVALIDATION_MAP_PATH}")
            continue
        if bucket in approved_buckets:
            errors.append(f"Duplicate verdict bucket '{bucket}' in {REVALIDATION_MAP_PATH}")
            continue
        approved_buckets.add(bucket)
        require_populated_fields(
            row,
            identifier=bucket,
            table_name="revalidation verdict buckets",
            required_fields=REQUIRED_REVALIDATION_BUCKET_FIELDS,
            errors=errors,
        )

    for bucket in sorted(REQUIRED_REVALIDATION_BUCKETS):
        if bucket not in approved_buckets:
            errors.append(f"Missing verdict bucket '{bucket}' in {REVALIDATION_MAP_PATH}")

    allowed_ledger_columns = {"Finding #", "Historical finding", *REQUIRED_REVALIDATION_LEDGER_FIELDS}
    if ledger_rows and set(ledger_rows[0].keys()) != allowed_ledger_columns:
        return [
            "Malformed historical finding ledger columns in "
            f"{REVALIDATION_MAP_PATH}: expected Finding #/Historical finding/Current remediation batch/"
            "Current owner files/Fresh re-check command/Fresh evidence source/Current verdict bucket/Conservative checkpoint"
        ]

    seen_numbers: set[int] = set()
    used_buckets: set[str] = set()
    for row in ledger_rows:
        finding_value = normalize_markdown_cell(row.get("Finding #", ""))
        if not finding_value:
            errors.append(f"Missing finding number in {REVALIDATION_MAP_PATH}")
            continue
        try:
            finding_number = int(finding_value)
        except ValueError:
            errors.append(f"Malformed finding number in {REVALIDATION_MAP_PATH}: {finding_value}")
            continue
        if finding_number in seen_numbers:
            errors.append(f"Duplicate finding number {finding_number} in {REVALIDATION_MAP_PATH}")
            continue
        seen_numbers.add(finding_number)

        historical_title = normalize_markdown_cell(row.get("Historical finding", ""))
        expected_title = historical_findings.get(finding_number)
        if expected_title is None:
            errors.append(f"Unmapped historical finding number {finding_number} in {REVALIDATION_MAP_PATH}")
            continue
        if historical_title != expected_title:
            errors.append(
                f"Historical finding title mismatch for {finding_number}: expected '{expected_title}', found '{historical_title}'"
            )

        batch_value = normalize_markdown_cell(row.get("Current remediation batch", ""))
        if not batch_value:
            errors.append(f"Missing remediation batch for revalidation-map finding {finding_number}")
        elif batch_value.startswith("Batch "):
            batch_name_match = re.match(r"^(Batch\s+\d+)", batch_value)
            batch_name = batch_name_match.group(1) if batch_name_match else batch_value
            if batch_name not in remediation_batches:
                errors.append(
                    f"Missing remediation batch reference '{batch_name}' for revalidation-map finding {finding_number}"
                )
        elif not batch_value.startswith("no batch"):
            errors.append(
                f"Malformed remediation batch note for revalidation-map finding {finding_number}: {batch_value}"
            )

        owner_files = extract_path_references(row.get("Current owner files", ""))
        require_existing_paths(
            owner_files,
            label="current owner files",
            finding_number=finding_number,
            root=root,
            errors=errors,
        )

        validate_named_recheck_field(
            row.get("Fresh re-check command", ""),
            label="fresh re-check command",
            finding_number=finding_number,
            errors=errors,
        )
        validate_named_recheck_field(
            row.get("Fresh evidence source", ""),
            label="fresh evidence source",
            finding_number=finding_number,
            errors=errors,
        )
        evidence_paths = extract_path_references(row.get("Fresh evidence source", ""))
        require_existing_paths(
            evidence_paths,
            label="fresh evidence source",
            finding_number=finding_number,
            root=root,
            errors=errors,
        )

        verdict_bucket = normalize_markdown_cell(row.get("Current verdict bucket", ""))
        if not verdict_bucket:
            errors.append(f"Missing current verdict bucket for revalidation-map finding {finding_number}")
        elif verdict_bucket not in approved_buckets:
            errors.append(
                f"Unsupported current verdict bucket '{verdict_bucket}' for revalidation-map finding {finding_number}"
            )
        else:
            used_buckets.add(verdict_bucket)

        if not normalize_markdown_cell(row.get("Conservative checkpoint", "")):
            errors.append(f"Missing Conservative checkpoint for revalidation-map finding {finding_number}")

    missing_numbers = [number for number in sorted(historical_findings) if number not in seen_numbers]
    for number in missing_numbers:
        errors.append(f"Missing revalidation-map row for finding {number}: {historical_findings[number]}")

    for bucket in sorted(REQUIRED_REVALIDATION_BOUNDARY_BUCKETS):
        if bucket not in used_buckets:
            errors.append(
                f"Missing boundary-condition verdict bucket usage '{bucket}' in {REVALIDATION_MAP_PATH}"
            )

    return errors


def run_selected_checks(root: Path, inventory: bool, proof_taxonomy: bool, revalidation_map: bool, strict: bool) -> list[str]:
    errors: list[str] = []
    if strict or inventory:
        errors.extend(verify_inventory(root))
    if strict or proof_taxonomy:
        errors.extend(verify_proof_taxonomy(root))
    if strict or revalidation_map:
        errors.extend(verify_revalidation_map(root))
    return errors


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify tracked current-state audit-frame artifacts.")
    parser.add_argument("--inventory", action="store_true", help="verify the claim inventory and manifest/matrix mapping")
    parser.add_argument("--proof-taxonomy", action="store_true", help="verify the proof taxonomy artifact")
    parser.add_argument("--revalidation-map", action="store_true", help="verify the historical revalidation-map artifact")
    parser.add_argument("--strict", action="store_true", help="run all audit-frame checks")
    parser.add_argument("--root", help="override the repository root for tests")
    args = parser.parse_args(argv)
    if not any([args.inventory, args.proof_taxonomy, args.revalidation_map, args.strict]):
        parser.error("select at least one of --inventory, --proof-taxonomy, --revalidation-map, or --strict")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    root = repo_root(args.root)
    errors = run_selected_checks(root, args.inventory, args.proof_taxonomy, args.revalidation_map, args.strict)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    if args.strict:
        print("strict: ok")
    else:
        if args.inventory:
            print("inventory: ok")
        if args.proof_taxonomy:
            print("proof-taxonomy: ok")
        if args.revalidation_map:
            print("revalidation-map: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
