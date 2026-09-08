# FIBP v1 message registry

This is the compact implementation registry. The normative definitions are in
[`protocol-spec.md`](protocol-spec.md).

```text
01 HELLO                 request_id=0  Flipper -> Mac
02 HELLO_ACK             request_id=0  Mac -> Flipper
03 PERMISSION_STATUS     request_id=0  Mac -> Flipper
04 PERMISSION_REQUIRED   request_id=0  Mac -> Flipper
10 REQUEST_START         request_id>0  Flipper -> Mac
11 REQUEST_HEADER        request_id>0  Flipper -> Mac
12 REQUEST_BODY_CHUNK    request_id>0  Flipper -> Mac
13 REQUEST_END           request_id>0  Flipper -> Mac
20 RESPONSE_START        request_id>0  Mac -> Flipper
21 RESPONSE_HEADER       request_id>0  Mac -> Flipper
22 RESPONSE_BODY_CHUNK   request_id>0  Mac -> Flipper
23 RESPONSE_END          request_id>0  Mac -> Flipper
30 CANCEL                either direction
31 PING                  either direction
32 PONG                  reply direction
7E ERROR                 either direction
7F DISCONNECT            either direction
```

Reserved ranges:

- `0x05...0x0F`: connection control
- `0x14...0x1F`: request extensions
- `0x24...0x2F`: response extensions
- `0x33...0x3F`: transport control
- `0x40...0x7D`: future application messages

