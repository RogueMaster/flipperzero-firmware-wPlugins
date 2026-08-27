# Middleware

## Middleware

### How to use Middleware

Middleware is a way to intercept requests to perform some operations on them, like authentication, authorization, logging, etc and also act on the response headers.

Middleware can either be attached to individual handlers, attached at the server level (thus applied to all handlers), or both.
They will be executed in the order they are attached, and they can stop the request processing by sending a response themselves.

You can have a look at the [examples](https://github.com/ESP32Async/ESPAsyncWebServer/tree/master/examples) for some use cases.

For example, such middleware would handle authentication and set some attributes on the request to make them available for the next middleware and for the handler which will process the request.

```c++
AsyncMiddlewareFunction complexAuth([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
  if (!request->authenticate("user", "password")) {
    return request->requestAuthentication();
  }

  request->setAttribute("user", "Mathieu");
  request->setAttribute("role", "staff");

  next(); // continue processing

  // you can act one the response object
  request->getResponse()->addHeader("X-Rate-Limit", "200");
});
```

**Here are the list of available middlewares:**

- `AsyncMiddlewareFunction`: can convert a lambda function (`ArMiddlewareCallback`) to a middleware
- `AsyncAuthenticationMiddleware`: to handle basic/digest authentication globally or per handler
- `AsyncAuthorizationMiddleware`: to handle authorization globally or per handler
- `AsyncCorsMiddleware`: to handle CORS preflight request globally or per handler
- `AsyncHeaderFilterMiddleware`: to filter out headers from the request
- `AsyncHeaderFreeMiddleware`: to only keep some headers from the request, and remove the others
- `LoggerMiddleware`: to log requests globally or per handler with the same pattern as curl. Will also record request processing time
- `AsyncRateLimitMiddleware`: to limit the number of requests on a windows of time globally or per handler

See the [Middleware example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Middleware/Middleware.ino).

### CORS with AsyncCorsMiddleware

You can handle CORS preflight requests and headers using `AsyncCorsMiddleware`.

```cpp
static AsyncCorsMiddleware cors;

void setup() {
  // Configure CORS
  cors.setOrigin("http://my-client-app.com");
  cors.setMethods("POST, GET, OPTIONS, DELETE");
  cors.setHeaders("X-Custom-Header");
  cors.setAllowCredentials(true);
  cors.setMaxAge(600);

  // Add middleware to server (valid for all handlers)
  server.addMiddleware(&cors);

  // Or add to specific handler only
  // server.on("/api/data", HTTP_GET, handleData).addMiddleware(&cors);
}
```

### Rate Limiting with AsyncRateLimitMiddleware

You can limit the number of requests within a time window using `AsyncRateLimitMiddleware`.

```cpp
static AsyncRateLimitMiddleware rateLimit;

void setup() {
  // Allow maximum 5 requests per 10 seconds
  rateLimit.setMaxRequests(5);
  rateLimit.setWindowSize(10);

  // Apply globally
  // server.addMiddleware(&rateLimit);

  // OR apply to specific sensitive endpoint
  server.on("/login", HTTP_POST, handleLogin).addMiddleware(&rateLimit);
}
```

### Authentication with AsyncAuthenticationMiddleware

**IMPORTANT**: Do not use the `setUsername()` and `setPassword()` methods on the handlers anymore.
They are deprecated.
These methods were causing a copy of the username and password for each handler, which is not efficient.

Now, you can use the `AsyncAuthenticationMiddleware` to handle authentication globally or per handler.

```c++
AsyncAuthenticationMiddleware authMiddleware;

// [...]

authMiddleware.setAuthType(AsyncAuthType::AUTH_DIGEST);
authMiddleware.setRealm("My app name");
authMiddleware.setUsername("admin");
authMiddleware.setPassword("admin");
authMiddleware.setAuthFailureMessage("Authentication failed");
authMiddleware.generateHash(); // optimization to avoid generating the hash at each request

// [...]

server.addMiddleware(&authMiddleware); // globally add authentication to the server

// [...]

myHandler.addMiddleware(&authMiddleware); // add authentication to a specific handler
```

See the [Auth example here](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/examples/arduino/Auth/Auth.ino).

### Migration to Middleware to improve performance and memory usage

- `AsyncEventSource.authorizeConnect(...)` => do not use this method anymore: add a common `AsyncAuthorizationMiddleware` to the handler or server, and make sure to add it AFTER the `AsyncAuthenticationMiddleware` if you use authentication.
- `AsyncWebHandler.setAuthentication(...)` => do not use this method anymore: add a common `AsyncAuthenticationMiddleware` to the handler or server
- `ArUploadHandlerFunction` and `ArBodyHandlerFunction` => these callbacks receiving body data and upload and not calling anymore the authentication code for performance reasons.
  These callbacks can be called multiple times during request parsing, so this is up to the user to now call the `AsyncAuthenticationMiddleware.allowed(request)` if needed and ideally when the method is called for the first time.
  These callbacks are also not triggering the whole middleware chain since they are not part of the request processing workflow (they are not the final handler).
