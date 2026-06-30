#include <SPI.h>

char str[] = "Hello Flipper Zero SPI Terminal!\n";
char* pos = str;
const char* end = str + sizeof(str);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D3, OUTPUT);
  SPI.begin();
  Serial.begin(9600);
  // put your setup code here, to run once:
}


void loop() {
  analogWrite(LED_BUILTIN, 250);
  digitalWrite(D3, LOW);
  while (pos != end) {
    SPI.transfer(*pos);
    pos++;
  }
  pos = str;
  digitalWrite(D3, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);

delay(100);
}
