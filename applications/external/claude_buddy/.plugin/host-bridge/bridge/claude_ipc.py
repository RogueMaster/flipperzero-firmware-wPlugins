"""Unix domain socket server for Claude Code hooks IPC.

Simple request/response: hooks connect, send a JSON action, get a response.
"""

import asyncio
import json
import logging
import os

from . import config

log = logging.getLogger(__name__)


class ClaudeIPC:
    def __init__(self):
        self._server: asyncio.Server | None = None
        self._on_action = None

    def on_action(self, callback):
        self._on_action = callback

    async def start(self):
        if os.path.exists(config.SOCKET_PATH):
            os.unlink(config.SOCKET_PATH)
        self._server = await asyncio.start_unix_server(
            self._handle_client, path=config.SOCKET_PATH
        )
        os.chmod(config.SOCKET_PATH, 0o666)
        log.info("IPC listening on %s", config.SOCKET_PATH)

    async def _handle_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ):
        try:
            data = await asyncio.wait_for(reader.read(65536), timeout=10.0)
            if not data:
                return

            request = json.loads(data.decode().strip())

            if self._on_action:
                response = await self._on_action(request)
            else:
                response = {"status": "ok"}

            writer.write(json.dumps(response).encode() + b"\n")
            await writer.drain()
        except asyncio.TimeoutError:
            log.warning("IPC: read timeout")
        except (ConnectionError, BrokenPipeError):
            log.debug("IPC: client disconnected")
        except Exception as e:
            log.error("IPC: error: %s", e)
        finally:
            writer.close()
            try:
                await writer.wait_closed()
            except Exception:
                pass

    async def stop(self):
        if self._server:
            self._server.close()
            await self._server.wait_closed()
        if os.path.exists(config.SOCKET_PATH):
            os.unlink(config.SOCKET_PATH)
