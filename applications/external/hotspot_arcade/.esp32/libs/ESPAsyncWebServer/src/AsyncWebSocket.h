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
#ifndef ASYNCWEBSOCKET_H_
#define ASYNCWEBSOCKET_H_

#include <Arduino.h>
#ifdef ESP32
  #include <AsyncTCP.h>
  #include <mutex>
  #ifndef WS_MAX_QUEUED_MESSAGES
    #define WS_MAX_QUEUED_MESSAGES 32
  #endif
#elif defined(ESP8266)
  #include <ESPAsyncTCP.h>
  #ifndef WS_MAX_QUEUED_MESSAGES
    #define WS_MAX_QUEUED_MESSAGES 8
  #endif
#elif defined(TARGET_RP2040)
  #include <AsyncTCP_RP2040W.h>
  #ifndef WS_MAX_QUEUED_MESSAGES
    #define WS_MAX_QUEUED_MESSAGES 32
  #endif
#endif

#include <ESPAsyncWebServer.h>

#include <memory>

#ifdef ESP8266
  #include <Hash.h>
  #ifdef CRYPTO_HASH_h // include Hash.h from espressif framework if the first include was from the crypto library
    #include <../src/Hash.h>
  #endif
#endif

#ifndef DEFAULT_MAX_WS_CLIENTS
  #ifdef ESP32
    #define DEFAULT_MAX_WS_CLIENTS 8
  #else
    #define DEFAULT_MAX_WS_CLIENTS 4
  #endif
#endif

using AsyncWebSocketSharedBuffer = std::shared_ptr<std::vector<uint8_t>>;

class AsyncWebSocket;
class AsyncWebSocketResponse;
class AsyncWebSocketClient;
class AsyncWebSocketControl;

typedef struct {
    /** Message type as defined by enum AwsFrameType.
     * Note: Applications will only see WS_TEXT and WS_BINARY.
     * All other types are handled by the library. */
    uint8_t message_opcode;
    /** Frame number of a fragmented message. */
    uint32_t num;
    /** Is this the last frame in a fragmented message ?*/
    uint8_t final;
    /** Is this frame masked? */
    uint8_t masked;
    /** Message type as defined by enum AwsFrameType.
     * This value is the same as message_opcode for non-fragmented
     * messages, but may also be WS_CONTINUATION in a fragmented message. */
    uint8_t opcode;
    /** Length of the current frame.
     * This equals the total length of the message if num == 0 && final == true */
    uint64_t len;
    /** Mask key */
    uint8_t mask[4];
    /** Offset of the data inside the current frame. */
    uint64_t index;
} AwsFrameInfo;

typedef enum { WS_DISCONNECTED,
               WS_CONNECTED,
               WS_DISCONNECTING } AwsClientStatus;
typedef enum { WS_CONTINUATION,
               WS_TEXT,
               WS_BINARY,
               WS_DISCONNECT = 0x08,
               WS_PING,
               WS_PONG } AwsFrameType;
typedef enum { WS_MSG_SENDING,
               WS_MSG_SENT,
               WS_MSG_ERROR } AwsMessageStatus;
typedef enum { WS_EVT_CONNECT,
               WS_EVT_DISCONNECT,
               WS_EVT_PING,
               WS_EVT_PONG,
               WS_EVT_ERROR,
               WS_EVT_DATA } AwsEventType;

class AsyncWebSocketMessageBuffer {
    friend AsyncWebSocket;
    friend AsyncWebSocketClient;

  private:
    AsyncWebSocketSharedBuffer _buffer;

  public:
    AsyncWebSocketMessageBuffer() {}
    explicit AsyncWebSocketMessageBuffer(size_t size);
    AsyncWebSocketMessageBuffer(const uint8_t* data, size_t size);
    //~AsyncWebSocketMessageBuffer();
    bool reserve(size_t size);
    uint8_t* get() { return _buffer->data(); }
    size_t length() const { return _buffer->size(); }
};

class AsyncWebSocketMessage {
  private:
    AsyncWebSocketSharedBuffer _WSbuffer;
    uint8_t _opcode{WS_TEXT};
    bool _mask{false};
    AwsMessageStatus _status{WS_MSG_ERROR};
    size_t _sent{};
    size_t _ack{};
    size_t _acked{};

  public:
    AsyncWebSocketMessage(AsyncWebSocketSharedBuffer buffer, uint8_t opcode = WS_TEXT, bool mask = false);

    bool finished() const { return _status != WS_MSG_SENDING; }
    bool betweenFrames() const { return _acked == _ack; }

    void ack(size_t len, uint32_t time);
    size_t send(AsyncClient* client);
};

class AsyncWebSocketClient {
  private:
    AsyncClient* _client;
    AsyncWebSocket* _server;
    uint32_t _clientId;
    AwsClientStatus _status;
#ifdef ESP32
    mutable std::mutex _lock;
#endif
    std::deque<AsyncWebSocketControl> _controlQueue;
    std::deque<AsyncWebSocketMessage> _messageQueue;
    bool closeWhenFull = true;

    uint8_t _pstate;
    AwsFrameInfo _pinfo;

    uint32_t _lastMessageTime;
    uint32_t _keepAlivePeriod;

    bool _queueControl(uint8_t opcode, const uint8_t* data = NULL, size_t len = 0, bool mask = false);
    bool _queueMessage(AsyncWebSocketSharedBuffer buffer, uint8_t opcode = WS_TEXT, bool mask = false);
    void _runQueue();
    void _clearQueue();

  public:
    void* _tempObject;

    AsyncWebSocketClient(AsyncWebServerRequest* request, AsyncWebSocket* server);
    ~AsyncWebSocketClient();

    // client id increments for the given server
    uint32_t id() const { return _clientId; }
    AwsClientStatus status() const { return _status; }
    AsyncClient* client() { return _client; }
    const AsyncClient* client() const { return _client; }
    AsyncWebSocket* server() { return _server; }
    const AsyncWebSocket* server() const { return _server; }
    AwsFrameInfo const& pinfo() const { return _pinfo; }

    //  - If "true" (default), the connection will be closed if the message queue is full.
    // This is the default behavior in yubox-node-org, which is not silently discarding messages but instead closes the connection.
    // The big issue with this behavior is  that is can cause the UI to automatically re-create a new WS connection, which can be filled again,
    // and so on, causing a resource exhaustion.
    //
    // - If "false", the incoming message will be discarded if the queue is full.
    // This is the default behavior in the original ESPAsyncWebServer library from me-no-dev.
    // This behavior allows the best performance at the expense of unreliable message delivery in case the queue is full (some messages may be lost).
    //
    // - In any case, when the queue is full, a message is logged.
    // - IT is recommended to use the methods queueIsFull(), availableForWriteAll(), availableForWrite(clientId) to check if the queue is full before sending a message.
    //
    // Usage:
    //  - can be set in the onEvent listener when connecting (event type is: WS_EVT_CONNECT)
    //
    // Use cases:,
    // - if using websocket to send logging messages, maybe some loss is acceptable.
    // - But if using websocket to send UI update messages, maybe the connection should be closed and the UI redrawn.
    void setCloseClientOnQueueFull(bool close) { closeWhenFull = close; }
    bool willCloseClientOnQueueFull() const { return closeWhenFull; }

    IPAddress remoteIP() const;
    uint16_t remotePort() const;

    bool shouldBeDeleted() const { return !_client; }

    // control frames
    void close(uint16_t code = 0, const char* message = NULL);
    bool ping(const uint8_t* data = NULL, size_t len = 0);

    // set auto-ping period in seconds. disabled if zero (default)
    void keepAlivePeriod(uint16_t seconds) {
      _keepAlivePeriod = seconds * 1000;
    }
    uint16_t keepAlivePeriod() {
      return (uint16_t)(_keepAlivePeriod / 1000);
    }

    // data packets
    void message(AsyncWebSocketSharedBuffer buffer, uint8_t opcode = WS_TEXT, bool mask = false) { _queueMessage(buffer, opcode, mask); }
    bool queueIsFull() const;
    size_t queueLen() const;

    size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3)));

    bool text(AsyncWebSocketSharedBuffer buffer);
    bool text(const uint8_t* message, size_t len);
    bool text(const char* message, size_t len);
    bool text(const char* message);
    bool text(const String& message);
    bool text(AsyncWebSocketMessageBuffer* buffer);

    bool binary(AsyncWebSocketSharedBuffer buffer);
    bool binary(const uint8_t* message, size_t len);
    bool binary(const char* message, size_t len);
    bool binary(const char* message);
    bool binary(const String& message);
    bool binary(AsyncWebSocketMessageBuffer* buffer);

    bool canSend() const;

    // system callbacks (do not call)
    void _onAck(size_t len, uint32_t time);
    void _onError(int8_t);
    void _onPoll();
    void _onTimeout(uint32_t time);
    void _onDisconnect();
    void _onData(void* pbuf, size_t plen);

#ifdef ESP8266
    size_t printf_P(PGM_P formatP, ...) __attribute__((format(printf, 2, 3)));
    bool text(const __FlashStringHelper* message);
    bool binary(const __FlashStringHelper* message, size_t len);
#endif
};

using AwsHandshakeHandler = std::function<bool(AsyncWebServerRequest* request)>;
using AwsEventHandler = std::function<void(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)>;

// WebServer Handler implementation that plays the role of a socket server
class AsyncWebSocket : public AsyncWebHandler {
  private:
    String _url;
    std::list<AsyncWebSocketClient> _clients;
    uint32_t _cNextId;
    AwsEventHandler _eventHandler{nullptr};
    AwsHandshakeHandler _handshakeHandler;
    bool _enabled;
#ifdef ESP32
    mutable std::mutex _lock;
#endif

  public:
    typedef enum {
      DISCARDED = 0,
      ENQUEUED = 1,
      PARTIALLY_ENQUEUED = 2,
    } SendStatus;

    explicit AsyncWebSocket(const char* url) : _url(url), _cNextId(1), _enabled(true) {}
    AsyncWebSocket(const String& url) : _url(url), _cNextId(1), _enabled(true) {}
    ~AsyncWebSocket() {};
    const char* url() const { return _url.c_str(); }
    void enable(bool e) { _enabled = e; }
    bool enabled() const { return _enabled; }
    bool availableForWriteAll();
    bool availableForWrite(uint32_t id);

    size_t count() const;
    AsyncWebSocketClient* client(uint32_t id);
    bool hasClient(uint32_t id) { return client(id) != nullptr; }

    void close(uint32_t id, uint16_t code = 0, const char* message = NULL);
    void closeAll(uint16_t code = 0, const char* message = NULL);
    void cleanupClients(uint16_t maxClients = DEFAULT_MAX_WS_CLIENTS);

    bool ping(uint32_t id, const uint8_t* data = NULL, size_t len = 0);
    SendStatus pingAll(const uint8_t* data = NULL, size_t len = 0); //  done

    bool text(uint32_t id, const uint8_t* message, size_t len);
    bool text(uint32_t id, const char* message, size_t len);
    bool text(uint32_t id, const char* message);
    bool text(uint32_t id, const String& message);
    bool text(uint32_t id, AsyncWebSocketMessageBuffer* buffer);
    bool text(uint32_t id, AsyncWebSocketSharedBuffer buffer);

    SendStatus textAll(const uint8_t* message, size_t len);
    SendStatus textAll(const char* message, size_t len);
    SendStatus textAll(const char* message);
    SendStatus textAll(const String& message);
    SendStatus textAll(AsyncWebSocketMessageBuffer* buffer);
    SendStatus textAll(AsyncWebSocketSharedBuffer buffer);

    bool binary(uint32_t id, const uint8_t* message, size_t len);
    bool binary(uint32_t id, const char* message, size_t len);
    bool binary(uint32_t id, const char* message);
    bool binary(uint32_t id, const String& message);
    bool binary(uint32_t id, AsyncWebSocketMessageBuffer* buffer);
    bool binary(uint32_t id, AsyncWebSocketSharedBuffer buffer);

    SendStatus binaryAll(const uint8_t* message, size_t len);
    SendStatus binaryAll(const char* message, size_t len);
    SendStatus binaryAll(const char* message);
    SendStatus binaryAll(const String& message);
    SendStatus binaryAll(AsyncWebSocketMessageBuffer* buffer);
    SendStatus binaryAll(AsyncWebSocketSharedBuffer buffer);

    size_t printf(uint32_t id, const char* format, ...) __attribute__((format(printf, 3, 4)));
    size_t printfAll(const char* format, ...) __attribute__((format(printf, 2, 3)));

#ifdef ESP8266
    bool text(uint32_t id, const __FlashStringHelper* message);
    SendStatus textAll(const __FlashStringHelper* message);
    bool binary(uint32_t id, const __FlashStringHelper* message, size_t len);
    SendStatus binaryAll(const __FlashStringHelper* message, size_t len);
    size_t printf_P(uint32_t id, PGM_P formatP, ...) __attribute__((format(printf, 3, 4)));
    size_t printfAll_P(PGM_P formatP, ...) __attribute__((format(printf, 2, 3)));
#endif

    void onEvent(AwsEventHandler handler) { _eventHandler = handler; }
    void handleHandshake(AwsHandshakeHandler handler) { _handshakeHandler = handler; }

    // system callbacks (do not call)
    uint32_t _getNextId() { return _cNextId++; }
    AsyncWebSocketClient* _newClient(AsyncWebServerRequest* request);
    void _handleEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
    bool canHandle(AsyncWebServerRequest* request) const override final;
    void handleRequest(AsyncWebServerRequest* request) override final;

    //  messagebuffer functions/objects.
    AsyncWebSocketMessageBuffer* makeBuffer(size_t size = 0);
    AsyncWebSocketMessageBuffer* makeBuffer(const uint8_t* data, size_t size);

    std::list<AsyncWebSocketClient>& getClients() { return _clients; }
};

// WebServer response to authenticate the socket and detach the tcp client from the web server request
class AsyncWebSocketResponse : public AsyncWebServerResponse {
  private:
    String _content;
    AsyncWebSocket* _server;

  public:
    AsyncWebSocketResponse(const String& key, AsyncWebSocket* server);
    void _respond(AsyncWebServerRequest* request);
    size_t _ack(AsyncWebServerRequest* request, size_t len, uint32_t time);
    bool _sourceValid() const { return true; }
};

#endif /* ASYNCWEBSOCKET_H_ */
