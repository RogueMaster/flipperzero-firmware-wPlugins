/*
  Asynchronous WebServer library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.

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
#include "Arduino.h"
#if defined(ESP32)
  #include <rom/ets_sys.h>
#endif
#include "AsyncEventSource.h"

#define ASYNC_SSE_NEW_LINE_CHAR (char)0xa

using namespace asyncsrv;

static String generateEventMessage(const char* message, const char* event, uint32_t id, uint32_t reconnect) {
  String str;
  size_t len{0};
  if (message)
    len += strlen(message);

  if (event)
    len += strlen(event);

  len += 42; // give it some overhead

  str.reserve(len);

  if (reconnect) {
    str += T_retry_;
    str += reconnect;
    str += ASYNC_SSE_NEW_LINE_CHAR; // '\n'
  }

  if (id) {
    str += T_id__;
    str += id;
    str += ASYNC_SSE_NEW_LINE_CHAR; // '\n'
  }

  if (event != NULL) {
    str += T_event_;
    str += event;
    str += ASYNC_SSE_NEW_LINE_CHAR; // '\n'
  }

  if (!message)
    return str;

  size_t messageLen = strlen(message);
  char* lineStart = (char*)message;
  char* lineEnd;
  do {
    char* nextN = strchr(lineStart, '\n');
    char* nextR = strchr(lineStart, '\r');
    if (nextN == NULL && nextR == NULL) {
      // a message is a single-line string
      str += T_data_;
      str += message;
      str += T_nn;
      return str;
    }

    // a message is a multi-line string
    char* nextLine = NULL;
    if (nextN != NULL && nextR != NULL) { // windows line-ending \r\n
      if (nextR + 1 == nextN) {
        // normal \r\n sequense
        lineEnd = nextR;
        nextLine = nextN + 1;
      } else {
        // some abnormal \n \r mixed sequence
        lineEnd = std::min(nextR, nextN);
        nextLine = lineEnd + 1;
      }
    } else if (nextN != NULL) { // Unix/Mac OS X LF
      lineEnd = nextN;
      nextLine = nextN + 1;
    } else { // some ancient garbage
      lineEnd = nextR;
      nextLine = nextR + 1;
    }

    str += T_data_;
    str.concat(lineStart, lineEnd - lineStart);
    str += ASYNC_SSE_NEW_LINE_CHAR; // \n

    lineStart = nextLine;
  } while (lineStart < ((char*)message + messageLen));

  // append another \n to terminate message
  str += ASYNC_SSE_NEW_LINE_CHAR; // '\n'

  return str;
}

// Message

size_t AsyncEventSourceMessage::ack(size_t len, __attribute__((unused)) uint32_t time) {
  // If the whole message is now acked...
  if (_acked + len > _data->length()) {
    // Return the number of extra bytes acked (they will be carried on to the next message)
    const size_t extra = _acked + len - _data->length();
    _acked = _data->length();
    return extra;
  }
  // Return that no extra bytes left.
  _acked += len;
  return 0;
}

size_t AsyncEventSourceMessage::write(AsyncClient* client) {
  if (!client)
    return 0;

  if (_sent >= _data->length() || !client->canSend()) {
    return 0;
  }

  size_t len = std::min(_data->length() - _sent, client->space());
  /*
    add() would call lwip's tcp_write() under the AsyncTCP hood with apiflags argument.
    By default apiflags=ASYNC_WRITE_FLAG_COPY
    we could have used apiflags with this flag unset to pass data by reference and avoid copy to socket buffer,
    but looks like it does not work for Arduino's lwip in ESP32/IDF
    it is enforced in https://github.com/espressif/esp-lwip/blob/0606eed9d8b98a797514fdf6eabb4daf1c8c8cd9/src/core/tcp_out.c#L422C5-L422C30
    if LWIP_NETIF_TX_SINGLE_PBUF is set, and it is set indeed in IDF
    https://github.com/espressif/esp-idf/blob/a0f798cfc4bbd624aab52b2c194d219e242d80c1/components/lwip/port/include/lwipopts.h#L744

    So let's just keep it enforced ASYNC_WRITE_FLAG_COPY and keep in mind that there is no zero-copy
  */
  size_t written = client->add(_data->c_str() + _sent, len, ASYNC_WRITE_FLAG_COPY); //  ASYNC_WRITE_FLAG_MORE
  _sent += written;
  return written;
}

size_t AsyncEventSourceMessage::send(AsyncClient* client) {
  size_t sent = write(client);
  return sent && client->send() ? sent : 0;
}

// Client

AsyncEventSourceClient::AsyncEventSourceClient(AsyncWebServerRequest* request, AsyncEventSource* server)
    : _client(request->client()), _server(server) {

  if (request->hasHeader(T_Last_Event_ID))
    _lastId = atoi(request->getHeader(T_Last_Event_ID)->value().c_str());

  _client->setRxTimeout(0);
  _client->onError(NULL, NULL);
  _client->onAck([](void* r, AsyncClient* c, size_t len, uint32_t time) { (void)c; static_cast<AsyncEventSourceClient*>(r)->_onAck(len, time); }, this);
  _client->onPoll([](void* r, AsyncClient* c) { (void)c; static_cast<AsyncEventSourceClient*>(r)->_onPoll(); }, this);
  _client->onData(NULL, NULL);
  _client->onTimeout([this](void* r, AsyncClient* c __attribute__((unused)), uint32_t time) { static_cast<AsyncEventSourceClient*>(r)->_onTimeout(time); }, this);
  _client->onDisconnect([this](void* r, AsyncClient* c) { static_cast<AsyncEventSourceClient*>(r)->_onDisconnect(); delete c; }, this);

  _server->_addClient(this);
  delete request;

  _client->setNoDelay(true);
}

AsyncEventSourceClient::~AsyncEventSourceClient() {
#ifdef ESP32
  std::lock_guard<std::mutex> lock(_lockmq);
#endif
  _messageQueue.clear();
  close();
}

bool AsyncEventSourceClient::_queueMessage(const char* message, size_t len) {
  if (_messageQueue.size() >= SSE_MAX_QUEUED_MESSAGES) {
#ifdef ESP8266
    ets_printf(String(F("ERROR: Too many messages queued\n")).c_str());
#elif defined(ESP32)
    log_e("Event message queue overflow: discard message");
#endif
    return false;
  }

#ifdef ESP32
  // length() is not thread-safe, thus acquiring the lock before this call..
  std::lock_guard<std::mutex> lock(_lockmq);
#endif

  _messageQueue.emplace_back(message, len);

  /*
    throttle queue run
    if Q is filled for >25% then network/CPU is congested, since there is no zero-copy mode for socket buff
    forcing Q run will only eat more heap ram and blow the buffer, let's just keep data in our own queue
    the queue will be processed at least on each onAck()/onPoll() call from AsyncTCP
  */
  if (_messageQueue.size() < SSE_MAX_QUEUED_MESSAGES >> 2 && _client->canSend()) {
    _runQueue();
  }

  return true;
}

bool AsyncEventSourceClient::_queueMessage(AsyncEvent_SharedData_t&& msg) {
  if (_messageQueue.size() >= SSE_MAX_QUEUED_MESSAGES) {
#ifdef ESP8266
    ets_printf(String(F("ERROR: Too many messages queued\n")).c_str());
#elif defined(ESP32)
    log_e("Event message queue overflow: discard message");
#endif
    return false;
  }

#ifdef ESP32
  // length() is not thread-safe, thus acquiring the lock before this call..
  std::lock_guard<std::mutex> lock(_lockmq);
#endif

  _messageQueue.emplace_back(std::move(msg));

  /*
    throttle queue run
    if Q is filled for >25% then network/CPU is congested, since there is no zero-copy mode for socket buff
    forcing Q run will only eat more heap ram and blow the buffer, let's just keep data in our own queue
    the queue will be processed at least on each onAck()/onPoll() call from AsyncTCP
  */
  if (_messageQueue.size() < SSE_MAX_QUEUED_MESSAGES >> 2 && _client->canSend()) {
    _runQueue();
  }
  return true;
}

void AsyncEventSourceClient::_onAck(size_t len __attribute__((unused)), uint32_t time __attribute__((unused))) {
#ifdef ESP32
  // Same here, acquiring the lock early
  std::lock_guard<std::mutex> lock(_lockmq);
#endif

  // adjust in-flight len
  if (len < _inflight)
    _inflight -= len;
  else
    _inflight = 0;

  // acknowledge as much messages's data as we got confirmed len from a AsyncTCP
  while (len && _messageQueue.size()) {
    len = _messageQueue.front().ack(len);
    if (_messageQueue.front().finished()) {
      // now we could release full ack'ed messages, we were keeping it unless send confirmed from AsyncTCP
      _messageQueue.pop_front();
    }
  }

  // try to send another batch of data
  if (_messageQueue.size())
    _runQueue();
}

void AsyncEventSourceClient::_onPoll() {
  if (_messageQueue.size()) {
#ifdef ESP32
    // Same here, acquiring the lock early
    std::lock_guard<std::mutex> lock(_lockmq);
#endif
    _runQueue();
  }
}

void AsyncEventSourceClient::_onTimeout(uint32_t time __attribute__((unused))) {
  if (_client)
    _client->close(true);
}

void AsyncEventSourceClient::_onDisconnect() {
  if (!_client)
    return;
  _client = nullptr;
  _server->_handleDisconnect(this);
}

void AsyncEventSourceClient::close() {
  if (_client)
    _client->close();
}

bool AsyncEventSourceClient::send(const char* message, const char* event, uint32_t id, uint32_t reconnect) {
  if (!connected())
    return false;
  return _queueMessage(std::make_shared<String>(generateEventMessage(message, event, id, reconnect)));
}

void AsyncEventSourceClient::_runQueue() {
  if (!_client)
    return;

  // there is no need to lock the mutex here, 'cause all the calls to this method must be already lock'ed
  size_t total_bytes_written = 0;
  for (auto i = _messageQueue.begin(); i != _messageQueue.end(); ++i) {
    if (!i->sent()) {
      const size_t bytes_written = i->write(_client);
      total_bytes_written += bytes_written;
      _inflight += bytes_written;
      if (bytes_written == 0 || _inflight > _max_inflight) {
        // Serial.print("_");
        break;
      }
    }
  }

  // flush socket
  if (total_bytes_written)
    _client->send();
}

void AsyncEventSourceClient::set_max_inflight_bytes(size_t value) {
  if (value >= SSE_MIN_INFLIGH && value <= SSE_MAX_INFLIGH)
    _max_inflight = value;
}

/*  AsyncEventSource  */

void AsyncEventSource::authorizeConnect(ArAuthorizeConnectHandler cb) {
  AsyncAuthorizationMiddleware* m = new AsyncAuthorizationMiddleware(401, cb);
  m->_freeOnRemoval = true;
  addMiddleware(m);
}

void AsyncEventSource::_addClient(AsyncEventSourceClient* client) {
  if (!client)
    return;
#ifdef ESP32
  std::lock_guard<std::mutex> lock(_client_queue_lock);
#endif
  _clients.emplace_back(client);
  if (_connectcb)
    _connectcb(client);

  _adjust_inflight_window();
}

void AsyncEventSource::_handleDisconnect(AsyncEventSourceClient* client) {
  if (_disconnectcb)
    _disconnectcb(client);
#ifdef ESP32
  std::lock_guard<std::mutex> lock(_client_queue_lock);
#endif
  for (auto i = _clients.begin(); i != _clients.end(); ++i) {
    if (i->get() == client)
      _clients.erase(i);
  }
  _adjust_inflight_window();
}

void AsyncEventSource::close() {
  // While the whole loop is not done, the linked list is locked and so the
  // iterator should remain valid even when AsyncEventSource::_handleDisconnect()
  // is called very early
#ifdef ESP32
  std::lock_guard<std::mutex> lock(_client_queue_lock);
#endif
  for (const auto& c : _clients) {
    if (c->connected())
      c->close();
  }
}

// pmb fix
size_t AsyncEventSource::avgPacketsWaiting() const {
  size_t aql = 0;
  uint32_t nConnectedClients = 0;
#ifdef ESP32
  std::lock_guard<std::mutex> lock(_client_queue_lock);
#endif
  if (!_clients.size())
    return 0;

  for (const auto& c : _clients) {
    if (c->connected()) {
      aql += c->packetsWaiting();
      ++nConnectedClients;
    }
  }
  return ((aql) + (nConnectedClients / 2)) / (nConnectedClients); // round up
}

AsyncEventSource::SendStatus AsyncEventSource::send(
  const char* message, const char* event, uint32_t id, uint32_t reconnect) {
  AsyncEvent_SharedData_t shared_msg = std::make_shared<String>(generateEventMessage(message, event, id, reconnect));
#ifdef ESP32
  std::lock_guard<std::mutex> lock(_client_queue_lock);
#endif
  size_t hits = 0;
  size_t miss = 0;
  for (const auto& c : _clients) {
    if (c->write(shared_msg))
      ++hits;
    else
      ++miss;
  }
  return hits == 0 ? DISCARDED : (miss == 0 ? ENQUEUED : PARTIALLY_ENQUEUED);
}

size_t AsyncEventSource::count() const {
#ifdef ESP32
  std::lock_guard<std::mutex> lock(_client_queue_lock);
#endif
  size_t n_clients{0};
  for (const auto& i : _clients)
    if (i->connected())
      ++n_clients;

  return n_clients;
}

bool AsyncEventSource::canHandle(AsyncWebServerRequest* request) const {
  return request->isSSE() && request->url().equals(_url);
}

void AsyncEventSource::handleRequest(AsyncWebServerRequest* request) {
  request->send(new AsyncEventSourceResponse(this));
}

void AsyncEventSource::_adjust_inflight_window() {
  if (_clients.size()) {
    size_t inflight = SSE_MAX_INFLIGH / _clients.size();
    for (const auto& c : _clients)
      c->set_max_inflight_bytes(inflight);
    // Serial.printf("adjusted inflight to: %u\n", inflight);
  }
}

/*  Response  */

AsyncEventSourceResponse::AsyncEventSourceResponse(AsyncEventSource* server) {
  _server = server;
  _code = 200;
  _contentType = T_text_event_stream;
  _sendContentLength = false;
  addHeader(T_Cache_Control, T_no_cache);
  addHeader(T_Connection, T_keep_alive);
}

void AsyncEventSourceResponse::_respond(AsyncWebServerRequest* request) {
  String out;
  _assembleHead(out, request->version());
  request->client()->write(out.c_str(), _headLength);
  _state = RESPONSE_WAIT_ACK;
}

size_t AsyncEventSourceResponse::_ack(AsyncWebServerRequest* request, size_t len, uint32_t time __attribute__((unused))) {
  if (len) {
    new AsyncEventSourceClient(request, _server);
  }
  return 0;
}
