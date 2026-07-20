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
#include "ESPAsyncWebServer.h"
#include "WebHandlerImpl.h"

using namespace asyncsrv;

AsyncWebHandler& AsyncWebHandler::setFilter(ArRequestFilterFunction fn) {
  _filter = fn;
  return *this;
}
AsyncWebHandler& AsyncWebHandler::setAuthentication(const char* username, const char* password, AsyncAuthType authMethod) {
  if (!_authMiddleware) {
    _authMiddleware = new AsyncAuthenticationMiddleware();
    _authMiddleware->_freeOnRemoval = true;
    addMiddleware(_authMiddleware);
  }
  _authMiddleware->setUsername(username);
  _authMiddleware->setPassword(password);
  _authMiddleware->setAuthType(authMethod);
  return *this;
};

AsyncStaticWebHandler::AsyncStaticWebHandler(const char* uri, FS& fs, const char* path, const char* cache_control)
    : _fs(fs), _uri(uri), _path(path), _default_file(F("index.htm")), _cache_control(cache_control), _last_modified(), _callback(nullptr) {
  // Ensure leading '/'
  if (_uri.length() == 0 || _uri[0] != '/')
    _uri = String('/') + _uri;
  if (_path.length() == 0 || _path[0] != '/')
    _path = String('/') + _path;

  // If path ends with '/' we assume a hint that this is a directory to improve performance.
  // However - if it does not end with '/' we, can't assume a file, path can still be a directory.
  _isDir = _path[_path.length() - 1] == '/';

  // Remove the trailing '/' so we can handle default file
  // Notice that root will be "" not "/"
  if (_uri[_uri.length() - 1] == '/')
    _uri = _uri.substring(0, _uri.length() - 1);
  if (_path[_path.length() - 1] == '/')
    _path = _path.substring(0, _path.length() - 1);
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setTryGzipFirst(bool value) {
  _tryGzipFirst = value;
  return *this;
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setIsDir(bool isDir) {
  _isDir = isDir;
  return *this;
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setDefaultFile(const char* filename) {
  _default_file = filename;
  return *this;
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setCacheControl(const char* cache_control) {
  _cache_control = cache_control;
  return *this;
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setLastModified(const char* last_modified) {
  _last_modified = last_modified;
  return *this;
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setLastModified(struct tm* last_modified) {
  char result[30];
#ifdef ESP8266
  auto formatP = PSTR("%a, %d %b %Y %H:%M:%S GMT");
  char format[strlen_P(formatP) + 1];
  strcpy_P(format, formatP);
#else
  static constexpr const char* format = "%a, %d %b %Y %H:%M:%S GMT";
#endif

  strftime(result, sizeof(result), format, last_modified);
  _last_modified = result;
  return *this;
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setLastModified(time_t last_modified) {
  return setLastModified((struct tm*)gmtime(&last_modified));
}

AsyncStaticWebHandler& AsyncStaticWebHandler::setLastModified() {
  time_t last_modified;
  if (time(&last_modified) == 0) // time is not yet set
    return *this;
  return setLastModified(last_modified);
}

bool AsyncStaticWebHandler::canHandle(AsyncWebServerRequest* request) const {
  return request->isHTTP() && request->method() == HTTP_GET && request->url().startsWith(_uri) && _getFile(request);
}

bool AsyncStaticWebHandler::_getFile(AsyncWebServerRequest* request) const {
  // Remove the found uri
  String path = request->url().substring(_uri.length());

  // We can skip the file check and look for default if request is to the root of a directory or that request path ends with '/'
  bool canSkipFileCheck = (_isDir && path.length() == 0) || (path.length() && path[path.length() - 1] == '/');

  path = _path + path;

  // Do we have a file or .gz file
  if (!canSkipFileCheck && const_cast<AsyncStaticWebHandler*>(this)->_searchFile(request, path))
    return true;

  // Can't handle if not default file
  if (_default_file.length() == 0)
    return false;

  // Try to add default file, ensure there is a trailing '/' ot the path.
  if (path.length() == 0 || path[path.length() - 1] != '/')
    path += String('/');
  path += _default_file;

  return const_cast<AsyncStaticWebHandler*>(this)->_searchFile(request, path);
}

#ifdef ESP32
  #define FILE_IS_REAL(f) (f == true && !f.isDirectory())
#else
  #define FILE_IS_REAL(f) (f == true)
#endif

bool AsyncStaticWebHandler::_searchFile(AsyncWebServerRequest* request, const String& path) {
  bool fileFound = false;
  bool gzipFound = false;

  String gzip = path + T__gz;

  if (_tryGzipFirst) {
    if (_fs.exists(gzip)) {
      request->_tempFile = _fs.open(gzip, fs::FileOpenMode::read);
      gzipFound = FILE_IS_REAL(request->_tempFile);
    }
    if (!gzipFound) {
      if (_fs.exists(path)) {
        request->_tempFile = _fs.open(path, fs::FileOpenMode::read);
        fileFound = FILE_IS_REAL(request->_tempFile);
      }
    }
  } else {
    if (_fs.exists(path)) {
      request->_tempFile = _fs.open(path, fs::FileOpenMode::read);
      fileFound = FILE_IS_REAL(request->_tempFile);
    }
    if (!fileFound) {
      if (_fs.exists(gzip)) {
        request->_tempFile = _fs.open(gzip, fs::FileOpenMode::read);
        gzipFound = FILE_IS_REAL(request->_tempFile);
      }
    }
  }

  bool found = fileFound || gzipFound;

  if (found) {
    // Extract the file name from the path and keep it in _tempObject
    size_t pathLen = path.length();
    char* _tempPath = (char*)malloc(pathLen + 1);
    snprintf_P(_tempPath, pathLen + 1, PSTR("%s"), path.c_str());
    request->_tempObject = (void*)_tempPath;
  }

  return found;
}

uint8_t AsyncStaticWebHandler::_countBits(const uint8_t value) const {
  uint8_t w = value;
  uint8_t n;
  for (n = 0; w != 0; n++)
    w &= w - 1;
  return n;
}

void AsyncStaticWebHandler::handleRequest(AsyncWebServerRequest* request) {
  // Get the filename from request->_tempObject and free it
  String filename((char*)request->_tempObject);
  free(request->_tempObject);
  request->_tempObject = NULL;

  if (request->_tempFile != true){
    request->send(404);
    return;
  }

    time_t lw = request->_tempFile.getLastWrite(); // get last file mod time (if supported by FS)
    // set etag to lastmod timestamp if available, otherwise to size
    String etag;
    if (lw) {
      setLastModified(lw);
#if defined(TARGET_RP2040)
      // time_t == long long int
      constexpr size_t len = 1 + 8 * sizeof(time_t);
      char buf[len];
      char* ret = lltoa(lw ^ request->_tempFile.size(), buf, len, 10);
      etag = ret ? String(ret) : String(request->_tempFile.size());
#else
      etag = lw ^ request->_tempFile.size();   // etag combines file size and lastmod timestamp
#endif
    } else {
      etag = request->_tempFile.size();
    }

    bool not_modified = false;

    // if-none-match has precedence over if-modified-since
    if (request->hasHeader(T_INM))
      not_modified = request->header(T_INM).equals(etag);
    else if (_last_modified.length())
      not_modified = request->header(T_IMS).equals(_last_modified);

    AsyncWebServerResponse* response;

    if (not_modified){
      request->_tempFile.close();
      response = new AsyncBasicResponse(304); // Not modified
    } else {
      response = new AsyncFileResponse(request->_tempFile, filename, emptyString, false, _callback);
    }

    response->addHeader(T_ETag, etag.c_str());

    if (_last_modified.length())
      response->addHeader(T_Last_Modified, _last_modified.c_str());
    if (_cache_control.length())
      response->addHeader(T_Cache_Control, _cache_control.c_str());
  
    request->send(response);

}

AsyncStaticWebHandler& AsyncStaticWebHandler::setTemplateProcessor(AwsTemplateProcessor newCallback) {
  _callback = newCallback;
  return *this;
}

void AsyncCallbackWebHandler::setUri(const String& uri) {
  _uri = uri;
  _isRegex = uri.startsWith("^") && uri.endsWith("$");
}

bool AsyncCallbackWebHandler::canHandle(AsyncWebServerRequest* request) const {
  if (!_onRequest || !request->isHTTP() || !(_method & request->method()))
    return false;

#ifdef ASYNCWEBSERVER_REGEX
  if (_isRegex) {
    std::regex pattern(_uri.c_str());
    std::smatch matches;
    std::string s(request->url().c_str());
    if (std::regex_search(s, matches, pattern)) {
      for (size_t i = 1; i < matches.size(); ++i) { // start from 1
        request->_addPathParam(matches[i].str().c_str());
      }
    } else {
      return false;
    }
  } else
#endif
    if (_uri.length() && _uri.startsWith("/*.")) {
    String uriTemplate = String(_uri);
    uriTemplate = uriTemplate.substring(uriTemplate.lastIndexOf("."));
    if (!request->url().endsWith(uriTemplate))
      return false;
  } else if (_uri.length() && _uri.endsWith("*")) {
    String uriTemplate = String(_uri);
    uriTemplate = uriTemplate.substring(0, uriTemplate.length() - 1);
    if (!request->url().startsWith(uriTemplate))
      return false;
  } else if (_uri.length() && (_uri != request->url() && !request->url().startsWith(_uri + "/")))
    return false;

  return true;
}

void AsyncCallbackWebHandler::handleRequest(AsyncWebServerRequest* request) {
  if (_onRequest)
    _onRequest(request);
  else
    request->send(500);
}
void AsyncCallbackWebHandler::handleUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  if (_onUpload)
    _onUpload(request, filename, index, data, len, final);
}
void AsyncCallbackWebHandler::handleBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (_onBody)
    _onBody(request, data, len, index, total);
}