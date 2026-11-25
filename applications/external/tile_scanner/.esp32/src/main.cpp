
#include <Arduino.h>
#include <NimBLEDevice.h>

#include "driver/uart.h"

#define SCAN_INTERVAL_MS    100
#define SCAN_WINDOW_MS      99

const NimBLEUUID TILE_SERVICE_UUID = NimBLEUUID((uint16_t)0xFEED);

#define UART_PORT_NUM      UART_NUM_1
#define UART_BAUD_RATE     115200
#define UART_TX            GPIO_NUM_16
#define UART_RX            GPIO_NUM_17
#define UART_BUF_SIZE      128

#define HEARTBEAT_TIMEOUT 1000

uint32_t g_lastHeartbeat = 0;

// Callback for BLE advertisements
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        // Tile Tags broadcast a 16 bit service UUID of 0xFEED
        // Any BLE device not broadcasting this is not a Tile Tag
        if(!advertisedDevice->haveServiceUUID()) {
            return;
        }
        
        if(advertisedDevice->getServiceUUID().bitSize() != BLE_UUID_TYPE_16){
            return;
        }
        
        if(advertisedDevice->getServiceUUID() != TILE_SERVICE_UUID){
            return;
        }
        
        printf("%ld: %s\n", millis(), advertisedDevice->toString().c_str());
        
        char bleData[32]; // 17 for mac address + 4 for rssi + 4 for fixed chars
        int payloadLength = snprintf(bleData, sizeof(bleData), "%s,%d\n", advertisedDevice->getAddress().toString().c_str(), advertisedDevice->getRSSI());
        uart_write_bytes(UART_PORT_NUM, bleData, payloadLength);
    }
    
    void onScanEnd(const NimBLEScanResults& scanResults, int reason) override {
        // no-op
        Serial.printf("Scan ended\n");
    }
    
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
        // no-op
    }
};

void startScanner() {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    
    printf("Starting Scanner\n");
    
    // continue = false -> To clear results
    // restart = false -> Do not restart scan if already in progress
    pScan->start(0, false, false);
}

void stopScanner() {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    
    printf("Stopping Scanner\n");
    pScan->stop();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    NimBLEDevice::init("Tile-Scanner");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Maximum TX power for best range

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(new ScanCallbacks(), true); // Enable duplicates
    pScan->setActiveScan(true);
    pScan->setInterval(SCAN_INTERVAL_MS);
    pScan->setWindow(SCAN_WINDOW_MS);
    pScan->setMaxResults(0); // Immediately forget the result since the callback handles it immediately
    
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void loop() {
    // Note: Commands are single letters
    char data = '\0';
    int len = uart_read_bytes(UART_PORT_NUM, &data, 1, pdMS_TO_TICKS(500));
    
    if(len > 0 && data != '\0'){
        printf("Received: %c (%02X)\n", data, (uint8_t)data);
        
        switch(data) {
            case 'B':
            case 'b': // Begin scanning
                startScanner();
                break;
            case 'S':
            case 's': // Stop scanning
                stopScanner();
                break;
            case 'T':
            case 't': // Toggle scanning
                if(NimBLEDevice::getScan()->isScanning()){
                    stopScanner();
                }else{
                    startScanner();
                }
                break;
            default:
                printf("Unknown command\n");
        }
    }
    
    if(millis() - g_lastHeartbeat > HEARTBEAT_TIMEOUT) {
        char heartbeat[3] = {'h', NimBLEDevice::getScan()->isScanning() ? '1' : '0', '\n'};
        uart_write_bytes(UART_PORT_NUM, heartbeat, sizeof(heartbeat));
        g_lastHeartbeat = millis();
    }
    
    delay(20);
}
