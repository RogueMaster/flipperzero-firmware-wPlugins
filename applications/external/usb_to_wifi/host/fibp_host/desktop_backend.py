"""UI-independent desktop ownership; USB/BLE never share consent or requests.

Qt only observes immutable snapshots and queues commands. Serial/GATT/session
mutations stay on each connection's worker; HTTPS stays in BridgeHost workers.
"""

from __future__ import annotations

import asyncio
import queue
import threading
import time
import uuid
from collections import deque
from concurrent.futures import Future
from dataclasses import dataclass, replace

from scripts.fibp_codec import MessageType, encode_permission_status
from .cli import BridgeHost, serve_port
from .permissions import PermissionStore
from .session import PermissionDecision
from .serial_ports import is_flipper_usb_device


def display_text(value, maximum=160):
    return "".join(c for c in str(value) if c.isprintable())[:maximum]


@dataclass(frozen=True)
class ConnectionSnapshot:
    link: str
    enabled: bool = False
    connected: bool = False
    name: str = "No Flipper connected"
    identity: str = "Device: —"
    status: str = "Not connected"
    access: bool = False
    active_request: bool = False
    error: str = ""


@dataclass(frozen=True)
class DesktopPrompt:
    id: str
    owner: str
    link: str
    kind: str
    name: str
    future: Future


class PromptBroker:
    """Non-blocking consent; stale/closed dialogs cannot grant a new session."""

    def __init__(self, notify=lambda: None):
        self.notify = notify
        self._lock = threading.Lock()
        self._pending = {}

    def request(self, owner, link, kind, name):
        item = DesktopPrompt(
            uuid.uuid4().hex, owner, link, kind, display_text(name, 32), Future()
        )
        with self._lock:
            self._pending[item.id] = item
        self.notify()
        return item

    def pending(self):
        with self._lock:
            return tuple(self._pending.values())

    def answer(self, prompt_id, answer):
        with self._lock:
            item = self._pending.pop(prompt_id, None)
            if item is not None and not item.future.done():
                item.future.set_result(answer)
        self.notify()

    def cancel(self, owner):
        with self._lock:
            for key, item in tuple(self._pending.items()):
                if item.owner == owner:
                    self._pending.pop(key)
                    if not item.future.done():
                        item.future.set_result(None)
        self.notify()


class DesktopHost(BridgeHost):
    def __init__(self, controller, worker, bluetooth=False):
        super().__init__(controller.permissions, None, False, bluetooth=bluetooth)
        self.controller, self.worker = controller, worker
        self.prompt = None
        self.prompt_nonce = None

    def log(self, message):
        self.controller.log(self.worker.link, message)

    def resolve_permission(self):
        hello = self.session.hello
        if not hello or not self.session.permission_pending:
            return []
        if self.prompt is not None and self.prompt_nonce != hello.client_nonce:
            self.controller.prompts.answer(self.prompt.id, None)
            self.prompt = None
        if self.prompt is None:
            self.prompt_nonce = hello.client_nonce
            self.prompt = self.controller.prompts.request(
                self.worker.owner, self.worker.link, "permission", hello.name
            )
        if not self.prompt.future.done():
            return []
        answer = self.prompt.future.result()
        self.prompt = None
        choice = (
            answer
            if isinstance(answer, PermissionDecision)
            else PermissionDecision.DENY
        )
        if choice == PermissionDecision.ALLOW_ALWAYS and not self.session.bluetooth:
            self.permission_store.grant(hello)
        return self.session.resolve_permission(choice)


def bridge_ports():
    """Probe official Flipper CDC devices; HELLO identifies the bridge port."""
    from serial.tools import list_ports

    ports = [p.device for p in list_ports.comports() if is_flipper_usb_device(p)]
    # The macOS CLI ends in 1 and bridge CDC in 3. Elsewhere this is only a
    # probe order; validated FIBP HELLO, not the port name, identifies the peer.
    return sorted(ports, key=lambda name: (not name.endswith("3"), name))


class ConnectionWorker:
    def __init__(self, controller, link):
        self.controller, self.link = controller, link
        self.owner = uuid.uuid4().hex
        self.stop_event = threading.Event()
        self.commands = queue.Queue(maxsize=32)
        self.thread = threading.Thread(
            target=self.run, daemon=True, name=f"bridge-desktop-{link}"
        )
        self.loop = self.task = self.host = None
        self.pairing = None
        self.predecessors = ()

    def stop(self):
        self.stop_event.set()
        self.controller.prompts.cancel(self.owner)
        if self.host and self.host.session.active:
            self.host.session.active.cancel.set()
        if self.loop and self.task:
            try:
                self.loop.call_soon_threadsafe(self.task.cancel)
            except RuntimeError:
                pass

    def command(self, action, value=None):
        try:
            self.commands.put_nowait((action, value))
        except queue.Full:
            self.controller.log(self.link, "Too many pending UI actions")

    def update(self, host, pairing=None):
        self.host = host
        self.pairing = pairing
        hello = host.session.hello or (pairing.hello if pairing else None)
        recognized = pairing is None or pairing.accepted
        access = recognized and host.session.permission in {
            PermissionDecision.ALLOW_ONCE,
            PermissionDecision.ALLOW_ALWAYS,
        }
        if pairing and not pairing.accepted:
            status = (
                "Enter the code shown on Flipper"
                if pairing.awaiting_code
                else "Select this computer in Flipper → Connection Requests"
            )
        elif host.session.permission_pending:
            status = "Waiting for internet permission"
        elif hello and not access:
            status = "Internet access denied"
        elif host.session.active:
            status = "Request in progress"
        elif access:
            status = "Internet access ready"
        else:
            status = "Waiting for Flipper handshake"
        snapshot = ConnectionSnapshot(
            self.link,
            True,
            bool(hello and recognized),
            display_text(hello.name, 32) if hello else "No Flipper connected",
            (
                f"Device: {display_text(hello.model, 16)} · ID …{hello.device_id.hex()[-8:].upper()}"
                if hello
                else "Device: —"
            ),
            status,
            access,
            host.session.active is not None,
        )
        self.controller.publish(self, snapshot)

    def tick(self, host, pairing=None):
        replies = []
        while True:
            try:
                action, value = self.commands.get_nowait()
            except queue.Empty:
                break
            hello = host.session.hello
            if action == "cancel":
                if host.session.active:
                    host.session.active.cancel.set()
            elif action in {"allow", "revoke"} and hello is not None:
                self.controller.prompts.cancel(self.owner)
                host.prompt = None
                if action == "allow":
                    choice = (
                        PermissionDecision.ALLOW_ONCE
                        if self.link == "bluetooth"
                        else PermissionDecision.ALLOW_ALWAYS
                    )
                    if self.link == "usb":
                        self.controller.permissions.grant(hello)
                    host.session.permission_pending = True
                    replies.extend(host.session.resolve_permission(choice))
                else:
                    if self.link == "usb":
                        self.controller.permissions.revoke(hello)
                    host.session.close()
                    host.session.permission_pending = False
                    host.session.permission = PermissionDecision.DENY
                    replies.append(
                        host.session._control_frame(
                            MessageType.PERMISSION_STATUS,
                            encode_permission_status(0, 2),
                        )
                    )
                    # Drop already-queued response data after permission revocation.
                    while not host.outgoing.empty():
                        try:
                            host.outgoing.get_nowait()
                        except queue.Empty:
                            break
            elif action == "forget" and pairing and pairing.hello:
                from .bluetooth_pairing import REVOKE, PairingStore
                from scripts.fibp_codec import Frame

                if PairingStore.key(pairing.hello) == value:
                    host.session.close()
                    host.session.permission = PermissionDecision.DENY
                    replies.append(Frame(REVOKE, pairing.prefix()))
                    self.stop_event.set()
        return replies

    async def ble(self):
        from .bluetooth_demo import serve_demo
        from .bluetooth_transport import connect_demo

        with self.controller.pairing_lock:
            store = self.controller.pairing_store()
        self.loop = asyncio.get_running_loop()
        self.task = asyncio.current_task()

        async def serve(transport):
            def prompt(_text, secret=False):
                hello = self.pairing.hello if self.pairing else None
                item = self.controller.prompts.request(
                    self.owner,
                    self.link,
                    "code" if secret else "permission",
                    hello.name if hello else "Flipper Zero",
                )

                # serve_demo owns its deadlines and cancels this waiter on disconnect.
                async def answer():
                    value = await asyncio.wrap_future(item.future)
                    if secret:
                        return value if isinstance(value, str) else ""
                    return "o" if value == PermissionDecision.ALLOW_ONCE else "d"

                return asyncio.ensure_future(answer())

            await serve_demo(
                transport,
                store,
                prompt=prompt,
                host_factory=lambda: DesktopHost(self.controller, self, bluetooth=True),
                on_update=self.update,
                on_tick=self.tick,
                stop=self.stop_event,
            )
            self.controller.refresh_pairings()

        await connect_demo(serve)

    def run(self):
        try:
            # Toggle/reconnect never opens the same port before the previous
            # owner has finished cancellation and closed its file descriptor.
            for previous in self.predecessors:
                while previous.thread.is_alive():
                    if self.stop_event.wait(0.05):
                        return
            self.predecessors = ()
            if self.stop_event.is_set():
                return
            if self.link == "bluetooth":
                asyncio.run(self.ble())
            else:
                while not self.stop_event.is_set():
                    devices = self.controller.port_provider()
                    if not devices:
                        self.controller.publish(
                            self,
                            ConnectionSnapshot(
                                self.link,
                                True,
                                status="Waiting for USB Internet Bridge on Flipper",
                            ),
                        )
                    for port in devices:
                        if self.stop_event.is_set():
                            break
                        host = DesktopHost(self.controller, self)
                        self.host = host
                        try:
                            serve_port(
                                port,
                                host,
                                stop=self.stop_event,
                                on_tick=self.tick,
                                on_update=self.update,
                            )
                        except (OSError, RuntimeError) as error:
                            self.controller.log(
                                self.link,
                                f"USB connection ended ({type(error).__name__}): {display_text(error)}",
                            )
                        finally:
                            host.close()
                            self.controller.prompts.cancel(self.owner)
                            self.host = None
                    self.stop_event.wait(0.5)
        except asyncio.CancelledError:
            pass
        except Exception as error:
            self.controller.publish(
                self, ConnectionSnapshot(self.link, False, error=display_text(error))
            )
            self.controller.log(
                self.link, f"Connection stopped: {type(error).__name__}"
            )
        finally:
            self.controller.prompts.cancel(self.owner)
            if self.host:
                self.host.close()
            self.controller.worker_ended(self)


class DesktopController:
    def __init__(
        self, permissions=None, port_provider=bridge_ports, pairing_store=None
    ):
        self.permissions = permissions if permissions is not None else PermissionStore()
        self.port_provider = port_provider
        self.pairing_lock = threading.RLock()
        self._pairing_store_factory = pairing_store
        self._pairing_store_cache = None
        self._lock = threading.RLock()
        self.on_change = lambda: None
        self.prompts = PromptBroker(lambda: self.on_change())
        self.snapshots = {
            link: ConnectionSnapshot(link) for link in ("usb", "bluetooth")
        }
        self.workers = {}
        self.retiring = {}
        self.records = ()
        self.logs = deque(maxlen=256)
        self.pairing_error = ""
        self._closed = False

    def pairing_store(self):
        with self.pairing_lock:
            if self._pairing_store_cache is None:
                if self._pairing_store_factory is not None:
                    store = self._pairing_store_factory()
                else:
                    from .bluetooth_pairing import PairingStore

                    store = PairingStore()
                self._pairing_store_cache = LockedPairingStore(store, self.pairing_lock)
            return self._pairing_store_cache

    def log(self, link, message):
        with self._lock:
            self.logs.append(
                (time.strftime("%H:%M:%S"), link.upper(), display_text(message))
            )
        self.on_change()

    def publish(self, worker, snapshot):
        with self._lock:
            if self.workers.get(worker.link) is not worker:
                return
            if self.snapshots[worker.link] == snapshot:
                return
            self.snapshots[worker.link] = snapshot
        self.on_change()

    def snapshot(self, link):
        with self._lock:
            return self.snapshots[link]

    def set_enabled(self, link, enabled):
        if link not in self.snapshots:
            raise ValueError("Unknown connection mode")
        with self._lock:
            if self._closed:
                return
            existing = self.workers.pop(link, None)
            if existing:
                self.retiring[existing.owner] = existing
                existing.stop()
            self.snapshots[link] = ConnectionSnapshot(
                link,
                enabled,
                status="Searching for Flipper Bridge" if enabled else "Disabled",
            )
            if enabled:
                worker = ConnectionWorker(self, link)
                # Include owners stopped by a previous Off, not only a direct
                # Reconnect. Off -> On must not race a still-closing USB port.
                worker.predecessors = tuple(
                    old for old in self.retiring.values() if old.link == link
                )
                self.workers[link] = worker
                worker.thread.start()
        self.on_change()

    def worker_ended(self, worker):
        with self._lock:
            self.retiring.pop(worker.owner, None)
            if self.workers.get(worker.link) is not worker:
                return
            self.workers.pop(worker.link)
            error = self.snapshots[worker.link].error
            self.snapshots[worker.link] = ConnectionSnapshot(
                worker.link,
                False,
                error=error,
                status="Connection ended; enable to reconnect",
            )
        self.on_change()

    def command(self, link, action):
        with self._lock:
            worker = self.workers.get(link)
            if worker:
                worker.command(action)

    def refresh_pairings(self):
        def work():
            try:
                with self.pairing_lock:
                    records = tuple(self.pairing_store().records())
                with self._lock:
                    self.records, self.pairing_error = records, ""
            except Exception as error:
                with self._lock:
                    self.pairing_error = display_text(error)
            self.on_change()

        threading.Thread(target=work, daemon=True, name="bridge-pairings-list").start()

    def revoke_pairing(self, key):
        def work():
            try:
                with self.pairing_lock:
                    store = self.pairing_store()
                    if key not in dict(store.records()):
                        return
                    store.revoke(key)
                    records = tuple(store.records())
                with self._lock:
                    self.records, self.pairing_error = records, ""
                    worker = self.workers.get("bluetooth")
                    if worker:
                        worker.command("forget", key)
            except Exception as error:
                with self._lock:
                    self.pairing_error = display_text(error)
            self.on_change()

        threading.Thread(target=work, daemon=True, name="bridge-pairing-revoke").start()

    def close(self):
        with self._lock:
            self._closed = True
            workers = tuple(
                {
                    worker.owner: worker
                    for worker in (*self.workers.values(), *self.retiring.values())
                }.values()
            )
            self.workers.clear()
            self.retiring.clear()
        for worker in workers:
            worker.stop()
        deadline = time.monotonic() + 2
        for worker in workers:
            if worker.thread.ident is not None:
                worker.thread.join(timeout=max(0, deadline - time.monotonic()))
        self.on_change = lambda: None


class LockedPairingStore:
    """One shared credential store; GUI revocation cannot race BLE acceptance."""

    def __init__(self, store, lock):
        self._store, self._lock = store, lock

    def __getattr__(self, name):
        with self._lock:
            value = getattr(self._store, name)
        if not callable(value):
            return value

        def call(*args, **kwargs):
            with self._lock:
                return value(*args, **kwargs)

        return call
