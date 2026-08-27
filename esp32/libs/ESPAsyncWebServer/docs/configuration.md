# Configuration

## Important recommendations for build options

Most of the crashes are caused by improper use or configuration of the AsyncTCP library used for the project.
Here are some recommendations to avoid them and build-time flags you can change.

- `CONFIG_ASYNC_TCP_MAX_ACK_TIME`: defines a timeout for TCP connection to be considered alive when waiting for data.
  In some bad network conditions you might consider increasing it.

- `CONFIG_ASYNC_TCP_QUEUE_SIZE`: defines the length of the queue for events related to connections handling.
  Both the server and AsyncTCP library were optimized to control the queue automatically. Do NOT try blindly increasing the queue size, it does not help you in a way you might think it is. If you receive debug messages about queue throttling, try to optimize your server callbacks code to execute as fast as possible.

- `CONFIG_ASYNC_TCP_RUNNING_CORE`: CPU core thread affinity that runs the queue events handling and executes server callbacks. Default is ANY core, so it means that for dualcore SoCs both cores could handle server activities. If your server's code is too heavy and unoptimized or you see that sometimes
  server might affect other network activities, you might consider to bind it to the same core that runs Arduino code (1) to minimize affect on radio part. Otherwise you can leave the default to let RTOS decide where to run the thread based on priority

- `CONFIG_ASYNC_TCP_STACK_SIZE`: stack size for the thread that runs sever events and callbacks. Default is 16k that is a way too much waste for well-defined short async code or simple static file handling. You might want to cosider reducing it to 4-8k to same RAM usage. If you do not know what this is or not sure about your callback code demands - leave it as default, should be enough even for very hungry callbacks in most cases.

- `ASYNCWEBSERVER_USE_CHUNK_INFLIGHT`: inflight control for chunked responses.
  If you need to serve chunk requests with a really low buffer (which should be avoided), you can set `-D ASYNCWEBSERVER_USE_CHUNK_INFLIGHT=0` to disable the in-flight control.

> [!NOTE]
> This relates to ESP32 only, ESP8266 uses different ESPAsyncTCP lib that does not has this build options

Recommended configurations:

```c++
  -D CONFIG_ASYNC_TCP_MAX_ACK_TIME=5000   // (keep default)
  -D CONFIG_ASYNC_TCP_PRIORITY=10         // (keep default)
  -D CONFIG_ASYNC_TCP_QUEUE_SIZE=64       // (keep default)
  -D CONFIG_ASYNC_TCP_RUNNING_CORE=1      // force async_tcp task to be on same core as Arduino app (default is any core)
  -D CONFIG_ASYNC_TCP_STACK_SIZE=4096     // reduce the stack size (default is 16K)
```

## Important things to remember

- This is fully asynchronous server and as such does not run on the loop thread.
- You can not use yield or delay or any function that uses them inside the callbacks
- The server is smart enough to know when to close the connection and free resources
- You can not send more than one response to a single request
