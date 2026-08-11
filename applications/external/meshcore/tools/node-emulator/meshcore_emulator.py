#!/usr/bin/env python3
"""
meshcore_emulator.py — a fake MeshCore companion node.

Speaks the companion serial protocol well enough that a real client believes it
is talking to a radio. It exists so the Flipper app (Logger / Messenger /
Configurator) and meshlog.py can be exercised end to end with no hardware.

This is a test fixture, not a firmware simulator. It answers requests and emits
plausible unsolicited events; it does not model the mesh.

Every byte layout here is taken from primary sources, never guessed:
  * framing and command codes — meshcore-dev/MeshCore docs/companion_protocol.md
    and examples/companion_radio/MyMesh.cpp
  * reply layouts — cross-checked against the reference Python client
    (`pip install meshcore`), which is what meshlog.py drives

Framing, the thing that is easiest to get backwards:

    frame = [type:u8][len:u16 little-endian][payload:len]
    type 0x3C '<'  app  -> radio   (what a client sends us)
    type 0x3E '>'  radio -> app    (what we send back)

Both directions confirmed three ways: the firmware's ArduinoSerialInterface
(writeFrame emits '>', checkRecvFrame waits for '<'), the reference Python
client (sends b"\\x3c", scans for 0x3e), and the meshcore_c constants.

Usage:
    python meshcore_emulator.py --tcp 127.0.0.1:5000
    python meshcore_emulator.py --tcp 127.0.0.1:5000 --scenario marginal
    python meshcore_emulator.py --serial COM7 --model v4 --name ROVER
"""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import logging
import random
import struct
import time
from collections import deque

LOG = logging.getLogger("emulator")

# ---------------------------------------------------------------- framing
FRAME_APP_TO_RADIO = 0x3C  # '<' — inbound, from the client
FRAME_RADIO_TO_APP = 0x3E  # '>' — outbound, from us

# The reference client rejects any frame claiming more than this.
MAX_FRAME_PAYLOAD = 300

# ---------------------------------------------------------------- commands
CMD_APP_START = 1
CMD_SEND_TXT_MSG = 2
CMD_SEND_CHANNEL_TXT_MSG = 3
CMD_GET_CONTACTS = 4
CMD_GET_DEVICE_TIME = 5
CMD_ADD_UPDATE_CONTACT = 9
CMD_SET_DEVICE_TIME = 6
CMD_SEND_SELF_ADVERT = 7
CMD_SET_ADVERT_NAME = 8
CMD_EXPORT_CONTACT = 17
CMD_SYNC_NEXT_MESSAGE = 10
CMD_SET_RADIO_PARAMS = 11
CMD_SET_RADIO_TX_POWER = 12
CMD_SET_ADVERT_LATLON = 14
CMD_GET_BATT_AND_STORAGE = 20
CMD_DEVICE_QUERY = 22
CMD_GET_CHANNEL = 31
CMD_SET_CHANNEL = 32
CMD_GET_STATS = 56
CMD_SET_PATH_HASH_MODE = 61

ERR_ILLEGAL_ARG = 2

# ---------------------------------------------------------------- responses
RESP_OK = 0
RESP_ERR = 1
RESP_CONTACTS_START = 2
RESP_CONTACT = 3
RESP_END_OF_CONTACTS = 4
RESP_SELF_INFO = 5
RESP_SENT = 6
RESP_CURR_TIME = 9
RESP_NO_MORE_MESSAGES = 10
RESP_CONTACT_URI = 11
RESP_BATT_AND_STORAGE = 12
RESP_DEVICE_INFO = 13
RESP_CONTACT_MSG_RECV_V3 = 16
RESP_CHANNEL_INFO = 18
RESP_STATS = 24

STATS_CORE, STATS_RADIO, STATS_PACKETS = 0, 1, 2

# ---------------------------------------------------------------- pushes
PUSH_ADVERT = 0x80
PUSH_SEND_CONFIRMED = 0x82  # the reference client surfaces this as ACK
PUSH_MSG_WAITING = 0x83
PUSH_LOG_RX_DATA = 0x88
PUSH_NEW_ADVERT = 0x8A  # carries a full contact record, unlike bare 0x80

# ---------------------------------------------------------------- scenarios
# Chosen against the thresholds the Logger judges a link by — SNR >= +5 dB and
# ping loss < 5 %. "good" clears both, "marginal" sits on the line, "drop"
# fails both, so the thresholds themselves can be exercised.
SCENARIOS = {
    "good": {"snr_mean": 9.0, "snr_spread": 2.0, "loss": 0.00, "rssi_base": -78},
    "marginal": {"snr_mean": 4.0, "snr_spread": 3.0, "loss": 0.15, "rssi_base": -104},
    "drop": {"snr_mean": -2.0, "snr_spread": 4.0, "loss": 0.60, "rssi_base": -117},
}

MODELS = {"t114": "Heltec T114", "v4": "Heltec V4"}

DEFAULT_CONTACTS = ["BASE", "REP1", "ROVER-M", "POCKET-S"]

# How often unsolicited traffic appears, in seconds. Fast enough that a test
# run produces data quickly, slow enough to look like a real quiet mesh.
RX_LOG_PERIOD = (1.5, 4.0)
ADVERT_PERIOD = (18.0, 30.0)
INCOMING_MSG_PERIOD = (25.0, 50.0)


def _cstr(text: str, width: int) -> bytes:
    """Fixed-width NUL-padded field, truncated if it does not fit."""
    raw = text.encode("utf-8")[:width]
    return raw + b"\0" * (width - len(raw))


def _pubkey_for(name: str) -> bytes:
    """Stable 32-byte identity per name, so restarts keep the same contacts."""
    return hashlib.sha256(("meshcore-emulator:" + name).encode()).digest()


def _contact_record(name: str, last_advert: int, lat: int, lon: int, lastmod: int) -> bytes:
    """The 147-byte contact record shared by the CONTACT (0x03) stream and the
    NEW_ADVERT (0x8A) push — both are parsed by the client as one mc_contact_t."""
    body = bytearray()
    body += _pubkey_for(name)  # 32
    body.append(1)  # type: chat node
    body.append(0)  # flags
    body.append(0)  # out_path_len
    body += b"\0" * 64  # out_path
    body += _cstr(name, 32)  # adv_name
    body += struct.pack("<I", last_advert)
    body += struct.pack("<i", lat)
    body += struct.pack("<i", lon)
    body += struct.pack("<I", lastmod)
    return bytes(body)


def _clamp_i8(value: int) -> int:
    return max(-128, min(127, value))


class NodeState:
    """What our fake node currently believes about itself."""

    def __init__(self, name: str, model: str, contacts: list[str], scenario: str):
        self.name = name
        self.model_key = model
        self.model = MODELS[model]
        self.public_key = _pubkey_for(name)
        self.started = time.monotonic()

        self.scenario_name = scenario
        self.scenario = SCENARIOS[scenario]

        # Radio settings in the units the wire actually uses: frequency in kHz,
        # bandwidth in Hz. They genuinely differ — the reference client encodes
        # set_radio as freq*1000 from MHz and bw*1000 from kHz.
        self.freq_khz = 869_525
        self.bw_hz = 250_000
        self.sf = 10
        self.cr = 5
        self.tx_power = 22
        self.max_tx_power = 30

        # Which path hash width this node sends with. Runtime, its own command,
        # persisted by real firmware -- so a preset apply has something to
        # change and something to read back. Bytes on the wire = mode + 1.
        self.path_hash_mode = 0

        # Advertised position: somewhere unremarkable, so logs look real.
        self.lat = 55_751_244  # degrees x 1e6
        self.lon = 37_618_423

        self.contacts = list(contacts)

        # Channel slots 0-7. Slot 0 is the well-known public channel (its secret
        # is the documented public key, not zeros, exactly like real firmware);
        # the rest start empty and are filled by SET_CHANNEL. Each is (name,
        # 16-byte secret).
        self.channels: dict[int, tuple[str, bytes]] = {
            0: ("public", bytes.fromhex("8b3387e9c5cdea6ac9e5edbaa115cd72"))
        }

        self.packets_recv = 0
        self.packets_sent = 0
        self.recv_errors = 0

        # SNR is a random walk around the scenario mean rather than fresh noise
        # each time: walking a route produces drift, not white noise, and the
        # difference matters to anything that smooths or thresholds the data.
        self._snr = self.scenario["snr_mean"]

        # Messages the node is holding for the client, drained by
        # SYNC_NEXT_MESSAGE exactly as real firmware does.
        self.mailbox: deque[bytes] = deque()

    @property
    def uptime(self) -> int:
        return int(time.monotonic() - self.started)

    def battery_mv(self) -> int:
        """A slow, monotonic discharge curve — 4.15 V falling ~1 mV/s."""
        return max(3300, 4150 - self.uptime)

    def next_snr_db(self) -> float:
        """One step of the walk, kept inside a plausible band."""
        spread = self.scenario["snr_spread"]
        self._snr += random.gauss(0, spread / 3.0)
        # Pull gently back towards the mean so it does not wander off forever.
        self._snr += (self.scenario["snr_mean"] - self._snr) * 0.15
        return max(-20.0, min(15.0, self._snr))


def fake_packet() -> bytes:
    """A byte string shaped like a MeshCore packet.

    The reference client runs the RX log payload through its packet parser and
    then reads route_type / payload_type out of the result, so this has to have
    a plausible header rather than being pure noise. Route types 0 and 3 carry
    a transport code, so 1 (flood) keeps it simple.
    """
    route_type = 1
    payload_type = random.choice([0, 1, 2, 3, 4])
    header = (payload_type << 2) | route_type
    path_len = random.randint(0, 3)
    path = bytes(random.getrandbits(8) for _ in range(path_len))
    body = bytes(random.getrandbits(8) for _ in range(random.randint(8, 40)))
    return bytes([header, path_len]) + path + body


class CompanionEmulator:
    """Turns inbound command payloads into outbound reply payloads.

    Deliberately transport-agnostic: TCP and serial both just move bytes.
    """

    def __init__(self, state: NodeState):
        self.state = state
        self.hub: "Hub | None" = None

    # -- individual replies -------------------------------------------------

    def _self_info(self) -> bytes:
        s = self.state
        body = bytearray()
        body.append(1)  # adv_type
        body.append(s.tx_power)
        body.append(s.max_tx_power)
        body += s.public_key  # 32
        body += struct.pack("<i", s.lat)
        body += struct.pack("<i", s.lon)
        body.append(0)  # multi_acks
        body.append(0)  # adv_loc_policy
        body.append(0)  # telemetry_mode
        body.append(0)  # manual_add_contacts
        body += struct.pack("<I", s.freq_khz)
        body += struct.pack("<I", s.bw_hz)
        body.append(s.sf)
        body.append(s.cr)
        body += s.name.encode("utf-8")
        return bytes([RESP_SELF_INFO]) + bytes(body)

    def _device_info(self) -> bytes:
        s = self.state
        # fw_ver 9 is the lowest that carries the repeater flag; anything >= 3
        # brings the model and version strings, which is what identifies us.
        # fw_ver 10 is the first that reports path_hash_mode, which is what a
        # preset apply reads back to prove it took.
        body = bytearray()
        body.append(10)  # fw_ver
        body.append(175)  # max_contacts / 2 -> 350
        body.append(20)  # max_channels
        body += struct.pack("<I", 123456)  # ble_pin
        body += _cstr("09-05-2026", 12)
        body += _cstr(s.model, 40)
        body += _cstr("v1.12.0", 20)
        body.append(0)  # repeat (fw_ver >= 9)
        body.append(s.path_hash_mode)  # (fw_ver >= 10)
        return bytes([RESP_DEVICE_INFO]) + bytes(body)

    def _battery(self) -> bytes:
        # [12][millivolts u16][used_kb u32][total_kb u32] -- straight from
        # MyMesh.cpp. The reference client calls this "level"; it is
        # millivolts, not a percentage.
        return bytes([RESP_BATT_AND_STORAGE]) + struct.pack(
            "<HII", self.state.battery_mv(), 512, 8192
        )

    def _stats(self, kind: int) -> bytes:
        s = self.state
        if kind == STATS_CORE:
            return bytes([RESP_STATS, STATS_CORE]) + struct.pack(
                "<HIHB", s.battery_mv(), s.uptime, s.recv_errors, 0
            )
        if kind == STATS_RADIO:
            noise = int(s.scenario["rssi_base"] - 10 + random.randint(-3, 3))
            return bytes([RESP_STATS, STATS_RADIO]) + struct.pack(
                "<hbbII",
                noise,
                _clamp_i8(int(s.scenario["rssi_base"])),
                _clamp_i8(int(s._snr * 4)),
                s.uptime // 10,
                s.uptime // 4,
            )
        if kind == STATS_PACKETS:
            return bytes([RESP_STATS, STATS_PACKETS]) + struct.pack(
                "<IIIIIII",
                s.packets_recv,
                s.packets_sent,
                s.packets_sent // 2,
                s.packets_sent - s.packets_sent // 2,
                s.packets_recv // 2,
                s.packets_recv - s.packets_recv // 2,
                s.recv_errors,
            )
        return bytes([RESP_ERR, 1])

    def _contact_frames(self) -> list[bytes]:
        """CONTACTS_START, one CONTACT each, then END_OF_CONTACTS."""
        s = self.state
        frames = [bytes([RESP_CONTACTS_START]) + struct.pack("<I", len(s.contacts))]

        now = int(time.time())
        for index, name in enumerate(s.contacts):
            # Heard from progressively longer ago, so "last seen" differs row to
            # row instead of every contact looking equally fresh.
            record = _contact_record(name, now - index * 600, s.lat, s.lon, now)
            frames.append(bytes([RESP_CONTACT]) + record)

        frames.append(bytes([RESP_END_OF_CONTACTS]) + struct.pack("<I", now))
        return frames

    def _send_text(self, payload: bytes) -> list[bytes]:
        """SEND_TXT_MSG: acknowledge the hand-off, then confirm delivery later.

        Delivery really is two steps on this protocol — MSG_SENT carries the
        tag to watch for, and the ACK push arrives when (if) the far end
        answers. Ping timing is measured across exactly that gap, so the delay
        and the loss have to live in the second step, not the first.
        """
        state = self.state
        state.packets_sent += 1

        tag = bytes(random.getrandbits(8) for _ in range(4))
        suggested_timeout = 6000
        reply = bytes([RESP_SENT, 1]) + tag + struct.pack("<I", suggested_timeout)

        lost = random.random() < state.scenario["loss"]
        if lost:
            LOG.info("ping tag %s: dropped (scenario %s)", tag.hex(), state.scenario_name)
        elif self.hub:
            # A real round trip over LoRa is tens to hundreds of milliseconds.
            delay = random.uniform(0.12, 0.55)
            self.hub.schedule(delay, bytes([PUSH_SEND_CONFIRMED]) + tag)

        UNUSED = payload  # the text itself does not change our behaviour
        del UNUSED
        return [reply]

    # -- dispatch -----------------------------------------------------------

    def handle(self, payload: bytes) -> list[bytes]:
        """Reply payloads for one command. Never empty: the reference client
        gives up after a few unanswered commands."""
        if not payload:
            return [bytes([RESP_ERR, 1])]

        cmd = payload[0]

        if cmd == CMD_APP_START:
            return [self._self_info()]

        if cmd == CMD_DEVICE_QUERY:
            return [self._device_info()]

        if cmd == CMD_GET_BATT_AND_STORAGE:
            return [self._battery()]

        if cmd == CMD_GET_STATS:
            kind = payload[1] if len(payload) > 1 else STATS_CORE
            return [self._stats(kind)]

        if cmd == CMD_GET_CONTACTS:
            return self._contact_frames()

        if cmd == CMD_GET_DEVICE_TIME:
            return [bytes([RESP_CURR_TIME]) + struct.pack("<I", int(time.time()))]

        if cmd == CMD_SET_DEVICE_TIME:
            return [bytes([RESP_OK])]

        if cmd in (CMD_SEND_TXT_MSG, CMD_SEND_CHANNEL_TXT_MSG):
            return self._send_text(payload)

        if cmd == CMD_SYNC_NEXT_MESSAGE:
            # One message per command, exactly as the firmware does it: the
            # client keeps asking until it hears NO_MORE_MESSAGES.
            if self.state.mailbox:
                return [self.state.mailbox.popleft()]
            return [bytes([RESP_NO_MORE_MESSAGES])]

        if cmd == CMD_SEND_SELF_ADVERT:
            # A real node gains contacts by *hearing* adverts, not sending them.
            # To make the "advert -> new peer appears" round trip testable with a
            # single emulator, pretend one neighbour heard us and advertised
            # back: add it once, so the next GET_CONTACTS shows one more row.
            if "NEWPEER" not in self.state.contacts:
                self.state.contacts.append("NEWPEER")
                LOG.info("advert heard back: added contact NEWPEER")
            return [bytes([RESP_OK])]

        if cmd == CMD_EXPORT_CONTACT:
            # NULL key (payload is just the command byte) means "export my own
            # card". The documented share format (docs.meshcore.io/qr_codes) is a
            # query URI the mobile app can add; type 1 == companion/chat node.
            name = self.state.name.replace(" ", "+")
            uri = (
                f"meshcore://contact/add?name={name}"
                f"&public_key={self.state.public_key.hex()}&type=1"
            ).encode("ascii")
            return [bytes([RESP_CONTACT_URI]) + uri]

        if cmd == CMD_ADD_UPDATE_CONTACT and len(payload) >= 132:
            # [9][pubkey 32][type][flags][path_len][path 64][adv_name 32]...
            # The client adds a contact from a shared link this way. Store the
            # name so it shows up in the next GET_CONTACTS — enough to prove the
            # import round trip; the emulator keys contacts by name, not the
            # imported pubkey, which is a test-fixture simplification.
            name = payload[100:132].split(b"\0", 1)[0].decode("utf-8", "ignore")
            if name and name not in self.state.contacts:
                self.state.contacts.append(name)
                LOG.info("contact imported via ADD_UPDATE_CONTACT: %s", name)
            return [bytes([RESP_OK])]

        if cmd == CMD_SET_ADVERT_NAME:
            self.state.name = payload[1:].decode("utf-8", "ignore")
            return [bytes([RESP_OK])]

        if cmd == CMD_SET_ADVERT_LATLON and len(payload) >= 9:
            self.state.lat, self.state.lon = struct.unpack("<ii", payload[1:9])
            return [bytes([RESP_OK])]

        if cmd == CMD_SET_RADIO_PARAMS and len(payload) >= 11:
            freq, bw, sf, cr = struct.unpack("<IIBB", payload[1:11])
            self.state.freq_khz, self.state.bw_hz = freq, bw
            self.state.sf, self.state.cr = sf, cr
            return [bytes([RESP_OK])]

        if cmd == CMD_SET_PATH_HASH_MODE:
            # [61][0][mode]. The zero second byte is mandatory and the firmware
            # rejects a mode of 3 or more; both are reproduced so a client that
            # gets either wrong finds out here rather than on a real node.
            if len(payload) < 3 or payload[1] != 0:
                return [bytes([RESP_ERR, ERR_ILLEGAL_ARG])]
            if payload[2] >= 3:
                return [bytes([RESP_ERR, ERR_ILLEGAL_ARG])]
            self.state.path_hash_mode = payload[2]
            return [bytes([RESP_OK])]

        if cmd == CMD_SET_RADIO_TX_POWER and len(payload) >= 5:
            self.state.tx_power = struct.unpack("<I", payload[1:5])[0]
            return [bytes([RESP_OK])]

        if cmd == CMD_GET_CHANNEL:
            index = payload[1] if len(payload) > 1 else 0
            name, secret = self.state.channels.get(index, ("", b"\0" * 16))
            body = bytes([index]) + _cstr(name, 32) + secret
            return [bytes([RESP_CHANNEL_INFO]) + body]

        if cmd == CMD_SET_CHANNEL and len(payload) >= 50:
            # [32][idx][name 32][secret 16]. Store it so GET_CHANNEL reads it
            # back, which is how the client confirms a create/join took.
            index = payload[1]
            name = payload[2:34].split(b"\0", 1)[0].decode("utf-8", "ignore")
            secret = payload[34:50]
            if name:
                self.state.channels[index] = (name, secret)
                LOG.info("channel set: slot %d = %s", index, name)
            elif index in self.state.channels and index != 0:
                del self.state.channels[index]  # empty name clears a private slot
            return [bytes([RESP_OK])]

        # A real node rejects a command it does not support with PACKET_ERROR
        # (0x01) code 1, not a bare OK. Mirroring that keeps the emulator from
        # masking an app bug where the client sends something the firmware would
        # refuse. Every command the app actually builds is handled above.
        LOG.info("unhandled command 0x%02X, answering ERR(1)", cmd)
        return [bytes([RESP_ERR, 1])]


class FrameCodec:
    """Reassembles inbound frames from a byte stream."""

    def __init__(self):
        self.buffer = bytearray()

    @staticmethod
    def encode(payload: bytes) -> bytes:
        if len(payload) > MAX_FRAME_PAYLOAD:
            raise ValueError(f"payload too large: {len(payload)}")
        return bytes([FRAME_RADIO_TO_APP]) + struct.pack("<H", len(payload)) + payload

    def feed(self, data: bytes) -> list[bytes]:
        """Append bytes, return every complete payload now available."""
        self.buffer += data
        out = []

        while True:
            # Resync: drop anything before a plausible lead byte, the way a
            # real node does when console noise shares the line.
            start = self.buffer.find(bytes([FRAME_APP_TO_RADIO]))
            if start < 0:
                self.buffer.clear()
                return out
            if start > 0:
                del self.buffer[:start]

            if len(self.buffer) < 3:
                return out

            length = struct.unpack("<H", self.buffer[1:3])[0]
            if length > MAX_FRAME_PAYLOAD:
                del self.buffer[:1]  # bogus length: skip the lead and resync
                continue
            if len(self.buffer) < 3 + length:
                return out

            out.append(bytes(self.buffer[3 : 3 + length]))
            del self.buffer[: 3 + length]


class Hub:
    """Everything that pushes bytes at whoever is currently connected.

    Unsolicited events are the half of this fixture that makes it useful: a
    node that only answers questions cannot exercise a passive logger.
    """

    def __init__(self, emulator: CompanionEmulator):
        self.emulator = emulator
        self.state = emulator.state
        self.writers: set = set()
        self._tasks: set[asyncio.Task] = set()
        emulator.hub = self

    # -- plumbing -----------------------------------------------------------

    def add(self, writer) -> None:
        self.writers.add(writer)

    def remove(self, writer) -> None:
        self.writers.discard(writer)

    def broadcast(self, payload: bytes) -> None:
        # Pushes are the half of the traffic that answers no question, so
        # nothing else in the log accounts for them. Without this line an ACK
        # that was sent and an ACK that was never sent look identical from the
        # client side.
        LOG.debug("=> push 0x%02X (%d bytes)", payload[0], len(payload))
        frame = FrameCodec.encode(payload)
        for writer in list(self.writers):
            try:
                writer.write(frame)
            except Exception:  # noqa: BLE001 - a dead client is not our problem
                self.writers.discard(writer)

    def schedule(self, delay: float, payload: bytes) -> None:
        """Send something after a delay, without blocking the caller."""

        async def later():
            await asyncio.sleep(delay)
            self.broadcast(payload)

        task = asyncio.create_task(later())
        self._tasks.add(task)
        task.add_done_callback(self._tasks.discard)

    # -- the events themselves ---------------------------------------------

    async def rx_log_loop(self) -> None:
        """Every packet the radio hears, with SNR that drifts as you walk."""
        while True:
            await asyncio.sleep(random.uniform(*RX_LOG_PERIOD))
            if not self.writers:
                continue

            snr_db = self.state.next_snr_db()
            rssi = int(self.state.scenario["rssi_base"] + random.randint(-6, 6))
            self.state.packets_recv += 1
            if random.random() < 0.02:
                self.state.recv_errors += 1

            payload = (
                bytes([PUSH_LOG_RX_DATA])
                + struct.pack("<bb", _clamp_i8(int(snr_db * 4)), _clamp_i8(rssi))
                + fake_packet()
            )
            self.broadcast(payload)

    async def advert_loop(self) -> None:
        while True:
            await asyncio.sleep(random.uniform(*ADVERT_PERIOD))
            if not self.writers or not self.state.contacts:
                continue
            who = random.choice(self.state.contacts)
            # Real firmware sends NEW_ADVERT (0x8A) with a full contact record
            # when it hears a fresh advert, and the bare ADVERT (0x80, key only)
            # otherwise. Alternate so both client paths get exercised.
            if random.random() < 0.5:
                now = int(time.time())
                record = _contact_record(who, now, self.state.lat, self.state.lon, now)
                self.broadcast(bytes([PUSH_NEW_ADVERT]) + record)
            else:
                self.broadcast(bytes([PUSH_ADVERT]) + _pubkey_for(who))

    async def incoming_msg_loop(self) -> None:
        """Queue a message, then tickle the client to come and drain it.

        This is how the protocol really delivers mail: there is no push that
        carries the text. The node says "something is waiting" and the client
        pulls with SYNC_NEXT_MESSAGE until told there is nothing left.
        """
        counter = 0
        while True:
            await asyncio.sleep(random.uniform(*INCOMING_MSG_PERIOD))
            if not self.writers or not self.state.contacts:
                continue

            counter += 1
            sender = random.choice(self.state.contacts)
            text = f"hello from {sender} #{counter}"

            body = bytearray()
            body.append(_clamp_i8(int(self.state._snr * 4)) & 0xFF)  # snr, quarter-dB
            body.append(_clamp_i8(int(self.state.scenario["rssi_base"])) & 0xFF)
            body.append(0)  # reserved
            body += _pubkey_for(sender)[:6]
            body.append(0xFF)  # path_len: heard direct
            body.append(0)  # txt_type: plain
            body += struct.pack("<I", int(time.time()))
            body += text.encode("utf-8")

            self.state.mailbox.append(bytes([RESP_CONTACT_MSG_RECV_V3]) + bytes(body))
            self.broadcast(bytes([PUSH_MSG_WAITING]))
            LOG.info("queued an incoming message from %s", sender)

    def start(self) -> None:
        for coro in (self.rx_log_loop(), self.advert_loop(), self.incoming_msg_loop()):
            task = asyncio.create_task(coro)
            self._tasks.add(task)
            task.add_done_callback(self._tasks.discard)


async def serve_tcp(host: str, port: int, emulator: CompanionEmulator, hub: Hub) -> None:
    async def on_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        peer = writer.get_extra_info("peername")
        LOG.info("client connected: %s", peer)
        codec = FrameCodec()
        hub.add(writer)
        try:
            while True:
                data = await reader.read(4096)
                if not data:
                    break
                for payload in codec.feed(data):
                    LOG.debug("<- cmd 0x%02X (%d bytes)", payload[0], len(payload))
                    for reply in emulator.handle(payload):
                        LOG.debug("-> resp 0x%02X (%d bytes)", reply[0], len(reply))
                        writer.write(FrameCodec.encode(reply))
                    await writer.drain()
        except (ConnectionResetError, BrokenPipeError):
            pass
        finally:
            hub.remove(writer)
            LOG.info("client gone: %s", peer)
            writer.close()

    server = await asyncio.start_server(on_client, host, port)
    LOG.info("listening on %s:%d", host, port)
    hub.start()
    async with server:
        await server.serve_forever()


async def serve_serial(port: str, baud: int, emulator: CompanionEmulator, hub: Hub) -> None:
    """Same node, on a real wire — for driving a Flipper through a USB-UART
    bridge. The protocol layer does not change; only the pipe does."""
    try:
        import serial_asyncio  # type: ignore
    except ImportError:
        import serial_asyncio_fast as serial_asyncio  # type: ignore

    reader, writer = await serial_asyncio.open_serial_connection(url=port, baudrate=baud)
    LOG.info("serial open: %s @ %d", port, baud)

    codec = FrameCodec()
    hub.add(writer)
    hub.start()
    try:
        while True:
            data = await reader.read(256)
            if not data:
                await asyncio.sleep(0.01)
                continue
            for payload in codec.feed(data):
                LOG.debug("<- cmd 0x%02X (%d bytes)", payload[0], len(payload))
                for reply in emulator.handle(payload):
                    # Logged here as well as on the TCP path. Without it a
                    # serial session shows only what the client sent, and a
                    # reply that never arrived looks exactly like a reply the
                    # client ignored.
                    LOG.debug("-> resp 0x%02X (%d bytes)", reply[0], len(reply))
                    writer.write(FrameCodec.encode(reply))
                await writer.drain()
    finally:
        hub.remove(writer)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Fake MeshCore companion node")
    transport = parser.add_mutually_exclusive_group(required=True)
    transport.add_argument("--tcp", metavar="HOST:PORT", help="e.g. 127.0.0.1:5000")
    transport.add_argument("--serial", metavar="PORT", help="e.g. COM7 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--model", choices=sorted(MODELS), default="t114")
    parser.add_argument("--name", default="EMU-1")
    parser.add_argument("--scenario", choices=sorted(SCENARIOS), default="good")
    parser.add_argument(
        "--contacts",
        default=",".join(DEFAULT_CONTACTS),
        help="comma-separated fake contact names",
    )
    parser.add_argument("--seed", type=int, help="fix the RNG for reproducible runs")
    parser.add_argument("--verbose", "-v", action="store_true")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%H:%M:%S",
    )
    if args.seed is not None:
        random.seed(args.seed)

    contacts = [c.strip() for c in args.contacts.split(",") if c.strip()]
    state = NodeState(args.name, args.model, contacts, args.scenario)
    emulator = CompanionEmulator(state)
    hub = Hub(emulator)

    LOG.info(
        "pretending to be %s (%s), scenario %s, contacts: %s",
        state.name,
        state.model,
        state.scenario_name,
        contacts,
    )

    try:
        if args.serial:
            asyncio.run(serve_serial(args.serial, args.baud, emulator, hub))
        else:
            host, _, port = args.tcp.partition(":")
            asyncio.run(serve_tcp(host or "127.0.0.1", int(port or 5000), emulator, hub))
    except KeyboardInterrupt:
        LOG.info("stopped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
