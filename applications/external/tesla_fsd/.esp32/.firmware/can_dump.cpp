#include "can_dump.h"
#include "config.h"
#include <stdarg.h>

// ─────────────────────────────────────────────────────────────────────────────
#if defined(BOARD_LILYGO)
// ─────────────────────────────────────────────────────────────────────────────

#include <SPI.h>
#include <SD.h>

#define DUMP_MAX_ENTRIES  1000000UL
#define DUMP_TIMEOUT_MS   (15UL * 60UL * 1000UL)

static SPIClass  g_spi(HSPI);
static bool      g_sd_ok    = false;
static bool      g_active   = false;
static uint32_t  g_start_ms = 0;
static uint32_t  g_entries  = 0;
static File      g_file;
static File      g_log_file;
static char      g_dir_path[32];
static File      g_syslog;          // /debug.log — persistent system log

static void syslog_open() {
    if (!g_sd_ok) return;
    g_syslog = SD.open("/debug.log", FILE_APPEND);
    if (!g_syslog) Serial.println("[SD] /debug.log open failed");
}

// ── helpers ───────────────────────────────────────────────────────────────────

// Delete a path (file or directory tree) by repeatedly picking the first
// remaining child — safe against iterator invalidation during deletion.
static void rm_recursive(const char *path) {
    String p = path;
    if (!p.startsWith("/")) p = "/" + p;

    {
        File f = SD.open(p);
        if (!f) return;
        bool is_dir = f.isDirectory();
        f.close();
        if (!is_dir) { SD.remove(p); return; }
    }
    for (;;) {
        File dir = SD.open(p);
        if (!dir) break;
        File child = dir.openNextFile();
        if (!child) { dir.close(); break; }
        String cp = child.name();
        if (!cp.startsWith("/")) cp = p + "/" + cp;
        child.close();
        dir.close();
        rm_recursive(cp.c_str());
    }
    SD.rmdir(p);
}

static uint32_t find_next_seq() {
    if (!SD.exists("/dumps")) {
        SD.mkdir("/dumps");
        return 1;
    }
    File dir = SD.open("/dumps");
    if (!dir) return 1;
    uint32_t max_seq = 0;
    File entry = dir.openNextFile();
    while (entry) {
        if (entry.isDirectory()) {
            const char *full = entry.name();
            const char *slash = strrchr(full, '/');
            uint32_t n = (uint32_t)atoi(slash ? slash + 1 : full);
            if (n > max_seq) max_seq = n;
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return max_seq + 1;
}

static bool open_new_file() {
    if (g_file) {
        g_file.flush();
        g_file.close();
    }
    uint32_t elapsed_s = (millis() - g_start_ms) / 1000;
    char path[72];
    snprintf(path, sizeof(path), "%s/candump_%lu.dump",
             g_dir_path, (unsigned long)elapsed_s);
    g_file   = SD.open(path, FILE_WRITE);
    g_entries = 0;
    if (!g_file) {
        Serial.printf("[SD] open failed: %s\n", path);
        return false;
    }
    Serial.printf("[SD] → %s\n", path);
    return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

void can_dump_init() {
    g_spi.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    // Use format_if_empty = true to recover from previous corruption
    if (!SD.begin(SD_CS, g_spi, 4000000, "/sd", 5, true)) {
        Serial.println("[SD] No card — CAN dump disabled");
        g_sd_ok = false;
        return;
    }
    g_sd_ok = true;
    uint64_t total_mb = SD.totalBytes() / (1024ULL * 1024ULL);
    uint64_t used_mb  = SD.usedBytes()  / (1024ULL * 1024ULL);
    Serial.printf("[SD] Card OK  %lluMB total  %lluMB used\n", total_mb, used_mb);
    syslog_open();
}

bool can_dump_start() {
    if (!g_sd_ok || g_active) return false;

    uint32_t seq = find_next_seq();
    snprintf(g_dir_path, sizeof(g_dir_path), "/dumps/%05lu", (unsigned long)seq);
    if (!SD.mkdir(g_dir_path)) {
        Serial.printf("[SD] mkdir failed: %s\n", g_dir_path);
        return false;
    }
    g_start_ms = millis();
    g_active   = true;

    if (!open_new_file()) {
        g_active = false;
        return false;
    }

    char log_path[72];
    snprintf(log_path, sizeof(log_path), "%s/debug.log", g_dir_path);
    g_log_file = SD.open(log_path, FILE_WRITE);
    if (!g_log_file)
        Serial.printf("[SD] debug.log open failed: %s\n", log_path);

    Serial.printf("[SD] Dump started  dir=%s\n", g_dir_path);
    return true;
}

void can_dump_stop() {
    if (!g_active) return;
    g_active = false;
    if (g_file) {
        g_file.flush();
        g_file.close();
    }
    if (g_log_file) {
        g_log_file.flush();
        g_log_file.close();
    }
    uint32_t elapsed_s = (millis() - g_start_ms) / 1000;
    Serial.printf("[SD] Dump stopped  elapsed=%lus  entries=%lu\n",
                  (unsigned long)elapsed_s, (unsigned long)g_entries);
}

void can_dump_record(const CanFrame &frame) {
    if (!g_active || !g_file) return;

    uint32_t elapsed_ms = millis() - g_start_ms;
    uint32_t sec  = elapsed_ms / 1000;
    uint32_t usec = (elapsed_ms % 1000) * 1000;

    // candump ASCII log: (sec.usec) can0 ID#DATA\n
    char line[48];
    int pos = snprintf(line, sizeof(line), "(%lu.%06lu) can0 %03X#",
                       (unsigned long)sec, (unsigned long)usec, frame.id);
    for (int i = 0; i < frame.dlc && i < 8; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02X", frame.data[i]);
    }
    line[pos++] = '\n';
    g_file.write((const uint8_t *)line, pos);

    g_entries++;
    if (g_entries >= DUMP_MAX_ENTRIES) {
        open_new_file();
    }
}

void can_dump_tick(uint32_t now_ms) {
    if (!g_active) return;
    if ((now_ms - g_start_ms) >= DUMP_TIMEOUT_MS) {
        Serial.println("[SD] 15-min auto-stop");
        can_dump_stop();
    }
}

bool can_dump_active() {
    return g_active;
}

void can_dump_log(const char *fmt, ...) {
    if (!g_active || !g_log_file) return;

    uint32_t elapsed_ms = millis() - g_start_ms;
    uint32_t sec  = elapsed_ms / 1000;
    uint32_t usec = (elapsed_ms % 1000) * 1000;

    char buf[160];
    int pos = snprintf(buf, sizeof(buf), "(%lu.%06lu) ", (unsigned long)sec, (unsigned long)usec);

    va_list ap;
    va_start(ap, fmt);
    pos += vsnprintf(buf + pos, sizeof(buf) - pos, fmt, ap);
    va_end(ap);

    if (pos < (int)sizeof(buf) - 1) buf[pos++] = '\n';
    g_log_file.write((const uint8_t *)buf, pos);
}

void sd_syslog(const char *fmt, ...) {
    if (!g_sd_ok) return;
    if (!g_syslog) {
        g_syslog = SD.open("/debug.log", FILE_APPEND);
        if (!g_syslog) return;
    }
    uint32_t ms   = millis();
    uint32_t sec  = ms / 1000;
    uint32_t usec = (ms % 1000) * 1000;
    char buf[160];
    int pos = snprintf(buf, sizeof(buf), "(%lu.%06lu) ",
                       (unsigned long)sec, (unsigned long)usec);
    va_list ap;
    va_start(ap, fmt);
    pos += vsnprintf(buf + pos, sizeof(buf) - pos, fmt, ap);
    va_end(ap);
    if (pos < (int)sizeof(buf) - 1) buf[pos++] = '\n';
    g_syslog.write((const uint8_t *)buf, pos);
    g_syslog.flush();
}

void sd_syslog_close() {
    if (g_syslog) {
        g_syslog.flush();
        g_syslog.close();
    }
}

String sd_format_card() {
    if (g_active) can_dump_stop();
    sd_syslog_close();

    if (g_sd_ok) {
        SD.end();
        g_sd_ok = false;
    }

    Serial.println("[SD] Wiping card content...");

    // Mount with format_if_failed=true to handle corrupted cards
    if (!SD.begin(SD_CS, g_spi, 4000000, "/sd", 5, true)) {
        return "{\"ok\":false,\"msg\":\"SD card mount failed\"}";
    }
    g_sd_ok = true;

    // Recursive wipe of all files/folders
    File root = SD.open("/");
    if (!root) return "{\"ok\":false,\"msg\":\"Could not open root\"}";

    File f = root.openNextFile();
    while (f) {
        String path = f.name();
        // Ensure absolute path
        if (!path.startsWith("/")) path = "/" + path;
        
        bool is_sys = (path.indexOf("System Volume Information") != -1) || 
                      (path.indexOf(".Spotlight-V100") != -1) ||
                      (path.indexOf(".Trashes") != -1);

        if (!is_sys) {
            Serial.printf("[SD] rm %s\n", path.c_str());
            f.close(); // Close before deleting
            rm_recursive(path.c_str());
        } else {
            Serial.printf("[SD] skip %s\n", path.c_str());
            f.close();
        }
        f = root.openNextFile();
    }
    root.close();

    uint64_t total_mb = SD.totalBytes() / (1024ULL * 1024ULL);
    uint64_t free_mb  = (SD.totalBytes() - SD.usedBytes()) / (1024ULL * 1024ULL);
    
    Serial.printf("[SD] Wipe complete. %lluMB free\n", free_mb);
    syslog_open();

    String r = "{\"ok\":true,\"msg\":\"SD card wiped clean\",\"total_mb\":";
    r += (uint32_t)total_mb;
    r += ",\"free_mb\":";
    r += (uint32_t)free_mb;
    r += '}';
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
#else  // Non-Lilygo stubs
// ─────────────────────────────────────────────────────────────────────────────

void   can_dump_init()                    {}
bool   can_dump_start()                   { return false; }
void   can_dump_stop()                    {}
void   can_dump_record(const CanFrame &)  {}
void   can_dump_tick(uint32_t)            {}
bool   can_dump_active()                  { return false; }
void   can_dump_log(const char *, ...)    {}
void   sd_syslog(const char *, ...)       {}
void   sd_syslog_close()                  {}
String sd_format_card()                   { return "{\"ok\":false,\"msg\":\"SD not available on this board\"}"; }

#endif
