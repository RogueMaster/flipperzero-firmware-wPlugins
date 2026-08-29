# WebSockets

## `AsyncWebSocketMessageBuffer` and `makeBuffer()`

The fork from [yubox-node-org](https://github.com/yubox-node-org/ESPAsyncWebServer) introduces some breaking API changes compared to the original library, especially regarding the use of `std::shared_ptr<std::vector<uint8_t>>` for WebSocket.

This library is compatible with the original library from [me-no-dev](https://github.com/me-no-dev/ESPAsyncWebServer) regarding WebSocket, and wraps the optimizations done by `yubox-node-org` in the `AsyncWebSocketMessageBuffer` class.
So you have the choice of which API to use.

Here are examples for serializing a Json document in a websocket message buffer:

```cpp
void send(JsonDocument& doc) {
  const size_t len = measureJson(doc);

  // original API from me-no-dev
  AsyncWebSocketMessageBuffer* buffer = _ws->makeBuffer(len);
  assert(buffer); // up to you to keep or remove this
  serializeJson(doc, buffer->get(), len);
  _ws->textAll(buffer);
}
```

```cpp
void send(JsonDocument& doc) {
  const size_t len = measureJson(doc);

  // this fork (originally from yubox-node-org), uses another API with shared pointer
  auto buffer = std::make_shared<std::vector<uint8_t>>(len);
  assert(buffer); // up to you to keep or remove this
  serializeJson(doc, buffer->data(), len);
  _ws->textAll(std::move(buffer));
}
```

I recommend to use the official API `AsyncWebSocketMessageBuffer` to retain further compatibility.

## Async WebSocket Plugin

The server includes a web socket plugin which lets you define different WebSocket locations to connect to
without starting another listening service or using different port

See the [WebSocket example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/WebSocket/WebSocket.ino) and [WebSocketEasy example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/WebSocketEasy/WebSocketEasy.ino).

### Async WebSocket Event

```cpp

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    //client connected
    os_printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    os_printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    os_printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    os_printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      os_printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        os_printf("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < info->len; i++){
          os_printf("%02x ", data[i]);
        }
        os_printf("\n");
      }
      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          os_printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        os_printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      os_printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        os_printf("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < len; i++){
          os_printf("%02x ", data[i]);
        }
        os_printf("\n");
      }

      if((info->index + len) == info->len){
        os_printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          os_printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}
```

### Methods for sending data to a socket client

```cpp



//Server methods
AsyncWebSocket ws("/ws");
//printf to a client
ws.printf((uint32_t)client_id, arguments...);
//printf to all clients
ws.printfAll(arguments...);
//printf_P to a client
ws.printf_P((uint32_t)client_id, PSTR(format), arguments...);
//printfAll_P to all clients
ws.printfAll_P(PSTR(format), arguments...);
//send text to a client
ws.text((uint32_t)client_id, (char*)text);
ws.text((uint32_t)client_id, (uint8_t*)text, (size_t)len);
//send text from PROGMEM to a client
ws.text((uint32_t)client_id, PSTR("text"));
const char flash_text[] PROGMEM = "Text to send"
ws.text((uint32_t)client_id, FPSTR(flash_text));
//send text to all clients
ws.textAll((char*)text);
ws.textAll((uint8_t*)text, (size_t)len);
//send binary to a client
ws.binary((uint32_t)client_id, (char*)binary);
ws.binary((uint32_t)client_id, (uint8_t*)binary, (size_t)len);
//send binary from PROGMEM to a client
const uint8_t flash_binary[] PROGMEM = { 0x01, 0x02, 0x03, 0x04 };
ws.binary((uint32_t)client_id, flash_binary, 4);
//send binary to all clients
ws.binaryAll((char*)binary);
ws.binaryAll((uint8_t*)binary, (size_t)len);

//client methods
AsyncWebSocketClient * client;
//printf
client->printf(arguments...);
//printf_P
client->printf_P(PSTR(format), arguments...);
//send text
client->text((char*)text);
client->text((uint8_t*)text, (size_t)len);
//send text from PROGMEM
client->text(PSTR("text"));
const char flash_text[] PROGMEM = "Text to send";
client->text(FPSTR(flash_text));
//send binary
client->binary((char*)binary);
client->binary((uint8_t*)binary, (size_t)len);
//send binary from PROGMEM
const uint8_t flash_binary[] PROGMEM = { 0x01, 0x02, 0x03, 0x04 };
client->binary(flash_binary, 4);
```

### Queue full behavior: `setCloseClientOnQueueFull()`

When a client cannot keep up, outgoing WebSocket messages are queued.
If the queue reaches `WS_MAX_QUEUED_MESSAGES`, new messages are either discarded or the client is closed, depending on this setting.

```cpp
client->setCloseClientOnQueueFull(bool close);
```

- `close == false` (default in this library): discard new messages when the queue is full.
- `close == true`: close the client when the queue is full.

We recommend using `false`.

When the queue starts filling, prefer reducing your sending rate and/or explicitly closing the client according to your application policy.

We do not recommend using `true` because it can lead to a crash under certain circumstances.

You can combine this with:

- `client->queueIsFull()`
- `client->queueLen()`
- `ws.availableForWrite(clientId)` and `ws.availableForWriteAll()`

Typical usage is to set the policy when the client connects:

```cpp
if (type == WS_EVT_CONNECT) {
  client->setCloseClientOnQueueFull(false); // default behavior
}
```

### Direct access to web socket message buffer

When sending a web socket message using the above methods a buffer is created. Under certain circumstances you might want to manipulate or populate this buffer directly from your application, for example to prevent unnecessary duplications of the data. This example below shows how to create a buffer and print data to it from an ArduinoJson object then send it.

```cpp
void sendDataWs(AsyncWebSocketClient * client)
{
    JsonDocument doc;
    doc["a"] = "abc";
    doc["b"] = "abcd";
    doc["c"] = "abcde";
    doc["d"] = "abcdef";
    doc["e"] = "abcdefg";
    size_t len = measureJson(doc);
    AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
    if (buffer) {
        serializeJson(doc, (char *)buffer->get(), len + 1);
        if (client) {
            client->text(buffer);
        } else {
            ws.textAll(buffer);
        }
    }
}
```

### Limiting the number of web socket clients

Browsers sometimes do not correctly close the websocket connection, even when the close() function is called in javascript. This will eventually exhaust the web server's resources and will cause the server to crash. Periodically calling the cleanClients() function from the main loop() function limits the number of clients by closing the oldest client when the maximum number of clients has been exceeded. This can called be every cycle, however, if you wish to use less power, then calling as infrequently as once per second is sufficient.

```cpp
void loop(){
  ws.cleanupClients();
}
```
