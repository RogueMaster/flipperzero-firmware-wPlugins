"""
Auto transport: tries USB first, falls back to BLE.

This is the default mode — mirrors what the Flipper app does at startup:
plug a cable in → USB CDC; no cable → BLE.
"""

import logging

from .transport import Transport
from .transport_usb import UsbTransport
from .transport_bt import BtTransport

log = logging.getLogger(__name__)


class AutoTransport(Transport):
    def __init__(self):
        self._usb = UsbTransport()
        self._bt = BtTransport()
        self._active: Transport | None = None

    # ── Transport interface ────────────────────────────────────────

    async def connect(self) -> bool:
        log.info("Auto: trying USB…")
        if await self._usb.connect():
            self._active = self._usb
            log.info("Auto: using USB")
            return True

        log.info("Auto: USB not found, trying BLE…")
        if await self._bt.connect():
            self._active = self._bt
            log.info("Auto: using BLE")
            return True

        self._active = None
        return False

    async def readline(self) -> bytes:
        return await self._active.readline()

    async def write(self, data: bytes) -> None:
        await self._active.write(data)

    async def drain(self) -> None:
        await self._active.drain()

    async def get_rssi(self) -> int | None:
        if self._active is None:
            return None
        return await self._active.get_rssi()

    def close(self) -> None:
        if self._active:
            self._active.close()
            self._active = None

    async def aclose(self) -> None:
        if self._active:
            try:
                await self._active.aclose()
            finally:
                self._active = None

    @property
    def is_closing(self) -> bool:
        if self._active is None:
            return True
        return self._active.is_closing
