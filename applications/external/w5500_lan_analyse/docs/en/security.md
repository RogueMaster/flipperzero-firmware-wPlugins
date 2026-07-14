[← Back to documentation index](README.md)

# Security

## Overview

The LAN Tester is a local network diagnostic tool, not an internet-facing service. Security measures focus on protecting against threats that exist on a shared LAN: unauthorized access, path traversal attacks from crafted requests, malicious network responses, and cross-site scripting.

The app does **not** implement TLS/HTTPS -- there are no cryptographic libraries available in the Flipper Zero FAP SDK. All network communication is unencrypted. Use on trusted networks only.

## Per-Component Security Measures

### File Manager (HTTP Server)

The File Manager is the most security-sensitive component since it exposes an HTTP server that accepts requests from the network.

**Authentication token**: each session generates a random 4-character hexadecimal token using hardware RNG. The token is displayed on the Flipper screen and required in every HTTP request as a `?t=XXXX` query parameter. Requests without a valid token receive `403 Forbidden`. The token is not discoverable over the network -- physical access to the Flipper is required to read it.

**Path traversal protection**: all URL paths are validated before file operations. Sequences containing `..` are rejected with `403 Forbidden`. This prevents requests like `GET /../../../etc/passwd` from escaping the SD card root.

**XSS prevention**: filenames are HTML-escaped (replacing `<`, `>`, `&`, `"`, `'` with HTML entities) before rendering in directory listings. This prevents a maliciously-named file from injecting JavaScript into the web UI.

**Upload filename sanitization**: path separator characters (`/` and `\`) are stripped from uploaded filenames. This prevents an upload from writing to a path outside the current directory.

**Content-Disposition safety**: special characters are stripped from filenames in the `Content-Disposition` HTTP header used for downloads. This prevents header injection attacks.

### PXE Server (TFTP)

**Path traversal protection**: TFTP filenames containing `..` or starting with `/` are rejected. This prevents a malicious PXE client from requesting files outside the `/ext/apps_data/lan_tester/pxe/` directory.

### DNS

**Response validation**: DNS responses are checked to ensure they come from the expected DNS server IP address. This mitigates DNS spoofing attacks from other devices on the LAN. If the response source IP doesn't match the server the query was sent to, the response is discarded.

### mDNS

**Recursive pointer depth limit**: mDNS responses can contain DNS name compression pointers that reference other parts of the packet. A malicious response could create recursive pointers to cause a stack overflow. The parser limits pointer following to a depth of 4, preventing this attack.

### MAC Address

**Unique per device**: on first boot, the app generates a random locally-administered MAC address using the Flipper's hardware RNG. This MAC is saved to `mac.conf` on the SD card and reused on subsequent boots. This ensures no two devices share the same default MAC, preventing MAC conflicts on the network.

## ARP Scan in Auto Test

When Auto Test runs with **AT ARP scan** enabled (the default), it sends approximately 254 ARP requests to probe every host on the local /24 subnet. On networks with strict port security policies or Dynamic ARP Inspection (DAI), this burst of ARP traffic may trigger security alerts, cause the switch port to be placed in an error-disabled state, or result in the Flipper's MAC being blocked.

**Recommendation**: disable **AT ARP scan** in Settings when connecting to production or security-hardened networks. The ARP scan step is optional and does not affect the pass/fail verdict of the other Auto Test steps.

## AT DNS Host for Isolated Networks

Auto Test resolves a hostname (default: `google.com`) to verify DNS functionality and then pings the resolved address to confirm internet connectivity. On closed or air-gapped networks where external DNS names are unreachable, the DNS and Internet Ping steps will always fail.

**Recommendation**: change **AT DNS host** in Settings to a hostname that is resolvable within the local network (e.g., an internal server or domain controller) so the DNS and Internet Ping steps produce meaningful results.

## What is NOT Protected

- **No encryption**: all network traffic is plaintext. HTTP, DHCP, TFTP, DNS -- everything is unencrypted.
- **No mutual authentication**: the File Manager token only authenticates the browser to the Flipper, not the other way around.
- **No rate limiting**: the HTTP server does not limit request rates. A flood of requests could slow down the Flipper.
- **No access control**: anyone on the LAN who knows the token can access all files on the SD card.
- **No TFTP authentication**: PXE/TFTP has no authentication mechanism -- any device on the LAN can request boot files.

## Reporting Security Issues

If you find a security vulnerability, please open a GitHub Issue with the `[SECURITY]` tag in the title. Describe the issue, affected code, and potential impact.
