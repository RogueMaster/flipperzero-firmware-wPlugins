"""
Flight Monitor Server - War Thunder Edition (GUI Version)
Sends data from War Thunder to Flipper Zero via Bluetooth

Requirements:
    pip install requests bleak

Usage:
    python flight_server_gui.py
"""

import sys
import subprocess
import json
import os


# Auto-install dependencies
def install_package(package):
    """Install a package using pip"""
    print(f"Installing {package}...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", package])


def check_and_install_dependencies():
    """Check and install required packages"""
    required_packages = {"requests": "requests", "bleak": "bleak"}

    missing_packages = []

    for import_name, package_name in required_packages.items():
        try:
            __import__(import_name)
        except ImportError:
            missing_packages.append(package_name)

    if missing_packages:
        print("Missing dependencies detected. Installing...")
        for package in missing_packages:
            try:
                install_package(package)
                print(f"✓ {package} installed successfully")
            except Exception as e:
                print(f"✗ Failed to install {package}: {e}")
                return False
        print("All dependencies installed successfully!")

    return True


# Install dependencies before importing them
if not check_and_install_dependencies():
    print("\nError: Could not install required dependencies.")
    print("Please install manually: pip install requests bleak")
    sys.exit(1)

import requests
import struct
import time
import asyncio
from tkinter import *
from tkinter import ttk, messagebox
from bleak import BleakClient, BleakScanner
import threading

CONFIG_FILE = "flight_server_config.json"
DEFAULT_API_URL = "http://localhost:8111/state"

SERIAL_SERVICE_UUID = "8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000"
SERIAL_RX_CHAR_UUID = "19ed82ae-ed21-4c9d-4145-228e62fe0000"
DEVICE_NAME_PREFIX = "Flight"


class Config:
    def __init__(self):
        self.api_url = DEFAULT_API_URL
        self.load()

    def load(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, "r") as f:
                    data = json.load(f)
                    self.api_url = data.get("api_url", DEFAULT_API_URL)
            except:
                pass

    def save(self):
        with open(CONFIG_FILE, "w") as f:
            json.dump({"api_url": self.api_url}, f)


class FlightData:
    def __init__(self):
        self.altitude = 0
        self.speed = 0
        self.throttle = 0
        self.vertical_speed = 0
        self.pitch = 0
        self.roll = 0
        self.flaps = 0
        self.gear = 0
        self.engine_power = 0
        self.fuel = 0

    def pack(self):
        return struct.pack(
            "<hHbhbbBBhh",
            self.altitude,
            self.speed,
            self.throttle,
            self.vertical_speed,
            self.pitch,
            self.roll,
            self.flaps,
            self.gear,
            self.engine_power,
            self.fuel,
        )


class FlightServerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Flight Monitor Server - War Thunder")
        self.root.geometry("500x600")
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)

        self.config = Config()
        self.running = False
        self.server_thread = None
        self.flipper_address = None

        # Logo frame
        logo_frame = Frame(root, bg="#1a1a2e", height=120)
        logo_frame.pack(fill=X, pady=10, padx=10)
        logo_frame.pack_propagate(False)

        # Plane ASCII art
        plane_art = """
    ✈️
        """

        Label(
            logo_frame, text=plane_art, font=("Courier", 24), bg="#1a1a2e", fg="#00d4ff"
        ).pack()
        Label(
            logo_frame,
            text="War Thunder Dashboard",
            font=("Arial", 14, "bold"),
            bg="#1a1a2e",
            fg="white",
        ).pack()
        Label(
            logo_frame, text="by Dr.Mosfet", font=("Arial", 9), bg="#1a1a2e", fg="#888"
        ).pack()

        # Configuration frame
        config_frame = LabelFrame(
            root, text="Configuration", font=("Arial", 10, "bold")
        )
        config_frame.pack(fill=X, padx=10, pady=10)

        Label(config_frame, text="War Thunder API URL:").grid(
            row=0, column=0, sticky=W, padx=10, pady=5
        )
        self.url_entry = Entry(config_frame, width=40)
        self.url_entry.grid(row=0, column=1, padx=10, pady=5)
        self.url_entry.insert(0, self.config.api_url)

        Button(
            config_frame,
            text="Save",
            command=self.save_config,
            bg="#4CAF50",
            fg="white",
            width=10,
        ).grid(row=1, column=1, sticky=E, padx=10, pady=5)

        # Status frame
        status_frame = LabelFrame(root, text="Status", font=("Arial", 10, "bold"))
        status_frame.pack(fill=BOTH, expand=True, padx=10, pady=10)

        # Status labels
        self.status_flipper = Label(
            status_frame, text="Flipper: Not connected", font=("Arial", 10), fg="red"
        )
        self.status_flipper.pack(anchor=W, padx=10, pady=5)

        self.status_game = Label(
            status_frame, text="Game: Not connected", font=("Arial", 10), fg="red"
        )
        self.status_game.pack(anchor=W, padx=10, pady=5)

        self.status_data = Label(
            status_frame, text="Data: Waiting...", font=("Arial", 10)
        )
        self.status_data.pack(anchor=W, padx=10, pady=5)

        # Log area
        log_frame = Frame(status_frame)
        log_frame.pack(fill=BOTH, expand=True, padx=10, pady=10)

        scrollbar = Scrollbar(log_frame)
        scrollbar.pack(side=RIGHT, fill=Y)

        self.log_text = Text(
            log_frame,
            height=15,
            width=55,
            yscrollcommand=scrollbar.set,
            bg="#f0f0f0",
            font=("Courier", 9),
        )
        self.log_text.pack(side=LEFT, fill=BOTH, expand=True)
        scrollbar.config(command=self.log_text.yview)

        # Control buttons
        control_frame = Frame(root)
        control_frame.pack(fill=X, padx=10, pady=10)

        self.start_button = Button(
            control_frame,
            text="START SERVER",
            command=self.start_server,
            bg="#2196F3",
            fg="white",
            font=("Arial", 12, "bold"),
            height=2,
        )
        self.start_button.pack(side=LEFT, expand=True, fill=X, padx=5)

        self.stop_button = Button(
            control_frame,
            text="STOP SERVER",
            command=self.stop_server,
            bg="#f44336",
            fg="white",
            font=("Arial", 12, "bold"),
            height=2,
            state=DISABLED,
        )
        self.stop_button.pack(side=RIGHT, expand=True, fill=X, padx=5)

        self.log("Server ready. Starting automatically...")

        # Auto-start serwera po 500ms (dajemy czas na wyrenderowanie GUI)
        self.root.after(500, self.start_server)

    def log(self, message):
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.insert(END, f"[{timestamp}] {message}\n")
        self.log_text.see(END)
        self.root.update()

    def save_config(self):
        self.config.api_url = self.url_entry.get()
        self.config.save()
        self.log("Configuration saved!")
        messagebox.showinfo("Success", "Configuration saved successfully!")

    def start_server(self):
        self.save_config()
        self.running = True
        self.start_button.config(state=DISABLED)
        self.stop_button.config(state=NORMAL)
        self.url_entry.config(state=DISABLED)

        self.log("Starting server...")
        self.server_thread = threading.Thread(target=self.run_server, daemon=True)
        self.server_thread.start()

    def stop_server(self):
        self.running = False
        self.start_button.config(state=NORMAL)
        self.stop_button.config(state=DISABLED)
        self.url_entry.config(state=NORMAL)
        self.status_flipper.config(text="Flipper: Not connected", fg="red")
        self.status_game.config(text="Game: Not connected", fg="red")
        self.status_data.config(text="Data: Waiting...", fg="black")
        self.log("Server stopped.")

    def run_server(self):
        asyncio.run(self.server_main_loop())

    async def server_main_loop(self):
        while self.running:
            try:
                self.log("Searching for Flipper Zero...")
                flipper_address = await self.find_flipper()

                if not flipper_address:
                    self.log("Flipper not found. Retrying in 5 seconds...")
                    await asyncio.sleep(5)
                    continue

                self.flipper_address = flipper_address
                self.status_flipper.config(
                    text=f"Flipper: Connected ({flipper_address})", fg="green"
                )

                await self.data_transmission_loop()

            except Exception as e:
                self.log(f"Error: {e}")
                await asyncio.sleep(5)

    async def find_flipper(self):
        devices = await BleakScanner.discover()
        for device in devices:
            if device.name and DEVICE_NAME_PREFIX in device.name:
                self.log(f"Found: {device.name} ({device.address})")
                return device.address
        return None

    async def data_transmission_loop(self):
        try:
            async with BleakClient(self.flipper_address) as client:
                if not client.is_connected:
                    self.log("Connection failed")
                    return

                self.log("Connected! Starting data transmission...")

                packet_count = 0
                in_menu = False
                last_valid_data = None

                while self.running and client.is_connected:
                    flight_data, is_valid = self.fetch_game_data()

                    if is_valid and flight_data:
                        self.status_game.config(text="Game: Connected", fg="green")

                        data_changed = True
                        if last_valid_data:
                            if (
                                flight_data.altitude == last_valid_data.altitude
                                and flight_data.speed == last_valid_data.speed
                                and flight_data.throttle == last_valid_data.throttle
                                and flight_data.vertical_speed
                                == last_valid_data.vertical_speed
                            ):
                                data_changed = False

                        if data_changed:
                            if in_menu:
                                self.log("Movement detected - resuming")
                                in_menu = False

                            data_packet = flight_data.pack()

                            if packet_count % 10 == 0:
                                data_str = f"ALT:{flight_data.altitude}m SPD:{flight_data.speed}km/h THR:{flight_data.throttle}%"
                                self.status_data.config(text=f"Data: {data_str}")
                                self.log(data_str)

                            await client.write_gatt_char(
                                SERIAL_RX_CHAR_UUID, data_packet, response=False
                            )
                            packet_count += 1
                            last_valid_data = flight_data

                        else:
                            if not in_menu:
                                self.log("No movement - waiting")
                                in_menu = True

                    elif not is_valid:
                        self.status_game.config(text="Game: In menu", fg="orange")
                        if not in_menu:
                            self.log("In menu - waiting for flight")
                            in_menu = True

                    await asyncio.sleep(0.1)

        except Exception as e:
            self.log(f"BLE error: {e}")
            self.status_flipper.config(text="Flipper: Disconnected", fg="red")

    def fetch_game_data(self):
        try:
            response = requests.get(self.config.api_url, timeout=1)
            if response.status_code == 200:
                data = response.json()

                if not data.get("valid", False):
                    return None, False

                flight = FlightData()
                flight.altitude = int(data.get("H, m", 0))
                flight.speed = int(data.get("IAS, km/h", data.get("TAS, km/h", 0)))
                flight.throttle = max(
                    0,
                    min(
                        100, int(data.get("throttle 1, %", data.get("throttle, %", 0)))
                    ),
                )
                flight.vertical_speed = int(data.get("Vy, m/s", 0) * 10)
                flight.pitch = int(data.get("AoA, deg", 0))
                flight.roll = int(data.get("AoS, deg", 0))
                flight.flaps = max(0, min(100, int(data.get("flaps, %", 0))))

                if flight.speed < 200 and flight.altitude < 500:
                    flight.gear = 1
                else:
                    flight.gear = 0

                flight.engine_power = int(
                    data.get("power 1, hp", data.get("power, hp", 0))
                )
                flight.fuel = int(data.get("Mfuel, kg", 0))

                return flight, True

        except requests.exceptions.ConnectionError:
            self.status_game.config(text="Game: Not running", fg="red")
            return None, False
        except Exception as e:
            return None, False

    def on_closing(self):
        if self.running:
            self.stop_server()
            time.sleep(0.5)
        self.root.destroy()


if __name__ == "__main__":
    root = Tk()
    app = FlightServerGUI(root)
    root.mainloop()
