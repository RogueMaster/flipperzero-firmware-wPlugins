# Requests

## Request Variables

### Common Variables

```cpp
request->version();       // uint8_t: 0 = HTTP/1.0, 1 = HTTP/1.1
request->method();        // enum:    HTTP_GET, HTTP_POST, HTTP_DELETE, HTTP_PUT, HTTP_PATCH, HTTP_HEAD, HTTP_OPTIONS
request->url();           // String:  URL of the request (not including host, port or GET parameters)
request->host();          // String:  The requested host (can be used for virtual hosting)
request->contentType();   // String:  ContentType of the request (not available in Handler::canHandle)
request->contentLength(); // size_t:  ContentLength of the request (not available in Handler::canHandle)
request->multipart();     // bool:    True if the request has content type "multipart"
```

### Headers

```cpp
//List all collected headers
int headers = request->headers();
int i;
for(i=0;i<headers;i++){
  const AsyncWebHeader* h = request->getHeader(i);
  Serial.printf("HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
}

//get specific header by name
if(request->hasHeader("MyHeader")){
  const AsyncWebHeader* h = request->getHeader("MyHeader");
  Serial.printf("MyHeader: %s\n", h->value().c_str());
}

//List all collected headers (Compatibility)
int headers = request->headers();
int i;
for(i=0;i<headers;i++){
  Serial.printf("HEADER[%s]: %s\n", request->headerName(i).c_str(), request->header(i).c_str());
}

//get specific header by name (Compatibility)
if(request->hasHeader("MyHeader")){
  Serial.printf("MyHeader: %s\n", request->header("MyHeader").c_str());
}
```

See the [Headers example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Headers/Headers.ino).

## Path Variable

With path variable you can create a custom regex rule for a specific parameter in a route.
For example we want a `sensorId` parameter in a route rule to match only a integer.

```cpp
  server.on("^\\/sensor\\/([0-9]+)$", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String sensorId = request->pathArg(0);
  });
```

_NOTE_: All regex patterns starts with `^` and ends with `$`

To enable the `Path variable` support, you have to define the buildflag `-DASYNCWEBSERVER_REGEX`.

For Arduino IDE create/update `platform.local.txt`:

`Windows`: C:\Users\(username)\AppData\Local\Arduino15\packages\\`{espxxxx}`\hardware\\`espxxxx`\\`{version}`\platform.local.txt

`Linux`: ~/.arduino15/packages/`{espxxxx}`/hardware/`{espxxxx}`/`{version}`/platform.local.txt

Add/Update the following line:

```
  compiler.cpp.extra_flags=-DASYNCWEBSERVER_REGEX
```

For platformio modify `platformio.ini`:

```ini
[env:myboard]
build_flags =
  -DASYNCWEBSERVER_REGEX
```

_NOTE_: By enabling `ASYNCWEBSERVER_REGEX`, `<regex>` will be included. This will add an 100k to your binary.

See the [URIMatcher example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/URIMatcher/URIMatcher.ino) and [URIMatcherTest example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/URIMatcherTest/URIMatcherTest.ino).

### GET, POST and FILE parameters

```cpp
//List all parameters
int params = request->params();
for(int i=0;i<params;i++){
  AsyncWebParameter* p = request->getParam(i);
  if(p->isFile()){ //p->isPost() is also true
    Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
  } else if(p->isPost()){
    Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
  } else {
    Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
  }
}

//Check if GET parameter exists
if(request->hasParam("download"))
  AsyncWebParameter* p = request->getParam("download");

//Check if POST (but not File) parameter exists
if(request->hasParam("download", true))
  AsyncWebParameter* p = request->getParam("download", true);

//Check if FILE was uploaded
if(request->hasParam("download", true, true))
  AsyncWebParameter* p = request->getParam("download", true, true);

//List all parameters (Compatibility)
int args = request->args();
for(int i=0;i<args;i++){
  Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
}

//Check if parameter exists (Compatibility)
if(request->hasArg("download"))
  String arg = request->arg("download");
```

See the [Params example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Params/Params.ino).

### FILE Upload handling

```cpp
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  if(!index){
    Serial.printf("UploadStart: %s\n", filename.c_str());
  }
  for(size_t i=0; i<len; i++){
    Serial.write(data[i]);
  }
  if(final){
    Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index+len);
  }
}
```

See the [Upload example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Upload/Upload.ino).

### Body data handling

```cpp
void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  if(!index){
    Serial.printf("BodyStart: %u B\n", total);
  }
  for(size_t i=0; i<len; i++){
    Serial.write(data[i]);
  }
  if(index + len == total){
    Serial.printf("BodyEnd: %u B\n", total);
  }
}
```

If needed, the `_tempObject` field on the request can be used to store a pointer to temporary data (e.g. from the body) associated with the request. If assigned, the pointer will automatically be freed along with the request.

### JSON body handling with ArduinoJson

Endpoints which consume JSON can use a special handler to get ready to use JSON data in the request callback:

```cpp
#include "AsyncJson.h"
#include "ArduinoJson.h"

AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/rest/endpoint", [](AsyncWebServerRequest *request, JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  // ...
});
server.addHandler(handler);
```

See the [Json example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Json/Json.ino).

### MessagePack body handling

Endpoints which consume MessagePack can use a special handler to get ready to use MessagePack data in the request callback:

```cpp
#include "AsyncMessagePack.h"
#include "ArduinoJson.h"

AsyncCallbackMessagePackWebHandler* handler = new AsyncCallbackMessagePackWebHandler("/rest/endpoint", [](AsyncWebServerRequest *request, JsonVariant &json) {
  JsonObject jsonObj = json.as<JsonObject>();
  // ...
});
server.addHandler(handler);
```

See the [MessagePack example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/MessagePack/MessagePack.ino).
