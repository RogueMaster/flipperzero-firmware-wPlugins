# Routing

## URL Matching and Routing

The `AsyncURIMatcher` class provides flexible and powerful URL routing mechanisms.

**Important**: When using plain strings (not `AsyncURIMatcher` objects), the library uses auto-detection (`URIMatchAuto`) which analyzes the URI pattern and applies appropriate matching rules. This is **not** simple exact matching - it combines exact and folder matching by default!

### Auto-Detection Behavior

When you pass a plain string or `const char*` to `server.on()`, the `URIMatchAuto` flag is used, which:

1. **Empty URI**: Matches everything
2. **Ends with `*`**: Becomes prefix match (`URIMatchPrefix`)
3. **Contains `/*.ext`**: Becomes extension match (`URIMatchExtension`)
4. **Starts with `^` and ends with `$`**: Becomes regex match (if regex enabled)
5. **Everything else**: Becomes **both** exact and folder match (`URIMatchPrefixFolder | URIMatchExact`)

This means traditional string-based routes like `server.on("/path", handler)` will match:

- `/path` (exact match)
- `/path/` (folder with trailing slash)
- `/path/anything` (folder match)

But will **NOT** match `/path-suffix` (prefix without folder separator).

### Usage Patterns

#### Traditional String-based Routing (Auto-Detection)

```cpp
// Auto-detection with exact + folder matching
server.on("/api", handler);          // Matches /api AND /api/anything
server.on("/login", handler);        // Matches /login AND /login/sub

// Auto-detection with prefix matching
server.on("/prefix*", handler);      // Matches /prefix, /prefix-test, /prefix/sub

// Auto-detection with extension matching
server.on("/images/*.jpg", handler); // Matches /images/pic.jpg, /images/sub/pic.jpg
```

#### Factory Functions

Using factory functions is clearer and gives you explicit control over matching behavior.

```cpp
// Exact matching only (matches "/login" but NOT "/login/sub")
server.on(AsyncURIMatcher::exact("/login"), handler);

// Prefix matching (matches "/api", "/api/v1", "/api-test")
server.on(AsyncURIMatcher::prefix("/api"), handler);

// Directory matching (matches "/admin/users" but NOT "/admin")
// Requires trailing slash in URL conceptually
server.on(AsyncURIMatcher::dir("/admin"), handler);

// Extension matching
server.on(AsyncURIMatcher::ext("/images/*.jpg"), handler);

// Case insensitive matching
server.on(AsyncURIMatcher::exact("/case", AsyncURIMatcher::CaseInsensitive), handler);
// matches "/case", "/CASE", "/CaSe"

// Regular Expression (requires -D ASYNCWEBSERVER_REGEX)
#ifdef ASYNCWEBSERVER_REGEX
server.on(AsyncURIMatcher::regex("^/user/([0-9]+)$"), handler);
#endif
```

## Param Rewrite With Matching

It is possible to rewrite the request url with parameter match. Here is an example with one parameter:
Rewrite for example "/radio/{frequence}" -> "/radio?f={frequence}"

```cpp
class OneParamRewrite : public AsyncWebRewrite
{
  protected:
    String _urlPrefix;
    int _paramIndex;
    String _paramsBackup;

  public:
  OneParamRewrite(const char* from, const char* to)
    : AsyncWebRewrite(from, to) {

      _paramIndex = _from.indexOf('{');

      if( _paramIndex >=0 && _from.endsWith("}")) {
        _urlPrefix = _from.substring(0, _paramIndex);
        int index = _params.indexOf('{');
        if(index >= 0) {
          _params = _params.substring(0, index);
        }
      } else {
        _urlPrefix = _from;
      }
      _paramsBackup = _params;
  }

  bool match(AsyncWebServerRequest *request) override {
    if(request->url().startsWith(_urlPrefix)) {
      if(_paramIndex >= 0) {
        _params = _paramsBackup + request->url().substring(_paramIndex);
      } else {
        _params = _paramsBackup;
      }
    return true;

    } else {
      return false;
    }
  }
};
```

Usage:

```cpp
  server.addRewrite( new OneParamRewrite("/radio/{frequence}", "/radio?f={frequence}") );
```

See the [Rewrite example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Rewrite/Rewrite.ino).

## Remove handlers and rewrites

Server goes through handlers in same order as they were added. You can't simple add handler with same path to override them.
To remove handler:

```arduino
// save callback for particular URL path
auto& handler = server.on("/some/path", [](AsyncWebServerRequest *request){
  //do something useful
});
// when you don't need handler anymore remove it
server.removeHandler(&handler);

// same with rewrites
server.removeRewrite(&someRewrite);

server.onNotFound([](AsyncWebServerRequest *request){
  request->send(404);
});

// remove server.onNotFound handler
server.onNotFound(NULL);

// remove all rewrites, handlers and onNotFound/onFileUpload/onRequestBody callbacks
server.reset();
```
