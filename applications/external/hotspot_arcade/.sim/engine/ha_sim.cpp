// Hosts the REAL engine (esp32/hotspot-arcade-fw/ha_games.h) off-target.
//
// The engine reaches the outside world only through 7 sink functions, which the
// firmware implements against AsyncWebServer and the UART. Here they append to an
// outbox queue instead, and ha_drain() hands it to the harness as JSON. That queue
// is the fidelity boundary: it carries exactly what the firmware would have sent.
#include "Arduino.h"

#include <string>
#include <vector>

static uint32_t g_millis = 0;
uint32_t millis() {
    return g_millis;
}

#include "../../esp32/hotspot_arcade_fw/ha_games.h"

static Engine engine;
static std::vector<std::string> g_outbox;
static std::string g_drained; // return buffer; must outlive the call

// Escape a C string for embedding as a JSON string value. Only nicknames and score
// reasons need this; every other payload is already JSON text and is spliced raw.
static std::string esc(const char* s) {
    std::string o;
    for(const char* p = s ? s : ""; *p; p++) {
        switch(*p) {
        case '"':
            o += "\\\"";
            break;
        case '\\':
            o += "\\\\";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            if((unsigned char)*p < 0x20) {
                char b[7];
                snprintf(b, sizeof(b), "\\u%04x", *p);
                o += b;
            } else {
                o += *p;
            }
        }
    }
    return o;
}

// --- the 7 sinks ---------------------------------------------------------------
// msg/json arguments are already valid JSON objects, so they are spliced in raw
// rather than escaped into a string. That keeps the drained payload directly
// usable as structured data on the JS side.

void haWsSendWs(uint32_t wsId, const String& msg) {
    g_outbox.push_back(
        "{\"to\":\"ws\",\"id\":" + std::to_string(wsId) + ",\"msg\":" + msg.str() + "}");
}

void haWsBroadcast(const String& msg) {
    g_outbox.push_back("{\"to\":\"all\",\"msg\":" + msg.str() + "}");
}

void haUartJoin(uint8_t pid, const char* nick) {
    g_outbox.push_back(
        "{\"to\":\"uart\",\"kind\":\"join\",\"pid\":" + std::to_string(pid) + ",\"nick\":\"" +
        esc(nick) + "\"}");
}

void haUartLeave(uint8_t pid) {
    g_outbox.push_back("{\"to\":\"uart\",\"kind\":\"leave\",\"pid\":" + std::to_string(pid) + "}");
}

void haUartScore(uint8_t pid, int delta, const char* reason) {
    g_outbox.push_back(
        "{\"to\":\"uart\",\"kind\":\"score\",\"pid\":" + std::to_string(pid) +
        ",\"delta\":" + std::to_string(delta) + ",\"reason\":\"" + esc(reason) + "\"}");
}

void haUartEvent(const String& json) {
    g_outbox.push_back("{\"to\":\"uart\",\"kind\":\"event\",\"json\":" + json.str() + "}");
}

void haUartRoundResult(const String& json) {
    g_outbox.push_back("{\"to\":\"uart\",\"kind\":\"round\",\"json\":" + json.str() + "}");
}

// --- exported C API ------------------------------------------------------------
extern "C" {

void ha_reset() {
    g_millis = 0;
    engine.reset();
}

void ha_tick(uint32_t now) {
    g_millis = now;
    engine.tick(now);
}

void ha_input(uint32_t wsId, const char* json) {
    engine.onInput(wsId, json);
}
void ha_disconnect(uint32_t wsId) {
    engine.onWsDisconnect(wsId);
}
void ha_select_game(int id) {
    engine.selectGame((uint8_t)id);
}
void ha_trivia_clear() {
    engine.triviaTopicsClear();
}
void ha_trivia_add_topic(const char* name) {
    engine.triviaAddTopic(name);
}
void ha_trivia_add_q(const char* json) {
    engine.triviaAddQ(json);
}
void ha_content_clear() {
    engine.contentClear();
}
void ha_content_pack(int game, const char* name) {
    engine.contentPack((uint8_t)game, name);
}
void ha_content_item(const char* json) {
    engine.contentItem(json);
}
void ha_round_end() {
    engine.roundEnd();
}
void ha_reset_scores() {
    engine.resetScores();
}

const char* ha_drain() {
    g_drained = "[";
    for(size_t i = 0; i < g_outbox.size(); i++) {
        if(i) g_drained += ",";
        g_drained += g_outbox[i];
    }
    g_drained += "]";
    g_outbox.clear();
    return g_drained.c_str();
}
}
