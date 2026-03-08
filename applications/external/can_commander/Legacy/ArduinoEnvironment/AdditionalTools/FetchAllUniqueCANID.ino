// CAN Commander Unique CAN ID Fetcher
// Matthew KuKanich

#include <esp32_can.h> 
#include <set>

std::set<uint32_t> uniqueCANIds;
bool printed = false;
bool newIdsFound = false;

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing CAN Bus...");
  CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5);
  CAN0.begin(500000); // 500Kbps
  CAN0.watchFor();
}

void loop()
{
  CAN_FRAME can_message;

  if (CAN0.read(can_message))
  {
    auto result = uniqueCANIds.insert(can_message.id);
    if (result.second) // If a new ID was inserted
    {
      newIdsFound = true;
    }
  }

  if (!printed || newIdsFound)
  {
    static unsigned long startTime = millis();
    if (millis() - startTime > 5000) // Adjust the time interval as needed
    {
      Serial.println("Unique CAN IDs:");
      for (const auto& id : uniqueCANIds)
      {
        Serial.print("0x");
        Serial.println(id, HEX);
      }
      printed = true; // Set the flag to true after printing once
      newIdsFound = false; // Reset the new IDs flag after printing
      startTime = millis(); // Reset the timer
    }
  }
}
