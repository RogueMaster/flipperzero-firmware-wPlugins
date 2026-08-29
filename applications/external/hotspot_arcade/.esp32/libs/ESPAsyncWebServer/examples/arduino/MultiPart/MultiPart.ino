// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright 2016-2026 Hristo Gochkov, Mathieu Carbou, Emil Muratov, Will Miles

/*
  Demo to test multi-part upload handling and boundary parsing in AsyncWebServer.

  Covers boundary-parsing cases introduced in the CWE-190/DoS hardening commit:
    - Token boundary (standard)
    - Quoted-string boundary (RFC 2046 §5.1)
    - Quoted-string with a quoted-pair escape sequence
    - Leading OWS (whitespace) after "boundary="
    - boundary= nested inside another quoted parameter value (must be ignored)
    - "x-boundary=" prefix must NOT be matched
    - Boundary longer than 70 chars → rejected (400)
    - Empty boundary → rejected (400)
    - Empty quoted-string boundary (boundary="") → rejected (400)
    - Unterminated quoted-string → rejected (400)

  ── /upload endpoint (all platforms) ──────────────────────────────────────────

  1. Standard token boundary (curl -F generates this automatically):

curl -v -F "field=hello" -F "file=@README.md" http://192.168.4.1/upload

  2. Quoted-string boundary:

curl -v \
  -H 'Content-Type: multipart/form-data; boundary="my-boundary"' \
  --data-binary $'--my-boundary\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n--my-boundary--\r\n' \
  http://192.168.4.1/upload

  3. Quoted-string with a quoted-pair escape (\" inside the boundary value):

curl -v \
  -H 'Content-Type: multipart/form-data; boundary="my-\"bnd\""' \
  --data-binary $'--my-"bnd"\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n--my-"bnd"--\r\n' \
  http://192.168.4.1/upload

  4. Leading whitespace after boundary= (non-RFC but tolerated):

curl -v \
  -H 'Content-Type: multipart/form-data; boundary=   simple' \
  --data-binary $'--simple\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n--simple--\r\n' \
  http://192.168.4.1/upload

  5. boundary= embedded in another quoted parameter value — must be ignored, real boundary is "real":

curl -v \
  -H 'Content-Type: multipart/form-data; foo="x; boundary=fake"; boundary=real' \
  --data-binary $'--real\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n--real--\r\n' \
  http://192.168.4.1/upload

  6. "x-boundary=" prefix must NOT match — request should be aborted:

curl -v \
  -H 'Content-Type: multipart/form-data; x-boundary=notreal' \
  --data-binary $'--notreal\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n--notreal--\r\n' \
  http://192.168.4.1/upload

  7. Boundary longer than 70 chars → abort:

curl -v \
  -H 'Content-Type: multipart/form-data; boundary=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX' \
  --data-binary $'--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX--\r\n' \
  http://192.168.4.1/upload

  8. Empty boundary → abort:

curl -v \
  -H 'Content-Type: multipart/form-data; boundary=' \
  --data-binary $'--\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n----\r\n' \
  http://192.168.4.1/upload

  9. Unterminated quoted-string → abort:

curl -v \
  -H 'Content-Type: multipart/form-data; boundary="unterminated' \
  --data-binary $'--unterminated\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n--unterminated--\r\n' \
  http://192.168.4.1/upload

  10. Empty quoted-string boundary → abort (must not be accepted as an empty
      delimiter that matches "--\r\n"):

curl -v \
  -H 'Content-Type: multipart/form-data; boundary=""' \
  --data-binary $'--\r\nContent-Disposition: form-data; name="field"\r\n\r\nhello\r\n----\r\n' \
  http://192.168.4.1/upload

  11. Boundary string appearing inside file data followed by a non-terminator
      byte (GHSA-8m8p-vhxc-jmjw, CWE-476 NULL-pointer dereference / DoS):

      Before the fix, the parser freed the upload buffer the moment it matched
      "--<boundary>" inside the file body, but left _itemIsFile = true.  When
      the next byte was neither the closing "--" nor a clean "\r\n", the rewind
      logic called itemWriteByte() which dereferenced the now-NULL _itemBuffer
      and crashed the device (StoreProhibited on ESP32 → reboot → DoS).

      The boundary must appear after a \r\n in the file body so the parser
      enters the boundary-matching state machine.  The byte after the match
      must be neither \r nor - to fall into the rewind branch:

curl -v \
  -H 'Content-Type: multipart/form-data; boundary=X' \
  --data-binary $'--X\r\nContent-Disposition: form-data; name="file"; filename="f.txt"\r\nContent-Type: text/plain\r\n\r\nhello\r\n--X\tjunk\r\n--X--\r\n' \
  http://192.168.4.1/upload

      After the fix the server must respond 200 and report the received
      parameter(s) without crashing.

  ── /flash endpoint (ESP32 only) ──────────────────────────────────────────────

  Flash firmware and filesystem in one request:
  1. Build firmware:  pio run -e arduino-3
  2. Build FS image:  pio run -e arduino-3 -t buildfs
  3. Flash both:

curl -v -F "name=Bob" -F "fw=@.pio/build/arduino-3/firmware.bin" -F "fs=@.pio/build/arduino-3/littlefs.bin" http://192.168.4.1/flash?name=Bill

*/

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
#include <StreamString.h>
#include <LittleFS.h>

// ESP32 example ONLY
#ifdef ESP32
#include <Update.h>
#endif

static AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }

#if ASYNCWEBSERVER_WIFI_SUPPORTED
  WiFi.mode(WIFI_AP);
  WiFi.softAP("esp-captive");
#endif

  // ── /upload — all platforms ───────────────────────────────────────────────
  //
  // Generic multipart endpoint used to exercise all boundary-parsing cases.
  // Responds 200 with a summary of every received parameter, or 400 if the
  // request is rejected by the parser (boundary too long, empty, malformed…).
  //
  server.on(
    "/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (request->getResponse()) {
        // A 400 was already sent by the upload handler — do not overwrite it.
        return;
      }

      StreamString body;
      const size_t params = request->params();
      body.printf("Received %u parameter(s):\n", params);
      for (size_t i = 0; i < params; i++) {
        const AsyncWebParameter *p = request->getParam(i);
        body.printf("[%u] %s=%s (post=%d file=%d size=%u)\n", i, p->name().c_str(), p->value().c_str(), p->isPost(), p->isFile(), p->size());
      }

      Serial.print(body);
      request->send(200, "text/plain", body);
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (request->getResponse()) {
        // Upload already aborted.
        return;
      }
      if (!index) {
        Serial.printf("Upload start: %s\n", filename.isEmpty() ? "(field)" : filename.c_str());
      }
      if (final) {
        Serial.printf("Upload end:   %s (%u bytes)\n", filename.isEmpty() ? "(field)" : filename.c_str(), index + len);
      }
    }
  );

// ── /flash — ESP32 only ───────────────────────────────────────────────────
#ifdef ESP32

  // Shows how to flash firmware and filesystem images from a multipart upload
  // and how to handle multiple files and parameters in a single request.
  server.on(
    "/flash", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (request->getResponse()) {
        // response already created
        return;
      }

      // list all parameters
      Serial.println("Request parameters:");
      const size_t params = request->params();
      for (size_t i = 0; i < params; i++) {
        const AsyncWebParameter *p = request->getParam(i);
        Serial.printf("Param[%u]: %s=%s, isPost=%d, isFile=%d, size=%u\n", i, p->name().c_str(), p->value().c_str(), p->isPost(), p->isFile(), p->size());
      }

      Serial.println("Flash / Filesystem upload completed");

      request->send(200, "text/plain", "Upload complete");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      Serial.printf("Upload[%s]: index=%u, len=%u, final=%d\n", filename.c_str(), index, len, final);

      if (request->getResponse() != nullptr) {
        // upload aborted
        return;
      }

      // start a new content-disposition upload
      if (!index) {
        // list all parameters
        const size_t params = request->params();
        for (size_t i = 0; i < params; i++) {
          const AsyncWebParameter *p = request->getParam(i);
          Serial.printf("Param[%u]: %s=%s, isPost=%d, isFile=%d, size=%u\n", i, p->name().c_str(), p->value().c_str(), p->isPost(), p->isFile(), p->size());
        }

        // get the content-disposition parameter
        const AsyncWebParameter *p = request->getParam(asyncsrv::T_name, true, true);
        if (p == nullptr) {
          request->send(400, "text/plain", "Missing content-disposition 'name' parameter");
          return;
        }

        // determine upload type based on the parameter name
        if (p->value() == "fs") {
          Serial.printf("Filesystem image upload for file: %s\n", filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            Update.printError(Serial);
            request->send(400, "text/plain", "Update begin failed");
            return;
          }

        } else if (p->value() == "fw") {
          Serial.printf("Firmware image upload for file: %s\n", filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
            request->send(400, "text/plain", "Update begin failed");
            return;
          }

        } else {
          Serial.printf("Unknown upload type for file: %s\n", filename.c_str());
          request->send(400, "text/plain", "Unknown upload type");
          return;
        }
      }

      // some bytes to write ?
      if (len) {
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
          Update.end();
          request->send(400, "text/plain", "Update write failed");
          return;
        }
      }

      // finish the content-disposition upload
      if (final) {
        if (!Update.end(true)) {
          Update.printError(Serial);
          request->send(400, "text/plain", "Update end failed");
          return;
        }

        // success response is created in the final request handler when all uploads are completed
        Serial.printf("Upload success of file %s\n", filename.c_str());
      }
    }
  );

#endif

  server.begin();
}

// not needed
void loop() {
  delay(100);
}
