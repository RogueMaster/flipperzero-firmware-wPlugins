//
//  SSE server with a load generator
//  it will auto adjust message push rate to minimize discards across all connected clients
//  per second stats is printed to a serial console and also published as SSE ping message
//  open /sse URL to start events generator

#include <Arduino.h>
#ifdef ESP32
  #include <AsyncTCP.h>
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#elif defined(TARGET_RP2040)
  #include <WebServer.h>
  #include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>

#if __has_include("ArduinoJson.h")
  #include <ArduinoJson.h>
  #include <AsyncJson.h>
  #include <AsyncMessagePack.h>
#endif

#include <LittleFS.h>

const char* htmlContent PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Sample HTML</title>
</head>
<body>
    <h1>Hello, World!</h1>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
</body>
</html>
)";

const char* staticContent PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Sample HTML</title>
</head>
<body>
    <h1>Hello, %IP%</h1>
</body>
</html>
)";

AsyncWebServer server(80);
AsyncEventSource events("/events");

/////////////////////////////////////////////////////////////////////////////////////////////////////

const char* PARAM_MESSAGE PROGMEM = "message";
const char* SSE_HTLM PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
  <title>Server-Sent Events</title>
  <script>
    if (!!window.EventSource) {
      var source = new EventSource('/events');
      source.addEventListener('open', function(e) {
        console.log("Events Connected");
      }, false);
      source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
        }
      }, false);
      source.addEventListener('message', function(e) {
        console.log("message", e);
      }, false);
      source.addEventListener('heartbeat', function(e) {
        console.log("heartbeat", e.data);
      }, false);
    }
  </script>
</head>
<body>
  <h1>Open your browser console!</h1>
</body>
</html>
)";

static const char* SSE_MSG = R"(Alice felt that this could not be denied, so she tried another question. 'What sort of people live about here?' 'In THAT direction,' the Cat said, waving its right paw round, 'lives a Hatter: and in THAT direction,' waving the other paw, 'lives a March Hare. Visit either you like: they're both mad.'
'But I don't want to go among mad people,' Alice remarked. 'Oh, you can't help that,' said the Cat: 'we're all mad here. I'm mad. You're mad.' 'How do you know I'm mad?' said Alice.
'You must be,' said the Cat, `or you wouldn't have come here.' Alice didn't think that proved it at all; however, she went on 'And how do you know that you're mad?' 'To begin with,' said the Cat, 'a dog's not mad. You grant that?'
)";

void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

static const char characters[] = "0123456789ABCDEF";
static size_t charactersIndex = 0;

void setup() {

  Serial.begin(115200);

#ifndef CONFIG_IDF_TARGET_ESP32H2
  /*
     WiFi.mode(WIFI_STA);
     WiFi.begin("SSID", "passwd");
     if (WiFi.waitForConnectResult() != WL_CONNECTED) {
       Serial.printf("WiFi Failed!\n");
       return;
     }
     Serial.print("IP Address: ");
     Serial.println(WiFi.localIP());
  */

  WiFi.mode(WIFI_AP);
  WiFi.softAP("esp-captive");
#endif

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", staticContent);
  });

  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("SSE Client reconnected! Last message ID that it gat is: %" PRIu32 "\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 1000);
  });

  server.on("/sse", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", SSE_HTLM);
  });

  // go to http://192.168.4.1/sse
  server.addHandler(&events);

  server.onNotFound(notFound);

  server.begin();
}

uint32_t lastSSE = 0;
uint32_t deltaSSE = 25;
uint32_t messagesSSE = 4; // how many messages to q each time
uint32_t sse_disc{0}, sse_enq{0}, sse_penq{0}, sse_second{0};

AsyncEventSource::SendStatus enqueue() {
  AsyncEventSource::SendStatus state = events.send(SSE_MSG, "message");
  if (state == AsyncEventSource::SendStatus::DISCARDED)
    ++sse_disc;
  else if (state == AsyncEventSource::SendStatus::ENQUEUED) {
    ++sse_enq;
  } else
    ++sse_penq;

  return state;
}

void loop() {
  uint32_t now = millis();
  if (now - lastSSE >= deltaSSE) {
    // enqueue messages
    for (uint32_t i = 0; i != messagesSSE; ++i) {
      auto err = enqueue();
      if (err == AsyncEventSource::SendStatus::DISCARDED || err == AsyncEventSource::SendStatus::PARTIALLY_ENQUEUED) {
        // throttle messaging a bit
        lastSSE = now + deltaSSE;
        break;
      }
    }

    lastSSE = millis();
  }

  if (now - sse_second > 1000) {
    String s;
    s.reserve(100);
    s = "Ping:";
    s += now / 1000;
    s += " clients:";
    s += events.count();
    s += " disc:";
    s += sse_disc;
    s += " enq:";
    s += sse_enq;
    s += " partial:";
    s += sse_penq;
    s += " avg wait:";
    s += events.avgPacketsWaiting();
    s += " heap:";
    s += ESP.getFreeHeap() / 1024;

    events.send(s, "heartbeat", now);
    Serial.println();
    Serial.println(s);

    // if we see discards or partial enqueues, let's decrease message rate, else - increase. So that we can come to a max sustained message rate
    if (sse_disc || sse_penq)
      ++deltaSSE;
    else if (deltaSSE > 5)
      --deltaSSE;

    sse_disc = sse_enq = sse_penq = 0;
    sse_second = now;
  }
}
