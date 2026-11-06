#include "tag_watch.h"

#include <string.h>

void tag_watch_clear(TagWatch* w) {
    memset(w, 0, sizeof(*w));
}

void tag_watch_set(TagWatch* w, const uint8_t* uid, size_t len) {
    tag_watch_clear(w);
    if(len == 0 || len > TAG_DUMP_MAX_UID) return;
    memcpy(w->uid, uid, len);
    w->uid_len = (uint8_t)len;
}

bool tag_watch_waiting(const TagWatch* w) {
    return w->uid_len > 0;
}

bool tag_watch_poll(TagWatch* w, const uint8_t* uid, size_t len) {
    if(!tag_watch_waiting(w)) return false;
    if(len == 0) {
        if(++w->misses >= TAG_WATCH_MISSES) tag_watch_clear(w);
    } else if(len == w->uid_len && memcmp(uid, w->uid, len) == 0) {
        w->misses = 0;
    } else {
        tag_watch_clear(w);
    }
    return tag_watch_waiting(w);
}
