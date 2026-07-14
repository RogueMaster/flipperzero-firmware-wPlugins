#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String apiKey;
String endpoint;
String userName;

void connectToWiFi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  WiFi.begin(ssid, password);

  int retries = 0;
  const int maxRetries = 30;
  while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
    delay(1000);
    Serial.print(".");
    retries++;
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Error: Could not connect to WiFi network.");
  }
}

void loadAPIKey() {
  Serial.println("Waiting for API key (send via Serial)...");

  while (!Serial.available());
  apiKey = Serial.readStringUntil('\n');
  apiKey.trim();
  endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent?key=" + apiKey;
  Serial.println("API key loaded successfully.");
}

void saveAP(const char* ssid, const char* password) {
  Serial.println("Saving Access Point information...");
  String apData = String(ssid) + "//" + String(password) + "\n";
  Serial.print("Writing to SavedAPs.txt: ");
  Serial.println(apData);
}

bool loadSavedAP(String &ssid, String &password) {
  Serial.println("Loading saved Access Points from SavedAPs.txt...");

  while (!Serial.available());
  String apData = Serial.readStringUntil('\n');

  int separatorIndex = apData.indexOf("//");
  if (separatorIndex == -1) return false;

  ssid = apData.substring(0, separatorIndex);
  password = apData.substring(separatorIndex + 2);

  ssid.trim();
  password.trim();

  return true;
}

bool autoConnectToWiFi(const String &networks) {
  Serial.println("Attempting to auto-connect to known networks...");

  int n = WiFi.scanNetworks();

  int startIndex = 0;
  while (startIndex < networks.length()) {
    int separatorIndex = networks.indexOf(",", startIndex);
    if (separatorIndex == -1) separatorIndex = networks.length();

    String pair = networks.substring(startIndex, separatorIndex);
    pair.trim();

    int ssidPasswordSeparator = pair.indexOf("//");
    if (ssidPasswordSeparator == -1) {
      Serial.println("Invalid format, skipping: " + pair);
      startIndex = separatorIndex + 1;
      continue;
    }

    String ssid = pair.substring(0, ssidPasswordSeparator);
    String password = pair.substring(ssidPasswordSeparator + 2);
    ssid.trim();
    password.trim();

    for (int i = 0; i < n; ++i) {
      if (ssid == WiFi.SSID(i)) {
        Serial.print("Found matching SSID: ");
        Serial.println(ssid);
        connectToWiFi(ssid.c_str(), password.c_str());
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Connected successfully to " + ssid);
          return true;
        } else {
          Serial.println("Failed to connect to " + ssid);
        }
      }
    }

    startIndex = separatorIndex + 1;
  }
  Serial.println("No matching networks found.");
  return false;
}

void manualConnect() {
  Serial.println("Please enter the SSID of the WiFi network you want to connect to:");
  while (!Serial.available());
  String ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.println("Please enter the password for the WiFi network:");
  while (!Serial.available());
  String password = Serial.readStringUntil('\n');
  password.trim();

  String formattedInput = ssid + "//" + password;

  connectToWiFi(ssid.c_str(), password.c_str());

  if (WiFi.status() == WL_CONNECTED) {
    saveAP(ssid.c_str(), password.c_str());
  } else {
    Serial.println("Failed to connect to " + ssid + ". Manual entry required.");
  }
}

void changeName() {
  Serial.println("Please enter the new name:");
  while (!Serial.available());
  userName = Serial.readStringUntil('\n');
  userName.trim();
  Serial.println("Name changed successfully to: " + userName);
}

void setup() {
  Serial.begin(115200);
  delay(10);

  loadAPIKey();

  Serial.println("Welcome to the Gemini ESP32!");
  Serial.println("Please enter your name:");
  while (!Serial.available());
  userName = Serial.readStringUntil('\n');
  userName.trim();

  Serial.println("Hello, " + userName + "!");

  Serial.println("Scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  Serial.println("Available WiFi networks:");
  for (int i = 0; i < n; ++i) {
    Serial.print("[");
    Serial.print(i + 1);
    Serial.print("] ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.println(")");
  }

  while (!Serial.available());
  String networks = Serial.readStringUntil('\n');
  networks.trim();

  if (!autoConnectToWiFi(networks)) {
    manualConnect();
  }
}

void loop() {
  if (Serial.available() > 0) {
    String userQuery = Serial.readStringUntil('\n');
    userQuery.trim();

    if (userQuery.equalsIgnoreCase("newname")) {
      changeName();
    }
    else if (userQuery.equalsIgnoreCase("restart_board")) {
      Serial.println("Restarting the board...");
      delay(1000);
      ESP.restart();
    }
    else if (userQuery.length() > 0) {
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        http.setTimeout(15000);
        http.begin(endpoint);
        http.addHeader("Content-Type", "application/json");

        String payload = "{\"contents\":[{\"parts\":[{\"text\":\"" + userQuery + "\"}]}]}";

        int httpResponseCode = http.POST(payload);

        if (httpResponseCode > 0) {
          String response = http.getString();

          DynamicJsonDocument doc(4096);
          DeserializationError error = deserializeJson(doc, response);

          if (!error) {
            const char* text = doc["candidates"][0]["content"]["parts"][0]["text"];
            Serial.println(userName + ": \"" + userQuery + "\"");
            Serial.println("Gemini response: \"" + String(text) + "\"");
          } else {
            Serial.print("Error parsing JSON: ");
            Serial.println(error.c_str());
          }
        } else {
          Serial.print("Request error: ");
          Serial.println(httpResponseCode);
          Serial.print("HTTP error: ");
          Serial.println(http.errorToString(httpResponseCode).c_str());
        }

        http.end();
      } else {
        Serial.println("WiFi connection error");
      }
    }
  }
}
