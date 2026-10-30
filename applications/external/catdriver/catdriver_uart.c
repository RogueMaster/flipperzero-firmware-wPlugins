#include "catdriver_uart.h"

static void removeSpaces(char* str) {
    int i = 0;
    while(isspace((unsigned char)str[i])) {
        i++;
    }

    int j = 0;
    while(str[i] != '\0') {
        str[j++] = str[i++];
    }
    str[j] = '\0';

    int len = strlen(str);
    while(len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

void catdriver_set_scan_mode(Context* ctx, ScanMode mode) {
    ctx->scan_mode = mode;
    const char* cmd = "MODE,ALL\n";
    if(mode == ScanModeWifi) {
        cmd = "MODE,WIFI\n";
    } else if(mode == ScanModeBle) {
        cmd = "MODE,BLE\n";
    }

    if(ctx->serial_handle_esp) {
        furi_hal_serial_tx(ctx->serial_handle_esp, (const uint8_t*)cmd, strlen(cmd));
    }
}

void catdriver_stop_scan(Context* ctx) {
    const char* cmd = "MODE,STOP\n";
    if(ctx->serial_handle_esp) {
        furi_hal_serial_tx(ctx->serial_handle_esp, (const uint8_t*)cmd, strlen(cmd));
    }
}

void catdriver_query_esp_info(Context* ctx) {
    ctx->esp_info_received = false;
    ctx->esp_version_info[0] = '\0';
    ctx->esp_query_tick = furi_get_tick();
    const char* cmd = "info\n";
    if(ctx->serial_handle_esp) {
        furi_hal_serial_tx(ctx->serial_handle_esp, (const uint8_t*)cmd, strlen(cmd));
    }
}

static char* split_csv_token(char** cursor) {
    if(!cursor || !*cursor) return NULL;
    char* token = *cursor;
    char* delim = strchr(token, ',');
    if(delim) {
        *delim = '\0';
        *cursor = delim + 1;
    } else {
        *cursor = NULL;
    }
    return token;
}

static void uart_parse_esp(void* context, char* line) {
    Context* ctx = context;

    if(!line || strlen(line) == 0) return;

    if(strncmp(line, "INFO,", 5) == 0) {
        strncpy(ctx->esp_version_info, line + 5, sizeof(ctx->esp_version_info) - 1);
        ctx->esp_version_info[sizeof(ctx->esp_version_info) - 1] = '\0';
        ctx->esp_info_received = true;
        return;
    } else if(strstr(line, "Marauder") != NULL || strstr(line, "CatDriver") != NULL) {
        strncpy(ctx->esp_version_info, line, sizeof(ctx->esp_version_info) - 1);
        ctx->esp_version_info[sizeof(ctx->esp_version_info) - 1] = '\0';
        ctx->esp_info_received = true;
        return;
    }

    AccessPoint ap;
    memset(&ap, 0, sizeof(AccessPoint));
    strncpy(ap.authmode, "[ESS]", MAX_AUTH_LENGTH);
    strncpy(ap.type, "WIFI", 8);

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));

    char* cursor = line;
    char* token = split_csv_token(&cursor);
    int i = 0;
    bool isAp = false;
    bool isBle = false;
    bool isValid = false;

    while(token != NULL) {
        switch(i) {
        case 0:
            if(strcmp(token, "AR") == 0) {
                isAp = true;
                isValid = true;
                strncpy(ap.type, "WIFI", 8);
            } else if(strcmp(token, "BT") == 0) {
                isAp = true;
                isBle = true;
                isValid = true;
                strncpy(ap.type, "BLE", 8);
                strncpy(ap.authmode, "[LE]", MAX_AUTH_LENGTH);
            } else if(strcmp(token, "PK") == 0) {
                isValid = true;
            }
            break;
        case 1:
            if(isAp && isValid) {
                removeSpaces(token);
                strncpy(ap.ssid, token, MAX_SSID_LENGTH);
                ap.ssid[MAX_SSID_LENGTH] = '\0';
            } else if(!isAp && isValid) {
                strncpy(pkt.recievedMac, token, MAX_BSSID_LENGTH);
                pkt.recievedMac[MAX_BSSID_LENGTH] = '\0';
            }
            break;
        case 2:
            if(isAp && isValid) {
                strncpy(ap.bssid, token, MAX_BSSID_LENGTH);
                ap.bssid[MAX_BSSID_LENGTH] = '\0';
            } else if(!isAp && isValid) {
                strncpy(pkt.sentMac, token, MAX_BSSID_LENGTH);
                pkt.sentMac[MAX_BSSID_LENGTH] = '\0';
            }
            break;
        case 3:
            if(isAp && isValid) {
                ap.rssi = atoi(token);
            }
            break;
        case 4:
            if(isAp && isValid) {
                ap.channel = atoi(token);
            }
            break;
        case 5:
            if(isAp && isValid) {
                removeSpaces(token);
                if(strlen(token) > 0) {
                    strncpy(ap.authmode, token, MAX_AUTH_LENGTH);
                    ap.authmode[MAX_AUTH_LENGTH] = '\0';
                }
            }
            break;
        }

        token = split_csv_token(&cursor);
        i++;
    }

    if(isAp && isValid) {
        // Validate basic parameters (for BLE channel is 0, for WiFi 1-165)
        if(strlen(ap.bssid) < 17 || ap.rssi > 0 || strlen(ap.ssid) < 1) {
            return;
        }
        if(!isBle && (ap.channel < 1 || ap.channel > 165)) {
            return;
        }

        furi_hal_rtc_get_datetime(&ap.datetime);

        if(isnan(ctx->gps_data.latitude) || isnan(ctx->gps_data.longitude)) {
            ap.latitude = 0;
            ap.longitude = 0;
        } else {
            ap.latitude = ctx->gps_data.latitude;
            ap.longitude = ctx->gps_data.longitude;
        }

        furi_mutex_acquire(ctx->mutex, FuriWaitForever);

        // 1. Check if already in the batch buffer
        bool in_batch = false;
        for(uint16_t j = 0; j < ctx->batch_count; j++) {
            if(strcmp(ctx->batch_buffer[j].bssid, ap.bssid) == 0) {
                in_batch = true;
                ctx->batch_buffer[j].rssi = ap.rssi;
                ctx->batch_buffer[j].channel = ap.channel;
                ctx->batch_buffer[j].datetime = ap.datetime;
                ctx->batch_buffer[j].latitude = ap.latitude;
                ctx->batch_buffer[j].longitude = ap.longitude;
                if(ap.authmode[0] != '\0') {
                    strncpy(ctx->batch_buffer[j].authmode, ap.authmode, MAX_AUTH_LENGTH);
                    ctx->batch_buffer[j].authmode[MAX_AUTH_LENGTH] = '\0';
                }
                break;
            }
        }

        if(!in_batch) {
            ctx->batch_buffer[ctx->batch_count] = ap;
            ctx->batch_count++;
            ctx->total_scanned_count++;

            // If batch reaches threshold (30 APs), flush immediately to file in append mode!
            if(ctx->batch_count >= BATCH_BUFFER_SIZE) {
                save_batch_file(ctx);
            }
        }

        // 2. Update recent APs circular window for UI
        bool in_recent = false;
        for(uint16_t j = 0; j < ctx->recent_count; j++) {
            if(strcmp(ctx->recent_aps[j].bssid, ap.bssid) == 0) {
                in_recent = true;
                ctx->recent_aps[j].rssi = ap.rssi;
                ctx->recent_aps[j].channel = ap.channel;
                ctx->recent_aps[j].datetime = ap.datetime;
                ctx->recent_aps[j].latitude = ap.latitude;
                ctx->recent_aps[j].longitude = ap.longitude;
                if(ap.authmode[0] != '\0') {
                    strncpy(ctx->recent_aps[j].authmode, ap.authmode, MAX_AUTH_LENGTH);
                    ctx->recent_aps[j].authmode[MAX_AUTH_LENGTH] = '\0';
                }
                break;
            }
        }

        if(!in_recent) {
            if(ctx->recent_count < RECENT_APS_SIZE) {
                ctx->recent_aps[ctx->recent_count] = ap;
                ctx->recent_count++;
            } else {
                memmove(
                    &ctx->recent_aps[0],
                    &ctx->recent_aps[1],
                    (RECENT_APS_SIZE - 1) * sizeof(AccessPoint));
                ctx->recent_aps[RECENT_APS_SIZE - 1] = ap;
            }
        }

        if(!ctx->pressedButton || ctx->recent_index >= ctx->recent_count) {
            ctx->recent_index = (ctx->recent_count > 0) ? (ctx->recent_count - 1) : 0;
            ctx->active_access_point = ctx->recent_aps[ctx->recent_index];
        }

        furi_mutex_release(ctx->mutex);
    } else if(!isAp && isValid) {
        if(strlen(pkt.recievedMac) < 17 || strlen(pkt.sentMac) < 17) {
            return;
        }

        furi_mutex_acquire(ctx->mutex, FuriWaitForever);
        for(uint16_t j = 0; j < ctx->recent_count; j++) {
            if(strcmp(ctx->recent_aps[j].bssid, pkt.recievedMac) == 0) {
                ctx->recent_aps[j].packetRxCount++;
                break;
            }
        }
        for(uint16_t j = 0; j < ctx->recent_count; j++) {
            if(strcmp(ctx->recent_aps[j].bssid, pkt.sentMac) == 0) {
                ctx->recent_aps[j].packetTxCount++;
                break;
            }
        }
        furi_mutex_release(ctx->mutex);
    }
}

static void uart_cb_esp(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    Context* ctx = (Context*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(ctx->rx_stream_esp, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(ctx->thread_esp), WorkerEvtRxDone);
    }
}

static int32_t uart_worker_esp(void* context) {
    Context* ctx = (Context*)context;

    size_t rx_offset = 0;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEvtStop) {
            break;
        }

        if(events & WorkerEvtRxDone) {
            size_t len = 0;
            do {
                len = furi_stream_buffer_receive(
                    ctx->rx_stream_esp,
                    ctx->rx_buf_esp + rx_offset,
                    RX_BUF_SIZE - 1 - rx_offset,
                    0);

                if(len > 0) {
                    rx_offset += len;
                    ctx->rx_buf_esp[rx_offset] = '\0';

                    char* line_current = (char*)ctx->rx_buf_esp;
                    while(1) {
                        while(*line_current == '\0' &&
                              line_current < (char*)ctx->rx_buf_esp + rx_offset - 1) {
                            line_current++;
                        }

                        char* newline = strchr(line_current, '\n');
                        if(newline) {
                            *newline = '\0';

                            int line_len = strlen(line_current);
                            if(line_len > 0 && line_current[line_len - 1] == '\r') {
                                line_current[line_len - 1] = '\0';
                            }

                            uart_parse_esp(ctx, line_current);

                            line_current = newline + 1;
                        } else {
                            if(line_current > (char*)ctx->rx_buf_esp) {
                                rx_offset = 0;
                                while(*line_current) {
                                    ctx->rx_buf_esp[rx_offset++] = *(line_current++);
                                }
                            }
                            break;
                        }
                    }
                }
            } while(len > 0);
        }
    }

    return 0;
}

static void uart_parse_gps(Context* ctx, char* line) {
    switch(minmea_sentence_id(line, false)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc frame;
        if(minmea_parse_rmc(&frame, line)) {
            ctx->gps_data.latitude = minmea_tocoord(&frame.latitude);
            ctx->gps_data.longitude = minmea_tocoord(&frame.longitude);
            ctx->gps_data.hour = frame.time.hours;
            ctx->gps_data.minute = frame.time.minutes;
            ctx->gps_data.second = frame.time.seconds;
        }
    } break;
    case MINMEA_SENTENCE_GGA: {
        struct minmea_sentence_gga frame;
        if(minmea_parse_gga(&frame, line)) {
            ctx->gps_data.latitude = minmea_tocoord(&frame.latitude);
            ctx->gps_data.longitude = minmea_tocoord(&frame.longitude);
            ctx->gps_data.hour = frame.time.hours;
            ctx->gps_data.minute = frame.time.minutes;
            ctx->gps_data.second = frame.time.seconds;
            ctx->gps_data.satelites = frame.satellites_tracked;
        }
    } break;

    case MINMEA_SENTENCE_GLL: {
        struct minmea_sentence_gll frame;
        if(minmea_parse_gll(&frame, line)) {
            ctx->gps_data.latitude = minmea_tocoord(&frame.latitude);
            ctx->gps_data.longitude = minmea_tocoord(&frame.longitude);
            ctx->gps_data.hour = frame.time.hours;
            ctx->gps_data.minute = frame.time.minutes;
            ctx->gps_data.second = frame.time.seconds;
        }
    } break;

    default:
        break;
    }
}

static void uart_cb_gps(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    Context* ctx = (Context*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(ctx->rx_stream_gps, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(ctx->thread_gps), WorkerEvtRxDone);
    }
}

static int32_t uart_worker_gps(void* context) {
    Context* ctx = (Context*)context;

    size_t rx_offset = 0;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEvtStop) {
            break;
        }

        if(events & WorkerEvtRxDone) {
            size_t len = 0;
            do {
                len = furi_stream_buffer_receive(
                    ctx->rx_stream_gps,
                    ctx->rx_buf_gps + rx_offset,
                    RX_BUF_SIZE - 1 - rx_offset,
                    0);

                if(len > 0) {
                    rx_offset += len;
                    ctx->rx_buf_gps[rx_offset] = '\0';

                    char* line_current = (char*)ctx->rx_buf_gps;
                    while(1) {
                        while(*line_current == '\0' &&
                              line_current < (char*)ctx->rx_buf_gps + rx_offset - 1) {
                            line_current++;
                        }

                        char* newline = strchr(line_current, '\n');
                        if(newline) {
                            *newline = '\0';
                            uart_parse_gps(ctx, line_current);
                            line_current = newline + 1;
                        } else {
                            if(line_current > (char*)ctx->rx_buf_gps) {
                                rx_offset = 0;
                                while(*line_current) {
                                    ctx->rx_buf_gps[rx_offset++] = *(line_current++);
                                }
                            }
                            break;
                        }
                    }
                }
            } while(len > 0);
        }
    }

    return 0;
}

void catdriver_uart_init(Context* ctx) {
    ctx->rx_stream_esp = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);

    ctx->thread_esp = furi_thread_alloc();
    furi_thread_set_name(ctx->thread_esp, "CatDriverUartWorkerESP");
    furi_thread_set_stack_size(ctx->thread_esp, 2048);
    furi_thread_set_context(ctx->thread_esp, ctx);
    furi_thread_set_callback(ctx->thread_esp, uart_worker_esp);

    furi_thread_start(ctx->thread_esp);

    ctx->serial_handle_esp = furi_hal_serial_control_acquire(UART_CH_ESP);
    furi_check(ctx->serial_handle_esp);
    furi_hal_serial_init(ctx->serial_handle_esp, 115200);
    furi_hal_serial_async_rx_start(ctx->serial_handle_esp, uart_cb_esp, ctx, false);

    if(UART_CH_ESP != UART_CH_GPS) {
        ctx->rx_stream_gps = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);

        ctx->thread_gps = furi_thread_alloc();
        furi_thread_set_name(ctx->thread_gps, "CatDriverUartWorkerGPS");
        furi_thread_set_stack_size(ctx->thread_gps, 2048);
        furi_thread_set_context(ctx->thread_gps, ctx);
        furi_thread_set_callback(ctx->thread_gps, uart_worker_gps);

        furi_thread_start(ctx->thread_gps);

        ctx->serial_handle_gps = furi_hal_serial_control_acquire(UART_CH_GPS);
        furi_check(ctx->serial_handle_gps);
        furi_hal_serial_init(ctx->serial_handle_gps, 9600);
        furi_hal_serial_async_rx_start(ctx->serial_handle_gps, uart_cb_gps, ctx, false);
    }
}

void catdriver_uart_deinit(Context* ctx) {
    furi_hal_serial_async_rx_stop(ctx->serial_handle_esp);
    furi_hal_serial_deinit(ctx->serial_handle_esp);
    furi_hal_serial_control_release(ctx->serial_handle_esp);

    if(UART_CH_ESP != UART_CH_GPS) {
        furi_hal_serial_async_rx_stop(ctx->serial_handle_gps);
        furi_hal_serial_deinit(ctx->serial_handle_gps);
        furi_hal_serial_control_release(ctx->serial_handle_gps);
    }

    furi_thread_flags_set(furi_thread_get_id(ctx->thread_esp), WorkerEvtStop);
    furi_thread_join(ctx->thread_esp);
    furi_thread_free(ctx->thread_esp);
    furi_stream_buffer_free(ctx->rx_stream_esp);

    if(UART_CH_ESP != UART_CH_GPS) {
        furi_thread_flags_set(furi_thread_get_id(ctx->thread_gps), WorkerEvtStop);
        furi_thread_join(ctx->thread_gps);
        furi_thread_free(ctx->thread_gps);
        furi_stream_buffer_free(ctx->rx_stream_gps);
    }
}
