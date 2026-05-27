"""Connection manager: wraps a Transport with reconnect loop, ping, and message routing."""

import asyncio
import logging

from . import config, protocol
from .transport import Transport

log = logging.getLogger(__name__)


class SerialConnection:
    def __init__(self, transport: Transport):
        self._transport = transport
        self.connected = False
        self._on_message = None
        self._on_connect = None
        self._read_task: asyncio.Task | None = None
        self._ping_task: asyncio.Task | None = None

    def on_message(self, callback):
        self._on_message = callback

    def on_connect(self, callback):
        self._on_connect = callback

    # ── Connection ─────────────────────────────────────────────────

    async def connect(self) -> bool:
        ok = await self._transport.connect()
        if ok:
            self.connected = True
        return ok

    def _mark_disconnected(self):
        if not self.connected:
            return
        self.connected = False
        self._transport.close()

    def _cancel_tasks(self):
        for task in (self._read_task, self._ping_task):
            if task and not task.done():
                task.cancel()
        self._read_task = None
        self._ping_task = None

    def close(self):
        self._cancel_tasks()
        self._mark_disconnected()

    async def aclose(self):
        """Async shutdown: cancel tasks and await the transport disconnect.

        Used on daemon shutdown so the Flipper sees a clean BLE disconnect
        before we exit, letting the next bridge process reconnect
        immediately instead of waiting for the link-supervision timeout.
        """
        self._cancel_tasks()
        was_connected = self.connected
        self.connected = False
        if was_connected:
            await self._transport.aclose()
        else:
            self._transport.close()

    # ── Send ───────────────────────────────────────────────────────

    async def send(self, data: bytes):
        if not self.connected:
            log.warning("Serial send dropped (not connected): %s", data.rstrip())
            return
        try:
            log.debug("Serial TX: %s", data.rstrip())
            await self._transport.write(data)
            await self._transport.drain()
        except Exception as e:
            log.error("Serial write error: %s", e)
            self._mark_disconnected()

    async def send_ping(self) -> None:
        await self.send(protocol.ping_msg(await self._transport.get_rssi()))

    # ── Read loop ──────────────────────────────────────────────────

    async def read_loop(self):
        while self.connected:
            try:
                line = await self._transport.readline()
                if not line:
                    log.warning("Serial EOF")
                    self._mark_disconnected()
                    break
                log.debug("Serial RX raw: %r", line)
                msg = protocol.decode(line)
                if msg:
                    if self._on_message:
                        await self._on_message(msg)
                else:
                    log.warning("Serial RX parse failed: %r", line)
            except asyncio.CancelledError:
                break
            except Exception as e:
                log.error("Serial read error: %s", e)
                self._mark_disconnected()
                break

    # ── Ping loop ──────────────────────────────────────────────────

    async def ping_loop(self):
        while self.connected:
            await asyncio.sleep(config.PING_INTERVAL)
            if self.connected:
                await self.send_ping()

    # ── Reconnect loop ─────────────────────────────────────────────

    async def reconnect_loop(self):
        while True:
            if self.connected and self._transport.is_closing:
                log.warning("Transport closing, marking disconnected")
                self._mark_disconnected()

            if not self.connected:
                self._cancel_tasks()
                log.info("Attempting to reconnect…")
                if await self.connect():
                    self._read_task = asyncio.create_task(self.read_loop())
                    self._ping_task = asyncio.create_task(self.ping_loop())
                    if self._on_connect:
                        await self._on_connect()

            await asyncio.sleep(3.0)
