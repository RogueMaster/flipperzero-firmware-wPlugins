#include "AsyncMessagePack.h"

#if ASYNC_MSG_PACK_SUPPORT == 1

  #if ARDUINOJSON_VERSION_MAJOR == 6
AsyncMessagePackResponse::AsyncMessagePackResponse(bool isArray, size_t maxJsonBufferSize) : _jsonBuffer(maxJsonBufferSize), _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_msgpack;
  if (isArray)
    _root = _jsonBuffer.createNestedArray();
  else
    _root = _jsonBuffer.createNestedObject();
}
  #else
AsyncMessagePackResponse::AsyncMessagePackResponse(bool isArray) : _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_msgpack;
  if (isArray)
    _root = _jsonBuffer.add<JsonArray>();
  else
    _root = _jsonBuffer.add<JsonObject>();
}
  #endif

size_t AsyncMessagePackResponse::setLength() {
  _contentLength = measureMsgPack(_root);
  if (_contentLength) {
    _isValid = true;
  }
  return _contentLength;
}

size_t AsyncMessagePackResponse::_fillBuffer(uint8_t* data, size_t len) {
  ChunkPrint dest(data, _sentLength, len);
  serializeMsgPack(_root, dest);
  return len;
}

  #if ARDUINOJSON_VERSION_MAJOR == 6
AsyncCallbackMessagePackWebHandler::AsyncCallbackMessagePackWebHandler(const String& uri, ArMessagePackRequestHandlerFunction onRequest, size_t maxJsonBufferSize)
    : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), maxJsonBufferSize(maxJsonBufferSize), _maxContentLength(16384) {}
  #else
AsyncCallbackMessagePackWebHandler::AsyncCallbackMessagePackWebHandler(const String& uri, ArMessagePackRequestHandlerFunction onRequest)
    : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), _maxContentLength(16384) {}
  #endif

bool AsyncCallbackMessagePackWebHandler::canHandle(AsyncWebServerRequest* request) const {
  if (!_onRequest || !request->isHTTP() || !(_method & request->method()))
    return false;

  if (_uri.length() && (_uri != request->url() && !request->url().startsWith(_uri + "/")))
    return false;

  if (request->method() != HTTP_GET && !request->contentType().equalsIgnoreCase(asyncsrv::T_application_msgpack))
    return false;

  return true;
}

void AsyncCallbackMessagePackWebHandler::handleRequest(AsyncWebServerRequest* request) {
  if (_onRequest) {
    if (request->method() == HTTP_GET) {
      JsonVariant json;
      _onRequest(request, json);
      return;
    } else if (request->_tempObject != NULL) {

  #if ARDUINOJSON_VERSION_MAJOR == 6
      DynamicJsonDocument jsonBuffer(this->maxJsonBufferSize);
      DeserializationError error = deserializeMsgPack(jsonBuffer, (uint8_t*)(request->_tempObject));
      if (!error) {
        JsonVariant json = jsonBuffer.as<JsonVariant>();
  #else
      JsonDocument jsonBuffer;
      DeserializationError error = deserializeMsgPack(jsonBuffer, (uint8_t*)(request->_tempObject));
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

void AsyncCallbackMessagePackWebHandler::handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
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

#endif // ASYNC_MSG_PACK_SUPPORT
