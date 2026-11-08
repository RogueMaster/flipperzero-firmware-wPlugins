#include "CommandRouter.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace cmd {

bool CommandRouter::startsWith(const char* line, const char* prefix) {
    if(line == nullptr || prefix == nullptr) {
        return false;
    }
    const size_t p_len = strlen(prefix);
    return strncmp(line, prefix, p_len) == 0;
}

void CommandRouter::copyArgs(const char* src, char* dst, uint16_t dst_len) {
    if(dst == nullptr || dst_len == 0) {
        return;
    }
    dst[0] = '\0';
    if(src == nullptr) {
        return;
    }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

bool CommandRouter::parseCli(const char* line, Request& out) const {
    out = Request{};
    out.source = core::Source::CLI;

    if(line == nullptr) {
        return false;
    }

    while(*line == ' ') {
        ++line;
    }
    if(*line == '\0') {
        return false;
    }

    if(strcmp(line, "help") == 0) {
        out.type = RequestType::HELP;
        return true;
    }
    if(strcmp(line, "status") == 0) {
        out.type = RequestType::STATUS;
        return true;
    }
    if(strcmp(line, "ver") == 0 || strcmp(line, "version") == 0) {
        out.type = RequestType::VER;
        return true;
    }
    if(strcmp(line, "stats") == 0) {
        out.type = RequestType::STATS;
        return true;
    }
    if(strcmp(line, "bus show") == 0) {
        out.type = RequestType::BUS_SHOW;
        return true;
    }
    if(strcmp(line, "tool stop") == 0) {
        out.type = RequestType::TOOL_STOP;
        return true;
    }
    if(strcmp(line, "tool status") == 0) {
        out.type = RequestType::TOOL_STATUS;
        return true;
    }
    if(strcmp(line, "dbc clear") == 0) {
        out.type = RequestType::DBC_CLEAR;
        return true;
    }
    if(strcmp(line, "dbc list") == 0) {
        out.type = RequestType::DBC_LIST;
        return true;
    }
    if(strcmp(line, "led show") == 0) {
        out.type = RequestType::LED_SHOW;
        return true;
    }
    if(startsWith(line, "led set ")) {
        out.type = RequestType::LED_SET;
        copyArgs(line + strlen("led set "), out.args, sizeof(out.args));
        return true;
    }
    if(strcmp(line, "wifi show") == 0) {
        out.type = RequestType::WIFI_SHOW;
        return true;
    }
    if(strcmp(line, "wifi start") == 0) {
        out.type = RequestType::WIFI_CONFIG;
        copyArgs("ap=1", out.args, sizeof(out.args));
        return true;
    }
    if(strcmp(line, "wifi stop") == 0) {
        out.type = RequestType::WIFI_CONFIG;
        copyArgs("ap=0", out.args, sizeof(out.args));
        return true;
    }

    if(startsWith(line, "bus set ")) {
        out.type = RequestType::BUS_SET;
        copyArgs(line + strlen("bus set "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "bus filter set ")) {
        out.type = RequestType::BUS_SET_FILTER;
        copyArgs(line + strlen("bus filter set "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "bus filter clear ")) {
        out.type = RequestType::BUS_CLEAR_FILTER;
        copyArgs(line + strlen("bus filter clear "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "send ")) {
        out.type = RequestType::SEND_FRAME;
        copyArgs(line + strlen("send "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "tool start ")) {
        out.type = RequestType::TOOL_START;
        copyArgs(line + strlen("tool start "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "tool config ")) {
        out.type = RequestType::TOOL_CONFIG;
        copyArgs(line + strlen("tool config "), out.args, sizeof(out.args));
        return true;
    }
    if(strcmp(line, "inject cancel") == 0) {
        out.type = RequestType::TOOL_CONFIG;
        copyArgs("cmd=cancel", out.args, sizeof(out.args));
        return true;
    }
    if(strcmp(line, "inject list") == 0) {
        out.type = RequestType::TOOL_CONFIG;
        copyArgs("cmd=list", out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "inject ")) {
        out.type = RequestType::TOOL_CONFIG;
        const int n =
            snprintf(out.args, sizeof(out.args), "cmd=inject %s", line + strlen("inject "));
        if(n <= 0 || n >= (int)sizeof(out.args)) {
            out.type = RequestType::INVALID;
        }
        return true;
    }
    if(startsWith(line, "dbc add ")) {
        out.type = RequestType::DBC_ADD;
        copyArgs(line + strlen("dbc add "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "dbc remove ")) {
        out.type = RequestType::DBC_REMOVE;
        copyArgs(line + strlen("dbc remove "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "wifi config ")) {
        out.type = RequestType::WIFI_CONFIG;
        copyArgs(line + strlen("wifi config "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "wifi set ")) {
        out.type = RequestType::WIFI_CONFIG;
        copyArgs(line + strlen("wifi set "), out.args, sizeof(out.args));
        return true;
    }
    if(startsWith(line, "wifi autostart ")) {
        out.type = RequestType::WIFI_CONFIG;
        const int n =
            snprintf(out.args, sizeof(out.args), "autostart=%s", line + strlen("wifi autostart "));
        if(n <= 0 || n >= (int)sizeof(out.args)) {
            out.type = RequestType::INVALID;
        }
        return true;
    }

    out.type = RequestType::INVALID;
    return true;
}

bool CommandRouter::parseFlipper(const core::FlipperPacket& packet, Request& out) const {
    out = Request{};
    out.source = core::Source::FLIPPER;
    out.flipper_cmd_id = packet.id;
    out.flipper_seq = packet.seq;
    out.payload_len = packet.len;

    if(packet.kind != core::FlipperPacketKind::CMD) {
        return false;
    }

    if(packet.len > BoardConfig::MAX_FLIPPER_PAYLOAD) {
        return false;
    }
    if(packet.len > 0) {
        memcpy(out.payload, packet.payload, packet.len);
    }

    switch(static_cast<core::FlipperCommandId>(packet.id)) {
    case core::FlipperCommandId::PING:
        out.type = RequestType::PING;
        return true;
    case core::FlipperCommandId::GET_INFO:
        out.type = RequestType::GET_INFO;
        return true;
    case core::FlipperCommandId::BUS_SET_CFG:
        out.type = RequestType::BUS_SET;
        return true;
    case core::FlipperCommandId::BUS_GET_CFG:
        out.type = RequestType::BUS_GET_CFG;
        return true;
    case core::FlipperCommandId::BUS_SET_FILTER:
        out.type = RequestType::BUS_SET_FILTER;
        return true;
    case core::FlipperCommandId::BUS_CLEAR_FILTER:
        out.type = RequestType::BUS_CLEAR_FILTER;
        return true;
    case core::FlipperCommandId::BUS_SEND_FRAME:
        out.type = RequestType::SEND_FRAME;
        return true;
    case core::FlipperCommandId::TOOL_START:
        out.type = RequestType::TOOL_START;
        return true;
    case core::FlipperCommandId::TOOL_STOP:
        out.type = RequestType::TOOL_STOP;
        return true;
    case core::FlipperCommandId::TOOL_STATUS:
        out.type = RequestType::TOOL_STATUS;
        return true;
    case core::FlipperCommandId::TOOL_CONFIG:
        out.type = RequestType::TOOL_CONFIG;
        return true;
    case core::FlipperCommandId::DBC_CLEAR:
        out.type = RequestType::DBC_CLEAR;
        return true;
    case core::FlipperCommandId::DBC_ADD_SIGNAL:
        out.type = RequestType::DBC_ADD;
        return true;
    case core::FlipperCommandId::DBC_REMOVE_SIGNAL:
        out.type = RequestType::DBC_REMOVE;
        return true;
    case core::FlipperCommandId::DBC_LIST:
        out.type = RequestType::DBC_LIST;
        return true;
    case core::FlipperCommandId::STATS_GET:
        out.type = RequestType::STATS;
        return true;
    case core::FlipperCommandId::WIFI_GET_CFG:
        out.type = RequestType::WIFI_SHOW;
        return true;
    case core::FlipperCommandId::WIFI_SET_CFG:
        out.type = RequestType::WIFI_CONFIG;
        return true;
    case core::FlipperCommandId::LED_SET_CFG:
        out.type = RequestType::LED_SET;
        return true;
    default:
        out.type = RequestType::INVALID;
        return true;
    }
}

} // namespace cmd
