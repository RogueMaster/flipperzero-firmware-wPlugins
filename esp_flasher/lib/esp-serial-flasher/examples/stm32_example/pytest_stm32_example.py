import pytest
from pytest_embedded import Dut
import subprocess
import serial


@pytest.fixture(autouse=True)
def flash_stm32(build_dir: str, port: str) -> None:
    # Flush input buffer, because there is some data left from previous bootloader communication (cannot enter bootloader mode otherwise)
    with serial.Serial(port, 115200) as ser:
        ser.reset_input_buffer()

    subprocess.run(
        [f"stm32loader --port {port} --erase --write stm32_flasher.bin"],
        cwd=f"./examples/stm32_example/{build_dir}/",
        shell=True,
        check=True,
    )


@pytest.mark.stm32
def test_stm32_example(dut: Dut) -> None:
    # Set DTR to false to unset the reset pin of the STM32
    dut.serial.proc.setDTR(False)

    # Check initial connection
    dut.expect("Connected to target")
    dut.expect("Transmission rate changed")

    # Check bootloader programming
    dut.expect("Loading bootloader")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")

    # Check partition table programming
    dut.expect("Loading partition table")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")

    # Check app programming
    dut.expect("Loading app")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    dut.expect("Flash verified")

    # Check target output
    dut.expect("Hello world!")
