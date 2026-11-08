#include "CliShell.h"

#include <string.h>

namespace io {

CliShell::CliShell() : stream_(nullptr), line_len_(0), discarding_(false), overflow_pending_(false) {
  memset(line_buf_, 0, sizeof(line_buf_));
}

void CliShell::begin(Stream &stream) {
  stream_ = &stream;
  line_len_ = 0;
  discarding_ = false;
  overflow_pending_ = false;
  queue_.clear();
}

void CliShell::poll() {
  if (stream_ == nullptr) {
    return;
  }

  // Stop reading once the line queue is full; unread bytes stay in the stream
  // buffer until the queued commands have run, so pasted batches are not dropped.
  while (!queue_.full() && stream_->available() > 0) {
    int c = stream_->read();
    if (c < 0) {
      break;
    }

    char ch = static_cast<char>(c);
    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      if (discarding_) {
        // End of an over-long line: drop it entirely, never run its tail.
        discarding_ = false;
        line_len_ = 0;
        continue;
      }
      if (line_len_ == 0) {
        continue;
      }
      core::CliLine out{};
      memcpy(out.line, line_buf_, line_len_);
      out.line[line_len_] = '\0';
      queue_.pushDropOldest(out);
      memset(line_buf_, 0, sizeof(line_buf_));
      line_len_ = 0;
      continue;
    }

    if (discarding_) {
      continue;
    }
    if (line_len_ < (BoardConfig::CLI_LINE_MAX - 1)) {
      line_buf_[line_len_++] = ch;
    } else {
      memset(line_buf_, 0, sizeof(line_buf_));
      line_len_ = 0;
      discarding_ = true;
      overflow_pending_ = true;
    }
  }
}

bool CliShell::popLine(core::CliLine &line) {
  return queue_.pop(line);
}

bool CliShell::takeOverflow() {
  const bool pending = overflow_pending_;
  overflow_pending_ = false;
  return pending;
}

} // namespace io
