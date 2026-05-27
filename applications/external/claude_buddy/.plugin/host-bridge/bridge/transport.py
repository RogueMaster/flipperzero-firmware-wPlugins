"""Abstract transport interface for the host bridge."""

from abc import ABC, abstractmethod


class Transport(ABC):
    """Raw byte transport: connect, read lines, write bytes, close."""

    @abstractmethod
    async def connect(self) -> bool:
        """Open the connection. Returns True on success."""

    @abstractmethod
    async def readline(self) -> bytes:
        """Read until newline and return the complete line (including \\n)."""

    @abstractmethod
    async def write(self, data: bytes) -> None:
        """Enqueue data for sending."""

    @abstractmethod
    async def drain(self) -> None:
        """Flush any buffered outgoing data."""

    @abstractmethod
    def close(self) -> None:
        """Close the connection (best-effort, non-blocking)."""

    async def aclose(self) -> None:
        """Close and wait for underlying resources to release.

        Default implementation delegates to ``close()``.  Transports whose
        clean shutdown is asynchronous (e.g. BLE GATT disconnect) should
        override this to actually await the disconnect — otherwise the
        remote peer has to wait for the link-supervision timeout before it
        notices we're gone, which blocks the next bridge process from
        reconnecting.
        """
        self.close()

    @property
    @abstractmethod
    def is_closing(self) -> bool:
        """True when the underlying connection has been lost or is closing."""

    async def get_rssi(self) -> int | None:
        """Return link RSSI in dBm when the transport can provide it."""
        return None
