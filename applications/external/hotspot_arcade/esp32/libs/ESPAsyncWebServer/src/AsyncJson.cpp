#include "AsyncJson.h"

#if ASYNC_JSON_SUPPORT == 1

  #if ARDUINOJSON_VERSION_MAJOR == 5
AsyncJsonResponse::AsyncJsonResponse(bool isArray) : _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_json;
  if (isArray)
    _root = _jsonBuffer.createArray();
  else
    _root = _jsonBuffer.createObject();
}
  #elif ARDUINOJSON_VERSION_MAJOR == 6
AsyncJsonResponse::AsyncJsonResponse(bool isArray, size_t maxJsonBufferSize) : _jsonBuffer(maxJsonBufferSize), _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_json;
  if (isArray)
    _root = _jsonBuffer.createNestedArray();
  else
    _root = _jsonBuffer.createNestedObject();
}
  #else
AsyncJsonResponse::AsyncJsonResponse(bool isArray) : _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_json;
  if (isArray)
    _root = _jsonBuffer.add<JsonArray>();
  else
    _root = _jsonBuffer.add<JsonObject>();
}
  #endif

size_t AsyncJsonResponse::setLength() {
  #if ARDUINOJSON_VERSION_MAJOR == 5
  _contentLength = _root.measureLength();
  #else
  _contentLength = measureJson(_root);
  #endif
  if (_contentLength) {
    _isValid = true;
  }
  return _contentLength;
}

size_t AsyncJsonResponse::_fillBuffer(uint8_t* data, size_t len) {
  ChunkPrint dest(data, _sentLength, len);
  #if ARDUINOJSON_VERSION_MAJOR == 5
  _root.printTo(dest);
  #else
  serializeJson(_root, dest);
  #endif
  return len;
}

  #if ARDUINOJSON_VERSION_MAJOR == 6
PrettyAsyncJsonResponse::PrettyAsyncJsonResponse(bool isArray, size_t maxJsonBufferSize) : AsyncJsonResponse{isArray, maxJsonBufferSize} {}
  #else
PrettyAsyncJsonResponse::PrettyAsyncJsonResponse(bool isArray) : AsyncJsonResponse{isArray} {}
  #endif

size_t PrettyAsyncJsonResponse::setLength() {
  #if ARDUINOJSON_VERSION_MAJOR == 5
  _contentLength = _root.measurePrettyLength();
  #else
  _contentLength = measureJsonPretty(_root);
  #endif
  if (_contentLength) {
    _isValid = true;
  }
  return _contentLength;
}

size_t PrettyAsyncJsonResponse::_fillBuffer(uint8_t* data, size_t len) {
  ChunkPrint dest(data, _sentLength, len);
  #if ARDUINOJSON_VERSION_MAJOR == 5
  _root.prettyPrintTo(dest);
  #else
  serializeJsonPretty(_root, dest);
  #endif
  return len;
}

  #if ARDUINOJSON_VERSION_MAJOR == 6
AsyncCallbackJsonWebHandler::AsyncCallbackJsonWebHandler(const String& uri, ArJsonRequestHandlerFunction onRequest, size_t maxJsonBufferSize)
    : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), maxJsonBufferSize(maxJsonBufferSize), _maxContentLength(16384) {}
  #else
AsyncCallbackJsonWebHandler::AsyncCallbackJsonWebHandler(const String& uri, ArJsonRequestHandlerFunction onRequest)
    : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), _maxContentLength(16384) {}
  #endif

bool AsyncCallbackJsonWebHandler::canHandle(AsyncWebServerRequest* request) const {
  if (!_onRequest || !request->isHTTP() || !(_method & request->method()))
    return false;

  if (_uri.length() && (_uri != request->url() && !request->url().startsWith(_uri + "/")))
    return false;

  if (request->method() != HTTP_GET && !request->contentType().equalsIgnoreCase(asyncsrv::T_application_json))
    return false;

  return true;
}

void AsyncCallbackJsonWebHandler::handleRequest(AsyncWebServerRequest* request) {
  if (_onRequest) {
    if (request->method() == HTTP_GET) {
      JsonVariant json;
      _onRequest(request, json);
      return;
    } else if (request->_tempObject != NULL) {

  #if ARDUINOJSON_VERSION_MAJOR == 5
      DynamicJsonBuffer jsonBuffer;
      JsonVariant json = jsonBuffer.parse((uint8_t*)(request->_tempObject));
      if (json.success()) {
  #elif ARDUINOJSON_VERSION_MAJOR == 6
      DynamicJsonDocument jsonBuffer(this->maxJsonBufferSize);
      DeserializationError error = deserializeJson(jsonBuffer, (uint8_t*)(request->_tempObject));
      if (!error) {
        JsonVariant json = jsonBuffer.as<JsonVariant>();
  #else
      JsonDocument jsonBuffer;
      DeserializationError error = deserializeJson(jsonBuffer, (uint8_t*)(request->_tempObject));
      if (!error) {
        JsonVariant json = jsonBuffer.as<JsonVariant>();
  #endif

        _onRequest(request, json);
        return;
      }
    }
    request->send(_contentLength > _maxContentLength ? 413 : 400);
  } else {
    request->send(500);
  }
}

void AsyncCallbackJsonWebHandler::handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (_onRequest) {
    _contentLength = total;
    if (total > 0 && request->_tempObject == NULL && total < _maxContentLength) {
      request->_tempObject = malloc(total);
    }
    if (request->_tempObject != NULL) {
      memcpy((uint8_t*)(request->_tempObject) + index, data, len);
    }
  }
}

#endif // ASYNC_JSON_SUPPORT
