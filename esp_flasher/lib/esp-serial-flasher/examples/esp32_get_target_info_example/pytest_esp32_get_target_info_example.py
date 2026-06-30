import re
import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
def test_esp32_get_target_info_example(dut: Dut) -> None:
    dut.expect(re.compile(r"Target flash size \[B\]: \d+"))

    dut.expect("Target WIFI MAC:")
    dut.expect(re.compile(r"([0-9A-Fa-f]{2} ){5}[0-9A-Fa-f]{2}"))

    dut.expect("Target Security Information:")
    dut.expect(re.compile(r"Target chip: ESP32-[A-Z0-9]+"))
    dut.expect(re.compile(r"Eco version number: \d+"))

    dut.expect(re.compile(r"Secure boot: (ENABLED|DISABLED)"))
    dut.expect(re.compile(r"Secure boot agressive revoke: (ENABLED|DISABLED)"))
    dut.expect(re.compile(r"Flash encryption: (ENABLED|DISABLED)"))
    dut.expect(re.compile(r"Secure download mode: (ENABLED|DISABLED)"))

    dut.expect(re.compile(r"Secure boot key 0 revoked: (TRUE|FALSE)"))
    dut.expect(re.compile(r"Secure boot key 1 revoked: (TRUE|FALSE)"))
    dut.expect(re.compile(r"Secure boot key 2 revoked: (TRUE|FALSE)"))

    dut.expect(re.compile(r"JTAG access: (ENABLED|DISABLED)"))
    dut.expect(re.compile(r"USB access: (ENABLED|DISABLED)"))

    dut.expect(re.compile(r"Data cache in UART download mode: (ENABLED|DISABLED)"))
    dut.expect(
        re.compile(r"Instruction cache in UART download mode: (ENABLED|DISABLED)")
    )
