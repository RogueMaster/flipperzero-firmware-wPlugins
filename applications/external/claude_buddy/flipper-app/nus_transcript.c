#include "nus_transcript.h"

#include <furi.h>
#include <string.h>

#define TAG "NusTranscript"

static FuriMutex* s_mutex = NULL;
/* Ring buffer indexed newest-first: entries[0] is the most recent.  When
 * full, adding a new entry drops the oldest. */
static char s_lines[NUS_TRANSCRIPT_CAP][NUS_TRANSCRIPT_LINE_MAX];
static int s_count = 0;

static void lock(void) {
    if(s_mutex) furi_mutex_acquire(s_mutex, FuriWaitForever);
}
static void unlock(void) {
    if(s_mutex) furi_mutex_release(s_mutex);
}

void nus_transcript_init(void) {
    if(!s_mutex) s_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    lock();
    s_count = 0;
    unlock();
}

void nus_transcript_free(void) {
    if(s_mutex) {
        furi_mutex_free(s_mutex);
        s_mutex = NULL;
    }
}

void nus_transcript_reset(void) {
    lock();
    s_count = 0;
    unlock();
}

static void push_front_locked(const char* src, int src_len) {
    if(src_len <= 0) return;
    /* Shift everything down by one slot (drop oldest if full). */
    int keep = s_count < NUS_TRANSCRIPT_CAP ? s_count : NUS_TRANSCRIPT_CAP - 1;
    for(int i = keep; i > 0; i--) {
        memcpy(s_lines[i], s_lines[i - 1], NUS_TRANSCRIPT_LINE_MAX);
    }
    int copy = src_len;
    if(copy >= NUS_TRANSCRIPT_LINE_MAX) copy = NUS_TRANSCRIPT_LINE_MAX - 1;
    memcpy(s_lines[0], src, copy);
    s_lines[0][copy] = '\0';
    if(s_count < NUS_TRANSCRIPT_CAP) s_count++;
}

void nus_transcript_replace_from_entries(const char* body, int body_len) {
    if(!body || body_len <= 0) return;
    lock();
    s_count = 0;

    /* Entries arrive newest-first; walk the array body in order and
     * write DIRECTLY into the ring buffer (newest at index 0).  Avoid
     * any additional large stack allocations — this runs on the BLE
     * event thread which has a ~2 KB stack. */
    const char* p = body;
    const char* end = body + body_len;
    int n = 0;
    while(p < end && n < NUS_TRANSCRIPT_CAP) {
        while(p < end && *p != '"') p++;
        if(p >= end) break;
        const char* start = ++p;
        while(p < end && *p != '"') {
            if(*p == '\\' && p + 1 < end) p++;
            p++;
        }
        if(p >= end) break;
        int len = (int)(p - start);
        if(len >= NUS_TRANSCRIPT_LINE_MAX) len = NUS_TRANSCRIPT_LINE_MAX - 1;
        memcpy(s_lines[n], start, len);
        s_lines[n][len] = '\0';
        n++;
        p++; /* skip closing quote */
    }
    s_count = n;
    unlock();
}

void nus_transcript_append(const char* line) {
    if(!line || !line[0]) return;
    lock();
    push_front_locked(line, (int)strlen(line));
    unlock();
}

int nus_transcript_count(void) {
    lock();
    int c = s_count;
    unlock();
    return c;
}

bool nus_transcript_get(int idx, char* out, int out_size) {
    if(!out || out_size <= 0) return false;
    out[0] = '\0';
    bool ok = false;
    lock();
    if(idx >= 0 && idx < s_count) {
        strlcpy(out, s_lines[idx], (size_t)out_size);
        ok = true;
    }
    unlock();
    return ok;
}
