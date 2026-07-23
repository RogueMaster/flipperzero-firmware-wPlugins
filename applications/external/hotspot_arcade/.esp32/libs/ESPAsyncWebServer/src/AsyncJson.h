// AsyncJson.h
/*
  Async Response to use with ArduinoJson and AsyncWebServer
  Written by Andrew Melvin (SticilFace) with help from me-no-dev and BBlanchon.

  Example of callback in use

   server.on("/json", HTTP_ANY, [](AsyncWebServerRequest * request) {

    AsyncJsonResponse * response = new AsyncJsonResponse();
    JsonObject& root = response->getRoot();
    root["key1"] = "key number one";
    JsonObject& nested = root.createNestedObject("nested");
    nested["key1"] = "key number one";

    response->setLength();
    request->send(response);
  });

  --------------------

  Async Request to use with ArduinoJson and AsyncWebServer
  Written by Arsène von Wyss (avonwyss)

  Example

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/rest/endpoint");
  handler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    // ...
  });
  server.addHandler(handler);

*/
#ifndef ASYNC_JSON_H_
#define ASYNC_JSON_H_

#if __has_include("ArduinoJson.h")
  #include <ArduinoJson.h>
  #if ARDUINOJSON_VERSION_MAJOR >= 5
    #define ASYNC_JSON_SUPPORT 1
  #else
    #define ASYNC_JSON_SUPPORT 0
  #endif // ARDUINOJSON_VERSION_MAJOR >= 5
#endif   // __has_include("ArduinoJson.h")

#if ASYNC_JSON_SUPPORT == 1
  #include <ESPAsyncWebServer.h>

  #include "ChunkPrint.h"

  #if ARDUINOJSON_VERSION_MAJOR == 6
    #ifndef DYNAMIC_JSON_DOCUMENT_SIZE
      #define DYNAMIC_JSON_DOCUMENT_SIZE 1024
    #endif
  #endif

class AsyncJsonResponse : public AsyncAbstractResponse {
  protected:
  #if ARDUINOJSON_VERSION_MAJOR == 5
    DynamicJsonBuffer _jsonBuffer;
  #elif ARDUINOJSON_VERSION_MAJOR == 6
    DynamicJsonDocument _jsonBuffer;
  #else
    JsonDocument _jsonBuffer;
  #endif

    JsonVariant _root;
    bool _isValid;

  public:
  #if ARDUINOJSON_VERSION_MAJOR == 6
    AsyncJsonResponse(bool isArray = false, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
  #else
    AsyncJsonResponse(bool isArray = false);
  #endif
    JsonVariant& getRoot() { return _root; }
    bool _sourceValid() const { return _isValid; }
    size_t setLength();
    size_t getSize() const { return _jsonBuffer.size(); }
    size_t _fillBuffer(uint8_t* data, size_t len);
  #if ARDUINOJSON_VERSION_MAJOR >= 6
    bool overflowed() const { return _jsonBuffer.overflowed(); }
  #endif
};

class PrettyAsyncJsonResponse : public AsyncJsonResponse {
  public:
  #if ARDUINOJSON_VERSION_MAJOR == 6
    PrettyAsyncJsonResponse(bool isArray = false, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
  #else
    PrettyAsyncJsonResponse(bool isArray = false);
  #endif
    size_t setLength();
    size_t _fillBuffer(uint8_t* data, size_t len);
};

typedef std::function<void(AsyncWebServerRequest* request, JsonVariant& json)> ArJsonRequestHandlerFunction;

class AsyncCallbackJsonWebHandler : public AsyncWebHandler {
  protected:
    String _uri;
    WebRequestMethodComposite _method;
    ArJsonRequestHandlerFunction _onRequest;
    size_t _contentLength;
  #if ARDUINOJSON_VERSION_MAJOR == 6
    size_t maxJsonBufferSize;
  #endif
    size_t _maxContentLength;

  public:
  #if ARDUINOJSON_VERSION_MAJOR == 6
    AsyncCallbackJsonWebHandler(const String& uri, ArJsonRequestHandlerFunction onRequest = nullptr, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
  #else
    AsyncCallbackJsonWebHandler(const String& uri, ArJsonRequestHandlerFunction onRequest = nullptr);
  #endif

    void setMethod(WebRequestMethodComposite method) { _method = method; }
    void setMaxContentLength(int maxContentLength) { _maxContentLength = maxContentLength; }
    void onRequest(ArJsonRequestHandlerFunction fn) { _onRequest = fn; }

    bool canHandle(AsyncWebServerRequest* request) const override final;
    void handleRequest(AsyncWebServerRequest* request) override final;
    void handleUpload(__unused AsyncWebServerRequest* request, __unused const String& filename, __unused size_t index, __unused uint8_t* data, __unused size_t len, __unused bool final) override final {}
    void handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) override final;
    bool isRequestHandlerTrivial() const override final { return !_onRequest; }
};

#endif // ASYNC_JSON_SUPPORT == 1

#endif // ASYNC_JSON_H_
