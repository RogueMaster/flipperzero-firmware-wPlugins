#include "meshcore_msglog.h"

#include <furi.h>
#include <storage/storage.h>

#define MESHCORE_MSGLOG_DIR EXT_PATH("apps_data/meshcore_cfg")
#define MESHCORE_MSGLOG_PATH MESHCORE_MSGLOG_DIR "/messages.log"

/* A saved line is the encoded message; sized with headroom over the text. */
#define MESHCORE_MSGLOG_LINE_MAX (MC_MAX_TEXT + 96u)

struct MeshCoreMsgLog {
    Storage* storage;
    File* file; /* append handle, opened lazily on the first write */
    bool open;
};

MeshCoreMsgLog* meshcore_msglog_alloc(void) {
    MeshCoreMsgLog* log = malloc(sizeof(MeshCoreMsgLog));
    log->storage = furi_record_open(RECORD_STORAGE);
    log->file = storage_file_alloc(log->storage);
    log->open = false;

    /* Best-effort: if the card is absent these fail and the log stays a no-op. */
    storage_simply_mkdir(log->storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(log->storage, MESHCORE_MSGLOG_DIR);

    return log;
}

void meshcore_msglog_free(MeshCoreMsgLog* log) {
    furi_assert(log);
    if(log->open) storage_file_close(log->file);
    storage_file_free(log->file);
    furi_record_close(RECORD_STORAGE);
    free(log);
}

void meshcore_msglog_load(MeshCoreMsgLog* log, MeshCoreMessages* store) {
    furi_assert(log);

    File* rf = storage_file_alloc(log->storage);
    if(storage_file_open(rf, MESHCORE_MSGLOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MESHCORE_MSGLOG_LINE_MAX];
        size_t line_len = 0;
        bool overflow = false;
        uint8_t buf[512];
        size_t n;

        while((n = storage_file_read(rf, buf, sizeof(buf))) > 0) {
            for(size_t i = 0; i < n; i++) {
                char c = (char)buf[i];
                if(c == '\n') {
                    if(!overflow && line_len > 0) {
                        line[line_len] = '\0';
                        MeshCoreMessage message;
                        if(meshcore_message_decode(line, &message)) {
                            /* Startup, before the mailbox worker runs, so the
                             * store needs no lock here. The ring keeps the last
                             * MESHCORE_MESSAGES_MAX; older lines just scroll off. */
                            meshcore_messages_add(store, &message);
                        }
                    }
                    line_len = 0;
                    overflow = false;
                } else if(line_len + 1 < sizeof(line)) {
                    line[line_len++] = c;
                } else {
                    /* A line longer than any message we write: skip to the next
                     * newline rather than truncate-and-misparse. */
                    overflow = true;
                }
            }
        }
        /* A trailing line with no newline (e.g. a write cut short by a pull). */
        if(!overflow && line_len > 0) {
            line[line_len] = '\0';
            MeshCoreMessage message;
            if(meshcore_message_decode(line, &message)) {
                meshcore_messages_add(store, &message);
            }
        }
    }
    storage_file_close(rf);
    storage_file_free(rf);
}

static bool meshcore_msglog_ensure_open(MeshCoreMsgLog* log) {
    if(log->open) return true;
    /* FSOM_OPEN_APPEND creates the file if absent and seeks to the end. */
    if(storage_file_open(log->file, MESHCORE_MSGLOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        log->open = true;
    } else {
        /* The API wants a close even after a failed open, so the handle can be
         * retried on the next append. */
        storage_file_close(log->file);
    }
    return log->open;
}

void meshcore_msglog_append(MeshCoreMsgLog* log, const MeshCoreMessage* message) {
    furi_assert(log);
    if(!meshcore_msglog_ensure_open(log)) return;

    char line[MESHCORE_MSGLOG_LINE_MAX];
    size_t len = meshcore_message_encode(message, line, sizeof(line));
    if(len == 0) return;

    if(storage_file_write(log->file, line, len) != len ||
       storage_file_write(log->file, "\n", 1) != 1) {
        /* A bad write (card pulled?) closes the handle so a later append can
         * reopen rather than keep writing into a broken file. */
        storage_file_close(log->file);
        log->open = false;
        return;
    }
    /* On disk before we return: a message the user has seen must not be the one
     * a battery pull loses. */
    storage_file_sync(log->file);
}
