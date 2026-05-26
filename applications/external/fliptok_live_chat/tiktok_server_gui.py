import sys
import subprocess
import struct
import time
import asyncio
import threading
import json
import os
from tkinter import *
from tkinter import ttk, filedialog, messagebox

# --- Automatyczna instalacja zależności ---
def install_package(package: str):
    subprocess.check_call([sys.executable, "-m", "pip", "install", package])

def check_and_install_dependencies() -> bool:
    required = {"TikTokLive": "TikTokLive", "bleak": "bleak", "playsound": "playsound==1.2.2"}
    missing = []
    for import_name, pkg_name in required.items():
        try:
            __import__(import_name)
        except ImportError:
            missing.append(pkg_name)

    if missing:
        print("Brakujące biblioteki – instalowanie...")
        for pkg in missing:
            try:
                install_package(pkg)
                print(f"  ✓ {pkg}")
            except Exception as e:
                print(f"  ✗ {pkg}: {e}")
                return False
    return True

if not check_and_install_dependencies():
    print("\nBłąd instalacji. Uruchom ręcznie: pip install TikTokLive bleak")
    sys.exit(1)

from bleak import BleakClient, BleakScanner
from TikTokLive import TikTokLiveClient
from TikTokLive.events import ConnectEvent, DisconnectEvent, CommentEvent, LikeEvent, GiftEvent, FollowEvent

# --- Konfiguracja Pakietów ---
USERNAME_LEN = 17
MESSAGE_LEN  = 65
PACKET_FORMAT = f"<B{USERNAME_LEN}s{MESSAGE_LEN}s"
MSG_TYPE_CHAT   = 0
MSG_TYPE_LIKE   = 1
MSG_TYPE_GIFT   = 2
MSG_TYPE_FOLLOW = 3

SERIAL_RX_CHAR_UUID = "19ed82ae-ed21-4c9d-4145-228e62fe0000"
# Zmienione na "Flip", aby było bardziej elastyczne
DEVICE_NAME_PREFIX  = "Flip" 
CONFIG_FILE = "tiktok_server_config.json"

_TRANSLIT_MAP = {
    'ą': 'a', 'ć': 'c', 'ę': 'e', 'ł': 'l', 'ń': 'n', 'ó': 'o', 'ś': 's', 'ź': 'z', 'ż': 'z',
    'Ą': 'A', 'Ć': 'C', 'Ę': 'E', 'Ł': 'L', 'Ń': 'N', 'Ó': 'O', 'Ś': 'S', 'Ź': 'Z', 'Ż': 'Z',
}

def transliterate(text: str) -> str:
    return "".join(_TRANSLIT_MAP.get(ch, ch) for ch in text)

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
        self.root.geometry("520x850")
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)

        self.running = False
        self._pending_packets = None
        self._tiktok_connected_at = 0.0
        self._msg_count = 0
        self._gift_sound_str = ""  # thread-safe kopia ścieżki dźwięku
        
        # TTS Worker
        self._tts_queue = []
        self._tts_lock = threading.Lock()
        self._tts_thread = threading.Thread(target=self._tts_worker, daemon=True)
        self._tts_thread.start()

        self._build_ui()
        self.log("Gotowy. Upewnij się, że Flipper ma włączony BT.")

    def _build_ui(self):
        header = Frame(self.root, bg="#010101", height=100)
        header.pack(fill=X, padx=8, pady=6)
        header.pack_propagate(False)
        Label(header, text="♪", font=("Arial", 36), bg="#010101", fg="#fe2c55").pack(side=LEFT, padx=16)
        info = Frame(header, bg="#010101")
        info.pack(side=LEFT, fill=Y, pady=10)
        Label(info, text="FlipTok Live", font=("Arial", 16, "bold"), bg="#010101", fg="white").pack(anchor=W)
        Label(info, text="by Dr.Mosfet", font=("Arial", 8), bg="#010101", fg="#888").pack(anchor=W)

        cfg = LabelFrame(self.root, text="Konfiguracja", font=("Arial", 10, "bold"))
        cfg.pack(fill=X, padx=8, pady=4)

        Label(cfg, text="TikTok Username:").grid(row=0, column=0, sticky=W, padx=10, pady=5)
        self.username_entry = Entry(cfg, width=30)
        self.username_entry.grid(row=0, column=1, padx=10)
        
        Label(cfg, text="Sign API Key:").grid(row=1, column=0, sticky=W, padx=10, pady=5)
        self.apikey_entry = Entry(cfg, width=30, show="*")
        self.apikey_entry.grid(row=1, column=1, padx=10)

        saved_data = self._load_config()
        self.username_entry.insert(0, saved_data.get("username", ""))
        self.apikey_entry.insert(0, saved_data.get("api_key", ""))

        self._gift_sound_path = StringVar(value=saved_data.get("gift_sound_path", ""))
        Label(cfg, text="Dźwięk prezentu:").grid(row=2, column=0, sticky=W, padx=10, pady=5)
        gift_frame = Frame(cfg)
        gift_frame.grid(row=2, column=1, sticky=W, padx=10)
        self.gift_sound_entry = Entry(gift_frame, textvariable=self._gift_sound_path, width=26, state="readonly")
        self.gift_sound_entry.pack(side=LEFT, fill=X, expand=True)
        Button(gift_frame, text="Wybierz", command=self._select_gift_sound).pack(side=LEFT, padx=4)

        self._flipper_var = BooleanVar(value=True)
        Checkbutton(cfg, text="Połącz z Flipperem przez BLE", variable=self._flipper_var).grid(row=3, columnspan=2, sticky=W, padx=10)

        status_frame = LabelFrame(self.root, text="Status", font=("Arial", 10, "bold"))
        status_frame.pack(fill=X, padx=8, pady=4)
        self.lbl_flipper = Label(status_frame, text="Flipper: Rozłączony", fg="red")
        self.lbl_flipper.pack(anchor=W, padx=10)
        self.lbl_tiktok = Label(status_frame, text="TikTok: Rozłączony", fg="red")
        self.lbl_tiktok.pack(anchor=W, padx=10)
        self.lbl_msgs = Label(status_frame, text="Wysłano pakietów: 0")
        self.lbl_msgs.pack(anchor=W, padx=10)

        log_frame = LabelFrame(self.root, text="Logi")
        log_frame.pack(fill=BOTH, expand=True, padx=8, pady=4)
        self.log_text = Text(log_frame, height=12, bg="#111", fg="#eee", font=("Courier", 9))
        self.log_text.pack(fill=BOTH, expand=True)

        tts_frame = LabelFrame(self.root, text="Opcje")
        tts_frame.pack(fill=X, padx=8, pady=4)
        self._tts_var = BooleanVar(value=False)
        Checkbutton(tts_frame, text="Czytaj czat (TTS)", variable=self._tts_var).pack(side=LEFT, padx=10)

        btn_frame = Frame(self.root)
        btn_frame.pack(fill=X, padx=8, pady=10)
        self.btn_start = Button(btn_frame, text="START", command=self.start, bg="#fe2c55", fg="white", font=("Arial", 12, "bold"), height=2)
        self.btn_start.pack(side=LEFT, expand=True, fill=X, padx=4)
        self.btn_stop = Button(btn_frame, text="STOP", command=self.stop, state=DISABLED, font=("Arial", 12, "bold"), height=2)
        self.btn_stop.pack(side=RIGHT, expand=True, fill=X, padx=4)

    def _load_config(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, "r") as f: return json.load(f)
            except: return {}
        return {}

    def _save_config(self):
        data = {
            "username": self.username_entry.get().strip(),
            "api_key": self.apikey_entry.get().strip(),
            "gift_sound_path": self._gift_sound_path.get().strip(),
        }
        with open(CONFIG_FILE, "w") as f: json.dump(data, f)

    def _select_gift_sound(self):
        path = filedialog.askopenfilename(
            title="Wybierz plik dźwiękowy prezentu",
            filetypes=[("Audio files", "*.wav *.mp3 *.ogg *.flac"), ("WAV files", "*.wav"), ("All files", "*.*")],
        )
        if path:
            self._gift_sound_path.set(path)
            self._gift_sound_str = path  # thread-safe kopia
            self._save_config()
            self._safe_log(f"Wybrano dźwięk prezentu: {os.path.basename(path)}")

    def _play_gift_sound(self, path: str):
        if not path or not os.path.exists(path):
            return
        threading.Thread(target=self._play_sound_thread, args=(path,), daemon=True).start()

    def _play_sound_thread(self, path: str):
        try:
            from playsound import playsound
            playsound(path, block=True)
        except Exception as e:
            self._safe_log(f"Błąd odtwarzania dźwięku: {e}")
            self._safe_log(f"Błąd odtwarzania dźwięku: {e}")

    def log(self, msg: str):
        self.log_text.insert(END, f"[{time.strftime('%H:%M:%S')}] {msg}\n")
        self.log_text.see(END)

    def _safe_log(self, msg: str):
        self.root.after(0, lambda: self.log(msg))

    def _set_label(self, widget, text, color):
        self.root.after(0, lambda: widget.config(text=text, fg=color))

    def start(self):
        user = self.username_entry.get().strip().lstrip("@")
        if not user:
            messagebox.showerror("Błąd", "Wpisz nazwę użytkownika TikTok!")
            return
        if not self._gift_sound_path.get():
            if messagebox.askyesno("Dźwięk prezentu", "Nie ustawiono pliku dźwiękowego prezentu. Wybrać teraz?"):
                self._select_gift_sound()
        self._save_config()
        self.running = True
        self._gift_sound_str = self._gift_sound_path.get()  # snapshot przed wątkiem
        self.btn_start.config(state=DISABLED)
        self.btn_stop.config(state=NORMAL)
        threading.Thread(target=self._run_event_loop, args=(user,), daemon=True).start()

    def stop(self):
        self.running = False
        self.btn_start.config(state=NORMAL)
        self.btn_stop.config(state=DISABLED)

    def _run_event_loop(self, username):
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        self._pending_packets = asyncio.Queue()
        try:
            loop.run_until_complete(self._main_logic(username))
        except Exception as e:
            self._safe_log(f"Błąd pętli: {e}")
        finally:
            loop.close()

    async def _main_logic(self, username):
        if self._flipper_var.get():
            self._safe_log("Skanowanie Bluetooth (5s)...")
            devices = await BleakScanner.discover(timeout=5.0)
            
            # Logowanie wszystkich urządzeń dla ułatwienia diagnozy
            for d in devices:
                self._safe_log(f" Widzę: {d.name or 'Nieznany'} [{d.address}]")

            target = next((d for d in devices if d.name and DEVICE_NAME_PREFIX in d.name), None)
            
            if target:
                self._safe_log(f"Łączenie z: {target.name}...")
                try:
                    async with BleakClient(target.address) as client:
                        self._set_label(self.lbl_flipper, f"Flipper: {target.name}", "green")
                        send_task = asyncio.create_task(self._send_to_ble(client))
                        await self._run_tiktok(username)
                        await send_task
                except Exception as e:
                    self._safe_log(f"Błąd połączenia BLE: {e}")
            else:
                self._safe_log("Nie znaleziono Flippera w pobliżu.")
                self._set_label(self.lbl_flipper, "Flipper: Nie znaleziono", "orange")
                await self._run_tiktok(username)
        else:
            self._safe_log("Flipper wyłączony. Uruchamiam tylko TikTok.")
            self._set_label(self.lbl_flipper, "Flipper: Wyłączony", "orange")
            await self._run_tiktok(username)
            while self.running:
                await asyncio.sleep(1.0)

    async def _run_tiktok(self, username: str):
        api_key = self.apikey_entry.get().strip()
        if api_key:
            from TikTokLive.client.web.web_settings import WebDefaults
            WebDefaults.tiktok_sign_api_key = api_key
        else:
            self._safe_log("Uwaga: nie ustawiono Sign API Key. Korzystam z domyślnego serwera sign.")

        client = TikTokLiveClient(unique_id=username)

        @client.on(ConnectEvent)
        async def on_connect(_):
            self._safe_log(f"TikTok Połączony: @{username}")
            self._set_label(self.lbl_tiktok, f"TikTok: @{username} LIVE", "green")
            self._tiktok_connected_at = time.monotonic()

        @client.on(CommentEvent)
        async def on_comment(event: CommentEvent):
            if time.monotonic() - self._tiktok_connected_at < 2.0:
                return
            # NAPRAWA BŁĘDU nickName
            u = (event.user_info.nickname or event.user_info.unique_id or "User")[:16]
            m = (event.comment or "")[:64]
            self._safe_log(f"[CHAT] {u}: {m}")
            if self._tts_var.get():
                self._speak(m)
            await self._queue_packet(MSG_TYPE_CHAT, u, m)

        @client.on(GiftEvent)
        async def on_gift(event: GiftEvent):
            try:
                u = (getattr(event.user, 'nickname', None) or getattr(event.user, 'unique_id', None) or "User")[:16]
                g = getattr(event.gift, 'name', None) or 'Gift'
                # Odtwarzaj dźwięk przy każdym gifcie (również podczas streaka)
                self._safe_log(f"[GIFT] {u} wysłał {g}")
                if self._gift_sound_str:
                    self._play_gift_sound(self._gift_sound_str)
                # Pakiet BLE tylko na końcu streaka lub gdy nie-streakable
                is_streakable = getattr(event.gift, 'streakable', False)
                is_streaking = getattr(event, 'streaking', False)
                if not is_streakable or not is_streaking:
                    await self._queue_packet(MSG_TYPE_GIFT, u, f"Prezent: {g}")
            except Exception as e:
                self._safe_log(f"[GIFT] błąd obsługi: {e}")

        retry = 0
        while self.running and retry < 3:
            try:
                await client.start()
                return
            except Exception as e:
                self._safe_log(f"Błąd TikTok: {e}")
                if retry < 2 and ("SIGN_NOT_200" in str(e) or "sign" in str(e).lower() or "503" in str(e)):
                    retry += 1
                    self._safe_log("Błąd Sign API. Ponawiam próbę za 5 sekund...")
                    await asyncio.sleep(5)
                    continue
                break

        self._safe_log("Nie udało się połączyć z TikTok. Sprawdź klucz Sign API lub spróbuj później.")

    async def _queue_packet(self, p_type, user, msg):
        if self._pending_packets and self.running:
            packet = build_packet(p_type, user, msg)
            await self._pending_packets.put(packet)

    async def _send_to_ble(self, client: BleakClient):
        while self.running and client.is_connected:
            try:
                packet = await asyncio.wait_for(self._pending_packets.get(), timeout=1.0)
                await client.write_gatt_char(SERIAL_RX_CHAR_UUID, packet, response=False)
                self._msg_count += 1
                self.root.after(0, lambda: self.lbl_msgs.config(text=f"Wysłano pakietów: {self._msg_count}"))
                await asyncio.sleep(0.1)
            except asyncio.TimeoutError: continue
            except: break

    def _speak(self, text):
        with self._tts_lock: self._tts_queue.append(text)

    def _tts_worker(self):
        while True:
            msg = None
            with self._tts_lock:
                if self._tts_queue: msg = self._tts_queue.pop(0)
            if msg:
                clean = msg.replace("'", "").replace('"', "")
                cmd = f"Add-Type -AssemblyName System.Speech; $s = New-Object System.Speech.Synthesis.SpeechSynthesizer; $s.Speak('{clean}')"
                subprocess.run(["powershell", "-Command", cmd], capture_output=True)
            else: time.sleep(0.2)

    def on_closing(self):
        self.running = False
        self.root.destroy()

if __name__ == "__main__":
    root = Tk()
    app = TikTokServerGUI(root)
    root.mainloop()

