// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright 2016-2026 Hristo Gochkov, Mathieu Carbou, Emil Muratov, Will Miles

#include "AsyncWebSocket.h"
#include "AsyncWebServerLogging.h"

#include <libb64/cencode.h>

#if defined(ESP32)
#if ESP_IDF_VERSION_MAJOR < 5
#include "BackPort_SHA1Builder.h"
#else
#include <SHA1Builder.h>
#endif
#include <rom/ets_sys.h>
#elif defined(TARGET_RP2040) || defined(TARGET_RP2350) || defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ESP8266)
#include <Hash.h>
#elif defined(LIBRETINY)
#include <mbedtls/sha1.h>
#elif defined(HOST)
#include "BackPort_SHA1Builder.h"
#ifndef FPSTR
#define FPSTR (const char *)
#endif
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <cstdarg>

#define STATE_FRAME_START 0
#define STATE_FRAME_MASK  1
#define STATE_FRAME_DATA  2

using namespace asyncsrv;

static AsyncWebSocketSharedBuffer makeSharedBuffer(const uint8_t *message, size_t len) {
  if (message) {
    return std::make_shared<std::vector<uint8_t>>(message, message + len);
  } else {
    return std::make_shared<std::vector<uint8_t>>(len);
  }
}

static size_t webSocketSendFrameWindow(AsyncClient *client) {
  if (!client || !client->canSend()) {
    return 0;
  }
  size_t space = client->space();
  if (space < 9) {
    return 0;
  }
  return space - 8;
}

// wire header length for a frame with this payload length/masking -- pure
// function of RFC 6455 framing rules, cheap enough to recompute on demand
// rather than cache
static uint8_t webSocketFrameHeaderLen(size_t payloadLen, bool mask) {
  return 2 + (mask ? 4 : 0) + ((payloadLen > 125) ? 2 : 0);
}

// Stack-allocated chunk size for masking payload data
static constexpr size_t WS_MASK_CHUNK_SIZE = 128;

// Commits any not-yet-committed bytes of one WS frame (header+payload) to
// the client, resuming from `frameSent` bytes already committed by a prior
// call for this exact frame. final/opcode/mask/maskKey/data/len must stay
// identical across calls for the same frame -- once any header byte is
// committed those values are fixed on the wire and can't change. Returns
// the number of *additional* bytes committed by this call (0 if none);
// never assumes an add() fully succeeds, since some AsyncClient backends
// (e.g. SSL) can commit a genuine partial amount.
static size_t
  webSocketAddFrame(AsyncClient *client, bool final, uint8_t opcode, bool mask, const uint8_t maskKey[4], const uint8_t *data, size_t len, size_t frameSent) {
  if (!client || !client->canSend()) {
    return 0;
  }

  size_t committed = 0;

  const uint8_t headLen = webSocketFrameHeaderLen(len, mask);
  if (frameSent < headLen) {
    uint8_t hdr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    hdr[0] = (opcode & 0x0F) | (final ? 0x80 : 0);
    if (len < 126) {
      hdr[1] = len & 0x7F;
    } else {
      hdr[1] = 126;
      hdr[2] = (uint8_t)((len >> 8) & 0xFF);
      hdr[3] = (uint8_t)(len & 0xFF);
    }
    if (mask) {
      hdr[1] |= 0x80;
      memcpy(hdr + (headLen - 4), maskKey, 4);
    }

    size_t added = client->add((const char *)(hdr + frameSent), headLen - frameSent, ASYNC_WRITE_FLAG_COPY | ((len > 0) ? ASYNC_WRITE_FLAG_MORE : 0));
    committed += added;
    frameSent += added;
    if (frameSent < headLen) {
      return committed;
    }
  }

  size_t payloadSent = frameSent - headLen;
  if (payloadSent < len) {
    size_t remaining = len - payloadSent;
    if (mask) {
      // The data buffer is shared, but each client masks with its own random key.
      // Mask in to a scratch buffer one chunk at a time to avoid malloc overhead.
      uint8_t masked_data[WS_MASK_CHUNK_SIZE];
      while (remaining > 0) {
        const size_t chunk = std::min(remaining, sizeof(masked_data));
        for (size_t i = 0; i < chunk; i++) {
          masked_data[i] = data[payloadSent + i] ^ maskKey[(payloadSent + i) % 4];
        }
        const size_t added = client->add((const char *)masked_data, chunk, ASYNC_WRITE_FLAG_COPY | ((chunk == remaining) ? 0 : ASYNC_WRITE_FLAG_MORE));
        committed += added;
        if (added < chunk) {
          break;  // out of space (or partial commit) -- resume here next time
        }
        payloadSent += added;
        remaining -= added;
      }
    } else {
      committed += client->add((const char *)(data + payloadSent), remaining, ASYNC_WRITE_FLAG_COPY);
    }
  }

  return committed;
}

/*
 *    AsyncWebSocketMessageBuffer
 */

AsyncWebSocketMessageBuffer::AsyncWebSocketMessageBuffer(const uint8_t *data, size_t size) : _buffer(std::make_shared<std::vector<uint8_t>>(size)) {
  if (_buffer->capacity() < size) {
    _buffer->reserve(size);
  } else {
    std::memcpy(_buffer->data(), data, size);
  }
}

AsyncWebSocketMessageBuffer::AsyncWebSocketMessageBuffer(size_t size) : _buffer(std::make_shared<std::vector<uint8_t>>(size)) {
  if (_buffer->capacity() < size) {
    _buffer->reserve(size);
  }
}

bool AsyncWebSocketMessageBuffer::reserve(size_t size) {
  if (_buffer->capacity() >= size) {
    return true;
  }
  _buffer->reserve(size);
  return _buffer->capacity() >= size;
}

/*
 * Async WebSocket Client
 */
const char *AWSC_PING_PAYLOAD = "ESPAsyncWebServer-PING";
const size_t AWSC_PING_PAYLOAD_LEN = 22;

AsyncWebSocketClient::AsyncWebSocketClient(AsyncClient *client, AsyncWebSocket *server)
  : _client(client), _server(server), _clientId(_server->_getNextId()), _status(WS_CONNECTED), _pstate(STATE_FRAME_START), _lastMessageTime(millis()),
    _keepAlivePeriod(0), _tempObject(NULL) {

  _client->setRxTimeout(0);
  _client->onError(
    [](void *r, AsyncClient *c, int8_t error) {
      (void)c;
      ((AsyncWebSocketClient *)(r))->_onError(error);
    },
    this
  );
  _client->onAck(
    [](void *r, AsyncClient *c, size_t len, uint32_t time) {
      (void)c;
      ((AsyncWebSocketClient *)(r))->_onAck(len, time);
    },
    this
  );
  _client->onDisconnect(
    [](void *r, AsyncClient *c) {
      ((AsyncWebSocketClient *)(r))->_onDisconnect();
      delete c;
    },
    this
  );
  _client->onTimeout(
    [](void *r, AsyncClient *c, uint32_t time) {
      (void)c;
      ((AsyncWebSocketClient *)(r))->_onTimeout(time);
    },
    this
  );
  _client->onData(
    [](void *r, AsyncClient *c, void *buf, size_t len) {
      (void)c;
      ((AsyncWebSocketClient *)(r))->_onData(buf, len);
    },
    this
  );
  _client->onPoll(
    [](void *r, AsyncClient *c) {
      (void)c;
      ((AsyncWebSocketClient *)(r))->_onPoll();
    },
    this
  );
  memset(&_pinfo, 0, sizeof(_pinfo));
}

AsyncWebSocketClient::~AsyncWebSocketClient() {
  {
    asyncsrv::lock_guard_type lock(_queue_lock);
    _messageQueue.clear();
    _controlQueue.clear();
  }
  _server->_handleEvent(this, WS_EVT_DISCONNECT, NULL, NULL, 0);
}

void AsyncWebSocketClient::_onAck(size_t len, uint32_t time) {
  (void)time;
  _lastMessageTime = millis();

  asyncsrv::unique_lock_type lock(_queue_lock);

  async_ws_log_v("[%s][%" PRIu32 "] ACK(%u)", _server->url(), _clientId, len);

  // Messages are dequeued as soon as they're fully add()'ed, not on ack --
  // an ack only ever matters here as a sign that space() may have grown.
  _runQueue(lock);
}

void AsyncWebSocketClient::_onPoll() {
  asyncsrv::unique_lock_type lock(_queue_lock);

  if (!_client) {
    return;
  }

  if (_client && _client->canSend() && (!_controlQueue.empty() || !_messageQueue.empty())) {
    _runQueue(lock);
  } else if (_keepAlivePeriod > 0 && (millis() - _lastMessageTime) >= _keepAlivePeriod && (_controlQueue.empty() && _messageQueue.empty())) {
    lock.unlock();
    ping((uint8_t *)AWSC_PING_PAYLOAD, AWSC_PING_PAYLOAD_LEN);
  }
}

void AsyncWebSocketClient::_runQueue(asyncsrv::unique_lock_type &lock) {
  // all calls to this method MUST be protected by a mutex lock, passed in as `lock`
  if (!_client) {
    return;
  }

  bool needs_send = false;
  while (true) {
    if (_frameSent == 0) {
      // idle: pick the next target by priority (control first)
      if (!_controlQueue.empty()) {
        _sendingControl = true;
      } else if (!_messageQueue.empty()) {
        _sendingControl = false;
      } else {
        break;  // nothing queued
      }

      AsyncWebSocketMessage &target = _sendingControl ? _controlQueue.front() : _messageQueue.front();
      if (_sendingControl) {
        _framePayloadLen = target.size();
      } else {
        const size_t window = webSocketSendFrameWindow(_client);
        if (!window) {
          break;  // no space to send right now
        }
        const size_t remaining = target.size() - _sent;
        _framePayloadLen = std::min(remaining, window);
      }
      if (target.mask()) {
        _maskKey[0] = rand() % 0xFF;  // NOLINT(runtime/threadsafe_fn)
        _maskKey[1] = rand() % 0xFF;  // NOLINT(runtime/threadsafe_fn)
        _maskKey[2] = rand() % 0xFF;  // NOLINT(runtime/threadsafe_fn)
        _maskKey[3] = rand() % 0xFF;  // NOLINT(runtime/threadsafe_fn)
      }
    }

    AsyncWebSocketMessage &target = _sendingControl ? _controlQueue.front() : _messageQueue.front();
    const bool final = _sendingControl || (_sent + _framePayloadLen == target.size());
    const uint8_t frameOpcode = (_sendingControl || _sent == 0) ? target.opcode() : (uint8_t)WS_CONTINUATION;
    uint8_t *payload = target.data();

    // Avoid UB pointer arithmetic if payload is nullptr
    if (payload && !_sendingControl) {
      payload += _sent;
    }

    const size_t added = webSocketAddFrame(_client, final, frameOpcode, target.mask(), _maskKey, payload, _framePayloadLen, _frameSent);

    async_ws_log_v(
      "[%s][%" PRIu32 "][%" PRIu8 "] SEND ctrl:%d %u/%u added %u", _server->url(), _clientId, frameOpcode, _sendingControl, _frameSent,
      webSocketFrameHeaderLen(_framePayloadLen, target.mask()) + _framePayloadLen, added
    );

    _frameSent += added;
    needs_send |= (added > 0);

    const size_t frameLen = webSocketFrameHeaderLen(_framePayloadLen, target.mask()) + _framePayloadLen;
    if (_frameSent < frameLen) {
      break;  // stalled; resume on the next trigger
    }

    // frame fully committed to TCP -- done with it, regardless of ack
    _frameSent = 0;
    const size_t framePayloadLen = _framePayloadLen;
    _framePayloadLen = 0;

    if (_sendingControl) {
      const uint8_t opcode = target.opcode();
      _controlQueue.pop_front();
      if (opcode == WS_DISCONNECT && _status == WS_DISCONNECTING) {
        _status = WS_DISCONNECTED;
        async_ws_log_v("[%s][%" PRIu32 "] DISCONNECT SENT", _server->url(), _clientId);
        // Capture _client before unlocking: close() may synchronously run
        // _onDisconnect() -> AsyncWebSocket::_handleDisconnect() -> list::erase(),
        // destroying *this -- nothing after unlock() may touch any member.
        // close() itself (not abort()) hands the pcb to lwIP's graceful
        // close, which flushes/retransmits already-add()'ed data (this
        // close frame included) independent of our lifetime from here on.
        AsyncClient *c = _client;
        lock.unlock();
        c->send();  // Needed on some backends (e.g. SSL) to flush the close frame out before closing
        c->close();
        return;
      }
    } else {
      _sent += framePayloadLen;
      if (_sent >= target.size()) {
        _messageQueue.pop_front();
        _sent = 0;
      }
    }
  }

  if (needs_send) {
    _client->send();
  }
}

bool AsyncWebSocketClient::queueIsFull() const {
  asyncsrv::lock_guard_type lock(_queue_lock);
  return (_messageQueue.size() >= WS_MAX_QUEUED_MESSAGES) || (_status != WS_CONNECTED);
}

size_t AsyncWebSocketClient::queueLen() const {
  asyncsrv::lock_guard_type lock(_queue_lock);
  return _messageQueue.size();
}

bool AsyncWebSocketClient::canSend() const {
  asyncsrv::lock_guard_type lock(_queue_lock);
  return _messageQueue.size() < WS_MAX_QUEUED_MESSAGES;
}

bool AsyncWebSocketClient::_queueControl(uint8_t opcode, const uint8_t *data, size_t len, bool mask) {
  asyncsrv::unique_lock_type lock(_queue_lock);

  if (!_client) {
    return false;
  }

  if (!data) {
    len = 0;
  } else if (len > 125) {
    len = 125;
  }
  AsyncWebSocketSharedBuffer buffer = len ? makeSharedBuffer(data, len) : AsyncWebSocketSharedBuffer{};
  _controlQueue.emplace_back(buffer, opcode, len && mask);
  async_ws_log_v("[%s][%" PRIu32 "] QUEUE CTRL (%u) << %" PRIu8, _server->url(), _clientId, _controlQueue.size(), opcode);

  if (_client && _client->canSend()) {
    _runQueue(lock);
  }

  return true;
}

bool AsyncWebSocketClient::_queueMessage(AsyncWebSocketSharedBuffer buffer, uint8_t opcode, bool mask) {
  asyncsrv::unique_lock_type lock(_queue_lock);

  if (!_client || !buffer || buffer->empty() || _status != WS_CONNECTED) {
    return false;
  }

  if (_messageQueue.size() >= WS_MAX_QUEUED_MESSAGES) {
    if (_closeWhenFull) {
      _status = WS_DISCONNECTED;

      async_ws_log_w("[%s][%" PRIu32 "] Too many messages queued: closing connection", _server->url(), _clientId);

      // Capture _client before unlocking: _client->close() triggers the _onDisconnect() --> _handleDisconnect() --> ~AsyncWebSocketClient() chain,
      // so we must not access any member after unlock.
      AsyncClient *c = _client;
      if (c) {
        lock.unlock();
        c->close();
      }

    } else {
      async_ws_log_w("[%s][%" PRIu32 "] Too many messages queued: discarding new message", _server->url(), _clientId);
    }

    return false;
  }

  _messageQueue.emplace_back(buffer, opcode, mask);
  async_ws_log_v("[%s][%" PRIu32 "] QUEUE MSG (%u/%u) << %" PRIu8, _server->url(), _clientId, _messageQueue.size(), WS_MAX_QUEUED_MESSAGES, opcode);

  if (_client && _client->canSend()) {
    _runQueue(lock);
  }

  return true;
}

void AsyncWebSocketClient::close(uint16_t code, const char *message) {
  if (_status != WS_CONNECTED) {
    return;
  }

  async_ws_log_w("[%s][%" PRIu32 "] CLOSE", _server->url(), _clientId);

  _status = WS_DISCONNECTING;

  if (code) {
    uint8_t packetLen = 2;
    if (message != NULL) {
      size_t mlen = strlen(message);
      if (mlen > 123) {
        mlen = 123;
      }
      packetLen += mlen;
    }
    char *buf = (char *)malloc(packetLen);
    if (buf != NULL) {
      buf[0] = (uint8_t)(code >> 8);
      buf[1] = (uint8_t)(code & 0xFF);
      if (message != NULL) {
        memcpy(buf + 2, message, packetLen - 2);
      }
      _queueControl(WS_DISCONNECT, (uint8_t *)buf, packetLen);
      free(buf);
      return;
    } else {
      async_ws_log_e("Failed to allocate");
      // Reads _client, then dereference it without any lock.
      // A concurrent _onDisconnect could null + delete the client between the check and the use.
      // Local capture ensures the pointer is read exactly once, eliminating the null-dereference.
      // (TOCTOU)
      AsyncClient *c = _client;
      if (c) {
        c->abort();
      }
      return;
    }
  }
  _queueControl(WS_DISCONNECT);
}

bool AsyncWebSocketClient::ping(const uint8_t *data, size_t len) {
  return _status == WS_CONNECTED && _queueControl(WS_PING, data, len);
}

void AsyncWebSocketClient::_onError(int8_t err) {
  async_ws_log_v("[%s][%" PRIu32 "] ERROR %" PRIi8, _server->url(), _clientId, static_cast<int8_t>(err));
}

void AsyncWebSocketClient::_onTimeout(uint32_t time) {
  // Reads _client, then dereference it without any lock.
  // A concurrent _onDisconnect could null + delete the client between the check and the use.
  // Local capture ensures the pointer is read exactly once, eliminating the null-dereference.
  // (TOCTOU)
  AsyncClient *c = _client;
  if (!c) {
    return;
  }
  async_ws_log_v("[%s][%" PRIu32 "] TIMEOUT %" PRIu32, _server->url(), _clientId, time);
  c->close();
}

void AsyncWebSocketClient::_onDisconnect() {
  async_ws_log_v("[%s][%" PRIu32 "] DISCONNECT", _server->url(), _clientId);
  _status = WS_DISCONNECTED;
  {
    // Every queue method (_queueControl, _queueMessage, _runQueue, _onPoll, _onAck) reads _client while holding _queue_lock.
    // For those guarded reads to be meaningful, the write must also be synchronized. This doesn't change _queue_lock's purpose — it still guards queue integrity — but ensures the "is client alive?" checks that protect queue operations see a consistent value.
    asyncsrv::lock_guard_type lock(_queue_lock);
    _client = nullptr;
  }
  _server->_handleDisconnect(this);
}

void AsyncWebSocketClient::_onData(void *pbuf, size_t plen) {
  _lastMessageTime = millis();
  uint8_t *data = (uint8_t *)pbuf;

  while (plen > 0) {
    async_ws_log_v(
      "[%s][%" PRIu32 "] DATA plen: %" PRIu32 ", _pstate: %" PRIu8 ", _status: %" PRIu8, _server->url(), _clientId, static_cast<uint32_t>(plen), _pstate,
      static_cast<uint8_t>(_status)
    );

    if (_pstate == STATE_FRAME_START) {
      const uint8_t *fdata = data;

      _pinfo.index = 0;
      _pinfo.final = (fdata[0] & 0x80) != 0;
      _pinfo.opcode = fdata[0] & 0x0F;
      _pinfo.masked = ((fdata[1] & 0x80) != 0) ? 1 : 0;
      _pinfo.len = fdata[1] & 0x7F;

      data += 2;
      plen -= 2;

      if (_pinfo.len == 126 && plen >= 2) {
        _pinfo.len = fdata[3] | (uint16_t)(fdata[2]) << 8;
        data += 2;
        plen -= 2;

      } else if (_pinfo.len == 127 && plen >= 8) {
        _pinfo.len = fdata[9] | (uint16_t)(fdata[8]) << 8 | (uint32_t)(fdata[7]) << 16 | (uint32_t)(fdata[6]) << 24 | (uint64_t)(fdata[5]) << 32
                     | (uint64_t)(fdata[4]) << 40 | (uint64_t)(fdata[3]) << 48 | (uint64_t)(fdata[2]) << 56;
        data += 8;
        plen -= 8;
      }
    }

    async_ws_log_v(
      "[%s][%" PRIu32 "] DATA _pinfo: index: %" PRIu64 ", final: %" PRIu8 ", opcode: %" PRIu8 ", masked: %" PRIu8 ", len: %" PRIu64, _server->url(), _clientId,
      _pinfo.index, _pinfo.final, _pinfo.opcode, _pinfo.masked, _pinfo.len
    );

    // Handle fragmented mask data - Safari may split the 4-byte mask across multiple packets
    // _pinfo.masked is 1 if we need to start reading mask bytes
    // _pinfo.masked is 2, 3, or 4 if we have partially read the mask
    // _pinfo.masked is 5 if the mask is complete
    while (_pinfo.masked && _pstate <= STATE_FRAME_MASK && _pinfo.masked < 5) {
      // check if we have some data
      if (plen == 0) {
        // Safari close frame edge case: masked bit set but no mask data
        if (_pinfo.opcode == WS_DISCONNECT) {
          async_ws_log_v("[%s][%" PRIu32 "] DATA close frame with incomplete mask, treating as unmasked", _server->url(), _clientId);
          _pinfo.masked = 0;
          _pinfo.index = 0;
          _pinfo.len = 0;
          _pstate = STATE_FRAME_START;
          break;
        }

        // wait for more data
        _pstate = STATE_FRAME_MASK;
        async_ws_log_v("[%s][%" PRIu32 "] DATA waiting for more mask data: read: %" PRIu8 "/4", _server->url(), _clientId, _pinfo.masked - 1);
        return;
      }

      // accumulate mask bytes
      _pinfo.mask[_pinfo.masked - 1] = data[0];
      data += 1;
      plen -= 1;
      _pinfo.masked++;
    }

    // all mask bytes read if we were reading them
    _pstate = STATE_FRAME_DATA;

    // restore masked to 1 for backward compatibility
    if (_pinfo.masked >= 5) {
      async_ws_log_v("[%s][%" PRIu32 "] DATA mask read complete", _server->url(), _clientId);
      _pinfo.masked = 1;
    }

    const size_t datalen = std::min((size_t)(_pinfo.len - _pinfo.index), plen);

    if (_pinfo.masked) {
      for (size_t i = 0; i < datalen; i++) {
        data[i] ^= _pinfo.mask[(_pinfo.index + i) % 4];
      }
    }

    if (_pinfo.index == 0) {  // first fragment of the frame
      // init message_opcode for this frame
      // note: For next WS_CONTINUATION frames, they have opcode 0, so message_opcode will stay like the first frame
      if (_pinfo.opcode == WS_TEXT || _pinfo.opcode == WS_BINARY) {
        _pinfo.message_opcode = _pinfo.opcode;
      }
      // init frame number to 0 if only 1 frame or if this is the first frame of a fragmented message
      if (_pinfo.final || datalen < _pinfo.len) {
        _pinfo.num = 0;
      }
    }

    if ((datalen + _pinfo.index) < _pinfo.len) {  // more fragments to read for this frame
      _pstate = STATE_FRAME_DATA;

      if (datalen > 0) {
        async_ws_log_v(
          "[%s][%" PRIu32 "] DATA processing next fragment of %s frame %" PRIu32 ", index: %" PRIu64 ", len: %" PRIu32 "", _server->url(), _clientId,
          (_pinfo.message_opcode == WS_TEXT) ? "text" : "binary", _pinfo.num, _pinfo.index, (uint32_t)datalen
        );
        if (!_handleDataEvent(data, datalen, datalen == plen)) {  // datalen == plen means that we are processing the last part of the current TCP packet
          return;                                                 // stop processing on failure
        }
      }

      // track index for next fragment
      _pinfo.index += datalen;

    } else if ((datalen + _pinfo.index) == _pinfo.len) {  // this is the last fragment for this frame
      _pstate = STATE_FRAME_START;

      if (_pinfo.opcode == WS_DISCONNECT) {
        async_ws_log_v("[%s][%" PRIu32 "] DATA WS_DISCONNECT", _server->url(), _clientId);

        if (datalen) {
          uint16_t reasonCode = (uint16_t)(data[0] << 8) + data[1];
          char *reasonString = (char *)(data + 2);
          if (reasonCode > 1001) {
            _server->_handleEvent(this, WS_EVT_ERROR, (void *)&reasonCode, (uint8_t *)reasonString, strlen(reasonString));
          }
        }
        if (_status == WS_DISCONNECTING) {
          _status = WS_DISCONNECTED;
          if (_client) {
            _client->close();
          }
          return;  // our object is now destroyed, so we must return immediately to avoid accessing any member
        } else {
          _status = WS_DISCONNECTING;
          if (_client) {
            _client->ackLater();
          }
          _queueControl(WS_DISCONNECT, data, datalen);
        }

      } else if (_pinfo.opcode == WS_PING) {
        async_ws_log_v("[%s][%" PRIu32 "] DATA PING", _server->url(), _clientId);
        _server->_handleEvent(this, WS_EVT_PING, NULL, NULL, 0);
        _queueControl(WS_PONG, data, datalen);

      } else if (_pinfo.opcode == WS_PONG) {
        async_ws_log_v("[%s][%" PRIu32 "] DATA PONG", _server->url(), _clientId);
        if (datalen != AWSC_PING_PAYLOAD_LEN || memcmp(AWSC_PING_PAYLOAD, data, AWSC_PING_PAYLOAD_LEN) != 0) {
          _server->_handleEvent(this, WS_EVT_PONG, NULL, data, datalen);
        }

      } else if (_pinfo.opcode < WS_DISCONNECT) {  // continuation or text/binary frame
        async_ws_log_v(
          "[%s][%" PRIu32 "] DATA processing final fragment of %s frame %" PRIu32 ", index: %" PRIu64 ", len: %" PRIu32 "", _server->url(), _clientId,
          (_pinfo.message_opcode == WS_TEXT) ? "text" : "binary", _pinfo.num, _pinfo.index, (uint32_t)datalen
        );

        if (!_handleDataEvent(data, datalen, datalen == plen)) {  // datalen == plen means that we are processing the last part of the current TCP packet
          return;                                                 // stop processing on failure
        }

        if (_pinfo.final) {
          _pinfo.num = 0;
        } else {
          _pinfo.num += 1;
        }
      }

    } else {
      // unexpected frame error, close connection
      _pstate = STATE_FRAME_START;

      async_ws_log_v(
        "[%s][%" PRIu32 "] DATA frame error: len: %u, index: %" PRIu64 ", total: %" PRIu64 "\n", _server->url(), _clientId, datalen, _pinfo.index, _pinfo.len
      );

      _status = WS_DISCONNECTING;
      if (_client) {
        _client->ackLater();
      }
      _queueControl(WS_DISCONNECT, data, datalen);
      break;
    }

    data += datalen;
    plen -= datalen;
  }
}

bool AsyncWebSocketClient::_handleDataEvent(uint8_t *data, size_t len, bool endOfPaquet) {
  // ------------------------------------------------------------
  // Issue 384: https://github.com/ESP32Async/ESPAsyncWebServer/issues/384
  // Discussion: https://github.com/ESP32Async/ESPAsyncWebServer/pull/383#discussion_r2760425739
  // The initial design of the library was doing a backup of the byte following the data buffer because the client code
  // was allowed and documented to do something like data[len] = 0; to facilitate null-terminated string handling.
  // This was a bit hacky but it was working and it was documented, although completely incorrect because it was modifying a byte outside of the data buffer.
  // So to fix this behavior and to avoid breaking existing client code that may be relying on this behavior, we now have to copy the data to a temporary buffer that has an extra byte for the null terminator.
  // ------------------------------------------------------------
  //
  // Optimization notes:
  //
  // 1) opcodes
  //
  // - info->opcode stores the current WS frame type (binary, text, continuation)
  // - info->message_opcode stores the WS frame type of the first frame of the message, which is used for fragmented messages to know the message type when processing subsequent frame with opcode 0 (continuation)
  // So we can use info->message_opcode to avoid copying the data for non-text frames, and only copy the data for text frames when we need to add a null terminator for client code convenience.
  //
  // 2) data copy vs data backup/restore
  // - endOfPaquet: is true when datalen == plen. plen is the remaining bytes in the current TCP packet, so if datalen == plen, it means that we are processing the last part of the current TCP packet.
  // In that case, we have to copy since we cannot backup/restore the byte after the data buffer.
  // Otherwise we can backup the byte and restore since we know that the byte after is owned by the current TCP packet (same pointer).
  if (_pinfo.message_opcode == WS_TEXT) {
    if (endOfPaquet) {
      std::unique_ptr<uint8_t[]> copy(new (std::nothrow) uint8_t[len + 1]());
      if (copy) {
        memcpy(copy.get(), data, len);
        copy[len] = 0;
        _server->_handleEvent(this, WS_EVT_DATA, (void *)&_pinfo, copy.get(), len);
      } else {
        async_ws_log_e("Failed to allocate");
        AsyncClient *c = _client;
        if (c) {
          c->abort();
        }
        return false;  // failure!
      }
    } else {
      uint8_t backup = data[len];
      data[len] = 0;
      _server->_handleEvent(this, WS_EVT_DATA, (void *)&_pinfo, data, len);
      data[len] = backup;
    }
  } else {
    _server->_handleEvent(this, WS_EVT_DATA, (void *)&_pinfo, data, len);
  }
  return true;
}

size_t AsyncWebSocketClient::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  size_t len = vsnprintf(nullptr, 0, format, arg);
  va_end(arg);

  if (len == 0) {
    return 0;
  }

  char *buffer = new char[len + 1];

  if (!buffer) {
    return 0;
  }

  va_start(arg, format);
  len = vsnprintf(buffer, len + 1, format, arg);
  va_end(arg);

  bool enqueued = text(buffer, len);
  delete[] buffer;
  return enqueued ? len : 0;
}

#ifdef ESP8266
size_t AsyncWebSocketClient::printf_P(PGM_P formatP, ...) {
  va_list arg;
  va_start(arg, formatP);
  size_t len = vsnprintf_P(nullptr, 0, formatP, arg);
  va_end(arg);

  if (len == 0) {
    return 0;
  }

  char *buffer = new char[len + 1];

  if (!buffer) {
    return 0;
  }

  va_start(arg, formatP);
  len = vsnprintf_P(buffer, len + 1, formatP, arg);
  va_end(arg);

  bool enqueued = text(buffer, len);
  delete[] buffer;
  return enqueued ? len : 0;
}
#endif

bool AsyncWebSocketClient::text(AsyncWebSocketMessageBuffer *buffer) {
  bool enqueued = false;
  if (buffer) {
    enqueued = text(std::move(buffer->_buffer));
    delete buffer;
  }
  return enqueued;
}

bool AsyncWebSocketClient::text(AsyncWebSocketSharedBuffer buffer) {
  return _queueMessage(buffer);
}

bool AsyncWebSocketClient::text(const uint8_t *message, size_t len) {
  return text(makeSharedBuffer(message, len));
}

bool AsyncWebSocketClient::text(const char *message, size_t len) {
  return text((const uint8_t *)message, len);
}

bool AsyncWebSocketClient::text(const char *message) {
  return text(message, strlen(message));
}

bool AsyncWebSocketClient::text(const String &message) {
  return text(message.c_str(), message.length());
}

#ifdef ESP8266
bool AsyncWebSocketClient::text(const __FlashStringHelper *data) {
  PGM_P p = reinterpret_cast<PGM_P>(data);

  size_t n = 0;
  while (1) {
    if (pgm_read_byte(p + n) == 0) {
      break;
    }
    n += 1;
  }

  char *message = (char *)malloc(n + 1);
  bool enqueued = false;
  if (message) {
    memcpy_P(message, p, n);
    message[n] = 0;
    enqueued = text(message, n);
    free(message);
  }
  return enqueued;
}
#endif  // ESP8266

bool AsyncWebSocketClient::binary(AsyncWebSocketMessageBuffer *buffer) {
  bool enqueued = false;
  if (buffer) {
    enqueued = binary(std::move(buffer->_buffer));
    delete buffer;
  }
  return enqueued;
}

bool AsyncWebSocketClient::binary(AsyncWebSocketSharedBuffer buffer) {
  return _queueMessage(buffer, WS_BINARY);
}

bool AsyncWebSocketClient::binary(const uint8_t *message, size_t len) {
  return binary(makeSharedBuffer(message, len));
}

bool AsyncWebSocketClient::binary(const char *message, size_t len) {
  return binary((const uint8_t *)message, len);
}

bool AsyncWebSocketClient::binary(const char *message) {
  return binary(message, strlen(message));
}

bool AsyncWebSocketClient::binary(const String &message) {
  return binary(message.c_str(), message.length());
}

#ifdef ESP8266
bool AsyncWebSocketClient::binary(const __FlashStringHelper *data, size_t len) {
  PGM_P p = reinterpret_cast<PGM_P>(data);
  char *message = (char *)malloc(len);
  bool enqueued = false;
  if (message) {
    memcpy_P(message, p, len);
    enqueued = binary(message, len);
    free(message);
  }
  return enqueued;
}
#endif

IPAddress AsyncWebSocketClient::remoteIP() const {
  // Reads _client, then dereference it without any lock.
  // A concurrent _onDisconnect could null + delete the client between the check and the use.
  // Local capture ensures the pointer is read exactly once, eliminating the null-dereference.
  // (TOCTOU)
  AsyncClient *c = _client;
  if (!c) {
    return IPAddress((uint32_t)0U);
  }
  return c->remoteIP();
}

uint16_t AsyncWebSocketClient::remotePort() const {
  // Reads _client, then dereference it without any lock.
  // A concurrent _onDisconnect could null + delete the client between the check and the use.
  // Local capture ensures the pointer is read exactly once, eliminating the null-dereference.
  // (TOCTOU)
  AsyncClient *c = _client;
  if (!c) {
    return 0;
  }
  return c->remotePort();
}

/*
 * Async Web Socket - Each separate socket location
 */

void AsyncWebSocket::_handleEvent(AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (_eventHandler != NULL) {
    _eventHandler(this, client, type, arg, data, len);
  }
}

AsyncWebSocketClient *AsyncWebSocket::_newClient(AsyncWebServerRequest *request) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  // Hold the request in scope for the user callback to inspect
  std::shared_ptr<AsyncWebServerRequest> req_lock = request->shared_from_this();
  // Adopt the client object from the request
  _clients.emplace_back(request->clientRelease(), this);
  // we've just detached AsyncTCP client from AsyncWebServerRequest
  _handleEvent(&_clients.back(), WS_EVT_CONNECT, request, NULL, 0);
  // req_lock releases the request object at the end of this function scope
  return &_clients.back();
}

void AsyncWebSocket::_handleDisconnect(AsyncWebSocketClient *client) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  const auto client_id = client->id();
  const auto iter = std::find_if(std::begin(_clients), std::end(_clients), [client_id](const AsyncWebSocketClient &c) {
    return c.id() == client_id;
  });
  if (iter != std::end(_clients)) {
    _clients.erase(iter);
  }
}

bool AsyncWebSocket::availableForWriteAll() {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  return std::none_of(std::begin(_clients), std::end(_clients), [](const AsyncWebSocketClient &c) {
    return c.queueIsFull();
  });
}

bool AsyncWebSocket::availableForWrite(uint32_t id) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  const auto iter = std::find_if(std::begin(_clients), std::end(_clients), [id](const AsyncWebSocketClient &c) {
    return c.id() == id;
  });
  if (iter == std::end(_clients)) {
    return true;
  }
  return !iter->queueIsFull();
}

size_t AsyncWebSocket::count() const {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  return std::count_if(std::begin(_clients), std::end(_clients), [](const AsyncWebSocketClient &c) {
    return c.status() == WS_CONNECTED;
  });
}

AsyncWebSocketClient *AsyncWebSocket::client(uint32_t id) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  const auto iter = std::find_if(_clients.begin(), _clients.end(), [id](const AsyncWebSocketClient &c) {
    return c.id() == id && c.status() == WS_CONNECTED;
  });
  if (iter == std::end(_clients)) {
    return nullptr;
  }

  return &(*iter);
}

void AsyncWebSocket::close(uint32_t id, uint16_t code, const char *message) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  if (AsyncWebSocketClient *c = client(id)) {
    c->close(code, message);
  }
}

void AsyncWebSocket::closeAll(uint16_t code, const char *message) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  for (auto &c : _clients) {
    if (c.status() == WS_CONNECTED) {
      c.close(code, message);
    }
  }
}

void AsyncWebSocket::cleanupClients(uint16_t maxClients) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  const size_t c = count();
  if (c > maxClients) {
    async_ws_log_v("[%s] CLEANUP %" PRIu32 " (%u/%" PRIu16 ")", _url.c_str(), _clients.front().id(), c, maxClients);
    _clients.front().close();
  }

  for (auto i = _clients.begin(); i != _clients.end(); ++i) {
    if (i->shouldBeDeleted()) {
      _clients.erase(i);
      break;
    }
  }
}

bool AsyncWebSocket::ping(uint32_t id, const uint8_t *data, size_t len) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  AsyncWebSocketClient *c = client(id);
  return c && c->ping(data, len);
}

AsyncWebSocket::SendStatus AsyncWebSocket::pingAll(const uint8_t *data, size_t len) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  size_t hit = 0;
  size_t miss = 0;
  for (auto &c : _clients) {
    if (c.status() == WS_CONNECTED && c.ping(data, len)) {
      hit++;
    } else {
      miss++;
    }
  }
  return hit == 0 ? DISCARDED : (miss == 0 ? ENQUEUED : PARTIALLY_ENQUEUED);
}

bool AsyncWebSocket::text(uint32_t id, const uint8_t *message, size_t len) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  AsyncWebSocketClient *c = client(id);
  return c && c->text(makeSharedBuffer(message, len));
}
bool AsyncWebSocket::text(uint32_t id, const char *message, size_t len) {
  return text(id, (const uint8_t *)message, len);
}
bool AsyncWebSocket::text(uint32_t id, const char *message) {
  return text(id, message, strlen(message));
}
bool AsyncWebSocket::text(uint32_t id, const String &message) {
  return text(id, message.c_str(), message.length());
}

#ifdef ESP8266
bool AsyncWebSocket::text(uint32_t id, const __FlashStringHelper *data) {
  PGM_P p = reinterpret_cast<PGM_P>(data);

  size_t n = 0;
  while (true) {
    if (pgm_read_byte(p + n) == 0) {
      break;
    }
    n += 1;
  }

  char *message = (char *)malloc(n + 1);
  bool enqueued = false;
  if (message) {
    memcpy_P(message, p, n);
    message[n] = 0;
    enqueued = text(id, message, n);
    free(message);
  }
  return enqueued;
}
#endif  // ESP8266

bool AsyncWebSocket::text(uint32_t id, AsyncWebSocketMessageBuffer *buffer) {
  bool enqueued = false;
  if (buffer) {
    enqueued = text(id, std::move(buffer->_buffer));
    delete buffer;
  }
  return enqueued;
}
bool AsyncWebSocket::text(uint32_t id, AsyncWebSocketSharedBuffer buffer) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  AsyncWebSocketClient *c = client(id);
  return c && c->text(buffer);
}

AsyncWebSocket::SendStatus AsyncWebSocket::textAll(const uint8_t *message, size_t len) {
  return textAll(makeSharedBuffer(message, len));
}
AsyncWebSocket::SendStatus AsyncWebSocket::textAll(const char *message, size_t len) {
  return textAll((const uint8_t *)message, len);
}
AsyncWebSocket::SendStatus AsyncWebSocket::textAll(const char *message) {
  return textAll(message, strlen(message));
}
AsyncWebSocket::SendStatus AsyncWebSocket::textAll(const String &message) {
  return textAll(message.c_str(), message.length());
}
#ifdef ESP8266
AsyncWebSocket::SendStatus AsyncWebSocket::textAll(const __FlashStringHelper *data) {
  PGM_P p = reinterpret_cast<PGM_P>(data);

  size_t n = 0;
  while (1) {
    if (pgm_read_byte(p + n) == 0) {
      break;
    }
    n += 1;
  }

  char *message = (char *)malloc(n + 1);
  AsyncWebSocket::SendStatus status = DISCARDED;
  if (message) {
    memcpy_P(message, p, n);
    message[n] = 0;
    status = textAll(message, n);
    free(message);
  }
  return status;
}
#endif  // ESP8266
AsyncWebSocket::SendStatus AsyncWebSocket::textAll(AsyncWebSocketMessageBuffer *buffer) {
  AsyncWebSocket::SendStatus status = DISCARDED;
  if (buffer) {
    status = textAll(std::move(buffer->_buffer));
    delete buffer;
  }
  return status;
}

AsyncWebSocket::SendStatus AsyncWebSocket::textAll(AsyncWebSocketSharedBuffer buffer) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  size_t hit = 0;
  size_t miss = 0;
  for (auto &c : _clients) {
    if (c.status() == WS_CONNECTED && c.text(buffer)) {
      hit++;
    } else {
      miss++;
    }
  }
  return hit == 0 ? DISCARDED : (miss == 0 ? ENQUEUED : PARTIALLY_ENQUEUED);
}

bool AsyncWebSocket::binary(uint32_t id, const uint8_t *message, size_t len) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  AsyncWebSocketClient *c = client(id);
  return c && c->binary(makeSharedBuffer(message, len));
}
bool AsyncWebSocket::binary(uint32_t id, const char *message, size_t len) {
  return binary(id, (const uint8_t *)message, len);
}
bool AsyncWebSocket::binary(uint32_t id, const char *message) {
  return binary(id, message, strlen(message));
}
bool AsyncWebSocket::binary(uint32_t id, const String &message) {
  return binary(id, message.c_str(), message.length());
}

#ifdef ESP8266
bool AsyncWebSocket::binary(uint32_t id, const __FlashStringHelper *data, size_t len) {
  PGM_P p = reinterpret_cast<PGM_P>(data);
  char *message = (char *)malloc(len);
  bool enqueued = false;
  if (message) {
    memcpy_P(message, p, len);
    enqueued = binary(id, message, len);
    free(message);
  }
  return enqueued;
}
#endif  // ESP8266

bool AsyncWebSocket::binary(uint32_t id, AsyncWebSocketMessageBuffer *buffer) {
  bool enqueued = false;
  if (buffer) {
    enqueued = binary(id, std::move(buffer->_buffer));
    delete buffer;
  }
  return enqueued;
}
bool AsyncWebSocket::binary(uint32_t id, AsyncWebSocketSharedBuffer buffer) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  AsyncWebSocketClient *c = client(id);
  return c && c->binary(buffer);
}

AsyncWebSocket::SendStatus AsyncWebSocket::binaryAll(const uint8_t *message, size_t len) {
  return binaryAll(makeSharedBuffer(message, len));
}
AsyncWebSocket::SendStatus AsyncWebSocket::binaryAll(const char *message, size_t len) {
  return binaryAll((const uint8_t *)message, len);
}
AsyncWebSocket::SendStatus AsyncWebSocket::binaryAll(const char *message) {
  return binaryAll(message, strlen(message));
}
AsyncWebSocket::SendStatus AsyncWebSocket::binaryAll(const String &message) {
  return binaryAll(message.c_str(), message.length());
}

#ifdef ESP8266
AsyncWebSocket::SendStatus AsyncWebSocket::binaryAll(const __FlashStringHelper *data, size_t len) {
  PGM_P p = reinterpret_cast<PGM_P>(data);
  char *message = (char *)malloc(len);
  AsyncWebSocket::SendStatus status = DISCARDED;
  if (message) {
    memcpy_P(message, p, len);
    status = binaryAll(message, len);
    free(message);
  }
  return status;
}
#endif  // ESP8266

AsyncWebSocket::SendStatus AsyncWebSocket::binaryAll(AsyncWebSocketMessageBuffer *buffer) {
  AsyncWebSocket::SendStatus status = DISCARDED;
  if (buffer) {
    status = binaryAll(std::move(buffer->_buffer));
    delete buffer;
  }
  return status;
}
AsyncWebSocket::SendStatus AsyncWebSocket::binaryAll(AsyncWebSocketSharedBuffer buffer) {
  asyncsrv::lock_guard_type lock(_ws_clients_lock);
  size_t hit = 0;
  size_t miss = 0;
  for (auto &c : _clients) {
    if (c.status() == WS_CONNECTED && c.binary(buffer)) {
      hit++;
    } else {
      miss++;
    }
  }
  return hit == 0 ? DISCARDED : (miss == 0 ? ENQUEUED : PARTIALLY_ENQUEUED);
}

size_t AsyncWebSocket::printf(uint32_t id, const char *format, ...) {
  AsyncWebSocketClient *c = client(id);
  if (c) {
    va_list arg;
    va_start(arg, format);
    size_t len = c->printf(format, arg);
    va_end(arg);
    return len;
  }
  return 0;
}

size_t AsyncWebSocket::printfAll(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  size_t len = vsnprintf(nullptr, 0, format, arg);
  va_end(arg);

  if (len == 0) {
    return 0;
  }

  char *buffer = new char[len + 1];

  if (!buffer) {
    return 0;
  }

  va_start(arg, format);
  len = vsnprintf(buffer, len + 1, format, arg);
  va_end(arg);

  AsyncWebSocket::SendStatus status = textAll(buffer, len);
  delete[] buffer;
  return status == DISCARDED ? 0 : len;
}

#ifdef ESP8266
size_t AsyncWebSocket::printf_P(uint32_t id, PGM_P formatP, ...) {
  AsyncWebSocketClient *c = client(id);
  if (c != NULL) {
    va_list arg;
    va_start(arg, formatP);
    size_t len = c->printf_P(formatP, arg);
    va_end(arg);
    return len;
  }
  return 0;
}

size_t AsyncWebSocket::printfAll_P(PGM_P formatP, ...) {
  va_list arg;
  va_start(arg, formatP);
  size_t len = vsnprintf_P(nullptr, 0, formatP, arg);
  va_end(arg);

  if (len == 0) {
    return 0;
  }

  char *buffer = new char[len + 1];

  if (!buffer) {
    return 0;
  }

  va_start(arg, formatP);
  len = vsnprintf_P(buffer, len + 1, formatP, arg);
  va_end(arg);

  AsyncWebSocket::SendStatus status = textAll(buffer, len);
  delete[] buffer;
  return status == DISCARDED ? 0 : len;
}
#endif

const char __WS_STR_CONNECTION[] PROGMEM = {"Connection"};
const char __WS_STR_UPGRADE[] PROGMEM = {"Upgrade"};
const char __WS_STR_ORIGIN[] PROGMEM = {"Origin"};
const char __WS_STR_COOKIE[] PROGMEM = {"Cookie"};
const char __WS_STR_VERSION[] PROGMEM = {"Sec-WebSocket-Version"};
const char __WS_STR_KEY[] PROGMEM = {"Sec-WebSocket-Key"};
const char __WS_STR_PROTOCOL[] PROGMEM = {"Sec-WebSocket-Protocol"};
const char __WS_STR_ACCEPT[] PROGMEM = {"Sec-WebSocket-Accept"};
const char __WS_STR_UUID[] PROGMEM = {"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"};

#define WS_STR_UUID_LEN 36

#define WS_STR_CONNECTION FPSTR(__WS_STR_CONNECTION)
#define WS_STR_UPGRADE    FPSTR(__WS_STR_UPGRADE)
#define WS_STR_ORIGIN     FPSTR(__WS_STR_ORIGIN)
#define WS_STR_COOKIE     FPSTR(__WS_STR_COOKIE)
#define WS_STR_VERSION    FPSTR(__WS_STR_VERSION)
#define WS_STR_KEY        FPSTR(__WS_STR_KEY)
#define WS_STR_PROTOCOL   FPSTR(__WS_STR_PROTOCOL)
#define WS_STR_ACCEPT     FPSTR(__WS_STR_ACCEPT)
#define WS_STR_UUID       FPSTR(__WS_STR_UUID)

bool AsyncWebSocket::canHandle(AsyncWebServerRequest *request) const {
  return _enabled && request->isWebSocketUpgrade() && request->url().equals(_url);
}

void AsyncWebSocket::handleRequest(AsyncWebServerRequest *request) {
  if (!request->hasHeader(WS_STR_VERSION) || !request->hasHeader(WS_STR_KEY)) {
    request->send(400);
    return;
  }
  if (_handshakeHandler != nullptr) {
    if (!_handshakeHandler(request)) {
      request->send(401);
      return;
    }
  }
  const AsyncWebHeader *version = request->getHeader(WS_STR_VERSION);
  if (version->value().toInt() != 13) {
    AsyncWebServerResponse *response = request->beginResponse(400);
    response->addHeader(WS_STR_VERSION, T_13);
    request->send(response);
    return;
  }
  const AsyncWebHeader *key = request->getHeader(WS_STR_KEY);
  AsyncWebServerResponse *response = new AsyncWebSocketResponse(key->value(), this);
  if (response == NULL) {
    async_ws_log_e("Failed to allocate");
    request->abort();
    return;
  }
  if (request->hasHeader(WS_STR_PROTOCOL)) {
    const AsyncWebHeader *protocol = request->getHeader(WS_STR_PROTOCOL);
    // ToDo: check protocol
    response->addHeader(WS_STR_PROTOCOL, protocol->value());
  }
  request->send(response);
}

AsyncWebSocketMessageBuffer *AsyncWebSocket::makeBuffer(size_t size) {
  return new AsyncWebSocketMessageBuffer(size);
}

AsyncWebSocketMessageBuffer *AsyncWebSocket::makeBuffer(const uint8_t *data, size_t size) {
  return new AsyncWebSocketMessageBuffer(data, size);
}

/*
 * Response to Web Socket request - sends the authorization and detaches the TCP Client from the web server
 * Authentication code from https://github.com/Links2004/arduinoWebSockets/blob/master/src/WebSockets.cpp#L480
 */

AsyncWebSocketResponse::AsyncWebSocketResponse(const String &key, AsyncWebSocket *server) : _server(server) {
  _code = 101;
  _sendContentLength = false;

  uint8_t hash[20];
  char buffer[33];

#if defined(ESP8266) || defined(TARGET_RP2040) || defined(PICO_RP2040) || defined(PICO_RP2350) || defined(TARGET_RP2350)
  sha1(key + WS_STR_UUID, hash);
#else
  String k;
  if (!k.reserve(key.length() + WS_STR_UUID_LEN)) {
    async_ws_log_e("Failed to allocate");
    return;
  }
  k.concat(key);
  k.concat(WS_STR_UUID);
#ifdef LIBRETINY
  mbedtls_sha1_context ctx;
  mbedtls_sha1_init(&ctx);
  mbedtls_sha1_starts(&ctx);
  mbedtls_sha1_update(&ctx, (const uint8_t *)k.c_str(), k.length());
  mbedtls_sha1_finish(&ctx, hash);
  mbedtls_sha1_free(&ctx);
#else
  SHA1Builder sha1;
  sha1.begin();
  sha1.add((const uint8_t *)k.c_str(), k.length());
  sha1.calculate();
  sha1.getBytes(hash);
#endif
#endif
  base64_encodestate _state;
  base64_init_encodestate(&_state);
  int len = base64_encode_block((const char *)hash, 20, buffer, &_state);
  len = base64_encode_blockend((buffer + len), &_state);
  addHeader(WS_STR_CONNECTION, WS_STR_UPGRADE);
  addHeader(WS_STR_UPGRADE, T_WS);
  addHeader(WS_STR_ACCEPT, buffer);
}

void AsyncWebSocketResponse::_respond(AsyncWebServerRequest *request) {
  if (_state == RESPONSE_FAILED) {
    request->client()->close();
    return;
  }
  // unbind client's onAck callback from AsyncWebServerRequest's, we will destroy it on next callback and steal the client,
  // can't do it now 'cause now we are in AsyncWebServerRequest::_onAck 's stack actually
  // here we are loosing time on one RTT delay, but with current design we can't get rid of Req/Resp objects other way
  _request = request;
  request->client()->onAck(
    [](void *r, AsyncClient *c, size_t len, uint32_t time) {
      if (len) {
        static_cast<AsyncWebSocketResponse *>(r)->_switchClient();
      }
    },
    this
  );
  String out;
  _assembleHead(out, request->version());
  request->client()->write(out.c_str(), _headLength);
  _state = RESPONSE_WAIT_ACK;
}

void AsyncWebSocketResponse::_switchClient() {
  // detach client from request
  _server->_newClient(_request);
  // _newClient() would also destruct _request and *this
}
