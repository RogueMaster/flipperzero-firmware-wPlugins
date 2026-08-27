# Responses

## Replace a Response

```c++
  // It is possible to replace a response.
  // The previous one will be deleted.
  // Response sending happens when the handler returns.
  server.on("/replace", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Hello, world");
    // oups! finally we want to send a different response
    request->send(400, "text/plain", "validation error");
  });
```

This will send error 400 instead of 200.

See the [example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Replace/Replace.ino).

## Request Continuation

Request Continuation is the ability to pause the processing of a request (the actual sending over the network) to be able to let another task commit the response on the network later.

This is a common supported use case amongst web servers.

A usage example can be found in the example called [RequestContinuation.ino](https://github.com/ESP32Async/ESPAsyncWebServer/blob/main/examples/RequestContinuation/RequestContinuation.ino)

In the handler receiving the request, just execute:

```c++
AsyncWebServerRequestPtr ptr = request->pause();
```

This will pause the request and return a `AsyncWebServerRequestPtr` (this is a weak pointer).

**The AsyncWebServerRequestPtr is the ONLY object authorized to leave the scope of the request handler.**

Save somewhere this smart pointer and use it later to commit the response like this:

```c++
// you can check for expiration
if (requestPtr.expired()) {
  // the request connection was closed some time ago so the request is not accessible anymore

} else if (longRunningTaskFinished) {
  // this is what you always need to do when you want to access the request.
  if (auto request = requestPtr.lock()) {
    // send back the response
    request->send(200, contentType, ...);

  } else {
    // the connection has been closed so the request is not accessible anymore
  }
}
```

Most of the time you can simply do like below if checking expiration is not needed:

```c++
if (auto request = requestPtr.lock()) {
  // send back the response
  request->send(200, contentType, ...);
}
```

See the [RequestContinuation example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/RequestContinuation/RequestContinuation.ino) and [RequestContinuationComplete example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/RequestContinuationComplete/RequestContinuationComplete.ino).

## Responses

### Redirect to another URL

```cpp
//to local url
request->redirect("/login");

//to external url
request->redirect("http://esp8266.com");
```

See the [Redirect example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Redirect/Redirect.ino).

### Basic response with HTTP Code

```cpp
request->send(404); //Sends 404 File Not Found
```

### Basic response with HTTP Code and extra headers

```cpp
AsyncWebServerResponse *response = request->beginResponse(404); //Sends 404 File Not Found
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Basic response with string content

```cpp
request->send(200, "text/plain", "Hello World!");
```

### Basic response with string content and extra headers

```cpp
AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Hello World!");
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Send large webpage from PROGMEM

```cpp
const char index_html[] PROGMEM = "..."; // large char array, tested with 14k
request->send_P(200, "text/html", index_html);
```

### Send large webpage from PROGMEM and extra headers

```cpp
const char index_html[] PROGMEM = "..."; // large char array, tested with 14k
AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Send large webpage from PROGMEM containing templates

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

const char index_html[] PROGMEM = "..."; // large char array, tested with 14k
request->send_P(200, "text/html", index_html, processor);
```

See the [Templates example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Templates/Templates.ino).

### Send large webpage from PROGMEM containing templates and extra headers

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

const char index_html[] PROGMEM = "..."; // large char array, tested with 14k
AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html, processor);
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Send binary content from PROGMEM

```cpp

//File: favicon.ico.gz, Size: 726
#define favicon_ico_gz_len 726
const uint8_t favicon_ico_gz[] PROGMEM = {
 0x1F, 0x8B, 0x08, 0x08, 0x0B, 0x87, 0x90, 0x57, 0x00, 0x03, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F,
 0x6E, 0x2E, 0x69, 0x63, 0x6F, 0x00, 0xCD, 0x53, 0x5F, 0x48, 0x9A, 0x51, 0x14, 0xBF, 0x62, 0x6D,
 0x86, 0x96, 0xA9, 0x64, 0xD3, 0xFE, 0xA8, 0x99, 0x65, 0x1A, 0xB4, 0x8A, 0xA8, 0x51, 0x54, 0x23,
 0xA8, 0x11, 0x49, 0x51, 0x8A, 0x34, 0x62, 0x93, 0x85, 0x31, 0x58, 0x44, 0x12, 0x45, 0x2D, 0x58,
 0xF5, 0x52, 0x41, 0x10, 0x23, 0x82, 0xA0, 0x20, 0x98, 0x2F, 0xC1, 0x26, 0xED, 0xA1, 0x20, 0x89,
 0x04, 0xD7, 0x83, 0x58, 0x20, 0x28, 0x04, 0xAB, 0xD1, 0x9B, 0x8C, 0xE5, 0xC3, 0x60, 0x32, 0x64,
 0x0E, 0x56, 0xBF, 0x9D, 0xEF, 0xF6, 0x30, 0x82, 0xED, 0xAD, 0x87, 0xDD, 0x8F, 0xF3, 0xDD, 0x8F,
 0x73, 0xCF, 0xEF, 0x9C, 0xDF, 0x39, 0xBF, 0xFB, 0x31, 0x26, 0xA2, 0x27, 0x37, 0x97, 0xD1, 0x5B,
 0xCF, 0x9E, 0x67, 0x30, 0xA6, 0x66, 0x8C, 0x99, 0xC9, 0xC8, 0x45, 0x9E, 0x6B, 0x3F, 0x5F, 0x74,
 0xA6, 0x94, 0x5E, 0xDB, 0xFF, 0xB2, 0xE6, 0xE7, 0xE7, 0xF9, 0xDE, 0xD6, 0xD6, 0x96, 0xDB, 0xD8,
 0xD8, 0x78, 0xBF, 0xA1, 0xA1, 0xC1, 0xDA, 0xDC, 0xDC, 0x2C, 0xEB, 0xED, 0xED, 0x15, 0x9B, 0xCD,
 0xE6, 0x4A, 0x83, 0xC1, 0xE0, 0x2E, 0x29, 0x29, 0x99, 0xD6, 0x6A, 0xB5, 0x4F, 0x75, 0x3A, 0x9D,
 0x61, 0x75, 0x75, 0x95, 0xB5, 0xB7, 0xB7, 0xDF, 0xC8, 0xD1, 0xD4, 0xD4, 0xF4, 0xB0, 0xBA, 0xBA,
 0xFA, 0x83, 0xD5, 0x6A, 0xFD, 0x5A, 0x5E, 0x5E, 0x9E, 0x28, 0x2D, 0x2D, 0x0D, 0x10, 0xC6, 0x4B,
 0x98, 0x78, 0x5E, 0x5E, 0xDE, 0x95, 0x42, 0xA1, 0x40, 0x4E, 0x4E, 0xCE, 0x65, 0x76, 0x76, 0xF6,
 0x47, 0xB5, 0x5A, 0x6D, 0x4F, 0x26, 0x93, 0xA2, 0xD6, 0xD6, 0x56, 0x8E, 0x6D, 0x69, 0x69, 0xD1,
 0x11, 0x36, 0x62, 0xB1, 0x58, 0x60, 0x32, 0x99, 0xA0, 0xD7, 0xEB, 0x51, 0x58, 0x58, 0x88, 0xFC,
 0xFC, 0x7C, 0x10, 0x16, 0x02, 0x56, 0x2E, 0x97, 0x43, 0x2A, 0x95, 0x42, 0x2C, 0x16, 0x23, 0x33,
 0x33, 0x33, 0xAE, 0x52, 0xA9, 0x1E, 0x64, 0x65, 0x65, 0x71, 0x7C, 0x7D, 0x7D, 0xBD, 0x93, 0xEA,
 0xFE, 0x30, 0x1A, 0x8D, 0xE8, 0xEC, 0xEC, 0xC4, 0xE2, 0xE2, 0x22, 0x6A, 0x6A, 0x6A, 0x40, 0x39,
 0x41, 0xB5, 0x38, 0x4E, 0xC8, 0x33, 0x3C, 0x3C, 0x0C, 0x87, 0xC3, 0xC1, 0x6B, 0x54, 0x54, 0x54,
 0xBC, 0xE9, 0xEB, 0xEB, 0x93, 0x5F, 0x5C, 0x5C, 0x30, 0x8A, 0x9D, 0x2E, 0x2B, 0x2B, 0xBB, 0xA2,
 0x3E, 0x41, 0xBD, 0x21, 0x1E, 0x8F, 0x63, 0x6A, 0x6A, 0x0A, 0x81, 0x40, 0x00, 0x94, 0x1B, 0x3D,
 0x3D, 0x3D, 0x42, 0x3C, 0x96, 0x96, 0x96, 0x70, 0x7E, 0x7E, 0x8E, 0xE3, 0xE3, 0x63, 0xF8, 0xFD,
 0xFE, 0xB4, 0xD7, 0xEB, 0xF5, 0x8F, 0x8F, 0x8F, 0x5B, 0x68, 0x5E, 0x6F, 0x05, 0xCE, 0xB4, 0xE3,
 0xE8, 0xE8, 0x08, 0x27, 0x27, 0x27, 0xD8, 0xDF, 0xDF, 0xC7, 0xD9, 0xD9, 0x19, 0x6C, 0x36, 0x1B,
 0x36, 0x36, 0x36, 0x38, 0x9F, 0x85, 0x85, 0x05, 0xAC, 0xAF, 0xAF, 0x23, 0x1A, 0x8D, 0x22, 0x91,
 0x48, 0x20, 0x16, 0x8B, 0xFD, 0xDA, 0xDA, 0xDA, 0x7A, 0x41, 0x33, 0x7E, 0x57, 0x50, 0x50, 0x80,
 0x89, 0x89, 0x09, 0x84, 0xC3, 0x61, 0x6C, 0x6F, 0x6F, 0x23, 0x12, 0x89, 0xE0, 0xE0, 0xE0, 0x00,
 0x43, 0x43, 0x43, 0x58, 0x5E, 0x5E, 0xE6, 0x9C, 0x7D, 0x3E, 0x1F, 0x46, 0x47, 0x47, 0x79, 0xBE,
 0xBD, 0xBD, 0x3D, 0xE1, 0x3C, 0x1D, 0x0C, 0x06, 0x9F, 0x10, 0xB7, 0xC7, 0x84, 0x4F, 0xF6, 0xF7,
 0xF7, 0x63, 0x60, 0x60, 0x00, 0x83, 0x83, 0x83, 0x18, 0x19, 0x19, 0xC1, 0xDC, 0xDC, 0x1C, 0x8F,
 0x17, 0x7C, 0xA4, 0x27, 0xE7, 0x34, 0x39, 0x39, 0x89, 0x9D, 0x9D, 0x1D, 0x6E, 0x54, 0xE3, 0x13,
 0xE5, 0x34, 0x11, 0x37, 0x49, 0x51, 0x51, 0xD1, 0x4B, 0xA5, 0x52, 0xF9, 0x45, 0x26, 0x93, 0x5D,
 0x0A, 0xF3, 0x92, 0x48, 0x24, 0xA0, 0x6F, 0x14, 0x17, 0x17, 0xA3, 0xB6, 0xB6, 0x16, 0x5D, 0x5D,
 0x5D, 0x7C, 0x1E, 0xBB, 0xBB, 0xBB, 0x9C, 0xD7, 0xE1, 0xE1, 0x21, 0x42, 0xA1, 0xD0, 0x6B, 0xD2,
 0x45, 0x4C, 0x33, 0x12, 0x34, 0xCC, 0xA0, 0x19, 0x54, 0x92, 0x56, 0x0E, 0xD2, 0xD9, 0x43, 0xF8,
 0xCF, 0x82, 0x56, 0xC2, 0xDC, 0xEB, 0xEA, 0xEA, 0x38, 0x7E, 0x6C, 0x6C, 0x4C, 0xE0, 0xFE, 0x9D,
 0xB8, 0xBF, 0xA7, 0xFA, 0xAF, 0x56, 0x56, 0x56, 0xEE, 0x6D, 0x6E, 0x6E, 0xDE, 0xB8, 0x47, 0x55,
 0x55, 0x55, 0x6C, 0x66, 0x66, 0x46, 0x44, 0xDA, 0x3B, 0x34, 0x1A, 0x4D, 0x94, 0xB0, 0x3F, 0x09,
 0x7B, 0x45, 0xBD, 0xA5, 0x5D, 0x2E, 0x57, 0x8C, 0x7A, 0x73, 0xD9, 0xED, 0xF6, 0x3B, 0x84, 0xFF,
 0xE7, 0x7D, 0xA6, 0x3A, 0x2C, 0x95, 0x4A, 0xB1, 0x8E, 0x8E, 0x0E, 0x6D, 0x77, 0x77, 0xB7, 0xCD,
 0xE9, 0x74, 0x3E, 0x73, 0xBB, 0xDD, 0x8F, 0x3C, 0x1E, 0x8F, 0xE6, 0xF4, 0xF4, 0x94, 0xAD, 0xAD,
 0xAD, 0xDD, 0xDE, 0xCF, 0x73, 0x0B, 0x0B, 0xB8, 0xB6, 0xE0, 0x5D, 0xC6, 0x66, 0xC5, 0xE4, 0x10,
 0x4C, 0xF4, 0xF7, 0xD8, 0x59, 0xF2, 0x7F, 0xA3, 0xB8, 0xB4, 0xFC, 0x0F, 0xEE, 0x37, 0x70, 0xEC,
 0x16, 0x4A, 0x7E, 0x04, 0x00, 0x00
};

AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon_ico_gz, favicon_ico_gz_len);
response->addHeader("Content-Encoding", "gzip");
request->send(response);
```

### Respond with content coming from a Stream

```cpp
//read 12 bytes from Serial and send them as Content Type text/plain
request->send(Serial, "text/plain", 12);
```

### Respond with content coming from a Stream and extra headers

```cpp
//read 12 bytes from Serial and send them as Content Type text/plain
AsyncWebServerResponse *response = request->beginResponse(Serial, "text/plain", 12);
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Respond with content coming from a Stream containing templates

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

//read 12 bytes from Serial and send them as Content Type text/plain
request->send(Serial, "text/plain", 12, processor);
```

### Respond with content coming from a Stream containing templates and extra headers

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

//read 12 bytes from Serial and send them as Content Type text/plain
AsyncWebServerResponse *response = request->beginResponse(Serial, "text/plain", 12, processor);
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Respond with content coming from a File

```cpp
//Send index.htm with default content type from SPIFFS
request->send(SPIFFS, "/index.htm");

//Send index.htm as text from SPIFFS
request->send(SPIFFS, "/index.htm", "text/plain");

//Download index.htm from SPIFFS
request->send(SPIFFS, "/index.htm", String(), true);

//Send index.htm with default content type from LittleFS
request->send(LittleFS, "/index.htm");

//Send index.htm as text from LittleFS
request->send(LittleFS, "/index.htm", "text/plain");

//Download index.htm from LittleFS
request->send(LittleFS, "/index.htm", String(), true);
```

See the [StaticFile example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/StaticFile/StaticFile.ino).

### Respond with content coming from a File and extra headers

```cpp
//Send index.htm with default content type from SPIFFS
AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.htm");

//Send index.htm as text from SPIFFS
AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.htm", "text/plain");

//Download index.htm from SPIFFS
AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.htm", String(), true);

//Send index.htm with default content type from LittleFS
AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.htm");

//Send index.htm as text from LittleFS
AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.htm", "text/plain");

//Download index.htm from LittleFS
AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.htm", String(), true);

response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Respond with content coming from a File containing templates

Internally uses [Chunked Response](#chunked-response).

Index.htm contents:

```
%HELLO_FROM_TEMPLATE%
```

Somewhere in source files:

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

//Send index.htm with template processor function from SPIFFS
request->send(SPIFFS, "/index.htm", String(), false, processor);

//Send index.htm with template processor function from LittleFS
request->send(LittleFS, "/index.htm", String(), false, processor);
```

### Respond with content using a callback

```cpp
//send 128 bytes as plain text
request->send("text/plain", 128, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //index equals the amount of bytes that have been already sent
  //You will not be asked for more bytes once the content length has been reached.
  //Keep in mind that you can not delay or yield waiting for more data!
  //Send what you currently have and you will be asked for more again
  return mySource.read(buffer, maxLen);
});
```

See the [ChunkResponse example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/ChunkResponse/ChunkResponse.ino).

### Respond with content using a callback and extra headers

```cpp
//send 128 bytes as plain text
AsyncWebServerResponse *response = request->beginResponse("text/plain", 128, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //index equals the amount of bytes that have been already sent
  //You will not be asked for more bytes once the content length has been reached.
  //Keep in mind that you can not delay or yield waiting for more data!
  //Send what you currently have and you will be asked for more again
  return mySource.read(buffer, maxLen);
});
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Respond with file content using a callback and extra headers

With this code your ESP is able to serve even large (large in terms of ESP, e.g. 100kB) files
without memory problems.

You need to create a file handler in outer function (to have a single one for request) but use
it in a lambda. The catch is that the lambda has it's own lifecycle which may/will cause it's
called after the original function is over thus the original file handle is destroyed. Using the
captured `&file` in the lambda then causes segfault (Hello, Exception 9!) and the whole ESP crashes.
By using this code, you tell the compiler to move the handle into the lambda so it won't be
destroyed when outer function (that one where you call `request->send(response)`) ends.

```cpp
// Example with SPIFFS
const File file = SPIFFS.open(path, "r");

const contentType = "application/javascript";

AsyncWebServerResponse *response = request->beginResponse(
  contentType,
  file.size(),
  [file](uint8_t *buffer, size_t maxLen, size_t total) mutable -> size_t {
     int bytes = file.read(buffer, maxLen);

     // close file at the end
     if (bytes + total == file.size()) file.close();

     return max(0, bytes); // return 0 even when no bytes were loaded
   }
);

if (gzipped) {
  response->addHeader(F("Content-Encoding"), F("gzip"));
}

request->send(response);
```

```cpp
// Example with LittleFS
const File file = LittleFS.open(path, "r");

const contentType = "application/javascript";

AsyncWebServerResponse *response = request->beginResponse(
  contentType,
  file.size(),
  [file](uint8_t *buffer, size_t maxLen, size_t total) mutable -> size_t {
     int bytes = file.read(buffer, maxLen);

     // close file at the end
     if (bytes + total == file.size()) file.close();

     return max(0, bytes); // return 0 even when no bytes were loaded
   }
);

if (gzipped) {
  response->addHeader(F("Content-Encoding"), F("gzip"));
}

request->send(response);
```

### Respond with content using a callback containing templates

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

//send 128 bytes as plain text
request->send("text/plain", 128, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //index equals the amount of bytes that have been already sent
  //You will not be asked for more bytes once the content length has been reached.
  //Keep in mind that you can not delay or yield waiting for more data!
  //Send what you currently have and you will be asked for more again
  return mySource.read(buffer, maxLen);
}, processor);
```

### Respond with content using a callback containing templates and extra headers

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

//send 128 bytes as plain text
AsyncWebServerResponse *response = request->beginResponse("text/plain", 128, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //index equals the amount of bytes that have been already sent
  //You will not be asked for more bytes once the content length has been reached.
  //Keep in mind that you can not delay or yield waiting for more data!
  //Send what you currently have and you will be asked for more again
  return mySource.read(buffer, maxLen);
}, processor);
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Chunked Response

Used when content length is unknown. Works best if the client supports HTTP/1.1

```cpp
AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //index equals the amount of bytes that have been already sent
  //You will be asked for more data until 0 is returned
  //Keep in mind that you can not delay or yield waiting for more data!
  return mySource.read(buffer, maxLen);
});
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

See the [ChunkRequest example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/ChunkRequest/ChunkRequest.ino) and [ChunkRetryResponse example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/ChunkRetryResponse/ChunkRetryResponse.ino).

### Chunked Response containing templates

Used when content length is unknown. Works best if the client supports HTTP/1.1

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //index equals the amount of bytes that have been already sent
  //You will be asked for more data until 0 is returned
  //Keep in mind that you can not delay or yield waiting for more data!
  return mySource.read(buffer, maxLen);
}, processor);
response->addHeader("Server","ESP Async Web Server");
request->send(response);
```

### Print to response

```cpp
AsyncResponseStream *response = request->beginResponseStream("text/html");
response->addHeader("Server","ESP Async Web Server");
response->printf("<!DOCTYPE html><html><head><title>Webpage at %s</title></head><body>", request->url().c_str());

response->print("<h2>Hello ");
response->print(request->client()->remoteIP());
response->print("</h2>");

response->print("<h3>General</h3>");
response->print("<ul>");
response->printf("<li>Version: HTTP/1.%u</li>", request->version());
response->printf("<li>Method: %s</li>", request->methodToString());
response->printf("<li>URL: %s</li>", request->url().c_str());
response->printf("<li>Host: %s</li>", request->host().c_str());
response->printf("<li>ContentType: %s</li>", request->contentType().c_str());
response->printf("<li>ContentLength: %u</li>", request->contentLength());
response->printf("<li>Multipart: %s</li>", request->multipart()?"true":"false");
response->print("</ul>");

response->print("<h3>Headers</h3>");
response->print("<ul>");
int headers = request->headers();
for(int i=0;i<headers;i++){
  AsyncWebHeader* h = request->getHeader(i);
  response->printf("<li>%s: %s</li>", h->name().c_str(), h->value().c_str());
}
response->print("</ul>");

response->print("<h3>Parameters</h3>");
response->print("<ul>");
int params = request->params();
for(int i=0;i<params;i++){
  AsyncWebParameter* p = request->getParam(i);
  if(p->isFile()){
    response->printf("<li>FILE[%s]: %s, size: %u</li>", p->name().c_str(), p->value().c_str(), p->size());
  } else if(p->isPost()){
    response->printf("<li>POST[%s]: %s</li>", p->name().c_str(), p->value().c_str());
  } else {
    response->printf("<li>GET[%s]: %s</li>", p->name().c_str(), p->value().c_str());
  }
}
response->print("</ul>");

response->print("</body></html>");
//send the response last
request->send(response);
```

See the [AsyncResponseStream example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/AsyncResponseStream/AsyncResponseStream.ino).

### ArduinoJson Basic Response

This way of sending Json is great for when the result is below 4KB

```cpp
#include "AsyncJson.h"
#include "ArduinoJson.h"

AsyncResponseStream *response = request->beginResponseStream("application/json");
JsonDocument doc;
doc["heap"] = ESP.getFreeHeap();
doc["ssid"] = WiFi.SSID();
serializeJson(doc, *response);
request->send(response);
```

### ArduinoJson Advanced Response

This response can handle really large Json objects (tested to 40KB)
There isn't any noticeable speed decrease for small results with the method above
Since ArduinoJson does not allow reading parts of the string, the whole Json has to
be passed every time a chunks needs to be sent, which shows speed decrease proportional
to the resulting json packets

```cpp
#include "AsyncJson.h"
#include "ArduinoJson.h"

AsyncJsonResponse * response = new AsyncJsonResponse();
response->addHeader("Server","ESP Async Web Server");
JsonDocument& doc = response->getRoot();
doc["heap"] = ESP.getFreeHeap();
doc["ssid"] = WiFi.SSID();
response->setLength();
request->send(response);
```

See the [Json example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Json/Json.ino).

### MessagePack Response

MessagePack is a binary serialization format that is more compact than JSON.

```cpp
#include "AsyncMessagePack.h"
#include "ArduinoJson.h"

AsyncMessagePackResponse * response = new AsyncMessagePackResponse();
response->addHeader("Server","ESP Async Web Server");
JsonDocument& doc = response->getRoot();
doc["heap"] = ESP.getFreeHeap();
doc["ssid"] = WiFi.SSID();
response->setLength();
request->send(response);
```

See the [MessagePack example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/MessagePack/MessagePack.ino).

## Adding Default Headers

In some cases, such as when working with CORS, or with some sort of custom authentication system,
you might need to define a header that should get added to all responses (including static, websocket and EventSource).
The DefaultHeaders singleton allows you to do this.

Example:

```cpp
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
webServer.begin();
```

_NOTE_: You will still need to respond to the OPTIONS method for CORS pre-flight in most cases. (unless you are only using GET)

This is one option:

```cpp
webServer.onNotFound([](AsyncWebServerRequest *request) {
  if (request->method() == HTTP_OPTIONS) {
    request->send(200);
  } else {
    request->send(404);
  }
});
```

See the [CORS example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/CORS/CORS.ino).

## Bad Responses

Some responses are implemented, but you should not use them, because they do not conform to HTTP.
The following example will lead to unclean close of the connection and more time wasted
than providing the length of the content

### Respond with content using a callback without content length to HTTP/1.0 clients

```cpp
//This is used as fallback for chunked responses to HTTP/1.0 Clients
request->send("text/plain", 0, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
  //Write up to "maxLen" bytes into "buffer" and return the amount written.
  //You will be asked for more data until 0 is returned
  //Keep in mind that you can not delay or yield waiting for more data!
  return mySource.read(buffer, maxLen);
});
```
