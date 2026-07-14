#include "src/session.h"

#define CBZS_MAGIC_NUMBER 0x43425A53

typedef struct {
    uint32_t magicNumber;
    SESSIONSETTINGS settings;
} CBZSHEADER, *PCBZSHEADER;

typedef struct {
    uint8_t scramble[8];
    uint64_t time;
} SESSIONENTRY, *PSESSIONENTRY;

uint8_t SessionCreate(const PSESSION session, const char* const path) {
    furi_check(session && path);

    if(storage_file_exists(session->storage, path)) {
        return SESSION_CREATE_DUPLICATE_FILE_NAME;
    }

    File* const file = storage_file_alloc(session->storage);

    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_close(file);
        storage_file_free(file);
        return SESSION_CREATE_FAILED_TO_CREATE_FILE;
    }

    const CBZSHEADER header = {CBZS_MAGIC_NUMBER, {0}};
    size_t written = 0;
    while(written < sizeof(CBZSHEADER))
        written += storage_file_write(file, &header + written, sizeof(CBZSHEADER) - written);

    if(session->file) {
        storage_file_close(session->file);
        storage_file_free(session->file);
    }

    session->file = file;
    return SESSION_CREATE_SUCCESS;
}

void SessionDelete(const PSESSION session) {
    furi_check(session);
    storage_common_remove(session->storage, furi_string_get_cstr(session->path));
    storage_file_close(session->file);
    storage_file_free(session->file);
    session->file = 0;
}

void SessionFree(const PSESSION session) {
    furi_check(session);

    if(session->file) {
        storage_file_close(session->file);
        storage_file_free(session->file);
        session->file = 0;
    }

    if(session->path) {
        furi_string_free(session->path);
        session->path = 0;
    }

    furi_record_close(RECORD_STORAGE);
    session->storage = 0;
}

void SessionInitialize(const PSESSION session) {
    furi_check(session);
    session->storage = furi_record_open(RECORD_STORAGE);
    session->path = furi_string_alloc();
    session->file = 0;
}

void SessionLoadSettings(const PSESSION session, const PSESSIONSETTINGS settings) {
    furi_check(session);

    if(!session->file) {
        return;
    }

    furi_check(settings);
    storage_file_read(session->file, settings, 1);
}

void SessionOpen(const PSESSION session) {
    furi_check(session);
}

void SessionRename(const PSESSION session) {
    furi_check(session);
}

void SessionSaveSettings(const PSESSION session, const PSESSIONSETTINGS settings) {
    furi_check(session);

    if(!session->file) {
        return;
    }

    furi_check(settings);
    storage_file_write(session->file, settings, sizeof(SESSIONSETTINGS));
}

/*#include "src/cuberzero.h"

uint8_t SessionOpen(const PCUBERZERO instance, const char* const path) {
	UNUSED(instance);
	UNUSED(path);
	Storage* storage = furi_record_open(RECORD_STORAGE);
	File* file = storage_file_alloc(storage);
	uint8_t codeExit = SESSION_SUCCESS;

	if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
		codeExit = SESSION_OPEN_FAILED;
		goto functionExit;
	}

	uint64_t size = storage_file_size(file);

	if(size < 4) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	CBZSHEADER header;

	if(storage_file_read(file, &header.magicNumber, 4) < 4) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	if(header.magicNumber != 0x43425A53) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	if(size < sizeof(CBZSHEADER)) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	storage_file_seek(file, 0, 1);

	if(storage_file_read(file, &header, sizeof(CBZSHEADER)) < sizeof(CBZSHEADER)) {
		codeExit = SESSION_INVALID_FORMAT;
		goto functionExit;
	}

	instance->session.file = file;
functionExit:
	storage_file_close(file);
	storage_file_free(file);
	furi_record_close(RECORD_STORAGE);
	return codeExit;
}

void SessionCleanup(const PCUBERZERO instance) {
	if(!instance->session.file) {
		return;
	}

	storage_file_close(instance->session.file);
	storage_file_free(instance->session.file);
}*/
