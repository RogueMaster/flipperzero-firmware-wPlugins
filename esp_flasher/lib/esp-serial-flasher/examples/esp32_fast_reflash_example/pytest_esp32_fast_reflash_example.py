import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_fast_reflash_example(dut: Dut) -> None:
    dut.expect("Hello world!")
    dut.serial.hard_reset()
    dut.expect("Bootloader MD5 match, skipping...")
    dut.expect("Partition table MD5 match, skipping...")
    dut.expect("Application MD5 match, skipping...")
    dut.expect("Hello world!")
