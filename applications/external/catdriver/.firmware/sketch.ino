#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

const wifi_promiscuous_filter_t filt = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};

typedef struct
{
  String recievedMac;
  String sendMac;
} DevicePacket;

#define maxCh 14 // max Channel -> US = 11, EU = 13, Japan = 14

DevicePacket packets[256];
int packetCount = 0;

int curChannel = 1;

enum ScanMode {
  MODE_IDLE = 0,
  MODE_ALL = 1,
  MODE_WIFI = 2,
  MODE_BLE = 3
};

ScanMode currentMode = MODE_IDLE;
BLEScan* pBLEScan = nullptr;
bool bleInitialized = false;

typedef struct
{
  unsigned frame_ctrl : 16;
  unsigned duration_id : 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl : 16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct
{
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

// Standard WiGLE AuthMode format
const char* getAuthModeWigle(wifi_auth_mode_t authmode) {
  switch (authmode) {
    case WIFI_AUTH_OPEN:
      return "[ESS]";
    case WIFI_AUTH_WEP:
      return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:
      return "[WPA-PSK-CCMP+TKIP][ESS]";
    case WIFI_AUTH_WPA2_PSK:
      return "[WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "[WPA-PSK-CCMP+TKIP][WPA2-PSK-CCMP+TKIP][ESS]";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "[WPA2-EAP-CCMP][ESS]";
    #ifdef WIFI_AUTH_WPA3_PSK
    case WIFI_AUTH_WPA3_PSK:
      return "[WPA3-SAE-CCMP][ESS]";
    #endif
    #ifdef WIFI_AUTH_WPA2_WPA3_PSK
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "[WPA2-PSK-CCMP][WPA3-SAE-CCMP][ESS]";
    #endif
    #ifdef WIFI_AUTH_WAPI_PSK
    case WIFI_AUTH_WAPI_PSK:
      return "[WAPI-PSK][ESS]";
    #endif
    #ifdef WIFI_AUTH_OWE
    case WIFI_AUTH_OWE:
      return "[OWE][ESS]";
    #endif
    default:
      return "[ESS]";
  }
}

// WiFi devices sniffer
void sniffer(void *buff, wifi_promiscuous_pkt_type_t type)
{
  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  if (packetCount >= 256)
  {
    return;
  }

  char addr1[18];
  char addr2[18];
  snprintf(addr1, sizeof(addr1), "%02X:%02X:%02X:%02X:%02X:%02X",
           hdr->addr1[0], hdr->addr1[1], hdr->addr1[2],
           hdr->addr1[3], hdr->addr1[4], hdr->addr1[5]);
  snprintf(addr2, sizeof(addr2), "%02X:%02X:%02X:%02X:%02X:%02X",
           hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
           hdr->addr2[3], hdr->addr2[4], hdr->addr2[5]);

  DevicePacket message;
  message.recievedMac = addr1;
  message.sendMac = addr2;

  packets[packetCount] = message;
  packetCount++;
}

void logWifiAP()
{
  // Scan all channels, include hidden networks
  int n = WiFi.scanNetworks(false, true);

  if (n > 0)
  {
    for (int i = 0; i < n; ++i)
    {
      String ssid = WiFi.SSID(i);
      // Clean SSID so commas or control characters do not break CSV parsing
      ssid.replace(',', '_');
      ssid.replace('\r', ' ');
      ssid.replace('\n', ' ');
      if (ssid.length() == 0) {
        ssid = "Hidden";
      }

      Serial.print("AR,");
      Serial.print(ssid);
      Serial.print(",");
      Serial.print(WiFi.BSSIDstr(i));
      Serial.print(",");
      Serial.print(WiFi.RSSI(i));
      Serial.print(",");
      Serial.print(WiFi.channel(i));
      Serial.print(",");
      Serial.print(getAuthModeWigle(WiFi.encryptionType(i)));
      Serial.print("\n");

      delay(15);
    }
    // Free scan buffer from ESP32 heap
    WiFi.scanDelete();
  }
}

void logWifiDevice()
{
  for (int i = 0; i < packetCount; i++)
  {
    DevicePacket pkt = packets[i];

    Serial.print("PK,");
    Serial.print(pkt.recievedMac);
    Serial.print(",");
    Serial.print(pkt.sendMac);
    Serial.print("\n");

    delay(15);
  }

  packetCount = 0;
}

void initBLE() {
  if (!bleInitialized) {
    BLEDevice::init("CatDriver");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    bleInitialized = true;
  }
}

void logBleDevices() {
  if (!pBLEScan) return;

  // Compatible with ESP32 core v2.x and v3.x
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    BLEScanResults* foundDevices = pBLEScan->start(1, false);
    if (foundDevices != nullptr) {
      int count = foundDevices->getCount();
      for (int i = 0; i < count; i++) {
        BLEAdvertisedDevice device = foundDevices->getDevice(i);
        String name = device.getName().c_str();
        name.replace(',', '_');
        name.replace('\r', ' ');
        name.replace('\n', ' ');
        if (name.length() == 0) {
          name = "Unknown_BLE";
        }

        String mac = device.getAddress().toString().c_str();
        mac.toUpperCase();
        int rssi = device.getRSSI();

        Serial.print("BT,");
        Serial.print(name);
        Serial.print(",");
        Serial.print(mac);
        Serial.print(",");
        Serial.print(rssi);
        Serial.print(",0,[LE]\n");

        delay(10);
      }
    }
  #else
    BLEScanResults foundDevices = pBLEScan->start(1, false);
    int count = foundDevices.getCount();
    for (int i = 0; i < count; i++) {
      BLEAdvertisedDevice device = foundDevices.getDevice(i);
      String name = device.getName().c_str();
      name.replace(',', '_');
      name.replace('\r', ' ');
      name.replace('\n', ' ');
      if (name.length() == 0) {
        name = "Unknown_BLE";
      }

      String mac = device.getAddress().toString().c_str();
      mac.toUpperCase();
      int rssi = device.getRSSI();

      Serial.print("BT,");
      Serial.print(name);
      Serial.print(",");
      Serial.print(mac);
      Serial.print(",");
      Serial.print(rssi);
      Serial.print(",0,[LE]\n");

      delay(10);
    }
  #endif

  pBLEScan->clearResults();
}

void checkSerialCommands() {
  while (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "MODE,ALL") {
      currentMode = MODE_ALL;
    } else if (cmd == "MODE,WIFI") {
      currentMode = MODE_WIFI;
    } else if (cmd == "MODE,BLE") {
      currentMode = MODE_BLE;
    } else if (cmd == "MODE,STOP" || cmd == "MODE,IDLE") {
      currentMode = MODE_IDLE;
      esp_wifi_set_promiscuous(false);
    } else if (cmd == "info" || cmd == "INFO" || cmd == "MODE,INFO") {
      Serial.println("INFO,CatDriver ESP32-S3 FW v1.0,WiFi+BLE");
    }
  }
}

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);

  initBLE();
}

void loop()
{
  checkSerialCommands();

  if (currentMode == MODE_IDLE) {
    delay(100);
    return;
  }

  if (currentMode == MODE_ALL || currentMode == MODE_WIFI) {
    if (curChannel >= maxCh) {
      curChannel = 1;
    } else {
      curChannel++;
    }
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);

    logWifiAP();
    logWifiDevice();
  }

  if (currentMode == MODE_ALL || currentMode == MODE_BLE) {
    esp_wifi_set_promiscuous(false);
    logBleDevices();
  }

  delay(150);
}
