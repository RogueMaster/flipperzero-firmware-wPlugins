/*
 * GhostTag - ESP32 companion firmware
 * ------------------------------------
 * Scans BLE advertisements, classifies known trackers (Apple Find My / AirTag,
 * Tile, Samsung SmartTag, Chipolo) and streams them to a Flipper Zero over UART.
 *
 * ---------------------------------------------------------------------------
 *  HARDWARE: THE OFFICIAL FLIPPER WIFI DEVBOARD DOES NOT WORK.
 *
 *  It is built around an ESP32-S2-WROVER, and the ESP32-S2 has NO Bluetooth
 *  radio of any kind - not BLE, not Classic. It is Wi-Fi only. No firmware can
 *  give it a radio it does not have, which is why the Marauder project also
 *  greys out every Bluetooth menu item on that board.
 *
 *  Use a BLE-capable ESP32 instead:
 *      ESP32 (original)  BT 4.2 BR/EDR + BLE   yes
 *      ESP32-S3          BLE 5.0               yes   <- recommended
 *      ESP32-C3          BLE 5.0               yes   <- recommended, cheap
 *      ESP32-C6          BLE 5.0/5.3           yes   (needs NimBLE 2.x)
 *      ESP32-S2          none                  NO
 *
 *  The #error below refuses to build for a chip that cannot do the job, rather
 *  than producing a binary that flashes fine and then finds nothing forever.
 * ---------------------------------------------------------------------------
 *
 * Library : NimBLE-Arduino. Both the 1.4.x and 2.x APIs are supported below;
 *           they renamed the scan callback class and its setter, so a sketch
 *           written against one fails to compile against the other.
 * Serial  : 115200 baud, wired to the Flipper USART.
 *
 *   Flipper pin 13 (TX)  ->  ESP RX
 *   Flipper pin 14 (RX)  ->  ESP TX      (note the cross-over)
 *   Flipper pin 11 (GND) ->  ESP GND     (pins 8 and 18 are also GND)
 *
 *   Power the board over its own USB while you are getting started. The
 *   Flipper's 3V3 on pin 9 can run a small module, but the documented current
 *   budget for that rail is not something to guess at with a radio attached.
 *
 * Wire protocol (see helpers/uart_link.h on the Flipper side):
 *   ESP32 -> Flipper : GT1,<mac12hex>,<rssi>,<typecode>,<name>\n
 *                      GTHELLO,<version>\n   on boot and in reply to PING
 *                      GTALIVE,<count>\n     heartbeat, every 2 s
 *   Flipper -> ESP32 : START\n  STOP\n  PING\n
 *
 * TrackerType codes: 1 Apple Find My (separated), 2 Apple nearby/owner,
 *                    3 Tile, 4 Samsung SmartTag, 5 Chipolo.
 */

#include <NimBLEDevice.h>

/* ---- refuse to build for a chip with no Bluetooth radio ---- */
#if defined(CONFIG_IDF_TARGET_ESP32S2)
#error "The ESP32-S2 has no Bluetooth radio, so it cannot scan for trackers. The OFFICIAL Flipper WiFi Devboard is an ESP32-S2 and will NOT work with GhostTag. Use an ESP32-S3, ESP32-C3, ESP32-C6 or a classic ESP32."
#endif
#if !defined(CONFIG_BT_ENABLED) && !defined(CONFIG_BT_NIMBLE_ENABLED)
#warning "Bluetooth does not appear to be enabled for this board target. If the sketch builds but never reports a tracker, check that the selected board has BLE."
#endif

#define GHOSTTAG_FW_VERSION "2.0"

/* NimBLE renamed the scan callback class and its setter in 2.x. */
#if defined(NIMBLE_CPP_VERSION_MAJOR) && NIMBLE_CPP_VERSION_MAJOR >= 2
#define GT_NIMBLE2 1
#else
#define GT_NIMBLE2 0
#endif

// ---- BLE advertisement signatures ----
static const uint16_t APPLE_COMPANY_ID   = 0x004C;
static const uint8_t  APPLE_TYPE_FINDMY  = 0x12; // offline finding / Find My
static const uint8_t  APPLE_TYPE_PAIRING = 0x07; // proximity pairing (AirPods)
static const uint8_t  APPLE_TYPE_NEARBY  = 0x10; // nearby info (phones, watches)
static const uint16_t TILE_UUID          = 0xFEED;
static const uint16_t SAMSUNG_UUID       = 0xFD5A; // SmartThings Find
static const uint16_t CHIPOLO_COMPANY_ID = 0x0157;

/*
 * A Find My advertisement of type 0x12 carries a length byte. The long form
 * (0x19 = 25 bytes) is the SEPARATED state - the tag has lost contact with its
 * owner and is broadcasting a full rotating key for anyone to relay. That is
 * the state a tag planted on you is in. The short form is the owner-nearby
 * case, which is almost always somebody's own kit sitting next to them.
 */
static const uint8_t APPLE_FINDMY_SEPARATED_LEN = 0x19;

enum {
  TYPE_NONE          = 0,
  TYPE_APPLE_FINDMY  = 1,
  TYPE_APPLE_NEARBY  = 2,
  TYPE_TILE          = 3,
  TYPE_SAMSUNG       = 4,
  TYPE_CHIPOLO       = 5,
};

static NimBLEScan* pScan = nullptr;
static bool scanning = false;
static uint32_t seenCount = 0;
static uint32_t lastBeat = 0;

#if GT_NIMBLE2
typedef const NimBLEAdvertisedDevice* GtDevice;
#else
typedef NimBLEAdvertisedDevice* GtDevice;
#endif

// Returns a TrackerType code, or TYPE_NONE if the advert is not a tracker.
static int classifyDevice(GtDevice dev) {
  if (dev->haveManufacturerData()) {
    std::string md = dev->getManufacturerData();
    if (md.size() >= 2) {
      uint16_t company = (uint8_t)md[0] | ((uint8_t)md[1] << 8);

      if (company == APPLE_COMPANY_ID && md.size() >= 3) {
        uint8_t appleType = (uint8_t)md[2];
        if (appleType == APPLE_TYPE_FINDMY) {
          uint8_t len = (md.size() >= 4) ? (uint8_t)md[3] : 0;
          // Separated = actively findable by strangers = the stalking case.
          return (len >= APPLE_FINDMY_SEPARATED_LEN) ? TYPE_APPLE_FINDMY
                                                     : TYPE_APPLE_NEARBY;
        }
        if (appleType == APPLE_TYPE_PAIRING || appleType == APPLE_TYPE_NEARBY) {
          return TYPE_APPLE_NEARBY;
        }
      }

      if (company == CHIPOLO_COMPANY_ID) return TYPE_CHIPOLO;

      /*
       * Samsung's company ID 0x0075 is DELIBERATELY not used here. It is on
       * every Samsung phone, watch, TV and pair of earbuds in the room, so
       * matching it reported half a train carriage as SmartTags. Only the
       * SmartThings Find service UUID below actually means "tracker".
       */
    }
  }

  for (int i = 0; i < dev->getServiceUUIDCount(); i++) {
    NimBLEUUID u = dev->getServiceUUID(i);
    if (u == NimBLEUUID(TILE_UUID))    return TYPE_TILE;
    if (u == NimBLEUUID(SAMSUNG_UUID)) return TYPE_SAMSUNG;
  }

  for (int i = 0; i < dev->getServiceDataCount(); i++) {
    NimBLEUUID u = dev->getServiceDataUUID(i);
    if (u == NimBLEUUID(SAMSUNG_UUID)) return TYPE_SAMSUNG;
    if (u == NimBLEUUID(TILE_UUID))    return TYPE_TILE;
  }

  return TYPE_NONE;
}

/*
 * The Flipper parses a line into a 96-byte buffer and drops anything longer,
 * so the name is both sanitised and hard-capped. A comma inside a device name
 * would otherwise be read as a field separator on the other side.
 */
static void sanitize(String& s) {
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == ',' || c == '\n' || c == '\r' || c < 32 || c > 126) s[i] = '_';
  }
  if (s.length() > 18) s = s.substring(0, 18);
}

static void report(GtDevice dev) {
  int type = classifyDevice(dev);
  if (type == TYPE_NONE) return;

  String mac = dev->getAddress().toString().c_str();
  mac.replace(":", "");
  mac.toUpperCase();
  if (mac.length() != 12) return;  // not an address we can send

  String name = "";
  if (dev->haveName()) {
    name = dev->getName().c_str();
    sanitize(name);
  }

  seenCount++;
  Serial.printf("GT1,%s,%d,%d,%s\n", mac.c_str(), dev->getRSSI(), type, name.c_str());
}

#if GT_NIMBLE2
class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override { report(dev); }
};
#else
class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override { report(dev); }
};
#endif

static ScanCallbacks scanCallbacks;

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
  pScan = NimBLEDevice::getScan();
#if GT_NIMBLE2
  pScan->setScanCallbacks(&scanCallbacks, /*wantDuplicates=*/true);
#else
  NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);
  pScan->setAdvertisedDeviceCallbacks(&scanCallbacks, /*wantDuplicates=*/true);
#endif
  pScan->setActiveScan(true);  // ask for names too
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->setMaxResults(0);  // use the callback, do not buffer

  Serial.printf("GTHELLO,%s\n", GHOSTTAG_FW_VERSION);
  startScan();  // auto-start; the Flipper can STOP/START on demand
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "START")      startScan();
    else if (cmd == "STOP")  stopScan();
    else if (cmd == "PING")  Serial.printf("GTHELLO,%s\n", GHOSTTAG_FW_VERSION);
  }

  /*
   * Heartbeat. Without it the Flipper has no way to tell "the board is fine and
   * there is genuinely nothing around" from "the board is dead", because a
   * quiet room produces no detections at all - and it used to report NO BOARD
   * to somebody whose hardware was working perfectly.
   */
  uint32_t now = millis();
  if (now - lastBeat >= 2000) {
    lastBeat = now;
    Serial.printf("GTALIVE,%lu\n", (unsigned long)seenCount);
  }

  // NimBLE may finish a scan cycle; keep it alive while we want to scan.
  if (scanning && !pScan->isScanning()) {
    pScan->start(0, false);
  }
  delay(20);
}
