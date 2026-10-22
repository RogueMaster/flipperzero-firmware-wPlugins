#include "sr_uart_cap.h"

#include "sr_marauder_join.h"
#include "sr_types.h"

#include <string.h>

enum {
    SR_UART_CAP_BYTES = 6144
};
enum {
    SR_UART_CAP_RESERVE = 24
};

static char g_buf[SR_UART_CAP_BYTES];
static size_t g_len;
static bool g_reset;
static bool g_full;
static int g_armed;

static bool armed_load(void) {
    return __atomic_load_n(&g_armed, __ATOMIC_ACQUIRE) != 0;
}

static void armed_store(bool on) {
    __atomic_store_n(&g_armed, on ? 1 : 0, __ATOMIC_RELEASE);
}

static bool append_bytes(const char* s, size_t n) {
    if(n == 0u) {
        return true;
    }
    if(g_len + n > (size_t)SR_UART_CAP_BYTES - (size_t)SR_UART_CAP_RESERVE) {
        return false;
    }
    memcpy(g_buf + g_len, s, n);
    g_len += n;
    return true;
}

static void note_full(void) {
    static const char kFull[] = "## cap-full\n";
    size_t n = sizeof(kFull) - 1u;

    if(g_full) {
        return;
    }
    g_full = true;
    if(g_len + n > (size_t)SR_UART_CAP_BYTES) {
        return;
    }
    memcpy(g_buf + g_len, kFull, n);
    g_len += n;
}

void sr_uart_cap_arm(bool reset) {
    if(reset) {
        memset(g_buf, 0, sizeof(g_buf));
        g_len = 0u;
        g_full = false;
        g_reset = true;
    }
    armed_store(true);
}

void sr_uart_cap_disarm(void) {
    armed_store(false);
}

bool sr_uart_cap_armed(void) {
    return armed_load();
}

void sr_uart_cap_mark(const char* label) {
    char line[48];
    size_t n = 0u;
    size_t i;

    if(!armed_load() || label == NULL || label[0] == '\0') {
        return;
    }
    line[n++] = '#';
    line[n++] = '#';
    line[n++] = ' ';
    for(i = 0u; label[i] != '\0' && n + 2u < sizeof(line); i++) {
        unsigned char c = (unsigned char)label[i];
        if(c < 0x20u || c > 0x7Eu) {
            break;
        }
        line[n++] = (char)c;
    }
    line[n++] = '\n';
    if(!append_bytes(line, n)) {
        note_full();
    }
}

static void scrub_secret(char* line, size_t* len, size_t cap) {
    size_t n;
    size_t i;
    static const char kFlag[] = " -p ";
    static const char kMask[] = "***";

    if(line == NULL || len == NULL || cap == 0u) {
        return;
    }
    n = *len;
    if(n > cap) {
        n = cap;
    }
    (void)sr_join_redact_echo(line, &n, cap);
    (void)sr_join_redact_key(line, &n, cap);
    if(n < 4u) {
        *len = n;
        return;
    }
    for(i = 0u; i + 4u <= n; i++) {
        if(memcmp(line + i, kFlag, 4u) != 0) {
            continue;
        }
        if(i + 4u + sizeof(kMask) - 1u >= cap) {
            break;
        }
        memcpy(line + i + 4u, kMask, sizeof(kMask) - 1u);
        n = i + 4u + sizeof(kMask) - 1u;
        if(n < cap) {
            line[n] = '\0';
        }
        break;
    }
    *len = n;
}

void sr_uart_cap_note(const char* line, size_t len) {
    /* Worker stack is 2048. This scrub buffer stays off that stack. */
    static char tmp[SR_RAW_LINE_MAX + 1u];
    size_t n;

    if(!armed_load() || line == NULL) {
        return;
    }
    n = len;
    if(n > (size_t)SR_RAW_LINE_MAX) {
        n = (size_t)SR_RAW_LINE_MAX;
    }
    if(n > 0u) {
        memcpy(tmp, line, n);
    }
    tmp[n] = '\0';
    scrub_secret(tmp, &n, sizeof(tmp));
    if(!append_bytes(tmp, n) || !append_bytes("\n", 1u)) {
        note_full();
        memset(tmp, 0, sizeof(tmp));
        return;
    }
    memset(tmp, 0, sizeof(tmp));
}

bool sr_uart_cap_pull(char* dst, size_t cap, size_t* out_n, bool* reset) {
    size_t n;

    if(out_n != NULL) {
        *out_n = 0u;
    }
    if(reset != NULL) {
        *reset = false;
    }
    if(dst == NULL || cap == 0u || out_n == NULL || reset == NULL) {
        return false;
    }
    if(g_reset) {
        *reset = true;
        g_reset = false;
    }
    n = g_len;
    if(n > cap) {
        n = cap;
    }
    if(n > 0u) {
        memcpy(dst, g_buf, n);
        memmove(g_buf, g_buf + n, g_len - n);
        g_len -= n;
        if(g_len == 0u) {
            memset(g_buf, 0, sizeof(g_buf));
        }
    }
    *out_n = n;
    return true;
}
