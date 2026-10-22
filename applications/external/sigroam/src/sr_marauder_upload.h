#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Marauder v1.17.0 Direct Upload, as driven from the Upload page.
 * No furi. Official command is `upload -d wigle`; `upload` / `uploadstatus`
 * stay the SigRoam firmware words.
 *
 * Flight ends only on a terminal line from that command. There is no timer:
 * a lab round-trip duration has not been recorded, and a guessed timeout
 * would either re-send or declare success early. `Upload complete` with no
 * preceding `Uploading ` line is an empty sd_files walk. The screen says
 * `No logs in list` / `nothing sent`, not a WiGLE receipt. A later file's
 * `Upload OK` does not rename an earlier rejected file.
 *
 * session arguments are SrSessionState values: 0 idle, 1 running, 2 stopped.
 */

enum {
    SR_MAU_COLS = 20
};

static inline const char* sr_upload_go_cmd(bool generic) {
    return generic ? "upload -d wigle\n" : "upload\n";
}

/* NULL: the Upload page must not transmit on enter. */
static inline const char* sr_upload_enter_cmd(bool generic) {
    return generic ? NULL : "uploadstatus\n";
}

/* Running joins Wi-Fi without StopScan and skips the wardrive cleanup. */
static inline bool sr_mau_allow_go(unsigned session, bool in_flight) {
    if(in_flight) {
        return false;
    }
    return session == 0u || session == 2u;
}

void sr_mau_up_reset(void);
void sr_mau_up_on_send(void);
void sr_mau_up_feed(const char* line, size_t len);
bool sr_mau_up_in_flight(void);
uint32_t sr_mau_up_rev(void);
void sr_mau_up_lines(unsigned session, char* line1, char* line2);

/*
 * Optional third line after a finished flight, at most SR_MAU_COLS, empty
 * when there is nothing to add. It never names the SSID. The two result
 * lines above it are unchanged.
 */
void sr_mau_up_context(char* line);
/*
 * `Check wigle.net` after a flight whose files all got
 * `[UPLOAD] WiGLE upload succeeded` (a WiGLE 200), else empty. WiGLE may
 * still be processing; the app cannot ask.
 */
void sr_mau_up_hint(char* line);
/* Wi-Fi was reached and the walk printed no `Uploading ` line. */
bool sr_mau_up_empty_list(void);

/*
 * The Join scene hands over the SSID it just connected. A later
 * `Connected to saved WiFi: ` with the same text means the board read it
 * back from its saved profiles. RAM only.
 */
void sr_mau_up_note_joined(const char* ssid);
void sr_mau_up_forget_joined(void);

/* Boot key import seen on this app run. Not proof the key works on WiGLE. */
enum {
    SrMauKeyUnknown = 0,
    SrMauKeyPartial,
    SrMauKeyImported,
};
unsigned sr_mau_up_key_state(void);
void sr_mau_up_key_forget(void);

/* Counts from the lines this flight already classified. False while it flies. */
typedef struct {
    uint32_t ok_n;
    uint32_t fail_n;
    bool wigle200;
    bool ready;
} SrMauUpTally;

bool sr_mau_up_tally(SrMauUpTally* out);
