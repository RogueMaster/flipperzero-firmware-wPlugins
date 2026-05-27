"""C maintenance wrapper for formatting, clang-tidy, cppcheck, and host tests.

The helper adapts UFBT's generated compile database into forms usable by desktop
LLVM tools, then keeps all C verification commands behind one stable CLI.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMPILE_COMMANDS = ROOT / ".vscode" / "compile_commands.json"
CLANG_TIDY_BUILD_DIR = ROOT / ".vscode" / "clang-tidy"
CLANG_TIDY_COMPILE_COMMANDS = CLANG_TIDY_BUILD_DIR / "compile_commands.json"
UFBT_ROOT = Path.home() / ".ufbt"
SDK_HEADERS_ROOT = UFBT_ROOT / "current" / "sdk_headers" / "f7_sdk"
TOOLCHAIN_ROOT = UFBT_ROOT / "toolchain"
FORMAT_GLOBS = ("src/*.c", "src/**/*.c", "src/*.h", "src/**/*.h", "*.h",
                "tests/native/**/*.c", "tests/native/**/*.h", "tests/native/**/*.inc")
CLANG_TIDY_GLOBS = ("src/*.c", "src/**/*.c")
CPP_SOURCE_GLOBS = ("src/*.c", "src/**/*.c")
COMPILE_COMMAND_INPUT_GLOBS = ("application.fam", "src/*.c", "src/**/*.c", "src/*.h", "src/**/*.h")
CLANG_TIDY_UNSUPPORTED_ARGS = {
    "-fsingle-precision-constant",
    "-mlong-calls",
    "-mword-relocations",
}
ANALYSIS_EXCLUDED_DIRS = ("src/crypto/micro_ecc",)
CPP_CHECK_ARG_PREFIXES = ("-D", "-I", "-U")
CPP_CHECK_ARG_EXACT = {"-std=c11", "-std=gnu11"}


def run(cmd: list[str], *, cwd: Path = ROOT, check: bool = True) -> int:
    """Run a command with Homebrew LLVM first on PATH and echo it for logs."""
    print("+", " ".join(cmd))
    env = os.environ.copy()
    env["PATH"] = "/opt/homebrew/opt/llvm/bin:" + env.get("PATH", "")
    result = subprocess.run(cmd, cwd=cwd, env=env)
    if check and result.returncode != 0:
        raise SystemExit(result.returncode)
    return result.returncode


def require_tool(name: str) -> str:
    """Find a required static-analysis tool, including common macOS fallback paths."""
    path = shutil.which(name)
    if path:
        return path

    fallbacks = {
        "clang-format": [
            "/Library/Developer/CommandLineTools/usr/bin/clang-format",
            "/opt/homebrew/opt/llvm/bin/clang-format",
        ],
        "clang-tidy": [
            "/opt/homebrew/opt/llvm/bin/clang-tidy",
            "/Library/Developer/CommandLineTools/usr/bin/clang-tidy",
        ],
        "cppcheck": [
            "/opt/homebrew/bin/cppcheck",
        ],
    }
    for candidate in fallbacks.get(name, []):
        if Path(candidate).exists():
            return candidate

    raise SystemExit(f"missing required tool: {name}")


def load_paths(patterns: tuple[str, ...]) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns:
        files.extend(path for path in ROOT.glob(pattern))
    return sorted(set(files))


def load_files(patterns: tuple[str, ...]) -> list[str]:
    return [str(path) for path in load_paths(patterns)]


def is_analysis_excluded(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    return any(rel == Path(prefix) or rel.is_relative_to(prefix) for prefix in ANALYSIS_EXCLUDED_DIRS)


def load_analysis_files(patterns: tuple[str, ...]) -> list[str]:
    return [str(path) for path in load_paths(patterns) if not is_analysis_excluded(path)]


def compile_commands_are_stale() -> bool:
    """Return true when source inputs are newer than the UFBT compile database."""
    if not COMPILE_COMMANDS.exists():
        return True

    compile_mtime = COMPILE_COMMANDS.stat().st_mtime
    for path in load_paths(COMPILE_COMMAND_INPUT_GLOBS):
        if path.stat().st_mtime > compile_mtime:
            return True

    try:
        data = load_compile_commands()
    except (OSError, ValueError, json.JSONDecodeError):
        return True

    expected_sources = {path.resolve() for path in load_paths(CLANG_TIDY_GLOBS)}
    compiled_sources: set[Path] = set()
    for entry in data:
        source = entry.get("file")
        if isinstance(source, str):
            compiled_sources.add(Path(source).resolve())

    return not expected_sources.issubset(compiled_sources)


def ensure_compile_commands() -> None:
    if not compile_commands_are_stale():
        return
    run(["uv", "run", "python", "-m", "ufbt"], check=True)
    if not COMPILE_COMMANDS.exists():
        raise SystemExit(f"compile database not found at {COMPILE_COMMANDS}")


def load_compile_commands() -> list[dict[str, object]]:
    return json.loads(COMPILE_COMMANDS.read_text())


def load_command_args(entry: dict[str, object]) -> list[str]:
    arguments = entry.get("arguments")
    if isinstance(arguments, list) and arguments:
        return [str(arg) for arg in arguments]

    command = entry.get("command")
    if isinstance(command, str) and command:
        return shlex.split(command)

    raise SystemExit("invalid compile command entry")


def build_clang_tidy_compile_commands(clang: str) -> Path:
    """Rewrite UFBT GCC commands into clang-compatible clang-tidy commands."""
    data = load_compile_commands()
    rewritten: list[dict[str, object]] = []
    toolchain_root = detect_toolchain_root(data)
    toolchain_include_args = build_toolchain_include_args(toolchain_root)

    for entry in data:
        args = load_command_args(entry)
        if len(args) < 2:
            continue

        source_file = str(entry["file"])
        filtered = [clang, "--target=arm-none-eabi", *toolchain_include_args]
        skip_next = False
        for arg in args[1:]:
            if skip_next:
                skip_next = False
                continue
            if arg == "-o":
                skip_next = True
                continue
            if arg in CLANG_TIDY_UNSUPPORTED_ARGS:
                continue
            if arg == source_file or arg.endswith(".o"):
                continue
            filtered.append(arg)

        filtered.append(source_file)
        rewritten.append(
            {
                "directory": str(entry["directory"]),
                "file": source_file,
                "arguments": filtered,
            }
        )

    CLANG_TIDY_BUILD_DIR.mkdir(parents=True, exist_ok=True)
    CLANG_TIDY_COMPILE_COMMANDS.write_text(json.dumps(rewritten, indent=2) + "\n")
    return CLANG_TIDY_BUILD_DIR


def detect_toolchain_root(data: list[dict[str, object]]) -> Path | None:
    """Infer the ARM GCC toolchain root from the generated compile database."""
    if not data:
        return None

    compiler = Path(load_command_args(data[0])[0]).resolve()
    if compiler.name != "arm-none-eabi-gcc":
        return None
    return compiler.parents[1]


def build_toolchain_include_args(toolchain_root: Path | None) -> list[str]:
    if toolchain_root is None:
        return []

    include_args: list[str] = []
    arm_include = toolchain_root / "arm-none-eabi" / "include"
    gcc_include_parent = toolchain_root / "lib" / "gcc" / "arm-none-eabi"
    gcc_include_dirs = sorted(gcc_include_parent.glob("*/include"))

    for include_dir in [arm_include, *gcc_include_dirs]:
        if include_dir.exists():
            include_args.extend(["-isystem", str(include_dir)])

    return include_args


def build_cppcheck_args() -> list[str]:
    """Extract portable preprocessor flags from the first compile command."""
    data = load_compile_commands()
    if not data:
        return []

    args = load_command_args(data[0])[1:]
    filtered: list[str] = [
        "--std=c11",
        "--platform=unix32",
        "-D__GNUC__=12",
        "-D__GNUC_MINOR__=3",
        "-D__GNUC_PATCHLEVEL__=0",
        "-D_ATTRIBUTE(x)=",
    ]
    seen = set(filtered)

    for arg in args:
        if arg.startswith(CPP_CHECK_ARG_PREFIXES) or arg in CPP_CHECK_ARG_EXACT:
            if arg not in seen:
                filtered.append(arg)
                seen.add(arg)

    return filtered


def cmd_format(args: argparse.Namespace) -> None:
    clang_format = require_tool("clang-format")
    files = load_files(FORMAT_GLOBS)
    if not files:
        return
    if args.fix:
        run([clang_format, "-i", *files])
    else:
        run([clang_format, "--dry-run", "-Werror", *files])


def cmd_tidy(_: argparse.Namespace) -> None:
    ensure_compile_commands()
    clang_tidy = require_tool("clang-tidy")
    clang = shutil.which("clang") or "/opt/homebrew/opt/llvm/bin/clang"
    files = load_analysis_files(CLANG_TIDY_GLOBS)
    if not files:
        return

    build_dir = build_clang_tidy_compile_commands(clang)
    header_filter = re.escape(str(ROOT / "src")) + "/.*"
    excluded_headers = "|".join(
        re.escape(str(ROOT / prefix)) + "/.*" for prefix in ANALYSIS_EXCLUDED_DIRS
    )
    cmd = [
        clang_tidy,
        "-p",
        str(build_dir),
        f"-header-filter={header_filter}",
        f"-exclude-header-filter={excluded_headers}",
    ]
    cmd.extend(files)
    run(cmd)


def cmd_cppcheck(_: argparse.Namespace) -> None:
    ensure_compile_commands()
    cppcheck = require_tool("cppcheck")
    files = load_analysis_files(CPP_SOURCE_GLOBS)
    if not files:
        return
    run(
        [
            cppcheck,
            "--enable=warning,performance,portability",
            "--error-exitcode=1",
            "--inline-suppr",
            "--quiet",
            "--check-level=exhaustive",
            f"--suppress=*:{SDK_HEADERS_ROOT}/*",
            f"--suppress=*:{TOOLCHAIN_ROOT}/*",
            "--suppress=missingIncludeSystem",
            "--suppress=preprocessorErrorDirective:"
            + str(SDK_HEADERS_ROOT / "lib" / "cmsis_core" / "cmsis_compiler.h"),
            *build_cppcheck_args(),
            *files,
        ]
    )


def cmd_native(_: argparse.Namespace) -> None:
    run(["uv", "run", "python", "tools/run_protocol_regressions.py"])


def cmd_all(args: argparse.Namespace) -> None:
    cmd_format(argparse.Namespace(fix=False))
    cmd_tidy(args)
    cmd_cppcheck(args)
    cmd_native(args)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run C validation for zerofido.")
    sub = parser.add_subparsers(dest="command", required=True)

    fmt = sub.add_parser("format", help="Run clang-format")
    fmt.add_argument("--fix", action="store_true", help="Rewrite files in place")
    fmt.set_defaults(func=cmd_format)

    tidy = sub.add_parser("tidy", help="Run clang-tidy")
    tidy.set_defaults(func=cmd_tidy)

    cpp = sub.add_parser("cppcheck", help="Run cppcheck")
    cpp.set_defaults(func=cmd_cppcheck)

    native = sub.add_parser("native", help="Run native protocol regressions")
    native.set_defaults(func=cmd_native)

    all_cmd = sub.add_parser("all", help="Run format check, clang-tidy, and cppcheck")
    all_cmd.set_defaults(func=cmd_all)

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.func(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
