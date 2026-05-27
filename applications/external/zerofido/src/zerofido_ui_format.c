/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "zerofido_ui_format.h"

#include <stdio.h>
#include <string.h>

/*
 * This module is deliberately presentation-only. It never loads records or
 * mutates store state; callers pass either full records or index entries and get
 * bounded, null-terminated strings for Flipper UI widgets.
 */
void zf_ui_hex_encode_truncated(const uint8_t *data, size_t size, char *out, size_t out_size) {
    static const char *hex = "0123456789abcdef";
    size_t limit = (out_size > 0) ? (out_size - 1) / 2 : 0;

    if (out_size == 0) {
        return;
    }

    if (size > limit) {
        size = limit;
    }

    for (size_t i = 0; i < size; ++i) {
        out[i * 2] = hex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[data[i] & 0x0F];
    }
    out[size * 2] = '\0';
}

const char *zf_ui_fido2_credential_type_label(bool resident_key) {
    return resident_key ? "Discoverable passkey" : "Saved passkey";
}

void zf_ui_format_passkey_fallback_label(const ZfCredentialIndexEntry *entry, char *out,
                                         size_t out_size) {
    char credential_id[17];

    if (!entry) {
        snprintf(out, out_size, "Saved passkey");
        return;
    }

    zf_ui_hex_encode_truncated(entry->credential_id, entry->credential_id_len, credential_id,
                               sizeof(credential_id));
    snprintf(out, out_size, "Passkey %s", credential_id);
}

void zf_ui_format_passkey_index_title(const ZfCredentialIndexEntry *entry, char *out,
                                      size_t out_size) {
    if (!entry) {
        snprintf(out, out_size, "Saved passkey");
        return;
    }

#ifdef ZF_HOST_TEST
    if (entry->user_display_name[0]) {
        snprintf(out, out_size, "%s", entry->user_display_name);
    } else if (entry->user_name[0]) {
        snprintf(out, out_size, "%s", entry->user_name);
    } else if (entry->rp_id[0]) {
        snprintf(out, out_size, "%s", entry->rp_id);
    } else
#endif
    {
        zf_ui_format_passkey_fallback_label(entry, out, out_size);
    }
}

void zf_ui_format_passkey_index_subtitle(const ZfCredentialIndexEntry *entry, char *out,
                                         size_t out_size) {
    if (!entry) {
        snprintf(out, out_size, "Saved passkey");
        return;
    }

#ifdef ZF_HOST_TEST
    if (entry->rp_id[0]) {
        snprintf(out, out_size, "%s - %s", entry->rp_id,
                 zf_ui_fido2_credential_type_label(entry->resident_key));
    } else
#endif
    {
        snprintf(out, out_size, "%s", zf_ui_fido2_credential_type_label(entry->resident_key));
    }
}

void zf_ui_format_passkey_title(const ZfCredentialRecord *record, char *out, size_t out_size) {
    if (!record) {
        snprintf(out, out_size, "Saved passkey");
        return;
    }

    if (record->user_display_name[0]) {
        snprintf(out, out_size, "%s", record->user_display_name);
    } else if (record->user_name[0]) {
        snprintf(out, out_size, "%s", record->user_name);
    } else if (record->rp_id[0]) {
        snprintf(out, out_size, "%s", record->rp_id);
    } else {
        snprintf(out, out_size, "Saved passkey");
    }
}

void zf_ui_format_approval_header(char *out, size_t out_size, ZfUiProtocol protocol,
                                  const char *operation) {
    if (protocol == ZfUiProtocolFido2) {
        if (operation && strcmp(operation, "Register") == 0) {
            snprintf(out, out_size, "Create passkey");
        } else if (operation && strcmp(operation, "Authenticate") == 0) {
            snprintf(out, out_size, "Use passkey");
        } else if (operation && strcmp(operation, "Reset") == 0) {
            snprintf(out, out_size, "Reset ZeroFIDO");
        } else {
            snprintf(out, out_size, "%s", operation ? operation : "Approve request");
        }
        return;
    }

    snprintf(out, out_size, "U2F %s", operation ? operation : "Request");
}

void zf_ui_format_approval_body(char *out, size_t out_size, ZfUiProtocol protocol,
                                const char *target_id, const char *user_text) {
    snprintf(out, out_size, "%s: %.60s\n%.40s", protocol == ZfUiProtocolU2f ? "App" : "Website",
             (target_id && target_id[0]) ? target_id : "(unknown)",
             (user_text && user_text[0]) ? user_text : "User: (not provided)");
}

void zf_ui_format_assertion_selection_index_label(const ZfCredentialIndexEntry *entry, char *out,
                                                  size_t out_size) {
    char title[72];
    char credential_id[9];

    if (!entry) {
        snprintf(out, out_size, "Account");
        return;
    }

    zf_ui_format_passkey_index_title(entry, title, sizeof(title));
    zf_ui_hex_encode_truncated(entry->credential_id, entry->credential_id_len, credential_id,
                               sizeof(credential_id));
    snprintf(out, out_size, "%s | %s", title, credential_id);
}

void zf_ui_format_assertion_selection_record_label(const ZfCredentialRecord *record, char *out,
                                                   size_t out_size) {
    const char *account = NULL;
    const char *website = NULL;

    if (!record) {
        snprintf(out, out_size, "Account");
        return;
    }

    if (record->user_display_name[0]) {
        account = record->user_display_name;
    } else if (record->user_name[0]) {
        account = record->user_name;
    } else {
        account = "No account name";
    }
    website = record->rp_id[0] ? record->rp_id : "Unknown website";
    snprintf(out, out_size, "%.48s | %.48s", account, website);
}

void zf_ui_format_fido2_credential_label(const ZfCredentialRecord *record, char *out,
                                         size_t out_size) {
    char title[72];

    if (!record) {
        snprintf(out, out_size, "Saved passkey");
        return;
    }

    zf_ui_format_passkey_title(record, title, sizeof(title));
    if (record->rp_id[0] && strcmp(title, record->rp_id) != 0) {
        snprintf(out, out_size, "%s | %s", title, record->rp_id);
    } else {
        snprintf(out, out_size, "%s", title);
    }
}

void zf_ui_format_fido2_credential_index_label(const ZfCredentialIndexEntry *entry, char *out,
                                               size_t out_size) {
    char title[72];

    if (!entry) {
        snprintf(out, out_size, "Saved passkey");
        return;
    }

    zf_ui_format_passkey_index_title(entry, title, sizeof(title));
#ifdef ZF_HOST_TEST
    if (entry->rp_id[0] && strcmp(title, entry->rp_id) != 0) {
        snprintf(out, out_size, "%s | %s", title, entry->rp_id);
    } else
#endif
    {
        snprintf(out, out_size, "%s", title);
    }
}

void zf_ui_format_fido2_credential_detail(const ZfCredentialRecord *record, char *out,
                                          size_t out_size) {
    const char *user = NULL;
    const char *website = NULL;
    const char *type = NULL;

    if (!record) {
        snprintf(out, out_size, "No passkey selected");
        return;
    }

    website = record->rp_id[0] ? record->rp_id : "Unknown website";
    if (record->user_display_name[0]) {
        user = record->user_display_name;
    } else if (record->user_name[0]) {
        user = record->user_name;
    } else {
        user = "No account name";
    }
    type = record->resident_key ? "Discoverable (RK)" : "Saved passkey";

    snprintf(out, out_size, "Website: %.40s\nAccount: %.40s\nType: %s", website, user, type);
}
