from __future__ import annotations

import unittest

from host.fibp_host.transforms import (
    NATIONAL_TODAY,
    RADIO_BROWSER,
    transform_for,
    transform_national_today,
    transform_radio_browser,
)
from scripts.fibp_codec import RequestStart


class HostTransformTests(unittest.TestCase):
    def test_detects_only_exact_special_endpoints(self) -> None:
        self.assertEqual(
            transform_for(RequestStart(1, 1000, "https://nationaltoday.com/today/")),
            NATIONAL_TODAY,
        )
        self.assertEqual(
            transform_for(
                RequestStart(
                    1,
                    1000,
                    "https://all.api.radio-browser.info/json/stations/search?limit=5",
                )
            ),
            RADIO_BROWSER,
        )
        self.assertIsNone(
            transform_for(RequestStart(1, 1000, "https://example.com/today/"))
        )

    def test_national_today_preserves_bold_markers_and_outputs_ascii(self) -> None:
        source = (
            '<div class="single-date-header-content"><p><b>World Test Day</b> '
            "celebrates safety &amp; Muğla — today.</p></div>"
        ).encode()
        self.assertEqual(
            transform_national_today(source),
            b"[[B]]World Test Day[[/B]]celebrates safety & Mugla - today.",
        )

    def test_radio_browser_keeps_only_small_https_mp3_streams(self) -> None:
        source = b"""[
          {"name":"Radio One","country":"UK","state":"London","codec":"MP3","bitrate":64,"url_resolved":"https://radio.example/live.mp3"},
          {"name":"Too Big","country":"US","codec":"MP3","bitrate":128,"url_resolved":"https://big.example/live"},
          {"name":"AAC","country":"US","codec":"AAC","bitrate":32,"url_resolved":"https://aac.example/live"}
        ]"""
        self.assertEqual(
            transform_radio_browser(source),
            b"FIBRADIO1\nRadio One\tUK\tLondon\t64\thttps://radio.example/live.mp3\n",
        )


if __name__ == "__main__":
    unittest.main()
