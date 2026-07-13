#include "session.h"
#include <furi_hal_rtc.h>

Session* session_alloc(void) {
    Session* session = malloc(sizeof(Session));
    session_reset(session, "New engagement");
    return session;
}

void session_free(Session* session) {
    if(session) free(session);
}

void session_reset(Session* session, const char* name) {
    furi_check(session);
    memset(session, 0, sizeof(Session));
    strncpy(session->name, name ? name : "New engagement", RECON_NAME_LEN - 1);
    session->created = furi_hal_rtc_get_timestamp();
    session->modified = session->created;
    session->next_id = 1;
}

uint16_t session_next_id(Session* session) {
    furi_check(session);
    return session->next_id++;
}

void session_touch(Session* session) {
    furi_check(session);
    session->modified = furi_hal_rtc_get_timestamp();
    session->dirty = true;
}

void session_mark_clean(Session* session) {
    furi_check(session);
    session->dirty = false;
}
