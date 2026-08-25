#include "call_runner.h"

#include "../utils/logger.h"

#define CALL_RUNNER_TIMEOUT_MS 30000

/** Map the app method enum to the FlipperHTTP protocol (HEAD falls back to GET). */
static HTTPMethod call_runner_http_method(const CallEntry* call) {
    switch(call->method) {
    case CallMethodPost:
        return POST;
    case CallMethodPut:
        return PUT;
    case CallMethodDelete:
        return DELETE;
    case CallMethodPatch:
        return PATCH;
    case CallMethodHead:
    case CallMethodGet:
    default:
        return GET;
    }
}

/** Append a string to a bounded buffer (strncat is disabled in the firmware API). */
static void call_runner_append(char* out, size_t out_size, size_t* pos, const char* src) {
    while(*src != '\0' && *pos < out_size - 1) {
        out[(*pos)++] = *src++;
    }
    out[*pos] = '\0';
}

/** Build the final URL: url + "?" + query (query may be empty). */
static void call_runner_build_url(const CallEntry* call, char* out, size_t out_size) {
    size_t pos = 0;
    call_runner_append(out, out_size, &pos, call->url);
    if(call->query[0] != '\0') {
        call_runner_append(out, out_size, &pos, "?");
        call_runner_append(out, out_size, &pos, call->query);
    }
}

/** RX line callback: keeps body lines only (protocol markers are skipped). */
static void call_runner_rx_line_cb(const char* line, void* context) {
    AppContext* app = context;
    if(app == NULL || line == NULL) {
        return;
    }

    // Authoritative request state: every line passes through here
    if(strstr(line, "SUCCESS]") != NULL) {
        app->call_received = true;
    } else if(strstr(line, "/END]") != NULL) {
        app->call_body_done = true;
        return;
    } else if(strstr(line, "[ERROR]") != NULL) {
        strncpy(app->call_error, line, sizeof(app->call_error) - 1);
        app->call_error[sizeof(app->call_error) - 1] = '\0';
        return;
    }

    static const char* const markers[] = {
        "[GET/",
        "[POST/",
        "[PUT/",
        "[PATCH/",
        "[DELETE/",
        "[SUCCESS]",
        "[INFO]",
        "[PING]",
        "[PONG]",
        "[CONNECTED]",
        "[DISCONNECTED]",
        "[FILE/",
        "[LED/"};
    for(size_t i = 0; i < COUNT_OF(markers); i++) {
        if(strncmp(line, markers[i], strlen(markers[i])) == 0) {
            return;
        }
    }

    // Append the body line plus a newline, sanitizing control characters.
    // The board sends CRLF line endings: a stray '\r' stops the TextBox
    // renderer (u8g2), so drop it and normalize other control bytes.
    for(const char* p = line; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if(c == '\r') {
            continue;
        }
        if(c < 0x20 || c == 0x7F) {
            c = ' ';
        }
        if(app->call_response_len >= sizeof(app->call_response) - 2) {
            app->call_response_truncated = true;
            break;
        }
        app->call_response[app->call_response_len++] = (char)c;
    }
    if(app->call_response_len < sizeof(app->call_response) - 2) {
        app->call_response[app->call_response_len++] = '\n';
        app->call_response[app->call_response_len] = '\0';
    }
}

bool call_runner_start(AppContext* app, const CallEntry* call) {
    furi_assert(app);
    furi_assert(call);

    app->call_status_code = 0;
    app->call_response_len = 0;
    app->call_response[0] = '\0';
    app->call_response_truncated = false;
    app->call_error[0] = '\0';
    app->call_received = false;
    app->call_body_done = false;
    app->call_in_progress = false;

    if(app->fhttp == NULL) {
        snprintf(app->call_error, sizeof(app->call_error), "ESP32 non collegato");
        return false;
    }

    char url[CALL_URL_MAX + CALL_QUERY_MAX + 2];
    call_runner_build_url(call, url, sizeof(url));

    // Reset the link state and capture the upcoming lines
    memset(app->fhttp->last_response, 0, RX_BUF_SIZE);
    app->fhttp->started_receiving = false;
    app->fhttp->state = IDLE;
    app->fhttp->user_rx_line_cb = call_runner_rx_line_cb;
    app->fhttp->user_callback_context = app;

    if(call->method == CallMethodHead) {
        logger_log("WARN HEAD non supportato dal protocollo: eseguito come GET");
    }

    HTTPMethod method = call_runner_http_method(call);
    bool sent;

    if(method == GET) {
        sent = flipper_http_request(
            app->fhttp, GET, url, call->headers[0] != '\0' ? call->headers : "", NULL);
    } else {
        // POST/PUT/PATCH/DELETE require JSON headers and payload fields
        const char* headers = call->headers[0] != '\0' ? call->headers : "{}";
        const char* payload = call->body[0] != '\0' ? call->body : "{}";
        sent = flipper_http_request(app->fhttp, method, url, headers, payload);
    }

    if(!sent) {
        app->fhttp->user_rx_line_cb = NULL;
        app->fhttp->user_callback_context = NULL;
        snprintf(app->call_error, sizeof(app->call_error), "Invio fallito (UART)");
        return false;
    }

    app->call_start_tick = furi_get_tick();
    app->call_in_progress = true;
    logger_log("START ok");
    return true;
}

bool call_runner_poll(AppContext* app) {
    furi_assert(app);

    if(!app->call_in_progress) {
        return true;
    }
    if(app->fhttp == NULL) {
        logger_log("POLL done (no board)");
        app->call_in_progress = false;
        return true;
    }

    uint32_t elapsed = furi_get_tick() - app->call_start_tick;

    // Terminal error captured by the RX line callback
    if(app->call_error[0] != '\0') {
        logger_log("POLL error");
        app->call_in_progress = false;
        app->fhttp->user_rx_line_cb = NULL;
        app->fhttp->user_callback_context = NULL;
        return true;
    }

    // Success marker followed by the end marker: the exchange is complete
    if(app->call_received && app->call_body_done) {
        logger_log("POLL done");
        app->call_status_code = app->fhttp->status_code;
        app->call_in_progress = false;
        app->fhttp->user_rx_line_cb = NULL;
        app->fhttp->user_callback_context = NULL;
        return true;
    }

    // Fallback for a lost line: the board-side state machine also terminates
    if(!app->call_received && app->fhttp->state == ISSUE) {
        logger_log("POLL issue");
        snprintf(app->call_error, sizeof(app->call_error), "Errore della board");
        app->call_in_progress = false;
        app->fhttp->user_rx_line_cb = NULL;
        app->fhttp->user_callback_context = NULL;
        return true;
    }

    if(elapsed >= CALL_RUNNER_TIMEOUT_MS) {
        logger_log("POLL timeout");
        snprintf(
            app->call_error,
            sizeof(app->call_error),
            "Timeout %s",
            app->call_received ? "(risposta incompleta)" : "(nessuna risposta)");
        app->call_in_progress = false;
        app->fhttp->user_rx_line_cb = NULL;
        app->fhttp->user_callback_context = NULL;
        return true;
    }

    return false;
}
