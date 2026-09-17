#include "yrm100x_module.h"
#include "yrm100x_worker.h"
#include "yrm100x_module_cmd.h"
#include "saved_epc_functions.h"
#include <furi.h>
#include "debug_log.h"

/* Production diagnostics compile out through debug_log.h. */
#undef FURI_LOG_D
#undef FURI_LOG_I
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)

static const char* UHF_MOD_TAG = "UHF_MOD";

#if 0
static const char* debug_bank_name(BankType bank) {
    switch(bank) {
    case ReservedBank:
        return "Reserved";
    case EPCBank:
        return "EPC";
    case TIDBank:
        return "TID";
    case UserBank:
        return "User";
    default:
        return "?";
    }
}
#endif

/**
 * File that handles the YRM100 module
 * @author frux-c
 * @author modified by haffnerriley
*/

#define DELAY_MS  100
#define WAIT_TICK 4000 // max wait time in between each byte

// Error frames use command byte 0xFF regardless of which command triggered them.
#define FRAME_ERROR_CMD  0xFF
// Maximum number of stale/mismatched frames to discard while hunting for the
// response that actually belongs to the command we just sent.
#define MAX_STALE_FRAMES 4

// Only tag-interaction commands can legitimately return a 0xFF error frame. For
// all other commands (e.g. Set Select, Set/Get Power) a 0xFF frame is a stale
// reply left over from an earlier read/poll and must be discarded, not accepted.
static bool cmd_can_return_error_frame(uint8_t command) {
    switch(command) {
    case 0x22: // single poll
    case 0x27: // multiple poll
    case 0x39: // read tag memory
    case 0x49: // write tag memory
    case 0x82: // lock tag
    case 0x65: // kill tag
        return true;
    default:
        return false;
    }
}

/* A 0xFF frame has no command echo, so its error family identifies its owner. */
static bool error_frame_matches_command(uint8_t command, uint8_t error_code) {
    if(error_code == 0x16U) return cmd_can_return_error_frame(command);

    switch(command) {
    case 0x22U: // single poll
    case 0x27U: // multiple poll
        return error_code == 0x09U || error_code == 0x15U;
    case 0x39U: // read tag memory
        return error_code == 0x09U || (error_code & 0xF0U) == 0xA0U;
    case 0x49U: // write tag memory
        return error_code == 0x10U || (error_code & 0xF0U) == 0xB0U;
    case 0x82U: // lock tag
        return error_code == 0x13U || (error_code & 0xF0U) == 0xC0U;
    case 0x65U: // kill tag
        return error_code == 0x12U || (error_code & 0xF0U) == 0xD0U;
    default:
        return false;
    }
}

static M100ResponseType setup_and_send_rx_abortable(
    M100Module* module,
    uint8_t* cmd,
    size_t cmd_length,
    UHFWorker* worker) {
    if(!module || !module->uart || !module->uart->handle || !module->uart->buffer || !cmd ||
       cmd_length < 3U) {
        UHF_E(
            "UART",
            "CMD rejected: transport unavailable module=%p uart=%p cmd=%p len=%u",
            (void*)module,
            module ? (void*)module->uart : NULL,
            (void*)cmd,
            (unsigned int)cmd_length);
        uhf_debug_flush();
        return M100ValidationFail;
    }

    UHFUart* uart = module->uart;
    Buffer* buffer = uart->buffer;
    // The module's response latency at the edge of read range swings from ~1ms to
    // well over 30ms, so a reply to a PREVIOUS command can still be in flight and
    // arrive during this command's window, one command out of phase. Every module
    // response echoes the request command byte in data[2] (or 0xFF for an error
    // frame), so we discard any frame whose command byte does not match what we
    // just sent and keep waiting for the matching one. This re-synchronises the
    // request/response pipeline and stops stale frames/fragments from being
    // mis-attributed (which was corrupting bank data).
    uint8_t expected_cmd = (cmd_length > 2) ? cmd[2] : 0x00;
    bool allow_error_frame = cmd_can_return_error_frame(expected_cmd);

    // clear buffer (discard any stale bytes already sitting in the ring)
    uhf_buffer_reset(buffer);

    /* Avoid allocating a FuriString on every RFID command. */
    FURI_LOG_D(UHF_MOD_TAG, "TX len=%d cmd=0x%02X", (int)cmd_length, (unsigned int)expected_cmd);

    // send cmd
    uhf_uart_send_wait(uart, cmd, cmd_length);

    for(int frame = 0;; frame++) {
        // Bound the receive wait as before, but cooperate with the scheduler and let
        // live-read workers abort an in-flight poll. Without this check, the GUI can
        // set Stop and then block forever joining a worker stuck in this byte loop.
        uhf_uart_tick_reset(uart);
        uint32_t wait_started = furi_get_tick();
        int tick_count = 0;

        while(!uhf_is_buffer_closed(buffer)) {
            tick_count++;

            if(worker && uhf_worker_stop_requested(worker)) {
                return M100EmptyResponse;
            }

            if((furi_get_tick() - wait_started) >= 250U) {
                break;
            }

            furi_delay_ms(1);
        }
        uint8_t* data = uhf_buffer_get_data(buffer);
        size_t length = uhf_buffer_get_size(buffer);

        /* Keep the receive path allocation-free. */
        FURI_LOG_D(
            UHF_MOD_TAG,
            "RX len=%d ticks=%d cmd=0x%02X",
            (int)length,
            tick_count,
            (unsigned int)expected_cmd);

        // genuine no-response: nothing arrived within the first-byte timeout
        if(!length) {
            FURI_LOG_D(UHF_MOD_TAG, "Response: empty (waited %d ticks)", tick_count);
            return M100EmptyResponse;
        }

        // framing check: a truncated frame or a fragment left over from a prior
        // command's response that was split across a buffer reset
        if(length < 7U || data[0] != FRAME_START || data[length - 1] != FRAME_END) {
            FURI_LOG_W(
                UHF_MOD_TAG,
                "Response: framing fail, len=%d (waited %d ticks)",
                (int)length,
                tick_count);
            if(frame < MAX_STALE_FRAMES) {
                uhf_buffer_reset(buffer);
                continue;
            }
            return M100ValidationFail;
        }

        uint16_t response_payload_len = (uint16_t)(((uint16_t)data[3] << 8) | data[4]);

        // command-echo check: discard frames belonging to a previous command
        uint8_t rtn_cmd = (length >= 3) ? data[2] : 0x00;
        bool cmd_matches = (rtn_cmd == expected_cmd) ||
                           (allow_error_frame && rtn_cmd == FRAME_ERROR_CMD);
        if(!cmd_matches) {
            FURI_LOG_W(
                UHF_MOD_TAG,
                "Response: stale frame cmd=0x%02X expected=0x%02X, discarding",
                rtn_cmd,
                expected_cmd);
            if(frame < MAX_STALE_FRAMES) {
                uhf_buffer_reset(buffer);
                continue;
            }
            return M100ValidationFail;
        }

        // check if checksum is correct
        if(checksum(data + 1, length - 3) != data[length - 2]) {
            FURI_LOG_W(
                UHF_MOD_TAG,
                "Response: checksum fail, len=%d, expected=0x%02X, got=0x%02X",
                length,
                data[length - 2],
                checksum(data + 1, length - 3));
            if(frame < MAX_STALE_FRAMES) {
                uhf_buffer_reset(buffer);
                continue;
            }
            return M100ChecksumFail;
        }

        bool tag_error_frame = rtn_cmd == FRAME_ERROR_CMD;
        bool notification_reply = expected_cmd == 0x22U || expected_cmd == 0x27U;
        bool protocol_ok =
            (size_t)response_payload_len + 7U == length &&
            (data[1] == 0x01U || (!tag_error_frame && notification_reply && data[1] == 0x02U)) &&
            (!tag_error_frame ||
             (response_payload_len >= 1U && error_frame_matches_command(expected_cmd, data[5])));
        if(!protocol_ok) {
            FURI_LOG_W(UHF_MOD_TAG, "Response: stale/invalid protocol frame");
            if(frame < MAX_STALE_FRAMES) {
                uhf_buffer_reset(buffer);
                continue;
            }
            return M100ValidationFail;
        }
        FURI_LOG_D(UHF_MOD_TAG, "Response: OK, len=%d", length);
        return M100SuccessResponse;
    }
}

static M100ResponseType setup_and_send_rx(M100Module* module, uint8_t* cmd, size_t cmd_length) {
    return setup_and_send_rx_abortable(module, cmd, cmd_length, NULL);
}

M100ModuleInfo* m100_module_info_alloc() {
    return (M100ModuleInfo*)calloc(1, sizeof(M100ModuleInfo));
}

void m100_module_info_free(M100ModuleInfo* module_info) {
    if(!module_info) return;

    if(module_info->hw_version != NULL) free(module_info->hw_version);
    if(module_info->sw_version != NULL) free(module_info->sw_version);
    if(module_info->manufacturer != NULL) free(module_info->manufacturer);
    free(module_info);
}

M100Module* m100_module_alloc() {
    M100Module* module = (M100Module*)calloc(1, sizeof(M100Module));
    if(!module) return NULL;

    module->transmitting_power = DEFAULT_TRANSMITTING_POWER;
    module->max_transmitting_power = 2600U;
    module->region = DEFAULT_WORKING_REGION;
    // A bank is enabled only when the user explicitly selects it. Starting with
    // every bit set made the first write in a session attempt all four banks.
    module->write_mask = 0;

    module->info = m100_module_info_alloc();
    if(!module->info) {
        free(module);
        return NULL;
    }

    module->uart = uhf_uart_alloc();
    if(!module->uart) {
        m100_module_info_free(module->info);
        free(module);
        return NULL;
    }

    return module;
}

void m100_module_free(M100Module* module) {
    if(!module) return;

    m100_module_info_free(module->info);
    if(module->uart) uhf_uart_free(module->uart);
    free(module);
}

uint8_t checksum(const uint8_t* data, size_t length) {
    // CheckSum8 Modulo 256
    // Sum of Bytes % 256
    uint64_t sum_val = 0x00;
    for(size_t i = 0; i < length; i++) {
        sum_val += data[i];
    }
    return (uint8_t)(sum_val % 0x100);
}

uint16_t crc16_genibus(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF; // Initial value
    uint16_t polynomial = 0x1021; // CRC-16/GENIBUS polynomial

    for(size_t i = 0; i < length; i++) {
        crc ^= (data[i] << 8); // Move byte into MSB of 16bit CRC
        for(int j = 0; j < 8; j++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc ^ 0xFFFF; // Post-inversion
}

char* _m100_info_helper(M100Module* module, char** info) {
    if(!module || !module->uart || !module->uart->buffer || !info) return NULL;

    size_t frame_len = uhf_buffer_get_size(module->uart->buffer);
    if(frame_len < 8) return NULL;

    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    if(!data || data[0] != FRAME_START || data[frame_len - 1] != FRAME_END) return NULL;

    uint16_t payload_len = ((uint16_t)data[3] << 8) | data[4];
    if(payload_len < 1) return NULL;

    size_t text_len = (size_t)payload_len - 1U;
    size_t text_offset = 6U;
    if(text_offset + text_len + 2U > frame_len) return NULL;

    char* replacement = (char*)realloc(*info, text_len + 1U);
    if(!replacement) return NULL;

    memcpy(replacement, data + text_offset, text_len);
    replacement[text_len] = '\0';
    *info = replacement;
    return *info;
}

char* m100_get_hardware_version(M100Module* module) {
    if(setup_and_send_rx(module, (uint8_t*)&CMD_HW_VERSION.cmd[0], CMD_HW_VERSION.length) !=
       M100SuccessResponse)
        return NULL;
    return _m100_info_helper(module, &module->info->hw_version);
}
char* m100_get_software_version(M100Module* module) {
    if(setup_and_send_rx(module, (uint8_t*)&CMD_SW_VERSION.cmd[0], CMD_SW_VERSION.length) !=
       M100SuccessResponse)
        return NULL;
    return _m100_info_helper(module, &module->info->sw_version);
}
char* m100_get_manufacturers(M100Module* module) {
    if(setup_and_send_rx(module, (uint8_t*)&CMD_MANUFACTURERS.cmd[0], CMD_MANUFACTURERS.length) !=
       M100SuccessResponse)
        return NULL;
    return _m100_info_helper(module, &module->info->manufacturer);
}

M100ResponseType m100_single_poll(M100Module* module, UHFTag* uhf_tag, UHFWorker* worker) {
    M100ResponseType rp_type = setup_and_send_rx_abortable(
        module, (uint8_t*)&CMD_SINGLE_POLLING.cmd[0], CMD_SINGLE_POLLING.length, worker);
    if(rp_type != M100SuccessResponse) return rp_type;
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t response_size = uhf_buffer_get_size(module->uart->buffer);
    // Single-poll no-tag replies use cmd=0xFF and contain no PC/EPC/CRC fields.
    // Reject them before indexing data[6+] so they cannot become the software lock.
    if(response_size < 8) return M100ValidationFail;
    if(data[2] == FRAME_ERROR_CMD) {
        uint8_t err = response_size > 5 ? data[5] : 0x00;
        if(err != 0x09 && err != 0x15) {
            UHF_W("POLL", "error frame code=0x%02X len=%u", err, (unsigned int)response_size);
            uhf_debug_hex(UHFDebugWarn, "POLL", "RX_ERROR", data, response_size);
        }
        return M100NoTagResponse;
    }
    uint16_t pc = data[6];
    uint16_t crc = 0;
    // mask out epc length from protocol control
    size_t epc_len = pc;
    epc_len >>= 3;
    epc_len *= 2;
    if(epc_len > EPC_MAX_BANK_SIZE || response_size < epc_len + 12) {
        return M100ValidationFail;
    }
    // get protocol control
    pc <<= 8;
    pc += data[7];
    // get cyclic redundency check
    crc = data[8 + epc_len];
    crc <<= 8;
    crc += data[8 + epc_len + 1];
    // validate crc
    if(crc16_genibus(data + 6, epc_len + 2) != crc) return M100ValidationFail;
    uhf_tag_set_epc_pc(uhf_tag, pc);
    uhf_tag_set_epc_crc(uhf_tag, crc);
    uhf_tag_set_epc(uhf_tag, data + 8, epc_len);
    // RSSI is at data[5] per §2.3.2 frame layout (same as multi_poll) — used by
    // the single-tag live read screen as a proximity/aiming meter.
    uhf_tag->epc->rssi = (int8_t)data[5];

    UHF_I(
        "POLL",
        "SUCCESS rssi=%d pc=0x%04X crc=0x%04X epc_bytes=%u",
        (int)uhf_tag->epc->rssi,
        (unsigned int)uhf_tag->epc->pc,
        (unsigned int)uhf_tag->epc->crc,
        (unsigned int)uhf_tag->epc->size);
    uhf_debug_hex(UHFDebugInfo, "POLL", "EPC", uhf_tag->epc->data, uhf_tag->epc->size);

    return M100SuccessResponse;
}

M100ResponseType m100_multi_poll(M100Module* module, UHFTagWrapper* wrapper, UHFWorker* worker) {
    UHFUart* uart = module->uart;
    Buffer* buffer = uart->buffer;
    M100ResponseType result = M100EmptyResponse;

    // Stop any in-progress inventory so the module is in a clean state before
    // we send CMD_MULTIPLE_POLLING.  If the previous scan was aborted (user pressed
    // Back) the module may still be running rounds and the new command would race
    // with the tail of the old inventory.
    uhf_buffer_reset(buffer);
    uhf_uart_tick_reset(uart);
    uhf_uart_send_wait(
        uart, (uint8_t*)&CMD_STOP_MULTIPLE_POLLING.cmd[0], CMD_STOP_MULTIPLE_POLLING.length);
    // Drain the stop-ack (or any in-flight EPC/no-tag frames) — 20 rounds ~= 4 ms
    for(int i = 0; i < 20; i++) {
        uhf_uart_tick_reset(uart);
        while(!uhf_is_buffer_closed(buffer) && !uhf_uart_tick(uart)) {
        }
        if(uhf_is_buffer_closed(buffer)) break;
    }

    // Send CMD_MULTIPLE_POLLING once to start the inventory round
    uhf_buffer_reset(buffer);
    uhf_uart_tick_reset(uart);
    uhf_uart_send_wait(uart, (uint8_t*)&CMD_MULTIPLE_POLLING.cmd[0], CMD_MULTIPLE_POLLING.length);

    // Live multi-tag scanning session. Runs until the user stops it (worker state
    // -> Stop) or the tag list is full (UHF_TAG_WRAPPER_MAX_TAGS). When a tag is in
    // the field the module streams its EPC continuously and never emits a cmd=0xFF
    // no-tag frame, so there is deliberately NO time budget or "settle" cutoff — the
    // session stays live so tags brought into range mid-scan are picked up. Newly
    // discovered EPCs are appended to the shared wrapper; the view's redraw timer
    // reads it for the live # EPCs / current-tag display (issue #4).
    while(wrapper->tag_count < UHF_TAG_WRAPPER_MAX_TAGS) {
        if(uhf_worker_stop_requested(worker)) {
            FURI_LOG_I(UHF_MOD_TAG, "multi_poll: aborted by worker stop");
            break;
        }

        // Wait up to 200 ms for the next frame using a wall-clock timeout.
        // furi_get_tick() returns FreeRTOS ms ticks.  This avoids the
        // volatile-int race between the main thread's uhf_uart_tick decrement
        // and the ISR's uhf_uart_tick_reset, which could cause the 500-round
        // spin loop to exit prematurely.
        uint32_t t0 = furi_get_tick();
        while(!uhf_is_buffer_closed(buffer)) {
            if(furi_get_tick() - t0 >= 200) break;
            if(uhf_worker_stop_requested(worker)) break;
            furi_delay_tick(1);
        }
        bool frame_ready = uhf_is_buffer_closed(buffer);

        // Copy frame data to a local buffer and reset the shared buffer immediately.
        // The UART ISR discards all bytes while buffer->closed == true. The module
        // sends frames back-to-back at 115200 baud (~87 µs/byte), so we must reopen
        // the ISR window before doing any heavy work (malloc, CRC, memcmp).
        uint8_t frame[64];
        size_t length = 0;
        if(frame_ready) {
            size_t raw_len = uhf_buffer_get_size(buffer);
            length = raw_len < sizeof(frame) ? raw_len : sizeof(frame);
            memcpy(frame, uhf_buffer_get_data(buffer), length);
        }
        uhf_buffer_reset(buffer); // reopen ISR window ASAP
        uhf_uart_tick_reset(uart);
        uint8_t* data = frame;

        // No frame within the window — nothing to process this iteration; keep the
        // session alive and wait for the next frame.
        if(!frame_ready) {
            continue;
        }

        // §2.3.3: no-tag / CRC-error response — cmd=0xFF. Normal between tag reads
        // (Gen2 anti-collision) and continuously when the field is empty. Just skip
        // it and keep scanning.
        if(data[0] == FRAME_START && length >= 4 && data[2] == 0xFF) {
            continue;
        }

        // Validate frame structure and checksum
        if(length < 13 || data[0] != FRAME_START || data[length - 1] != FRAME_END) {
            FURI_LOG_W(UHF_MOD_TAG, "multi_poll: invalid frame, len=%d", (int)length);
            continue;
        }
        if(checksum(data + 1, length - 3) != data[length - 2]) {
            FURI_LOG_W(UHF_MOD_TAG, "multi_poll: frame checksum fail");
            continue;
        }

        // Parse EPC — same layout as m100_single_poll (data[6]=PC_hi, data[7]=PC_lo)
        uint16_t pc = data[6];
        size_t epc_len = (pc >> 3) * 2;
        pc = (uint16_t)((pc << 8) | data[7]);

        if(length < (size_t)(8 + epc_len + 4)) {
            FURI_LOG_W(
                UHF_MOD_TAG,
                "multi_poll: epc_len %d exceeds frame len %d",
                (int)epc_len,
                (int)length);
            continue;
        }

        uint16_t crc = (uint16_t)(((uint16_t)data[8 + epc_len] << 8) | data[8 + epc_len + 1]);
        if(crc16_genibus(data + 6, epc_len + 2) != crc) {
            FURI_LOG_W(UHF_MOD_TAG, "multi_poll: EPC CRC fail");
            continue;
        }

        uint8_t* epc_data = data + 8;
        // RSSI is at data[5] per §2.3.2 frame layout: BB type cmd PL_MSB PL_LSB RSSI ...
        int8_t rssi = (int8_t)data[5];

        // Deduplicate — skip if EPC already present in the list; but update RSSI (live)
        bool duplicate = false;
        for(size_t i = 0; i < wrapper->tag_count; i++) {
            UHFTag* existing = wrapper->tags[i];
            if(existing->epc != NULL && existing->epc->size == epc_len &&
               memcmp(existing->epc->data, epc_data, epc_len) == 0) {
                existing->epc->rssi = rssi; // live RSSI update
                duplicate = true;
                break;
            }
        }
        if(duplicate) {
            continue;
        }

        // Allocate new tag, populate EPC fields, add to wrapper list
        UHFTag* new_tag = uhf_tag_alloc();
        if(!new_tag) {
            FURI_LOG_E(UHF_MOD_TAG, "multi_poll: tag allocation failed");
            result = M100ValidationFail;
            break;
        }
        uhf_tag_reset(new_tag);
        uhf_tag_set_epc_pc(new_tag, pc);
        uhf_tag_set_epc_crc(new_tag, crc);
        uhf_tag_set_epc(new_tag, epc_data, epc_len);
        new_tag->epc->rssi = rssi;

        if(!uhf_tag_wrapper_add_tag(wrapper, new_tag)) {
            // Guard: shouldn't happen since loop condition checks tag_count
            FURI_LOG_W(UHF_MOD_TAG, "multi_poll: wrapper add failed");
            uhf_tag_free(new_tag);
            break;
        }
        result = M100SuccessResponse;
        FURI_LOG_I(UHF_MOD_TAG, "multi_poll: added unique tag %d", (int)wrapper->tag_count);
        // The view's own 300ms redraw timer snapshots the wrapper for the live
        // # EPCs / current-tag display, so the worker never posts view events during
        // a scan. That keeps this thread free of blocking view_dispatcher sends, so a
        // GUI-thread join at stop can never deadlock against it (ADR-0002).
    }

    // Always stop the inventory round regardless of exit path
    uhf_buffer_reset(buffer);
    uhf_uart_send_wait(
        uart, (uint8_t*)&CMD_STOP_MULTIPLE_POLLING.cmd[0], CMD_STOP_MULTIPLE_POLLING.length);
    // Drain the stop-ack frame (module responds quickly; 20 rounds is sufficient)
    for(int round = 0; round < 20; round++) {
        uhf_uart_tick_reset(uart);
        while(!uhf_is_buffer_closed(buffer) && !uhf_uart_tick(uart)) {
        }
        if(uhf_is_buffer_closed(buffer)) break;
    }
    uhf_uart_tick_reset(uart);

    FURI_LOG_I(
        UHF_MOD_TAG,
        "multi_poll: done, total tags=%d, result=%d",
        (int)wrapper->tag_count,
        (int)result);
    return result;
}

bool m100_set_select_mode(M100Module* module, uint8_t mode) {
    UHF_I("SELECT", "MODE request=0x%02X", mode);
    if(!module || mode > 0x02) return false;

    uint8_t cmd[8];
    memcpy(cmd, CMD_SET_SELECT_MODE.cmd, CMD_SET_SELECT_MODE.length);
    cmd[5] = mode;
    cmd[6] = checksum(cmd + 1, 5);
    cmd[7] = FRAME_END;

    M100ResponseType status = setup_and_send_rx(module, cmd, sizeof(cmd));

    if(status != M100SuccessResponse) return false;

    size_t len = uhf_buffer_get_size(module->uart->buffer);
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);

    if(!data || len < 8) return false;
    if(data[2] == 0xFF) return false;

    // Firmware variants differ in the command byte reported in this response;
    // the success payload is consistently 0x00.
    return data[5] == 0x00;
}

M100ResponseType m100_set_select(M100Module* module, UHFTag* uhf_tag) {
    UHF_T(
        "SELECT",
        "SET SELECT BEGIN epc_bytes=%u",
        (unsigned int)(uhf_tag && uhf_tag->epc ? uhf_tag->epc->size : 0U));
    if(uhf_tag && uhf_tag->epc) {
        uhf_debug_hex(UHFDebugTrace, "SELECT", "EPC", uhf_tag->epc->data, uhf_tag->epc->size);
    }

    // Set select
    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_SET_SELECT_PARAMETER.length;
    size_t mask_length_bytes = uhf_tag->epc->size;
    size_t mask_length_bits = mask_length_bytes * 8;
    // payload len == sel param len + ptr len + mask len + epc len
    size_t payload_len = 7 + mask_length_bytes;
    memcpy(cmd, CMD_SET_SELECT_PARAMETER.cmd, cmd_length);
    // set new length
    cmd_length = 12 + mask_length_bytes + 2;
    // set payload length
    cmd[3] = (payload_len >> 8) & 0xFF;
    cmd[4] = payload_len & 0xFF;
    // set select param
    cmd[5] = 0x01; // 0x00=rfu, 0x01=epc, 0x10=tid, 0x11=user
    // set ptr
    cmd[9] = 0x20; // epc data begins after 0x20
    // set mask length
    cmd[10] = mask_length_bits;
    // truncate
    cmd[11] = false;
    // set mask
    memcpy((void*)&cmd[12], uhf_tag->epc->data, mask_length_bytes);

    // set checksum
    cmd[cmd_length - 2] = checksum(cmd + 1, 11 + mask_length_bytes);
    // end frame
    cmd[cmd_length - 1] = FRAME_END;

    M100ResponseType rp_type = setup_and_send_rx(module, cmd, cmd_length);

    if(rp_type != M100SuccessResponse) return rp_type;

    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    if(data[5] != 0x00) return M100ValidationFail; // error if not 0

    return M100SuccessResponse;
}

void m100_enable_write_mask(M100Module* module, WriteMask mask) {
    module->write_mask |= mask;
}

void m100_disable_write_mask(M100Module* module, WriteMask mask) {
    module->write_mask &= ~mask;
}

bool m100_is_write_mask_enabled(M100Module* module, WriteMask mask) {
    return (module->write_mask & mask) == mask;
}

UHFTag* m100_get_select_param(M100Module* module) {
    uhf_buffer_reset(module->uart->buffer);
    // furi_hal_uart_set_irq_cb(FuriHalUartIdLPUART1, rx_callback, module->uart->buffer);
    // furi_hal_uart_tx(
    //     FuriHalUartIdUSART1,
    //     (uint8_t*)&CMD_GET_SELECT_PARAMETER.cmd,
    //     CMD_GET_SELECT_PARAMETER.length);
    // furi_delay_ms(DELAY_MS);
    // UHFTag* uhf_tag = uhf_tag_alloc();
    // uint8_t* data = buffer_get_data(module->uart->buffer);
    // size_t mask_length =
    // uhf_tag_set_epc(uhf_tag, data + 12, )
    // TODO : implement
    return NULL;
}

//Modified by William Riley Haffner (haffnerriley)
M100ResponseType m100_read_label_data_storage(
    M100Module* module,
    UHFTag* uhf_tag,
    BankType bank,
    uint32_t access_pwd,
    uint16_t word_count) {
    /*
        Will probably remove UHFTag as param and get it from get selected tag
    */
    if(bank == EPCBank) return M100SuccessResponse;

    UHF_T(
        "READ",
        "request bank=%u words=%u ap_nonzero=%d",
        (unsigned int)bank,
        (unsigned int)word_count,
        access_pwd != 0 ? 1 : 0);

    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_READ_LABEL_DATA_STORAGE_AREA.length;
    memcpy(cmd, CMD_READ_LABEL_DATA_STORAGE_AREA.cmd, cmd_length);
    // set access password
    cmd[5] = (access_pwd >> 24) & 0xFF;
    cmd[6] = (access_pwd >> 16) & 0xFF;
    cmd[7] = (access_pwd >> 8) & 0xFF;
    cmd[8] = access_pwd & 0xFF;
    // set mem bank
    cmd[9] = (uint8_t)bank;
    // set word counter (let the binary search control DL for all banks,
    // including ReservedBank; the old hardcode of 4 prevented the search
    // from ever probing other sizes)
    cmd[12] = (word_count >> 8) & 0xFF;
    cmd[13] = word_count & 0xFF;
    // calc checksum
    cmd[cmd_length - 2] = checksum(cmd + 1, cmd_length - 3);

    M100ResponseType rp_type = setup_and_send_rx(module, cmd, cmd_length);
    if(rp_type != M100SuccessResponse) {
        UHF_W(
            "READ",
            "transport fail bank=%u words=%u status=%d",
            (unsigned int)bank,
            (unsigned int)word_count,
            (int)rp_type);
        return rp_type;
    }

    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);

    uint8_t rtn_command = data[2];
    uint16_t payload_len = data[3];
    payload_len = (payload_len << 8) + data[4];

    if(rtn_command == 0xFF) {
        // The error code is the first payload byte (data[5]); it is NOT implied by
        // the payload length. A 0x09 (tag-not-found / EPC mismatch) frame can carry
        // a PC+EPC echo, giving it the SAME payload_len (0x10) as a real 0xA3
        // memory-overrun. Classifying by length alone made every transient RF miss
        // look like the bank-length boundary and truncated the read. Distinguish by
        // the actual code byte instead.
        uint8_t error_code = (payload_len >= 1) ? data[5] : 0x09;
        UHF_T(
            "READ",
            "error bank=%u words=%u code=0x%02X payload=%u",
            (unsigned int)bank,
            (unsigned int)word_count,
            error_code,
            (unsigned int)payload_len);
        if(error_code == 0xA3) return M100MemoryOverrun; // DL is past the end of the bank
        if(error_code == 0x16) return M100APWrong; // wrong access password
        // 0xA0|x = EPC Gen2 read errors
        if((error_code & 0xF0) == 0xA0) {
            uint8_t gen2_err = error_code & 0x0F;
            if(gen2_err == 0x02) return M100APWrong; // 0xA2: insufficient privileges
            if(gen2_err == 0x04) return M100MemoryLocked; // 0xA4: memory locked / not readable
            return M100MemoryOverrun; // 0xA3: out of range, etc.
        }
        // 0x09 and any other code are NOT a length boundary — report as no-tag so
        // the caller retries rather than truncating.
        return M100NoTagResponse;
    }

    size_t ptr_offset = 5 /*<-ptr offset*/ + uhf_tag_get_epc_size(uhf_tag) + 3 /*<-pc + ul*/;
    size_t bank_data_length = payload_len - (ptr_offset - 5 /*dont include the offset*/);
    UHF_T(
        "READ",
        "success bank=%u words=%u data_bytes=%u payload=%u",
        (unsigned int)bank,
        (unsigned int)word_count,
        (unsigned int)bank_data_length,
        (unsigned int)payload_len);

    if(bank == TIDBank) {
        uhf_tag_set_tid(uhf_tag, data + ptr_offset, bank_data_length);
    } else if(bank == UserBank) {
        uhf_tag_set_user(uhf_tag, data + ptr_offset, bank_data_length);
    } else if(bank == ReservedBank) {
        uhf_tag_set_kill_pwd(uhf_tag, data + ptr_offset, bank_data_length);
        uhf_tag_set_access_pwd(uhf_tag, data + ptr_offset, bank_data_length);
        // Store the full Reserved bank exactly as read (variable length, incl. any
        // words past the two passwords) for display/save. The passwords above remain
        // the authoritative bytes 0-3 / 4-7; this buffer is display-only.
        uhf_tag_set_reserved(uhf_tag, data + ptr_offset, bank_data_length);
    }

    return M100SuccessResponse;
}

//Created by William Riley Haffner (haffnerriley)
//Function that handles creating the lock parameter bytes in the lock payload
//Follows the Gen2 Standards: https://www.gs1.org/sites/default/files/docs/epc/Gen2_Protocol_Standard.pdf
uint32_t get_lock_param(uint32_t lock_param, BankType bank, LockType lockfunction) {
    if(lockfunction == Lock) {
        switch(bank) {
        case KillPwd:
            lock_param = 0x80200;
            break;
        case AccessPwd:
            lock_param = 0x20080;
            break;
        case EPCBank:
            lock_param = 0x08020;
            break;
        case TIDBank:
            lock_param = 0x02008;
            break;
        case FileZero:
            lock_param = 0x00802;
            break;
        default:
            return 0x02008;
        }
    } else if(lockfunction == PermaLock) {
        // PermaLock: mask=10 (apply), action=11 (not writable from any state)
        switch(bank) {
        case KillPwd:
            lock_param = 0x80300;
            break;
        case AccessPwd:
            lock_param = 0x200C0;
            break;
        case EPCBank:
            lock_param = 0x08030;
            break;
        case TIDBank:
            lock_param = 0x0200C;
            break;
        case FileZero:
            lock_param = 0x00803;
            break;
        default:
            return 0x200C0;
        }
    } else if(lockfunction == PermaUnlock) {
        // PermaUnlock: mask=10 (apply), action=01 (permanently writable, may never be locked)
        switch(bank) {
        case KillPwd:
            lock_param = 0x80100;
            break;
        case AccessPwd:
            lock_param = 0x20040;
            break;
        case EPCBank:
            lock_param = 0x08010;
            break;
        case TIDBank:
            lock_param = 0x02004;
            break;
        case FileZero:
            lock_param = 0x00801;
            break;
        default:
            return 0x20040;
        }
    } else {
        // Unlock: mask=10 (apply), action=00 (writable from open or secured state)
        switch(bank) {
        case KillPwd:
            lock_param = 0x80000;
            break;
        case AccessPwd:
            lock_param = 0x20000;
            break;
        case EPCBank:
            lock_param = 0x08000;
            break;
        case TIDBank:
            lock_param = 0x02000;
            break;
        case FileZero:
            lock_param = 0x00800;
            break;
        default:
            return 0x20000;
        }
    }

    return lock_param;
}
//Created by William Riley Haffner
//Handles locking the uhf tag
M100ResponseType m100_lock_label_data(
    M100Module* module,
    BankType bank,
    uint32_t access_pwd,
    LockType lockfunction) {
    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_LOCK_LABEL_DATA_STORE.length;
    uint32_t lock_param = 0;
    memcpy(cmd, CMD_LOCK_LABEL_DATA_STORE.cmd, cmd_length);

    cmd[5] = (access_pwd >> 24) & 0xFF;
    cmd[6] = (access_pwd >> 16) & 0xFF;
    cmd[7] = (access_pwd >> 8) & 0xFF;
    cmd[8] = access_pwd & 0xFF;

    lock_param = get_lock_param(lock_param, bank, lockfunction);

    cmd[9] = (lock_param >> 16) & 0xFF;
    cmd[10] = (lock_param >> 8) & 0xFF;
    cmd[11] = lock_param & 0xFF;

    // Calculate checksum
    cmd[cmd_length - 2] = checksum(cmd + 1, cmd_length - 3);

    // Set end frame
    cmd[cmd_length - 1] = FRAME_END;

    // Send command and receive response
    M100ResponseType rp_type = setup_and_send_rx(module, cmd, cmd_length);
    if(rp_type != M100SuccessResponse) return rp_type;

    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);

    // Validate response
    uint8_t rtn_command = data[2];
    uint16_t payload_len = data[3];
    payload_len = (payload_len << 8) + data[4];

    //Need to add a check here for an incorrect password. Look for an error code of some sort
    if(rtn_command == 0xFF) {
        if(payload_len == 0x01 && data[5] == 0x13)
            return M100NoTagResponse; // PL=1: no EPC, tag not found
        else if(data[5] == 0x13)
            return M100ValidationFail; // PL>1: found tag but Lock cmd got no RF response
        else if(data[5] == 0x16)
            return M100APWrong;
        else if((data[5] & 0xF0) == 0xC0) { // 0xC0|err = EPC Gen2 lock error
            uint8_t gen2_err = data[5] & 0x0F;
            if(gen2_err == 0x02) return M100APWrong; // 0xC2: insufficient privileges (wrong AP)
            if(gen2_err == 0x04)
                return M100MemoryLocked; // 0xC4: memory area is permanently locked
            return M100MemoryOverrun; // 0xC3: memory overrun, or other Gen2 error
        }
        return M100MemoryOverrun;
    }

    return M100SuccessResponse;
}

//Created by William Riley Haffner (haffnerriley)
//Kills the tag
M100ResponseType m100_kill_tag(M100Module* module, uint32_t kill_pwd) {
    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_INACTIVATE_KILL_TAG.length;
    memcpy(cmd, CMD_INACTIVATE_KILL_TAG.cmd, cmd_length);

    cmd[5] = (kill_pwd >> 24) & 0xFF;
    cmd[6] = (kill_pwd >> 16) & 0xFF;
    cmd[7] = (kill_pwd >> 8) & 0xFF;
    cmd[8] = kill_pwd & 0xFF;

    // Calculate checksum
    cmd[cmd_length - 2] = checksum(cmd + 1, cmd_length - 3);

    // Set end frame
    cmd[cmd_length - 1] = FRAME_END;

    // Send command and receive response
    M100ResponseType rp_type = setup_and_send_rx(module, cmd, cmd_length);
    if(rp_type != M100SuccessResponse) return rp_type;

    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);

    // Validate response
    uint8_t rtn_command = data[2];
    uint16_t payload_len = data[3];
    payload_len = (payload_len << 8) + data[4];

    if(rtn_command == 0xFF) {
        if(payload_len == 0x01 || data[5] == 0x12)
            return M100NoTagResponse;
        else if(data[5] == 0x16)
            return M100APWrong;
        return M100MemoryOverrun;
    }

    return M100SuccessResponse;
}

//Modified by William Riley Haffner to add TID and reserved write support
M100ResponseType m100_probe_label_data_storage(
    M100Module* module,
    const UHFTag* selected_tag,
    BankType bank,
    uint32_t access_pwd,
    uint16_t word_count,
    size_t* data_bytes) {
    if(data_bytes) *data_bytes = 0;

    if(!module || !selected_tag || !selected_tag->epc || selected_tag->epc->size == 0 ||
       word_count == 0) {
        return M100ValidationFail;
    }

    UHF_T(
        "SIZE",
        "probe request bank=%u words=%u ap_nonzero=%d",
        (unsigned int)bank,
        (unsigned int)word_count,
        access_pwd != 0 ? 1 : 0);

    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_READ_LABEL_DATA_STORAGE_AREA.length;
    memcpy(cmd, CMD_READ_LABEL_DATA_STORAGE_AREA.cmd, cmd_length);

    cmd[5] = (uint8_t)((access_pwd >> 24) & 0xFFU);
    cmd[6] = (uint8_t)((access_pwd >> 16) & 0xFFU);
    cmd[7] = (uint8_t)((access_pwd >> 8) & 0xFFU);
    cmd[8] = (uint8_t)(access_pwd & 0xFFU);
    cmd[9] = (uint8_t)bank;
    cmd[12] = (uint8_t)((word_count >> 8) & 0xFFU);
    cmd[13] = (uint8_t)(word_count & 0xFFU);
    cmd[cmd_length - 2] = checksum(cmd + 1, cmd_length - 3);

    M100ResponseType rp_type = setup_and_send_rx(module, cmd, cmd_length);
    if(rp_type != M100SuccessResponse) {
        UHF_W(
            "SIZE",
            "probe transport fail bank=%u words=%u status=%d",
            (unsigned int)bank,
            (unsigned int)word_count,
            (int)rp_type);
        return rp_type;
    }

    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t frame_size = uhf_buffer_get_size(module->uart->buffer);
    if(!data || frame_size < 7U) return M100ValidationFail;

    uint8_t rtn_command = data[2];
    uint16_t payload_len = (uint16_t)(((uint16_t)data[3] << 8) | data[4]);

    if(rtn_command == 0xFF) {
        uint8_t error_code = payload_len >= 1U ? data[5] : 0x09U;

        UHF_T(
            "SIZE",
            "probe error bank=%u words=%u code=0x%02X payload=%u",
            (unsigned int)bank,
            (unsigned int)word_count,
            (unsigned int)error_code,
            (unsigned int)payload_len);

        if(error_code == 0xA3) return M100MemoryOverrun;
        if(error_code == 0x16) return M100APWrong;

        if((error_code & 0xF0U) == 0xA0U) {
            uint8_t gen2_err = error_code & 0x0FU;
            if(gen2_err == 0x02U) return M100APWrong;
            if(gen2_err == 0x04U) return M100MemoryLocked;
            if(gen2_err == 0x03U) return M100MemoryOverrun;
            return M100ValidationFail;
        }

        if(error_code == 0x09U || error_code == 0x13U) return M100NoTagResponse;
        return M100ValidationFail;
    }

    size_t response_prefix = selected_tag->epc->size + 3U;
    if((size_t)payload_len < response_prefix) {
        UHF_W(
            "SIZE",
            "probe malformed payload bank=%u payload=%u prefix=%u",
            (unsigned int)bank,
            (unsigned int)payload_len,
            (unsigned int)response_prefix);
        return M100ValidationFail;
    }

    size_t returned_bytes = (size_t)payload_len - response_prefix;
    if(data_bytes) *data_bytes = returned_bytes;

    UHF_T(
        "SIZE",
        "probe success bank=%u words=%u returned=%u",
        (unsigned int)bank,
        (unsigned int)word_count,
        (unsigned int)returned_bytes);

    return M100SuccessResponse;
}

static M100ResponseType m100_decode_memory_error(uint8_t error_code, bool write_operation) {
    if(error_code == 0x16U) return M100APWrong;
    if(error_code == 0x09U || error_code == 0x13U) return M100NoTagResponse;

    const uint8_t family = error_code & 0xF0U;
    if((!write_operation && family == 0xA0U) ||
       (write_operation && (family == 0xB0U || family == 0xC0U))) {
        switch(error_code & 0x0FU) {
        case 0x02U:
            return M100APWrong;
        case 0x03U:
            return M100MemoryOverrun;
        case 0x04U:
            return M100MemoryLocked;
        default:
            return M100ValidationFail;
        }
    }

    return M100ValidationFail;
}

#if 0
static const char* m100_write_error_name(uint8_t error_code) {
    switch(error_code) {
    case 0x10U:
        return "WRITE_FAIL_NO_RESPONSE_OR_DATA_CRC";
    case 0x16U:
        return "WRONG_ACCESS_PASSWORD";
    case 0xB2U:
        return "INSUFFICIENT_PRIVILEGES";
    case 0xC2U:
        return "UNEXPECTED_LOCK_INSUFFICIENT_PRIVILEGES";
    case 0xB3U:
        return "MEMORY_OVERRUN";
    case 0xC3U:
        return "UNEXPECTED_LOCK_MEMORY_OVERRUN";
    case 0xB4U:
        return "MEMORY_LOCKED";
    case 0xC4U:
        return "UNEXPECTED_LOCK_MEMORY_LOCKED";
    case 0xBBU:
        return "INSUFFICIENT_POWER";
    case 0xBFU:
        return "NONSPECIFIC_TAG_ERROR";
    case 0x09U:
        return "UNEXPECTED_READ_FAIL";
    case 0x13U:
        return "UNEXPECTED_LOCK_FAIL";
    default:
        return "UNEXPECTED_WRITE_ERROR";
    }
}

/*
 * Decode every WRITE_BANK reply in one place.
 *
 * The M100 protocol may return the same raw error either as an 8-byte frame
 * (no EPC context) or as an extended frame containing UL/PC/EPC.  The frame
 * length does not change the meaning of data[5].  In particular, raw 0x10 is
 * Write Fail in both forms: the tag did not respond or the tag reported a data
 * CRC error.  Treat it as retryable, never as evidence that the bank is short.
 */
static M100ResponseType m100_decode_write_reply(
    const uint8_t* response,
    size_t frame_size,
    BankType bank,
    uint16_t start_word,
    uint16_t word_count) {
    if(!response || frame_size < 8U) {
        UHF_W(
            "WRITE",
            "MALFORMED bank=%s(%u) sa=%u words=%u frame=%u",
            debug_bank_name(bank),
            (unsigned int)bank,
            (unsigned int)start_word,
            (unsigned int)word_count,
            (unsigned int)frame_size);
        uhf_debug_flush();
        return M100ValidationFail;
    }

    if(response[0] != FRAME_START || response[1] != 0x01U ||
       response[frame_size - 1U] != FRAME_END ||
       checksum(response + 1U, frame_size - 3U) != response[frame_size - 2U]) {
        UHF_W(
            "WRITE",
            "MALFORMED ENVELOPE bank=%s(%u) sa=%u words=%u "
            "type=0x%02X frame=%u",
            debug_bank_name(bank),
            (unsigned int)bank,
            (unsigned int)start_word,
            (unsigned int)word_count,
            (unsigned int)response[1],
            (unsigned int)frame_size);
        uhf_debug_hex(UHFDebugWarn, "WRITE", "RX_MALFORMED", response, frame_size);
        uhf_debug_flush();
        return M100ValidationFail;
    }

    uint16_t payload_len = (uint16_t)(((uint16_t)response[3] << 8) | response[4]);
    size_t expected_size = (size_t)payload_len + 7U;
    if(expected_size != frame_size) {
        UHF_W(
            "WRITE",
            "MALFORMED bank=%s(%u) sa=%u words=%u "
            "declared_payload=%u expected_frame=%u actual_frame=%u",
            debug_bank_name(bank),
            (unsigned int)bank,
            (unsigned int)start_word,
            (unsigned int)word_count,
            (unsigned int)payload_len,
            (unsigned int)expected_size,
            (unsigned int)frame_size);
        uhf_debug_hex(UHFDebugWarn, "WRITE", "RX_MALFORMED", response, frame_size);
        uhf_debug_flush();
        return M100ValidationFail;
    }

    if(response[2] != FRAME_ERROR_CMD) {
        if(response[2] != 0x49U) {
            UHF_W(
                "WRITE",
                "UNEXPECTED RESPONSE bank=%s(%u) sa=%u words=%u cmd=0x%02X",
                debug_bank_name(bank),
                (unsigned int)bank,
                (unsigned int)start_word,
                (unsigned int)word_count,
                (unsigned int)response[2]);
            uhf_debug_hex(UHFDebugWarn, "WRITE", "RX_UNEXPECTED", response, frame_size);
            uhf_debug_flush();
            return M100ValidationFail;
        }

        /*
         * Canonical success payload:
         *   [UL][PC+EPC: UL bytes][Parameter=00]
         * so PL must be UL+2.  Do not accept a matching command byte alone as
         * success; a malformed/non-zero Parameter is not a completed write.
         */
        uint8_t ul = payload_len >= 1U ? response[5] : 0U;
        size_t parameter_index = 6U + (size_t)ul;
        uint16_t pc = payload_len >= 3U ? (uint16_t)(((uint16_t)response[6] << 8) | response[7]) :
                                          0U;
        size_t pc_ul = 2U + (size_t)(((pc >> 11) & 0x1FU) * 2U);
        bool success_layout = payload_len >= 4U && ul >= 2U && (ul & 1U) == 0U &&
                              payload_len == (uint16_t)ul + 2U && (size_t)ul == pc_ul &&
                              parameter_index == frame_size - 3U;

        if(!success_layout || response[parameter_index] != 0x00U) {
            UHF_W(
                "WRITE",
                "MALFORMED SUCCESS bank=%s(%u) sa=%u words=%u "
                "payload=%u ul=%u parameter=%s",
                debug_bank_name(bank),
                (unsigned int)bank,
                (unsigned int)start_word,
                (unsigned int)word_count,
                (unsigned int)payload_len,
                (unsigned int)ul,
                success_layout && response[parameter_index] == 0x00U ? "OK" : "INVALID");
            uhf_debug_hex(UHFDebugWarn, "WRITE", "RX_MALFORMED", response, frame_size);
            uhf_debug_flush();
            return M100ValidationFail;
        }

        return M100SuccessResponse;
    }

    if(payload_len < 1U) {
        UHF_W(
            "WRITE",
            "MALFORMED ERROR bank=%s(%u) sa=%u words=%u payload=0",
            debug_bank_name(bank),
            (unsigned int)bank,
            (unsigned int)start_word,
            (unsigned int)word_count);
        uhf_debug_hex(UHFDebugWarn, "WRITE", "RX_ERROR", response, frame_size);
        uhf_debug_flush();
        return M100ValidationFail;
    }

    /*
     * Canonical error payloads are either [error] or
     * [error][UL][PC+EPC: UL bytes].  Reject an internally inconsistent B3 so
     * corrupted context can never authorize a shorter User write.
     */
    bool error_layout = payload_len == 1U;
    if(payload_len >= 4U) {
        uint8_t ul = response[6];
        uint16_t pc = (uint16_t)(((uint16_t)response[7] << 8) | response[8]);
        size_t pc_ul = 2U + (size_t)(((pc >> 11) & 0x1FU) * 2U);
        error_layout = ul >= 2U && (ul & 1U) == 0U && payload_len == (uint16_t)ul + 2U &&
                       (size_t)ul == pc_ul;
    }

    if(!error_layout) {
        UHF_W(
            "WRITE",
            "MALFORMED ERROR bank=%s(%u) sa=%u words=%u payload=%u",
            debug_bank_name(bank),
            (unsigned int)bank,
            (unsigned int)start_word,
            (unsigned int)word_count,
            (unsigned int)payload_len);
        uhf_debug_hex(UHFDebugWarn, "WRITE", "RX_ERROR", response, frame_size);
        uhf_debug_flush();
        return M100ValidationFail;
    }

    uint8_t error_code = response[5];
    M100ResponseType mapped = M100ValidationFail;

    if(error_code == 0x10U) {
        mapped = M100NoTagResponse;
    } else if(error_code == 0x16U || (error_code == 0xB2U && payload_len > 1U)) {
        mapped = M100APWrong;
    } else if(error_code == 0xB3U && payload_len > 1U) {
        /*
         * MemoryOverrun is a tag reply and therefore must carry UL/PC/EPC
         * context.  A context-free B3 cannot safely change the fit boundary.
         */
        mapped = M100MemoryOverrun;
    } else if(error_code == 0xB4U && payload_len > 1U) {
        mapped = M100MemoryLocked;
    }

    UHF_W(
        "WRITE",
        "ERROR bank=%s(%u) sa=%u words=%u raw=0x%02X kind=%s "
        "payload=%u context=%s mapped=%d",
        debug_bank_name(bank),
        (unsigned int)bank,
        (unsigned int)start_word,
        (unsigned int)word_count,
        (unsigned int)error_code,
        m100_write_error_name(error_code),
        (unsigned int)payload_len,
        payload_len > 1U ? "PC_EPC" : "NONE",
        (int)mapped);
    uhf_debug_hex(UHFDebugWarn, "WRITE", "RX_ERROR", response, frame_size);
    uhf_debug_flush();

    return mapped;
}
#endif

/*
 * Compact strict WRITE decoder.  The old alpha45 implementation repeated a
 * large diagnostic block for every validation branch and pushed the FAP over
 * the loader's RAM limit. Keep the same compact protocol checks without logs.
 */
static M100ResponseType m100_decode_write_reply(
    const uint8_t* response,
    size_t frame_size,
    BankType bank,
    uint16_t start_word,
    uint16_t word_count) {
    M100ResponseType result = M100ValidationFail;
    uint8_t code = 0U;

    if(!response || frame_size < 8U || response[0] != FRAME_START || response[1] != 0x01U ||
       response[frame_size - 1U] != FRAME_END ||
       checksum(response + 1U, frame_size - 3U) != response[frame_size - 2U]) {
        goto done;
    }

    const uint16_t payload_len = (uint16_t)(((uint16_t)response[3] << 8) | response[4]);
    if((size_t)payload_len + 7U != frame_size) goto done;

    if(response[2] == 0x49U) {
        /* Success: [UL][PC+EPC][parameter=0]. */
        if(payload_len < 4U) goto done;
        const uint8_t ul = response[5];
        const size_t parameter = 6U + (size_t)ul;
        if(ul < 2U || (ul & 1U) != 0U || payload_len != (uint16_t)ul + 2U ||
           parameter != frame_size - 3U) {
            goto done;
        }

        const uint16_t pc = (uint16_t)(((uint16_t)response[6] << 8) | response[7]);
        if((size_t)ul != 2U + (size_t)(((pc >> 11) & 0x1FU) * 2U) || response[parameter] != 0U) {
            goto done;
        }

        return M100SuccessResponse;
    }

    if(response[2] != FRAME_ERROR_CMD || payload_len < 1U) goto done;
    code = response[5];

    /* 0x10 is transient Write Fail, never a bank-size result. */
    if(code == 0x10U) {
        result = M100NoTagResponse;
    } else if(code == 0x16U) {
        result = M100APWrong;
    } else if((code & 0xF0U) == 0xB0U && payload_len > 1U) {
        /* A Gen2 B* error must contain a structurally valid UL/PC/EPC. */
        if(payload_len < 4U) goto done;
        const uint8_t ul = response[6];
        const uint16_t pc = (uint16_t)(((uint16_t)response[7] << 8) | response[8]);
        if(ul < 2U || (ul & 1U) != 0U || payload_len != (uint16_t)ul + 2U ||
           (size_t)ul != 2U + (size_t)(((pc >> 11) & 0x1FU) * 2U)) {
            goto done;
        }

        switch(code & 0x0FU) {
        case 2U:
            result = M100APWrong;
            break;
        case 3U:
            result = M100MemoryOverrun;
            break;
        case 4U:
            result = M100MemoryLocked;
            break;
        default:
            break;
        }
    }

done:
    UHF_W(
        "WRITE",
        "reply bank=%u sa=%u words=%u code=%02X status=%d",
        (unsigned int)bank,
        (unsigned int)start_word,
        (unsigned int)word_count,
        (unsigned int)code,
        (int)result);
    if(response) uhf_debug_hex(UHFDebugTrace, "WRITE", "RX", response, frame_size);
    return result;
}

M100ResponseType m100_read_raw_words(
    M100Module* module,
    const UHFTag* selected_tag,
    BankType bank,
    uint16_t start_word,
    uint16_t word_count,
    uint32_t access_pwd,
    uint8_t* output,
    size_t output_capacity,
    size_t* output_size) {
    if(output_size) *output_size = 0U;
    if(!module || !selected_tag || !selected_tag->epc || selected_tag->epc->size == 0U ||
       !output || bank > UserBank || word_count == 0U || word_count > (USER_MAX_BANK_SIZE / 2U)) {
        return M100ValidationFail;
    }

    const size_t requested_bytes = (size_t)word_count * 2U;
    if(requested_bytes > output_capacity) return M100ValidationFail;

    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_READ_LABEL_DATA_STORAGE_AREA.length;
    memcpy(cmd, CMD_READ_LABEL_DATA_STORAGE_AREA.cmd, cmd_length);
    cmd[5] = (uint8_t)((access_pwd >> 24) & 0xFFU);
    cmd[6] = (uint8_t)((access_pwd >> 16) & 0xFFU);
    cmd[7] = (uint8_t)((access_pwd >> 8) & 0xFFU);
    cmd[8] = (uint8_t)(access_pwd & 0xFFU);
    cmd[9] = (uint8_t)bank;
    cmd[10] = (uint8_t)((start_word >> 8) & 0xFFU);
    cmd[11] = (uint8_t)(start_word & 0xFFU);
    cmd[12] = (uint8_t)((word_count >> 8) & 0xFFU);
    cmd[13] = (uint8_t)(word_count & 0xFFU);
    cmd[cmd_length - 2U] = checksum(cmd + 1U, cmd_length - 3U);

    M100ResponseType status = setup_and_send_rx(module, cmd, cmd_length);
    if(status != M100SuccessResponse) return status;

    uint8_t* response = uhf_buffer_get_data(module->uart->buffer);
    size_t frame_size = uhf_buffer_get_size(module->uart->buffer);
    if(!response || frame_size < 8U || response[0] != FRAME_START || response[1] != 0x01U ||
       response[frame_size - 1U] != FRAME_END ||
       checksum(response + 1U, frame_size - 3U) != response[frame_size - 2U]) {
        return M100ValidationFail;
    }

    uint16_t payload_len = (uint16_t)(((uint16_t)response[3] << 8) | response[4]);
    if((size_t)payload_len + 7U != frame_size) return M100ValidationFail;

    if(response[2] == 0xFFU) {
        if(payload_len < 1U) return M100ValidationFail;

        uint8_t error_code = response[5];
        if(payload_len == 1U) {
            /* A Gen2 A* tag reply must carry UL/PC/EPC context. */
            if((error_code & 0xF0U) == 0xA0U) return M100ValidationFail;
        } else {
            if(payload_len < 4U) return M100ValidationFail;

            uint8_t ul = response[6];
            uint16_t pc = (uint16_t)(((uint16_t)response[7] << 8) | response[8]);
            size_t expected_ul = selected_tag->epc->size + 2U;
            size_t pc_ul = 2U + (size_t)(((pc >> 11) & 0x1FU) * 2U);
            if(ul < 2U || (ul & 1U) != 0U || (size_t)ul != expected_ul || (size_t)ul != pc_ul ||
               payload_len != (uint16_t)ul + 2U ||
               memcmp(response + 9U, selected_tag->epc->data, selected_tag->epc->size) != 0) {
                return M100ValidationFail;
            }
        }

        return m100_decode_memory_error(error_code, false);
    }

    if(response[2] != 0x39U || payload_len < 3U) return M100ValidationFail;

    /* Success payload: [UL][PC+EPC: UL bytes][requested data]. */
    uint8_t ul = response[5];
    uint16_t pc = (uint16_t)(((uint16_t)response[6] << 8) | response[7]);
    size_t expected_ul = selected_tag->epc->size + 2U;
    size_t pc_ul = 2U + (size_t)(((pc >> 11) & 0x1FU) * 2U);
    size_t data_offset = 6U + (size_t)ul;
    if(ul < 2U || (ul & 1U) != 0U || (size_t)ul != expected_ul || (size_t)ul != pc_ul ||
       payload_len != (uint16_t)(1U + (size_t)ul + requested_bytes) ||
       data_offset + requested_bytes != frame_size - 2U ||
       memcmp(response + 8U, selected_tag->epc->data, selected_tag->epc->size) != 0) {
        return M100ValidationFail;
    }

    memcpy(output, response + data_offset, requested_bytes);
    if(output_size) *output_size = requested_bytes;
    return M100SuccessResponse;
}

M100ResponseType m100_write_raw_words(
    M100Module* module,
    BankType bank,
    uint16_t start_word,
    const uint8_t* data,
    size_t data_size,
    uint32_t access_pwd) {
    if(!module || !data || bank > UserBank || data_size == 0U || (data_size & 1U) != 0U ||
       data_size > USER_MAX_BANK_SIZE) {
        return M100ValidationFail;
    }

    uint8_t cmd[MAX_BUFFER_SIZE];
    memcpy(cmd, CMD_WRITE_LABEL_DATA_STORE.cmd, CMD_WRITE_LABEL_DATA_STORE.length);

    const uint16_t payload_len = (uint16_t)(9U + data_size);
    const uint16_t word_count = (uint16_t)(data_size / 2U);
    const size_t cmd_length = 7U + payload_len;
    cmd[3] = (uint8_t)((payload_len >> 8) & 0xFFU);
    cmd[4] = (uint8_t)(payload_len & 0xFFU);
    cmd[5] = (uint8_t)((access_pwd >> 24) & 0xFFU);
    cmd[6] = (uint8_t)((access_pwd >> 16) & 0xFFU);
    cmd[7] = (uint8_t)((access_pwd >> 8) & 0xFFU);
    cmd[8] = (uint8_t)(access_pwd & 0xFFU);
    cmd[9] = (uint8_t)bank;
    cmd[10] = (uint8_t)((start_word >> 8) & 0xFFU);
    cmd[11] = (uint8_t)(start_word & 0xFFU);
    cmd[12] = (uint8_t)((word_count >> 8) & 0xFFU);
    cmd[13] = (uint8_t)(word_count & 0xFFU);
    memcpy(cmd + 14U, data, data_size);
    cmd[cmd_length - 2U] = checksum(cmd + 1U, cmd_length - 3U);
    cmd[cmd_length - 1U] = FRAME_END;

    M100ResponseType status = setup_and_send_rx(module, cmd, cmd_length);
    if(status != M100SuccessResponse) return status;

    uint8_t* response = uhf_buffer_get_data(module->uart->buffer);
    size_t frame_size = uhf_buffer_get_size(module->uart->buffer);
    return m100_decode_write_reply(response, frame_size, bank, start_word, word_count);
}

M100ResponseType m100_write_label_data_storage(
    M100Module* module,
    UHFTag* saved_tag,
    UHFTag* selected_tag,
    BankType bank,
    uint16_t source_address,
    uint32_t access_pwd) {
    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_WRITE_LABEL_DATA_STORE.length;
    memcpy(cmd, CMD_WRITE_LABEL_DATA_STORE.cmd, cmd_length);
    uint16_t payload_len = 9;
    uint16_t data_length = 0;
    if(bank == ReservedBank) {
        //Handles writing the access password and uses the saved password
        if(source_address == 32) {
            source_address = 0;
            payload_len += 8;
            data_length = 8;
            memcpy(cmd + 14, uhf_tag_get_kill_pwd(saved_tag), 4);
            memcpy(cmd + 18, uhf_tag_get_access_pwd(saved_tag), 4);

        } else if(source_address == 1) {
            //For both
            source_address = 0;
            payload_len += 8;
            data_length = 8;
            memcpy(cmd + 14, uhf_tag_get_kill_pwd(saved_tag), 4);
            memcpy(cmd + 18, uhf_tag_get_access_pwd(saved_tag), 4);
        } else {
            payload_len += 4;
            data_length = 4;
            memcpy(cmd + 14, uhf_tag_get_kill_pwd(saved_tag), 4);
        }
    } else if(bank == EPCBank) {
        /*
         * EPC memory layout:
         *   word 0 = StoredCRC
         *   word 1 = PC
         *   word 2.. = EPC
         *
         * WriteSingle keeps its legacy source_address=0 path. Clone uses
         * source_address=1 so the frame writes only PC+EPC. That leaves
         * StoredCRC word 0 alone and lets the tag compute the correct CRC.
         */
        if(source_address == 1U) {
            payload_len += 2 + uhf_tag_get_epc_size(saved_tag);
            data_length = 2 + uhf_tag_get_epc_size(saved_tag);

            uint16_t pc = uhf_tag_get_epc_pc(saved_tag);
            cmd[14] = (uint8_t)((pc >> 8) & 0xFFU);
            cmd[15] = (uint8_t)(pc & 0xFFU);
            memcpy(cmd + 16, uhf_tag_get_epc(saved_tag), uhf_tag_get_epc_size(saved_tag));
        } else {
            payload_len += 4 + uhf_tag_get_epc_size(saved_tag);
            data_length = 4 + uhf_tag_get_epc_size(saved_tag);

            uint8_t tmp_arr[4];
            tmp_arr[0] = (uint8_t)((uhf_tag_get_epc_crc(selected_tag) >> 8) & 0xFFU);
            tmp_arr[1] = (uint8_t)(uhf_tag_get_epc_crc(selected_tag) & 0xFFU);
            tmp_arr[2] = (uint8_t)((uhf_tag_get_epc_pc(saved_tag) >> 8) & 0xFFU);
            tmp_arr[3] = (uint8_t)(uhf_tag_get_epc_pc(saved_tag) & 0xFFU);
            memcpy(cmd + 14, tmp_arr, 4);
            memcpy(cmd + 18, uhf_tag_get_epc(saved_tag), uhf_tag_get_epc_size(saved_tag));
        }
    } else if(bank == TIDBank) {
        payload_len += uhf_tag_get_tid_size(saved_tag);
        data_length = uhf_tag_get_tid_size(saved_tag);
        // set data
        memcpy(cmd + 14, uhf_tag_get_tid(saved_tag), uhf_tag_get_tid_size(saved_tag));
    } else if(bank == UserBank) {
        payload_len += uhf_tag_get_user_size(saved_tag);
        data_length = uhf_tag_get_user_size(saved_tag);
        // set data
        memcpy(cmd + 14, uhf_tag_get_user(saved_tag), uhf_tag_get_user_size(saved_tag));
    }

    // set payload length
    cmd[3] = (payload_len >> 8) & 0xFF;
    cmd[4] = payload_len & 0xFF;
    // set access password
    cmd[5] = (access_pwd >> 24) & 0xFF;
    cmd[6] = (access_pwd >> 16) & 0xFF;
    cmd[7] = (access_pwd >> 8) & 0xFF;
    cmd[8] = access_pwd & 0xFF;
    // set membank
    cmd[9] = (uint8_t)bank;
    // set source address
    cmd[10] = (source_address >> 8) & 0xFF;
    cmd[11] = source_address & 0xFF;
    // set data length
    size_t data_length_words = data_length / 2;
    cmd[12] = (data_length_words >> 8) & 0xFF;
    cmd[13] = data_length_words & 0xFF;
    // update cmd len
    cmd_length = 7 + payload_len;
    // calculate checksum
    cmd[cmd_length - 2] = checksum(cmd + 1, cmd_length - 3);
    cmd[cmd_length - 1] = FRAME_END;
    // send cmd
    M100ResponseType rp_type = setup_and_send_rx(module, cmd, cmd_length);
    if(rp_type != M100SuccessResponse) return rp_type;
    uint8_t* buff_data = uhf_buffer_get_data(module->uart->buffer);
    size_t buff_length = uhf_buffer_get_size(module->uart->buffer);
    return m100_decode_write_reply(
        buff_data, buff_length, bank, source_address, (uint16_t)data_length_words);
}

// Shared helper: write 2 words (4 bytes) to Reserved bank at given source address.
static M100ResponseType
    m100_write_reserved_words(M100Module* module, uint32_t current_ap, uint16_t sa, uint8_t* data) {
    uint8_t cmd[MAX_BUFFER_SIZE];
    size_t cmd_length = CMD_WRITE_LABEL_DATA_STORE.length;
    memcpy(cmd, CMD_WRITE_LABEL_DATA_STORE.cmd, cmd_length);
    uint16_t payload_len = 9 + 4; // 9-byte base + 4 data bytes
    cmd[3] = (payload_len >> 8) & 0xFF;
    cmd[4] = payload_len & 0xFF;
    cmd[5] = (current_ap >> 24) & 0xFF;
    cmd[6] = (current_ap >> 16) & 0xFF;
    cmd[7] = (current_ap >> 8) & 0xFF;
    cmd[8] = current_ap & 0xFF;
    cmd[9] = 0x00; // Reserved bank
    cmd[10] = (sa >> 8) & 0xFF;
    cmd[11] = sa & 0xFF;
    cmd[12] = 0x00; // DL high
    cmd[13] = 0x02; // DL = 2 words
    memcpy(cmd + 14, data, 4);
    cmd_length = 7 + payload_len;
    cmd[cmd_length - 2] = checksum(cmd + 1, cmd_length - 3);
    cmd[cmd_length - 1] = FRAME_END;
    M100ResponseType rp_type = setup_and_send_rx(module, cmd, cmd_length);
    if(rp_type != M100SuccessResponse) return rp_type;
    uint8_t* buff_data = uhf_buffer_get_data(module->uart->buffer);
    size_t buff_length = uhf_buffer_get_size(module->uart->buffer);
    return m100_decode_write_reply(buff_data, buff_length, ReservedBank, sa, 2U);
}

// Write new access password (words 2-3 of Reserved bank, SA=2, DL=2).
M100ResponseType m100_write_access_pwd(M100Module* module, uint32_t current_ap, uint8_t* new_ap) {
    return m100_write_reserved_words(module, current_ap, 2, new_ap);
}

// Write new kill password (words 0-1 of Reserved bank, SA=0, DL=2).
M100ResponseType
    m100_write_kill_pwd_only(M100Module* module, uint32_t current_ap, uint8_t* new_kp) {
    return m100_write_reserved_words(module, current_ap, 0, new_kp);
}

void m100_set_baudrate(M100Module* module, uint32_t baudrate) {
    size_t length = CMD_SET_COMMUNICATION_BAUD_RATE.length;
    uint8_t cmd[length];
    memcpy(cmd, CMD_SET_COMMUNICATION_BAUD_RATE.cmd, length);
    uint16_t br_mod = baudrate / 100; // module format
    cmd[6] = 0xFF & br_mod; // pow LSB
    cmd[5] = 0xFF & (br_mod >> 8); // pow MSB
    cmd[length - 2] = checksum(cmd + 1, length - 3);
    // setup_and_send_rx(module, cmd, length);
    uhf_uart_send_wait(module->uart, cmd, length);
    uhf_uart_set_baudrate(module->uart, baudrate);
    module->uart->baudrate = baudrate;
}

bool m100_set_working_region(M100Module* module, WorkingRegion region) {
    size_t length = CMD_SET_WORK_AREA.length;
    uint8_t cmd[length];
    memcpy(cmd, CMD_SET_WORK_AREA.cmd, length);
    cmd[5] = (uint8_t)region;
    cmd[length - 2] = checksum(cmd + 1, length - 3);
    M100ResponseType result = setup_and_send_rx(module, cmd, length);
    if(result != M100SuccessResponse) return false;
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t response_length = uhf_buffer_get_size(module->uart->buffer);
    if(response_length < 8 || data[2] != 0x07 || data[5] != 0x00) return false;
    module->region = region;
    return true;
}

bool m100_get_working_region(M100Module* module, WorkingRegion* region) {
    M100ResponseType result =
        setup_and_send_rx(module, (uint8_t*)&CMD_GET_WORK_AREA.cmd[0], CMD_GET_WORK_AREA.length);
    if(result != M100SuccessResponse) return false;
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t length = uhf_buffer_get_size(module->uart->buffer);
    if(length < 8 || data[2] != 0x08) return false;

    WorkingRegion actual = (WorkingRegion)data[5];
    if(actual != WR_CHINA_900 && actual != WR_US && actual != WR_EU && actual != WR_CHINA_800 &&
       actual != WR_KOREA) {
        return false;
    }
    module->region = actual;
    *region = actual;
    return true;
}

bool m100_get_transmitting_power(M100Module* module, uint16_t* power_raw) {
    M100ResponseType result = setup_and_send_rx(
        module, (uint8_t*)&CMD_GET_TRANSMITTING_POWER.cmd[0], CMD_GET_TRANSMITTING_POWER.length);
    if(result != M100SuccessResponse) return false;
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t length = uhf_buffer_get_size(module->uart->buffer);
    // Expect: BB 01 B7 00 02 HH LL cs 7E (9 bytes)
    if(length < 9 || data[2] != 0xB7) return false;
    *power_raw = ((uint16_t)data[5] << 8) | data[6];
    return true;
}

bool m100_set_transmitting_power(M100Module* module, uint16_t power) {
    size_t length = CMD_SET_TRANSMITTING_POWER.length;
    uint8_t cmd[length];
    memcpy(cmd, CMD_SET_TRANSMITTING_POWER.cmd, length);
    cmd[5] = (power >> 8) & 0xFF;
    cmd[6] = power & 0xFF;
    cmd[length - 2] = checksum(cmd + 1, length - 3);
    M100ResponseType result = setup_and_send_rx(module, cmd, length);
    if(result != M100SuccessResponse) return false;
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t response_length = uhf_buffer_get_size(module->uart->buffer);
    if(response_length < 8 || data[2] != 0xB6 || data[5] != 0x00) return false;
    module->transmitting_power = power;
    return true;
}

bool m100_get_query_params(M100Module* module, uint8_t* session, uint8_t* target) {
    M100ResponseType result = setup_and_send_rx(
        module, (uint8_t*)&CMD_GET_QUERY_PARAMETERS.cmd[0], CMD_GET_QUERY_PARAMETERS.length);
    if(result != M100SuccessResponse) return false;
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t length = uhf_buffer_get_size(module->uart->buffer);
    // Expect: BB 01 0D 00 02 b0 b1 cs 7E (9 bytes)
    if(length < 9 || data[2] != 0x0D) return false;
    *session = data[5] & 0x03;
    *target = (data[6] >> 7) & 0x01;
    return true;
}

// Set EPC Gen2 Query parameters (§2.14).
// Fixed fields: DR=8, M=1, TRext=pilot, Sel=ALL, Q=4.
// Byte 0: 0x10 | (session & 0x03)  — bits[1:0] = Session
// Byte 1: ((target & 0x01) << 7) | 0x20  — bit7=Target, bits[6:3]=Q=4
bool m100_set_query_params(M100Module* module, uint8_t session, uint8_t target) {
    uint8_t b0 = 0x10 | (session & 0x03);
    uint8_t b1 = ((target & 0x01) << 7) | 0x20;
    // Frame: BB 00 0E 00 02 b0 b1 checksum 7E
    uint8_t cmd[9] = {0xBB, 0x00, 0x0E, 0x00, 0x02, b0, b1, 0x00, 0x7E};
    cmd[7] = checksum(cmd + 1, 6); // sum(Type..params) & 0xFF
    M100ResponseType result = setup_and_send_rx(module, cmd, sizeof(cmd));
    if(result != M100SuccessResponse) return false;
    uint8_t* data = uhf_buffer_get_data(module->uart->buffer);
    size_t length = uhf_buffer_get_size(module->uart->buffer);
    return length >= 8 && data[2] == 0x0E && data[5] == 0x00;
}

bool m100_set_freq_hopping(M100Module* module, bool hopping) {
    UNUSED(module);
    UNUSED(hopping);
    return true;
}

bool m100_set_power(M100Module* module, uint8_t* power) {
    UNUSED(module);
    UNUSED(power);
    return true;
}

uint32_t m100_get_baudrate(M100Module* module) {
    return module->uart->baudrate;
}
