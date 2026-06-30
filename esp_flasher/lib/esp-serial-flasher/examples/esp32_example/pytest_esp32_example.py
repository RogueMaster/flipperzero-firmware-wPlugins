import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
@pytest.mark.parametrize(
    "config",
    [None, "_md5_disabled"],
    ids=["default", "md5_disabled"],
    indirect=True,
)
def test_esp32_example(dut: Dut, config: str) -> None:
    # Check initial connection
    dut.expect("Connected to target")
    dut.expect("Transmission rate changed")

    # Check bootloader programming
    dut.expect("Loading bootloader...")
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    if config is None:
        dut.expect("Flash verified")
        dut.expect("Loading partition table...")
    else:
        dut.expect("Loading partition table...", not_matching="Flash verified")

    # Check partition table programming
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    if config is None:
        dut.expect("Flash verified")
        dut.expect("Loading app...")
    else:
        dut.expect("Loading app...", not_matching="Flash verified")

    # Check app programming
    dut.expect("Erasing flash")
    dut.expect("Start programming")
    dut.expect("Finished programming")
    if config is None:
        dut.expect("Flash verified")
        dut.expect("Hello world!")
    else:
        dut.expect("Hello world!", not_matching="Flash verified")
