"""Build and validate a release FAP with the symbol-budget gate applied.

The script optionally invokes UFBT, then delegates the actual export stripping
and budget checks to check_symbol_gate so CI and manual release packaging use
the same policy.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

import check_symbol_gate


ROOT = Path(__file__).resolve().parents[1]

RELEASE_SAFE_BUILD_FLAGS = {
    "ZEROFIDO_RELEASE_DIAGNOSTICS": "0",
    "ZEROFIDO_USB_DIAGNOSTICS": "0",
    "ZEROFIDO_AUTO_ACCEPT_REQUESTS": "0",
    "ZEROFIDO_DEV_SCREENSHOT": "0",
    "ZEROFIDO_DEV_FIDO2_1": "0",
    "ZEROFIDO_PACKED_ATTESTATION": "1",
}

FORBIDDEN_RELEASE_PATTERNS = {
    b"ZeroFIDO:CTAP": "CTAP diagnostics log tag",
    b"ZeroFIDO:MEM": "memory telemetry log tag",
    b"ZeroFIDO:NFC": "NFC diagnostics log tag",
    b"usb_diag.log": "USB diagnostics log path",
    b"gi caps": "USB diagnostics getInfo text",
    b"mc flags": "USB diagnostics makeCredential text",
    b"empty-pin-auth": "USB diagnostics makeCredential text",
    b"pin auth": "USB diagnostics pinAuth text",
    b"cmd=": "diagnostic command log text",
    b"trace dropped": "NFC trace buffer log text",
    b"idle heartbeat": "idle telemetry log text",
    b"redacted": "redacted diagnostic payload marker",
    b"CP-RT": "ClientPIN diagnostic tag",
    b"CP-GA": "ClientPIN diagnostic tag",
    b"CP-SP": "ClientPIN diagnostic tag",
    b"CP-CH": "ClientPIN diagnostic tag",
    b"CP-TK": "ClientPIN diagnostic tag",
    b"CP-PT": "ClientPIN diagnostic tag",
    b"CP-UK": "ClientPIN diagnostic tag",
}


def _root_relative(path: Path) -> Path:
    """Resolve CLI paths relative to the repository root unless already absolute."""
    return path if path.is_absolute() else ROOT / path


def parse_args(argv: list[str]) -> argparse.Namespace:
    """Parse release packaging options without touching the filesystem."""
    parser = argparse.ArgumentParser(description="Build and package the stripped ZeroFIDO release FAP")
    parser.add_argument(
        "--fap",
        type=Path,
        default=Path("dist/zerofido.fap"),
        help="UFBT output FAP to package",
    )
    parser.add_argument(
        "--output-fap",
        type=Path,
        default=check_symbol_gate.FAP_DEFAULT_OUTPUT,
        help="stripped release FAP path",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="package the existing --fap without running UFBT first",
    )
    parser.add_argument(
        "--allow-prebuilt-input",
        action="store_true",
        help="allow --skip-build to package a prebuilt FAP for inspection",
    )
    parser.add_argument(
        "--no-packed-attestation",
        action="store_false",
        dest="packed_attestation",
        default=True,
        help="build the release FAP without packed FIDO2 attestation support",
    )
    return parser.parse_args(argv)


def run_ufbt(*, packed_attestation: bool = True) -> None:
    """Run a clean UFBT build in the repository root."""
    env = os.environ.copy()
    env.update(RELEASE_SAFE_BUILD_FLAGS)
    env["ZEROFIDO_PACKED_ATTESTATION"] = "1" if packed_attestation else "0"
    subprocess.run([sys.executable, "-m", "ufbt", "-c"], cwd=ROOT, check=True, env=env)
    subprocess.run([sys.executable, "-m", "ufbt"], cwd=ROOT, check=True, env=env)


def validate_release_payload(fap: Path) -> list[str]:
    """Return forbidden release payload markers found in a packaged FAP."""
    data = fap.read_bytes()
    return [
        f"{description}: {pattern.decode('ascii', errors='replace')}"
        for pattern, description in FORBIDDEN_RELEASE_PATTERNS.items()
        if pattern in data
    ]


def package_release(
    fap: Path, output_fap: Path, *, skip_build: bool, allow_prebuilt_input: bool = False,
    packed_attestation: bool = False
) -> int:
    """Build if requested, then enforce release FAP symbol and payload gates."""
    if skip_build and not allow_prebuilt_input:
        print(
            "--skip-build requires --allow-prebuilt-input because prebuilt FAPs may carry dev flags",
            file=sys.stderr,
        )
        return 2
    if not skip_build:
        run_ufbt(packed_attestation=packed_attestation)

    output = _root_relative(output_fap)
    status = check_symbol_gate.check_fap_symbol_budget(
        _root_relative(fap),
        fix=False,
        output_fap=output,
    )
    if status != 0:
        return status

    violations = validate_release_payload(output)
    if violations:
        print("release payload gate failed")
        for item in violations:
            print(f"  - {item}")
        return 1

    return 0


def main(argv: list[str] | None = None) -> int:
    """CLI entry point used by tests and manual packaging."""
    args = parse_args(sys.argv[1:] if argv is None else argv)
    return package_release(
        args.fap,
        args.output_fap,
        skip_build=args.skip_build,
        allow_prebuilt_input=args.allow_prebuilt_input,
        packed_attestation=args.packed_attestation,
    )


if __name__ == "__main__":
    raise SystemExit(main())
