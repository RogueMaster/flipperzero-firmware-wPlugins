#include <SPI.h>
#include <mcp2515.h>
 
struct can_frame canMsg;
MCP2515 mcp2515(10);
int cntr = 0;
unsigned long oldTime = 0;
 
 
void setup() {
  Serial.begin(115200);
 
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();
 
  Serial.println("------- CAN Speedtest ----------");
}
 
void loop() {
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    cntr++;
  }
 
  if ((millis()-oldTime)>1000) {
    oldTime = millis();
    Serial.print(cntr);
    Serial.println(" msg/sec");
    cntr = 0;      
  }
}
