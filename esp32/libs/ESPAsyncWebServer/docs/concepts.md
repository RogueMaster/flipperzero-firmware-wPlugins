# Concepts

## Principles of operation

### The Async Web server

- Listens for connections
- Wraps the new clients into `Request`
- Keeps track of clients and cleans memory
- Manages `Rewrites` and apply them on the request url
- Manages `Handlers` and attaches them to Requests

### Request Life Cycle

- TCP connection is received by the server
- The connection is wrapped inside `Request` object
- When the request head is received (type, url, get params, http version and host),
  the server goes through all `Rewrites` (in the order they were added) to rewrite the url and inject query parameters,
  next, it goes through all attached `Handlers`(in the order they were added) trying to find one
  that `canHandle` the given request. If none are found, the default(catch-all) handler is attached.
- The rest of the request is received, calling the `handleUpload` or `handleBody` methods of the `Handler` if they are needed (POST+File/Body)
- When the whole request is parsed, the result is given to the `handleRequest` method of the `Handler` and is ready to be responded to
- In the `handleRequest` method, to the `Request` is attached a `Response` object (see below) that will serve the response data back to the client
- When the `Response` is sent, the client is closed and freed from the memory

```mermaid
flowchart TD
    A([TCP Connection]) --> B[New AsyncWebServerRequest]
    B --> C["Parse request line\nMETHOD · URL · GET params · HTTP/version"]
    C --> D[Parse headers line by line]
    D --> E["Apply Rewrites in order\nAll matching rewrites applied sequentially\nEach can rewrite URL and/or inject params"]
    E --> F["Attach Handler\nIterate handlers in order added:\nfilter() → canHandle()\nFirst match wins · fallback to catch-all"]
    F --> G{Body or upload?}
    G -- "Content-Length > 0\nor chunked transfer" --> H["Receive body / file upload\nhandleBody() · handleUpload()"]
    H --> I
    G -- no --> I([PARSE_REQ_END])

    I --> J{mustSkipServerMiddlewares?}
    J -- "no (default)" --> K["Server Middlewares\nexecuted in order added\neach can call next() or short-circuit"]
    J -- "yes (skipServerMiddlewares)" --> L
    K -- "next()" --> L["Handler Middlewares\nexecuted in order added\neach can call next() or short-circuit"]
    K -- "short-circuit" --> N
    L -- "next()" --> M["handleRequest()\nattach Response object"]
    L -- "short-circuit" --> N
    M --> N["_send()\nwrite response to client asynchronously"]
    N --> O([Close connection · free memory])
```

### Rewrites and how do they work

- The `Rewrites` are used to rewrite the request url and/or inject get parameters for a specific request url path.
- All `Rewrites` are evaluated on the request in the order they have been added to the server.
- The `Rewrite` will change the request url only if the request url (excluding get parameters) is fully match
  the rewrite url, and when the optional `Filter` callback return true.
- Setting a `Filter` to the `Rewrite` enables to control when to apply the rewrite, decision can be based on
  request url, http version, request host/port/target host, get parameters or the request client's localIP or remoteIP.
- Two filter callbacks are provided: `ON_AP_FILTER` to execute the rewrite when request is made to the AP interface,
  `ON_STA_FILTER` to execute the rewrite when request is made to the STA interface.
- The `Rewrite` can specify a target url with optional get parameters, e.g. `/to-url?with=params`

### Handlers and how do they work

- The `Handlers` are used for executing specific actions to particular requests
- One `Handler` instance can be attached to any request and lives together with the server
- Setting a `Filter` to the `Handler` enables to control when to apply the handler, decision can be based on
  request url, http version, request host/port/target host, get parameters or the request client's localIP or remoteIP.
- Two filter callbacks are provided: `ON_AP_FILTER` to execute the rewrite when request is made to the AP interface,
  `ON_STA_FILTER` to execute the rewrite when request is made to the STA interface.
- The `canHandle` method is used for handler specific control on whether the requests can be handled
  and for declaring any interesting headers that the `Request` should parse. Decision can be based on request
  method, request url, http version, request host/port/target host and get parameters
- Once a `Handler` is attached to given `Request` (`canHandle` returned true)
  that `Handler` takes care to receive any file/data upload and attach a `Response`
  once the `Request` has been fully parsed
- `Handlers` are evaluated in the order they are attached to the server. The `canHandle` is called only
  if the `Filter` that was set to the `Handler` return true.
- The first `Handler` that can handle the request is selected, not further `Filter` and `canHandle` are called.

### Responses and how do they work

- The `Response` objects are used to send the response data back to the client
- The `Response` object lives with the `Request` and is freed on end or disconnect
- Different techniques are used depending on the response type to send the data in packets
  returning back almost immediately and sending the next packet when this one is received.
  Any time in between is spent to run the user loop and handle other network packets
- Responding asynchronously is probably the most difficult thing for most to understand
- Many different options exist for the user to make responding a background task

### Template processing

- ESPAsyncWebserver contains simple template processing engine.
- Template processing can be added to most response types.
- Currently it supports only replacing template placeholders with actual values. No conditional processing, cycles, etc.
- Placeholders are delimited with `%` symbols. Like this: `%TEMPLATE_PLACEHOLDER%`.
- It works by extracting placeholder name from response text and passing it to user provided function which should return actual value to be used instead of placeholder.
- Since it's user provided function, it is possible for library users to implement conditional processing and cycles themselves.
- Since it's impossible to know the actual response size after template processing step in advance (and, therefore, to include it in response headers), the response becomes [chunked](responses.md#chunked-response).
