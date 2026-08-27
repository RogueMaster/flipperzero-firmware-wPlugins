# Filters

## Using filters

Filters can be set to `Rewrite` or `Handler` in order to control when to apply the rewrite and consider the handler.
A filter is a callback function that evaluates the request and return a boolean `true` to include the item
or `false` to exclude it.
Two filter callback are provided for convince:

- `ON_STA_FILTER` - return true when requests are made to the STA (station mode) interface.
- `ON_AP_FILTER` - return true when requests are made to the AP (access point) interface.

See the [Filters example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Filters/Filters.ino).

### Serve different site files in AP mode

```cpp
// For SPIFFS
server.serveStatic("/", SPIFFS, "/www/").setFilter(ON_STA_FILTER);
server.serveStatic("/", SPIFFS, "/ap/").setFilter(ON_AP_FILTER);

// For LittleFS
server.serveStatic("/", LittleFS, "/www/").setFilter(ON_STA_FILTER);
server.serveStatic("/", LittleFS, "/ap/").setFilter(ON_AP_FILTER);
```

### Rewrite to different index on AP

```cpp
// Serve the file "/www/index-ap.htm" in AP, and the file "/www/index.htm" on STA (SPIFFS)
server.rewrite("/", "index.htm");
server.rewrite("/index.htm", "index-ap.htm").setFilter(ON_AP_FILTER);
server.serveStatic("/", SPIFFS, "/www/");

// Serve the file "/www/index-ap.htm" in AP, and the file "/www/index.htm" on STA (LittleFS)
server.rewrite("/", "index.htm");
server.rewrite("/index.htm", "index-ap.htm").setFilter(ON_AP_FILTER);
server.serveStatic("/", LittleFS, "/www/");
```

### Serving different hosts

```cpp
// Filter callback using request host
bool filterOnHost1(AsyncWebServerRequest *request) { return request->host() == "host1"; }

// Server setup: server files in "/host1/" to requests for "host1", and files in "/www/" otherwise (SPIFFS).
server.serveStatic("/", SPIFFS, "/host1/").setFilter(filterOnHost1);
server.serveStatic("/", SPIFFS, "/www/");

// Server setup: server files in "/host1/" to requests for "host1", and files in "/www/" otherwise (LittleFS).
server.serveStatic("/", LittleFS, "/host1/").setFilter(filterOnHost1);
server.serveStatic("/", LittleFS, "/www/");
```

### Determine interface inside callbacks

```cpp
  String RedirectUrl = "http://";
  if (ON_STA_FILTER(request)) {
    RedirectUrl += WiFi.localIP().toString();
  } else {
    RedirectUrl += WiFi.softAPIP().toString();
  }
  RedirectUrl += "/index.htm";
  request->redirect(RedirectUrl);
```
