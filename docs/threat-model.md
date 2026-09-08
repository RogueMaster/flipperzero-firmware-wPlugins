# Threat model

## Assets and trust boundaries

Protected assets are the Mac's network reachability, login/session credentials,
cookies, local files, local-network services, user identity, and the durable
permission decision. The USB cable, every frame field, the Flipper application,
DNS answers, HTTP response metadata, and remote servers are untrusted.

The macOS helper is the policy boundary. A permission grants only bounded HTTPS
proxy requests; it does not grant shell, filesystem, browser session, keychain,
Wi-Fi configuration, Internet Sharing, or arbitrary URL scheme access.

## Principal threats and controls

| Threat | MVP control |
| --- | --- |
| A random serial device is mistaken for a Flipper | IOKit VID/PID filter, varsa beklenen interface numarası, ardından CRC-protected HELLO, exact model, supported version, bounded fields ve non-empty hardware UID doğrulaması; bunlar geçmeden prompt yok |
| A different Flipper inherits permission | Persistent key is bound to device UID, selected protocol version, and helper permission schema version |
| Internet access occurs before consent | Session state rejects request messages until an explicit or stored grant is applied |
| SSRF reaches the Mac or LAN | HTTPS-only URL parser, hostname restrictions, DNS resolution, block-list for non-global IP ranges, and the same validation for every redirect |
| DNS rebinding between validation and connect | Reject any mixed public/private answer and revalidate redirects. Residual URLSession DNS TOCTOU risk remains; see limitations |
| Cookies or credentials leak | Ephemeral URLSession, no shared cookie/credential/cache stores, request-header allow-list; system server-trust challenge uses default handling and every other authentication challenge is rejected |
| Parser memory exhaustion/corruption | Fixed 28-byte header, 512-byte payload maximum, fixed buffers, checked arithmetic, both CRCs before dispatch, resynchronization on magic |
| Response exhausts Flipper RAM | Mac 16 KiB cap, 192-byte chunks, fixed 768-byte display preview, discard/cancel after limits |
| Replay or duplicate request causes repeated network work | Non-zero request IDs USB oturumu boyunca monoton/single-use, per-direction sequence exact; Mac replay durumunu sınırsız küme yerine tek son-ID değeriyle bounded tutar |
| Unique request-ID flood grows helper memory | Only an accepted request advances one scalar `lastAcceptedRequestID`; rejected IDs are not retained in a collection |
| Unplug leaves work running | Serial close/error immediately cancels URLSession task, clears one-time grant and resets session state |
| Diagnostic log leaks content | Log states, IDs, byte counts and error categories; do not log bodies, secrets, full query strings, or request headers |

## Residual risks

- Foundation's URLSession does not expose a supported way to bind a prior
  `getaddrinfo` result to the TLS socket. DNS rebinding between validation and
  connection is reduced but not eliminated. A production-hardening phase should
  use a Network.framework transport that connects to the validated endpoint while
  preserving TLS hostname verification, or an approved outbound relay.
- `getaddrinfo` has no cancellation API. The request-wide deadline prevents a
  late DNS result from starting URLSession work and returns timeout to the
  caller, but an OS-level stuck resolver call can hold the serial validation
  queue until the helper is restarted.
- USB CDC has no cryptographic peer authentication. Physical possession plus the
  hardware UID and explicit local prompt are the MVP identity model. A malicious
  USB device can spoof HELLO fields but still cannot bypass first-use consent.
- A persistent grant authorizes anyone who physically possesses that Flipper.
  The menu provides revocation and the grant becomes invalid on protocol identity
  changes.
- The helper is intentionally not sandboxed in the command-line SwiftPM MVP;
  distribution should add signing, hardened runtime, sandbox entitlements review,
  and notarization.
