#pragma once

#include <stdio.h>

#include "../core/ParseUtils.h"
#include "../core/Types.h"
#include "ToolBase.h"

namespace tools {

inline bool busMatches(core::BusId wanted, core::BusId frame_bus) {
  return wanted == core::BusId::BOTH || wanted == frame_bus;
}

// Reads the optional `bus=` token. Sets `out` to `fallback` when absent. Returns false
// when the token is present but invalid, so a typo never selects a different bus.
inline bool parseBusArg(const char *args, core::BusId fallback, core::BusId &out) {
  core::BusId bus = fallback;
  if (!core::parse::parseOptBus(args, "bus", bus)) {
    return false;
  }
  out = bus;
  return true;
}

// Emits an error and returns false when the selected bus hardware is not available,
// so missing or miswired hardware is reported instead of looking like a silent bus.
inline bool requireBusAvailable(ToolHost &host, core::ToolId tool, core::BusId bus) {
  if (host.isBusAvailable(bus)) {
    return true;
  }
  char msg[48];
  snprintf(msg, sizeof(msg), "bus %s unavailable", core::busName(bus));
  host.emitToolEvent(tool, core::ToolEventCode::ERROR, msg);
  return false;
}

// Same as parseBusArg, but transmitting tools need a single concrete bus.
inline bool parseTxBusArg(const char *args, core::BusId fallback, core::BusId &out) {
  core::BusId bus = fallback;
  if (!parseBusArg(args, fallback, bus) || !core::isConcreteBus(bus)) {
    return false;
  }
  out = bus;
  return true;
}

inline void formatFrameHex(const core::CanFrame &frame, char *out, size_t out_len, bool ascii_mode) {
  if (out == nullptr || out_len == 0) {
    return;
  }

  int n = snprintf(out, out_len,
                   "bus=%s id=0x%lX %s %s dlc=%u data=",
                   core::busName(frame.bus),
                   (unsigned long)frame.id,
                   frame.ext ? "ext" : "std",
                   frame.rtr ? "rtr" : "data",
                   frame.dlc);
  if (n < 0) {
    out[0] = '\0';
    return;
  }

  size_t pos = (size_t)n;
  for (uint8_t i = 0; i < frame.dlc && pos < out_len; i++) {
    int w = snprintf(out + pos, out_len - pos, "%02X", frame.data[i]);
    if (w < 0) {
      break;
    }
    pos += (size_t)w;
    if (i + 1 < frame.dlc && pos < out_len) {
      out[pos++] = ' ';
      out[pos] = '\0';
    }
  }

  if (ascii_mode && pos < out_len) {
    int w = snprintf(out + pos, out_len - pos, " ascii=");
    if (w > 0) {
      pos += (size_t)w;
      for (uint8_t i = 0; i < frame.dlc && pos + 1 < out_len; i++) {
        char c = (frame.data[i] >= 32 && frame.data[i] <= 126) ? (char)frame.data[i] : '.';
        out[pos++] = c;
      }
      out[pos] = '\0';
    }
  }
}

} // namespace tools
