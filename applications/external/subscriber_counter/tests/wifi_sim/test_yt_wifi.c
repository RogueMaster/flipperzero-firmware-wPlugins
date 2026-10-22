/* Drives yt_wifi.c against a scripted FlipperHTTP board. The board replies
 * with the exact lines of the FlipperHTTP firmware (FlipperHTTP.cpp/http.cpp). */
#include "yt_wifi.h"
#include "yt_parse.h"
#include "furi.h"
#include "vendor/flipper_http/flipper_http.h"
#include "expansion/expansion.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

uint32_t sim_tick = 1000;
size_t strlcpy(char* d, const char* s, size_t n) {
    size_t l = strlen(s);
    if(n) {
        size_t c = l < n - 1 ? l : n - 1;
        memcpy(d, s, c);
        d[c] = 0;
    }
    return l;
}
void* furi_record_open(const char* n) {
    (void)n;
    return (void*)1;
}
void furi_record_close(const char* n) {
    (void)n;
}
static int expansion_disabled;
void expansion_disable(Expansion* e) {
    (void)e;
    expansion_disabled++;
}
void expansion_enable(Expansion* e) {
    (void)e;
    expansion_disabled--;
}

/* --- scripted board --- */
static struct {
    bool present, saved_network, network_reachable;
    int http_status;
    const char* body;
    bool get_error;
    int saves, connects, pings;
    char last_url[400];
} board;
static FlipperHTTP fhttp;
static void say(const char* line) {
    fhttp.user_rx_line_cb(line, fhttp.user_callback_context);
}
FlipperHTTP* flipper_http_alloc(void) {
    memset(&fhttp, 0, sizeof fhttp);
    return &fhttp;
}
void flipper_http_free(FlipperHTTP* f) {
    (void)f;
}
bool flipper_http_send_command(FlipperHTTP* f, HTTPCommand c) {
    (void)f;
    if(!board.present) return true;
    if(c == HTTP_CMD_PING) {
        board.pings++;
        say("[PONG]");
    }
    if(c == HTTP_CMD_WIFI_CONNECT) {
        board.connects++;
        if(board.saved_network && board.network_reachable)
            say("[SUCCESS] Connected to Wifi.");
        else
            say("[ERROR] Failed to connect to Wifi.");
    }
    return true;
}
bool flipper_http_save_wifi(FlipperHTTP* f, const char* ssid, const char* pw) {
    (void)f;
    (void)ssid;
    (void)pw;
    board.saves++;
    if(board.network_reachable) {
        board.saved_network = true;
        say("[SUCCESS] WiFi settings saved and WiFi connected successfully.");
    } else
        say("[ERROR] Failed to connect to WiFi.");
    return true;
}
bool flipper_http_request(
    FlipperHTTP* f,
    HTTPMethod m,
    const char* url,
    const char* h,
    const char* p) {
    (void)f;
    (void)m;
    (void)h;
    (void)p;
    strlcpy(board.last_url, url, sizeof board.last_url);
    if(board.get_error) {
        say("[ERROR] GET Request Failed, error: connection refused");
        return true;
    }
    if(board.http_status == 0) return true; /* board stays silent -> timeout */
    char head[96];
    snprintf(
        head,
        sizeof head,
        "[GET/SUCCESS]{\"Status-Code\":%d,\"Content-Length\":%d}",
        board.http_status,
        (int)strlen(board.body));
    say(head);
    /* println(body): one callback per non-empty trimmed line */
    char* copy = strdup(board.body);
    char* save = NULL;
    for(char* line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        while(*line == ' ')
            line++;
        if(*line) say(line);
    }
    free(copy);
    say("[GET/END]");
    return true;
}

static const char* YT_OK =
    "{\n  \"items\": [\n    {\n      \"snippet\": {\n        \"title\": \"Pixel Workshop\"\n      },\n"
    "      \"statistics\": {\n        \"viewCount\": \"52317\",\n        \"subscriberCount\": \"948\",\n"
    "        \"hiddenSubscriberCount\": false,\n        \"videoCount\": \"64\"\n      }\n    }\n  ]\n}\n";
static const char* YT_BAD_KEY =
    "{\n  \"error\": {\n    \"code\": 400,\n    \"message\": \"API key not valid. Please pass a valid API key.\"\n  }\n}\n";

static YtWifiState run(YtWifi* w, int ms) {
    for(int i = 0; i < ms; i += 50) {
        sim_tick += 50;
        yt_wifi_tick(w);
    }
    return yt_wifi_state(w);
}
static void fetch(YtWifi* w, YtStats* stats, uint16_t* status, char* error) {
    char body[2048];
    assert(yt_wifi_get(
        w, "https://www.googleapis.com/youtube/v3/channels?part=snippet,statistics&id=x&key=y"));
    assert(run(w, 200) == YtWifiDone);
    assert(yt_wifi_take_result(w, body, sizeof body, status, error, 96));
    assert(yt_wifi_state(w) == YtWifiReady);
    yt_parse_response(body, *status, stats);
}

int main(void) {
    YtStats st;
    uint16_t status;
    char error[96];

    /* 1. Board with a saved network: ping -> connect -> GET 200 */
    memset(&board, 0, sizeof board);
    board.present = board.saved_network = board.network_reachable = true;
    board.http_status = 200;
    board.body = YT_OK;
    YtWifi* w = yt_wifi_alloc();
    assert(w && expansion_disabled == 1);
    yt_wifi_start(w, "", "");
    assert(run(w, 200) == YtWifiReady && board.saves == 0 && board.connects == 1);
    fetch(w, &st, &status, error);
    assert(
        status == 200 && st.ok && st.subscribers == 948 && st.views == 52317 && st.videos == 64);
    assert(strcmp(st.title, "Pixel Workshop") == 0 && error[0] == 0);
    puts("ok  saved network, GET 200, multi-line JSON parsed");

    /* 2. API error comes back as a normal response with status 400 */
    board.http_status = 400;
    board.body = YT_BAD_KEY;
    fetch(w, &st, &status, error);
    assert(
        status == 400 && !st.ok && st.result == YtParseHttpError &&
        strstr(st.error, "API key not valid"));
    puts("ok  HTTP 400 body -> API error message");

    /* 3. Board-side failure: [ERROR] instead of a response */
    board.get_error = true;
    assert(yt_wifi_get(w, "https://x"));
    assert(run(w, 100) == YtWifiDone);
    {
        char body[64];
        yt_wifi_take_result(w, body, sizeof body, &status, error, sizeof error);
    }
    assert(status == 0 && strstr(error, "GET Request Failed"));
    puts("ok  [ERROR] during GET -> status 0 + reason");

    /* 4. Board silent after the request -> timeout after 25 s */
    board.get_error = false;
    board.http_status = 0;
    assert(yt_wifi_get(w, "https://x"));
    assert(run(w, 24000) == YtWifiFetching);
    assert(run(w, 2000) == YtWifiDone);
    {
        char body[64];
        yt_wifi_take_result(w, body, sizeof body, &status, error, sizeof error);
    }
    assert(status == 0 && strcmp(error, "Request timed out") == 0);
    puts("ok  silent board -> timeout");

    /* 5. Body larger than the caller's buffer is reported */
    board.http_status = 200;
    board.body = YT_OK;
    assert(yt_wifi_get(w, "https://x") && run(w, 100) == YtWifiDone);
    {
        char small[32];
        assert(!yt_wifi_take_result(w, small, sizeof small, &status, error, sizeof error));
    }
    puts("ok  truncated body detected");
    yt_wifi_free(w);
    assert(expansion_disabled == 0);
    puts("ok  expansion service re-enabled on free");

    /* 6. No saved network, credentials in config -> save once, then ready */
    memset(&board, 0, sizeof board);
    board.present = true;
    board.network_reachable = true;
    w = yt_wifi_alloc();
    yt_wifi_start(w, "HomeNet", "secret");
    assert(run(w, 200) == YtWifiReady && board.connects == 1 && board.saves == 1);
    puts("ok  no saved network -> WIFI/SAVE once -> ready");

    /* 7. Network unreachable -> Failed with the board's reason */
    memset(&board, 0, sizeof board);
    board.present = true;
    yt_wifi_start(w, "HomeNet", "secret");
    assert(run(w, 200) == YtWifiFailed && board.saves == 1);
    assert(strstr(yt_wifi_error(w), "Failed to connect"));
    puts("ok  unreachable network -> failed with reason");

    /* 8. No board on the header -> 3 pings, then NoBoard */
    memset(&board, 0, sizeof board);
    yt_wifi_start(w, "", "");
    assert(run(w, 2500) == YtWifiStarting);
    assert(run(w, 1000) == YtWifiNoBoard);
    puts("ok  no board -> not found after 3 pings");

    /* 9. Not ready -> GET refused */
    assert(!yt_wifi_get(w, "https://x"));
    puts("ok  GET refused while not ready");
    yt_wifi_free(w);
    puts("ALL OK");
    return 0;
}
