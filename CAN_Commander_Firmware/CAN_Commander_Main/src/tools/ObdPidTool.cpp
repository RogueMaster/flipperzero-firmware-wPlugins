#include "ObdPidTool.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

static constexpr uint8_t SERVICE_CURRENT_DATA = 0x01;
static constexpr uint8_t SERVICE_DTC_CLEAR = 0x04;
static constexpr uint8_t SERVICE_DTC_PENDING = 0x07;
static constexpr uint8_t SERVICE_VEHICLE_INFO = 0x09;
static constexpr uint8_t SERVICE_DTC_PERMANENT = 0x0A;

static constexpr uint8_t PID_COOLANT_TEMP = 0x05;
static constexpr uint8_t PID_ENGINE_RPM = 0x0C;
static constexpr uint8_t PID_VEHICLE_SPEED = 0x0D;
static constexpr uint8_t PID_THROTTLE_POSITION = 0x11;
static constexpr uint8_t PID_ENGINE_LOAD = 0x04;
static constexpr uint8_t PID_FUEL_LEVEL = 0x2F;
static constexpr uint8_t PID_INTAKE_TEMP = 0x0F;
static constexpr uint8_t PID_BAROMETRIC_PRESSURE = 0x33;
static constexpr uint8_t PID_ODOMETER = 0xA6;
static constexpr uint8_t PID_VIN = 0x02;

static constexpr uint32_t ONE_SHOT_TIMEOUT_MS = 1800;
static constexpr uint32_t DTC_MODULE_WINDOW_MS = 260;

struct PidAlias {
  const char *name;
  uint8_t pid;
};

static const PidAlias kPidAliases[] = {
    {"coolant", PID_COOLANT_TEMP},
    {"coolant_temp", PID_COOLANT_TEMP},
    {"temp", PID_COOLANT_TEMP},
    {"rpm", PID_ENGINE_RPM},
    {"engine_rpm", PID_ENGINE_RPM},
    {"speed", PID_VEHICLE_SPEED},
    {"vehicle_speed", PID_VEHICLE_SPEED},
    {"throttle", PID_THROTTLE_POSITION},
    {"throttle_position", PID_THROTTLE_POSITION},
    {"load", PID_ENGINE_LOAD},
    {"engine_load", PID_ENGINE_LOAD},
    {"fuel", PID_FUEL_LEVEL},
    {"fuel_level", PID_FUEL_LEVEL},
    {"iat", PID_INTAKE_TEMP},
    {"intake_temp", PID_INTAKE_TEMP},
    {"baro", PID_BAROMETRIC_PRESSURE},
    {"barometric_pressure", PID_BAROMETRIC_PRESSURE},
    {"odometer", PID_ODOMETER},
};

static bool equalsIgnoreCase(const char *a, const char *b) {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  while (*a != '\0' && *b != '\0') {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return false;
    }
    ++a;
    ++b;
  }
  return (*a == '\0' && *b == '\0');
}

static bool parseMode01Pid(const char *token, uint8_t &pid_out) {
  if (token == nullptr || token[0] == '\0') {
    return false;
  }

  uint32_t parsed = 0;
  if (core::parse::parseU32Text(token, parsed, 16) && parsed <= 0xFF) {
    pid_out = (uint8_t)parsed;
    return true;
  }

  for (size_t i = 0; i < (sizeof(kPidAliases) / sizeof(kPidAliases[0])); i++) {
    if (equalsIgnoreCase(token, kPidAliases[i].name)) {
      pid_out = kPidAliases[i].pid;
      return true;
    }
  }

  return false;
}

static const char *pidShortName(uint8_t pid) {
  switch (pid) {
    case PID_COOLANT_TEMP: return "coolant";
    case PID_ENGINE_RPM: return "rpm";
    case PID_VEHICLE_SPEED: return "speed";
    case PID_THROTTLE_POSITION: return "throttle";
    case PID_ENGINE_LOAD: return "load";
    case PID_FUEL_LEVEL: return "fuel";
    case PID_INTAKE_TEMP: return "iat";
    case PID_BAROMETRIC_PRESSURE: return "baro";
    case PID_ODOMETER: return "odometer";
    default: return "custom";
  }
}

ObdPidTool::ObdPidTool()
    : bus_(core::BusId::CAN0),
      request_kind_(RequestKind::MODE01_PID),
      pid_(PID_ENGINE_RPM),
      service_(SERVICE_CURRENT_DATA),
      expected_resp_service_(0x41),
      request_id_(0x7DF),
      one_shot_(false),
      request_sent_(false),
      awaiting_response_(false),
      interval_ms_(250),
      next_send_ms_(0),
      request_sent_ms_(0),
      dtc_all_index_(0),
      dtc_all_running_(false),
      dtc_any_response_(false),
      dtc_last_response_ms_(0),
      dtc_responder_mask_(0),
      send_failures_(0) {
  resetIsoTp();
}

const char *ObdPidTool::requestName() const {
  switch (request_kind_) {
    case RequestKind::MODE01_PID: return pidShortName(pid_);
    case RequestKind::VIN: return "vin";
    case RequestKind::DTC_STORED: return "dtc_stored";
    case RequestKind::DTC_PENDING: return "dtc_pending";
    case RequestKind::DTC_PERMANENT: return "dtc_permanent";
    case RequestKind::DTC_CLEAR: return "clear_dtcs";
    case RequestKind::DTC_ALL: return "dtc_all";
    default: return "unknown";
  }
}

const char *ObdPidTool::dtcLabelForCurrentService() const {
  if (expected_resp_service_ == 0x47) {
    return "pending";
  }
  if (expected_resp_service_ == 0x4A) {
    return "permanent";
  }
  return "stored";
}

void ObdPidTool::resetIsoTp() {
  memset(isotp_rx_, 0, sizeof(isotp_rx_));
}

ObdPidTool::IsoTpRx *ObdPidTool::isoTpFor(uint32_t ecu_id) {
  if (ecu_id < kFirstResponseId || ecu_id >= kFirstResponseId + kMaxResponders) {
    return nullptr;
  }
  return &isotp_rx_[ecu_id - kFirstResponseId];
}

bool ObdPidTool::start(const char *args, ToolHost &host) {
  core::BusId bus = core::BusId::CAN0;
  if (!parseTxBusArg(args, core::BusId::CAN0, bus)) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "invalid bus (use can0 or can1)");
    return false;
  }
  if (!requireBusAvailable(host, id(), bus)) {
    return false;
  }

  uint32_t request_id = 0x7DF;
  if (!core::parse::parseOptU32(args, "req_id", request_id, 16) || request_id > core::kMaxStdCanId) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "invalid req_id (11-bit hex, e.g. 7DF or 7E0)");
    return false;
  }

  uint32_t interval_ms = 250;
  if (!core::parse::parseOptU32(args, "interval_ms", interval_ms, 10) || interval_ms > kMaxIntervalMs) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "invalid interval_ms");
    return false;
  }

  RequestKind kind = RequestKind::MODE01_PID;
  uint8_t pid = PID_ENGINE_RPM;
  char pid_arg[32] = {0};
  if (!core::parse::parseOptString(args, "pid", pid_arg, sizeof(pid_arg))) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "invalid pid");
    return false;
  }
  if (pid_arg[0] != '\0') {
    if (equalsIgnoreCase(pid_arg, "vin")) {
      kind = RequestKind::VIN;
    } else if (equalsIgnoreCase(pid_arg, "dtc") ||
               equalsIgnoreCase(pid_arg, "dtc_all")) {
      kind = RequestKind::DTC_ALL;
    } else if (equalsIgnoreCase(pid_arg, "dtc_stored") ||
               equalsIgnoreCase(pid_arg, "stored_dtc") ||
               equalsIgnoreCase(pid_arg, "stored_dtcs")) {
      kind = RequestKind::DTC_STORED;
    } else if (equalsIgnoreCase(pid_arg, "dtc_pending") ||
               equalsIgnoreCase(pid_arg, "pending_dtc") ||
               equalsIgnoreCase(pid_arg, "pending_dtcs")) {
      kind = RequestKind::DTC_PENDING;
    } else if (equalsIgnoreCase(pid_arg, "dtc_permanent") ||
               equalsIgnoreCase(pid_arg, "permanent_dtc") ||
               equalsIgnoreCase(pid_arg, "permanent_dtcs")) {
      kind = RequestKind::DTC_PERMANENT;
    } else if (equalsIgnoreCase(pid_arg, "clear_dtc") ||
               equalsIgnoreCase(pid_arg, "clear_codes") ||
               equalsIgnoreCase(pid_arg, "clear_dtcs") ||
               equalsIgnoreCase(pid_arg, "clear_mil") ||
               equalsIgnoreCase(pid_arg, "cel_reset")) {
      kind = RequestKind::DTC_CLEAR;
    } else if (!parseMode01Pid(pid_arg, pid)) {
      host.emitToolEvent(id(),
                         core::ToolEventCode::ERROR,
                         "invalid pid: use hex/name or vin/dtc_all/dtc_stored/dtc_pending/dtc_permanent/clear_dtcs");
      return false;
    }
  }

  bus_ = bus;
  request_id_ = request_id;
  request_kind_ = kind;
  pid_ = pid;

  service_ = SERVICE_CURRENT_DATA;
  expected_resp_service_ = 0x41;
  one_shot_ = false;
  interval_ms_ = 250;

  switch (request_kind_) {
    case RequestKind::MODE01_PID:
      service_ = SERVICE_CURRENT_DATA;
      expected_resp_service_ = 0x41;
      one_shot_ = false;
      interval_ms_ = interval_ms < 10 ? 10 : interval_ms;
      break;
    case RequestKind::VIN:
      service_ = SERVICE_VEHICLE_INFO;
      pid_ = PID_VIN;
      expected_resp_service_ = 0x49;
      one_shot_ = true;
      break;
    case RequestKind::DTC_STORED:
      service_ = 0x03;
      expected_resp_service_ = 0x43;
      one_shot_ = true;
      break;
    case RequestKind::DTC_PENDING:
      service_ = SERVICE_DTC_PENDING;
      expected_resp_service_ = 0x47;
      one_shot_ = true;
      break;
    case RequestKind::DTC_PERMANENT:
      service_ = SERVICE_DTC_PERMANENT;
      expected_resp_service_ = 0x4A;
      one_shot_ = true;
      break;
    case RequestKind::DTC_CLEAR:
      service_ = SERVICE_DTC_CLEAR;
      expected_resp_service_ = 0x44;
      one_shot_ = true;
      break;
    case RequestKind::DTC_ALL:
      service_ = 0x03;
      expected_resp_service_ = 0x43;
      one_shot_ = true;
      break;
  }

  request_sent_ = false;
  awaiting_response_ = false;
  request_sent_ms_ = 0;
  dtc_all_index_ = 0;
  dtc_all_running_ = (request_kind_ == RequestKind::DTC_ALL);
  dtc_any_response_ = false;
  dtc_last_response_ms_ = 0;
  dtc_responder_mask_ = 0;
  send_failures_ = 0;
  next_send_ms_ = host.nowMs();
  resetIsoTp();
  return true;
}

bool ObdPidTool::configure(const char *args, ToolHost &host) {
  uint32_t request = 0;
  if (core::parse::parseU32(args, "request", request, 10) && request == 1) {
    request_sent_ = false;
    awaiting_response_ = false;
    request_sent_ms_ = 0;
    dtc_all_index_ = 0;
    dtc_all_running_ = (request_kind_ == RequestKind::DTC_ALL);

    if (request_kind_ == RequestKind::DTC_ALL) {
      service_ = 0x03;
      expected_resp_service_ = 0x43;
    }

    dtc_any_response_ = false;
    dtc_last_response_ms_ = 0;
    dtc_responder_mask_ = 0;
    send_failures_ = 0;
    next_send_ms_ = host.nowMs();
    resetIsoTp();
    host.emitToolEvent(id(), core::ToolEventCode::INFO, "obd request queued");
    return true;
  }

  if (request_kind_ == RequestKind::MODE01_PID) {
    uint32_t interval_ms = 0;
    if (core::parse::parseU32(args, "interval_ms", interval_ms, 10) && interval_ms <= kMaxIntervalMs) {
      interval_ms_ = interval_ms < 10 ? 10 : interval_ms;
      host.emitToolEvent(id(), core::ToolEventCode::INFO, "obd interval updated");
      return true;
    }
  }

  uint32_t req_id = 0;
  if (core::parse::parseU32(args, "req_id", req_id, 16) && req_id <= core::kMaxStdCanId) {
    request_id_ = req_id;
    host.emitToolEvent(id(), core::ToolEventCode::INFO, "obd req_id updated");
    return true;
  }

  return false;
}

void ObdPidTool::stop(ToolHost &host) {
  (void)host;
  awaiting_response_ = false;
  request_sent_ = false;
  dtc_all_running_ = false;
  dtc_any_response_ = false;
  dtc_last_response_ms_ = 0;
  dtc_responder_mask_ = 0;
  resetIsoTp();
}

bool ObdPidTool::sendCurrentRequest(ToolHost &host, uint32_t now_ms) {
  core::CanFrame req{};
  req.ts_ms = now_ms;
  req.bus = bus_;
  req.id = request_id_;
  req.ext = false;
  req.rtr = false;
  req.dlc = 8;
  memset(req.data, 0xCC, sizeof(req.data));

  if (request_kind_ == RequestKind::MODE01_PID || request_kind_ == RequestKind::VIN) {
    req.data[0] = 0x02;
    req.data[1] = service_;
    req.data[2] = pid_;
  } else {
    req.data[0] = 0x01;
    req.data[1] = service_;
  }

  return host.sendCanFrame(req);
}

void ObdPidTool::tick(ToolHost &host) {
  const uint32_t now = host.nowMs();
  const bool functional_request = (request_id_ == 0x7DF);

  if (request_kind_ == RequestKind::DTC_ALL) {
    if (!dtc_all_running_) {
      return;
    }

    if (awaiting_response_) {
      if (dtc_any_response_) {
        if (functional_request) {
          if ((uint32_t)(now - request_sent_ms_) > ONE_SHOT_TIMEOUT_MS) {
            // Keep listening for the full one-shot timeout on functional requests.
            // Some modules respond later than others.
            advanceDtcAll(host, false);
          }
        } else if ((uint32_t)(now - dtc_last_response_ms_) > DTC_MODULE_WINDOW_MS) {
          advanceDtcAll(host, false);
        }
      } else if ((uint32_t)(now - request_sent_ms_) > ONE_SHOT_TIMEOUT_MS) {
        advanceDtcAll(host, true);
      }
      return;
    }

    if ((int32_t)(now - next_send_ms_) < 0) {
      return;
    }
    if (!sendCurrentRequest(host, now)) {
      send_failures_++;
      if (send_failures_ >= kMaxSendFailures) {
        char msg[80];
        snprintf(msg, sizeof(msg), "dtc_%s request send failed; skipping", dtcLabelForCurrentService());
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, msg);
        advanceDtcAll(host, false);
        next_send_ms_ = now;
        return;
      }
      host.emitToolEvent(id(), core::ToolEventCode::ERROR, "obd request send failed; retrying");
      next_send_ms_ = now + kSendRetryMs;
      return;
    }

    send_failures_ = 0;
    request_sent_ = true;
    awaiting_response_ = true;
    request_sent_ms_ = now;
    dtc_any_response_ = false;
    dtc_last_response_ms_ = 0;
    dtc_responder_mask_ = 0;

    char msg[96];
    snprintf(msg, sizeof(msg), "obd request sent: dtc_%s", dtcLabelForCurrentService());
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return;
  }

  if (one_shot_) {
    const bool single_dtc_query = (request_kind_ == RequestKind::DTC_STORED ||
                                   request_kind_ == RequestKind::DTC_PENDING ||
                                   request_kind_ == RequestKind::DTC_PERMANENT);

    if (request_sent_ && awaiting_response_) {
      if (single_dtc_query) {
        if (dtc_any_response_) {
          const bool complete = functional_request
                                    ? ((uint32_t)(now - request_sent_ms_) > ONE_SHOT_TIMEOUT_MS)
                                    : ((uint32_t)(now - dtc_last_response_ms_) > DTC_MODULE_WINDOW_MS);
          if (complete) {
            awaiting_response_ = false;
            uint8_t responders = 0;
            uint16_t mask = dtc_responder_mask_;
            while (mask != 0) {
              responders += (uint8_t)(mask & 1u);
              mask >>= 1;
            }
            char msg[80];
            snprintf(msg,
                     sizeof(msg),
                     "dtc_%s complete responders=%u",
                     dtcLabelForCurrentService(),
                     (unsigned)responders);
            host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);
          }
        } else if ((uint32_t)(now - request_sent_ms_) > ONE_SHOT_TIMEOUT_MS) {
          awaiting_response_ = false;
          host.emitToolEvent(id(), core::ToolEventCode::WARNING, "obd request timeout (no response)");
        }
      } else if ((uint32_t)(now - request_sent_ms_) > ONE_SHOT_TIMEOUT_MS) {
        awaiting_response_ = false;
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, "obd request timeout (no response)");
      }
      return;
    }
    if (request_sent_ && !awaiting_response_) {
      return;
    }
  } else {
    if ((int32_t)(now - next_send_ms_) < 0) {
      return;
    }
  }

  if (!sendCurrentRequest(host, now)) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "obd request send failed");
    if (!one_shot_) {
      next_send_ms_ = now + interval_ms_;
    } else {
      request_sent_ = true;
    }
    return;
  }

  resetIsoTp();
  if (one_shot_) {
    request_sent_ = true;
    awaiting_response_ = true;
    request_sent_ms_ = now;
    dtc_any_response_ = false;
    dtc_last_response_ms_ = 0;
    dtc_responder_mask_ = 0;

    char msg[96];
    snprintf(msg, sizeof(msg), "obd request sent: %s", requestName());
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
  } else {
    next_send_ms_ = now + interval_ms_;
  }
}

void ObdPidTool::formatDtcCode(uint8_t hi, uint8_t lo, char *out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return;
  }

  static const char kSystemChars[] = {'P', 'C', 'B', 'U'};
  const char sys = kSystemChars[(hi >> 6) & 0x03];
  const uint8_t d1 = (uint8_t)((hi >> 4) & 0x03);
  const uint8_t d2 = (uint8_t)(hi & 0x0F);
  const uint8_t d3 = (uint8_t)((lo >> 4) & 0x0F);
  const uint8_t d4 = (uint8_t)(lo & 0x0F);

  snprintf(out, out_len, "%c%01X%01X%01X%01X", sys, d1, d2, d3, d4);
}

void ObdPidTool::advanceDtcAll(ToolHost &host, bool due_to_timeout) {
  if (!dtc_all_running_) {
    return;
  }

  if (due_to_timeout) {
    char msg[96];
    snprintf(msg, sizeof(msg), "dtc_%s request timeout", dtcLabelForCurrentService());
    host.emitToolEvent(id(), core::ToolEventCode::WARNING, msg);
  }

  dtc_all_index_++;
  awaiting_response_ = false;
  send_failures_ = 0;
  dtc_any_response_ = false;
  dtc_last_response_ms_ = 0;
  dtc_responder_mask_ = 0;
  resetIsoTp();

  if (dtc_all_index_ >= 3) {
    dtc_all_running_ = false;
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "dtc_all complete");
    return;
  }

  if (dtc_all_index_ == 1) {
    service_ = SERVICE_DTC_PENDING;
    expected_resp_service_ = 0x47;
  } else {
    service_ = SERVICE_DTC_PERMANENT;
    expected_resp_service_ = 0x4A;
  }
}

void ObdPidTool::emitParsedMode01(const uint8_t *payload, uint16_t payload_len, ToolHost &host) {
  char msg[128];

  if (pid_ == PID_COOLANT_TEMP && payload_len >= 3) {
    const int temp = (int)payload[2] - 40;
    snprintf(msg, sizeof(msg), "Coolant Temp: %d C", temp);
  } else if (pid_ == PID_ENGINE_RPM && payload_len >= 4) {
    const int rpm = (((int)payload[2] * 256) + payload[3]) / 4;
    snprintf(msg, sizeof(msg), "Engine RPM: %d RPM", rpm);
  } else if (pid_ == PID_VEHICLE_SPEED && payload_len >= 3) {
    snprintf(msg, sizeof(msg), "Vehicle Speed: %u km/h", payload[2]);
  } else if (pid_ == PID_THROTTLE_POSITION && payload_len >= 3) {
    snprintf(msg, sizeof(msg), "Throttle Position: %u%%", (unsigned)((payload[2] * 100U) / 255U));
  } else if (pid_ == PID_ENGINE_LOAD && payload_len >= 3) {
    snprintf(msg, sizeof(msg), "Engine Load: %u%%", (unsigned)((payload[2] * 100U) / 255U));
  } else if (pid_ == PID_FUEL_LEVEL && payload_len >= 3) {
    snprintf(msg, sizeof(msg), "Fuel Level: %u%%", (unsigned)((payload[2] * 100U) / 255U));
  } else if (pid_ == PID_INTAKE_TEMP && payload_len >= 3) {
    snprintf(msg, sizeof(msg), "Intake Temp: %d C", (int)payload[2] - 40);
  } else if (pid_ == PID_BAROMETRIC_PRESSURE && payload_len >= 3) {
    snprintf(msg, sizeof(msg), "Barometric Pressure: %u kPa", payload[2]);
  } else if (pid_ == PID_ODOMETER && payload_len >= 6) {
    unsigned long odometer_km = ((unsigned long)payload[2] << 24) |
                                ((unsigned long)payload[3] << 16) |
                                ((unsigned long)payload[4] << 8) |
                                ((unsigned long)payload[5]);
    odometer_km /= 10;
    const float odometer_mi = odometer_km * 0.621371f;
    snprintf(msg, sizeof(msg), "Odometer: %lu km | %.2f miles", odometer_km, odometer_mi);
  } else {
    const uint8_t a = payload_len > 2 ? payload[2] : 0;
    const uint8_t b = payload_len > 3 ? payload[3] : 0;
    const uint8_t c = payload_len > 4 ? payload[4] : 0;
    const uint8_t d = payload_len > 5 ? payload[5] : 0;
    const uint8_t e = payload_len > 6 ? payload[6] : 0;
    snprintf(msg,
             sizeof(msg),
             "PID 0x%02X data=%02X %02X %02X %02X %02X",
             pid_,
             a,
             b,
             c,
             d,
             e);
  }

  host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
}

void ObdPidTool::emitParsedVin(const uint8_t *payload, uint16_t payload_len, ToolHost &host) {
  if (payload_len < 4) {
    host.emitToolEvent(id(), core::ToolEventCode::WARNING, "VIN response too short");
    return;
  }

  char vin[18] = {0};
  size_t vin_len = 0;

  for (uint16_t i = 3; i < payload_len && vin_len < 17; i++) {
    const uint8_t c = payload[i];
    if (c >= 32 && c <= 126) {
      vin[vin_len++] = (char)c;
    }
  }
  vin[vin_len] = '\0';

  if (vin_len == 0) {
    host.emitToolEvent(id(), core::ToolEventCode::WARNING, "VIN decode failed");
    return;
  }

  char msg[96];
  snprintf(msg, sizeof(msg), "VIN: %s", vin);
  host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
}

void ObdPidTool::emitParsedDtc(const uint8_t *payload, uint16_t payload_len, uint32_t ecu_resp_id, ToolHost &host) {
  const char *label = dtcLabelForCurrentService();

  // ISO 15765-4 (CAN) response: [service+0x40] [DTC count] [hi lo] * count
  if (payload_len < 2) {
    char msg[80];
    snprintf(msg, sizeof(msg), "%s DTC response too short ecu=0x%lX", label, (unsigned long)ecu_resp_id);
    host.emitToolEvent(id(), core::ToolEventCode::WARNING, msg);
    return;
  }

  const uint8_t count = payload[1];
  const uint16_t available = (uint16_t)((payload_len - 2) / 2);
  const uint16_t pairs = count < available ? count : available;
  if (count > available) {
    char warn[96];
    snprintf(warn,
             sizeof(warn),
             "%s DTC response truncated ecu=0x%lX count=%u received=%u",
             label,
             (unsigned long)ecu_resp_id,
             (unsigned)count,
             (unsigned)available);
    host.emitToolEvent(id(), core::ToolEventCode::WARNING, warn);
  }

  // Format: "<stored|pending|permanent> DTCs: P0301 P0420 ecu=0x7E8". The Flipper
  // dashboard reads the codes after "DTCs:" (its parse buffer is 96 bytes), so the
  // ECU id goes last and the line stays under 96 characters; extra codes are
  // reported in a "+N more" line.
  char ecu_suffix[16];
  snprintf(ecu_suffix, sizeof(ecu_suffix), " ecu=0x%lX", (unsigned long)ecu_resp_id);
  char msg[96];
  int n = snprintf(msg, sizeof(msg), "%s DTCs:", label);
  size_t pos = n > 0 ? (size_t)n : 0;
  const size_t codes_limit = sizeof(msg) - strlen(ecu_suffix) - 1;

  uint16_t total = 0;
  uint16_t listed = 0;

  for (uint16_t p = 0; p < pairs; p++) {
    const uint16_t i = (uint16_t)(2 + p * 2);
    const uint8_t hi = payload[i];
    const uint8_t lo = payload[i + 1];
    if (hi == 0 && lo == 0) {
      continue;
    }

    total++;
    char code[8];
    formatDtcCode(hi, lo, code, sizeof(code));

    if (pos + 1 + strlen(code) <= codes_limit) {
      const int w = snprintf(msg + pos, sizeof(msg) - pos, " %s", code);
      if (w > 0) {
        pos += (size_t)w;
        listed++;
      }
    }
  }

  if (total == 0) {
    char none[80];
    snprintf(none, sizeof(none), "%s DTCs: none%s", label, ecu_suffix);
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, none);
    return;
  }

  snprintf(msg + pos, sizeof(msg) - pos, "%s", ecu_suffix);
  host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);

  if (listed < total) {
    char extra[64];
    snprintf(extra, sizeof(extra), "%s DTCs: +%u more", label, (unsigned)(total - listed));
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, extra);
  }
}

void ObdPidTool::processPayload(const uint8_t *payload,
                                uint16_t payload_len,
                                const core::CanFrame &frame,
                                ToolHost &host) {
  if (payload == nullptr || payload_len == 0) {
    return;
  }

  if (payload[0] == 0x7F) {
    if (payload_len >= 3 && payload[1] == service_) {
      const bool is_dtc_query = (request_kind_ == RequestKind::DTC_STORED ||
                                 request_kind_ == RequestKind::DTC_PENDING ||
                                 request_kind_ == RequestKind::DTC_PERMANENT ||
                                 request_kind_ == RequestKind::DTC_ALL);
      const bool functional_dtc_query = is_dtc_query && (request_id_ == 0x7DF);
      const uint8_t nrc = payload[2];
      char msg[96];
      snprintf(msg,
               sizeof(msg),
               "OBD negative response ecu=0x%lX service=0x%02X nrc=0x%02X",
               (unsigned long)frame.id,
               payload[1],
               nrc);

      const core::ToolEventCode code = (nrc == 0x78) ? core::ToolEventCode::WARNING : core::ToolEventCode::ERROR;
      host.emitToolEvent(id(), code, msg);

      if (nrc == 0x78) {
        // Response pending: wait for a follow-up response.
        return;
      }

      if (!functional_dtc_query) {
        if (request_kind_ == RequestKind::DTC_ALL) {
          // Terminal rejection of this service: move on instead of re-sending it.
          advanceDtcAll(host, false);
        } else {
          awaiting_response_ = false;
        }
      }
    }
    return;
  }

  if (payload[0] != expected_resp_service_) {
    return;
  }

  if (request_kind_ == RequestKind::MODE01_PID) {
    if (payload_len < 3 || payload[1] != pid_) {
      return;
    }
    emitParsedMode01(payload, payload_len, host);
    return;
  }

  if (request_kind_ == RequestKind::VIN) {
    if (payload_len < 3 || payload[1] != PID_VIN) {
      return;
    }
    emitParsedVin(payload, payload_len, host);
    awaiting_response_ = false;
    return;
  }

  if (request_kind_ == RequestKind::DTC_STORED ||
      request_kind_ == RequestKind::DTC_PENDING ||
      request_kind_ == RequestKind::DTC_PERMANENT ||
      request_kind_ == RequestKind::DTC_ALL) {
    emitParsedDtc(payload, payload_len, frame.id, host);
    dtc_any_response_ = true;
    dtc_last_response_ms_ = host.nowMs();
    if (frame.id >= 0x7E8 && frame.id <= 0x7EF) {
      const uint8_t bit = (uint8_t)(frame.id - 0x7E8);
      dtc_responder_mask_ = (uint16_t)(dtc_responder_mask_ | (uint16_t)(1u << bit));
    }
    return;
  }

  if (request_kind_ == RequestKind::DTC_CLEAR) {
    char msg[96];
    snprintf(msg,
             sizeof(msg),
             "clear DTC acknowledged by ECU 0x%lX",
             (unsigned long)frame.id);
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);
    awaiting_response_ = false;
    return;
  }
}

bool ObdPidTool::sendFlowControl(const core::CanFrame &first_frame, ToolHost &host) {
  core::CanFrame fc{};
  fc.ts_ms = host.nowMs();
  fc.bus = first_frame.bus;
  fc.id = request_id_;
  fc.ext = false;
  fc.rtr = false;
  fc.dlc = 8;
  memset(fc.data, 0, sizeof(fc.data));
  fc.data[0] = 0x30; // Continue To Send

  const bool primary_ok = host.sendCanFrame(fc);

  // For functional requests (7DF), some stacks expect FC to physical ECU ID (e.g. 7E0).
  if (request_id_ == 0x7DF && first_frame.id >= 8) {
    const uint32_t physical_fc_id = first_frame.id - 8;
    if (physical_fc_id != fc.id) {
      fc.id = physical_fc_id;
      (void)host.sendCanFrame(fc);
    }
  }

  return primary_ok;
}

void ObdPidTool::onSingleFrame(const core::CanFrame &frame, ToolHost &host) {
  const uint8_t payload_len = (uint8_t)(frame.data[0] & 0x0F);
  if (payload_len == 0 || payload_len > 7) {
    return;
  }

  uint8_t payload[8] = {0};
  const uint8_t available = (frame.dlc > 1) ? (uint8_t)(frame.dlc - 1) : 0;
  const uint8_t copy_len = payload_len < available ? payload_len : available;
  if (copy_len == 0) {
    return;
  }

  memcpy(payload, &frame.data[1], copy_len);
  processPayload(payload, copy_len, frame, host);
}

void ObdPidTool::onFirstFrame(const core::CanFrame &frame, ToolHost &host) {
  if (frame.dlc < 3) {
    return;
  }

  IsoTpRx *rx = isoTpFor(frame.id);
  if (rx == nullptr) {
    return;
  }

  const uint16_t total_len = (uint16_t)(((uint16_t)(frame.data[0] & 0x0F) << 8) | frame.data[1]);
  if (total_len == 0 || total_len > sizeof(rx->payload)) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "ISO-TP payload too large");
    return;
  }

  memset(rx, 0, sizeof(*rx));
  rx->active = true;
  rx->ecu_id = frame.id;
  rx->total_len = total_len;
  rx->next_seq = 1;

  const uint8_t bytes_in_first = (frame.dlc > 2) ? (uint8_t)(frame.dlc - 2) : 0;
  const uint8_t copy_len = (uint8_t)(bytes_in_first > total_len ? total_len : bytes_in_first);
  if (copy_len > 0) {
    memcpy(rx->payload, &frame.data[2], copy_len);
    rx->collected_len = copy_len;
  }

  if (!sendFlowControl(frame, host)) {
    memset(rx, 0, sizeof(*rx));
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "ISO-TP flow control send failed");
    return;
  }

  if (rx->collected_len >= rx->total_len) {
    processPayload(rx->payload, rx->total_len, frame, host);
    memset(rx, 0, sizeof(*rx));
  }
}

void ObdPidTool::onConsecutiveFrame(const core::CanFrame &frame, ToolHost &host) {
  IsoTpRx *rx = isoTpFor(frame.id);
  if (rx == nullptr || !rx->active || frame.id != rx->ecu_id) {
    return;
  }

  const uint8_t seq = (uint8_t)(frame.data[0] & 0x0F);
  if (seq != rx->next_seq) {
    memset(rx, 0, sizeof(*rx));
    host.emitToolEvent(id(), core::ToolEventCode::WARNING, "ISO-TP sequence mismatch; drop frame");
    return;
  }

  const uint16_t remaining = (uint16_t)(rx->total_len - rx->collected_len);
  if (remaining == 0) {
    memset(rx, 0, sizeof(*rx));
    return;
  }

  const uint8_t bytes_in_cf = (frame.dlc > 1) ? (uint8_t)(frame.dlc - 1) : 0;
  const uint8_t copy_len = (uint8_t)(bytes_in_cf > remaining ? remaining : bytes_in_cf);
  if (copy_len > 0) {
    memcpy(&rx->payload[rx->collected_len], &frame.data[1], copy_len);
    rx->collected_len = (uint16_t)(rx->collected_len + copy_len);
  }

  rx->next_seq = (uint8_t)((rx->next_seq + 1) & 0x0F);

  if (rx->collected_len >= rx->total_len) {
    processPayload(rx->payload, rx->total_len, frame, host);
    memset(rx, 0, sizeof(*rx));
  }
}

void ObdPidTool::onFrame(const core::CanFrame &frame, ToolHost &host) {
  if (!busMatches(bus_, frame.bus)) {
    return;
  }
  if (frame.dlc < 2) {
    return;
  }

  // If using a physical request ID (7E0-7E7), expect matching physical response (req+8).
  if (request_id_ >= 0x7E0 && request_id_ <= 0x7E7) {
    if (frame.id != (request_id_ + 8)) {
      return;
    }
  } else if (frame.id < 0x7E8 || frame.id > 0x7EF) {
    return;
  }

  if (one_shot_ && !awaiting_response_) {
    return;
  }

  const uint8_t pci_type = (uint8_t)((frame.data[0] >> 4) & 0x0F);
  if (pci_type == 0x0) {
    onSingleFrame(frame, host);
    return;
  }
  if (pci_type == 0x1) {
    onFirstFrame(frame, host);
    return;
  }
  if (pci_type == 0x2) {
    onConsecutiveFrame(frame, host);
    return;
  }
}

void ObdPidTool::describe(char *out, size_t out_len) const {
  if (request_kind_ == RequestKind::MODE01_PID) {
    snprintf(out,
             out_len,
             "obd_pid bus=%s mode=pid pid=0x%02X (%s) req_id=0x%lX interval_ms=%lu",
             core::busName(bus_),
             pid_,
             requestName(),
             (unsigned long)request_id_,
             (unsigned long)interval_ms_);
    return;
  }

  snprintf(out,
           out_len,
           "obd_pid bus=%s mode=%s req_id=0x%lX one_shot sent=%u awaiting=%u",
           core::busName(bus_),
           requestName(),
           (unsigned long)request_id_,
           request_sent_ ? 1 : 0,
           awaiting_response_ ? 1 : 0);
}

} // namespace tools
