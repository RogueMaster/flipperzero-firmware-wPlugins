import os, shutil, subprocess, sys, tempfile
from pathlib import Path

here = Path(__file__).resolve().parent
momentum = Path(os.environ["MOMENTUM_ROOT"])
toolchain = momentum / "toolchain/x86_64-windows/bin"
env = os.environ.copy()
env["PATH"] = str(toolchain) + os.pathsep + env.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="drmosfet_can_") as directory:
    temp = Path(directory)
    shim = temp / "fixture.c"
    shutil.copyfile(here / "fixture.c.txt", shim)
    elf = temp / "protocol.elf"
    subprocess.run(
        [
            str(toolchain / "arm-none-eabi-gcc.exe"),
            "-mcpu=cortex-m4",
            "-mthumb",
            "-mfloat-abi=soft",
            "-Os",
            "-nostartfiles",
            "--specs=nosys.specs",
            "-I",
            str(here.parent),
            str(shim),
            str(here.parent / "gvret_protocol.c"),
            "-T",
            str(here / "test.ld"),
            "-o",
            str(elf),
        ],
        env=env,
        check=True,
    )
    env["TEST_ELF"] = str(elf)
    raise SystemExit(
        subprocess.run(
            [sys.executable, str(here / "test_protocol.py")], env=env
        ).returncode
    )
