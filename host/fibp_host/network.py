from __future__ import annotations

import http.client
import ipaddress
import socket
import ssl
import time
from collections.abc import Callable
from dataclasses import dataclass
from threading import Event
from urllib.parse import urljoin, urlsplit, urlunsplit

from scripts.fibp_codec import HeaderField, RequestStart, encode_header

from .transforms import apply_transform, maximum_source_bytes, transform_for

MAX_REDIRECTS = 3
MAX_RESPONSE_HEADERS = 8
READ_SIZE = 192
REQUEST_HEADER_ALLOWLIST = frozenset({"accept", "accept-language", "content-type"})
RESPONSE_HEADER_ALLOWLIST = frozenset(
    {
        "content-type",
        "content-language",
        "content-length",
        "date",
        "last-modified",
        "etag",
        "cache-control",
    }
)


class NetworkPolicyError(ValueError):
    pass


class NetworkRequestError(RuntimeError):
    pass


@dataclass(frozen=True)
class FetchResult:
    bytes_sent: int
    truncated: bool
    cancelled: bool


def _public_ip(value: str) -> bool:
    address = ipaddress.ip_address(value.split("%", 1)[0])
    return address.is_global and not (
        address.is_private
        or address.is_loopback
        or address.is_link_local
        or address.is_multicast
        or address.is_reserved
        or address.is_unspecified
    )


def validate_and_resolve(url: str) -> tuple[str, int, str, list[str]]:
    if len(url.encode("utf-8")) > 384 or any(
        ord(char) < 0x20 or ord(char) == 0x7F for char in url
    ):
        raise NetworkPolicyError(
            "URL is empty, too long, or contains control characters"
        )
    try:
        url.encode("ascii")
    except UnicodeEncodeError as error:
        raise NetworkPolicyError("URL must use ASCII/percent encoding") from error

    parts = urlsplit(url)
    if parts.scheme.lower() != "https":
        raise NetworkPolicyError("only HTTPS URLs are allowed")
    if not parts.hostname or parts.username is not None or parts.password is not None:
        raise NetworkPolicyError("URL host is missing or contains credentials")
    try:
        port = parts.port or 443
    except ValueError as error:
        raise NetworkPolicyError("URL port is invalid") from error
    if not 1 <= port <= 65535:
        raise NetworkPolicyError("URL port is invalid")

    hostname = parts.hostname.rstrip(".").lower()
    if hostname in {"localhost", "localhost.localdomain"} or hostname.endswith(
        ".localhost"
    ):
        raise NetworkPolicyError("localhost is blocked")
    try:
        literal = ipaddress.ip_address(hostname.split("%", 1)[0])
    except ValueError:
        literal = None
    if literal is not None and not _public_ip(str(literal)):
        raise NetworkPolicyError("local and private IP addresses are blocked")

    try:
        records = socket.getaddrinfo(hostname, port, type=socket.SOCK_STREAM)
    except socket.gaierror as error:
        raise NetworkRequestError(f"DNS lookup failed: {error}") from error
    addresses = list(dict.fromkeys(record[4][0] for record in records))
    if not addresses:
        raise NetworkRequestError("DNS lookup returned no addresses")
    if any(not _public_ip(address) for address in addresses):
        raise NetworkPolicyError("DNS resolved to a local or private address")

    target = urlunsplit(("", "", parts.path or "/", parts.query, ""))
    return hostname, port, target, addresses


def _request_once(
    method: str,
    url: str,
    headers: list[HeaderField],
    body: bytes,
    timeout: float,
) -> tuple[http.client.HTTPResponse, ssl.SSLSocket]:
    hostname, port, target, addresses = validate_and_resolve(url)
    last_error: OSError | None = None
    raw_socket: socket.socket | None = None
    for address in addresses:
        try:
            raw_socket = socket.create_connection((address, port), timeout=timeout)
            break
        except OSError as error:
            last_error = error
    if raw_socket is None:
        raise NetworkRequestError(f"connection failed: {last_error}")

    context = ssl.create_default_context()
    try:
        tls = context.wrap_socket(raw_socket, server_hostname=hostname)
        tls.settimeout(timeout)
        host_header = hostname
        if ":" in hostname:
            host_header = f"[{hostname}]"
        if port != 443:
            host_header += f":{port}"
        outgoing = {
            "Host": host_header,
            "Connection": "close",
            "Accept-Encoding": "identity",
            "User-Agent": "Flipper-USB-Internet-Bridge/0.3",
        }
        for item in headers:
            if item.name.lower() in REQUEST_HEADER_ALLOWLIST:
                outgoing[item.name] = item.value
        if body:
            outgoing["Content-Length"] = str(len(body))
        request_head = (
            f"{method} {target} HTTP/1.1\r\n"
            + "".join(f"{name}: {value}\r\n" for name, value in outgoing.items())
            + "\r\n"
        )
        tls.sendall(request_head.encode("ascii") + body)
        response = http.client.HTTPResponse(tls)
        response.begin()
        return response, tls
    except Exception:
        raw_socket.close()
        raise


def perform_request(
    start: RequestStart,
    headers: list[HeaderField],
    body: bytes,
    maximum_bytes: int,
    cancel: Event,
    on_start: Callable[[int, list[HeaderField], int], None],
    on_chunk: Callable[[bytes], None],
) -> FetchResult:
    method = "GET" if start.method == 1 else "POST"
    current_url = start.url
    current_body = body
    deadline = time.monotonic() + min(start.timeout_ms, 30000) / 1000.0

    for redirect_index in range(MAX_REDIRECTS + 1):
        if cancel.is_set():
            return FetchResult(0, False, True)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("network request timed out")
        response, connection = _request_once(
            method, current_url, headers, current_body, min(remaining, 30.0)
        )
        try:
            if response.status in {301, 302, 303, 307, 308}:
                location = response.getheader("location")
                if not location:
                    raise NetworkRequestError("redirect has no Location header")
                if redirect_index >= MAX_REDIRECTS:
                    raise NetworkPolicyError("too many redirects")
                current_url = urljoin(current_url, location)
                # Validate before the next loop performs any connection.
                validate_and_resolve(current_url)
                if response.status == 303 or (
                    response.status in {301, 302} and method == "POST"
                ):
                    method, current_body = "GET", b""
                continue

            transform = transform_for(start)
            if transform is not None:
                source_limit = maximum_source_bytes(transform)
                source = response.read(source_limit + 1)
                if len(source) > source_limit:
                    raise NetworkRequestError(
                        "special endpoint response exceeds its source limit"
                    )
                transformed = apply_transform(transform, source)[:maximum_bytes]
                transformed_headers = [
                    HeaderField("content-type", "text/plain; charset=utf-8")
                ]
                on_start(response.status, transformed_headers, len(transformed))
                sent = 0
                for offset in range(0, len(transformed), READ_SIZE):
                    if cancel.is_set():
                        return FetchResult(sent, False, True)
                    chunk = transformed[offset : offset + READ_SIZE]
                    on_chunk(chunk)
                    sent += len(chunk)
                return FetchResult(sent, False, cancel.is_set())

            response_headers: list[HeaderField] = []
            for name, value in response.getheaders():
                if (
                    name.lower() in RESPONSE_HEADER_ALLOWLIST
                    and len(response_headers) < MAX_RESPONSE_HEADERS
                ):
                    candidate = HeaderField(name.lower(), value[:256])
                    try:
                        encode_header(candidate)
                    except ValueError:
                        continue
                    response_headers.append(candidate)
            declared = 0xFFFFFFFF
            content_length = response.getheader("content-length")
            if content_length and content_length.isdecimal():
                parsed = int(content_length)
                if parsed <= maximum_bytes:
                    declared = parsed
            on_start(response.status, response_headers, declared)

            sent = 0
            truncated = False
            while not cancel.is_set():
                if time.monotonic() >= deadline:
                    raise TimeoutError("network request timed out")
                chunk = response.read(min(READ_SIZE, maximum_bytes - sent + 1))
                if not chunk:
                    break
                allowed = min(len(chunk), maximum_bytes - sent)
                if allowed:
                    on_chunk(chunk[:allowed])
                    sent += allowed
                if allowed != len(chunk) or sent >= maximum_bytes:
                    truncated = (
                        response.read(1) != b"" if allowed == len(chunk) else True
                    )
                    break
            return FetchResult(sent, truncated, cancel.is_set())
        finally:
            connection.close()

    raise NetworkPolicyError("too many redirects")
