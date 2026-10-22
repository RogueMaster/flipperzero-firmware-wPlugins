#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Read-only root listing for factory Marauder v1.17.0. No furi.
 *
 * Official `ls /` prints each entry as name, TAB, size, and then the CLI
 * prints `> ` with no newline (CommandLine.cpp / SDInterface::listDir,
 * tag v1.17.0, retrieved 2026-10-09). It does not fill sd_files and it has
 * no directory footer. sr_line drops TAB, so a completed line is name glued
 * to the decimal size. A prompt without a newline never becomes a line.
 * This module does not add a timeout and does not treat silence as zero logs.
 *
 * The CLI prints `> ` after every command (CommandLine.cpp 87) and echoes
 * the next one as `#` + input (270), so the echo arrives as `> #ls /`.
 * After that echo the scene sends `channel` once. Its echo `> #channel`
 * is the end of the listing. `channel` without `-s` only prints the channel.
 */

#define SR_MAU_LOGS_HELP_1 "CLI list empty"
#define SR_MAU_LOGS_HELP_2 "SD logs may exist"
#define SR_MAU_LOGS_HELP_3 "Read card on PC"
#define SR_MAU_LOGS_HELP_4 "Upload via WiGLE"
/* Replaces HELP_2 once a finished listing and an empty upload walk agree. */
#define SR_MAU_LOGS_SEEN_2 "CLI can't list them"
/* Official Direct Upload key import: SD root files, read at boot, then deleted. */
#define SR_MAU_KEY_HELP_1  "Key: board SD root"
#define SR_MAU_KEY_HELP_2  "wigle_api_name.txt"
#define SR_MAU_KEY_HELP_3  "wigle_api_token.txt"
#define SR_MAU_KEY_HELP_4  "then reboot board"

enum {
    SrMauLogsBlockNone = 0,
    SrMauLogsBlockScan,
    SrMauLogsBlockUpload,
    SrMauLogsBlockJoin,
    SrMauLogsBlockBusy,
};

const char* sr_mau_logs_cmd(void);
const char* sr_mau_logs_barrier_cmd(void);
/* True once the `ls /` echo is seen and the barrier is not yet sent. */
bool sr_mau_logs_barrier_due(void);
void sr_mau_logs_barrier_sent(void);
/*
 * Queue the barrier this tick. generic includes Scout Lite: its Version has
 * no "-sigroam-". slot_taken means this tick already queued a command, so
 * the barrier waits until the next tick. Not due, or not generic, stays false.
 */
bool sr_mau_logs_barrier_send(bool generic, bool slot_taken);

/* Idle or Stopped, and no upload or join transaction. */
bool sr_mau_logs_allow(unsigned session, bool upload_flight, bool join_busy);

void sr_mau_logs_reset(void);
void sr_mau_logs_arm(void);
/* A scan start drops the current query. Idle with no query stays idle. */
void sr_mau_logs_invalidate(void);
void sr_mau_logs_feed(const char* line, size_t len);

bool sr_mau_logs_armed(void);
uint32_t sr_mau_logs_rev(void);
uint32_t sr_mau_logs_matches(void);
bool sr_mau_logs_closed(void);
bool sr_mau_logs_cut(void);
bool sr_mau_logs_card(void);
bool sr_mau_logs_dropped(void);

/* `Card has N logs` after a finished listing with N >= 1, else false and "". */
bool sr_mau_logs_card_line(char* line);

/*
 * Sent means Marauder's own <log>.wigle sidecar is on the card. It is
 * written only after a WiGLE 200 (WiFiScan.cpp 11827-11865, v1.18.0
 * b2b9f92). It is not proof WiGLE finished processing the file.
 * The table holds SR_MAU_LOG_SLOTS names of at most SR_MAU_LOG_NAME_MAX
 * bytes. Past that the sent count is unknown and only the total is shown.
 */
enum {
    SR_MAU_LOG_SLOTS = 16,
    SR_MAU_LOG_NAME_MAX = 31
};
#define SR_MAU_LOGS_UNKNOWN UINT32_MAX

/* `N logs, K sent`, or `Card has N logs` when K is unknown. Needs N >= 1. */
bool sr_mau_logs_sent_line(char* line);
/* Logs without a sidecar after a finished listing, else SR_MAU_LOGS_UNKNOWN. */
uint32_t sr_mau_logs_unsent(void);
/* i-th unsent log as `name size`, at most SR_MAU_COLS. False past the end. */
bool sr_mau_logs_unsent_entry(unsigned i, char* line);

/* i-th stored log after a finished listing. Does not change the lines above. */
bool sr_mau_logs_slot(unsigned i, char* name, size_t name_cap, uint32_t* size, bool* sent);
/* Logs past the 16 slots after a finished listing. 0 when every log fit.
 * SR_MAU_LOGS_UNKNOWN until the listing is closed. */
uint32_t sr_mau_logs_extra(void);

/* Two screen lines, each at most SR_MAU_COLS. Not an upload receipt. */
void sr_mau_logs_lines(char* line1, char* line2);
void sr_mau_logs_block_lines(unsigned why, char* line1, char* line2);
