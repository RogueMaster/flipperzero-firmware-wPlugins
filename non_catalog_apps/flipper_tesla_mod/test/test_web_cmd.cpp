/*
 * test_web_cmd.cpp — host unit tests for the dashboard command parser in
 * esp32/.firmware/web_cmd.h (JSON tokenizer, command message, wifi_cfg value).
 *
 * The header has no Arduino dependencies, so what a message means is checked
 * here; web_dashboard.cpp only feeds it WebSocket frames and POST bodies and
 * acts on the result.
 *
 * Build + run:  make -C test check
 */

#include <stdio.h>
#include <string.h>
#include <string>

#include "web_cmd.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if(cond) {                                        \
            g_pass++;                                     \
        } else {                                          \
            g_fail++;                                     \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while(0)

// cmd.value points into the message, so the message has to outlive it: a
// literal, or a std::string the caller keeps.
static bool parse(const char* msg, WebCmd* cmd) {
    return web_cmd_parse(msg, strlen(msg), cmd);
}

static bool parse(const std::string& msg, WebCmd* cmd) {
    return web_cmd_parse(msg.data(), msg.size(), cmd);
}

static std::string value_text(const WebCmd& cmd) {
    return cmd.value.p ? std::string(cmd.value.p, cmd.value.len) : std::string("<none>");
}

static WebJson json(const char* text) {
    return WebJson{text, strlen(text)};
}

// Byte-for-byte what the page's cmd() sends: JSON.stringify({cmd:c,value:v}).
static void test_page_messages() {
    struct {
        const char *msg, *name, *value;
    } cases[] = {
        {"{\"cmd\":\"mode\",\"value\":null}", "mode", "null"},
        {"{\"cmd\":\"nag\",\"value\":true}", "nag", "true"},
        {"{\"cmd\":\"nag\",\"value\":false}", "nag", "false"},
        {"{\"cmd\":\"14x_warning\",\"value\":false}", "14x_warning", "false"},
        {"{\"cmd\":\"hw_override\",\"value\":3}", "hw_override", "3"},
        {"{\"cmd\":\"apmv3_branch\",\"value\":255}", "apmv3_branch", "255"},
        {"{\"cmd\":\"disp_br\",\"value\":null}", "disp_br", "null"}, // parseInt('') is NaN
        {"{\"cmd\":\"sleep\",\"value\":120000}", "sleep", "120000"},
        {"{\"cmd\":\"sig_cfg\",\"value\":\"0x399,0,0,0xF,5,6,0x3,0,0,1\"}",
         "sig_cfg",
         "\"0x399,0,0,0xF,5,6,0x3,0,0,1\""},
        {"{\"cmd\":\"blackbox_delete\",\"value\":\"evt_0001\"}", "blackbox_delete", "\"evt_0001\""},
        {"{\"cmd\":\"blackbox_delete_all\",\"value\":true}", "blackbox_delete_all", "true"},
        {"{\"cmd\":\"wifi_cfg\",\"value\":{\"ssid\":\"Tesla-FSD\",\"pass\":\"***\",\"hidden\":false,"
         "\"sta_ssid\":\"\",\"sta_pass\":\"\"}}",
         "wifi_cfg",
         "{\"ssid\":\"Tesla-FSD\",\"pass\":\"***\",\"hidden\":false,\"sta_ssid\":\"\",\"sta_pass\":\"\"}"},
    };
    for(auto& c : cases) {
        WebCmd cmd;
        CHECK(parse(c.msg, &cmd), "parses: %s", c.msg);
        CHECK(cmd.is(c.name), "%s: name %s", c.msg, cmd.name);
        CHECK(value_text(cmd) == c.value, "%s: value %s", c.msg, value_text(cmd).c_str());
        CHECK(!cmd.bad_value && cmd.field == nullptr, "%s: clean state", c.msg);
    }

    WebCmd cmd;
    bool b = false;
    int n = 0;
    char s[40];
    CHECK(parse("{\"cmd\":\"nag\",\"value\":true}", &cmd) && cmd.boolean(&b) && b, "bool true");
    CHECK(parse("{\"cmd\":\"nag\",\"value\":false}", &cmd) && cmd.boolean(&b) && !b, "bool false");
    CHECK(
        parse("{\"cmd\":\"sleep\",\"value\":120000}", &cmd) && cmd.integer(&n) && n == 120000,
        "int");
    CHECK(
        parse("{\"cmd\":\"hw_override\",\"value\":0}", &cmd) && cmd.integer(&n) && n == 0,
        "int 0");
    CHECK(
        parse("{\"cmd\":\"blackbox_delete\",\"value\":\"evt_0001\"}", &cmd) &&
            cmd.string(s, sizeof(s)) && strcmp(s, "evt_0001") == 0,
        "string");
}

// The bug this parser fixes: the old dispatcher looked for "<name>" anywhere
// in the message, so data that spelled a command ran that command.
static void test_command_names_inside_data() {
    const char* names[] = {
        "nag",
        "precondition",
        "mode",
        "force_fsd",
        "fsd_unlock",
        "dump",
        "sleep",
        "bms",
        "wifi_cfg"};
    for(const char* n : names) {
        std::string name = n, q = "\"" + name + "\"";
        std::string msgs[] = {
            "{\"cmd\":\"wifi_cfg\",\"value\":{\"ssid\":" + q + ",\"pass\":\"***\"}}",
            "{\"cmd\":\"wifi_cfg\",\"value\":{\"ssid\":\"Car\",\"pass\":" + q + "}}",
            "{\"cmd\":\"wifi_cfg\",\"value\":{\"sta_ssid\":" + q + ",\"sta_pass\":\"\\\"" + name +
                "\\\"\"}}",
            "{\"cmd\":\"blackbox_delete\",\"value\":" + q + "}",
            "{\"cmd\":\"sig_cfg\",\"value\":\"\\\"" + name + "\\\":true\"}",
        };
        for(auto& m : msgs) {
            WebCmd cmd;
            CHECK(parse(m, &cmd), "parses: %s", m.c_str());
            CHECK(!cmd.is(n) || strcmp(n, "wifi_cfg") == 0, "%s ran as %s", m.c_str(), n);
        }
        WebCmd cmd;
        CHECK(parse(msgs[0], &cmd) && cmd.is("wifi_cfg"), "SSID %s: still wifi_cfg", n);
        CHECK(
            parse(msgs[3], &cmd) && cmd.is("blackbox_delete"),
            "event %s: still blackbox_delete",
            n);
    }

    // A "cmd" or "value" key nested in the value is not the message's own.
    WebCmd cmd;
    CHECK(
        parse("{\"cmd\":\"wifi_cfg\",\"value\":{\"cmd\":\"nag\",\"value\":true}}", &cmd) &&
            cmd.is("wifi_cfg"),
        "nested cmd key");
    CHECK(
        parse("{\"value\":{\"cmd\":\"nag\"},\"cmd\":\"sig_cfg\"}", &cmd) && cmd.is("sig_cfg"),
        "nested cmd key before the real one");
    CHECK(!parse("{\"value\":{\"cmd\":\"nag\"}}", &cmd), "only a nested cmd key: not a command");
    CHECK(
        parse("{\"cmd\":\"nag\",\"x\":{\"value\":false},\"value\":true}", &cmd) &&
            value_text(cmd) == "true",
        "value read from the message's own key");
}

static void test_layout_variations() {
    WebCmd cmd;
    bool b = false;
    CHECK(
        parse("{\"value\":true,\"cmd\":\"nag\"}", &cmd) && cmd.is("nag") && cmd.boolean(&b) && b,
        "value before cmd");
    CHECK(
        parse(" \r\n\t{ \"cmd\" : \"nag\" , \"value\" : false } \n", &cmd) && cmd.is("nag") &&
            cmd.boolean(&b) && !b,
        "whitespace everywhere");
    CHECK(
        parse("{\"cmd\":\"capability_recheck\"}", &cmd) && cmd.is("capability_recheck") &&
            cmd.value.p == nullptr,
        "no value member");
    CHECK(
        parse("{\"cmd\":\"nag\",\"extra\":[1,2,{\"a\":null}],\"value\":true}", &cmd) &&
            cmd.boolean(&b) && b,
        "unknown members are skipped");
    CHECK(parse("{\"cmd\":\"n\\u0061g\",\"value\":true}", &cmd) && cmd.is("nag"), "escaped name");
    CHECK(
        parse("{\"cmd\":\"nag\",\"cmd\":\"mode\"}", &cmd) && cmd.is("nag"),
        "repeated cmd: first counts");
    CHECK(parse("{\"cmd\":\"\",\"value\":1}", &cmd) && cmd.name[0] == '\0', "empty name");

    std::string long_name(WEB_CMD_NAME_MAX, 'a');
    CHECK(
        parse("{\"cmd\":\"" + long_name + "\"}", &cmd) && strlen(cmd.name) == WEB_CMD_NAME_MAX,
        "name at the limit");
    CHECK(
        parse("{\"cmd\":\"" + long_name + "b\"}", &cmd) && cmd.name[0] == '\0',
        "name over the limit matches nothing");
}

static void test_malformed_messages() {
    const char* bad[] = {
        "",
        "   ",
        "nag",
        "\"nag\"",
        "[\"nag\",true]",
        "{}",
        "{\"value\":true}",
        "{\"cmd\":nag}",
        "{\"cmd\":1,\"value\":true}",
        "{\"cmd\":null}",
        "{\"cmd\":[\"nag\"]}",
        "{'cmd':'nag'}",
        "{cmd:\"nag\"}",
        "{\"cmd\":\"nag\",\"value\":true",
        "{\"cmd\":\"nag\",\"value\":true}}",
        "{\"cmd\":\"nag\",\"value\":true}x",
        "{\"cmd\":\"nag\",\"value\":true}{\"cmd\":\"mode\"}",
        "{\"cmd\":\"nag\",\"value\":true,}",
        "{\"cmd\":\"nag\",,\"value\":true}",
        "{\"cmd\":\"nag\" \"value\":true}",
        "{\"cmd\" \"nag\"}",
        "{\"cmd\":\"nag\",\"value\":tru}",
        "{\"cmd\":\"nag\",\"value\":True}",
        "{\"cmd\":\"nag\",\"value\":truex}",
        "{\"cmd\":\"nag\",\"value\":undefined}",
        "{\"cmd\":\"sleep\",\"value\":012}",
        "{\"cmd\":\"sleep\",\"value\":1.}",
        "{\"cmd\":\"sleep\",\"value\":.5}",
        "{\"cmd\":\"sleep\",\"value\":1e}",
        "{\"cmd\":\"sleep\",\"value\":-}",
        "{\"cmd\":\"sleep\",\"value\":+1}",
        "{\"cmd\":\"sleep\",\"value\":0x10}",
        "{\"cmd\":\"nag\",\"value\":\"unterminated}",
        "{\"cmd\":\"nag\",\"value\":\"bad \\x escape\"}",
        "{\"cmd\":\"nag\",\"value\":\"\\u12\"}",
        "{\"cmd\":\"nag\",\"value\":\"\\u12G4\"}",
        "{\"cmd\":\"nag\",\"value\":\"raw\ttab\"}",
        "{\"cmd\":\"nag\",\"value\":\"raw\nnewline\"}",
        "{\"cmd\":\"nag\",\"value\":[1,2}",
        "{\"cmd\":\"nag\",\"value\":{\"a\":1]}",
        "{\"cmd\":\"nag\",\"value\":{\"a\"}}",
        "{\"cmd\":\"nag\",\"value\":{1:2}}",
        // Deeper than WEB_JSON_MAX_DEPTH (the message itself is level 1).
        "{\"cmd\":\"nag\",\"value\":[[[[1]]]]}",
    };
    for(const char* m : bad) {
        WebCmd cmd;
        CHECK(!parse(m, &cmd), "malformed accepted: %s", m);
    }
    WebCmd cmd;
    CHECK(parse("{\"cmd\":\"nag\",\"value\":[[[1]]]}", &cmd), "nesting at the depth limit");

    // A NUL inside the buffer is not the end of the message.
    std::string with_nul("{\"cmd\":\"nag\",\"value\":true}\0{", 27);
    CHECK(!parse(with_nul, &cmd), "NUL and more after the object");
    std::string nul_in_string("{\"cmd\":\"na\0g\"}", 14);
    CHECK(!parse(nul_in_string, &cmd), "raw NUL inside a string");
    CHECK(!web_cmd_parse(nullptr, 5, &cmd), "null buffer");
}

static void test_value_types() {
    WebCmd cmd;
    bool b = true;
    int n = 0;

    // Booleans: only the two literals. The old dispatcher read anything but
    // a literal true as "off".
    const char* not_bool[] = {"null", "1", "0", "\"true\"", "\"false\"", "[]", "{}"};
    for(const char* v : not_bool) {
        std::string m = std::string("{\"cmd\":\"nag\",\"value\":") + v + "}";
        CHECK(parse(m, &cmd), "parses: %s", m.c_str());
        CHECK(!cmd.boolean(&b) && cmd.bad_value, "not a bool: %s", v);
    }
    CHECK(parse("{\"cmd\":\"nag\"}", &cmd) && !cmd.boolean(&b) && cmd.bad_value, "bool absent");

    struct {
        const char* v;
        bool ok;
        int n;
    } ints[] = {
        {"0", true, 0},
        {"-0", true, 0},
        {"5", true, 5},
        {"-12", true, -12},
        {"2147483647", true, 2147483647},
        {"-2147483648", true, (int)-2147483648LL},
        {"2147483648", false, 0},
        {"-2147483649", false, 0},
        {"99999999999", false, 0},
        {"1.5", false, 0},
        {"1e3", false, 0},
        {"1.0", false, 0},
        {"null", false, 0},
        {"\"5\"", false, 0},
        {"true", false, 0},
    };
    for(auto& c : ints) {
        std::string m = std::string("{\"cmd\":\"sleep\",\"value\":") + c.v + "}";
        CHECK(parse(m, &cmd), "parses: %s", m.c_str());
        n = 12345;
        bool ok = cmd.integer(&n);
        CHECK(ok == c.ok && (!ok || n == c.n), "int %s: ok=%d n=%d", c.v, ok, n);
        CHECK(cmd.bad_value == !c.ok, "int %s: bad_value", c.v);
    }

    char s[9];
    CHECK(
        parse("{\"cmd\":\"x\",\"value\":\"12345678\"}", &cmd) && cmd.string(s, sizeof(s)) &&
            strcmp(s, "12345678") == 0,
        "string that just fits");
    CHECK(
        parse("{\"cmd\":\"x\",\"value\":\"123456789\"}", &cmd) && !cmd.string(s, sizeof(s)) &&
            cmd.bad_value,
        "string one byte too long is refused, not cut");
    CHECK(
        parse("{\"cmd\":\"x\",\"value\":5}", &cmd) && !cmd.string(s, sizeof(s)),
        "number is no string");
}

// Percentages and timeouts: limited on the int, before the caller narrows it
// to uint8_t / uint32_t. 300 used to become 44 and -1 a 49-day sleep timeout.
static void test_capped_integers() {
    WebCmd cmd;
    struct {
        const char* v;
        int min, cap;
        bool ok;
        int n;
    } c[] = {
        {"0", 0, 100, true, 0},
        {"55", 0, 100, true, 55},
        {"100", 0, 100, true, 100},
        {"101", 0, 100, true, 100},
        {"300", 0, 100, true, 100},
        {"256", 0, 100, true, 100},
        {"2147483647", 0, 100, true, 100},
        {"-1", 0, 100, false, 0},
        {"-256", 0, 100, false, 0},
        {"-2147483648", 0, 100, false, 0},
        {"3600", 0, 3600, true, 3600},
        {"7200", 0, 3600, true, 3600},
        {"-5", 0, 3600, false, 0},
        {"10000", 10000, INT32_MAX, true, 10000},
        {"9999", 10000, INT32_MAX, false, 0},
        {"-1", 10000, INT32_MAX, false, 0},
        {"2147483647", 10000, INT32_MAX, true, 2147483647},
        {"\"50\"", 0, 100, false, 0},
        {"50.5", 0, 100, false, 0},
        {"null", 0, 100, false, 0},
    };
    for(auto& t : c) {
        std::string m = std::string("{\"cmd\":\"disp_br\",\"value\":") + t.v + "}";
        CHECK(parse(m, &cmd), "parses: %s", m.c_str());
        int n = 12345;
        bool ok = cmd.integer_capped(&n, t.min, t.cap);
        CHECK(
            ok == t.ok && (!ok || n == t.n),
            "%s in [%d, cap %d]: ok=%d n=%d",
            t.v,
            t.min,
            t.cap,
            ok,
            n);
        CHECK(cmd.bad_value == !t.ok, "%s: bad_value=%d", t.v, cmd.bad_value);
        if(ok) CHECK((uint8_t)n == n || t.cap > 255, "%s narrows to uint8_t unchanged", t.v);
    }
    CHECK(
        parse("{\"cmd\":\"disp_br\"}", &cmd) && !cmd.integer_capped(nullptr, 0, 100) &&
            cmd.bad_value,
        "absent value refused");
}

static bool str(const char* quoted, std::string* out, size_t cap = 80) {
    char buf[128];
    size_t len = 0;
    if(!web_json_str(json(quoted), buf, cap, &len)) return false;
    *out = std::string(buf, len);
    return out->size() == strlen(buf);
}

static void test_string_decoding() {
    std::string s;
    CHECK(str("\"plain\"", &s) && s == "plain", "plain");
    CHECK(str("\"\"", &s) && s.empty(), "empty");
    CHECK(str("\"a\\\"b\\\\c\\/d\"", &s) && s == "a\"b\\c/d", "quote, backslash, slash");
    CHECK(str("\"\\b\\f\\n\\r\\t\"", &s) && s == "\b\f\n\r\t", "control escapes");
    CHECK(str("\"\\u0041\\u00e9\\u4e2d\"", &s) && s == "A\xC3\xA9\xE4\xB8\xAD", "\\u to UTF-8");
    CHECK(str("\"\\u00E9\"", &s) && s == "\xC3\xA9", "upper-case hex");
    CHECK(str("\"\\ud83d\\ude97\"", &s) && s == "\xF0\x9F\x9A\x97", "surrogate pair");
    CHECK(
        str("\"\xE4\xB8\xAD\xF0\x9F\x9A\x97\"", &s) && s == "\xE4\xB8\xAD\xF0\x9F\x9A\x97",
        "raw UTF-8");

    CHECK(!str("\"\\u0000\"", &s), "NUL escape");
    CHECK(!str("\"\\ud83d\"", &s), "lone high surrogate");
    CHECK(!str("\"\\ud83dx\"", &s), "high surrogate, then text");
    CHECK(!str("\"\\ud83d\\u0041\"", &s), "high surrogate, then no low one");
    CHECK(!str("\"\\ude97\"", &s), "lone low surrogate");
    CHECK(!str("\"\xC3\"", &s), "truncated UTF-8");
    CHECK(!str("\"\xC0\xAF\"", &s), "overlong UTF-8");
    CHECK(!str("\"\xED\xA0\x80\"", &s), "UTF-8 surrogate");
    CHECK(!str("\"\xF4\x90\x80\x80\"", &s), "above U+10FFFF");
    CHECK(!str("\"\x80\"", &s), "stray continuation byte");
    CHECK(!str("\"\xFF\"", &s), "0xFF");
    CHECK(!str("plain", &s), "unquoted");
    CHECK(!str("\"", &s), "lone quote");

    // Capacity counts bytes after decoding, plus the NUL.
    CHECK(str("\"\\u4e2d\"", &s, 4), "3-byte char in 4");
    CHECK(!str("\"\\u4e2d\"", &s, 3), "3-byte char in 3");
    CHECK(str("\"ab\"", &s, 3) && !str("\"abc\"", &s, 3), "ASCII at the edge");
    CHECK(!str("\"\\ud83d\\ude97\"", &s, 4), "4-byte char in 4");

    char tiny[1];
    CHECK(!web_json_str(json("\"\""), tiny, 0), "zero capacity");
    CHECK(web_json_str(json("\"\""), tiny, 1) && tiny[0] == '\0', "empty in 1");
}

static void test_get() {
    WebJson obj = json("{\"a\":1,\"bb\":{\"a\":2},\"c\":\"x\",\"a\":3}");
    WebJson v;
    CHECK(web_json_get(obj, "a", &v) && std::string(v.p, v.len) == "1", "first a");
    CHECK(web_json_get(obj, "bb", &v) && std::string(v.p, v.len) == "{\"a\":2}", "object member");
    CHECK(web_json_get(obj, "c", &v) && std::string(v.p, v.len) == "\"x\"", "string member");
    CHECK(!web_json_get(obj, "d", &v), "missing member");
    CHECK(!web_json_get(json("[1,2]"), "a", &v), "array is no object");
    CHECK(!web_json_get(json("\"a\""), "a", &v), "string is no object");
    CHECK(!web_json_get(WebJson{nullptr, 0}, "a", &v), "absent value");
    CHECK(!web_json_get(json("{}"), "a", &v), "empty object");
}

// ── wifi_cfg ──
static const char* wifi(const std::string& value, WebWifiCfg* cfg) {
    return web_wifi_parse(WebJson{value.data(), value.size()}, cfg);
}

static std::string wifi_value(
    const std::string& ssid,
    const std::string& pass,
    const std::string& sta_ssid = "\"\"",
    const std::string& sta_pass = "\"\"") {
    return "{\"ssid\":" + ssid + ",\"pass\":" + pass +
           ",\"hidden\":false,\"sta_ssid\":" + sta_ssid + ",\"sta_pass\":" + sta_pass + "}";
}

static void test_wifi_page_message() {
    WebWifiCfg cfg;
    const char* bad = wifi(
        "{\"ssid\":\"MyCar-AP\",\"pass\":\"hunter2hunter2\",\"hidden\":true,"
        "\"sta_ssid\":\"HomeNet\",\"sta_pass\":\"correct horse\"}",
        &cfg);
    CHECK(bad == nullptr, "page message rejected: %s", bad ? bad : "");
    CHECK(cfg.has_ssid && strcmp(cfg.ssid, "MyCar-AP") == 0, "ssid");
    CHECK(cfg.has_pass && strcmp(cfg.pass, "hunter2hunter2") == 0, "pass");
    CHECK(cfg.has_hidden && cfg.hidden, "hidden");
    CHECK(cfg.has_sta_ssid && strcmp(cfg.sta_ssid, "HomeNet") == 0, "sta_ssid");
    CHECK(cfg.has_sta_pass && strcmp(cfg.sta_pass, "correct horse") == 0, "sta_pass");

    // Passwords sent back as "***" are kept; empty ones clear.
    bad = wifi(
        "{\"ssid\":\"Tesla-FSD\",\"pass\":\"***\",\"hidden\":false,\"sta_ssid\":\"\",\"sta_pass\":\"\"}",
        &cfg);
    CHECK(bad == nullptr, "default page message rejected: %s", bad ? bad : "");
    CHECK(!cfg.has_pass, "*** keeps the AP password");
    CHECK(cfg.has_sta_ssid && cfg.sta_ssid[0] == '\0', "empty sta_ssid clears it");
    CHECK(cfg.has_sta_pass && cfg.sta_pass[0] == '\0', "empty sta_pass clears it");
    CHECK(
        wifi(wifi_value("\"a\"", "\"***\"", "\"Home\"", "\"***\""), &cfg) == nullptr &&
            !cfg.has_sta_pass,
        "*** keeps the network password");
    CHECK(
        wifi(wifi_value("\"a\"", "\"\""), &cfg) == nullptr && cfg.has_pass && cfg.pass[0] == '\0',
        "empty AP password = open AP");

    // Members are optional: an API client may send only what changes.
    CHECK(
        wifi("{\"sta_ssid\":\"Home\",\"sta_pass\":\"secret123\"}", &cfg) == nullptr &&
            !cfg.has_ssid && !cfg.has_pass && !cfg.has_hidden && cfg.has_sta_ssid &&
            cfg.has_sta_pass,
        "partial update");
    CHECK(wifi("{}", &cfg) == nullptr && !cfg.has_ssid && !cfg.has_sta_pass, "empty object");
    CHECK(wifi("{\"ssid\":\"a\",\"other\":[1]}", &cfg) == nullptr, "unknown member ignored");
}

static void test_wifi_quotes_and_backslashes() {
    WebWifiCfg cfg;
    // JSON.stringify of  pa"ss\word"  and  "quoted" net\  — the old parser
    // skipped any field with a backslash and restarted anyway.
    const char* bad = wifi(
        wifi_value(
            "\"\\\"quoted\\\" net\\\\\"",
            "\"pa\\\"ss\\\\word\\\"\"",
            "\"Home \\\"5G\\\"\"",
            "\"\\\\\\\\back\\\\slash\\\"quote\\\"\""),
        &cfg);
    CHECK(bad == nullptr, "quotes / backslashes rejected: %s", bad ? bad : "");
    CHECK(strcmp(cfg.ssid, "\"quoted\" net\\") == 0, "ssid decoded: %s", cfg.ssid);
    CHECK(strcmp(cfg.pass, "pa\"ss\\word\"") == 0, "pass decoded: %s", cfg.pass);
    CHECK(strcmp(cfg.sta_ssid, "Home \"5G\"") == 0, "sta_ssid decoded: %s", cfg.sta_ssid);
    CHECK(
        strcmp(cfg.sta_pass, "\\\\back\\slash\"quote\"") == 0,
        "sta_pass decoded: %s",
        cfg.sta_pass);

    // 64 quote characters: 128 bytes of JSON, 64 once decoded. They fit the
    // buffer (a network password with no network is not checked further), but
    // 64 characters that are not hex are no password (web_wifi_pass_ok()).
    std::string q64;
    for(int i = 0; i < 64; i++)
        q64 += "\\\"";
    CHECK(
        wifi(wifi_value("\"a\"", "\"\"", "\"\"", "\"" + q64 + "\""), &cfg) == nullptr &&
            strlen(cfg.sta_pass) == 64,
        "64 escaped quotes fit");
    CHECK(
        wifi(wifi_value("\"a\"", "\"\"", "\"\"", "\"" + q64 + "\\\"\""), &cfg) != nullptr,
        "65 do not");
    bad = wifi(wifi_value("\"a\"", "\"" + q64 + "\""), &cfg);
    CHECK(bad && strcmp(bad, "pass") == 0, "64 quotes are no AP password: %s", bad ? bad : "null");
    std::string q63 = q64.substr(2);
    CHECK(
        wifi(wifi_value("\"a\"", "\"" + q63 + "\""), &cfg) == nullptr && strlen(cfg.pass) == 63,
        "63 escaped quotes are");

    CHECK(
        wifi(
            wifi_value("\"\\u00e9t\\u00e9\"", "\"\\u5bc6\\u78bc\\u5bc6\\u78bc\\u5bc6\\u78bc\""),
            &cfg) == nullptr &&
            strcmp(cfg.ssid, "\xC3\xA9t\xC3\xA9") == 0 && strlen(cfg.pass) == 18,
        "escaped non-ASCII");
}

static void test_wifi_lengths() {
    WebWifiCfg cfg;
    std::string s32(32, 's'), s33(33, 's'), p7(7, 'p'), p8(8, 'p'), p63(63, 'p'), p64(64, 'p'),
        p65(65, 'p');
    std::string hex64 = "0123456789abcdefABCDEF0123456789abcdef0123456789ABCDEF0123456789";
    auto q = [](const std::string& s) { return "\"" + s + "\""; };

    CHECK(wifi(wifi_value(q(s32), q(p8)), &cfg) == nullptr, "32-byte SSID, 8-byte password");
    CHECK(wifi(wifi_value(q("x"), q(p63)), &cfg) == nullptr, "63-byte password");
    // 64 characters are the key in hex: the SoftAP does not start on any other.
    CHECK(hex64.size() == 64, "hex64 is 64 long");
    CHECK(
        wifi(wifi_value(q("x"), q(hex64)), &cfg) == nullptr &&
            strcmp(cfg.pass, hex64.c_str()) == 0,
        "64 hex digits");
    const char* bad = wifi(wifi_value(q("x"), q(p64)), &cfg);
    CHECK(bad && strcmp(bad, "pass") == 0, "64 characters, not hex: %s", bad ? bad : "null");
    std::string hex63g = hex64.substr(0, 63) + "g";
    bad = wifi(wifi_value(q("x"), q(hex63g)), &cfg);
    CHECK(bad && strcmp(bad, "pass") == 0, "63 hex digits and a g: %s", bad ? bad : "null");
    bad = wifi(wifi_value(q("x"), q(p8), q("Home"), q(p64)), &cfg);
    CHECK(
        bad && strcmp(bad, "sta_pass") == 0,
        "64-character network password, not hex: %s",
        bad ? bad : "null");
    CHECK(
        wifi(wifi_value(q("x"), q(p8), q("Home"), q(hex64)), &cfg) == nullptr,
        "64 hex digits for the network");
    CHECK(
        wifi(wifi_value(q("x"), q(p8), q(""), q(p64)), &cfg) == nullptr,
        "anything with no network (the page does not check it then)");
    CHECK(
        web_wifi_pass_ok("", 0) && web_wifi_pass_ok("12345678", 8) &&
            !web_wifi_pass_ok("1234567", 7),
        "empty, 8, 7");
    bad = wifi(wifi_value(q(s33), q(p8)), &cfg);
    CHECK(bad && strcmp(bad, "ssid") == 0, "33-byte SSID: %s", bad ? bad : "null");
    bad = wifi(wifi_value(q(""), q(p8)), &cfg);
    CHECK(bad && strcmp(bad, "ssid") == 0, "empty SSID: %s", bad ? bad : "null");
    bad = wifi(wifi_value(q("x"), q(p7)), &cfg);
    CHECK(bad && strcmp(bad, "pass") == 0, "7-byte password: %s", bad ? bad : "null");
    bad = wifi(wifi_value(q("x"), q(p65)), &cfg);
    CHECK(bad && strcmp(bad, "pass") == 0, "65-byte password: %s", bad ? bad : "null");
    bad = wifi(wifi_value(q("x"), q(p8), q(s33)), &cfg);
    CHECK(bad && strcmp(bad, "sta_ssid") == 0, "33-byte network name: %s", bad ? bad : "null");
    bad = wifi(wifi_value(q("x"), q(p8), q("Home"), q(p7)), &cfg);
    CHECK(bad && strcmp(bad, "sta_pass") == 0, "7-byte network password: %s", bad ? bad : "null");
    bad = wifi(wifi_value(q("x"), q(p8), q("Home"), q(p65)), &cfg);
    CHECK(bad && strcmp(bad, "sta_pass") == 0, "65-byte network password: %s", bad ? bad : "null");
    CHECK(
        wifi(wifi_value(q("x"), q(p8), q(""), q("abc")), &cfg) == nullptr,
        "short network password with no network (the page allows it)");
    bad = wifi("{\"sta_pass\":\"abc\"}", &cfg);
    CHECK(bad && strcmp(bad, "sta_pass") == 0, "short network password, network not sent");

    // Bytes, not characters: 中 is 3 bytes, so 10 fit in an SSID and 11 do not.
    std::string zh10, zh11;
    for(int i = 0; i < 10; i++)
        zh10 += "\xE4\xB8\xAD";
    zh11 = zh10 + "\xE4\xB8\xAD";
    CHECK(wifi(wifi_value(q(zh10 + "xx"), q(p8)), &cfg) == nullptr, "30+2 bytes");
    bad = wifi(wifi_value(q(zh11), q(p8)), &cfg);
    CHECK(bad && strcmp(bad, "ssid") == 0, "11 CJK characters (33 bytes): %s", bad ? bad : "null");
    // A password of 3-byte characters is long enough by bytes.
    CHECK(
        wifi(wifi_value(q("x"), q("\xE5\xAF\x86\xE7\xA2\xBC\xE5\xAF\x86")), &cfg) == nullptr,
        "3 CJK characters = 9 bytes");
}

static void test_wifi_bad_fields() {
    WebWifiCfg cfg;
    struct {
        std::string value;
        const char* field;
    } cases[] = {
        {"[]", "value"},
        {"\"Tesla-FSD\"", "value"},
        {"null", "value"},
        {"{\"ssid\":5}", "ssid"},
        {"{\"ssid\":null}", "ssid"},
        {"{\"ssid\":\"tab\\there\"}", "ssid"},
        {"{\"ssid\":\"new\\nline\"}", "ssid"},
        {"{\"ssid\":\"del\x7f\"}", "ssid"},
        {"{\"ssid\":\"ok\",\"pass\":true}", "pass"},
        {"{\"ssid\":\"ok\",\"pass\":\"12345678\\u0007\"}", "pass"},
        {"{\"ssid\":\"ok\",\"pass\":\"\\ud800abcdefgh\"}", "pass"},
        {"{\"hidden\":\"true\"}", "hidden"},
        {"{\"hidden\":1}", "hidden"},
        {"{\"hidden\":null}", "hidden"},
        {"{\"sta_ssid\":[]}", "sta_ssid"},
        {"{\"sta_ssid\":\"\\r\"}", "sta_ssid"},
        {"{\"sta_pass\":12345678}", "sta_pass"},
        {"{\"sta_pass\":\"12345678\\n\"}", "sta_pass"},
        // The first bad member is the one reported.
        {"{\"ssid\":\"\",\"pass\":\"x\",\"sta_pass\":\"y\"}", "ssid"},
    };
    for(auto& c : cases) {
        const char* bad = wifi(c.value, &cfg);
        CHECK(
            bad && strcmp(bad, c.field) == 0,
            "%s: got %s, want %s",
            c.value.c_str(),
            bad ? bad : "null",
            c.field);
    }
}

static void test_hostname() {
    std::string h31(31, 'h'), h32(32, 'h');
    const char* good[] = {
        "tesla-fsd", "Tesla-FSD", "a", "Z", "7", "car2", "my-car-2", "a--b", "123", h31.c_str()};
    for(const char* h : good)
        CHECK(web_hostname_valid(h), "valid hostname refused: %s", h);
    const char* bad[] = {
        "",
        "-car",
        "car-",
        "-",
        "my_car",
        "my.car",
        "my car",
        " car",
        "car\n",
        "caf\xC3\xA9",
        "tesla-fsd.local",
        h32.c_str()};
    for(const char* h : bad)
        CHECK(!web_hostname_valid(h), "invalid hostname accepted: %s", h);
    CHECK(!web_hostname_valid(nullptr), "null hostname");

    WebWifiCfg cfg;
    CHECK(wifi("{\"ssid\":\"a\"}", &cfg) == nullptr && !cfg.has_host, "no host member: unchanged");
    CHECK(
        wifi("{\"host\":\"\"}", &cfg) == nullptr && cfg.has_host && cfg.host[0] == '\0',
        "empty host: back to the default");
    CHECK(
        wifi("{\"host\":\"Tesla-FSD\"}", &cfg) == nullptr && strcmp(cfg.host, "Tesla-FSD") == 0,
        "case is kept");
    CHECK(
        wifi("{\"host\":\"" + h31 + "\"}", &cfg) == nullptr && strlen(cfg.host) == 31,
        "31 characters");
    struct {
        std::string value;
    } refused[] = {
        {"{\"host\":\"" + h32 + "\"}"},
        {"{\"host\":\"my_car\"}"},
        {"{\"host\":\"car.lan\"}"},
        {"{\"host\":\"-car\"}"},
        {"{\"host\":5}"},
        {"{\"host\":null}"},
        {"{\"host\":\"car\\n\"}"},
        {"{\"host\":\"\\u00e9\"}"},
    };
    for(auto& c : refused) {
        const char* bad_field = wifi(c.value, &cfg);
        CHECK(
            bad_field && strcmp(bad_field, "host") == 0,
            "%s: got %s",
            c.value.c_str(),
            bad_field ? bad_field : "null");
    }
    // The whole page message, hostname included.
    CHECK(
        wifi(
            "{\"ssid\":\"Tesla-FSD\",\"pass\":\"***\",\"hidden\":false,\"sta_ssid\":\"Home\","
            "\"sta_pass\":\"***\",\"host\":\"garage-car\"}",
            &cfg) == nullptr &&
            cfg.has_host && strcmp(cfg.host, "garage-car") == 0,
        "page message with host");
    const char* bad_field =
        wifi("{\"ssid\":\"Tesla-FSD\",\"pass\":\"***\",\"host\":\"bad name\"}", &cfg);
    CHECK(bad_field && strcmp(bad_field, "host") == 0, "a bad host rejects the whole message");
}

static void test_reasons() {
    CHECK(web_cmd_error(WEB_CMD_OK) == nullptr, "ok");
    CHECK(web_cmd_error(WEB_CMD_RESTART) == nullptr, "restart");
    CHECK(strcmp(web_cmd_error(WEB_CMD_TOO_LONG), "too_long") == 0, "too_long");
    CHECK(strcmp(web_cmd_error(WEB_CMD_BAD_JSON), "bad_json") == 0, "bad_json");
    CHECK(strcmp(web_cmd_error(WEB_CMD_UNKNOWN), "unknown_cmd") == 0, "unknown_cmd");
    CHECK(strcmp(web_cmd_error(WEB_CMD_BAD_VALUE), "bad_value") == 0, "bad_value");
}

int main() {
    printf("test_web_cmd: dashboard command parser host tests\n");
    test_page_messages();
    test_command_names_inside_data();
    test_layout_variations();
    test_malformed_messages();
    test_value_types();
    test_capped_integers();
    test_string_decoding();
    test_get();
    test_wifi_page_message();
    test_wifi_quotes_and_backslashes();
    test_wifi_lengths();
    test_wifi_bad_fields();
    test_hostname();
    test_reasons();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
