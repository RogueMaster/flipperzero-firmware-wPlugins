/*
 * web_origin.h — which pages the dashboard takes commands from (no Arduino
 * dependencies)
 *
 * The WebSocket on :81 and POST /api/cmd change what goes out on the car's
 * buses, and any web page open in a browser on the same network can reach
 * them. A browser names the page a request comes from in its Origin header
 * and the page cannot change that, so commands are only taken from the
 * dashboard's own page: Origin http://<one of our names>. A client that sends
 * no Origin is not a page in a browser and is let through.
 *
 * Our names are the addresses the device has right now (SoftAP, station) and
 * the configured hostname, bare or under a domain that only exists on a local
 * network (.local, .lan, ...). Matching our names, rather than only comparing
 * Origin with Host, is what also stops DNS rebinding: a page on
 * attacker.example whose name has been pointed at the device sends Origin and
 * Host attacker.example, which agree with each other but are not ours. On the
 * SoftAP every name resolves to the device (captive portal), so a page loaded
 * under any other name is always in that position; the dashboard sends such a
 * page load to its own address instead (handle_root()).
 *
 * The CAN stream on :82 is read by the dashboard page across ports, which a
 * browser allows only when the reply grants the page's origin: it grants our
 * own page and nothing else (web_stream_access()).
 *
 * Plain C++ over const char* so it is unit tested on the host
 * (test/test_web_origin.cpp); web_dashboard.cpp feeds it the request headers
 * and the device's current names.
 */
#ifndef WEB_ORIGIN_H
#define WEB_ORIGIN_H

#include <stddef.h>
#include <string.h>

// What the device answers to. An empty string, nullptr or 0.0.0.0 is a name
// it does not have right now.
struct WebOwnNames {
    const char *ap_ip;      // SoftAP address, in AP mode
    const char *sta_ip;     // station address, while connected
    const char *hostname;   // configured station hostname
};

// Domains that do not exist on the public internet, so nobody can point a
// name under them at the device from outside: RFC 6762 .local, RFC 8375
// home.arpa, .internal (reserved by ICANN for private use) and the
// undelegated names home routers hand out. A router domain that is a real
// public domain (fritz.box, ISP domains) is not on the list: a page loaded
// under it is sent to the device's address.
static constexpr const char *WEB_LOCAL_DOMAINS[] = {
    "local", "lan", "home", "internal", "localdomain", "home.arpa",
};

inline bool web_origin_blank(char c) { return c == ' ' || c == '\t'; }

// [s, s + n) equals NUL-terminated b, ignoring ASCII case.
inline bool web_origin_same(const char *s, size_t n, const char *b) {
    auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; };
    size_t i = 0;
    for (; i < n; i++) {
        if (b[i] == '\0' || lower(s[i]) != lower(b[i])) return false;
    }
    return b[i] == '\0';
}

inline bool web_origin_have(const char *name) {
    return name && *name && strcmp(name, "0.0.0.0") != 0;
}

// [host, host + n) is one of our names, with no port or `port`: :80, the port
// the page is served on, unless the request went to another of our servers.
inline bool web_origin_host_ours(const char *host, size_t n, const WebOwnNames &own,
                                 const char *port = ":80") {
    size_t pl = strlen(port);
    if (n >= pl && memcmp(host + n - pl, port, pl) == 0) n -= pl;
    if (n == 0) return false;
    if (web_origin_have(own.ap_ip) && web_origin_same(host, n, own.ap_ip)) return true;
    if (web_origin_have(own.sta_ip) && web_origin_same(host, n, own.sta_ip)) return true;
    if (!web_origin_have(own.hostname)) return false;
    size_t h = strlen(own.hostname);
    if (n < h || !web_origin_same(host, h, own.hostname)) return false;
    if (n == h) return true;
    if (host[h] != '.') return false;
    for (const char *domain : WEB_LOCAL_DOMAINS) {
        if (web_origin_same(host + h + 1, n - h - 1, domain)) return true;
    }
    return false;
}

// Header value with the whitespace around it dropped (legal HTTP; WebServer
// leaves it in on some requests). False when nothing is left.
inline bool web_origin_trim(const char *v, const char **p, size_t *n) {
    if (!v) return false;
    while (web_origin_blank(*v)) v++;
    size_t len = strlen(v);
    while (len > 0 && web_origin_blank(v[len - 1])) len--;
    *p = v;
    *n = len;
    return len > 0;
}

// Host header: one of our names, or no Host at all (HTTP/1.0). `port` as in
// web_origin_host_ours().
inline bool web_host_header_ours(const char *host, const WebOwnNames &own,
                                 const char *port = ":80") {
    const char *p;
    size_t n;
    if (!web_origin_trim(host, &p, &n)) return true;
    return web_origin_host_ours(p, n, own, port);
}

// Origin header: http:// and one of our names. Not "null", not https (the
// device serves no TLS), nothing after the host.
inline bool web_origin_ours(const char *origin, const WebOwnNames &own) {
    static const char scheme[] = "http://";
    const size_t s = sizeof(scheme) - 1;
    const char *p;
    size_t n;
    if (!web_origin_trim(origin, &p, &n) || n <= s || !web_origin_same(p, s, scheme)) return false;
    return web_origin_host_ours(p + s, n - s, own);
}

// WebSocket handshake. Browsers always send Origin on one, so no Origin is a
// client that is not a page: taken. Otherwise it has to be our own page.
inline bool web_ws_origin_ok(const char *origin, const WebOwnNames &own) {
    const char *p;
    size_t n;
    return !web_origin_trim(origin, &p, &n) || web_origin_ours(origin, own);
}

// POST /api/cmd and /sdformat, the same rule as the WebSocket. Browsers send
// Origin on every POST, so no Origin is a client that is not a page; any
// Origin has to be our own page. That refuses "null", which the dashboard
// page never sends but sandboxed frames, data: URLs and file:// pages do,
// with whatever Host they aim at: the CORS preflight that /api/cmd's JSON
// Content-Type forces would be all that stood between them and a command.
inline bool web_post_origin_ok(const char *origin, const WebOwnNames &own) {
    return web_ws_origin_ok(origin, own);
}

// The CAN stream on :82 (http_can_stream.cpp). The dashboard's own page reads
// it from :80, so it gets the stream with a CORS grant for its Origin. Any
// other page is refused outright: without a grant it could not read the
// stream, but taking it would still end a capture in progress and replace its
// filters. No Origin is a client that is not a page (curl, a capture script)
// and is served without a grant. Host has to be ours in every case: a page on
// a rebound name reads the stream as its own origin, and sends no Origin at
// all on that GET.
enum WebStreamAccess {
    WEB_STREAM_REFUSE,
    WEB_STREAM_SERVE,         // no Origin: not a browser page
    WEB_STREAM_SERVE_GRANT,   // the dashboard page: echo its Origin
};

inline WebStreamAccess web_stream_access(const char *origin, const char *host,
                                         const WebOwnNames &own) {
    const char *p;
    size_t n;
    if (!web_host_header_ours(host, own, ":82")) return WEB_STREAM_REFUSE;
    if (!web_origin_trim(origin, &p, &n)) return WEB_STREAM_SERVE;
    return web_origin_ours(origin, own) ? WEB_STREAM_SERVE_GRANT : WEB_STREAM_REFUSE;
}

#endif // WEB_ORIGIN_H
