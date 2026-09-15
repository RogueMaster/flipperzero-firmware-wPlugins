from __future__ import annotations

import socket
import unittest
from unittest.mock import patch

from host.fibp_host.network import NetworkPolicyError, validate_and_resolve


class NetworkPolicyTests(unittest.TestCase):
    def test_accepts_https_when_every_dns_answer_is_public(self) -> None:
        records = [
            (socket.AF_INET, socket.SOCK_STREAM, 6, "", ("93.184.216.34", 443)),
            (
                socket.AF_INET6,
                socket.SOCK_STREAM,
                6,
                "",
                ("2606:2800:220:1:248:1893:25c8:1946", 443, 0, 0),
            ),
        ]
        with patch("socket.getaddrinfo", return_value=records):
            host, port, target, addresses = validate_and_resolve(
                "https://example.com/a?q=1"
            )
        self.assertEqual((host, port, target), ("example.com", 443, "/a?q=1"))
        self.assertEqual(len(addresses), 2)

    def test_rejects_non_https_and_localhost(self) -> None:
        with self.assertRaises(NetworkPolicyError):
            validate_and_resolve("http://example.com/")
        with self.assertRaises(NetworkPolicyError):
            validate_and_resolve("https://localhost/")

    def test_rejects_private_ip_literal(self) -> None:
        with self.assertRaises(NetworkPolicyError):
            validate_and_resolve("https://192.168.1.10/")
        with self.assertRaises(NetworkPolicyError):
            validate_and_resolve("https://[::1]/")

    def test_rejects_hostname_if_any_dns_answer_is_private(self) -> None:
        records = [
            (socket.AF_INET, socket.SOCK_STREAM, 6, "", ("93.184.216.34", 443)),
            (socket.AF_INET, socket.SOCK_STREAM, 6, "", ("10.0.0.4", 443)),
        ]
        with (
            patch("socket.getaddrinfo", return_value=records),
            self.assertRaises(NetworkPolicyError),
        ):
            validate_and_resolve("https://example.com/")

    def test_rejects_url_credentials_and_controls(self) -> None:
        with self.assertRaises(NetworkPolicyError):
            validate_and_resolve("https://user:secret@example.com/")
        with self.assertRaises(NetworkPolicyError):
            validate_and_resolve("https://example.com/\r\nX-Test: yes")


if __name__ == "__main__":
    unittest.main()
