/* SPDX-License-Identifier: GPL-3.0-only */
#include "profile.h"
#include "ffkv.h"
#include "cstr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy(char* dst, size_t size, const char* src) {
    if(!size) return;
    size_t n = strlen(src);
    if(n >= size) n = size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* ---------------- targets ---------------- */

bool user_valid(const char* user) {
    size_t n = strlen(user);
    if(n == 0 || n > PROFILE_USER_MAX) return false;
    for(size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)user[i];
        if(!(isalnum(c) || c == '_' || c == '-' || c == '.' || c == '$')) return false;
    }
    return user[0] != '-';
}

bool host_valid(const char* host) {
    size_t n = strlen(host);
    if(n == 0 || n > PROFILE_HOST_MAX) return false;
    bool has_colon = strchr(host, ':') != NULL;
    for(size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)host[i];
        if(has_colon) {
            /* IPv6 literal (optionally with %zone) */
            if(!(isxdigit(c) || c == ':' || c == '.' || c == '%' || isalnum(c))) return false;
        } else if(!(isalnum(c) || c == '-' || c == '.' || c == '_')) {
            return false;
        }
    }
    return host[0] != '-' && host[0] != '.';
}

TargetError target_parse(const char* text, SshTarget* out) {
    memset(out, 0, sizeof(*out));
    out->port = PROFILE_DEFAULT_PORT;
    while(*text == ' ')
        text++;
    size_t len = strlen(text);
    while(len && text[len - 1] == ' ')
        len--;
    if(len == 0) return TargetErrEmpty;
    if(len > PROFILE_USER_MAX + PROFILE_HOST_MAX + 10) return TargetErrTooLong;

    char tmp[PROFILE_USER_MAX + PROFILE_HOST_MAX + 12];
    memcpy(tmp, text, len);
    tmp[len] = '\0';

    char* at = strrchr(tmp, '@');
    if(!at) return TargetErrNoUser;
    *at = '\0';
    if(strlen(tmp) > PROFILE_USER_MAX || !user_valid(tmp)) return TargetErrBadUser;
    copy(out->user, sizeof(out->user), tmp);

    char* host = at + 1;
    char* port = NULL;
    if(host[0] == '[') {
        char* close = strchr(host, ']');
        if(!close) return TargetErrBadHost;
        *close = '\0';
        host++;
        if(close[1] == ':')
            port = close + 2;
        else if(close[1] != '\0')
            return TargetErrBadHost;
    } else {
        char* colon = strchr(host, ':');
        if(colon) {
            if(strchr(colon + 1, ':')) return TargetErrBadHost; /* bare IPv6 needs [] */
            *colon = '\0';
            port = colon + 1;
        }
    }
    if(strlen(host) > PROFILE_HOST_MAX || !host_valid(host)) return TargetErrBadHost;
    copy(out->host, sizeof(out->host), host);

    if(port) {
        if(!*port) return TargetErrBadPort;
        char* end = NULL;
        unsigned long p = strtoul(port, &end, 10);
        if(*end != '\0' || p == 0 || p > 65535 || !isdigit((unsigned char)port[0]))
            return TargetErrBadPort;
        out->port = (uint16_t)p;
    }
    return TargetOk;
}

const char* target_error_str(TargetError err) {
    switch(err) {
    case TargetOk:
        return "OK";
    case TargetErrEmpty:
        return "Enter a user and host";
    case TargetErrNoUser:
        return "Add a user name";
    case TargetErrBadUser:
        return "Invalid user name";
    case TargetErrBadHost:
        return "Invalid host";
    case TargetErrBadPort:
        return "Port must be 1-65535";
    case TargetErrTooLong:
        return "Too long";
    }
    return "Invalid";
}

void target_format(const SshTarget* t, char* buf, size_t size) {
    bool v6 = strchr(t->host, ':') != NULL;
    if(t->port == PROFILE_DEFAULT_PORT)
        snprintf(buf, size, v6 ? "%s@[%s]" : "%s@%s", t->user, t->host);
    else
        snprintf(buf, size, v6 ? "%s@[%s]:%u" : "%s@%s:%u", t->user, t->host, (unsigned)t->port);
}

bool target_equal(const SshTarget* a, const SshTarget* b) {
    return a->port == b->port && strcmp(a->user, b->user) == 0 && strcmp(a->host, b->host) == 0;
}

/* ---------------- profiles ---------------- */

void profile_init(HostProfile* p) {
    memset(p, 0, sizeof(*p));
    p->target.port = PROFILE_DEFAULT_PORT;
    p->auth = ProfileAuthPassword;
    p->remember = false;
    p->transport = ProtoTransportAuto;
    p->policy = ProtoPolicyPrompt;
}

bool alias_valid(const char* alias) {
    size_t n = strlen(alias);
    if(n == 0 || n > PROFILE_ALIAS_MAX) return false;
    if(!isalnum((unsigned char)alias[0])) return false;
    for(size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)alias[i];
        if(!(isalnum(c) || c == '.' || c == '_' || c == '-')) return false;
    }
    /* Reserved by FAT / confusing as filenames */
    if(alias[n - 1] == '.') return false;
    return true;
}

void alias_suggest(const char* host, char* out, size_t size) {
    char tmp[PROFILE_ALIAS_MAX + 1];
    size_t o = 0;
    bool numeric = true, digit = false;
    for(const char* c = host; *c; c++) {
        if(isdigit((unsigned char)*c))
            digit = true;
        else if(*c != '.')
            numeric = false;
    }
    numeric = numeric && digit;
    if(numeric && strchr(host, '.')) {
        /* IPv4: use the last two octets */
        const char* last = strrchr(host, '.');
        const char* prev = last;
        while(prev > host && prev[-1] != '.')
            prev--;
        snprintf(tmp, sizeof(tmp), "host-%.*s-%s", (int)(last - prev), prev, last + 1);
    } else {
        for(const char* c = host; *c && o < PROFILE_ALIAS_MAX; c++) {
            unsigned char ch = (unsigned char)*c;
            if(ch == '.' && o > 0) break; /* first DNS label */
            tmp[o++] = (isalnum(ch) || ch == '-' || ch == '_') ? (char)ch : '-';
        }
        tmp[o] = '\0';
    }
    if(!alias_valid(tmp)) snprintf(tmp, sizeof(tmp), "host");
    copy(out, size, tmp);
}

void target_id(const SshTarget* t, char* buf, size_t size) {
    if(strchr(t->host, ':'))
        snprintf(buf, size, "[%s]:%u", t->host, (unsigned)t->port);
    else
        snprintf(buf, size, "%s:%u", t->host, (unsigned)t->port);
}

void profile_id(const HostProfile* p, char* buf, size_t size) {
    if(p->alias[0])
        copy(buf, size, p->alias);
    else
        target_id(&p->target, buf, size);
}

static bool cmd_valid(const char* cmd) {
    size_t n = strlen(cmd);
    if(n == 0 || n > PROFILE_CMD_MAX) return false;
    return !cstr_has_any(cmd, "\r\n");
}

bool profile_add_cmd(HostProfile* p, const char* cmd) {
    if(p->cmd_count >= PROFILE_MAX_CMDS || !cmd_valid(cmd)) return false;
    copy(p->cmds[p->cmd_count++], PROFILE_CMD_MAX + 1, cmd);
    return true;
}

bool profile_remove_cmd(HostProfile* p, uint8_t index) {
    if(index >= p->cmd_count) return false;
    for(uint8_t i = index; i + 1 < p->cmd_count; i++)
        memcpy(p->cmds[i], p->cmds[i + 1], sizeof(p->cmds[i]));
    p->cmd_count--;
    memset(p->cmds[p->cmd_count], 0, sizeof(p->cmds[0]));
    return true;
}

bool profile_set_cmd(HostProfile* p, uint8_t index, const char* cmd) {
    if(index >= p->cmd_count || !cmd_valid(cmd)) return false;
    copy(p->cmds[index], PROFILE_CMD_MAX + 1, cmd);
    return true;
}

static const char* const TRANSPORTS[] = {"auto", "lan", "wifi"};
static const char* const AUTHS[] = {"password", "default", "key"};
static const char* const POLICIES[] = {"prompt", "strict", "off"};

const char* transport_str(uint8_t t) {
    return t < 3 ? TRANSPORTS[t] : "auto";
}
const char* auth_str(uint8_t a) {
    return a < 3 ? AUTHS[a] : "password";
}
const char* policy_str(uint8_t p) {
    return p < 3 ? POLICIES[p] : "prompt";
}

static bool parse_enum(const char* s, const char* const* names, uint8_t n, uint8_t* out) {
    if(!s) return false;
    for(uint8_t i = 0; i < n; i++)
        if(strcmp(s, names[i]) == 0) {
            *out = i;
            return true;
        }
    return false;
}
bool transport_parse(const char* s, uint8_t* out) {
    return parse_enum(s, TRANSPORTS, 3, out);
}
bool auth_parse(const char* s, uint8_t* out) {
    return parse_enum(s, AUTHS, 3, out);
}
bool policy_parse(const char* s, uint8_t* out) {
    return parse_enum(s, POLICIES, 3, out);
}

bool profile_to_text(const HostProfile* p, char* buf, size_t size) {
    FfkvWriter w;
    ffkv_writer_init(&w, buf, size);
    ffkv_write_header(&w, PROFILE_FILETYPE, FILE_VERSION);
    ffkv_write(&w, "Alias", p->alias);
    ffkv_write(&w, "User", p->target.user);
    ffkv_write(&w, "Host", p->target.host);
    ffkv_write_u32(&w, "Port", p->target.port);
    bool named = p->auth == ProfileAuthKey && p->key[0];
    ffkv_write(
        &w,
        "Auth",
        auth_str(
            named                     ? ProfileAuthKey :
            p->auth == ProfileAuthKey ? ProfileAuthDefault :
                                        p->auth));
    if(named) ffkv_write(&w, "Key", p->key);
    ffkv_write(&w, "Remember_password", p->remember ? "yes" : "no");
    ffkv_write(&w, "Transport", transport_str(p->transport));
    ffkv_write(&w, "Hostkey_policy", policy_str(p->policy));
    for(uint8_t i = 0; i < p->cmd_count; i++)
        ffkv_write(&w, "Cmd", p->cmds[i]);
    return !w.err;
}

bool key_name_valid(const char* s) {
    size_t n = strlen(s);
    if(n == 0 || n > KEY_NAME_MAX || s[0] == '.') return false;
    for(size_t i = 0; i < n; i++)
        if(!isalnum((unsigned char)s[i]) && s[i] != '.' && s[i] != '_' && s[i] != '-')
            return false;
    return !(n >= 4 && strcmp(s + n - 4, ".pub") == 0);
}

uint8_t profile_wire_auth(const HostProfile* p) {
    return p->auth == ProfileAuthPassword ? ProtoAuthPassword : ProtoAuthKey;
}

const char* profile_key_name(const HostProfile* p, const char* default_key) {
    if(p->auth == ProfileAuthKey) return p->key;
    if(p->auth == ProfileAuthDefault && default_key && default_key[0]) return default_key;
    return NULL;
}

bool profile_rename_key(HostProfile* p, const char* old_name, const char* new_name) {
    if(p->auth != ProfileAuthKey || strcmp(p->key, old_name) != 0 || !new_name) return false;
    copy(p->key, sizeof(p->key), new_name);
    return true;
}

bool profile_key_deleted(HostProfile* p, const char* name, const char* default_after) {
    bool named = p->auth == ProfileAuthKey && strcmp(p->key, name) == 0;
    bool was_default = p->auth == ProfileAuthDefault && !(default_after && default_after[0]);
    if(!named && !was_default) return false;
    p->auth = default_after && default_after[0] ? ProfileAuthDefault : ProfileAuthPassword;
    p->key[0] = '\0';
    return true;
}

static const char* parse_error = "";

const char* profile_error(void) {
    return parse_error;
}

bool profile_from_text(char* text, HostProfile* p) {
    profile_init(p);
    parse_error = "Not a host file";
    Ffkv kv;
    ffkv_parse(&kv, text);
    if(!ffkv_check_header(&kv, PROFILE_FILETYPE, FILE_VERSION)) return false;
    const char* alias = ffkv_get(&kv, "Alias");
    const char* user = ffkv_get(&kv, "User");
    const char* host = ffkv_get(&kv, "Host");
    parse_error = "Bad alias, user or host";
    if(!alias || !alias_valid(alias) || !user || !user_valid(user) || !host || !host_valid(host))
        return false;
    copy(p->alias, sizeof(p->alias), alias);
    copy(p->target.user, sizeof(p->target.user), user);
    copy(p->target.host, sizeof(p->target.host), host);
    const char* port = ffkv_get(&kv, "Port");
    if(port) {
        char* end = NULL;
        unsigned long v = strtoul(port, &end, 10);
        parse_error = "Bad port";
        if(*end != '\0' || v == 0 || v > 65535) return false;
        p->target.port = (uint16_t)v;
    }
    /* Unknown enum values fall back to safe defaults rather than rejecting the file. */
    (void)auth_parse(ffkv_get(&kv, "Auth"), &p->auth);
    if(p->auth == ProfileAuthKey) {
        /* "Auth: key" without a valid Key: (as v0 wrote it) = the default key */
        const char* key = ffkv_get(&kv, "Key");
        if(key && strlen(key) <= KEY_NAME_MAX && key_name_valid(key))
            copy(p->key, sizeof(p->key), key);
        else
            p->auth = ProfileAuthDefault;
    }
    (void)transport_parse(ffkv_get(&kv, "Transport"), &p->transport);
    (void)policy_parse(ffkv_get(&kv, "Hostkey_policy"), &p->policy);
    const char* rem = ffkv_get(&kv, "Remember_password");
    p->remember = rem && strcmp(rem, "yes") == 0;
    /* a command that cannot run as written makes the file unreadable rather than being
     * dropped, so a later save never loses it (an empty Cmd line holds nothing) */
    size_t n = ffkv_count(&kv, "Cmd");
    for(size_t i = 0; i < n; i++) {
        const char* c = ffkv_get_nth(&kv, "Cmd", i);
        if(!c[0]) continue;
        if(!profile_add_cmd(p, c)) {
            parse_error = p->cmd_count >= PROFILE_MAX_CMDS ? "More than 8 commands" :
                                                             "A command over 127 chars";
            return false;
        }
    }
    parse_error = "";
    return true;
}

/* ---------------- recent ---------------- */

void recent_init(RecentList* r) {
    memset(r, 0, sizeof(*r));
}

static bool recent_same(const RecentEntry* a, const RecentEntry* b) {
    if(a->alias[0] || b->alias[0]) return strcmp(a->alias, b->alias) == 0;
    return target_equal(&a->target, &b->target);
}

void recent_push(RecentList* r, const RecentEntry* e) {
    RecentEntry copy_e = *e;
    uint8_t found = r->count;
    for(uint8_t i = 0; i < r->count; i++)
        if(recent_same(&r->items[i], e)) {
            found = i;
            break;
        }
    uint8_t shift_end = found < r->count ? found :
                                           (r->count < RECENT_MAX ? r->count : RECENT_MAX - 1);
    for(uint8_t i = shift_end; i > 0; i--)
        r->items[i] = r->items[i - 1];
    r->items[0] = copy_e;
    if(found == r->count && r->count < RECENT_MAX) r->count++;
}

bool recent_remove(RecentList* r, uint8_t index) {
    if(index >= r->count) return false;
    for(uint8_t i = index; i + 1 < r->count; i++)
        r->items[i] = r->items[i + 1];
    r->count--;
    memset(&r->items[r->count], 0, sizeof(r->items[0]));
    return true;
}

void recent_forget_alias(RecentList* r, const char* alias) {
    for(uint8_t i = 0; i < r->count; i++)
        if(strcmp(r->items[i].alias, alias) == 0) r->items[i].alias[0] = '\0';
}

bool recent_to_text(const RecentList* r, char* buf, size_t size) {
    FfkvWriter w;
    ffkv_writer_init(&w, buf, size);
    ffkv_write_header(&w, RECENT_FILETYPE, FILE_VERSION);
    ffkv_write_comment(&w, "Conn: <transport> <user@host[:port]> [alias]");
    for(uint8_t i = 0; i < r->count; i++) {
        char t[PROFILE_USER_MAX + PROFILE_HOST_MAX + 12];
        char line[sizeof(t) + PROFILE_ALIAS_MAX + 12];
        target_format(&r->items[i].target, t, sizeof(t));
        snprintf(
            line,
            sizeof(line),
            "%s %s%s%s",
            transport_str(r->items[i].transport),
            t,
            r->items[i].alias[0] ? " " : "",
            r->items[i].alias);
        ffkv_write(&w, "Conn", line);
    }
    return !w.err;
}

bool recent_from_text(char* text, RecentList* r) {
    recent_init(r);
    Ffkv kv;
    ffkv_parse(&kv, text);
    if(!ffkv_check_header(&kv, RECENT_FILETYPE, FILE_VERSION)) return false;
    size_t n = ffkv_count(&kv, "Conn");
    for(size_t i = 0; i < n && r->count < RECENT_MAX; i++) {
        char line[PROFILE_USER_MAX + PROFILE_HOST_MAX + PROFILE_ALIAS_MAX + 24];
        copy(line, sizeof(line), ffkv_get_nth(&kv, "Conn", i));
        char* save = NULL;
        char* tr = cstr_next_word(line, &save);
        char* tg = cstr_next_word(NULL, &save);
        char* al = cstr_next_word(NULL, &save);
        RecentEntry e;
        memset(&e, 0, sizeof(e));
        if(!tr || !tg || !transport_parse(tr, &e.transport)) continue;
        if(target_parse(tg, &e.target) != TargetOk) continue;
        if(al) {
            if(!alias_valid(al)) continue;
            copy(e.alias, sizeof(e.alias), al);
        }
        r->items[r->count++] = e;
    }
    return true;
}

void recent_label(const RecentEntry* e, char* buf, size_t size) {
    if(e->alias[0])
        copy(buf, size, e->alias);
    else
        target_format(&e->target, buf, size);
}

/* ---------------- global commands ---------------- */

void commands_init(CommandList* c) {
    memset(c, 0, sizeof(*c));
}

bool commands_add(CommandList* c, const char* cmd) {
    if(c->count >= GLOBAL_MAX_CMDS || !cmd_valid(cmd)) return false;
    copy(c->cmds[c->count++], PROFILE_CMD_MAX + 1, cmd);
    return true;
}

bool commands_remove(CommandList* c, uint8_t index) {
    if(index >= c->count) return false;
    for(uint8_t i = index; i + 1 < c->count; i++)
        memcpy(c->cmds[i], c->cmds[i + 1], sizeof(c->cmds[i]));
    c->count--;
    memset(c->cmds[c->count], 0, sizeof(c->cmds[0]));
    return true;
}

bool commands_to_text(const CommandList* c, char* buf, size_t size) {
    FfkvWriter w;
    ffkv_writer_init(&w, buf, size);
    ffkv_write_header(&w, COMMANDS_FILETYPE, FILE_VERSION);
    ffkv_write_comment(&w, "Global saved commands, shown for every host");
    for(uint8_t i = 0; i < c->count; i++)
        ffkv_write(&w, "Cmd", c->cmds[i]);
    return !w.err;
}

bool commands_from_text(char* text, CommandList* c) {
    commands_init(c);
    Ffkv kv;
    ffkv_parse(&kv, text);
    if(!ffkv_check_header(&kv, COMMANDS_FILETYPE, FILE_VERSION)) return false;
    /* the app never writes this file back: what cannot run is counted, and the run menu
     * says so, instead of disappearing */
    size_t n = ffkv_count(&kv, "Cmd");
    for(size_t i = 0; i < n; i++) {
        const char* cmd = ffkv_get_nth(&kv, "Cmd", i);
        if(cmd[0] && !commands_add(c, cmd) && c->bad < 255) c->bad++;
    }
    return true;
}

/* the value of a "Cmd:" line starting at `line` (spaces after the colon skipped), else NULL;
 * *len is its length up to the line end, trailing spaces and CR off */
static const char* cmd_value(const char* line, size_t* len) {
    while(*line == ' ' || *line == '\t')
        line++;
    if(strncmp(line, "Cmd:", 4) != 0) return NULL;
    line += 4;
    while(*line == ' ' || *line == '\t')
        line++;
    size_t n = strcspn(line, "\n");
    while(n && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
        n--;
    *len = n;
    return line;
}

static bool text_has_cmd(const char* text, const char* cmd, unsigned* count) {
    bool found = false;
    size_t want = strlen(cmd);
    if(count) *count = 0;
    for(const char* l = text; l && *l; l = strchr(l, '\n'), l = l ? l + 1 : NULL) {
        size_t n;
        const char* v = cmd_value(l, &n);
        if(!v || !n) continue;
        if(count) (*count)++;
        if(n == want && !memcmp(v, cmd, n)) found = true;
    }
    return found;
}

unsigned commands_text_count(const char* text) {
    unsigned n = 0;
    (void)text_has_cmd(text, "", &n);
    return n;
}

void commands_missing(
    const char* cur,
    char* examples,
    char* out,
    size_t out_size,
    unsigned* added,
    unsigned* no_room) {
    *added = 0;
    *no_room = 0;
    if(out_size) out[0] = '\0';
    unsigned have = 0;
    (void)text_has_cmd(cur, "", &have);
    Ffkv kv;
    ffkv_parse(&kv, examples);
    if(!ffkv_check_header(&kv, COMMANDS_FILETYPE, FILE_VERSION)) return;
    size_t len = 0, n = ffkv_count(&kv, "Cmd");
    for(size_t i = 0; i < n; i++) {
        const char* c = ffkv_get_nth(&kv, "Cmd", i);
        if(!cmd_valid(c) || text_has_cmd(cur, c, NULL)) continue;
        int w = have < GLOBAL_MAX_CMDS ? snprintf(out + len, out_size - len, "Cmd: %s\n", c) : -1;
        if(w < 0 || (size_t)w >= out_size - len) {
            if(len < out_size) out[len] = '\0';
            (*no_room)++;
            continue;
        }
        len += (size_t)w;
        have++;
        (*added)++;
    }
}

/* ---------------- scripts ---------------- */

void script_interpreter(const char* first_line, char* out, size_t size) {
    static const char* const allowed[] = {"sh", "bash", "ash", "dash", "zsh", "ksh"};
    copy(out, size, "sh");
    if(!first_line || first_line[0] != '#' || first_line[1] != '!') return;
    const char* p = first_line + 2;
    while(*p == ' ')
        p++;
    /* first word, basename */
    char word[48];
    size_t n = 0;
    while(p[n] && p[n] != ' ' && p[n] != '\r' && p[n] != '\n' && n < sizeof(word) - 1)
        n++;
    memcpy(word, p, n);
    word[n] = '\0';
    const char* base = strrchr(word, '/');
    base = base ? base + 1 : word;
    char interp[16];
    if(strcmp(base, "env") == 0) {
        const char* q = p + n;
        while(*q == ' ')
            q++;
        size_t m = 0;
        while(q[m] && q[m] != ' ' && q[m] != '\r' && q[m] != '\n' && m < sizeof(interp) - 1)
            m++;
        memcpy(interp, q, m);
        interp[m] = '\0';
    } else {
        copy(interp, sizeof(interp), base);
    }
    for(size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++)
        if(strcmp(interp, allowed[i]) == 0) {
            copy(out, size, interp);
            return;
        }
}

bool script_name_valid(const char* name) {
    size_t n = strlen(name);
    if(n < 4 || n > 64) return false;
    if(cstr_has_any(name, "/\\:")) return false;
    if(name[0] == '.') return false;
    return strcmp(name + n - 3, ".sh") == 0;
}
