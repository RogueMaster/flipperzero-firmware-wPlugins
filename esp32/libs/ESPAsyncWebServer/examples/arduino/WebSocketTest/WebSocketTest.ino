// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright 2016-2026 Hristo Gochkov, Mathieu Carbou, Emil Muratov, Will Miles

//
// WebSocketTest
//
// A coverage-focused example for the WebSocket server send-path, designed to
// exercise the PR #462 "Fully handle websocket frame tearing" refactoring.
//
// Goals:
//   * exercise every AsyncWebSocket / AsyncWebSocketClient send API,
//   * stress the partial-frame ("tearing") resume logic with large / bursty
//     payloads and interleaved control frames,
//   * validate that shared buffers are not mutated when masked per-client,
//   * measure ping RTT via the new pong-data callback,
//   * cover client lifecycle: connect, disconnect, close, max-clients
//     middleware, cleanupClients(), keepAlive, queue-full policy.
//
// Two ways to drive it:
//
//   1. Open http://<ESP_IP>/ in a browser and click the buttons.
//   2. Use websocat / curl from a terminal (commands are listed on the page
//      and on the serial console at boot).
//
// The WS endpoint is /ws .  Commands are sent as TEXT frames whose payload is
// a single line:  "<command>" or "<command> <arg>".
//
// Available commands:
//
//   ping                      -> server pings the sender (pong RTT printed)
//   pingall                   -> pingAll() with a 4-byte timestamp payload
//   echo <text>               -> text() back to sender
//   echoall <text>            -> textAll()
//   bin <hex...>              -> binary() back to sender (e.g. "bin deadbeef")
//   binall <hex...>           -> binaryAll()
//   binall <len>              -> binaryAll() of <len> deterministic bytes
//   text <len>                -> text() of <len> deterministic bytes (one client)
//   textall <len>             -> textAll() of <len> deterministic bytes
//   burst <n> <len>           -> enqueue n text(<len>) frames back-to-back
//   masked <len>              -> per-client masked broadcast of one shared buffer
//   close                      -> close() the sender with code 1000
//   closeall                   -> closeAll(1011, "server going away")
//   drop <id>                  -> close(id) a specific client by id
//   status                     -> serial dump of every client
//
// Boundary lengths worth trying (exercises frame header size variants and
// fragmentation across the TCP MSS):
//
//   textall 0      (empty frame: header only, no mask byte)
//   textall 125    (largest 7-bit length)
//   textall 126    (smallest 16-bit length)
//   textall 1460   (~one TCP MSS, single fragment boundary)
//   textall 2000   (forces fragmentation across MSS)
//   textall 8192   (multi-frame, exercises resumable sends)
//   textall 65000  (large message, many fragments)
//
// With websocat:
//
//   websocat ws://192.168.4.1/ws
//   > textall 8192
//   > burst 100 2000
//   > masked 4000
//   > ping
//
// Masked-broadcast deliberately violates RFC 6455 5.1 (servers MUST NOT mask),
// so it requires a tolerant client:
//
//   websocat4 --ws-ignore-invalid-masks ws://192.168.4.1/ws
//   > masked 4000
//
// HTTP-only checks via curl:
//
//   curl -i http://192.168.4.1/                       # control page
//   curl -i http://192.168.4.1/clients                # live client count (JSON)
//   curl -i -H "Connection: Upgrade" -H "Upgrade: websocket" \
//        -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
//        -H "Sec-WebSocket-Version: 13" http://192.168.4.1/ws   # handshake
//

#include <Arduino.h>

#if defined(ESP32) || defined(LIBRETINY)
#include <AsyncTCP.h>
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#elif defined(TARGET_RP2040) || defined(TARGET_RP2350) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include <RPAsyncTCP.h>
#include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

// ...........................................................................
// Configuration
// ...........................................................................

static const char *WIFI_SSID = "esp-ws-test";
static const uint16_t HTTP_PORT = 80;
static const char *WS_URL = "/ws";
// Maximum simultaneous WS clients allowed by the middleware gate.
static const size_t MAX_WS_CLIENTS = 4;
// Periodic background broadcaster interval (ms).  Keeps the queues busy so
// fragmentation / tearing paths are exercised even with no user input.
static const uint32_t BG_BROADCAST_MS = 2000;
// Size of the periodic background broadcast (large enough to fragment).
static const size_t BG_BROADCAST_LEN = 4000;

// ...........................................................................
// Globals
// ...........................................................................

static AsyncWebServer server(HTTP_PORT);
static AsyncWebSocket ws(WS_URL);

// Monotonic message counter baked into deterministic payloads so a receiver
// can verify ordering / completeness across fragments and bursts.
static uint32_t g_seq = 0;

// ...........................................................................
// Payload helpers
// ...........................................................................

// Build a deterministic, self-describing payload of exactly `len` bytes:
//   "SEQ=<n> LEN=<len>\n"  followed by repeating '0'..'9' filler.
// Lets a client assert that every byte arrived intact after reassembly.
// Returns false on allocation failure (so callers can avoid abort() on
// devices with tight RAM like the ESP32).
static bool fillDeterministic(std::vector<uint8_t> &out, size_t len, uint32_t seq) {
  char header[48];
  const int written = snprintf(header, sizeof(header), "SEQ=%" PRIu32 " LEN=%u\n", seq, (unsigned)len);
  const size_t headerLen = std::min((size_t)std::max(written, 0), sizeof(header));
  // Avoid std::bad_alloc: some cores (RP2040, ESP8266) build with -fno-exceptions
  // so resize() would abort the whole program on heap exhaustion.  Pre-check the
  // free heap and bail out gracefully instead so the test keeps running.
  out.clear();
  if (len) {
#if defined(ESP32) || defined(ESP8266) || defined(LIBRETINY)
    if (ESP.getFreeHeap() < len + 256) {
      return false;
    }
#endif
    out.reserve(len);
    if (out.capacity() < len) {
      return false;
    }
    out.resize(len);
  }
  const size_t n = std::min(headerLen, len);
  memcpy(out.data(), header, n);
  for (size_t i = n; i < len; i++) {
    out[i] = '0' + ((i - n) % 10);
  }
  return true;
}

static AsyncWebSocketSharedBuffer makeShared(size_t len, uint32_t seq) {
  auto buffer = std::make_shared<std::vector<uint8_t>>();
  if (!fillDeterministic(*buffer, len, seq)) {
    return nullptr;  // allocation failed -- caller should check
  }
  return buffer;
}

// Hex-decode a space-separated hex string (e.g. "deadbeef") into out.
// Returns false on bad input.
static bool hexDecode(const char *s, std::vector<uint8_t> &out) {
  out.clear();
  while (*s) {
    while (*s == ' ' || *s == '\t') {
      s++;
    }
    if (!*s) {
      break;
    }
    char hi = *s++;
    if (!*s) {
      return false;
    }
    char lo = *s++;
    auto nib = [](char c, uint8_t &v) -> bool {
      if (c >= '0' && c <= '9') {
        v = c - '0';
        return true;
      }
      if (c >= 'a' && c <= 'f') {
        v = c - 'a' + 10;
        return true;
      }
      if (c >= 'A' && c <= 'F') {
        v = c - 'A' + 10;
        return true;
      }
      return false;
    };
    uint8_t h, l;
    if (!nib(hi, h) || !nib(lo, l)) {
      return false;
    }
    out.push_back((h << 4) | l);
  }
  return true;
}

// ...........................................................................
// Test actions (server -> client(s))
// ...........................................................................

// Per-client masked broadcast of ONE shared buffer to every connected client.
// Each client gets its own random mask key; if the library ever mutates the
// shared buffer while masking, clients will receive different decoded bytes
// and the test fails.  (RFC 6455 5.1 forbids server-side masking, so this is
// a deliberate conformance violation -- use a tolerant client like websocat4
// --ws-ignore-invalid-masks.)
static size_t sendMaskedBroadcast(size_t len) {
  AsyncWebSocketSharedBuffer buffer = makeShared(len, g_seq++);
  if (!buffer) {
    Serial.printf("[masked] ERROR: could not allocate %u bytes\n", (unsigned)len);
    return 0;
  }
  size_t sent = 0;
  for (auto &client : ws.getClients()) {
    if (client.status() == WS_CONNECTED && client.message(buffer, WS_TEXT, /*mask=*/true)) {
      sent++;
    }
  }
  Serial.printf("[masked] len=%u sent to %u client(s), buffer use_count=%ld\n", (unsigned)len, (unsigned)sent, (long)buffer.use_count());
  return sent;
}

// Enqueue `n` text frames of `len` bytes to one client, back to back.
// Puts the queue under load so the "one outstanding frame" invariant and
// resumable-send logic both get exercised.  Stops early if the WS queue is
// full (the library drops/closes the client) or if an allocation fails (heap
// exhaustion) so we never trigger a std::bad_alloc abort.
static void sendBurst(AsyncWebSocketClient *client, size_t n, size_t len) {
  for (size_t i = 0; i < n; i++) {
    AsyncWebSocketSharedBuffer buffer = makeShared(len, g_seq++);
    if (!buffer) {
      Serial.printf("[burst] ALLOC FAILED at %u/%u (len=%u)\n", (unsigned)i, (unsigned)n, (unsigned)len);
      return;
    }
    if (!client->text(buffer)) {
      Serial.printf("[burst] queue full at %u/%u (len=%u)\n", (unsigned)i, (unsigned)n, (unsigned)len);
      return;
    }
  }
  Serial.printf("[burst] enqueued %u x %u bytes\n", (unsigned)n, (unsigned)len);
}

// ...........................................................................
// Command parser (text frames from clients)
// ...........................................................................

static void handleCommand(AsyncWebSocketClient *client, const char *cmd) {
  // Skip leading whitespace.
  while (*cmd == ' ' || *cmd == '\t') {
    cmd++;
  }
  if (!*cmd) {
    return;
  }

  // Split first token.
  const char *sp = strchr(cmd, ' ');
  size_t tokLen = sp ? (size_t)(sp - cmd) : strlen(cmd);
  // ESP8266's String has no (const char*, size_t) constructor, so build the
  // verb explicitly in a portable way.
  String verb;
  verb.reserve(tokLen);
  for (size_t i = 0; i < tokLen; i++) {
    verb.concat(cmd[i]);
  }
  const char *rest = sp ? sp + 1 : "";
  while (*rest == ' ') {
    rest++;
  }

  Serial.printf("[cmd] client #%" PRIu32 ": %s %s\n", client->id(), verb.c_str(), rest);

  if (verb == "ping") {
    uint32_t t = micros();
    client->ping((const uint8_t *)&t, sizeof(t));
  } else if (verb == "pingall") {
    uint32_t t = micros();
    ws.pingAll((const uint8_t *)&t, sizeof(t));
  } else if (verb == "echo") {
    client->text(rest);
  } else if (verb == "echoall") {
    ws.textAll(rest);
  } else if (verb == "bin") {
    std::vector<uint8_t> bytes;
    if (hexDecode(rest, bytes) && !bytes.empty()) {
      client->binary(bytes.data(), bytes.size());
    } else {
      client->text("ERR: bad hex");
    }
  } else if (verb == "binall") {
    // Two forms: "binall <hex>" or "binall <len>" (deterministic payload).
    char *end = nullptr;
    unsigned long len = strtoul(rest, &end, 10);
    if (end != rest && (*end == '\0' || *end == ' ')) {
      AsyncWebSocketSharedBuffer buffer = makeShared(len, g_seq++);
      if (!buffer) {
        client->text("ERR: allocation failed");
      } else {
        ws.binaryAll(buffer);
      }
    } else {
      std::vector<uint8_t> bytes;
      if (hexDecode(rest, bytes) && !bytes.empty()) {
        ws.binaryAll(bytes.data(), bytes.size());
      } else {
        ws.textAll("ERR: bad hex");
      }
    }
  } else if (verb == "text") {
    unsigned long len = strtoul(rest, nullptr, 10);
    if (len > 0 && len <= 65000) {
      AsyncWebSocketSharedBuffer buffer = makeShared(len, g_seq++);
      if (!buffer) {
        client->text("ERR: allocation failed");
      } else {
        client->text(buffer);
      }
    } else {
      client->text("ERR: 0 < len <= 65000");
    }
  } else if (verb == "textall") {
    unsigned long len = strtoul(rest, nullptr, 10);
    if (len <= 65000) {
      AsyncWebSocketSharedBuffer buffer = makeShared(len, g_seq++);
      if (!buffer) {
        ws.textAll("ERR: allocation failed");
      } else {
        ws.textAll(buffer);
      }
    } else {
      ws.textAll("ERR: 0 <= len <= 65000");
    }
  } else if (verb == "burst") {
    // "burst <n> <len>"
    unsigned long n = strtoul(rest, nullptr, 10);
    const char *p2 = strchr(rest, ' ');
    unsigned long len = p2 ? strtoul(p2 + 1, nullptr, 10) : 0;
    if (n > 0 && n <= 1000 && len > 0 && len <= 65000) {
      sendBurst(client, n, len);
    } else {
      client->text("ERR: 0 < n <= 1000 and 0 < len <= 65000");
    }
  } else if (verb == "masked") {
    unsigned long len = strtoul(rest, nullptr, 10);
    if (len > 0 && len <= 65000) {
      sendMaskedBroadcast(len);
    } else {
      client->text("ERR: 0 < len <= 65000");
    }
  } else if (verb == "close") {
    client->close(1000, "bye");
  } else if (verb == "closeall") {
    ws.closeAll(1011, "server going away");
  } else if (verb == "drop") {
    unsigned long id = strtoul(rest, nullptr, 10);
    ws.close((uint32_t)id, 1000, "dropped by command");
  } else if (verb == "status") {
    Serial.printf("WS clients: %u connected / %u in list\n", (unsigned)ws.count(), (unsigned)ws.getClients().size());
    for (auto &c : ws.getClients()) {
      Serial.printf("  id=%" PRIu32 " status=%d queueLen=%u queueFull=%d\n", c.id(), (int)c.status(), (unsigned)c.queueLen(), c.queueIsFull() ? 1 : 0);
    }
  } else {
    client->text("ERR: unknown command. Try: ping pingall echo echoall bin binall text textall burst masked close closeall drop status");
  }
}

// ...........................................................................
// WebSocket event handler
// ...........................................................................

static void onWsEvent(AsyncWebSocket *srv, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
    {
      Serial.printf(
        "[ws] connect id=%" PRIu32 " from %s:%u (now %u client(s))\n", client->id(), client->remoteIP().toString().c_str(), client->remotePort(),
        (unsigned)ws.count()
      );
      // Enable application-level keepalive: every 10s the library will emit a
      // ping with the AWSC_PING_PAYLOAD; the pong reply exercises the new
      // pong-data forwarding path added in PR #462.
      client->keepAlivePeriod(10);
      ws.textAll("a new client connected");
      // Probe the new client immediately: a small ping exercises control-frame
      // queuing on a fresh connection.
      uint32_t t = micros();
      client->ping((const uint8_t *)&t, sizeof(t));
      break;
    }
    case WS_EVT_DISCONNECT: Serial.printf("[ws] disconnect id=%" PRIu32 " (now %u client(s))\n", client->id(), (unsigned)ws.count()); break;

    case WS_EVT_ERROR: Serial.printf("[ws] error id=%" PRIu32 "\n", client->id()); break;

    case WS_EVT_PONG:
    {
      // PR #462 forwards the pong payload to the callback.  If it's our
      // 4-byte micros() timestamp, we can measure RTT.
      if (len == sizeof(uint32_t)) {
        uint32_t sent;
        memcpy(&sent, data, sizeof(sent));
        Serial.printf("[ws] pong id=%" PRIu32 " rtt=%" PRIu32 " us\n", client->id(), (uint32_t)(micros() - sent));
      } else {
        Serial.printf("[ws] pong id=%" PRIu32 " len=%u\n", client->id(), (unsigned)len);
      }
      break;
    }

    case WS_EVT_DATA:
    {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      // We only care about fully-assembled TEXT messages for commands.
      if (info->final && info->index == 0 && info->len == len && info->message_opcode == WS_TEXT) {
        // Null-terminate (library guarantees it for text frames, but be safe).
        char *text = (char *)data;
        size_t textLen = strnlen(text, len);
        text[textLen] = '\0';
        handleCommand(client, text);
      } else if (info->message_opcode == WS_BINARY) {
        // Echo binary frames back as binary, so clients can round-trip checks.
        client->binary(data, len);
      } else {
        // Log fragmented reception (useful when a client sends a >MSS frame).
        Serial.printf(
          "[ws] fragmented frame id=%" PRIu32 " num=%" PRIu32 " idx=%" PRIu64 " len=%" PRIu64 " final=%u op=%u\n", client->id(), info->num, info->index,
          info->len, info->final, info->message_opcode
        );
      }
      break;
    }

    default: break;
  }
  (void)srv;
}

// ...........................................................................
// HTTP control page
// ...........................................................................

static const char HTML_PAGE[] PROGMEM = "\n"
                                        "<!DOCTYPE html>\n"
                                        "<html>\n"
                                        "<head>\n"
                                        "<meta charset=\"utf-8\">\n"
                                        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
                                        "<title>WebSocket Test</title>\n"
                                        "<style>\n"
                                        "  body{font:14px system-ui,sans-serif;margin:16px;color:#222}\n"
                                        "  h1{font-size:18px;margin:0 0 8px}\n"
                                        "  button{margin:2px;padding:6px 10px;border:1px solid #888;background:#f5f5f5;border-radius:4px;cursor:pointer}\n"
                                        "  button:hover{background:#eaeaea}\n"
                                        "  pre{background:#111;color:#0f0;padding:8px;overflow:auto;height:260px;border-radius:4px}\n"
                                        "  input{width:80px}\n"
                                        "  .row{margin:6px 0}\n"
                                        "  code{background:#eee;padding:1px 4px;border-radius:3px}\n"
                                        "</style>\n"
                                        "</head>\n"
                                        "<body>\n"
                                        "<h1>WebSocket Test</h1>\n"
                                        "<div id=\"status\">WS: connecting...</div>\n"
                                        "\n"
                                        "<div class=\"row\">\n"
                                        "  <b>Control frames</b><br>\n"
                                        "  <button onclick='cmd(\"ping\")'>ping (this client)</button>\n"
                                        "  <button onclick='cmd(\"pingall\")'>pingAll()</button>\n"
                                        "  <button onclick='cmd(\"status\")'>status (serial)</button>\n"
                                        "</div>\n"
                                        "\n"
                                        "<div class=\"row\">\n"
                                        "  <b>Echo</b><br>\n"
                                        "  <button onclick='cmd(\"echo hello\")'>echo hello</button>\n"
                                        "  <button onclick='cmd(\"echoall hello\")'>echoAll hello</button>\n"
                                        "</div>\n"
                                        "\n"
                                        "<div class=\"row\">\n"
                                        "  <b>Boundary sizes (textAll)</b><br>\n"
                                        "  <button onclick='cmd(\"textall 0\")'>0</button>\n"
                                        "  <button onclick='cmd(\"textall 125\")'>125</button>\n"
                                        "  <button onclick='cmd(\"textall 126\")'>126</button>\n"
                                        "  <button onclick='cmd(\"textall 1460\")'>1460 (MSS)</button>\n"
                                        "  <button onclick='cmd(\"textall 2000\")'>2000 (frag)</button>\n"
                                        "  <button onclick='cmd(\"textall 8192\")'>8192</button>\n"
                                        "  <button onclick='cmd(\"textall 65000\")'>65000</button>\n"
                                        "</div>\n"
                                        "\n"
                                        "<div class=\"row\">\n"
                                        "  <b>Per-client</b><br>\n"
                                        "  <button onclick='cmd(\"text 4000\")'>text 4000</button>\n"
                                        "  <button onclick='cmd(\"bin deadbeef\")'>bin deadbeef</button>\n"
                                        "  <button onclick='cmd(\"binall 3000\")'>binall 3000</button>\n"
                                        "</div>\n"
                                        "\n"
                                        "<div class=\"row\">\n"
                                        "  <b>Stress</b><br>\n"
                                        "  <button onclick='cmd(\"burst 100 2000\")'>burst 100x2000</button>\n"
                                        "  <button onclick='cmd(\"burst 200 8000\")'>burst 200x8000 (heavy)</button>\n"
                                        "  <button onclick='cmd(\"masked 4000\")'>masked 4000 (RFC violation)</button>\n"
                                        "</div>\n"
                                        "\n"
                                        "<div class=\"row\">\n"
                                        "  <b>Lifecycle</b><br>\n"
                                        "  <button onclick='cmd(\"close\")'>close me</button>\n"
                                        "  <button onclick='cmd(\"closeall\")'>closeAll</button>\n"
                                        "  <button onclick='cmd(\"drop 1\")'>drop id=1</button>\n"
                                        "</div>\n"
                                        "\n"
                                        "<div class=\"row\">\n"
                                        "  <b>Custom:</b>\n"
                                        "  <input id=\"custom\" style=\"width:240px\" placeholder=\"textall 1234\">\n"
                                        "  <button onclick='cmd(document.getElementById(\"custom\").value)'>send</button>\n"
                                        "</div>\n"
                                        "\n"
                                        "<pre id=\"log\"></pre>\n"
                                        "\n"
                                        "<script>\n"
                                        "const log = document.getElementById('log');\n"
                                        "function out(s){log.textContent += s + \"\\n\"; log.scrollTop = log.scrollHeight;}\n"
                                        "const ws = new WebSocket((location.protocol==='https:'?'wss:':'ws:') + '//' + location.host + '/ws');\n"
                                        "ws.onopen = () => {document.getElementById('status').textContent = 'WS: open'; out('connected');};\n"
                                        "ws.onclose = () => {document.getElementById('status').textContent = 'WS: closed'; out('disconnected');};\n"
                                        "ws.onerror = e => out('error: ' + JSON.stringify(e));\n"
                                        "ws.onmessage = e => {\n"
                                        "  if (e.data instanceof Blob) {\n"
                                        "    e.data.arrayBuffer().then(b => out('bin(' + b.byteLength + '): ' + Array.from(new "
                                        "Uint8Array(b)).slice(0,16).map(x=>x.toString(16).padStart(2,'0')).join(' ') + (b.byteLength>16?' ...':'')));\n"
                                        "  } else {\n"
                                        "    let s = e.data;\n"
                                        "    if (s.length > 80) s = s.slice(0,40) + '...[' + s.length + ']...' + s.slice(-40);\n"
                                        "    out('txt: ' + s);\n"
                                        "  }\n"
                                        "};\n"
                                        "function cmd(c){if(ws.readyState===WebSocket.OPEN){ws.send(c); out('> ' + c);} else {out('WS not open');}}\n"
                                        "</script>\n"
                                        "</body>\n"
                                        "</html>\n"
                                        "\n";

// JSON snapshot of live clients, for curl-based monitoring.
static String clientsJson() {
  String s = "{\"count\":";
  s += (unsigned)ws.count();
  s += ",\"clients\":[";
  bool first = true;
  for (auto &c : ws.getClients()) {
    if (!first) {
      s += ",";
    }
    first = false;
    s += "{\"id\":";
    s += (unsigned)c.id();
    s += ",\"status\":";
    s += (int)c.status();
    s += ",\"queueLen\":";
    s += (unsigned)c.queueLen();
    s += "}";
  }
  s += "]}";
  return s;
}

// ...........................................................................
// Setup
// ...........................................................................

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n================ WebSocketTest ================");

#if ASYNCWEBSERVER_WIFI_SUPPORTED
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID);
  Serial.printf("AP: %s\n", WIFI_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.printf("Open http://%s/  or  ws://%s/ws\n", WiFi.softAPIP().toString().c_str(), WiFi.softAPIP().toString().c_str());
#else
  Serial.println("WiFi not supported on this target; connect over an existing network interface.");
#endif

  // --- HTTP routes ---

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", HTML_PAGE);
  });

  // Live client list (handy for scripted tests / curl).
  server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", clientsJson());
  });

  // --- WebSocket ---

  ws.onEvent(onWsEvent);

  // Per-client keepAlive (auto-ping) and explicit ping/pingall commands both
  // exercise the control-frame queue and the pong-data forwarding added in
  // PR #462.  keepAlive is set on each new client at connect time inside the
  // event handler.

  // Gate the number of concurrent WS clients via middleware.  This exercises
  // the upgrade path being rejected mid-handshake (HTTP 503) -- a corner case
  // for the request lifecycle.
  server.addHandler(&ws).addMiddleware([](AsyncWebServerRequest *request, ArMiddlewareNext next) {
    // ws.count() does NOT include the in-flight upgrade request yet.
    if (ws.count() >= MAX_WS_CLIENTS) {
      request->send(503, "text/plain", "WS server full");
    } else {
      next();
    }
  });

  server.begin();
  Serial.println("HTTP + WS server started");
}

// ...........................................................................
// Loop: periodic background broadcast to stress the send path
// ...........................................................................

static uint32_t lastBg = 0;
static uint32_t lastReport = 0;

void loop() {
  const uint32_t now = millis();

  // Periodic broadcast: forces fragmentation and interleaved control frames.
  // Uses a fresh shared buffer each time.
  if (now - lastBg >= BG_BROADCAST_MS) {
    lastBg = now;
    if (ws.count() > 0) {
      AsyncWebSocketSharedBuffer buffer = makeShared(BG_BROADCAST_LEN, g_seq++);
      ws.textAll(buffer);
      // Inject a pingAll right after a big broadcast to stress the
      // "control frame interleaved with in-flight data frame" path --
      // the core scenario for PR #462.
      uint32_t t = micros();
      ws.pingAll((const uint8_t *)&t, sizeof(t));
    }
  }

  // Light periodic report on the serial console.
  if (now - lastReport >= 5000) {
    lastReport = now;
    Serial.printf("[loop] clients=%u seq=%" PRIu32 "\n", (unsigned)ws.count(), g_seq);
#ifdef ESP32
    Serial.printf("  freeHeap=%" PRIu32 " minFreeHeap=%" PRIu32 "\n", (uint32_t)ESP.getFreeHeap(), (uint32_t)ESP.getMinFreeHeap());
#endif
    // Enforce a soft cap on clients (exercises cleanupClients).
    ws.cleanupClients(MAX_WS_CLIENTS);
  }
}
