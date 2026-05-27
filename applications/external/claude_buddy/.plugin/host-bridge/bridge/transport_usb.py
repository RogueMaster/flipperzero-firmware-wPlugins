"""USB CDC transport — wraps serial_asyncio (extracted from serial_conn.py)."""

import glob
import logging

import serial_asyncio

from . import config
from .transport import Transport

log = logging.getLogger(__name__)


class UsbTransport(Transport):
    def __init__(self):
        self._reader = None
        self._writer = None

    # ── Transport interface ────────────────────────────────────────

    async def connect(self) -> bool:
        port = self._detect_port()
        if not port:
            log.warning("USB: no Flipper serial port found")
            return False
        try:
            self._reader, self._writer = await serial_asyncio.open_serial_connection(
                url=port, baudrate=config.SERIAL_BAUD
            )
            log.info("USB: connected on %s", port)
            return True
        except Exception as e:
            log.error("USB: connect failed on %s: %s", port, e)
            return False

    async def readline(self) -> bytes:
        return await self._reader.readline()

    async def write(self, data: bytes) -> None:
        self._writer.write(data)

    async def drain(self) -> None:
        await self._writer.drain()

    def close(self) -> None:
        if self._writer:
            try:
                self._writer.close()
            except Exception:
                pass

    @property
    def is_closing(self) -> bool:
        if self._writer:
            t = self._writer.transport
            return t is not None and t.is_closing()
        return False

    # ── Port detection ─────────────────────────────────────────────

    def _detect_port(self) -> str | None:
        if config.SERIAL_PORT:
            return config.SERIAL_PORT
        ports = sorted(glob.glob(config.SERIAL_GLOB_PATTERN))
        # Dual CDC: channel 0 = CLI, channel 1 = app (higher suffix)
        return ports[-1] if ports else None
