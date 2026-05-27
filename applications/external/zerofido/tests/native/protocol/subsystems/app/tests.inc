static void test_command_scratch_is_allocated_lifetime_and_single_owner(void) {
    ZerofidoApp app = {0};
    uint8_t *first = zf_app_command_scratch_acquire(&app, 128);

    expect(first != NULL, "command scratch should acquire arena");
    expect(app.command_scratch != NULL, "command scratch should allocate arena on demand");
    expect(first == app.command_scratch->bytes, "command scratch should return arena bytes");
    expect(app.command_scratch_in_use, "command scratch should be marked in use");
    memset(first, 0xA5, 128);
    expect(zf_app_command_scratch_acquire(&app, 64) == NULL,
           "command scratch should reject nested acquire");

    zf_app_command_scratch_release(&app);
    expect(app.command_scratch == NULL, "command scratch release should free arena");
    expect(!app.command_scratch_in_use, "command scratch release should clear in-use flag");
    expect(app.command_scratch_size == 0, "command scratch release should clear used size");

    uint8_t *second = zf_app_command_scratch_acquire(&app, ZF_COMMAND_SCRATCH_SIZE);
    expect(second != NULL, "command scratch should reacquire after release");
    zf_app_command_scratch_destroy(&app);
    expect(app.command_scratch == NULL, "command scratch destroy should free active arena");
    expect(!app.command_scratch_in_use, "command scratch destroy should leave arena idle");
}

static bool contains_text(const uint8_t *buffer, size_t size, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || needle_len > size) {
        return false;
    }

    for (size_t i = 0; i + needle_len <= size; ++i) {
        if (memcmp(buffer + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool cbor_map_contains_uint_key(const uint8_t *buffer, size_t size, uint64_t wanted_key) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint64_t key = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == wanted_key) {
            return true;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool cbor_map_uint_key_bool_value(const uint8_t *buffer, size_t size, uint64_t wanted_key,
                                         bool *value) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint64_t key = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == wanted_key) {
            return zf_cbor_read_bool(&cursor, value);
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static void test_enable_auto_accept_requests(ZerofidoApp *app) {
    zf_runtime_config_load_defaults(&app->runtime_config);
    app->runtime_config.auto_accept_requests = true;
    zf_runtime_config_resolve_capabilities(&app->runtime_config, &app->capabilities);
    app->capabilities_resolved = true;
}

static void test_enable_fido2_1_experimental(ZerofidoApp *app) {
    zf_runtime_config_load_defaults(&app->runtime_config);
    app->runtime_config.fido2_profile = ZfFido2ProfileCtap2_1Experimental;
    app->pin_state.pin_set = true;
    zf_runtime_config_resolve_capabilities(&app->runtime_config, &app->capabilities);
    app->capabilities_resolved = true;
}

static void test_enable_auto_accept_fido2_1_experimental(ZerofidoApp *app) {
    zf_runtime_config_load_defaults(&app->runtime_config);
    app->runtime_config.auto_accept_requests = true;
    app->runtime_config.fido2_profile = ZfFido2ProfileCtap2_1Experimental;
    app->pin_state.pin_set = true;
    zf_runtime_config_resolve_capabilities(&app->runtime_config, &app->capabilities);
    app->capabilities_resolved = true;
}

static bool parse_assertion_response_credential_id(const uint8_t *buffer, size_t size,
                                                   const uint8_t **credential_id,
                                                   size_t *credential_id_len) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint64_t key = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        size_t credential_pairs = 0;
        const uint8_t *field_value = NULL;
        size_t field_value_len = 0;
        const uint8_t *field_name = NULL;
        size_t field_name_len = 0;

        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key != 1) {
            if (!zf_cbor_skip(&cursor)) {
                return false;
            }
            continue;
        }
        if (!zf_cbor_read_map_start(&cursor, &credential_pairs)) {
            return false;
        }
        for (size_t j = 0; j < credential_pairs; ++j) {
            if (!zf_cbor_read_text_ptr(&cursor, &field_name, &field_name_len)) {
                return false;
            }
            if (field_name_len == 2 && memcmp(field_name, "id", 2) == 0) {
                if (!zf_cbor_read_bytes_ptr(&cursor, &field_value, &field_value_len)) {
                    return false;
                }
                *credential_id = field_value;
                *credential_id_len = field_value_len;
                return true;
            }
            if (!zf_cbor_skip(&cursor)) {
                return false;
            }
        }
        return false;
    }

    return false;
}

static bool parse_pin_token_response(const uint8_t *buffer, size_t size, const uint8_t **token,
                                     size_t *token_len) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint64_t key = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs) || pairs != 1) {
        return false;
    }
    if (!zf_cbor_read_uint(&cursor, &key) || key != 2) {
        return false;
    }
    if (!zf_cbor_read_bytes_ptr(&cursor, token, token_len)) {
        return false;
    }

    return cursor.ptr == cursor.end;
}

static bool get_info_has_extension(const uint8_t *buffer, size_t size, const char *expected) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    size_t expected_len = strlen(expected);

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 2) {
            size_t items = 0;
            if (!zf_cbor_read_array_start(&cursor, &items)) {
                return false;
            }
            for (size_t j = 0; j < items; ++j) {
                const uint8_t *extension = NULL;
                size_t extension_len = 0;
                if (!zf_cbor_read_text_ptr(&cursor, &extension, &extension_len)) {
                    return false;
                }
                if (extension_len == expected_len &&
                    memcmp(extension, expected, expected_len) == 0) {
                    return true;
                }
            }
            return false;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool get_info_has_cred_protect_extension(const uint8_t *buffer, size_t size) {
    return get_info_has_extension(buffer, size, "credProtect");
}

static bool get_info_has_version(const uint8_t *buffer, size_t size, const char *version) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    size_t version_len = strlen(version);

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 1) {
            size_t items = 0;
            if (!zf_cbor_read_array_start(&cursor, &items)) {
                return false;
            }
            for (size_t item = 0; item < items; ++item) {
                const uint8_t *value = NULL;
                size_t value_len = 0;
                if (!zf_cbor_read_text_ptr(&cursor, &value, &value_len)) {
                    return false;
                }
                if (value_len == version_len && memcmp(value, version, version_len) == 0) {
                    return true;
                }
            }
            return false;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool get_info_has_uint_field(const uint8_t *buffer, size_t size, uint64_t expected_key,
                                    uint64_t expected_value) {
    ZfCborCursor cursor;
    size_t pairs = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        uint64_t value = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == expected_key) {
            return zf_cbor_read_uint(&cursor, &value) && value == expected_value;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool get_info_has_pin_uv_auth_protocol(const uint8_t *buffer, size_t size,
                                              uint64_t expected_protocol) {
    ZfCborCursor cursor;
    size_t pairs = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 6) {
            size_t items = 0;
            if (!zf_cbor_read_array_start(&cursor, &items)) {
                return false;
            }
            for (size_t item = 0; item < items; ++item) {
                uint64_t protocol = 0;
                if (!zf_cbor_read_uint(&cursor, &protocol)) {
                    return false;
                }
                if (protocol == expected_protocol) {
                    return true;
                }
            }
            return false;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool get_info_pin_uv_auth_protocols_equal(const uint8_t *buffer, size_t size,
                                                 const uint64_t *expected, size_t expected_count) {
    ZfCborCursor cursor;
    size_t pairs = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 6) {
            size_t items = 0;
            if (!zf_cbor_read_array_start(&cursor, &items) || items != expected_count) {
                return false;
            }
            for (size_t item = 0; item < items; ++item) {
                uint64_t protocol = 0;
                if (!zf_cbor_read_uint(&cursor, &protocol) || protocol != expected[item]) {
                    return false;
                }
            }
            return cursor.ptr <= cursor.end;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool get_info_option_bool(const uint8_t *buffer, size_t size, const char *option,
                                 bool *value) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    size_t option_len = strlen(option);

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 4) {
            size_t option_pairs = 0;
            if (!zf_cbor_read_map_start(&cursor, &option_pairs)) {
                return false;
            }
            for (size_t option_index = 0; option_index < option_pairs; ++option_index) {
                const uint8_t *name = NULL;
                size_t name_len = 0;
                if (!zf_cbor_read_text_ptr(&cursor, &name, &name_len)) {
                    return false;
                }
                if (name_len == option_len && memcmp(name, option, option_len) == 0) {
                    return zf_cbor_read_bool(&cursor, value);
                }
                if (!zf_cbor_skip(&cursor)) {
                    return false;
                }
            }
            return false;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool parse_make_credential_cred_protect_output(const uint8_t *buffer, size_t size,
                                                      uint8_t *cred_protect) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    const uint8_t *auth_data = NULL;
    size_t auth_data_len = 0;
    size_t offset = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 2) {
            if (!zf_cbor_read_bytes_ptr(&cursor, &auth_data, &auth_data_len)) {
                return false;
            }
            break;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    if (!auth_data || auth_data_len < 55 || (auth_data[32] & 0xC0U) != 0xC0U) {
        return false;
    }

    offset = 55 + (((size_t)auth_data[53] << 8) | auth_data[54]);
    if (offset >= auth_data_len) {
        return false;
    }

    zf_cbor_cursor_init(&cursor, auth_data + offset, auth_data_len - offset);
    if (!zf_cbor_skip(&cursor)) {
        return false;
    }
    if (!zf_cbor_read_map_start(&cursor, &pairs) || pairs != 1) {
        return false;
    }

    const uint8_t *name = NULL;
    size_t name_len = 0;
    uint64_t raw = 0;
    if (!zf_cbor_read_text_ptr(&cursor, &name, &name_len) ||
        zf_ctap_classify_text_key(name, name_len) != ZfCtapTextKeyCredProtect ||
        !zf_cbor_read_uint(&cursor, &raw) || raw > UINT8_MAX || cursor.ptr != cursor.end) {
        return false;
    }

    *cred_protect = (uint8_t)raw;
    return true;
}

static bool parse_make_credential_hmac_secret_output(const uint8_t *buffer, size_t size,
                                                     bool *hmac_secret) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    const uint8_t *auth_data = NULL;
    size_t auth_data_len = 0;
    size_t offset = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 2) {
            if (!zf_cbor_read_bytes_ptr(&cursor, &auth_data, &auth_data_len)) {
                return false;
            }
            break;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    if (!auth_data || auth_data_len < 55 || (auth_data[32] & 0xC0U) != 0xC0U) {
        return false;
    }

    offset = 55 + (((size_t)auth_data[53] << 8) | auth_data[54]);
    if (offset >= auth_data_len) {
        return false;
    }

    zf_cbor_cursor_init(&cursor, auth_data + offset, auth_data_len - offset);
    if (!zf_cbor_skip(&cursor) || !zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        const uint8_t *name = NULL;
        size_t name_len = 0;
        if (!zf_cbor_read_text_ptr(&cursor, &name, &name_len)) {
            return false;
        }
        if (zf_ctap_classify_text_key(name, name_len) == ZfCtapTextKeyHmacSecret) {
            return zf_cbor_read_bool(&cursor, hmac_secret);
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool parse_hmac_secret_extension_output(const uint8_t *buffer, size_t size,
                                               const uint8_t **hmac_secret_enc,
                                               size_t *hmac_secret_enc_len) {
    ZfCborCursor cursor;
    size_t pairs = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }
    for (size_t i = 0; i < pairs; ++i) {
        const uint8_t *name = NULL;
        size_t name_len = 0;
        if (!zf_cbor_read_text_ptr(&cursor, &name, &name_len)) {
            return false;
        }
        if (zf_ctap_classify_text_key(name, name_len) == ZfCtapTextKeyHmacSecret) {
            return zf_cbor_read_bytes_ptr(&cursor, hmac_secret_enc, hmac_secret_enc_len) &&
                   cursor.ptr == cursor.end;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return false;
}

static bool parse_assertion_response_hmac_secret_output(const uint8_t *buffer, size_t size,
                                                        const uint8_t **hmac_secret_enc,
                                                        size_t *hmac_secret_enc_len) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    const uint8_t *auth_data = NULL;
    size_t auth_data_len = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 2) {
            if (!zf_cbor_read_bytes_ptr(&cursor, &auth_data, &auth_data_len)) {
                return false;
            }
            break;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    if (!auth_data || auth_data_len <= 37U || (auth_data[32] & 0x80U) == 0) {
        return false;
    }

    return parse_hmac_secret_extension_output(auth_data + 37U, auth_data_len - 37U, hmac_secret_enc,
                                              hmac_secret_enc_len);
}

static bool parse_make_credential_auth_flags(const uint8_t *buffer, size_t size, uint8_t *flags) {
    ZfCborCursor cursor;
    size_t pairs = 0;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        const uint8_t *auth_data = NULL;
        size_t auth_data_len = 0;

        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key != 2) {
            if (!zf_cbor_skip(&cursor)) {
                return false;
            }
            continue;
        }

        if (!zf_cbor_read_bytes_ptr(&cursor, &auth_data, &auth_data_len) || auth_data_len < 37U) {
            return false;
        }
        *flags = auth_data[32];
        return true;
    }

    return false;
}

#if ZF_PACKED_ATTESTATION
static bool parse_make_credential_packed_attestation(const uint8_t *buffer, size_t size) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    bool saw_fmt = false;
    bool saw_x5c = false;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;

        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 1) {
            const uint8_t *fmt = NULL;
            size_t fmt_len = 0;
            if (!zf_cbor_read_text_ptr(&cursor, &fmt, &fmt_len) || fmt_len != 6 ||
                memcmp(fmt, "packed", 6) != 0) {
                return false;
            }
            saw_fmt = true;
        } else if (key == 3) {
            size_t att_stmt_pairs = 0;
            if (!zf_cbor_read_map_start(&cursor, &att_stmt_pairs)) {
                return false;
            }
            for (size_t j = 0; j < att_stmt_pairs; ++j) {
                const uint8_t *att_key = NULL;
                size_t att_key_len = 0;
                if (!zf_cbor_read_text_ptr(&cursor, &att_key, &att_key_len)) {
                    return false;
                }
                if (att_key_len == 3 && memcmp(att_key, "x5c", 3) == 0) {
                    size_t cert_count = 0;
                    const uint8_t *cert = NULL;
                    size_t cert_len = 0;
                    if (!zf_cbor_read_array_start(&cursor, &cert_count) || cert_count != 1 ||
                        !zf_cbor_read_bytes_ptr(&cursor, &cert, &cert_len) || cert_len == 0) {
                        return false;
                    }
                    saw_x5c = true;
                } else if (!zf_cbor_skip(&cursor)) {
                    return false;
                }
            }
        } else if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return saw_fmt && saw_x5c && cursor.ptr == cursor.end;
}
#endif

static bool parse_make_credential_none_attestation(const uint8_t *buffer, size_t size) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    bool saw_fmt = false;
    bool saw_empty_att_stmt = false;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }

    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;

        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == 1) {
            const uint8_t *fmt = NULL;
            size_t fmt_len = 0;
            if (!zf_cbor_read_text_ptr(&cursor, &fmt, &fmt_len) || fmt_len != 4 ||
                memcmp(fmt, "none", 4) != 0) {
                return false;
            }
            saw_fmt = true;
        } else if (key == 3) {
            size_t att_stmt_pairs = 1;
            if (!zf_cbor_read_map_start(&cursor, &att_stmt_pairs) || att_stmt_pairs != 0) {
                return false;
            }
            saw_empty_att_stmt = true;
        } else if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }

    return saw_fmt && saw_empty_att_stmt && cursor.ptr == cursor.end;
}

static void mark_pin_token_issued(ZfClientPinState *state) {
    state->pin_token_active = true;
    state->pin_token_issued_at = g_fake_tick;
}
