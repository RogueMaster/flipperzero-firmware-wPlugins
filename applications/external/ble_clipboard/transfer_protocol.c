#include "transfer_protocol.h"
#include "keyboard_layout.h"
#include "companion_version.h"
#include <string.h>

uint32_t clipboard_crc_update(uint32_t crc, const uint8_t* data, size_t size) {
    for(size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for(unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0);
    }
    return crc;
}

static void drop_job(ClipboardServer* server) {
    if(server->engine.state == ClipboardEngineRunning)
        clipboard_engine_cancel(&server->engine);
    else if(server->engine.keyboard.release)
        server->engine.keyboard.release(server->engine.keyboard.context);
    server->io.close(server->io.context);
    server->state = ClipboardJobIdle;
    server->job_id = 0;
    server->total = server->received = 0;
    server->flags = 0;
}

void clipboard_server_disconnect(ClipboardServer* server) {
    if(server->state != ClipboardJobRunning || !(server->flags & CLIPBOARD_STANDALONE))
        drop_job(server);
    server->authenticated = false;
    server->link_connected = false;
    server->frame_size = server->frame_expected = 0;
    memset(server->frame, 0, sizeof(server->frame));
}

static bool
    emit(ClipboardServer* server, ClipboardCommand command, const uint8_t* payload, size_t size) {
    uint8_t frame[128];
    if(size > sizeof(frame) - 4) return false;
    if(!server->link_connected) return true;
    frame[0] = CLIPBOARD_WIRE_VERSION;
    frame[1] = command;
    clipboard_write_u16(frame + 2, (uint16_t)size);
    if(size) memcpy(frame + 4, payload, size);
    if(server->io.send(server->io.context, frame, size + 4)) return true;
    clipboard_server_disconnect(server);
    return false;
}

static void emit_id(ClipboardServer* server, ClipboardCommand command) {
    uint8_t payload[4];
    clipboard_write_u32(payload, server->job_id);
    emit(server, command, payload, sizeof(payload));
}

static void
    reject(ClipboardServer* server, uint32_t id, uint8_t code, const char* message, bool discard) {
    uint8_t payload[124];
    clipboard_write_u32(payload, id);
    payload[4] = code;
    size_t size = strlen(message);
    if(size > sizeof(payload) - 5) size = sizeof(payload) - 5;
    memcpy(payload + 5, message, size);
    if(discard) drop_job(server);
    emit(server, ClipboardError, payload, size + 5);
}

void clipboard_server_cancel(ClipboardServer* server) {
    if(!server->job_id) return;
    uint8_t payload[5];
    clipboard_write_u32(payload, server->job_id);
    payload[4] = 1;
    drop_job(server);
    emit(server, ClipboardDone, payload, sizeof(payload));
}

void clipboard_server_init(ClipboardServer* server, ClipboardServerIO io, const char* device_name) {
    memset(server, 0, sizeof(*server));
    server->io = io;
    server->engine.keyboard =
        (ClipboardKeyboard){io.context, io.press, io.release, io.mouse_move, io.random};
    strncpy(
        server->device_name,
        device_name && device_name[0] ? device_name : "Flipper",
        sizeof(server->device_name) - 1);
}

static void start_job(ClipboardServer* server, uint32_t now) {
    ClipboardReader reader = {server->io.context, server->total, server->io.read};
    ClipboardKeyboard keyboard = {
        server->io.context,
        server->io.press,
        server->io.release,
        server->io.mouse_move,
        server->io.random};
    clipboard_engine_start(
        &server->engine,
        reader,
        keyboard,
        server->layout,
        server->layout_id == 1,
        server->layout_id == 1 && server->target == 0,
        now);
    server->state = ClipboardJobRunning;
    server->progress_at = now;
    server->started_at = now;
    server->execution_cursor = 0;
    emit_id(server, ClipboardStarted);
}

static void emit_active(ClipboardServer* server, uint32_t now) {
    if(server->state != ClipboardJobRunning || !(server->flags & CLIPBOARD_STANDALONE)) return;
    uint8_t payload[17];
    clipboard_write_u32(payload, server->job_id);
    clipboard_write_u32(payload + 4, server->total);
    clipboard_write_u32(payload + 8, now - server->started_at);
    clipboard_write_u32(payload + 12, clipboard_engine_mouse_remaining(&server->engine, now));
    payload[16] = server->flags;
    emit(server, ClipboardActive, payload, sizeof(payload));
}

bool clipboard_server_run_saved(
    ClipboardServer* server,
    const ClipboardSaved* saved,
    uint32_t id,
    uint32_t now) {
    if(server->state != ClipboardJobIdle || !id || !server->io.usb_connected(server->io.context))
        return false;
    ClipboardReader reader = {server->io.context, saved->size, server->io.read};
    if(!server->io.load_layout(server->io.context, saved->layout, saved->target, server->layout) ||
       !clipboard_saved_verify(saved, &reader) ||
       clipboard_program_validate(&reader, server->layout, saved->layout == 1) !=
           ClipboardProgramOk)
        return false;
    server->job_id = id;
    server->total = server->received = saved->size;
    server->layout_id = saved->layout;
    server->target = saved->target;
    server->flags = CLIPBOARD_STANDALONE;
    start_job(server, now);
    // Physical Run gets a focus allowance and never queues for a future USB connection.
    server->engine.due = now + 3000;
    return true;
}

static void command(
    ClipboardServer* server,
    uint8_t cmd,
    const uint8_t* payload,
    size_t size,
    uint32_t now) {
    uint32_t id = size >= 4 ? clipboard_read_u32(payload) : 0;
    if(cmd == ClipboardHello) {
        if(size != 9 || memcmp(payload, "BLECLIP/4", 9) != 0 ||
           (server->state != ClipboardJobIdle &&
            !(server->state == ClipboardJobRunning && (server->flags & CLIPBOARD_STANDALONE)))) {
            reject(server, 0, 1, "Protocol v4 handshake required", false);
            return;
        }
        server->authenticated = true;
        uint8_t ready[41];
        clipboard_write_u32(ready, CLIPBOARD_PROGRAM_MAX);
        clipboard_write_u16(ready + 4, CLIPBOARD_CHUNK_MAX);
        ready[6] = CLIPBOARD_LAYOUT_COUNT;
        ready[7] = 7;
        ready[8] = 63; // Existing capabilities plus optional, read-only companion info.
        size_t name_size = strlen(server->device_name);
        memcpy(ready + 9, server->device_name, name_size);
        if(emit(server, ClipboardReady, ready, 9 + name_size)) emit_active(server, now);
        return;
    }
    if(!server->authenticated) {
        reject(server, id, 1, "Handshake required", false);
        return;
    }
    if(cmd == ClipboardInfo) {
        if(size) {
            reject(server, 0, 2, "Invalid info query", false);
            return;
        }
        uint8_t info[13] = {1};
        clipboard_write_u16(info + 1, CLIPBOARD_APP_VERSION_MAJOR);
        clipboard_write_u16(info + 3, CLIPBOARD_APP_VERSION_MINOR);
        clipboard_write_u16(info + 5, CLIPBOARD_APP_VERSION_PATCH);
        info[7] = CLIPBOARD_WIRE_VERSION;
        clipboard_write_u16(info + 8, CLIPBOARD_BUILD_API_MAJOR);
        clipboard_write_u16(info + 10, CLIPBOARD_BUILD_API_MINOR);
        info[12] = CLIPBOARD_BUILD_TARGET;
        emit(server, ClipboardInfoReply, info, sizeof(info));
        return;
    }
    if(cmd == ClipboardBegin) {
        if(size != 15 || !id || server->state != ClipboardJobIdle) {
            reject(server, id, 2, "Busy or invalid job header", false);
            return;
        }
        uint32_t total = clipboard_read_u32(payload + 6);
        if(total < 6 || total > CLIPBOARD_PROGRAM_MAX || payload[4] >= CLIPBOARD_LAYOUT_COUNT ||
           payload[5] > 2 || payload[14] > 2) {
            reject(server, id, 2, "Unsupported job size, target or layout", false);
            return;
        }
        if(!server->io.load_layout(server->io.context, payload[4], payload[5], server->layout)) {
            reject(server, id, 3, "Keyboard layout unavailable", false);
            return;
        }
        if(!server->io.open(server->io.context, total)) {
            reject(server, id, 4, "Cannot create temporary job storage", true);
            return;
        }
        server->job_id = id;
        server->total = total;
        server->received = 0;
        server->expected_crc = clipboard_read_u32(payload + 10);
        server->crc = 0xFFFFFFFFU;
        server->layout_id = payload[4];
        server->target = payload[5];
        server->flags = payload[14];
        server->state = ClipboardJobReceiving;
        server->activity_at = now;
        emit_id(server, ClipboardBegun);
        return;
    }
    if(size < 4 || !id || id != server->job_id) {
        reject(server, id, 2, "No matching job", false);
        return;
    }
    if(cmd == ClipboardCancel && size == 4) {
        clipboard_server_cancel(server);
        return;
    }
    if(cmd == ClipboardChunk) {
        if(server->state != ClipboardJobReceiving || size <= 8 || size > CLIPBOARD_CHUNK_MAX + 8 ||
           clipboard_read_u32(payload + 4) != server->received ||
           size - 8 > server->total - server->received) {
            reject(server, id, 2, "Invalid chunk offset or size", true);
            return;
        }
        if(!server->io.write(server->io.context, server->received, payload + 8, size - 8)) {
            reject(server, id, 4, "Temporary job write failed", true);
            return;
        }
        server->crc = clipboard_crc_update(server->crc, payload + 8, size - 8);
        server->received += size - 8;
        server->activity_at = now;
        uint8_t ack[8];
        clipboard_write_u32(ack, id);
        clipboard_write_u32(ack + 4, server->received);
        emit(server, ClipboardChunked, ack, sizeof(ack));
        return;
    }
    if(cmd == ClipboardCommit && size == 4 && server->state == ClipboardJobReceiving) {
        if(server->received != server->total ||
           (server->crc ^ 0xFFFFFFFFU) != server->expected_crc) {
            reject(server, id, 5, "Incomplete job or checksum mismatch", true);
            return;
        }
        ClipboardReader reader = {server->io.context, server->total, server->io.read};
        ClipboardProgramResult result =
            clipboard_program_validate(&reader, server->layout, server->layout_id == 1);
        if(result != ClipboardProgramOk) {
            reject(server, id, 6, "Invalid program, unsupported text or unmapped key", true);
            return;
        }
        server->state = ClipboardJobVerified;
        server->activity_at = now;
        emit_id(server, ClipboardVerified);
        return;
    }
    if(cmd == ClipboardSave && server->state == ClipboardJobVerified &&
       server->flags == CLIPBOARD_SAVE_ONLY) {
        if(size < 22 || payload[20] == 0 || payload[20] >= CLIPBOARD_SAVED_NAME ||
           size != 21U + payload[20]) {
            reject(server, id, 2, "Invalid saved macro label", true);
            return;
        }
        ClipboardSaved saved = {
            .layout = server->layout_id,
            .target = server->target,
            .size = server->total,
            .crc = server->expected_crc};
        memcpy(saved.id, payload + 4, 16);
        memcpy(saved.name, payload + 21, payload[20]);
        uint8_t header[CLIPBOARD_SAVED_HEADER];
        if(!clipboard_saved_encode(&saved, header) || !server->io.save_program ||
           !server->io.save_program(server->io.context, &saved)) {
            reject(server, id, 4, "Save failed; check SD space or the 20-slot limit", true);
            return;
        }
        uint8_t reply[4];
        clipboard_write_u32(reply, server->job_id);
        drop_job(server);
        emit(server, ClipboardSavedReply, reply, sizeof(reply));
        return;
    }
    if(cmd == ClipboardExecute && size == 4 && server->state == ClipboardJobVerified &&
       !(server->flags & CLIPBOARD_SAVE_ONLY)) {
        if(server->io.usb_connected(server->io.context))
            start_job(server, now);
        else {
            server->state = ClipboardJobWaitingUSB;
            server->activity_at = now;
            emit_id(server, ClipboardWaitingUSB);
        }
        return;
    }
    reject(server, id, 2, "Command not valid for the current job", true);
}

void clipboard_server_feed(ClipboardServer* server, const uint8_t* data, size_t size, uint32_t now) {
    server->link_connected = true;
    for(size_t i = 0; i < size; i++) {
        server->frame[server->frame_size++] = data[i];
        if(server->frame[0] != CLIPBOARD_WIRE_VERSION) {
            clipboard_server_disconnect(server);
            reject(server, 0, 1, "Wire protocol v4 required", false);
            return;
        }
        if(server->frame_size == 4) {
            server->frame_expected = 4U + clipboard_read_u16(server->frame + 2);
            if(server->frame_expected > sizeof(server->frame)) {
                clipboard_server_disconnect(server);
                reject(server, 0, 2, "Frame too large", false);
                return;
            }
        }
        if(server->frame_expected && server->frame_size == server->frame_expected) {
            size_t length = server->frame_expected - 4;
            uint8_t cmd = server->frame[1];
            server->frame_size = server->frame_expected = 0;
            command(server, cmd, server->frame + 4, length, now);
            memset(server->frame, 0, sizeof(server->frame));
        }
    }
}

void clipboard_server_tick(ClipboardServer* server, uint32_t now, bool connected) {
    if(!connected) {
        if(server->link_connected || server->authenticated || server->frame_size)
            clipboard_server_disconnect(server);
        if(server->state != ClipboardJobRunning || !(server->flags & CLIPBOARD_STANDALONE)) return;
    } else
        server->link_connected = true;
    uint32_t elapsed = now - server->activity_at;
    if((server->state == ClipboardJobReceiving && elapsed >= 15000) ||
       ((server->state == ClipboardJobVerified || server->state == ClipboardJobWaitingUSB) &&
        elapsed >= 30000)) {
        reject(server, server->job_id, 7, "Job expired; no automatic execution", true);
        return;
    }
    if(server->state == ClipboardJobWaitingUSB && server->io.usb_connected(server->io.context))
        start_job(server, now);
    if(server->state != ClipboardJobRunning) return;
    clipboard_engine_tick(&server->engine, now, server->io.usb_connected(server->io.context));
    if(server->engine.cursor > server->execution_cursor)
        server->execution_cursor = (uint32_t)server->engine.cursor;
    if(server->engine.state == ClipboardEngineDone) {
        uint8_t payload[5];
        clipboard_write_u32(payload, server->job_id);
        payload[4] = 0;
        drop_job(server);
        emit(server, ClipboardDone, payload, sizeof(payload));
    } else if(server->engine.state == ClipboardEngineFailed) {
        reject(
            server, server->job_id, 8, "USB or storage failure; output may be incomplete", true);
    } else if(now - server->progress_at >= 250) {
        uint8_t payload[20];
        clipboard_write_u32(payload, server->job_id);
        clipboard_write_u32(payload + 4, server->execution_cursor);
        clipboard_write_u32(payload + 8, server->total);
        clipboard_write_u32(payload + 12, now - server->started_at);
        clipboard_write_u32(payload + 16, clipboard_engine_mouse_remaining(&server->engine, now));
        server->progress_at = now;
        emit(server, ClipboardProgress, payload, sizeof(payload));
    }
}
