// Phone panels. Each is the REAL client (web/dist/index.html) in an iframe, wired
// to the in-page engine through the duck-typed socket the client asks us for.
const MAX_PHONES = 8; // AP_MAX_CONN in the firmware
const WS_MSG_MAX = 512; // WS_MSG_MAX in esp32/hotspot-arcade-fw/hotspot-arcade-fw.ino

const sockets = new Map(); // wsId -> socket object handed to a client

// onTeardown(wsId) is the engine-side disconnect (ha_disconnect); it runs at most once
// per socket no matter which path (an explicit close() call, or the harness removing the
// panel) gets there first — see the `closed` guard below.
export function makeSocket(wsId, onSend, onTeardown) {
  let closed = false;
  const sock = {
    readyState: 1,
    onopen: null,
    onmessage: null,
    onclose: null,
    send(data) {
      // The firmware's onWsEvent (hotspot-arcade-fw.ino) only reads a text frame into
      // its stack buffer when `len < WS_MSG_MAX`, where len is the frame's byte length
      // on the wire; anything at or over that is silently dropped on real hardware.
      // Mirror that here so an oversized message fails the same way in the sim instead
      // of quietly working — otherwise this is exactly the class of drift the
      // simulator exists to catch, just inverted. Measure UTF-8 bytes, not JS's
      // UTF-16 .length, since a message can carry multi-byte nicknames/emoji.
      if (typeof data === "string" && new TextEncoder().encode(data).length >= WS_MSG_MAX) return;
      onSend(wsId, data);
    },
    close() {
      if (closed) return; // already torn down (e.g. the harness got here first) — no-op
      closed = true;
      sock.readyState = 3;
      sockets.delete(wsId);
      if (onTeardown) onTeardown(wsId);
      if (sock.onclose) sock.onclose({});
    },
  };
  sockets.set(wsId, sock);
  // The client assigns its handlers immediately after connect() returns, so defer
  // the open until it has had the chance.
  setTimeout(() => { if (sock.onopen) sock.onopen({}); }, 0);
  return sock;
}

export function deliver(wsId, msg) {
  const sock = sockets.get(wsId);
  if (sock && sock.onmessage) sock.onmessage({ data: msg });
}

export function broadcast(msg) {
  for (const wsId of sockets.keys()) deliver(wsId, msg);
}

export function getSocket(wsId) { return sockets.get(wsId); }
export { MAX_PHONES };
