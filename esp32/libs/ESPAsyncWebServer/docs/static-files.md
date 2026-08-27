# Static Files

## Serving static files

In addition to serving files from SPIFFS / LittleFS as described above, the server provide a dedicated handler that optimize the
performance of serving files from SPIFFS / LittleFS - `AsyncStaticWebHandler`. Use `server.serveStatic()` function to
initialize and add a new instance of `AsyncStaticWebHandler` to the server.
The Handler will not handle the request if the file does not exists, e.g. the server will continue to look for another
handler that can handle the request.
Notice that you can chain setter functions to setup the handler, or keep a pointer to change it at a later time.

### Serving specific file by name

```cpp
// Serve the file "/www/page.htm" when request url is "/page.htm" from SPIFFS
server.serveStatic("/page.htm", SPIFFS, "/www/page.htm");

// Serve the file "/www/page.htm" when request url is "/page.htm" from LittleFS
server.serveStatic("/page.htm", LittleFS, "/www/page.htm");
```

### Serving files in directory

To serve files in a directory, the path to the files should specify a directory in SPIFFS / LittleFS and ends with "/".

```cpp
// Serve files in directory "/www/" when request url starts with "/" from SPIFFS
// Request to the root or none existing files will try to server the default
// file name "index.htm" if exists
server.serveStatic("/", SPIFFS, "/www/");

// Server with different default file from SPIFFS
server.serveStatic("/", SPIFFS, "/www/").setDefaultFile("default.html");

// Serve files in directory "/www/" when request url starts with "/" from LittleFS
// Request to the root or none existing files will try to server the default
// file name "index.htm" if exists
server.serveStatic("/", LittleFS, "/www/");

// Server with different default file from LittleFS
server.serveStatic("/", LittleFS, "/www/").setDefaultFile("default.html");
```

### Serving static files with authentication

**IMPORTANT**: Use `AsyncAuthenticationMiddleware` instead of the deprecated `setAuthentication()` method.

```cpp
AsyncAuthenticationMiddleware authMiddleware;
authMiddleware.setAuthType(AsyncAuthType::AUTH_DIGEST);
authMiddleware.setRealm("My app name");
authMiddleware.setUsername("admin");
authMiddleware.setPassword("admin");
authMiddleware.generateHash();

// For SPIFFS
server
    .serveStatic("/", SPIFFS, "/www/")
    .setDefaultFile("default.html")
    .addMiddleware(&authMiddleware);

// For LittleFS
server
    .serveStatic("/", LittleFS, "/www/")
    .setDefaultFile("default.html")
    .addMiddleware(&authMiddleware);
```

### Specifying Cache-Control header

It is possible to specify Cache-Control header value to reduce the number of calls to the server once the client loaded
the files. For more information on Cache-Control values see [Cache-Control](https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.9)

```cpp
// Cache responses for 10 minutes (600 seconds) from SPIFFS
server.serveStatic("/", SPIFFS, "/www/").setCacheControl("max-age=600");

// Cache responses for 10 minutes (600 seconds) from LittleFS
server.serveStatic("/", LittleFS, "/www/").setCacheControl("max-age=600");

//*** Change Cache-Control after server setup ***

// During setup - keep a pointer to the handler (SPIFFS)
AsyncStaticWebHandler* handler = &server.serveStatic("/", SPIFFS, "/www/").setCacheControl("max-age=600");

// During setup - keep a pointer to the handler (LittleFS)
AsyncStaticWebHandler* handler = &server.serveStatic("/", LittleFS, "/www/").setCacheControl("max-age=600");

// At a later event - change Cache-Control
handler->setCacheControl("max-age=30");
```

### Specifying Date-Modified header

It is possible to specify Date-Modified header to enable the server to return Not-Modified (304) response for requests
with "If-Modified-Since" header with the same value, instead of responding with the actual file content.

```cpp
// Update the date modified string every time files are updated (SPIFFS)
server.serveStatic("/", SPIFFS, "/www/").setLastModified("Mon, 20 Jun 2016 14:00:00 GMT");

// Update the date modified string every time files are updated (LittleFS)
server.serveStatic("/", LittleFS, "/www/").setLastModified("Mon, 20 Jun 2016 14:00:00 GMT");

//*** Change last modified value at a later stage ***

// During setup - read last modified value from config or EEPROM (SPIFFS)
String date_modified = loadDateModified();
AsyncStaticWebHandler* handler = &server.serveStatic("/", SPIFFS, "/www/");
handler->setLastModified(date_modified);

// During setup - read last modified value from config or EEPROM (LittleFS)
String date_modified = loadDateModified();
AsyncStaticWebHandler* handler = &server.serveStatic("/", LittleFS, "/www/");
handler->setLastModified(date_modified);

// At a later event when files are updated
String date_modified = getNewDateModfied();
saveDateModified(date_modified); // Save for next reset
handler->setLastModified(date_modified);
```

### Specifying Template Processor callback

It is possible to specify template processor for static files. For information on template processor see
[Respond with content coming from a File containing templates](responses.md#respond-with-content-coming-from-a-file-containing-templates).

```cpp
String processor(const String& var)
{
  if(var == "HELLO_FROM_TEMPLATE")
    return F("Hello world!");
  return String();
}

// ...

// For SPIFFS
server.serveStatic("/", SPIFFS, "/www/").setTemplateProcessor(processor);

// For LittleFS
server.serveStatic("/", LittleFS, "/www/").setTemplateProcessor(processor);
```

### Serving static files by custom handling

It may happen your static files are too big and the ESP will crash the request before it sends the whole file.
In that case, you can handle static files with custom file serving through not found handler.

This code below is more-or-less equivalent to this:

```cpp
webServer.serveStatic("/", SPIFFS, STATIC_FILES_PREFIX).setDefaultFile("index.html")
// or
webServer.serveStatic("/", LittleFS, STATIC_FILES_PREFIX).setDefaultFile("index.html")
```

First, declare the handling function:

```cpp
bool handleStaticFile(AsyncWebServerRequest *request) {
  String path = STATIC_FILES_PREFIX + request->url();

  if (path.endsWith("/")) path += F("index.html");

  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";

  // Try SPIFFS first
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    bool gzipped = false;
    if (SPIFFS.exists(pathWithGz)) {
        gzipped = true;
        path += ".gz";
    }

    // TODO serve the file

    return true;
  }

  // Try LittleFS if SPIFFS fails
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) {
    bool gzipped = false;
    if (LittleFS.exists(pathWithGz)) {
        gzipped = true;
        path += ".gz";
    }

    // TODO serve the file

    return true;
  }

  return false;
}
```

And then configure your webserver:

```cpp
webServer.onNotFound([](AsyncWebServerRequest *request) {
  if (handleStaticFile(request)) return;

  request->send(404);
});
```

You may want to try [Respond with file content using a callback and extra headers](responses.md#respond-with-file-content-using-a-callback-and-extra-headers)
For actual serving the file.
