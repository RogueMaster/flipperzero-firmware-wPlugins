#pragma once

/*
   server.on("/msg_pack", HTTP_ANY, [](AsyncWebServerRequest * request) {
    AsyncMessagePackResponse * response = new AsyncMessagePackResponse();
    JsonObject& root = response->getRoot();
    root["key1"] = "key number one";
    JsonObject& nested = root.createNestedObject("nested");
    nested["key1"] = "key number one";
    response->setLength();
    request->send(response);
  });

  --------------------

  AsyncCallbackMessagePackWebHandler* handler = new AsyncCallbackMessagePackWebHandler("/msg_pack/endpoint");
  handler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    // ...
  });
  server.addHandler(handler);
*/

#if __has_include("ArduinoJson.h")
  #include <ArduinoJson.h>
  #if ARDUINOJSON_VERSION_MAJOR >= 6
    #define ASYNC_MSG_PACK_SUPPORT 1
  #else
    #define ASYNC_MSG_PACK_SUPPORT 0
  #endif // ARDUINOJSON_VERSION_MAJOR >= 6
#endif   // __has_include("ArduinoJson.h")

#if ASYNC_MSG_PACK_SUPPORT == 1
  #include <ESPAsyncWebServer.h>

  #include "ChunkPrint.h"

  #if ARDUINOJSON_VERSION_MAJOR == 6
    #ifndef DYNAMIC_JSON_DOCUMENT_SIZE
      #define DYNAMIC_JSON_DOCUMENT_SIZE 1024
    #endif
  #endif

class AsyncMessagePackResponse : public AsyncAbstractResponse {
  protected:
  #if ARDUINOJSON_VERSION_MAJOR == 6
    DynamicJsonDocument _jsonBuffer;
  #else
    JsonDocument _jsonBuffer;
  #endif

    JsonVariant _root;
    bool _isValid;

  public:
  #if ARDUINOJSON_VERSION_MAJOR == 6
    AsyncMessagePackResponse(bool isArray = false, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
  #else
    AsyncMessagePackResponse(bool isArray = false);
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

typedef std::function<void(AsyncWebServerRequest* request, JsonVariant& json)> ArMessagePackRequestHandlerFunction;

class AsyncCallbackMessagePackWebHandler : public AsyncWebHandler {
  protected:
    String _uri;
    WebRequestMethodComposite _method;
    ArMessagePackRequestHandlerFunction _onRequest;
    size_t _contentLength;
  #if ARDUINOJSON_VERSION_MAJOR == 6
    size_t maxJsonBufferSize;
  #endif
    size_t _maxContentLength;

  public:
  #if ARDUINOJSON_VERSION_MAJOR == 6
    AsyncCallbackMessagePackWebHandler(const String& uri, ArMessagePackRequestHandlerFunction onRequest = nullptr, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
  #else
    AsyncCallbackMessagePackWebHandler(const String& uri, ArMessagePackRequestHandlerFunction onRequest = nullptr);
  #endif

    void setMethod(WebRequestMethodComposite method) { _method = method; }
    void setMaxContentLength(int maxContentLength) { _maxContentLength = maxContentLength; }
    void onRequest(ArMessagePackRequestHandlerFunction fn) { _onRequest = fn; }

    bool canHandle(AsyncWebServerRequest* request) const override final;
    void handleRequest(AsyncWebServerRequest* request) override final;
    void handleUpload(__unused AsyncWebServerRequest* request, __unused const String& filename, __unused size_t index, __unused uint8_t* data, __unused size_t len, __unused bool final) override final {}
    void handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) override final;
    bool isRequestHandlerTrivial() const override final { return !_onRequest; }
};

#endif // ASYNC_MSG_PACK_SUPPORT == 1
