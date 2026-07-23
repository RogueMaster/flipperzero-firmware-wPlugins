/*
  Asynchronous WebServer library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef ASYNCWEBSERVERRESPONSEIMPL_H_
#define ASYNCWEBSERVERRESPONSEIMPL_H_

#ifdef Arduino_h
  // arduino is not compatible with std::vector
  #undef min
  #undef max
#endif
#include "literals.h"
#include <StreamString.h>
#include <memory>
#include <vector>

// It is possible to restore these defines, but one can use _min and _max instead. Or std::min, std::max.

class AsyncBasicResponse : public AsyncWebServerResponse {
  private:
    String _content;

  public:
    explicit AsyncBasicResponse(int code, const char* contentType = asyncsrv::empty, const char* content = asyncsrv::empty);
    AsyncBasicResponse(int code, const String& contentType, const String& content = emptyString) : AsyncBasicResponse(code, contentType.c_str(), content.c_str()) {}
    void _respond(AsyncWebServerRequest* request) override final;
    size_t _ack(AsyncWebServerRequest* request, size_t len, uint32_t time) override final;
    bool _sourceValid() const override final { return true; }
};

class AsyncAbstractResponse : public AsyncWebServerResponse {
  private:
    // amount of responce data in-flight, i.e. sent, but not acked yet
    size_t _in_flight{0};
    // in-flight queue credits
    size_t _in_flight_credit{2};
    String _head;
    // Data is inserted into cache at begin().
    // This is inefficient with vector, but if we use some other container,
    // we won't be able to access it as contiguous array of bytes when reading from it,
    // so by gaining performance in one place, we'll lose it in another.
    std::vector<uint8_t> _cache;
    size_t _readDataFromCacheOrContent(uint8_t* data, const size_t len);
    size_t _fillBufferAndProcessTemplates(uint8_t* buf, size_t maxLen);

  protected:
    AwsTemplateProcessor _callback;

  public:
    AsyncAbstractResponse(AwsTemplateProcessor callback = nullptr);
    virtual ~AsyncAbstractResponse() {}
    void _respond(AsyncWebServerRequest* request) override final;
    size_t _ack(AsyncWebServerRequest* request, size_t len, uint32_t time) override final;
    virtual bool _sourceValid() const { return false; }
    virtual size_t _fillBuffer(uint8_t* buf __attribute__((unused)), size_t maxLen __attribute__((unused))) { return 0; }
};

#ifndef TEMPLATE_PLACEHOLDER
  #define TEMPLATE_PLACEHOLDER '%'
#endif

#define TEMPLATE_PARAM_NAME_LENGTH 32
class AsyncFileResponse : public AsyncAbstractResponse {
    using File = fs::File;
    using FS = fs::FS;

  private:
    File _content;
    String _path;
    void _setContentTypeFromPath(const String& path);

  public:
    AsyncFileResponse(FS& fs, const String& path, const char* contentType = asyncsrv::empty, bool download = false, AwsTemplateProcessor callback = nullptr);
    AsyncFileResponse(FS& fs, const String& path, const String& contentType, bool download = false, AwsTemplateProcessor callback = nullptr) : AsyncFileResponse(fs, path, contentType.c_str(), download, callback) {}
    AsyncFileResponse(File content, const String& path, const char* contentType = asyncsrv::empty, bool download = false, AwsTemplateProcessor callback = nullptr);
    AsyncFileResponse(File content, const String& path, const String& contentType, bool download = false, AwsTemplateProcessor callack = nullptr) : AsyncFileResponse(content, path, contentType.c_str(), download, callack) {}
    ~AsyncFileResponse() { _content.close(); }
    bool _sourceValid() const override final { return !!(_content); }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override final;
};

class AsyncStreamResponse : public AsyncAbstractResponse {
  private:
    Stream* _content;

  public:
    AsyncStreamResponse(Stream& stream, const char* contentType, size_t len, AwsTemplateProcessor callback = nullptr);
    AsyncStreamResponse(Stream& stream, const String& contentType, size_t len, AwsTemplateProcessor callback = nullptr) : AsyncStreamResponse(stream, contentType.c_str(), len, callback) {}
    bool _sourceValid() const override final { return !!(_content); }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override final;
};

class AsyncCallbackResponse : public AsyncAbstractResponse {
  private:
    AwsResponseFiller _content;
    size_t _filledLength;

  public:
    AsyncCallbackResponse(const char* contentType, size_t len, AwsResponseFiller callback, AwsTemplateProcessor templateCallback = nullptr);
    AsyncCallbackResponse(const String& contentType, size_t len, AwsResponseFiller callback, AwsTemplateProcessor templateCallback = nullptr) : AsyncCallbackResponse(contentType.c_str(), len, callback, templateCallback) {}
    bool _sourceValid() const override final { return !!(_content); }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override final;
};

class AsyncChunkedResponse : public AsyncAbstractResponse {
  private:
    AwsResponseFiller _content;
    size_t _filledLength;

  public:
    AsyncChunkedResponse(const char* contentType, AwsResponseFiller callback, AwsTemplateProcessor templateCallback = nullptr);
    AsyncChunkedResponse(const String& contentType, AwsResponseFiller callback, AwsTemplateProcessor templateCallback = nullptr) : AsyncChunkedResponse(contentType.c_str(), callback, templateCallback) {}
    bool _sourceValid() const override final { return !!(_content); }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override final;
};

class AsyncProgmemResponse : public AsyncAbstractResponse {
  private:
    const uint8_t* _content;
    size_t _readLength;

  public:
    AsyncProgmemResponse(int code, const char* contentType, const uint8_t* content, size_t len, AwsTemplateProcessor callback = nullptr);
    AsyncProgmemResponse(int code, const String& contentType, const uint8_t* content, size_t len, AwsTemplateProcessor callback = nullptr) : AsyncProgmemResponse(code, contentType.c_str(), content, len, callback) {}
    bool _sourceValid() const override final { return true; }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override final;
};

class AsyncResponseStream : public AsyncAbstractResponse, public Print {
  private:
    StreamString _content;

  public:
    AsyncResponseStream(const char* contentType, size_t bufferSize);
    AsyncResponseStream(const String& contentType, size_t bufferSize) : AsyncResponseStream(contentType.c_str(), bufferSize) {}
    bool _sourceValid() const override final { return (_state < RESPONSE_END); }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override final;
    size_t write(const uint8_t* data, size_t len);
    size_t write(uint8_t data);
    using Print::write;
};

#endif /* ASYNCWEBSERVERRESPONSEIMPL_H_ */
