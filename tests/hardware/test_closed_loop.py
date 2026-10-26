import tempfile
import unittest
from pathlib import Path

from tests.hardware.credential import load_read_fixture
from tests.hardware.pcsc import legacy_cmac, native_apdu
from tests.hardware.report import write_reports
from tests.hardware.run_closed_loop import FIXTURES, cases_for


class ClosedLoopUnitTests(unittest.TestCase):
    def test_additional_frame_uses_five_byte_apdu(self):
        self.assertEqual(native_apdu(0xAF), bytes.fromhex("90AF000000"))

    def test_cmac_is_deterministic_and_chained(self):
        key = bytes.fromhex("CB71BE7F218DB3766FD93192A7551E74")
        first = legacy_cmac(key, bytes(8), b"test")
        self.assertEqual(first, legacy_cmac(key, bytes(8), b"test"))
        self.assertNotEqual(first, legacy_cmac(key, first, b"test"))

    def test_fixtures_match_suite_contract(self):
        smoke = load_read_fixture(FIXTURES / "iso_2k3des_mac_1k.dfc")
        plain = load_read_fixture(FIXTURES / "ev3_plain_2k.dfc")
        plain_second = load_read_fixture(FIXTURES / "ev3_plain_2k.dfc", file_index=1)
        self.assertEqual(len(smoke.data), 1024)
        self.assertEqual(len(smoke.key), 16)
        self.assertEqual(smoke.key_number, 1)
        self.assertEqual(len(plain.data) + len(plain_second.data), 2048)
        self.assertEqual(len(cases_for("smoke")), 1)
        self.assertEqual(len(cases_for("matrix")), 2)

    def test_reports_include_json_and_junit_failure(self):
        run = {
            "cases": [
                {
                    "suite": "smoke",
                    "name": "case",
                    "cycle": 1,
                    "status": "failed",
                    "error": "expected",
                    "duration_ms": 1.0,
                }
            ]
        }
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            write_reports(output, run)
            self.assertIn('"status": "failed"', (output / "results.json").read_text())
            self.assertIn("<failure", (output / "junit.xml").read_text())


if __name__ == "__main__":
    unittest.main()
