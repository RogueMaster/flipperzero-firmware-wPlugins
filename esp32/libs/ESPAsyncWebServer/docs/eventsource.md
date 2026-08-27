# EventSource

## Async Event Source Plugin

The server includes EventSource (Server-Sent Events) plugin which can be used to send short text events to the browser.
Difference between EventSource and WebSockets is that EventSource is single direction, text-only protocol.

See the [ServerSentEvents example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/ServerSentEvents/ServerSentEvents.ino).

### Setup Event Source on the server

```cpp
AsyncWebServer server(80);
AsyncEventSource events("/events");

void setup(){
  // setup ......
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    //send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!",NULL,millis(),1000);
  });

  server.addHandler(&events);
  // setup ......
}

void loop(){
  if(eventTriggered){ // your logic here
    //send event "myevent"
    events.send("my event content","myevent",millis());
  }
}
```

**IMPORTANT**: Use `AsyncAuthenticationMiddleware` instead of the deprecated `setAuthentication()` method for authentication.

```cpp
AsyncAuthenticationMiddleware authMiddleware;
authMiddleware.setAuthType(AsyncAuthType::AUTH_DIGEST);
authMiddleware.setRealm("My app name");
authMiddleware.setUsername("admin");
authMiddleware.setPassword("admin");
authMiddleware.generateHash();

events.addMiddleware(&authMiddleware);
```

### Setup Event Source in the browser

```javascript
if (!!window.EventSource) {
  var source = new EventSource("/events");

  source.addEventListener(
    "open",
    function (e) {
      console.log("Events Connected");
    },
    false,
  );

  source.addEventListener(
    "error",
    function (e) {
      if (e.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected");
      }
    },
    false,
  );

  source.addEventListener(
    "message",
    function (e) {
      console.log("message", e.data);
    },
    false,
  );

  source.addEventListener(
    "myevent",
    function (e) {
      console.log("myevent", e.data);
    },
    false,
  );
}
```
