#include <storage/storage.h>

typedef struct {
    Storage* storage;
    FuriString* path;
    File* file;
} SESSION, *PSESSION;

typedef enum {
    SESSION_CREATE_SUCCESS,
    SESSION_CREATE_DUPLICATE_FILE_NAME,
    SESSION_CREATE_FAILED_TO_CREATE_FILE,
    SESSION_CREATE_FAILED_TO_WRITE_FILE
} SESSIONCREATE;

typedef struct {
    uint8_t cube;
} SESSIONSETTINGS, *PSESSIONSETTINGS;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t SessionCreate(const PSESSION session, const char* const path);
void SessionDelete(const PSESSION session);
void SessionFree(const PSESSION session);
void SessionInitialize(const PSESSION session);
void SessionLoadSettings(const PSESSION session, const PSESSIONSETTINGS settings);
void SessionOpen(const PSESSION session);
void SessionRename(const PSESSION session);
void SessionSaveSettings(const PSESSION session, const PSESSIONSETTINGS settings);
#ifdef __cplusplus
}
#endif
