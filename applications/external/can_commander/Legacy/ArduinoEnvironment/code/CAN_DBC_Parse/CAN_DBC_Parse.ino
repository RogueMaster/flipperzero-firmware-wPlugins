// CAN DBC file parser by Matthew KuKanich
#include <SD.h>
#include <mcp2515.h>

const int chipSelect = 11; // Define the chip select pin for the SD card

struct can_frame canMsg;
MCP2515 mcp2515(10); // MCP2515 chip select

unsigned int selectedMessageId = 0;

struct Signal {
    String name;
    uint8_t startBit;
    uint8_t length;
    float scale;
    float offset;
    String unit;
    bool isLittleEndian;
};

Signal signals[10];
int numSignals = 0;

void setup() {
    Serial.begin(115200);

    if (SD.begin(chipSelect)) {
        Serial.println("SD card is ready to use.");
    } else {
        Serial.println("SD card initialization failed");
        return;
    }

    browseAndSelectDBC();

    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ); // High-Speed CAN Bus
    mcp2515.setNormalMode();
}

void loop() {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        if (canMsg.can_id == selectedMessageId) {
            decodeCANMessage(&canMsg, signals, numSignals);
        }
    }
}

void browseAndSelectDBC() {
    File root = SD.open("/");
    File entry = root.openNextFile();
    
    Serial.println("Available DBC Files:");
    
    while (entry) {
        if (entry.isDirectory()) {
            // Skip directories
        } else {
            // Check if the file has a .dbc extension
            if (String(entry.name()).endsWith(".dbc")) {
                // Print the file name
                Serial.println(entry.name());
            }
        }
        
        entry.close();
        entry = root.openNextFile();
    }
    
    root.close();
    
    // Allow user to select a file and open it
    Serial.println("Enter the name of the DBC file you want to open:");
    while (Serial.available() == 0) {
    }
    
    String selectedFileName = Serial.readStringUntil('\n');
    selectedFileName.trim(); 
    
    File dbcFile = SD.open(selectedFileName.c_str());
    if (dbcFile) {
        Serial.println("DBC file opened successfully.");
        decodeDBC(dbcFile);
    } else {
        Serial.println("Error opening the selected DBC file.");
    }
}

void decodeDBC(File dbcFile) {
    if (dbcFile) {
        Serial.println("Listing CAN Messages from DBC file:");
        
        while (dbcFile.available()) {
            String line = dbcFile.readStringUntil('\n');
            line.trim();
            
            if (line.startsWith("BO_")) {
                // Extract message ID and name
                int spaceIndex = line.indexOf(' ', 4); // Find the space after "BO_"
                String messageId = line.substring(4, spaceIndex);
                int messageNameStart = line.indexOf(' ', spaceIndex + 1) + 1;
                int messageNameEnd = line.indexOf(':', messageNameStart);
                String messageName = line.substring(messageNameStart, messageNameEnd);
                
                Serial.print("Message ID: ");
                Serial.print(messageId);
                Serial.print(" - Message Name: ");
                Serial.println(messageName);
            }
        }
        
        dbcFile.close();
    } else {
        Serial.println("Error opening the DBC file.");
    }
}

uint32_t extractSignalValue(struct can_frame *canMsg, Signal signal) {
    uint32_t value = 0;
    uint8_t startByte = signal.startBit / 8;
    uint8_t startBitInByte = signal.startBit % 8;
    uint8_t bitsRemaining = signal.length;
    
    for (int i = startByte; i < 8 && bitsRemaining > 0; i++) {
        uint8_t byte = canMsg->data[i];
        
        if (startBitInByte > 0) {
            byte <<= startBitInByte;
        }
        
        byte >>= (8 - min(bitsRemaining, 8 - startBitInByte));
        
        value <<= min(bitsRemaining, 8 - startBitInByte);
        value |= byte;
        
        bitsRemaining -= min(bitsRemaining, 8 - startBitInByte);
        startBitInByte = 0;
    }
    
    return value;
}

void decodeCANMessage(struct can_frame *canMsg, Signal signals[], int numSignals) {
    for (int i = 0; i < numSignals; i++) {
        Signal signal = signals[i];
        
        uint32_t rawValue = extractSignalValue(canMsg, signal);
        float decodedValue = rawValue * signal.scale + signal.offset;
        
        // Print the decoded signal value
        Serial.print("Signal ");
        Serial.print(signal.name);
        Serial.print(": ");
        Serial.println(decodedValue);
    }
}

