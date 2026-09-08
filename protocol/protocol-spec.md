# Flipper USB Internet Bridge Protocol (FIBP) v1.0

Status: MVP wire format  
Transport: USB CDC ACM byte stream  
Byte order: little endian for every multi-byte integer

## 1. Scope

FIBP carries bounded application-layer HTTPS requests between a Flipper Zero FAP
and a user-space macOS helper. It is not an IP tunnel and exposes no Ethernet,
socket, shell, filesystem, DNS, or Wi-Fi credential API to the Flipper.

The CDC transport is an arbitrary byte stream: a USB transfer can contain a
partial frame, one frame, or several frames. Only this format defines boundaries.
The words MUST, MUST NOT, SHOULD, and MAY are normative.

## 2. Frame format

Each frame has a fixed 28-byte header, `payload_length` payload bytes, and a
4-byte trailer. Implementations encode fields byte by byte and MUST NOT cast a
packed struct or perform an unaligned load.

| Offset | Size | Field | Description |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `FIBP`, bytes `46 49 42 50` |
| 4 | 1 | major | Major version, `1` |
| 5 | 1 | minor | Minor version, `0` |
| 6 | 1 | header_length | `28` for v1 |
| 7 | 1 | message_type | Section 4 registry |
| 8 | 2 | flags | Section 3 bit field |
| 10 | 2 | reserved | Sender writes zero |
| 12 | 4 | request_id | `0` for connection control; otherwise non-zero |
| 16 | 4 | sequence | Zero-based sequence in its directional stream |
| 20 | 4 | payload_length | At most 512 in v1 |
| 24 | 4 | header_crc32 | CRC of bytes 0 through 23 |
| 28 | N | payload | Message-specific bytes |
| 28+N | 4 | frame_crc32 | CRC of bytes 0 through 23 followed by payload |

CRC is CRC-32/ISO-HDLC: reflected polynomial `0xEDB88320`, initial value
`0xFFFFFFFF`, final XOR `0xFFFFFFFF`, and check value `CBF43926` for ASCII
`123456789`. CRC detects corruption; it is not authentication.

The maximum encoded frame is 544 bytes. Header CRC is checked before trusting
`payload_length`; a length over 512 is rejected without allocating or waiting for
the claimed body.

## 3. Flags

| Bit | Name | Meaning |
| ---: | --- | --- |
| `0x0001` | `ACK_REQUIRED` | Reserved; ignored by v1 |
| `0x0002` | `FINAL` | Ends the current stream |
| `0x0004` | `TRUNCATED` | Sender stopped at its size limit |
| `0x0008` | `RETRYABLE` | Associated error may be retried |
| all others | reserved | Sender writes zero; receiver ignores in v1 |

## 4. Message types

| Value | Name | Direction |
| ---: | --- | --- |
| `0x01` | `HELLO` | Flipper to Mac |
| `0x02` | `HELLO_ACK` | Mac to Flipper |
| `0x03` | `PERMISSION_STATUS` | Mac to Flipper |
| `0x04` | `PERMISSION_REQUIRED` | Mac to Flipper |
| `0x10` | `REQUEST_START` | Flipper to Mac |
| `0x11` | `REQUEST_HEADER` | Flipper to Mac |
| `0x12` | `REQUEST_BODY_CHUNK` | Flipper to Mac |
| `0x13` | `REQUEST_END` | Flipper to Mac |
| `0x20` | `RESPONSE_START` | Mac to Flipper |
| `0x21` | `RESPONSE_HEADER` | Mac to Flipper |
| `0x22` | `RESPONSE_BODY_CHUNK` | Mac to Flipper |
| `0x23` | `RESPONSE_END` | Mac to Flipper |
| `0x30` | `CANCEL` | Either direction |
| `0x31` | `PING` | Either direction |
| `0x32` | `PONG` | Reply to PING |
| `0x7E` | `ERROR` | Either direction |
| `0x7F` | `DISCONNECT` | Either direction |

Unknown types produce `ERROR(UNSUPPORTED_MESSAGE)` after the frame passes both
CRC checks. An ERROR never triggers another ERROR.

## 5. Primitive encodings

- Text is UTF-8, is never NUL-terminated on wire, and uses the length prefix in
  its schema.
- Text fields reject NUL and disallowed control characters.
- HTTP header names are non-empty ASCII RFC `tchar` tokens. Header values are
  valid UTF-8; HTAB is the only permitted C0 control byte, and NUL, CR, LF,
  every other C0 byte, and DEL are rejected.
- A length is checked against both its containing payload and field limit before
  reading bytes.
- Device hardware IDs are binary in HELLO; UI/log output uses redacted hex.

## 6. Payload schemas

### 6.1 HELLO (`request_id=0`, `sequence=0`)

```
u8  minimum_major
u8  minimum_minor
u8  maximum_major
u8  maximum_minor
u32 capabilities
u16 maximum_rx_payload       # 28...512
u32 maximum_response_bytes
u64 client_nonce
u8  model_length             # 1...16
u8  model[model_length]
u8  name_length              # 1...32
u8  name[name_length]
u8  id_type                  # 1 = Flipper hardware UID
u8  device_id_length         # 1...16
u8  device_id[device_id_length]
u8  app_version_length       # 1...16
u8  app_version[app_version_length]
```

The FAP obtains model/name/UID from exported `furi_hal_version_*` APIs. A hardware
UID is a permission correlation identifier, not cryptographic attestation.

Capability bits:

| Bit | Capability |
| ---: | --- |
| `0x00000001` | HTTPS GET |
| `0x00000002` | HTTPS POST |
| `0x00000004` | Request headers |
| `0x00000008` | Response headers |
| `0x00000010` | Cancellation |

### 6.2 HELLO_ACK (`request_id=0`, `sequence=0`)

```
u8  selected_major
u8  selected_minor
u32 capabilities
u16 maximum_payload          # 28...512
u32 maximum_response_bytes
u64 echoed_client_nonce
u64 server_nonce
```

The negotiated limits are the smaller supported values. The echoed nonce binds
the ACK to the current app run. No common major/minor produces
`ERROR(UNSUPPORTED_VERSION)` and disconnect before any permission prompt.

### 6.3 PERMISSION_REQUIRED

```
u8 reason                    # 0 first use, 1 saved grant invalidated
```

No request is accepted while permission is unresolved.

### 6.4 PERMISSION_STATUS

```
u8 state                     # 0 denied, 1 allowed once, 2 always allowed
u8 reason                    # 0 user, 1 saved grant, 2 revoked
```

Persistent permission binds to `(id_type, device_id, selected version,
permission-schema version)`. Denial or revocation cancels active work.

### 6.5 REQUEST_START (`request_id != 0`, `sequence=0`)

```
u8  method                   # 1 GET, 2 POST
u32 timeout_ms               # 1...30000
u16 url_length               # 1...384
u32 declared_body_length     # 0...4096
u8  declared_header_count    # 0...8
u8  url[url_length]
```

### 6.6 REQUEST_HEADER

```
u8  name_length              # 1...64
u16 value_length             # 0...256
u8  name[name_length]
u8  value[value_length]
```

At most eight headers and 1024 aggregate header bytes are accepted. Only names in
the helper allow-list survive.

### 6.7 REQUEST_BODY_CHUNK

Payload is raw bytes. The accumulated POST body is at most 4096 bytes. GET MUST
have a declared length of zero and no body chunks.

### 6.8 REQUEST_END

Payload is empty and `FINAL` is set. Only after header count and body length match
the declarations may the helper create a network task.

### 6.9 RESPONSE_START (`sequence=0`)

```
u16 http_status
u8  declared_header_count
u8  reserved                 # zero
u32 declared_body_length      # 0xFFFFFFFF when unknown
```

### 6.10 RESPONSE_HEADER

Uses the REQUEST_HEADER encoding. The helper forwards at most eight safe response
headers. It never forwards `set-cookie`.

### 6.11 RESPONSE_BODY_CHUNK

Payload is raw body data, emitted in chunks no larger than 192 bytes. The helper
sends at most 16 KiB. Flipper processes chunks incrementally and keeps only a
fixed 768-byte screen preview.

### 6.12 RESPONSE_END

```
u8  result                    # 0 complete, 1 truncated, 2 cancelled
u32 bytes_sent
```

`FINAL` is set. `TRUNCATED` is also set for result 1.

### 6.13 CANCEL

```
u8 reason                    # 0 user, 1 timeout, 2 disconnect, 3 policy
```

Cancellation is idempotent. Its non-zero request ID selects the work.
Its sequence belongs to the HTTP direction in which it travels: a Flipper-to-Mac
CANCEL uses the next request-stream sequence; a Mac-to-Flipper CANCEL uses the
next response-stream sequence. A CANCEL with a duplicate or gapped sequence has
no cancellation side effect. A malformed CANCEL payload likewise has no
cancellation side effect and does not consume a request/response sequence.

### 6.14 PING and PONG

Payload is an opaque 8-byte token. PONG echoes it exactly. Connection probes use
request ID 0 and do not affect an HTTP request.

### 6.15 ERROR

```
u16 error_code
u8  scope                     # 0 session, 1 request
u8  offending_message_type
u16 detail_length             # 0...128
u8  detail[detail_length]
```

Detail is diagnostic only. Logs must not include URL queries, bodies, request
headers, full UID, or credentials. A sender truncates detail as needed so the
complete ERROR payload does not exceed the negotiated payload limit. Detail is
valid UTF-8 and contains no C0 or DEL control byte.

ERROR uses the sender's connection-control sequence even when `request_id` is
non-zero to identify request scope. After magic/header/version/both CRC checks,
an ERROR carrying the exact expected control sequence consumes that sequence
even when its ERROR payload schema is malformed; the malformed payload has no
semantic side effect and is silently ignored. A duplicate or gapped ERROR, and
an ERROR candidate that fails framing/version/CRC validation, consumes no
sequence. None of these cases may trigger another ERROR.

### 6.16 DISCONNECT

```
u8 reason                    # 0 normal, 1 USB lost, 2 protocol error
```

An invalid reason/payload has no disconnect side effect and consumes no control
sequence. This is best effort; physical USB loss is authoritative.

## 7. Error codes

| Value | Name |
| ---: | --- |
| `0x0001` | `MALFORMED_FRAME` |
| `0x0002` | `BAD_HEADER_CRC` |
| `0x0003` | `BAD_FRAME_CRC` |
| `0x0004` | `PAYLOAD_TOO_LARGE` |
| `0x0005` | `UNSUPPORTED_VERSION` |
| `0x0006` | `UNSUPPORTED_MESSAGE` |
| `0x0007` | `DUPLICATE_SEQUENCE` |
| `0x0008` | `SEQUENCE_GAP` |
| `0x0009` | `INVALID_STATE` |
| `0x000A` | `DUPLICATE_REQUEST_ID` |
| `0x000B` | `PERMISSION_DENIED` |
| `0x000C` | `INVALID_REQUEST` |
| `0x000D` | `SECURITY_BLOCKED` |
| `0x000E` | `TIMEOUT` |
| `0x000F` | `CANCELLED` |
| `0x0010` | `RESPONSE_TOO_LARGE` |
| `0x0011` | `RX_OVERFLOW` |
| `0x0012` | `INTERNAL_ERROR` |
| `0x0013` | `TRANSPORT_LOST` |
| `0x0014` | `NETWORK_FAILURE` |

## 8. Parser recovery

Receiver state is:

```
SEEK_MAGIC -> READ_HEADER -> VALIDATE_HEADER -> READ_PAYLOAD
           -> READ_TRAILER -> VALIDATE_FRAME -> DISPATCH
```

The receiver MUST:

1. Use a rolling magic matcher independent of read boundaries.
2. Validate header length, reserved bytes, header CRC, and payload limit before
   changing semantic state or waiting for payload.
3. Verify frame CRC before dispatch or any network side effect.
4. On invalid input, report a bounded diagnostic and resume magic search without
   terminating the process or reading beyond fixed buffers.
5. Reset a partial frame after one second of no progress.
6. On RX ring overflow, reset parser/request state and report `RX_OVERFLOW` from
   worker context, never crash in a USB callback.
7. Rate-limit ERROR frames; never reply to malformed ERROR with ERROR.

## 9. State and sequencing

```
WAITING_FOR_HELLO -> HANDSHAKEN -> PERMISSION_PENDING -> READY
                                              \-----> DENIED
any state + USB loss -> DISCONNECTED
```

- No HTTP frame is accepted before READY.
- MVP permits one active non-zero request ID.
- Request IDs are single-use for a USB session. Flipper uses a non-zero random
  starting ID and increments; wrap requires a new session.
- HELLO and HELLO_ACK use sequence 0. Each direction then owns a control sequence.
- Permission messages, PING/PONG, ERROR, and DISCONNECT consume that directional
  control sequence. A framing-valid ERROR at the exact expected sequence consumes
  it whether or not its payload is valid; only a valid payload may affect its
  scope. Malformed payload is then ignored. Duplicate or gapped ERROR input is
  dropped without consuming a sequence, and no ERROR case receives an ERROR reply.
- For a non-zero ID, Flipper-to-Mac request and Mac-to-Flipper response are
  independent streams beginning at sequence 0.
- A sequence lower than expected is a duplicate and MUST NOT repeat a side
  effect. Except for the explicit CANCEL and ERROR no-side-effect rules above,
  a higher sequence is a gap and aborts only the affected request.
- A repeated HELLO with the same client nonce is idempotent. A new nonce resets
  connection-scoped state.

## 10. Network security policy

Before a request and again for every redirect, the helper MUST enforce:

- scheme exactly `https`, hostname present, URL at most 384 bytes;
- no username, password, `localhost`, `.localhost`, or `.local` host;
- DNS resolution succeeds and every returned address is globally routable;
- reject unspecified, loopback, link-local, private/unique-local, carrier-grade
  NAT, multicast, reserved, and IPv4-mapped blocked IPv6 ranges;
- at most three redirects;
- ephemeral URLSession with no shared cookies, credential store, or cache;
- default system TLS trust only; no custom certificate or disabled validation;
- reject authentication challenges other than default server trust;
- request-header allow-list; never forward `Authorization`, `Cookie`,
  `Proxy-Authorization`, `Host`, or connection-control headers;
- request body at most 4 KiB and response at most 16 KiB;
- serial loss or permission revocation immediately cancels network work.

DNS pre-validation plus redirect validation reduces SSRF, but URLSession resolves
again. The remaining DNS time-of-check/time-of-use risk is documented in the
threat model.

## 11. Central MVP limits

| Limit | Value |
| --- | ---: |
| Wire payload | 512 B |
| Minimum advertised/negotiated payload | 28 B |
| Encoded frame | 544 B |
| Response chunk | 192 B |
| URL | 384 B |
| Request headers | 8 |
| Header name/value | 64 B / 256 B |
| Aggregate request headers | 1024 B |
| POST body | 4 KiB |
| Response body | 16 KiB |
| Flipper display preview | 768 B |
| Redirects | 3 |
| Request timeout | default 15 s, maximum 30 s |
| Frame assembly timeout | 1 s |
| Logical request assembly timeout | 5 s |
| HELLO deadline | 5 s, followed by slow retries |
| READY-state idle serial session | 30 s |
| READY-state PONG deadline | 2 s |

## 12. Versioning

The 28-byte header is stable for major 1. HELLO advertises a supported range and
HELLO_ACK selects one exact version. An incompatible version is never silently
downgraded. Optional behavior uses capabilities or reserved flags; an incompatible
future header uses a new major/parser branch.
