
import sys
import subprocess
import struct
import time
import asyncio
import threading
from tkinter import *
from tkinter import ttk, messagebox

def install_package(package: str):
    subprocess.check_call([sys.executable, "-m", "pip", "install", package])

def check_and_install_dependencies() -> bool:
    required = {"TikTokLive": "TikTokLive", "bleak": "bleak"}
    missing = []
    for import_name, pkg_name in required.items():
        try:
            __import__(import_name)
        except ImportError:
            missing.append(pkg_name)

    if missing:
        print("Missing dependencies – installing...")
        for pkg in missing:
            try:
                install_package(pkg)
                print(f"  ✓ {pkg}")
            except Exception as e:
                print(f"  ✗ {pkg}: {e}")
                return False
    return True

if not check_and_install_dependencies():
    print("\nFailed to install dependencies.")
    print("Run manually: pip install TikTokLive bleak")
    sys.exit(1)

from bleak import BleakClient, BleakScanner
from TikTokLive import TikTokLiveClient
from TikTokLive.client.web.web_settings import WebDefaults
from TikTokLive.events import (
    ConnectEvent,
    DisconnectEvent,
    CommentEvent,
    LikeEvent,
    GiftEvent,
    FollowEvent,
)

USERNAME_LEN = 17
MESSAGE_LEN  = 65

PACKET_FORMAT = f"<B{USERNAME_LEN}s{MESSAGE_LEN}s"
PACKET_SIZE   = struct.calcsize(PACKET_FORMAT)

MSG_TYPE_CHAT   = 0
MSG_TYPE_LIKE   = 1
MSG_TYPE_GIFT   = 2
MSG_TYPE_FOLLOW = 3

SERIAL_SERVICE_UUID = "8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000"
SERIAL_RX_CHAR_UUID = "19ed82ae-ed21-4c9d-4145-228e62fe0000"
DEVICE_NAME_PREFIX  = "FlipTok"

CONFIG_FILE = "tiktok_server_config.json"

_TRANSLIT_MAP = {
    'ą': 'a', 'ć': 'c', 'ę': 'e', 'ł': 'l', 'ń': 'n',
    'ó': 'o', 'ś': 's', 'ź': 'z', 'ż': 'z',
    'Ą': 'A', 'Ć': 'C', 'Ę': 'E', 'Ł': 'L', 'Ń': 'N',
    'Ó': 'O', 'Ś': 'S', 'Ź': 'Z', 'Ż': 'Z',
    'á': 'a', 'à': 'a', 'â': 'a', 'ä': 'a', 'ã': 'a', 'å': 'a',
    'æ': 'ae', 'ç': 'c',
    'é': 'e', 'è': 'e', 'ê': 'e', 'ë': 'e',
    'í': 'i', 'ì': 'i', 'î': 'i', 'ï': 'i',
    'ñ': 'n',
    'ô': 'o', 'ö': 'o', 'õ': 'o', 'ø': 'o',
    'ú': 'u', 'ù': 'u', 'û': 'u', 'ü': 'u',
    'ý': 'y', 'ÿ': 'y',
    'Á': 'A', 'À': 'A', 'Â': 'A', 'Ä': 'A', 'Ã': 'A', 'Å': 'A',
    'Æ': 'AE', 'Ç': 'C',
    'É': 'E', 'È': 'E', 'Ê': 'E', 'Ë': 'E',
    'Í': 'I', 'Ì': 'I', 'Î': 'I', 'Ï': 'I',
    'Ñ': 'N',
    'Ô': 'O', 'Ö': 'O', 'Õ': 'O', 'Ø': 'O',
    'Ú': 'U', 'Ù': 'U', 'Û': 'U', 'Ü': 'U',
    'Ý': 'Y',
}

def transliterate(text: str) -> str:
    result = []
    for ch in text:
        result.append(_TRANSLIT_MAP.get(ch, ch))
    return ''.join(result)

def build_packet(msg_type: int, username: str, message: str) -> bytes:
    username = transliterate(username)
    message  = transliterate(message)
    user_b = username.encode("ascii", errors="replace")[:USERNAME_LEN - 1].ljust(USERNAME_LEN, b"\x00")
    msg_b  = message.encode("ascii",  errors="replace")[:MESSAGE_LEN - 1].ljust(MESSAGE_LEN,  b"\x00")
    return struct.pack(PACKET_FORMAT, msg_type, user_b, msg_b)

class TikTokServerGUI:
    def __init__(self, root: Tk):
        self.root = root
        self.root.title("FlipTok Live → Flipper Zero")
        self.root.geometry("520x800")
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)

        self.running          = False
        self.flipper_address  = None
        self.ble_client       = None
        self._ble_loop        = None
        self._pending_packets = None
        self._dedup_cache: list = []
        self._tiktok_connected_at: float = 0.0
        self._lock            = threading.Lock()

        self._tts_queue: list = []
        self._tts_lock  = threading.Lock()
        self._tts_thread = threading.Thread(target=self._tts_worker, daemon=True)
        self._tts_thread.start()

        self._build_ui()
        self.log("Ready. Enter TikTok username and click START.")

    def _build_ui(self):
        header = Frame(self.root, bg="#010101", height=110)
        header.pack(fill=X, padx=8, pady=6)
        header.pack_propagate(False)

        Label(header, text="♪", font=("Arial", 36), bg="#010101", fg="#fe2c55").pack(side=LEFT, padx=16)

        info = Frame(header, bg="#010101")
        info.pack(side=LEFT, fill=Y, pady=10)
        Label(info, text="FlipTok Live", font=("Arial", 18, "bold"), bg="#010101", fg="white").pack(anchor=W)
        Label(info, text="Chat → Flipper Zero", font=("Arial", 11), bg="#010101", fg="#69c9d0").pack(anchor=W)
        Label(info, text="by Dr.Mosfet", font=("Arial", 9), bg="#010101", fg="#888").pack(anchor=W)

        cfg = LabelFrame(self.root, text="Configuration", font=("Arial", 10, "bold"))
        cfg.pack(fill=X, padx=8, pady=4)

        Label(cfg, text="TikTok username (without @):").grid(row=0, column=0, sticky=W, padx=10, pady=5)
        self.username_entry = Entry(cfg, width=32)
        self.username_entry.grid(row=0, column=1, padx=10, pady=5)
        self.username_entry.insert(0, self._load_username())

        Button(cfg, text="Save", command=self._save_username,
               bg="#4CAF50", fg="white", width=8).grid(row=0, column=2, padx=6)

        Label(cfg, text="EulerStream API key:").grid(row=1, column=0, sticky=W, padx=10, pady=5)
        self.apikey_entry = Entry(cfg, width=32, show="*")
        self.apikey_entry.grid(row=1, column=1, padx=10, pady=5)
        self.apikey_entry.insert(0, self._load_apikey())
        Label(cfg, text="(optional, free at eulerstream.com)", fg="gray", font=("Arial", 8)).grid(
            row=2, column=0, columnspan=3, sticky=W, padx=10, pady=(0, 4))

        self._flipper_var = BooleanVar(value=self._load_flipper())
        Checkbutton(cfg, text="\U0001f3ae Send to Flipper Zero (via BLE)",
                    variable=self._flipper_var, font=("Arial", 10)).grid(
            row=3, column=0, columnspan=3, sticky=W, padx=10, pady=(2, 6))

        status_frame = LabelFrame(self.root, text="Status", font=("Arial", 10, "bold"))
        status_frame.pack(fill=X, padx=8, pady=4)

        self.lbl_flipper = Label(status_frame, text="Flipper:  Not connected", fg="red", font=("Arial", 10))
        self.lbl_flipper.pack(anchor=W, padx=10, pady=2)

        self.lbl_tiktok = Label(status_frame, text="TikTok:   Not connected", fg="red", font=("Arial", 10))
        self.lbl_tiktok.pack(anchor=W, padx=10, pady=2)

        self.lbl_msgs = Label(status_frame, text="Messages: 0 sent", font=("Arial", 10))
        self.lbl_msgs.pack(anchor=W, padx=10, pady=2)

        log_frame = LabelFrame(self.root, text="Log", font=("Arial", 10, "bold"))
        log_frame.pack(fill=BOTH, expand=True, padx=8, pady=4)

        sb = Scrollbar(log_frame)
        sb.pack(side=RIGHT, fill=Y)
        self.log_text = Text(log_frame, height=16, yscrollcommand=sb.set,
                             bg="#111", fg="#eee", font=("Courier", 9), insertbackground="white")
        self.log_text.pack(fill=BOTH, expand=True)
        sb.config(command=self.log_text.yview)

        btn_frame = Frame(self.root)
        btn_frame.pack(fill=X, padx=8, pady=8)

        self.btn_start = Button(btn_frame, text="START", command=self.start,
                                bg="#fe2c55", fg="white", font=("Arial", 13, "bold"), height=2)
        self.btn_start.pack(side=LEFT, expand=True, fill=X, padx=4)

        self.btn_stop = Button(btn_frame, text="STOP", command=self.stop,
                               bg="#333", fg="white", font=("Arial", 13, "bold"), height=2, state=DISABLED)
        self.btn_stop.pack(side=RIGHT, expand=True, fill=X, padx=4)

        tts_frame = LabelFrame(self.root, text="Text-to-Speech", font=("Arial", 10, "bold"))
        tts_frame.pack(fill=X, padx=8, pady=(0, 6))

        row0 = Frame(tts_frame)
        row0.pack(fill=X, padx=6, pady=(4, 2))
        self._tts_var = BooleanVar(value=False)
        Checkbutton(row0, text="\U0001f50a Read chat aloud (TTS)",
                    variable=self._tts_var, font=("Arial", 10)).pack(side=LEFT)

        row1 = Frame(tts_frame)
        row1.pack(fill=X, padx=6, pady=2)
        Label(row1, text="Voice:", font=("Arial", 9)).pack(side=LEFT)
        self._tts_voice_var = StringVar(value="auto")
        self._tts_voice_cb = ttk.Combobox(row1, textvariable=self._tts_voice_var,
                                          state="readonly", width=36)
        self._tts_voice_cb["values"] = ["auto"]
        self._tts_voice_cb.pack(side=LEFT, padx=6)
        Button(row1, text="Refresh", font=("Arial", 8),
               command=self._refresh_voices).pack(side=LEFT)

        row2 = Frame(tts_frame)
        row2.pack(fill=X, padx=6, pady=(2, 6))
        Label(row2, text="Speed:", font=("Arial", 9)).pack(side=LEFT)
        self._tts_rate_var = IntVar(value=160)
        Scale(row2, from_=80, to=300, orient=HORIZONTAL,
              variable=self._tts_rate_var, length=180,
              tickinterval=0, showvalue=True).pack(side=LEFT, padx=6)
        Label(row2, text="wpm", font=("Arial", 9)).pack(side=LEFT)

        row3 = Frame(tts_frame)
        row3.pack(fill=X, padx=6, pady=(0, 6))
        Button(row3, text="▶ Test TTS", font=("Arial", 9),
               bg="#555", fg="white",
               command=lambda: self._speak("Hello! FlipTok Live is working. Comments will be read aloud.")).pack(side=LEFT, padx=2)

        self.root.after(500, self._refresh_voices)

        self._msg_count = 0

    def _load_username(self) -> str:
        import os, json
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE) as f:
                    return json.load(f).get("username", "")
            except Exception:
                pass
        return ""

    def _load_apikey(self) -> str:
        import os, json
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE) as f:
                    return json.load(f).get("api_key", "")
            except Exception:
                pass
        return ""

    def _load_flipper(self) -> bool:
        import os, json
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE) as f:
                    return json.load(f).get("use_flipper", True)
            except Exception:
                pass
        return True

    def _save_username(self):
        import json
        username = self.username_entry.get().strip().lstrip("@")
        api_key  = self.apikey_entry.get().strip()
        use_flipper = self._flipper_var.get()
        with open(CONFIG_FILE, "w") as f:
            json.dump({"username": username, "api_key": api_key, "use_flipper": use_flipper}, f)
        self.log("Configuration saved.")

    def _refresh_voices(self):
        try:
            import subprocess
            ps = (
                "Add-Type -AssemblyName System.Speech; "
                "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
                "$s.GetInstalledVoices() | ForEach-Object { $_.VoiceInfo.Name }"
            )
            result = subprocess.run(
                ["powershell", "-NoProfile", "-Command", ps],
                capture_output=True, text=True, timeout=10
            )
            voices = [v.strip() for v in result.stdout.strip().splitlines() if v.strip()]
            self._tts_voices_list = [None] + voices
            self._tts_voice_cb["values"] = ["auto"] + voices
            self._tts_voice_cb.set("auto")
        except Exception as e:
            self.log(f"TTS voice list error: {e}")

    def _tts_worker(self):
        while True:
            text = None
            with self._tts_lock:
                if self._tts_queue:
                    text = self._tts_queue.pop(0)
            if text:
                try:
                    import subprocess
                    rate = self._tts_rate_var.get()
                    voice_idx = self._tts_voice_cb.current()
                    voices_list = getattr(self, "_tts_voices_list", [None])
                    voice_name = voices_list[voice_idx] if voice_idx > 0 and voice_idx < len(voices_list) else None

                    safe_text = text.replace("'", " ")

                    voice_cmd = f"$s.SelectVoice('{voice_name}'); " if voice_name else ""
                    ps = (
                        "Add-Type -AssemblyName System.Speech; "
                        "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
                        f"$s.Rate = {self._rate_to_sapi(rate)}; "
                        f"{voice_cmd}"
                        f"$s.Speak('{safe_text}');"
                    )
                    subprocess.run(
                        ["powershell", "-NoProfile", "-Command", ps],
                        timeout=30
                    )
                except Exception:
                    pass
            else:
                time.sleep(0.1)

    @staticmethod
    def _rate_to_sapi(wpm: int) -> int:
        rate = int((wpm - 150) / 15)
        return max(-10, min(10, rate))

    def _speak(self, text: str):
        if not self._tts_var.get():
            return
        with self._tts_lock:
            if len(self._tts_queue) < 5:
                self._tts_queue.append(text)

    def log(self, msg: str):
        ts = time.strftime("%H:%M:%S")
        self.log_text.insert(END, f"[{ts}] {msg}\n")
        self.log_text.see(END)
        self.root.update_idletasks()

    def _safe_log(self, msg: str):
        self.root.after(0, lambda: self.log(msg))

    def _set_label(self, widget: Label, text: str, color: str):
        self.root.after(0, lambda: widget.config(text=text, fg=color))

    def start(self):
        username = self.username_entry.get().strip().lstrip("@")
        if not username:
            messagebox.showerror("Error", "Enter a TikTok username first.")
            return

        self._save_username()
        self.running = True
        self.btn_start.config(state=DISABLED)
        self.btn_stop.config(state=NORMAL)
        self.username_entry.config(state=DISABLED)
        self._msg_count = 0

        self.log(f"Starting… username: @{username}")

        t = threading.Thread(target=self._run_event_loop, args=(username,), daemon=True)
        t.start()

    def stop(self):
        self.running = False
        self.btn_start.config(state=NORMAL)
        self.btn_stop.config(state=DISABLED)
        self.username_entry.config(state=NORMAL)
        self._set_label(self.lbl_flipper, "Flipper:  Not connected", "red")
        self._set_label(self.lbl_tiktok,  "TikTok:   Not connected", "red")
        self.log("Stopped.")

    def _run_event_loop(self, username: str):
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        self._ble_loop        = loop
        self._pending_packets = asyncio.Queue(maxsize=20) if self._flipper_var.get() else None
        try:
            loop.run_until_complete(self._main(username))
        except Exception as e:
            self._safe_log(f"Fatal error: {e}")
        finally:
            loop.close()

    async def _main(self, username: str):
        if not self._flipper_var.get():
            self._set_label(self.lbl_flipper, "Flipper:  Disabled", "gray")
            await self._run_tiktok(username)
            return

        while self.running:
            try:
                self._safe_log("Scanning for Flipper Zero…")
                address = await self._find_flipper()
                if not address:
                    self._safe_log("Flipper not found. Retrying in 5s…")
                    await asyncio.sleep(5)
                    continue

                self.flipper_address = address
                self._set_label(self.lbl_flipper, f"Flipper:  {address}", "green")
                self._safe_log(f"Flipper found: {address}")

                async with BleakClient(address) as client:
                    if not client.is_connected:
                        self._safe_log("BLE connection failed, retrying…")
                        await asyncio.sleep(3)
                        continue

                    self._safe_log("BLE connected! Starting TikTok listener…")

                    tiktok_task    = asyncio.create_task(self._run_tiktok(username))
                    send_task      = asyncio.create_task(self._send_loop(client))
                    keepalive_task = asyncio.create_task(self._keepalive_loop())

                    done, pending = await asyncio.wait(
                        [tiktok_task, send_task, keepalive_task],
                        return_when=asyncio.FIRST_COMPLETED
                    )
                    for t in pending:
                        t.cancel()

            except Exception as e:
                self._safe_log(f"Error: {e}")
                self._set_label(self.lbl_flipper, "Flipper:  Disconnected", "red")
                self._set_label(self.lbl_tiktok,  "TikTok:   Disconnected", "red")
                await asyncio.sleep(5)

    async def _find_flipper(self):
        devices = await BleakScanner.discover(timeout=8.0)
        for d in devices:
            if d.name and DEVICE_NAME_PREFIX in d.name:
                return d.address
        return None

    async def _keepalive_loop(self):
        while self.running:
            await asyncio.sleep(15)
            if self._pending_packets is not None:
                try:
                    self._pending_packets.put_nowait(build_packet(MSG_TYPE_GIFT, "keepalive", "ping"))
                except asyncio.QueueFull:
                    pass

    async def _send_loop(self, client: BleakClient):
        while self.running and client.is_connected:
            try:
                packet = await asyncio.wait_for(self._pending_packets.get(), timeout=1.0)
                await client.write_gatt_char(SERIAL_RX_CHAR_UUID, packet, response=False)
                await asyncio.sleep(0.08)
                self._msg_count += 1
                self.root.after(0, lambda c=self._msg_count: self.lbl_msgs.config(
                    text=f"Messages: {c} sent"))
            except asyncio.TimeoutError:
                pass
            except Exception as e:
                self._safe_log(f"BLE send error: {e}")
                break

    def _is_duplicate(self, msg_type: int, username: str, message: str, window: float = 3.0) -> bool:
        key = (msg_type, username, message)
        now = time.monotonic()
        self._dedup_cache = [(k, t) for k, t in self._dedup_cache if now - t < window]
        for k, _ in self._dedup_cache:
            if k == key:
                return True
        self._dedup_cache.append((key, now))
        return False

    async def _run_tiktok(self, username: str):
        self._safe_log(f"Connecting to @{username}'s live stream…")

        api_key = self.apikey_entry.get().strip()
        if api_key:
            from TikTokLive.client.web.web_settings import WebDefaults
            WebDefaults.tiktok_sign_api_key = api_key
        client = TikTokLiveClient(unique_id=username)

        @client.on(ConnectEvent)
        async def on_connect(event: ConnectEvent):
            self._safe_log(f"Connected to @{username} live!")
            self._set_label(self.lbl_tiktok, f"TikTok:   @{username} LIVE", "green")
            self._tiktok_connected_at = time.monotonic()
            self._safe_log("Discarding backlog for 3s…")

        @client.on(DisconnectEvent)
        async def on_disconnect(event: DisconnectEvent):
            self._safe_log("TikTok stream disconnected.")
            self._set_label(self.lbl_tiktok, "TikTok:   Disconnected", "orange")

        @client.on(CommentEvent)
        async def on_comment(event: CommentEvent):
            if time.monotonic() - self._tiktok_connected_at < 3.0:
                return
            user = (event.user.nickname or event.user.unique_id or "?")[:16]
            msg  = (event.comment or "")[:64]
            if self._is_duplicate(MSG_TYPE_CHAT, user, msg):
                return
            self._safe_log(f"[CHAT] {user}: {msg}")
            self._speak(f"{user} says: {msg}")
            if self._pending_packets is not None:
                try:
                    self._pending_packets.put_nowait(build_packet(MSG_TYPE_CHAT, user, msg))
                except asyncio.QueueFull:
                    self._safe_log("[SKIP] Queue full, dropping message")

        @client.on(LikeEvent)
        async def on_like(event: LikeEvent):
            pass

        @client.on(GiftEvent)
        async def on_gift(event: GiftEvent):
            user = (event.user.nickname or event.user.unique_id or "?")[:16]
            gift = getattr(event, "gift", None)
            name = getattr(gift, "name", "gift")[:32] if gift else "gift"
            count = getattr(gift, "repeat_count", 1) if gift else 1
            msg = f"{name} x{count}"
            if self._is_duplicate(MSG_TYPE_GIFT, user, msg):
                return
            self._safe_log(f"[GIFT] {user}: {msg}")
            if self._pending_packets is not None:
                try:
                    self._pending_packets.put_nowait(build_packet(MSG_TYPE_GIFT, user, msg))
                except asyncio.QueueFull:
                    pass

        @client.on(FollowEvent)
        async def on_follow(event: FollowEvent):
            user = (event.user.nickname or event.user.unique_id or "?")[:16]
            msg  = "followed"
            if self._is_duplicate(MSG_TYPE_FOLLOW, user, msg, window=10.0):
                return
            self._safe_log(f"[FOLLOW] {user}")
            if self._pending_packets is not None:
                try:
                    self._pending_packets.put_nowait(build_packet(MSG_TYPE_FOLLOW, user, msg))
                except asyncio.QueueFull:
                    pass

        try:
            await client.start()
        except Exception as e:
            self._safe_log(f"TikTok error: {e}")
            self._set_label(self.lbl_tiktok, "TikTok:   Error", "red")

        while self.running:
            await asyncio.sleep(1)

    def on_closing(self):
        if self.running:
            self.stop()
            time.sleep(0.5)
        self.root.destroy()

if __name__ == "__main__":
    root = Tk()
    app = TikTokServerGUI(root)
    root.mainloop()
