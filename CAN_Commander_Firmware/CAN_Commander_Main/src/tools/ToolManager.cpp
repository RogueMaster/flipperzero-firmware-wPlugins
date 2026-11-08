#include "ToolManager.h"

#include <string.h>

namespace tools {

ToolManager::ToolManager() : active_id_(core::ToolId::NONE) {}

ToolBase *ToolManager::find(core::ToolId id) {
  switch (id) {
    case core::ToolId::READ_ALL: return &read_all_;
    case core::ToolId::FILTERED: return &filtered_;
    case core::ToolId::WRITE: return &write_;
    case core::ToolId::SPEED: return &speed_;
    case core::ToolId::VALTRACK: return &valtrack_;
    case core::ToolId::UNIQUE_IDS: return &unique_ids_;
    case core::ToolId::BITTRACK: return &bittrack_;
    case core::ToolId::REVERSE: return &reverse_;
    case core::ToolId::OBD_PID: return &obd_pid_;
    case core::ToolId::DBC_DECODE: return &dbc_decode_;
    case core::ToolId::CUSTOM_INJECT: return &custom_inject_;
    case core::ToolId::REPLAY: return &replay_;
    default: return nullptr;
  }
}

const ToolBase *ToolManager::findConst(core::ToolId id) const {
  return const_cast<ToolManager *>(this)->find(id);
}

// Starts the new tool before stopping the current one, so a rejected start (bad
// arguments, unavailable bus) leaves the running tool untouched. Every tool's start()
// validates its arguments before changing any state.
bool ToolManager::startById(core::ToolId id, const char *args, ToolHost &host) {
  ToolBase *next = find(id);
  if (next == nullptr) {
    return false;
  }

  ToolBase *active = find(active_id_);
  if (!next->start(args, host)) {
    return false;
  }

  if (active != nullptr && active != next) {
    active->stop(host);
  }
  active_id_ = id;
  return true;
}

core::ToolId ToolManager::parseToolIdName(const char *name) {
  if (name == nullptr) {
    return core::ToolId::NONE;
  }
  if (strcmp(name, "read_all") == 0) {
    return core::ToolId::READ_ALL;
  }
  if (strcmp(name, "filtered") == 0) {
    return core::ToolId::FILTERED;
  }
  if (strcmp(name, "write") == 0) {
    return core::ToolId::WRITE;
  }
  if (strcmp(name, "speed") == 0) {
    return core::ToolId::SPEED;
  }
  if (strcmp(name, "valtrack") == 0) {
    return core::ToolId::VALTRACK;
  }
  if (strcmp(name, "unique_ids") == 0) {
    return core::ToolId::UNIQUE_IDS;
  }
  if (strcmp(name, "bittrack") == 0) {
    return core::ToolId::BITTRACK;
  }
  if (strcmp(name, "reverse") == 0) {
    return core::ToolId::REVERSE;
  }
  if (strcmp(name, "obd_pid") == 0) {
    return core::ToolId::OBD_PID;
  }
  if (strcmp(name, "dbc_decode") == 0) {
    return core::ToolId::DBC_DECODE;
  }
  if (strcmp(name, "custom_inject") == 0) {
    return core::ToolId::CUSTOM_INJECT;
  }
  if (strcmp(name, "replay") == 0) {
    return core::ToolId::REPLAY;
  }
  return core::ToolId::NONE;
}

const char *ToolManager::toolName(core::ToolId id) {
  switch (id) {
    case core::ToolId::READ_ALL: return "read_all";
    case core::ToolId::FILTERED: return "filtered";
    case core::ToolId::WRITE: return "write";
    case core::ToolId::SPEED: return "speed";
    case core::ToolId::VALTRACK: return "valtrack";
    case core::ToolId::UNIQUE_IDS: return "unique_ids";
    case core::ToolId::BITTRACK: return "bittrack";
    case core::ToolId::REVERSE: return "reverse";
    case core::ToolId::OBD_PID: return "obd_pid";
    case core::ToolId::DBC_DECODE: return "dbc_decode";
    case core::ToolId::CUSTOM_INJECT: return "custom_inject";
    case core::ToolId::REPLAY: return "replay";
    default: return "none";
  }
}

bool ToolManager::startByName(const char *name, const char *args, ToolHost &host, core::ToolId &started_id) {
  const core::ToolId id = parseToolIdName(name);
  if (id == core::ToolId::NONE) {
    return false;
  }
  if (!startById(id, args, host)) {
    return false;
  }
  started_id = id;
  return true;
}

bool ToolManager::stop(ToolHost &host) {
  if (active_id_ == core::ToolId::NONE) {
    return false;
  }
  ToolBase *active = find(active_id_);
  if (active != nullptr) {
    active->stop(host);
  }
  active_id_ = core::ToolId::NONE;
  return true;
}

bool ToolManager::configureActive(const char *args, ToolHost &host) {
  if (active_id_ == core::ToolId::NONE) {
    return false;
  }
  ToolBase *active = find(active_id_);
  if (active == nullptr) {
    return false;
  }
  return active->configure(args, host);
}

void ToolManager::onFrame(const core::CanFrame &frame, ToolHost &host) {
  if (active_id_ == core::ToolId::NONE) {
    return;
  }
  ToolBase *active = find(active_id_);
  if (active != nullptr) {
    active->onFrame(frame, host);
  }
}

void ToolManager::tick(ToolHost &host) {
  if (active_id_ == core::ToolId::NONE) {
    return;
  }
  ToolBase *active = find(active_id_);
  if (active != nullptr) {
    active->tick(host);
  }
}

bool ToolManager::isActive() const {
  return active_id_ != core::ToolId::NONE;
}

core::ToolId ToolManager::activeId() const {
  return active_id_;
}

const char *ToolManager::activeName() const {
  return toolName(active_id_);
}

void ToolManager::describeActive(char *out, size_t out_len) const {
  if (out == nullptr || out_len == 0) {
    return;
  }

  const ToolBase *active = findConst(active_id_);
  if (active == nullptr) {
    strncpy(out, "no active tool", out_len - 1);
    out[out_len - 1] = '\0';
    return;
  }
  active->describe(out, out_len);
}

} // namespace tools
