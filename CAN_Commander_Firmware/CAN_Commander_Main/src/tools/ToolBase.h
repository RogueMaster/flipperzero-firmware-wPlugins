#pragma once

#include "../core/Types.h"
#include "../decode/DbcSignalStore.h"

namespace tools {

class ToolHost {
 public:
  virtual ~ToolHost() = default;

  virtual uint32_t nowMs() const = 0;
  virtual bool sendCanFrame(const core::CanFrame &frame) = 0;

  virtual void emitCanFrameEvent(const core::CanFrame &frame, bool ascii_mode) = 0;
  virtual void emitToolEvent(core::ToolId tool, core::ToolEventCode code, const char *text) = 0;
  virtual void emitDbcDecodeEvent(const core::DbcDecodedValue &value) = 0;

  virtual decode::DbcSignalStore &dbcStore() = 0;

  // Free entries in the fullest output queue. Tools that emit long listings use it
  // to pace themselves across ticks instead of overflowing the queues.
  virtual uint16_t outputHeadroom() const { return 0xFFFF; }

  // True when the bus hardware is initialized. BOTH means "at least one bus".
  virtual bool isBusAvailable(core::BusId bus) const {
    (void)bus;
    return true;
  }
};

class ToolBase {
 public:
  virtual ~ToolBase() = default;

  virtual core::ToolId id() const = 0;
  virtual const char *name() const = 0;

  virtual bool start(const char *args, ToolHost &host) = 0;
  virtual void stop(ToolHost &host) = 0;

  virtual void onFrame(const core::CanFrame &frame, ToolHost &host) = 0;
  virtual void tick(ToolHost &host) = 0;

  virtual bool configure(const char *args, ToolHost &host) {
    (void)args;
    (void)host;
    return false;
  }

  virtual void describe(char *out, size_t out_len) const = 0;
};

} // namespace tools
