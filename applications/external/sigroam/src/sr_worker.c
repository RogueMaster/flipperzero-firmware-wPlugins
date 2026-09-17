#include "sr_worker.h"

#include "sr_line.h"
#include "sr_parse_marauder.h"

#include <furi_hal.h>
#include <string.h>

#define SR_WORKER_TAG           "SrWorker"
#define SR_WORKER_STACK_SIZE    2048u
#define SR_WORKER_CHUNK         64u
#define SR_WORKER_RX_TIMEOUT_MS 50u
/*
 * Product/engineering choice, not a measured I2C duration.
 * Power service polls VBUS at 1 Hz; an unplug edge is persistent, so 250 ms is
 * 4× that. RX headroom is 2048 B ≈ 178 ms @115200 (sr_io.c SR_IO_RX_CAP) —
 * sampling must stay rare relative to that. On-device rx_dropped / rx_max_fill
 * before/after is card D11-RESYNC §5.3 and is not taken here (no ufbt launch).
 */
#define SR_WORKER_VBUS_PERIOD_MS 250u
/* Must stay equal to SR_IO_VBUS_PRESENT_V in sr_io.c (that symbol is file-local). */
#define SR_WORKER_VBUS_PRESENT_V 4.5f

/* Heap singletons only. Do not declare the line/parser/event types as
 * locals, VLAs, or static buffers (ADR-009 / ADR-015). */
struct SrWorker {
    SrIo* io;
    SrModel* model;
    FuriMutex* mtx;
    FuriThread* thread;
    SrLine* line;
    SrParser* parser;
    SrEvent* ev;
    bool exit;
    char cmd_buf[SR_WORKER_CMD_MAX];
    size_t cmd_len;
    bool cmd_ready;
    SrWorkerStats stats;
    uint8_t vbus_present; /* 0/1, written by the worker thread, read from any thread */
    uint32_t vbus_last_ms;
};

static void sr_worker_sample_stack(SrWorker* w) {
    uint32_t space;
    uint32_t prev;

    space = furi_thread_get_stack_space(furi_thread_get_current_id());
    prev = __atomic_load_n(&w->stats.stack_min, __ATOMIC_RELAXED);
    if(prev == 0u || space < prev) {
        __atomic_store_n(&w->stats.stack_min, space, __ATOMIC_RELAXED);
    }
}

static void sr_worker_handle_ready(SrWorker* w) {
    size_t len = 0;
    const char* text;
    bool apply = false;

    __atomic_add_fetch(&w->stats.lines_ready, 1u, __ATOMIC_RELAXED);

    text = sr_line_text(w->line, &len);
    if(sr_line_truncated(w->line)) {
        __atomic_add_fetch(&w->stats.lines_truncated, 1u, __ATOMIC_RELAXED);
        memset(w->ev, 0, sizeof(*w->ev));
        w->ev->kind = SrEventUnknown;
        w->ev->u.unknown.text = text;
        w->ev->u.unknown.len = len;
        apply = true;
    } else {
        SrParseResult r = sr_codec_marauder.feed_line(w->parser, text, len, w->ev);
        if(r != SrParseNeedMore) {
            apply = true;
        }
    }

    if(apply) {
        furi_mutex_acquire(w->mtx, FuriWaitForever);
        sr_model_apply(w->model, w->ev, furi_get_tick());
        furi_mutex_release(w->mtx);
        __atomic_add_fetch(&w->stats.lines_applied, 1u, __ATOMIC_RELAXED);
    }

    sr_line_consume(w->line);
}

static void sr_worker_sample_vbus(SrWorker* w) {
    uint32_t now;
    float v;
    uint8_t present;

    now = furi_get_tick();
    if(w->vbus_last_ms != 0u &&
       (uint32_t)(now - w->vbus_last_ms) < SR_WORKER_VBUS_PERIOD_MS) {
        return;
    }
    v = furi_hal_power_get_usb_voltage();
    present = (v >= SR_WORKER_VBUS_PRESENT_V) ? 1u : 0u;
    __atomic_store_n(&w->vbus_present, present, __ATOMIC_RELAXED);
    w->vbus_last_ms = now;
}

static void sr_worker_handle_bytes(SrWorker* w, const uint8_t* data, size_t n) {
    size_t off = 0;

    while(off < n) {
        off += sr_line_feed(w->line, (const char*)data + off, n - off);
        if(!sr_line_ready(w->line)) {
            continue;
        }
        sr_worker_handle_ready(w);
    }
}

static int32_t sr_worker_thread(void* context) {
    SrWorker* w = context;
    uint8_t chunk[SR_WORKER_CHUNK];

    while(!__atomic_load_n(&w->exit, __ATOMIC_ACQUIRE)) {
        sr_worker_sample_stack(w);

        if(__atomic_exchange_n(&w->cmd_ready, false, __ATOMIC_ACQ_REL)) {
            (void)sr_io_write(w->io, w->cmd_buf, w->cmd_len);
        }

        {
            size_t n = sr_io_read(w->io, chunk, SR_WORKER_CHUNK, SR_WORKER_RX_TIMEOUT_MS);
            if(n > 0u) {
                sr_worker_handle_bytes(w, chunk, n);
            }
        }
        sr_worker_sample_vbus(w);
    }

    for(;;) {
        size_t n = sr_io_read(w->io, chunk, SR_WORKER_CHUNK, 0);
        if(n == 0u) {
            break;
        }
        sr_worker_handle_bytes(w, chunk, n);
    }

    return 0;
}

SrWorker* sr_worker_alloc(SrIo* io, SrModel* model, FuriMutex* mtx) {
    SrWorker* w;

    if(io == NULL || model == NULL || mtx == NULL) {
        return NULL;
    }

    w = malloc(sizeof(SrWorker));
    if(w == NULL) {
        return NULL;
    }
    memset(w, 0, sizeof(*w));
    w->io = io;
    w->model = model;
    w->mtx = mtx;

    w->line = malloc(sizeof(SrLine));
    w->parser = malloc(sizeof(SrParser));
    w->ev = malloc(sizeof(SrEvent));
    if(w->line == NULL || w->parser == NULL || w->ev == NULL) {
        free(w->line);
        free(w->parser);
        free(w->ev);
        free(w);
        return NULL;
    }

    sr_line_init(w->line);
    memset(w->parser, 0, sizeof(*w->parser));
    memset(w->ev, 0, sizeof(*w->ev));
    return w;
}

void sr_worker_free(SrWorker* w) {
    if(w == NULL) {
        return;
    }
    furi_check(w->thread == NULL);
    free(w->line);
    free(w->parser);
    free(w->ev);
    free(w);
}

bool sr_worker_start(SrWorker* w) {
    if(w == NULL || w->thread != NULL || !sr_io_is_open(w->io)) {
        return false;
    }

    w->thread = furi_thread_alloc_ex(
        SR_WORKER_TAG, SR_WORKER_STACK_SIZE, sr_worker_thread, w);
    if(w->thread == NULL) {
        return false;
    }

    furi_thread_set_priority(w->thread, FuriThreadPriorityNormal);
    __atomic_store_n(&w->exit, false, __ATOMIC_RELEASE);
    furi_thread_start(w->thread);
    return true;
}

void sr_worker_request_stop(SrWorker* w) {
    if(w == NULL) {
        return;
    }
    __atomic_store_n(&w->exit, true, __ATOMIC_RELEASE);
}

void sr_worker_join(SrWorker* w) {
    if(w == NULL || w->thread == NULL) {
        return;
    }
    furi_thread_join(w->thread);
    furi_thread_free(w->thread);
    w->thread = NULL;
}

bool sr_worker_send_cmd(SrWorker* w, const char* cmd) {
    size_t n = 0;

    if(w == NULL || cmd == NULL) {
        return false;
    }
    if(__atomic_load_n(&w->cmd_ready, __ATOMIC_ACQUIRE)) {
        return false;
    }
    while(cmd[n] != '\0') {
        n++;
    }
    if(n == 0u || n > (size_t)(SR_WORKER_CMD_MAX - 1)) {
        return false;
    }
    memcpy(w->cmd_buf, cmd, n);
    w->cmd_len = n;
    __atomic_store_n(&w->cmd_ready, true, __ATOMIC_RELEASE);
    return true;
}

void sr_worker_get_stats(const SrWorker* w, SrWorkerStats* out) {
    if(out == NULL) {
        return;
    }
    if(w == NULL) {
        memset(out, 0, sizeof(*out));
        return;
    }
    out->lines_ready = __atomic_load_n(&w->stats.lines_ready, __ATOMIC_RELAXED);
    out->lines_truncated = __atomic_load_n(&w->stats.lines_truncated, __ATOMIC_RELAXED);
    out->lines_applied = __atomic_load_n(&w->stats.lines_applied, __ATOMIC_RELAXED);
    out->stack_min = __atomic_load_n(&w->stats.stack_min, __ATOMIC_RELAXED);
}

uint32_t sr_worker_cmdack_count(const SrWorker* w, SrCmdAckClass cls) {
    if(w == NULL || w->parser == NULL) {
        return 0u;
    }
    if((int)cls <= (int)SrCmdAckNone || (int)cls >= (int)SrCmdAckClassCount) {
        return 0u;
    }
    return __atomic_load_n(&w->parser->cmdack.count[cls], __ATOMIC_RELAXED);
}

bool sr_worker_vbus_present(const SrWorker* w) {
    if(w == NULL) {
        return false;
    }
    return __atomic_load_n(&w->vbus_present, __ATOMIC_RELAXED) != 0u;
}
