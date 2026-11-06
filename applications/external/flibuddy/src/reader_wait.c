#include "reader_wait.h"

bool reader_wait_stop(uint32_t flags, bool stopping) {
    if(stopping) return true;
    return !(flags & READER_FLAG_ERROR_BIT) && (flags & READER_FLAG_STOP);
}

bool reader_tag_answered(bool read_ok, uint8_t uid_len) {
    return read_ok && uid_len > 0;
}
