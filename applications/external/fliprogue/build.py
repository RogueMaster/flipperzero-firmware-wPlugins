#!/usr/bin/env python3
import argparse
import contextlib
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
APPID = "fliprogue"
BUILD_DIR = ROOT.parent / ".host_build" / APPID
DIST_DIR = ROOT / "dist"
FAP_OUTPUT = Path.home() / ".ufbt" / "build" / f"{APPID}.fap"
HOST_EXCLUDED_SOURCES = {
    "fliprogue_app.c",
    "score_store.c",
    "tile_sprites.c",
    "ui_camera.c",
    "ui_draw.c",
    "ui_feedback.c",
    "ui_help.c",
    "ui_input.c",
    "ui_screens.c",
}


def run(cmd, cwd=ROOT):
    print("[RUN]", " ".join(map(str, cmd)))
    subprocess.check_call(cmd, cwd=cwd)


def ufbt_cmd():
    return [sys.executable, "-m", "ufbt"]


def host_sources():
    return [
        str(path)
        for path in sorted((ROOT / "src").glob("*.c"))
        if path.name not in HOST_EXCLUDED_SOURCES
    ]


def ensure_ufbt_available():
    try:
        subprocess.check_call(
            [*ufbt_cmd(), "--help"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=ROOT,
        )
    except subprocess.CalledProcessError:
        print("[BUILD] The current Python cannot run `python -m ufbt`.")
        print("        Install UFBT in the active Python environment, then run:")
        print("        python build.py package")
        sys.exit(2)


def task_test(_args):
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    out = BUILD_DIR / "test_game_logic"
    run(
        [
            "cc",
            "-std=c99",
            "-Wall",
            "-Wextra",
            "-I",
            "src",
            "-x",
            "c",
            "tests/test_game_logic.c",
            "-x",
            "none",
            *host_sources(),
            "-o",
            str(out),
        ]
    )
    run([str(out)])
    print("[TEST] Host tests passed.")


def task_build(_args):
    ensure_ufbt_available()
    shutil_rmtree_if_exists(ROOT / "__pycache__")
    shutil_rmtree_if_exists(ROOT / "build")
    run([*ufbt_cmd(), "build"])


def task_package(args):
    task_test(args)
    task_build(args)
    DIST_DIR.mkdir(exist_ok=True)
    if not FAP_OUTPUT.exists():
        print(f"[PKG] Expected FAP not found: {FAP_OUTPUT}")
        sys.exit(1)
    dest = DIST_DIR / f"{APPID}.fap"
    shutil.copy2(FAP_OUTPUT, dest)
    print(f"[PKG] Wrote {dest}")


def task_clean(_args):
    for path in (BUILD_DIR, DIST_DIR):
        if path.exists():
            print(f"[CLEAN] Removing {path}")
            shutil_rmtree_if_exists(path)
    shutil_rmtree_if_exists(ROOT / "__pycache__")
    shutil_rmtree_if_exists(ROOT / "build")
    with contextlib.suppress(subprocess.CalledProcessError):
        ensure_ufbt_available()
        run([*ufbt_cmd(), "-c"])


def task_all(args):
    task_package(args)


def shutil_rmtree_if_exists(path):
    if path.exists():
        shutil.rmtree(path)


def main():
    parser = argparse.ArgumentParser(description="Build FlipRogue for Flipper Zero.")
    parser.add_argument(
        "command",
        nargs="?",
        default="all",
        choices=["test", "build", "package", "clean", "all"],
    )
    args = parser.parse_args()
    {
        "test": task_test,
        "build": task_build,
        "package": task_package,
        "clean": task_clean,
        "all": task_all,
    }[args.command](args)


if __name__ == "__main__":
    main()
