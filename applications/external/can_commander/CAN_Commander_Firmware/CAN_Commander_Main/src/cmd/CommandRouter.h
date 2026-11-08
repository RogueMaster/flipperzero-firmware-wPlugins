#pragma once

#include <stdint.h>

#include "../BoardConfig.h"
#include "../core/Types.h"

namespace cmd {

enum class RequestType : uint8_t {
  INVALID = 0,
  HELP,
  STATUS,
  PING,
  GET_INFO,
  VER,
  BUS_SET,
  BUS_SHOW,
  BUS_GET_CFG,
  BUS_SET_FILTER,
  BUS_CLEAR_FILTER,
  SEND_FRAME,
  TOOL_START,
  TOOL_STOP,
  TOOL_STATUS,
  TOOL_CONFIG,
  DBC_CLEAR,
  DBC_ADD,
  DBC_REMOVE,
  DBC_LIST,
  STATS,
  LED_SHOW,
  LED_SET,
  WIFI_SHOW,
  WIFI_CONFIG,
};

struct Request {
  core::Source source = core::Source::CLI;
  RequestType type = RequestType::INVALID;

  // Flipper metadata (valid if source == FLIPPER)
  uint8_t flipper_cmd_id = 0;
  uint8_t flipper_seq = 0;

  // CLI-style arg string
  char args[BoardConfig::CLI_LINE_MAX] = {0};

  // Raw payload for binary commands
  uint8_t payload[BoardConfig::MAX_FLIPPER_PAYLOAD] = {0};
  uint16_t payload_len = 0;
};

class CommandRouter {
 public:
  bool parseCli(const char *line, Request &out) const;
  bool parseFlipper(const core::FlipperPacket &packet, Request &out) const;

 private:
  static bool startsWith(const char *line, const char *prefix);
  static void copyArgs(const char *src, char *dst, uint16_t dst_len);
};

} // namespace cmd
