/*
 * GhostTag - ESP32 companion firmware
 * ------------------------------------
 * Scans BLE advertisements, classifies known trackers (Apple Find My / AirTag,
 * Tile, Samsung SmartTag) and streams them to the Flipper Zero over UART.
 *
 * Hardware : Flipper Zero WiFi/BLE devboard (ESP32-S2) or any ESP32.
 * Library  : NimBLE-Arduino 1.4.x  (Library Manager: "NimBLE-Arduino")
 * Serial   : 115200 baud, wired to Flipper USART (pins 13 TX / 14 RX).
 *
 * Wire protocol (see helpers/uart_link.h on the Flipper side):
 *   ESP32 -> Flipper : GT1,<mac12hex>,<rssi>,<typecode>,<name>\n
 *                      GTHELLO,<version>\n        (on boot)
 *   Flipper -> ESP32 : START\n  STOP\n
 *
 * TrackerType codes: 1 Apple Find My, 2 Apple nearby, 3 Tile, 4 Samsung SmartTag.
 */

#include <NimBLEDevice.h>

#define GHOSTTAG_FW_VERSION "1.0"

// ---- BLE advertisement signatures ----
static const uint16_t APPLE_COMPANY_ID = 0x004C;
static const uint8_t APPLE_TYPE_FINDMY = 0x12;
static const uint8_t APPLE_TYPE_NEARBY = 0x07;
static const uint16_t TILE_UUID = 0xFEED;
static const uint16_t SAMSUNG_UUID = 0xFD5A;
static const uint16_t SAMSUNG_COMPANY_ID = 0x0075;

// TrackerType wire codes (must match helpers/ble_signatures.h)
enum {
  TYPE_NONE = 0,
  TYPE_APPLE_FINDMY = 1,
  TYPE_APPLE_NEARBY = 2,
  TYPE_TILE = 3,
  TYPE_SAMSUNG = 4,
};

static NimBLEScan* pScan = nullptr;
static bool scanning = false;

// Returns a TrackerType code, or TYPE_NONE if the advert is not a tracker.
static int classifyDevice(NimBLEAdvertisedDevice* dev) {
  // Manufacturer-specific data (Apple / Samsung)
  if (dev->haveManufacturerData()) {
    std::string md = dev->getManufacturerData();
    if (md.size() >= 2) {
      uint16_t company = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
      if (company == APPLE_COMPANY_ID && md.size() >= 3) {
        uint8_t apple_type = (uint8_t)md[2];
        if (apple_type == APPLE_TYPE_FINDMY) return TYPE_APPLE_FINDMY;
        if (apple_type == APPLE_TYPE_NEARBY) return TYPE_APPLE_NEARBY;
      }
      if (company == SAMSUNG_COMPANY_ID) return TYPE_SAMSUNG;
    }
  }

  // 16-bit service UUIDs (Tile / Samsung)
  for (int i = 0; i < dev->getServiceUUIDCount(); i++) {
    NimBLEUUID u = dev->getServiceUUID(i);
    if (u == NimBLEUUID(TILE_UUID)) return TYPE_TILE;
    if (u == NimBLEUUID(SAMSUNG_UUID)) return TYPE_SAMSUNG;
  }

  // Service data UUIDs (SmartThings Find / Tile)
  for (int i = 0; i < dev->getServiceDataCount(); i++) {
    NimBLEUUID u = dev->getServiceDataUUID(i);
    if (u == NimBLEUUID(SAMSUNG_UUID)) return TYPE_SAMSUNG;
    if (u == NimBLEUUID(TILE_UUID)) return TYPE_TILE;
  }

  return TYPE_NONE;
}

static void sanitize(String& s) {
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == ',' || c == '\n' || c == '\r' || c < 32) s[i] = '_';
  }
  if (s.length() > 18) s = s.substring(0, 18);
}

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    int type = classifyDevice(dev);
    if (type == TYPE_NONE) return;

    // 12-char uppercase hex MAC (no separators)
    String mac = dev->getAddress().toString().c_str();
    mac.replace(":", "");
    mac.toUpperCase();

    String name = "";
    if (dev->haveName()) {
      name = dev->getName().c_str();
      sanitize(name);
    }

    Serial.printf("GT1,%s,%d,%d,%s\n", mac.c_str(), dev->getRSSI(), type, name.c_str());
  }
};

static void startScan() {
  if (scanning) return;
  pScan->start(0, false);  // duration 0 = scan continuously
  scanning = true;
}

static void stopScan() {
  if (!scanning) return;
  pScan->stop();
  scanning = false;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  NimBLEDevice::init("");
  NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);
  pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), /*wantDuplicates=*/true);
  pScan->setActiveScan(true);  // request names too
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->setMaxResults(0);  // use the callback, don't buffer

  Serial.printf("GTHELLO,%s\n", GHOSTTAG_FW_VERSION);
  startScan();  // auto-start; Flipper can STOP/START on demand
}

void loop() {
  // Handle commands from the Flipper
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "START") {
      startScan();
    } else if (cmd == "STOP") {
      stopScan();
    } else if (cmd == "PING") {
      Serial.printf("GTHELLO,%s\n", GHOSTTAG_FW_VERSION);
    }
  }

  // NimBLE may finish a scan cycle; keep it alive while we want to scan.
  if (scanning && !pScan->isScanning()) {
    pScan->start(0, false);
  }
  delay(20);
}
