import pytest
from pytest_embedded import Dut
import serial
import time
import os


@pytest.fixture(autouse=True)
def flash_pico(build_dir: str, port: str) -> None:
    try:
        with serial.Serial(port, baudrate=1200):
            pass
    except serial.SerialException:
        print(f"Could not open {port}, Raspberry Pi Pico already in bootloader mode")

    while not os.path.exists("/dev/sda1"):
        time.sleep(0.1)
    file_info = os.stat(f"examples/pi_pico_example/{build_dir}/pi_pico_example.uf2")
    size = file_info.st_size
    # Writing using dd, because mounting did not work in the docker, nor in base system
    os.system(
        f"dd if=examples/pi_pico_example/{build_dir}/pi_pico_example.uf2 of=/dev/sda1 bs={size}"
    )

    while not os.path.exists(port):
        time.sleep(0.01)


@pytest.mark.pi_pico
def test_pi_pico_example(dut: Dut) -> None:
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
