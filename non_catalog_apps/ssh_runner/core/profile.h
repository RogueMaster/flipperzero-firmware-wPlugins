/*
 * Host profiles, Quick Connect targets, the recent-connections list and saved
 * commands. Pure C (no Furi) so it is unit-tested on the host.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <proto/proto_msg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROFILE_ALIAS_MAX    20
#define PROFILE_USER_MAX     PROTO_STR_USER
#define PROFILE_HOST_MAX     PROTO_STR_HOST
#define PROFILE_CMD_MAX      127
#define PROFILE_MAX_CMDS     8
#define GLOBAL_MAX_CMDS      16
#define RECENT_MAX           10
#define PROFILE_DEFAULT_PORT 22
#define KEY_NAME_MAX         32 /* a file name in keys/ */

#define PROFILE_FILETYPE  "SSH Runner Host"
#define RECENT_FILETYPE   "SSH Runner Recent"
#define COMMANDS_FILETYPE "SSH Runner Commands"
#define SETTINGS_FILETYPE "SSH Runner Settings"
#define FILE_VERSION      1

typedef enum {
    TargetOk = 0,
    TargetErrEmpty,
    TargetErrNoUser, /* missing "user@" */
    TargetErrBadUser,
    TargetErrBadHost,
    TargetErrBadPort,
    TargetErrTooLong,
} TargetError;

typedef struct {
    char user[PROFILE_USER_MAX + 1];
    char host[PROFILE_HOST_MAX + 1];
    uint16_t port;
} SshTarget;

/* How a saved host logs in (Termius-style). On the wire everything but password
 * is ProtoAuthKey with one staged key file. */
typedef enum {
    ProfileAuthPassword = 0,
    ProfileAuthDefault = 1, /* the key named DefaultKey in settings.conf */
    ProfileAuthKey = 2, /* the key in `key` */
} ProfileAuth;

typedef struct {
    char alias[PROFILE_ALIAS_MAX + 1];
    SshTarget target;
    uint8_t auth; /* ProfileAuth */
    char key[KEY_NAME_MAX + 1]; /* ProfileAuthKey only */
    bool remember; /* remember password in ESP32 NVS */
    uint8_t transport; /* ProtoTransport */
    uint8_t policy; /* ProtoPolicy */
    uint8_t cmd_count;
    char cmds[PROFILE_MAX_CMDS][PROFILE_CMD_MAX + 1];
} HostProfile;

typedef struct {
    SshTarget target;
    uint8_t transport;
    char alias[PROFILE_ALIAS_MAX + 1]; /* empty for unsaved Quick Connect targets */
} RecentEntry;

typedef struct {
    uint8_t count;
    RecentEntry items[RECENT_MAX];
} RecentList;

typedef struct {
    uint8_t count;
    uint8_t bad; /* lines of commands.conf that cannot run as written: over-long or past the 16th */
    char cmds[GLOBAL_MAX_CMDS][PROFILE_CMD_MAX + 1];
} CommandList;

/* ---- targets ---- */
TargetError target_parse(const char* text, SshTarget* out);
const char* target_error_str(TargetError err);
/* "user@host" when port is 22, else "user@host:port"; IPv6 hosts in brackets. */
void target_format(const SshTarget* t, char* buf, size_t size);
bool target_equal(const SshTarget* a, const SshTarget* b);
bool host_valid(const char* host);
bool user_valid(const char* user);

/* ---- profiles ---- */
void profile_init(HostProfile* p);
/* Restore Examples: the commands of `examples` (a commands.conf) that `cur` (commands.conf as
 * it is, "" for none) does not have, as "Cmd: ..." lines in `out`, in order, while `cur` has
 * room (GLOBAL_MAX_CMDS). `cur` is only read: appending `out` changes nothing else in it.
 * *added counts the lines written, *no_room the ones left out for lack of room. */
void commands_missing(
    const char* cur,
    char* examples,
    char* out,
    size_t out_size,
    unsigned* added,
    unsigned* no_room);
/* The "Cmd:" lines of a commands.conf text that hold a command. */
unsigned commands_text_count(const char* text);
/* Why the last profile_from_text() refused a file ("" after success). */
const char* profile_error(void);
bool alias_valid(const char* alias); /* [A-Za-z0-9._-], starts alnum, 1..PROFILE_ALIAS_MAX */
/* Suggest a valid alias from a host: "nas.lan" -> "nas", "192.168.1.10" -> "host-1-10". */
void alias_suggest(const char* host, char* out, size_t size);
/* Host-key / secret namespace sent to the ESP32: alias, or "host:port". */
void profile_id(const HostProfile* p, char* buf, size_t size);
void target_id(const SshTarget* t, char* buf, size_t size);
bool profile_add_cmd(HostProfile* p, const char* cmd);
bool profile_remove_cmd(HostProfile* p, uint8_t index);
/* Replace command `index` in place (its position in the run menu stays). */
bool profile_set_cmd(HostProfile* p, uint8_t index, const char* cmd);

/* Serialise to / parse from FlipperFormat text. Parsing modifies `text` in place.
 * Auth: password | default | key (+ Key: <name>). Older files keep working:
 * "Auth: key" without a valid Key: means the default key. */
bool profile_to_text(const HostProfile* p, char* buf, size_t size);
bool profile_from_text(char* text, HostProfile* p);

/* A keys/ file name: [A-Za-z0-9._-], 1..KEY_NAME_MAX, not a dotfile, not *.pub. */
bool key_name_valid(const char* name);
/* Wire auth for SSH_CONNECT. */
uint8_t profile_wire_auth(const HostProfile* p);
/* The key file this host uses, or NULL for password (or a default that is not set). */
const char* profile_key_name(const HostProfile* p, const char* default_key);
/* A key was renamed: hosts that name it follow. Returns true if `p` changed. */
bool profile_rename_key(HostProfile* p, const char* old_name, const char* new_name);
/* Key `name` was deleted in the app; `default_after` is the default key from now on ("" =
 * none). A host that named it, or used it as the default, switches to the default key, or
 * to password when there is none. Returns true if `p` changed. */
bool profile_key_deleted(HostProfile* p, const char* name, const char* default_after);

/* ---- enums <-> strings ---- */
const char* transport_str(uint8_t t); /* "auto" "lan" "wifi" */
const char* auth_str(uint8_t a); /* "password" "default" "key" */
const char* policy_str(uint8_t p); /* "prompt" "strict" "off" */
bool transport_parse(const char* s, uint8_t* out);
bool auth_parse(const char* s, uint8_t* out);
bool policy_parse(const char* s, uint8_t* out);

/* ---- recent ---- */
void recent_init(RecentList* r);
/* Insert at the front; an entry with the same alias (or same target, if unsaved)
 * moves to the front instead of duplicating. Oldest falls off past RECENT_MAX. */
void recent_push(RecentList* r, const RecentEntry* e);
bool recent_remove(RecentList* r, uint8_t index);
/* After an alias is deleted or renamed. */
void recent_forget_alias(RecentList* r, const char* alias);
bool recent_to_text(const RecentList* r, char* buf, size_t size);
bool recent_from_text(char* text, RecentList* r);
/* Submenu label: "nas" or "user@192.168.1.10". */
void recent_label(const RecentEntry* e, char* buf, size_t size);

/* ---- global commands ---- */
void commands_init(CommandList* c);
bool commands_add(CommandList* c, const char* cmd);
bool commands_remove(CommandList* c, uint8_t index);
bool commands_to_text(const CommandList* c, char* buf, size_t size);
bool commands_from_text(char* text, CommandList* c);

/* ---- script helpers ---- */
/* Interpreter from the first line: "#!/bin/bash" or "#!/usr/bin/env bash" -> "bash",
 * anything without a shebang -> "sh". Only a small allow-list is accepted. */
void script_interpreter(const char* first_line, char* out, size_t size);
bool script_name_valid(const char* name); /* ends in .sh, no path separators */
