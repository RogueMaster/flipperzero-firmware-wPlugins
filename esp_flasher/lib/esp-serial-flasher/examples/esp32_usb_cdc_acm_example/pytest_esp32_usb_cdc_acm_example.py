import pytest
from pytest_embedded import Dut


@pytest.mark.esp32s3
def test_esp32_usb_cdc_acm_example(dut: Dut) -> None:
    # Check initial connection
    dut.expect("Connected to target")

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
