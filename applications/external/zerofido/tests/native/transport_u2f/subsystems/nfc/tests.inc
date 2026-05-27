static void test_nfc_listener_profile_is_valid_iso14443_4a(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    const Iso14443_3aData *base = NULL;
    const Iso14443_4aAtsData *ats = NULL;
    test_reset();
    test_nfc_init_app(&app, &mutex);

    base = app.transport_nfc_state_storage.iso14443_4a_data->iso14443_3a_data;
    ats = &app.transport_nfc_state_storage.iso14443_4a_data->ats_data;

    expect(base->uid_len == 7, "nfc listener should expose a double-size UID");
    expect(memcmp(base->uid, (const uint8_t[]){0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6}, 7) == 0,
           "nfc listener UID should remain stable across the SoloKey-style NFC profile");
    expect(base->atqa[0] == 0x44 && base->atqa[1] == 0x00,
           "nfc listener should expose a double-size ISO-DEP ATQA");
    expect(base->sak == 0x20, "nfc listener should advertise ISO-DEP without MIFARE Classic");
    expect(ats->tl == 0x05, "nfc listener should expose the Nitrokey-style ATS length");
    expect(ats->t0 == 0x78, "nfc listener should advertise the iOS-compatible FSC ATS format byte");
    expect(ats->ta_1 == 0x91, "nfc listener should expose ATS TA1 from the Nitrokey profile");
    expect(ats->tb_1 == 0xE8, "nfc listener should expose ATS TB1 with long FWT for CTAP2");
    expect(ats->tc_1 == 0x00, "nfc listener should expose ATS TC1 from the Nitrokey profile");
    expect(simple_array_get_count(ats->t1_tk) == 0,
           "nfc listener should not include ATS historical bytes");

    test_nfc_deinit_app(&app);
}

static void test_nfc_deinit_app(ZerofidoApp *app) {
    iso14443_4a_free(app->transport_nfc_state_storage.iso14443_4a_data);
    bit_buffer_free(app->transport_nfc_state_storage.tx_buffer);
    nfc_free(app->transport_nfc_state_storage.nfc);
    zf_app_transport_arena_release(app);
}

static void test_nfc_select_applet(ZerofidoApp *app) {
    static const uint8_t select_apdu[] = {
        0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01,
    };

    expect(zf_transport_nfc_handle_apdu(app, &app->transport_nfc_state_storage, select_apdu,
                                        sizeof(select_apdu)),
           "FIDO applet SELECT should be accepted");
    expect(app->transport_nfc_state_storage.applet_selected,
           "SELECT should mark the FIDO applet as selected");
}

static void test_nfc_select_applet_accepts_p2_no_fci(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t select_apdu[] = {
        0x00, 0xA4, 0x04, 0x0C, 0x08, 0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, select_apdu,
                                        sizeof(select_apdu)),
           "FIDO applet SELECT should accept P2=0x0C without FCI");
    expect(app.transport_nfc_state_storage.applet_selected,
           "SELECT with P2=0x0C should mark the FIDO applet as selected");

    test_nfc_deinit_app(&app);
}

static void test_nfc_select_applet_returns_fido2_version_when_u2f_disabled(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    const uint8_t *payload = NULL;
    static const uint8_t select_apdu[] = {
        0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.runtime_config.transport_mode = ZfTransportModeNfc;
    app.runtime_config.u2f_enabled = false;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, select_apdu,
                                        sizeof(select_apdu)),
           "FIDO2-only applet SELECT should be accepted");
    expect(app.transport_nfc_state_storage.applet_selected,
           "FIDO2-only SELECT should mark the FIDO applet as selected");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 10U && memcmp(payload, "FIDO_2_0", 8) == 0 &&
               payload[8] == 0x90 && payload[9] == 0x00,
           "FIDO2-only SELECT should return FIDO_2_0 plus SW_SUCCESS");

    test_nfc_deinit_app(&app);
}

static void test_nfc_select_applet_accepts_le_and_legacy_nine_byte_aid(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t select_with_le[] = {
        0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01, 0x00,
    };
    static const uint8_t select_lc9[] = {
        0x00, 0xA4, 0x04, 0x00, 0x09, 0xA0, 0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01, 0x00,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, select_with_le,
                                        sizeof(select_with_le)),
           "FIDO applet SELECT should accept canonical Le=0");
    expect(app.transport_nfc_state_storage.applet_selected,
           "SELECT with Le=0 should mark the FIDO applet as selected");
    expect(test_log_contains("FIDO SELECT"), "SELECT with Le=0 should expose the FIDO breadcrumb");
    expect(g_last_status_text[0] == '\0', "SELECT diagnostic should not update UI status");

    app.transport_nfc_state_storage.applet_selected = false;
    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, select_lc9,
                                        sizeof(select_lc9)),
           "FIDO applet SELECT should accept legacy Lc=9 AID-plus-zero form");
    expect(app.transport_nfc_state_storage.applet_selected,
           "legacy Lc=9 SELECT should mark the FIDO applet as selected");

    test_nfc_deinit_app(&app);
}

static void test_nfc_event_callback_accepts_bare_select_apdu(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x06,
                                              0x47, 0x2F, 0x00, 0x01},
                            13U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "bare SELECT APDU should be accepted through the NFC listener callback");
    expect(app.transport_nfc_state_storage.applet_selected,
           "bare SELECT APDU should select the FIDO applet");
    expect(g_last_nfc_tx_len == 11U && g_last_nfc_tx[0] == 0x02 &&
               memcmp(&g_last_nfc_tx[1], "U2F_V2", 6) == 0 && g_last_nfc_tx[7] == 0x90 &&
               g_last_nfc_tx[8] == 0x00,
           "bare SELECT APDU should still return U2F_V2 plus SW_SUCCESS inside an I-block");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_listener_event_decodes_block_and_sends_framed_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "ISO14443-4A listener event should decode ISO-DEP blocks before APDU dispatch");
    expect(app.transport_nfc_state_storage.applet_selected,
           "decoded APDU SELECT should select the FIDO applet");
    expect(g_last_nfc_tx_len == 11U && g_last_nfc_tx[0] == 0x02 &&
               memcmp(&g_last_nfc_tx[1], "U2F_V2", 6) == 0 && g_last_nfc_tx[7] == 0x90 &&
               g_last_nfc_tx[8] == 0x00,
           "decoded APDU SELECT should send a framed ISO-DEP response through the backport");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_duplicate_complete_i_block_replays_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };
    static const uint8_t select_block[] = {0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0,
                                           0x00, 0x00, 0x06, 0x47, 0x2F, 0x00, 0x01};
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;
    ZfTransportSessionId selected_session_id = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, select_block, sizeof(select_block));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "initial complete I-block should dispatch the SELECT APDU");
    expect(app.transport_nfc_state_storage.applet_selected,
           "initial complete I-block should select the FIDO applet");
    selected_session_id = app.transport_nfc_state_storage.session_id;
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, select_block, sizeof(select_block));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "duplicate complete I-block should be handled as an ISO-DEP retransmission");
    expect(app.transport_nfc_state_storage.session_id == selected_session_id,
           "duplicate complete I-block should not re-dispatch the SELECT APDU");
    expect(g_nfc_tx_count == 2U,
           "duplicate complete I-block should only transmit the replayed response");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "duplicate complete I-block should replay the cached response byte-for-byte");
    expect(test_log_contains("NFC I-dup replay"),
           "duplicate complete I-block should leave a replay diagnostic breadcrumb");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_duplicate_final_chain_block_replays_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };
    static const uint8_t select_first[] = {0x12, 0x00, 0xA4, 0x04, 0x00, 0x08};
    static const uint8_t select_final[] = {0x03, 0xA0, 0x00, 0x00, 0x06,
                                           0x47, 0x2F, 0x00, 0x01};
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;
    ZfTransportSessionId selected_session_id = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, select_first, sizeof(select_first));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "first chained I-block should be acknowledged");
    expect(app.transport_nfc_state_storage.command_chain_active,
           "first chained I-block should leave an RX chain active");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, select_final, sizeof(select_final));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "terminal chained I-block should dispatch the assembled SELECT APDU");
    expect(app.transport_nfc_state_storage.applet_selected,
           "terminal chained I-block should select the FIDO applet");
    selected_session_id = app.transport_nfc_state_storage.session_id;
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, select_final, sizeof(select_final));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "duplicate terminal chained I-block should replay the assembled response");
    expect(app.transport_nfc_state_storage.session_id == selected_session_id,
           "duplicate terminal chained I-block should not re-dispatch the assembled APDU");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "duplicate terminal chained I-block should replay the cached response byte-for-byte");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_duplicate_i_block_preserves_tx_chain_pcb(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(80U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };
    uint8_t ctap2_block[48] = {0x02, 0x80, 0x10, 0x00, 0x00, 0x00,
                               0x00, 0x28, ZfCtapeCmdClientPin};
    const size_t response_len = ZF_NFC_MAX_TX_FRAME_INF_SIZE + 50U;
    uint8_t first_response[sizeof(g_last_nfc_tx)] = {0};
    size_t first_response_len = 0U;
    uint8_t first_pcb = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    g_ctap2_large_response_len = response_len;
    for (size_t i = 9U; i < sizeof(ctap2_block); ++i) {
        ctap2_block[i] = (uint8_t)i;
    }

    bit_buffer_append_bytes(buffer, ctap2_block, sizeof(ctap2_block));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "complete I-block should start a chained CTAP2 response");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_active,
           "large CTAP2 response should leave an ISO-DEP TX chain active");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) != 0U,
           "large CTAP2 response should send a chained first response block");
    first_pcb = g_last_nfc_tx[0];
    first_response_len = g_last_nfc_tx_len;
    memcpy(first_response, g_last_nfc_tx, first_response_len);

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, ctap2_block, sizeof(ctap2_block));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "duplicate complete I-block should replay the first chained response");
    expect(g_last_nfc_tx_len == first_response_len &&
               memcmp(g_last_nfc_tx, first_response, first_response_len) == 0,
           "duplicate complete I-block should replay the first chained response byte-for-byte");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA3U}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-ACK after duplicate replay should advance the queued TX chain");
    expect(g_last_nfc_tx[0] != first_pcb,
           "R-ACK after duplicate replay should use the next response block number");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_listener_raw60_returns_unsupported_via_send_block(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x60}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "decoded native DESFire GetVersion should receive an unsupported status");
    expect(g_last_nfc_tx_len == 11U && g_last_nfc_tx[0] == 0x02 && g_last_nfc_tx[1] == 0xAF &&
               g_last_nfc_tx[2] == 0x04 && g_last_nfc_tx[8] == 0x05,
           "decoded native DESFire GetVersion should send status-first native version data");
    expect(test_log_contains("DESFire native version"),
           "decoded native DESFire GetVersion should expose the classifier breadcrumb");
    expect(g_last_status_text[0] == '\0',
           "decoded native DESFire diagnostic should not update UI status");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_listener_native_desfire_payload_is_not_parsed_as_apdu(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02, 0x60}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "native DESFire payload inside an I-block should receive an unsupported status");
    expect(
        g_last_nfc_tx_len == 11U && g_last_nfc_tx[0] == 0x02 && g_last_nfc_tx[1] == 0xAF &&
            g_last_nfc_tx[2] == 0x04 && g_last_nfc_tx[8] == 0x05,
        "native DESFire payload inside an I-block should return status-first native version data");
    expect(test_log_contains("DESFire native version"),
           "native DESFire payload inside an I-block should not fall through to APDU 6700");
    expect(g_last_status_text[0] == '\0',
           "native DESFire payload diagnostic should not update UI status");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "native DESFire payload inside an I-block should leave the FIDO applet unselected");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_desfire_deselect_sleeps_before_next_activation(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(16U);
    Iso14443_4aListenerEventData iso4_data = {.buffer = buffer};
    Iso14443_4aListenerEvent iso4_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &iso4_data,
    };
    Iso14443_3aListenerEventData iso3_data = {.buffer = buffer};
    Iso14443_3aListenerEvent iso3_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &iso3_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &iso4_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02, 0x60}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "DESFire discovery should receive the native version shim");
    expect(g_last_nfc_tx_len == 11U && g_last_nfc_tx[0] == 0x02 && g_last_nfc_tx[1] == 0xAF,
           "DESFire discovery should transmit the working native response");
    expect(app.transport_nfc_state_storage.iso4_last_tx_valid &&
               app.transport_nfc_state_storage.iso4_last_tx_len > 0U,
           "DESFire discovery response should be cached before deselect");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xC2}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandSleep,
           "DESFire S-DESELECT should sleep the current activation after ACKing");
    expect(g_last_nfc_tx_len == 3U && g_last_nfc_tx[0] == 0xC2U,
           "DESFire S-DESELECT should still receive the ISO-DEP deselect response");
    expect(!app.transport_nfc_state_storage.iso4_active &&
               !app.transport_nfc_state_storage.applet_selected,
           "DESFire S-DESELECT should clear ISO-DEP state before the next activation");
    expect(!app.transport_nfc_state_storage.iso4_last_tx_valid &&
               app.transport_nfc_state_storage.iso4_last_tx_len == 0U,
           "DESFire S-DESELECT should clear stale replay state before the next activation");
    expect(!zf_transport_nfc_replay_last_iso_response(&app.transport_nfc_state_storage),
           "DESFire S-DESELECT should not leave the previous response replayable");
    expect(g_transport_connected_set_count >= 1U && !g_last_transport_connected,
           "DESFire S-DESELECT should publish a disconnected activation boundary");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    event.protocol = NfcProtocolIso14443_3a;
    event.instance = NULL;
    event.event_data = &iso3_event;
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "RATS after DESFire S-DESELECT should start a clean activation");
    expect(app.transport_nfc_state_storage.iso4_active,
           "RATS after DESFire S-DESELECT should reactivate ISO-DEP");
    expect(g_last_nfc_tx_len == 7U &&
               memcmp(g_last_nfc_tx, (const uint8_t[]){0x05, 0x78, 0x91, 0xE8, 0x00}, 5U) == 0,
           "RATS after DESFire S-DESELECT should transmit the iOS ATS");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_rats_sends_ats_with_public_tx(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "3A RATS should be handled in-app");
    expect(app.transport_nfc_state_storage.iso4_active,
           "RATS should activate the local ISO-DEP layer");
    expect(g_nfc_tx_count == 1U && g_last_nfc_tx_len == 7U,
           "RATS should transmit ATS plus ISO14443A CRC through public nfc_listener_tx");
    expect(memcmp(g_last_nfc_tx, (const uint8_t[]){0x05, 0x78, 0x91, 0xE8, 0x00}, 5U) == 0,
           "RATS should receive the configured short ATS");
    expect(test_log_contains("NFC RATS E0 len=1 80"),
           "RATS should expose the activation breadcrumb");
    expect(g_last_status_text[0] == '\0', "RATS diagnostic should not update UI status");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_repeated_rats_resends_ats_instead_of_iso4_block(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "first RATS should activate ISO-DEP");

    app.transport_nfc_state_storage.applet_selected = true;
    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "repeated RATS should restart activation instead of falling through as an ISO4 block");
    expect(g_nfc_tx_count == 2U && g_last_nfc_tx_len == 7U,
           "repeated RATS should send ATS a second time");
    expect(memcmp(g_last_nfc_tx, (const uint8_t[]){0x05, 0x78, 0x91, 0xE8, 0x00}, 5U) == 0,
           "repeated RATS should receive the configured short ATS");
    expect(test_log_contains("NFC RATS E0 len=1 80"),
           "repeated RATS should keep the activation breadcrumb instead of NFC block");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "repeated RATS should clear stale FIDO applet selection");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_reqa_data_resets_active_iso_dep_session(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.applet_selected = true;

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x26}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandReset,
           "REQA delivered as data should reset the active ISO-DEP session");
    expect(!app.transport_nfc_state_storage.iso4_active,
           "REQA data should clear ISO-DEP active state");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "REQA data should clear stale FIDO applet selection");
    expect(test_log_contains("NFC poll restart 26 len=0"),
           "REQA data should expose the poll restart breadcrumb");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_iso_dep_select_decodes_and_responds(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "3A RATS should activate ISO-DEP before APDUs");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "3A listener should decode ISO-DEP I-blocks after ATS");
    expect(app.transport_nfc_state_storage.applet_selected,
           "3A ISO-DEP SELECT should select the FIDO applet");
    expect(g_last_nfc_tx[0] == 0x02, "3A ISO-DEP response should be an I-block");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 8U && memcmp(payload, "U2F_V2", 6) == 0 &&
               payload[6] == 0x90 && payload[7] == 0x00,
           "3A ISO-DEP SELECT should return U2F_V2 plus SW_SUCCESS");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_r_nak_replays_last_iso_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "3A RATS should activate ISO-DEP before replay testing");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "SELECT response should be cached for R-NAK replay");
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xB3}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-NAK should be handled without resetting the NFC session");
    expect(g_nfc_tx_count == 3U, "R-NAK should transmit one replayed response");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "R-NAK should replay the cached ISO-DEP response byte-for-byte");
    expect(test_log_contains("NFC R-NAK replay B3 len=0"),
           "R-NAK replay should expose the reader PCB breadcrumb");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_control_frame_does_not_corrupt_cached_replay(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t response[] = {0x01, 0xA0};
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_send_apdu_payload(&app.transport_nfc_state_storage, response,
                                              sizeof(response), ZF_NFC_SW_SUCCESS),
           "test should prime a cached ISO-DEP response");
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    expect(zf_transport_nfc_send_r_ack(&app.transport_nfc_state_storage, 0x02),
           "test should send a non-replayable control frame after the cached response");
    expect(g_last_nfc_tx_len != cached_response_len ||
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) != 0,
           "control frame should replace the physical TX frame");

    expect(zf_transport_nfc_replay_last_iso_response(&app.transport_nfc_state_storage),
           "cached replay should survive an intervening control frame");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "cached replay should not resend the intervening control frame");

    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_r_ack_advances_chained_response_and_r_nak_replays(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t response[(ZF_NFC_TX_CHAIN_CHUNK_SIZE * 2U) + 88U] = {0};
    uint8_t cached_first[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_first_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)i;
    }

    expect(zf_transport_nfc_begin_chained_apdu_payload(&app.transport_nfc_state_storage, response,
                                                       sizeof(response), ZF_NFC_SW_SUCCESS),
           "oversized APDU payload should start ISO-DEP response chaining");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_active,
           "first chained block should leave remaining response bytes queued");
    expect(g_last_nfc_tx_len == ZF_NFC_TX_CHAIN_CHUNK_SIZE + 3U,
           "first chained response should fill one configured ISO-DEP chunk plus CRC");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) != 0U,
           "first chained response should set the ISO-DEP chaining bit");
    cached_first_len = g_last_nfc_tx_len;
    memcpy(cached_first, g_last_nfc_tx, cached_first_len);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xB2}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-NAK should be handled while a response chain is active");
    expect(g_last_nfc_tx_len == cached_first_len &&
               memcmp(g_last_nfc_tx, cached_first, cached_first_len) == 0,
           "R-NAK should replay the first chained I-block byte-for-byte");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA2}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-ACK should advance to the next queued response I-block");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_active,
           "middle chained response block should leave remaining response bytes queued");
    expect(g_last_nfc_tx_len == ZF_NFC_TX_CHAIN_CHUNK_SIZE + 3U,
           "middle chained response should fill one configured ISO-DEP chunk plus CRC");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) != 0U,
           "middle chained response should keep the ISO-DEP chaining bit set");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA3}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "second R-ACK should advance to the final queued response I-block");
    expect(!app.transport_nfc_state_storage.iso4_tx_chain_active,
           "final chained response block should clear the queued response");
    expect(g_last_nfc_tx_len == 93U,
           "final chained response should carry remaining bytes plus CRC");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) == 0U,
           "final chained response should clear the ISO-DEP chaining bit");
    expect(g_last_nfc_tx[g_last_nfc_tx_len - 4U] == 0x90 &&
               g_last_nfc_tx[g_last_nfc_tx_len - 3U] == 0x00,
           "final chained response should terminate with SW_SUCCESS before CRC");
    expect(!test_log_contains("NFC R-ACK chain"),
           "response-chain R-ACKs should avoid timing-sensitive per-frame logging");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_make_credential_sized_response_uses_single_i_block(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    uint8_t response[176U] = {0};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)(0x40U + i);
    }

    expect(zf_transport_nfc_send_apdu_payload(&app.transport_nfc_state_storage, response,
                                              sizeof(response), ZF_NFC_SW_SUCCESS),
           "iOS MakeCredential-sized response should fit one ISO-DEP I-block");
    expect(!app.transport_nfc_state_storage.iso4_tx_chain_active &&
               !app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "MakeCredential-sized response should not enter outbound chaining");
    expect(g_last_nfc_tx_len == sizeof(response) + 5U,
           "MakeCredential-sized single I-block should include PCB, SW, and CRC");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) == 0U,
           "MakeCredential-sized single I-block should not set the chaining bit");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == sizeof(response) + 2U &&
               payload[test_nfc_payload_len() - 2U] == 0x90 &&
               payload[test_nfc_payload_len() - 1U] == 0x00,
           "MakeCredential-sized single I-block should terminate with SW_SUCCESS");

    test_nfc_deinit_app(&app);
}

static void test_nfc_terminal_r_ack_after_chained_response_keeps_fido_selected(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    static const uint8_t ctap2_get_info_apdu[] = {0x80, 0x10, 0x00, 0x00, 0x01, ZfCtapeCmdGetInfo};
    uint8_t response[190U] = {0};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)i;
    }

    expect(zf_transport_nfc_begin_chained_apdu_payload(&app.transport_nfc_state_storage, response,
                                                       sizeof(response), ZF_NFC_SW_SUCCESS),
           "oversized APDU payload should start ISO-DEP response chaining");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_active,
           "first chained block should leave remaining response bytes queued");

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA2}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-ACK should advance to the final chained response block");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "final chained block should wait for the reader's terminal ACK");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA3}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "terminal R-ACK should acknowledge the completed chain");
    expect(!app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "terminal R-ACK should clear only the completed-chain latch");
    expect(app.transport_nfc_state_storage.applet_selected,
           "terminal R-ACK must not deselect the FIDO applet");
    expect(!app.transport_nfc_state_storage.post_success_cooldown_active,
           "terminal R-ACK should not arm post-success discovery cooldown");

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_get_info_apdu,
                                        sizeof(ctap2_get_info_apdu)),
           "a CTAP APDU after terminal R-ACK should still reach the selected FIDO applet");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() > 2U && payload[0] == ZF_CTAP_SUCCESS,
           "post-chain CTAP APDU should return a CTAP response instead of 6985");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_post_success_cooldown_allows_new_fido_select(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t response[(ZF_NFC_TX_CHAIN_CHUNK_SIZE * 2U) + 88U] = {0};

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    g_fake_tick = 1000U;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)i;
    }

    expect(zf_transport_nfc_begin_chained_apdu_payload(&app.transport_nfc_state_storage, response,
                                                       sizeof(response), ZF_NFC_SW_SUCCESS),
           "oversized APDU payload should start ISO-DEP response chaining");
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA2}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-ACK should send the middle chained response block");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_active,
           "middle chained block should keep remaining response bytes queued");
    expect(!app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "middle chained block should not wait for a terminal ACK yet");
    expect(!app.transport_nfc_state_storage.post_success_cooldown_active,
           "cooldown should not start while response bytes remain queued");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA3}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "second R-ACK should send the final chained response block");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "final chained block should wait for the reader's terminal ACK");
    expect(!app.transport_nfc_state_storage.post_success_cooldown_active,
           "cooldown should not start until the final block is acknowledged");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA2}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "terminal R-ACK should complete the transaction");
    expect(!app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "terminal R-ACK should clear the completed-chain latch");
    expect(!app.transport_nfc_state_storage.post_success_cooldown_active,
           "terminal R-ACK should not start the post-success cooldown by itself");
    expect(!app.transport_nfc_state_storage.post_success_probe_sleep_active,
           "terminal R-ACK should not arm post-success probe suppression by itself");
    expect(g_nfc_tx_count == 3U, "terminal R-ACK should not transmit another NFC frame");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "post-success cooldown should still allow a new ISO-DEP activation");
    expect(g_nfc_tx_count == 4U && g_last_nfc_tx_len == 7U,
           "cooldown RATS should transmit ATS so a real authentication can select FIDO");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "FIDO SELECT during cooldown should start a new session");
    expect(app.transport_nfc_state_storage.applet_selected,
           "FIDO SELECT during cooldown should select the applet");
    expect(!app.transport_nfc_state_storage.post_success_cooldown_active,
           "FIDO SELECT should clear post-success cooldown immediately");
    expect(!app.transport_nfc_state_storage.post_success_probe_sleep_active,
           "FIDO SELECT should clear post-success probe suppression");
    expect(g_nfc_tx_count == 5U && g_last_nfc_tx[0] == 0x02,
           "FIDO SELECT during cooldown should transmit the normal select response");

    app.transport_nfc_state_storage.post_success_cooldown_active = true;
    app.transport_nfc_state_storage.post_success_probe_sleep_active = true;
    app.transport_nfc_state_storage.post_success_cooldown_until_tick =
        g_fake_tick + ZF_NFC_POST_SUCCESS_COOLDOWN_MS;
    app.transport_nfc_state_storage.applet_selected = false;
    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "cooldown should still allow activation before suppressing non-FIDO probes");
    expect(g_nfc_tx_count == 6U && g_last_nfc_tx_len == 7U,
           "cooldown activation should send ATS before the NDEF probe is classified");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00,
                                              0x00, 0x85, 0x01, 0x01, 0x00},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "first NDEF SELECT during cooldown should be rejected so Safari can continue to FIDO");
    expect(g_nfc_tx_count == 7U && g_last_nfc_tx_len == 5U && g_last_nfc_tx[1] == 0x6A &&
               g_last_nfc_tx[2] == 0x82,
           "first cooldown NDEF SELECT should transmit the normal not-found response");

    app.transport_nfc_state_storage.applet_selected = false;
    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x03, 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00,
                                              0x00, 0x85, 0x01, 0x01, 0x00},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "repeated NDEF SELECT during cooldown should stay responsive for same-run auth");
    expect(g_nfc_tx_count == 8U && g_last_nfc_tx_len == 5U && g_last_nfc_tx[1] == 0x6A &&
               g_last_nfc_tx[2] == 0x82,
           "repeated cooldown NDEF SELECT should transmit the normal not-found response");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "FIDO SELECT after repeated cooldown NDEF should still start same-run auth");
    expect(app.transport_nfc_state_storage.applet_selected,
           "FIDO SELECT after repeated cooldown NDEF should select the applet");
    expect(g_nfc_tx_count == 9U && g_last_nfc_tx[0] == 0x02,
           "FIDO SELECT after repeated cooldown NDEF should transmit the select response");

    g_fake_tick = app.transport_nfc_state_storage.post_success_cooldown_until_tick + 1U;
    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "expired post-success cooldown should allow normal discovery again");
    expect(g_nfc_tx_count == 10U && g_last_nfc_tx_len == 7U,
           "normal discovery should resume with one ATS transmission after cooldown expiry");

    app.transport_nfc_state_storage.post_success_probe_sleep_active = true;
    app.transport_nfc_state_storage.applet_selected = false;
    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00,
                                              0x00, 0x85, 0x01, 0x01, 0x00},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "post-success NDEF SELECT after cooldown expiry should use normal rejection");
    expect(g_nfc_tx_count == 11U && g_last_nfc_tx_len == 5U && g_last_nfc_tx[1] == 0x6A &&
               g_last_nfc_tx[2] == 0x82,
           "expired post-success NDEF SELECT should transmit the normal not-found response");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_empty_i_block_replays_cached_large_response_before_helper(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t response[ZF_NFC_MAX_TX_FRAME_INF_SIZE - 2U] = {0};
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)(0xA0U + i);
    }

    expect(zf_transport_nfc_send_apdu_payload(&app.transport_nfc_state_storage, response,
                                              sizeof(response), ZF_NFC_SW_SUCCESS),
           "max safe single-frame APDU response should be sent and cached");
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "empty I-block should be intercepted before the ISO4 helper consumes it");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "empty I-block should replay the cached large response byte-for-byte");
    expect(test_log_contains("NFC I-empty replay 02 len=0"),
           "empty I-block replay should expose the recovery breadcrumb");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xB3}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-NAK after empty I-block recovery should still be handled");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "R-NAK after empty I-block should keep replaying the original response");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_empty_i_block_replays_encoded_frame_when_cache_flag_missing(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t response[ZF_NFC_MAX_TX_FRAME_INF_SIZE - 2U] = {0};
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)(0x20U + i);
    }

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "SELECT should prime the ISO-DEP helper response state");
    bit_buffer_reset(buffer);

    expect(zf_transport_nfc_send_apdu_payload(&app.transport_nfc_state_storage, response,
                                              sizeof(response), ZF_NFC_SW_SUCCESS),
           "max safe APDU response should leave an encoded ISO-DEP frame available");
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    app.transport_nfc_state_storage.iso4_last_tx_valid = false;
    app.transport_nfc_state_storage.iso4_last_tx_len = 0U;
    memset(app.transport_nfc_state_storage.iso4_tx_frame, 0,
           sizeof(app.transport_nfc_state_storage.iso4_tx_frame));

    const size_t tx_count_before_empty_i = g_nfc_tx_count;

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "empty I-block should not use a stale helper-owned frame cache");
    expect(g_nfc_tx_count == tx_count_before_empty_i + 1U && g_last_nfc_tx_len == 3U &&
               g_last_nfc_tx[0] == 0xA2U,
           "empty I-block without an array replay should receive only a bounded R-ACK");
    expect(g_last_nfc_tx_len != cached_response_len ||
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) != 0,
           "disabled array replay must not fall back to mutable BitBuffer contents");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_late_pps_then_r_nak_replays_cached_large_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t response[ZF_NFC_MAX_TX_FRAME_INF_SIZE - 2U] = {0};
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)(0x40U + i);
    }

    expect(zf_transport_nfc_send_apdu_payload(&app.transport_nfc_state_storage, response,
                                              sizeof(response), ZF_NFC_SW_SUCCESS),
           "max safe APDU response should be cached before late PPS recovery");
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xD9, 0x33, 0x63}, 3U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "late PPS-like frame should be handled after a large response");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "late PPS-like frame should not transmit or overwrite the cached large response");
    expect(test_log_contains("NFC PPS defer D9 len=2 33 63"),
           "late PPS-like frame should expose the deferred-control breadcrumb");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xB2}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "R-NAK after late PPS should be handled without deselecting");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "R-NAK after late PPS should replay the cached large response byte-for-byte");
    expect(test_log_contains("NFC R-NAK replay B2 len=0"),
           "R-NAK after late PPS should expose the replay breadcrumb");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_3a_late_pps_frame_is_acknowledged(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xE0, 0x80}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "3A RATS should activate ISO-DEP before PPS testing");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "SELECT should establish a live ISO-DEP exchange before late PPS");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xD9, 0x33, 0x63}, 3U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "late PPS-like frame should be acknowledged instead of being silently ignored");
    expect(g_nfc_tx_count == 3U && g_last_nfc_tx_len == 3U && g_last_nfc_tx[0] == 0xD9,
           "late PPS-like frame should echo PPSS through public NFC TX");
    expect(test_log_contains("NFC PPS ack D9 len=2 33 63"),
           "late PPS-like frame should expose the PPS ack breadcrumb");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_framed_select_response_uses_reader_block_number(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x03, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "framed SELECT APDU should be accepted through the NFC listener callback");
    expect(g_last_nfc_tx[0] == 0x03,
           "framed SELECT response should use the reader ISO-DEP block number");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 8 && memcmp(payload, "U2F_V2", 6) == 0 && payload[6] == 0x90 &&
               payload[7] == 0x00,
           "framed SELECT APDU should return U2F_V2 plus SW_SUCCESS inside an I-block");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_cache_last_iso_response_handles_frame_buffer_alias(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    const uint8_t frame[] = {0x03, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x32, 0x90, 0x00};
    uint8_t source[sizeof(frame)] = {0};

    test_reset();
    test_nfc_init_app(&app, &mutex);

    memcpy(source, frame, sizeof(source));
    zf_transport_nfc_cache_last_iso_response(&app.transport_nfc_state_storage, source,
                                             sizeof(source));
    memset(source, 0, sizeof(source));
    expect(app.transport_nfc_state_storage.iso4_last_tx_valid &&
               app.transport_nfc_state_storage.iso4_last_tx_len == sizeof(frame),
           "last ISO response cache should keep the aliased frame length");
    expect(memcmp(app.transport_nfc_state_storage.iso4_last_tx, frame, sizeof(frame)) == 0,
           "last ISO response cache should be an owned byte-array snapshot");
    expect(app.transport_nfc_state_storage.tx_frame_len == 0U,
           "manual cache snapshot must not depend on a prior TX frame");

    test_nfc_deinit_app(&app);
}

static void test_nfc_unadvertised_cid_frame_is_ignored(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x0A, 0x05, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00,
                                              0x00, 0x06, 0x47, 0x2F, 0x00, 0x01},
                            15U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "CID-bearing framed SELECT APDU should be skipped when ATS did not advertise CID");
    expect(g_nfc_tx_count == 0U, "unadvertised CID frame should not receive a response");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "unadvertised CID frame should not select the FIDO applet");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_ndef_type4_select_is_rejected_for_fido_only_surface(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00,
                                              0x00, 0x85, 0x01, 0x01},
                            13U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "Type 4 NDEF app SELECT should be rejected for the authenticator-only surface");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "NDEF app SELECT should not select the FIDO applet");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U && payload[0] == 0x6A && payload[1] == 0x82,
           "NDEF app SELECT should return SW_FILE_NOT_FOUND");
    expect(test_log_contains("NDEF reject"),
           "NDEF app SELECT should expose the authenticator-only breadcrumb");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_applet_select_is_required_before_ctap2(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t ctap2_apdu[] = {0x80, 0x10, 0x00, 0x00, 0x01, ZfCtapeCmdGetInfo};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_apdu,
                                        sizeof(ctap2_apdu)),
           "CTAP2 APDU should be rejected with a status word before SELECT");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2,
           "pre-SELECT CTAP2 rejection should return only a status word");
    expect(payload[0] == 0x69 && payload[1] == 0x85,
           "pre-SELECT CTAP2 rejection should return SW_CONDITIONS_NOT_SATISFIED");

    test_nfc_deinit_app(&app);
}

static void test_nfc_preselect_apdu_80_60_reports_instruction_not_supported(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t probe_apdu[] = {0x80, 0x60, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, probe_apdu,
                                        sizeof(probe_apdu)),
           "pre-SELECT APDU 80 60 should receive a status word");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2 && payload[0] == 0x6D && payload[1] == 0x00,
           "pre-SELECT APDU 80 60 should report INS_NOT_SUPPORTED instead of wrong state");
    expect(test_log_contains("APDU 8060 reject"),
           "pre-SELECT APDU 80 60 should expose the exact status word breadcrumb");
    expect(g_last_status_text[0] == '\0',
           "pre-SELECT APDU 80 60 diagnostic should not update UI status");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "pre-SELECT APDU 80 60 should not select the FIDO applet");

    test_nfc_deinit_app(&app);
}

static void test_nfc_preselect_wrapped_apdu_90_60_bootstraps_fido_applet(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t probe_apdu[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, probe_apdu,
                                        sizeof(probe_apdu)),
           "pre-SELECT wrapped APDU 90 60 should receive an unsupported status");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 30U && payload[0] == 0x04 && payload[6] == 0x05 &&
               payload[7] == 0x04 && payload[14] == 0x04 && payload[28] == 0x91 &&
               payload[29] == 0x00,
           "pre-SELECT wrapped APDU 90 60 should return terminal DESFire GetVersion data");
    expect(test_log_contains("DESFire version done"),
           "pre-SELECT wrapped APDU 90 60 should expose the classifier breadcrumb");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "pre-SELECT wrapped APDU 90 60 should leave the FIDO applet unselected");

    test_nfc_deinit_app(&app);
}

static void test_nfc_preselect_wrapped_apdu_90_af_reports_cla_not_supported(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t more_apdu[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, more_apdu,
                                        sizeof(more_apdu)),
           "pre-SELECT wrapped APDU 90 AF should receive a CLA-invalid status word");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 9U && payload[0] == 0x04 && payload[6] == 0x05 &&
               payload[7] == 0x91 && payload[8] == 0xAF,
           "pre-SELECT wrapped APDU 90 AF should return a DESFire continuation frame");
    expect(test_log_contains("DESFire version"),
           "pre-SELECT wrapped APDU 90 AF should expose the classifier breadcrumb");

    test_nfc_deinit_app(&app);
}

static void test_nfc_preselect_wrapped_desfire_get_version_does_not_sequence(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t probe_apdu[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    static const uint8_t more_apdu[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, probe_apdu,
                                        sizeof(probe_apdu)),
           "pre-SELECT wrapped DESFire GetVersion should return a terminal status");
    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, more_apdu,
                                        sizeof(more_apdu)),
           "pre-SELECT wrapped DESFire continuation should remain terminal");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 9U && payload[7] == 0x91 && payload[8] == 0xAF,
           "DESFire continuation should return another continuation frame");

    test_nfc_deinit_app(&app);
}

static void test_nfc_selected_wrapped_desfire_get_version_is_terminal_unsupported(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t probe_apdu[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, probe_apdu,
                                        sizeof(probe_apdu)),
           "selected wrapped DESFire GetVersion should receive a terminal unsupported status");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 30U && payload[28] == 0x91 && payload[29] == 0x00,
           "selected wrapped DESFire GetVersion should return terminal DESFire GetVersion data");
    expect(app.transport_nfc_state_storage.applet_selected,
           "selected wrapped DESFire GetVersion should not clear FIDO applet selection");
    test_nfc_deinit_app(&app);
}

static void test_nfc_event_callback_bare_90_60_bootstraps_fido_applet(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(16U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x90, 0x60, 0x00, 0x00}, 4U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "bare 90 60 probe should receive an ISO-DEP-framed status even without Le");
    expect(g_nfc_tx_count == 1U && g_last_nfc_tx_len == 33U && g_last_nfc_tx[0] == 0x02 &&
               g_last_nfc_tx[1] == 0x04 && g_last_nfc_tx[29] == 0x91 && g_last_nfc_tx[30] == 0x00,
           "bare 90 60 probe should transmit a DESFire version APDU response inside an I-block");
    expect(test_log_contains("DESFire version done"),
           "bare 90 60 probe should expose the classifier breadcrumb");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "bare 90 60 probe should leave the FIDO applet unselected");

    listener_event.type = Iso14443_4aListenerEventTypeHalted;
    listener_event.data = NULL;
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandSleep,
           "halt after the 90 60 response should sleep the NFC listener");
    expect(test_log_contains("DESFire version done"),
           "halt after the bare 90 60 probe should preserve the last APDU status");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "9060 probe reset should leave the selected applet state cleared");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_listener_bare_90_60_is_iso_dep_framed(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(16U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x90, 0x60, 0x00, 0x00, 0x00}, 5U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "bare 90 60 from the official ISO4 listener should be dispatched as an APDU");
    expect(g_nfc_tx_count == 1U && g_last_nfc_tx_len == 33U && g_last_nfc_tx[0] == 0x02 &&
               g_last_nfc_tx[1] == 0x04 && g_last_nfc_tx[29] == 0x91 && g_last_nfc_tx[30] == 0x00,
           "bare 90 60 from the official ISO4 listener should return a DESFire version APDU");
    expect(test_log_contains("DESFire version done"),
           "bare 90 60 from the official ISO4 listener should keep the APDU status visible");

    listener_event.type = Iso14443_4aListenerEventTypeHalted;
    listener_event.data = NULL;
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandSleep,
           "reader halt after bare 90 60 should reset the NFC session");
    expect(test_log_contains("DESFire version done"),
           "reader halt after bare 90 60 should not hide the rejection status");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap2_get_info_returns_immediately(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t ctap2_apdu[] = {0x80, 0x10, 0x00, 0x00, 0x01, ZfCtapeCmdGetInfo};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_apdu,
                                        sizeof(ctap2_apdu)),
           "CTAP2 GetInfo APDU should complete immediately after SELECT");
    expect(g_get_info_builder_called, "NFC GetInfo should use the shared GetInfo builder");
    expect(g_get_info_builder_saw_nfc_transport,
           "NFC GetInfo should force-advertise the NFC transport");
    expect(!g_get_info_builder_saw_usb_transport,
           "NFC GetInfo should not inherit USB transport advertising");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() > 2U && test_nfc_payload_len() < 250U,
           "GetInfo response should return the shared CTAP GetInfo payload plus SW");
    expect(payload[0] == ZF_CTAP_SUCCESS,
           "GetInfo response should preserve the CTAP success status");
    expect(payload[test_nfc_payload_len() - 2U] == 0x90 &&
               payload[test_nfc_payload_len() - 1U] == 0x00,
           "GetInfo response should terminate with SW_SUCCESS");
    expect(test_log_contains("CTAP2 getInfo"),
           "GetInfo should expose the synchronous NFC breadcrumb");
    expect(!test_log_contains("apdu-tx-data"),
           "GetInfo diagnostics should avoid APDU payload previews in the NFC callback");
    expect(g_last_status_text[0] == '\0', "GetInfo NFC diagnostic should not update UI status");
    expect(g_notify_prompt_count == 0U, "NFC CTAP2 GetInfo should not start the prompt light");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap2_get_info_accepts_webkit_extended_apdu(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t ctap2_apdu[] = {
        0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, ZfCtapeCmdGetInfo, 0x00, 0x00,
    };
    ZfNfcApdu parsed;
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_parse_apdu(ctap2_apdu, sizeof(ctap2_apdu), &parsed),
           "WebKit extended CTAP2 GetInfo APDU should parse");
    expect(parsed.extended && parsed.has_le && parsed.le == 0U,
           "WebKit extended CTAP2 GetInfo APDU should preserve extended Le=0000");
    expect(zf_transport_nfc_normalize_le(&parsed) == 65536U,
           "extended Le=0000 should normalize to the extended maximum response length");
    expect(parsed.data_len == 1U && parsed.data[0] == ZfCtapeCmdGetInfo,
           "WebKit extended CTAP2 GetInfo APDU should expose the CTAP command byte as data");

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_apdu,
                                        sizeof(ctap2_apdu)),
           "WebKit extended CTAP2 GetInfo APDU should complete immediately after SELECT");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() > 2U && test_nfc_payload_len() < 250U,
           "extended GetInfo response should return the shared CTAP GetInfo payload plus SW");
    expect(payload[0] == ZF_CTAP_SUCCESS,
           "extended GetInfo response should preserve the CTAP success status");
    expect(payload[test_nfc_payload_len() - 2U] == 0x90 &&
               payload[test_nfc_payload_len() - 1U] == 0x00,
           "extended GetInfo response should terminate with SW_SUCCESS");
    expect(test_log_contains("CTAP2 getInfo"),
           "extended GetInfo should expose the synchronous NFC breadcrumb");

    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_listener_extended_get_info_is_iso_dep_framed(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(16U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    memset(g_last_nfc_tx, 0, sizeof(g_last_nfc_tx));
    g_last_nfc_tx_bits = 0;
    g_last_nfc_tx_len = 0;
    g_nfc_tx_count = 0;

    bit_buffer_append_bytes(
        buffer,
        (const uint8_t[]){0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, ZfCtapeCmdGetInfo, 0x00, 0x00},
        10U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "WebKit extended CTAP2 GetInfo from the official ISO4 listener should dispatch as APDU");
    expect(g_nfc_tx_count >= 1U && g_last_nfc_tx_len >= 3U,
           "extended listener GetInfo should return an ISO-DEP-framed APDU response plus CRC");
    expect(g_last_nfc_tx[0] == 0x03 && g_last_nfc_tx[1] == ZF_CTAP_SUCCESS &&
               g_last_nfc_tx[g_last_nfc_tx_len - 4U] == 0x90 &&
               g_last_nfc_tx[g_last_nfc_tx_len - 3U] == 0x00,
           "extended listener GetInfo should return CTAP OK and SW_SUCCESS in an I-block");
    expect(test_log_contains("CTAP2 getInfo"),
           "extended listener GetInfo should expose the CTAP2 status");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_listener_ctap2_msg_signals_worker_after_unlock(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(16U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    memset(g_last_nfc_tx, 0, sizeof(g_last_nfc_tx));
    g_last_nfc_tx_bits = 0;
    g_last_nfc_tx_len = 0;
    g_nfc_tx_count = 0;

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x80, 0x10, ZF_NFC_CTAP_MSG_P1_GET_RESPONSE, 0x00,
                                              0x00, 0x00, 0x01, ZfCtapeCmdMakeCredential, 0x00,
                                              0x00},
                            10U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "ISO4 listener CTAP2 MSG should queue worker processing");
    expect((g_last_thread_flags_set & ZF_NFC_WORKER_EVT_REQUEST) != 0U,
           "locked ISO4 CTAP2 MSG should signal the persistent NFC worker");

    zf_transport_nfc_process_request(&app);
    expect(test_log_contains("CTAP2 worker done"),
           "persistent NFC worker should reach CTAP2 processing");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_iso4_listener_busy_ctap2_retry_has_minimal_side_effects(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListener listener = {0};
    BitBuffer *buffer = bit_buffer_alloc(16U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .instance = &listener,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    g_transport_connected_set_count = 0;
    memset(g_last_status_text, 0, sizeof(g_last_status_text));
    memset(g_last_nfc_tx, 0, sizeof(g_last_nfc_tx));
    g_last_nfc_tx_bits = 0;
    g_last_nfc_tx_len = 0;
    g_nfc_tx_count = 0;

    app.transport_nfc_state_storage.processing = true;
    app.transport_nfc_state_storage.request_pending = false;
    app.transport_nfc_state_storage.response_ready = false;

    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x80, 0x10, ZF_NFC_CTAP_MSG_P1_GET_RESPONSE, 0x00,
                                              0x00, 0x00, 0x01, ZfCtapeCmdMakeCredential, 0x00,
                                              0x00},
                            10U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "busy ISO4 CTAP2 retry should still return STATUS_UPDATE");
    expect(g_last_nfc_tx_len >= 5U && g_last_nfc_tx[1] == ZF_NFC_STATUS_PROCESSING &&
               g_last_nfc_tx[2] == 0x91 && g_last_nfc_tx[3] == 0x00,
           "busy ISO4 CTAP2 retry should emit processing status plus SW");
    expect(g_transport_connected_set_count <= 1U,
           "busy ISO4 CTAP2 retry should avoid per-retry transport UI churn");
    expect(g_last_status_text[0] == '\0',
           "busy ISO4 CTAP2 retry should not update the visible APDU status");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap2_msg_queues_worker_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t ctap2_apdu[] = {0x80, 0x10, ZF_NFC_CTAP_MSG_P1_GET_RESPONSE,
                                         0x00, 0x01, ZfCtapeCmdMakeCredential};
    static const uint8_t get_response_apdu[] = {0x80, ZF_NFC_INS_CTAP_GET_RESPONSE, 0x00, 0x00,
                                                0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_apdu,
                                        sizeof(ctap2_apdu)),
           "NFC CTAP2 MSG should queue worker processing and return a status update");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 3U,
           "queued NFC CTAP2 response should return processing status plus SW");
    expect(payload[0] == ZF_NFC_STATUS_PROCESSING && payload[1] == 0x91 && payload[2] == 0x00,
           "queued NFC CTAP2 response should use STATUS_UPDATE");
    expect(app.transport_nfc_state_storage.processing &&
               app.transport_nfc_state_storage.request_pending,
           "queued NFC CTAP2 should leave a worker request pending");
    expect((g_last_thread_flags_set & ZF_NFC_WORKER_EVT_REQUEST) != 0U,
           "queued NFC CTAP2 should signal the persistent NFC worker");

    zf_transport_nfc_process_request(&app);
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending,
           "worker-completed NFC CTAP2 should clear the background request");
    expect(g_notify_prompt_count == 0U,
           "queued NFC CTAP2 auto-accept should not start the user-presence prompt light");
    expect(g_ctap2_saw_transport_auto_accept,
           "worker NFC CTAP2 should auto-satisfy user presence for bring-up");
    expect(!app.transport_auto_accept_transaction,
           "worker NFC CTAP2 should restore the auto-accept override");
    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, get_response_apdu,
                                        sizeof(get_response_apdu)),
           "worker-completed NFC CTAP2 should expose the response through GET_RESPONSE");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 4U,
           "worker-completed NFC CTAP2 response should include CTAP body and SW");
    expect(payload[0] == ZF_CTAP_SUCCESS && payload[1] == 0xA0,
           "worker-completed NFC CTAP2 response should preserve the CTAP response body");
    expect(payload[2] == 0x90 && payload[3] == 0x00,
           "worker-completed NFC CTAP2 response should terminate with SW_SUCCESS");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap2_msg_without_get_response_returns_direct_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t ctap2_apdu[] = {0x80, 0x10, 0x00, 0x00, 0x01, ZfCtapeCmdMakeCredential};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_apdu,
                                        sizeof(ctap2_apdu)),
           "NFC CTAP2 MSG without GET_RESPONSE support should complete directly");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 4U, "direct no-GR NFC CTAP2 should include CTAP body and SW");
    expect(payload[0] == ZF_CTAP_SUCCESS && payload[1] == 0xA0 && payload[2] == 0x90 &&
               payload[3] == 0x00,
           "direct no-GR NFC CTAP2 should return CTAP OK and SW_SUCCESS");
    expect(g_ctap2_saw_transport_auto_accept,
           "direct no-GR NFC CTAP2 should auto-satisfy user presence");
    expect(!g_ctap2_request_response_overlap,
           "direct no-GR NFC CTAP2 should not pass the shared arena as both request and response");
    expect(g_notify_prompt_count == 0U,
           "direct no-GR NFC CTAP2 should not start the user-presence prompt light");
    expect(!app.transport_auto_accept_transaction,
           "direct no-GR NFC CTAP2 should restore the auto-accept override");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending &&
               !app.transport_nfc_state_storage.response_ready,
           "direct no-GR NFC CTAP2 should not leave a background request");
    test_nfc_deinit_app(&app);
}

static void test_nfc_large_ctap2_msg_without_get_response_uses_iso_response_chaining(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    uint8_t ctap2_apdu[45] = {0x80, 0x10, 0x00, 0x00, 0x28, ZfCtapeCmdClientPin};
    static const uint8_t get_response_apdu[] = {0x00, ZF_NFC_INS_ISO_GET_RESPONSE, 0x00, 0x00,
                                                0x00};
    const uint8_t *payload = NULL;
    const size_t first_chunk_len = ZF_NFC_GET_RESPONSE_CHUNK_SIZE;
    const size_t response_len = ZF_NFC_MAX_TX_FRAME_INF_SIZE + 50U;
    const size_t remaining_len = response_len - first_chunk_len;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    g_ctap2_large_response_len = response_len;

    for (size_t i = 6U; i < sizeof(ctap2_apdu); ++i) {
        ctap2_apdu[i] = (uint8_t)i;
    }

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_apdu,
                                        sizeof(ctap2_apdu)),
           "large NFC CTAP2 MSG without GET_RESPONSE support should start APDU response chaining");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == first_chunk_len + 2U,
           "large no-GR CTAP2 response should return one APDU response page plus SW");
    expect(payload[0] == ZF_CTAP_SUCCESS && payload[1] == 0xA1,
           "large no-GR CTAP2 response should start with the CTAP response body");
    expect(payload[first_chunk_len] == 0x61U &&
               payload[first_chunk_len + 1U] == (uint8_t)remaining_len,
           "large no-GR CTAP2 response should advertise remaining bytes with SW_BYTES_REMAINING");
    expect(g_ctap2_saw_transport_auto_accept,
           "large no-GR CTAP2 should auto-satisfy user presence");
    expect(!g_ctap2_request_response_overlap,
           "large no-GR CTAP2 should copy request bytes away from the response arena");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending &&
               app.transport_nfc_state_storage.response_ready &&
               app.transport_nfc_state_storage.response_offset == first_chunk_len &&
               !app.transport_nfc_state_storage.iso4_tx_chain_active,
           "large no-GR CTAP2 should stage the remainder for ISO GET RESPONSE");

    size_t remaining_after_page = remaining_len;
    while (remaining_after_page > ZF_NFC_GET_RESPONSE_CHUNK_SIZE) {
        expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage,
                                            get_response_apdu, sizeof(get_response_apdu)),
               "large no-GR CTAP2 response should continue through ISO GET RESPONSE");
        payload = test_nfc_payload();
        expect(test_nfc_payload_len() == ZF_NFC_GET_RESPONSE_CHUNK_SIZE + 2U,
               "intermediate no-GR CTAP2 response page should stay within the NFC page budget");
        expect(payload[ZF_NFC_GET_RESPONSE_CHUNK_SIZE] == 0x61U,
               "intermediate no-GR CTAP2 response page should advertise more bytes");
        remaining_after_page -= ZF_NFC_GET_RESPONSE_CHUNK_SIZE;
    }

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, get_response_apdu,
                                        sizeof(get_response_apdu)),
           "large no-GR CTAP2 response should finish through ISO GET RESPONSE");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == remaining_after_page + 2U,
           "final no-GR CTAP2 response page should include remaining bytes plus SW");
    expect(payload[remaining_after_page] == 0x90U && payload[remaining_after_page + 1U] == 0x00U,
           "final no-GR CTAP2 response page should terminate with SW_SUCCESS");
    expect(!app.transport_nfc_state_storage.response_ready,
           "final no-GR CTAP2 response page should clear the staged response");

    test_nfc_deinit_app(&app);
}

static void test_nfc_packed_attestation_sized_extended_response_finishes_iso_dep_chain(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    static uint8_t response[822U] = {0};
    const size_t total_len = sizeof(response) + 2U;
    const size_t full_chunks = total_len / ZF_NFC_TX_CHAIN_CHUNK_SIZE;
    const size_t final_payload_len = total_len - (full_chunks * ZF_NFC_TX_CHAIN_CHUNK_SIZE);
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;
    for (size_t i = 0; i < sizeof(response); ++i) {
        response[i] = (uint8_t)(0x30U + i);
    }

    expect(zf_transport_nfc_begin_chained_apdu_payload(&app.transport_nfc_state_storage, response,
                                                       sizeof(response), ZF_NFC_SW_SUCCESS),
           "packed attestation sized response should start ISO-DEP response chaining");
    expect(test_nfc_payload_len() == ZF_NFC_TX_CHAIN_CHUNK_SIZE,
           "packed attestation chain should send a bounded first ISO-DEP payload");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) != 0U,
           "packed attestation first block should set the chaining bit");

    for (size_t ack_index = 0U; ack_index < full_chunks; ++ack_index) {
        const uint8_t ack = (ack_index % 2U) == 0U ? 0xA2U : 0xA3U;

        bit_buffer_reset(buffer);
        bit_buffer_append_bytes(buffer, &ack, 1U);
        expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
               "packed attestation R-ACK should advance the ISO-DEP response chain");
    }

    payload = test_nfc_payload();
    expect(!app.transport_nfc_state_storage.iso4_tx_chain_active &&
               app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "packed attestation final block should wait for the terminal reader ACK");
    expect(test_nfc_payload_len() == final_payload_len,
           "packed attestation final block should carry only remaining bytes and SW");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) == 0U,
           "packed attestation final block should clear the chaining bit");
    expect(payload[final_payload_len - 2U] == 0x90U && payload[final_payload_len - 1U] == 0x00U,
           "packed attestation final block should terminate with SW_SUCCESS");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA2U}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "packed attestation terminal R-ACK should complete the ISO-DEP transaction");
    expect(!app.transport_nfc_state_storage.iso4_tx_chain_completed,
           "packed attestation terminal R-ACK should clear the completed-chain latch");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_large_extended_ctap2_msg_without_get_response_uses_extended_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t ctap2_apdu[47] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x28, ZfCtapeCmdClientPin};
    const uint8_t *payload = NULL;
    const size_t response_len = ZF_NFC_MAX_TX_FRAME_INF_SIZE + 50U;
    uint8_t first_pcb = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    g_ctap2_large_response_len = response_len;

    for (size_t i = 8U; i < sizeof(ctap2_apdu); ++i) {
        ctap2_apdu[i] = (uint8_t)i;
    }

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, ctap2_apdu,
                                        sizeof(ctap2_apdu)),
           "large extended NFC CTAP2 MSG should start an extended APDU response");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == ZF_NFC_TX_CHAIN_CHUNK_SIZE,
           "large extended CTAP2 response should use bounded ISO-DEP response chunks");
    expect(payload[0] == ZF_CTAP_SUCCESS && payload[1] == 0xA1,
           "large extended CTAP2 response should start with the CTAP response body");
    expect((g_last_nfc_tx[0] & ZF_NFC_PCB_CHAIN) != 0U,
           "large extended CTAP2 response should continue via ISO-DEP chaining");
    expect(app.transport_nfc_state_storage.iso4_tx_chain_active &&
               !app.transport_nfc_state_storage.response_ready,
           "large extended CTAP2 response must not use short APDU GET RESPONSE chaining");
    expect(test_log_contains("CTAP2 direct ext"),
           "large extended CTAP2 response should expose the extended-response breadcrumb");
    first_pcb = g_last_nfc_tx[0];

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0xA3U}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "extended CTAP2 R-ACK should advance the ISO-DEP response chain");
    expect(g_last_nfc_tx[0] != first_pcb,
           "extended CTAP2 R-ACK should transmit the next chained I-block");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_chained_ctap2_request_acknowledges_next_block_number(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(160U);
    Iso14443_3aListenerEventData event_data = {.buffer = buffer};
    Iso14443_3aListenerEvent listener_event = {
        .type = Iso14443_3aListenerEventTypeReceivedStandardFrame,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_3a,
        .event_data = &listener_event,
    };
    uint8_t chained_frame[126U] = {0x12U, 0x80U, 0x10U, 0x00U, 0x00U, 0x00U, 0x00U, 0xA5U};
    const char *chain_log = NULL;
    const char *ack_log = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    app.transport_nfc_state_storage.iso4_active = true;
    for (size_t i = 8U; i < sizeof(chained_frame); ++i) {
        chained_frame[i] = (uint8_t)i;
    }

    bit_buffer_append_bytes(buffer, chained_frame, sizeof(chained_frame));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "first chained NFC CTAP2 APDU block should be accepted");
    expect(g_nfc_tx_count > 0U && g_last_nfc_tx_len == 3U && g_last_nfc_tx[0] == 0xA2U,
           "I-chain block number 0 should be acknowledged with the working iOS R-ACK A2");
    expect(app.transport_nfc_state_storage.command_chain_active,
           "first chained NFC CTAP2 APDU block should leave command assembly active");
    expect(test_log_contains("NFC I-chain 12"),
           "chained NFC CTAP2 APDU should expose the I-chain breadcrumb");
    chain_log = strstr(g_log_text, "NFC I-chain 12");
    ack_log = strstr(g_log_text, "iso-tx len=1 head=A2");
    expect(chain_log && ack_log && chain_log < ack_log,
           "iOS receive-chain ACK should preserve the previous working command order");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_duplicate_chained_i_block_does_not_corrupt_assembled_apdu(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    uint8_t apdu[174U] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0xA5, ZfCtapeCmdMakeCredential};
    const size_t first_len = 125U;
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;

    for (size_t i = 8U; i < 172U; ++i) {
        apdu[i] = (uint8_t)i;
    }

    furi_mutex_acquire(&mutex, FuriWaitForever);
    expect(zf_transport_nfc_handle_iso4_payload_locked(&app, &app.transport_nfc_state_storage, apdu,
                                                       first_len, 0x12, true,
                                                       false) == NfcCommandContinue,
           "first chained I-block should be accepted");
    expect(mutex.depth == 0U, "first chained I-block should release the callback mutex");
    expect(app.transport_nfc_state_storage.command_chain_active &&
               app.transport_nfc_state_storage.request_len == first_len,
           "first chained I-block should seed APDU assembly");

    furi_mutex_acquire(&mutex, FuriWaitForever);
    expect(zf_transport_nfc_handle_iso4_payload_locked(&app, &app.transport_nfc_state_storage, apdu,
                                                       first_len, 0x12, true,
                                                       false) == NfcCommandContinue,
           "retransmitted chained I-block should be acknowledged");
    expect(mutex.depth == 0U, "duplicate chained I-block should release the callback mutex");
    expect(app.transport_nfc_state_storage.command_chain_active &&
               app.transport_nfc_state_storage.request_len == first_len,
           "duplicate chained I-block must not be appended twice");
    expect(test_log_contains("NFC I-chain dup"),
           "duplicate chained I-block should expose a replay breadcrumb");

    furi_mutex_acquire(&mutex, FuriWaitForever);
    expect(zf_transport_nfc_handle_iso4_payload_locked(&app, &app.transport_nfc_state_storage,
                                                       &apdu[first_len], sizeof(apdu) - first_len,
                                                       0x02, false, false) == NfcCommandContinue,
           "terminal I-block should complete APDU assembly after a duplicate");
    expect(mutex.depth == 0U, "terminal I-block should release the callback mutex");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 4U && payload[0] == ZF_CTAP_SUCCESS && payload[1] == 0xA0 &&
               payload[2] == 0x90 && payload[3] == 0x00,
           "deduplicated no-GR CTAP2 APDU should complete directly");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending &&
               !app.transport_nfc_state_storage.response_ready,
           "deduplicated no-GR APDU should not leave background worker state");

    test_nfc_deinit_app(&app);
}

static void test_nfc_duplicate_chained_i_block_stall_resets_exchange(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    uint8_t apdu[126U] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0xA5, ZfCtapeCmdMakeCredential};
    NfcCommand command = NfcCommandContinue;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    app.transport_nfc_state_storage.iso4_active = true;
    app.transport_nfc_state_storage.iso_pcb = ZF_NFC_PCB_BLOCK;

    for (size_t i = 8U; i < sizeof(apdu); ++i) {
        apdu[i] = (uint8_t)i;
    }

    furi_mutex_acquire(&mutex, FuriWaitForever);
    expect(zf_transport_nfc_handle_iso4_payload_locked(&app, &app.transport_nfc_state_storage, apdu,
                                                       sizeof(apdu), 0x12, true,
                                                       false) == NfcCommandContinue,
           "first chained I-block should start APDU assembly");
    expect(app.transport_nfc_state_storage.command_chain_active &&
               app.transport_nfc_state_storage.request_len == sizeof(apdu),
           "first chained I-block should be retained");

    for (size_t i = 0U; i < 9U; ++i) {
        furi_mutex_acquire(&mutex, FuriWaitForever);
        command = zf_transport_nfc_handle_iso4_payload_locked(
            &app, &app.transport_nfc_state_storage, apdu, sizeof(apdu), 0x12, true, false);
    }

    expect(command == NfcCommandReset,
           "repeated duplicate chained I-blocks should break the RF exchange");
    expect(mutex.depth == 0U, "stalled chained I-block reset should release the callback mutex");
    expect(!app.transport_nfc_state_storage.command_chain_active &&
               app.transport_nfc_state_storage.request_len == 0U &&
               !app.transport_nfc_state_storage.rx_chain_last_valid,
           "stalled chained I-block reset should discard partial APDU assembly");
    expect(test_log_contains("NFC I-chain stall"),
           "stalled chained I-block reset should expose a diagnostic breadcrumb");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap2_msg_rejects_invalid_p1_p2(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t rfu_p1_apdu[] = {0x80, 0x10, 0x01, 0x00, 0x01, ZfCtapeCmdMakeCredential};
    static const uint8_t rfu_p2_apdu[] = {0x80, 0x10, ZF_NFC_CTAP_MSG_P1_GET_RESPONSE,
                                          0x01, 0x01, ZfCtapeCmdMakeCredential};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, rfu_p1_apdu,
                                        sizeof(rfu_p1_apdu)),
           "NFC CTAP2 MSG should reject RFU P1 bits");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U && payload[0] == 0x6B && payload[1] == 0x00,
           "RFU P1 bits should return SW_WRONG_P1P2");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending,
           "RFU P1 rejection must not queue NFC background processing");

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, rfu_p2_apdu,
                                        sizeof(rfu_p2_apdu)),
           "NFC CTAP2 MSG should reject RFU P2");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U && payload[0] == 0x6B && payload[1] == 0x00,
           "RFU P2 should return SW_WRONG_P1P2");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending,
           "RFU P2 rejection must not queue NFC background processing");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap2_get_info_rejects_invalid_p1_p2(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t rfu_p1_get_info_apdu[] = {0x80, 0x10, 0x01, 0x00, 0x01, ZfCtapeCmdGetInfo};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage,
                                        rfu_p1_get_info_apdu, sizeof(rfu_p1_get_info_apdu)),
           "immediate NFC CTAP2 GetInfo should reject RFU P1/P2");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U && payload[0] == 0x6B && payload[1] == 0x00,
           "immediate NFC CTAP2 GetInfo RFU P1/P2 should return SW_WRONG_P1P2");

    test_nfc_deinit_app(&app);
}

static void test_nfc_locked_ctap2_get_info_does_not_reenter_ui_mutex(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t get_info_apdu[] = {0x80, 0x10, 0x00, 0x00, 0x01, ZfCtapeCmdGetInfo};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    g_mutex_max_depth = 0;

    furi_mutex_acquire(&mutex, FuriWaitForever);
    expect(zf_transport_nfc_handle_apdu_locked(&app, &app.transport_nfc_state_storage,
                                               get_info_apdu, sizeof(get_info_apdu)),
           "locked NFC CTAP2 GetInfo should complete");
    expect(mutex.depth == 1U, "locked NFC CTAP2 GetInfo should return with caller mutex held");
    furi_mutex_release(&mutex);

    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 4U && payload[0] == ZF_CTAP_SUCCESS && payload[1] == 0xA0 &&
               payload[2] == 0x90 && payload[3] == 0x00,
           "locked NFC CTAP2 GetInfo should return the immediate response");
    expect(g_mutex_max_depth == 1U, "locked NFC CTAP2 GetInfo must not re-acquire UI mutex");

    test_nfc_deinit_app(&app);
}

static void test_nfc_stale_worker_completion_preserves_new_request(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t first_request[] = {ZfCtapeCmdMakeCredential};
    static const uint8_t second_request[] = {ZfCtapeCmdGetAssertion};
    static const uint8_t stale_response[] = {ZF_CTAP_SUCCESS};
    ZfTransportSessionId old_session = 0;
    uint32_t old_generation = 0U;
    uint32_t new_generation = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_queue_request_locked(&app, &app.transport_nfc_state_storage,
                                                 ZfNfcRequestKindCtap2, first_request,
                                                 sizeof(first_request)),
           "first NFC CTAP2 request should queue");
    old_session = app.transport_nfc_state_storage.processing_session_id;
    old_generation = app.transport_nfc_state_storage.processing_generation;

    zf_transport_nfc_reset_exchange_locked(&app.transport_nfc_state_storage);
    expect(zf_transport_nfc_queue_request_locked(&app, &app.transport_nfc_state_storage,
                                                 ZfNfcRequestKindCtap2, second_request,
                                                 sizeof(second_request)),
           "second NFC CTAP2 request should queue after reset");
    new_generation = app.transport_nfc_state_storage.processing_generation;
    expect(new_generation != old_generation, "new NFC request should get a new generation token");

    zf_transport_nfc_store_response(&app, &app.transport_nfc_state_storage, old_session,
                                    old_generation, stale_response, sizeof(stale_response), false,
                                    false, ZF_NFC_SW_SUCCESS);
    expect(app.transport_nfc_state_storage.processing &&
               app.transport_nfc_state_storage.request_pending,
           "stale NFC completion should not clear the newer queued request");
    expect(app.transport_nfc_state_storage.processing_generation == new_generation,
           "stale NFC completion should not overwrite the newer generation");
    expect(!app.transport_nfc_state_storage.response_ready,
           "stale NFC completion should not publish a response");

    test_nfc_deinit_app(&app);
}

static void test_nfc_worker_response_buffer_survives_requeued_request(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t first_request[] = {ZfCtapeCmdGetInfo};

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_queue_request_locked(&app, &app.transport_nfc_state_storage,
                                                 ZfNfcRequestKindCtap2, first_request,
                                                 sizeof(first_request)),
           "first NFC CTAP2 request should queue for worker processing");
    g_ctap2_requeue_during_handle = true;

    zf_transport_nfc_process_request(&app);
    expect(!g_ctap2_requeued_arena_corrupted,
           "late NFC worker response writes must not corrupt a requeued request arena");
    expect(app.transport_arena && app.transport_arena[0] == ZfCtapeCmdGetAssertion,
           "requeued NFC request should remain intact after stale worker completion");
    expect(app.transport_nfc_state_storage.processing &&
               app.transport_nfc_state_storage.request_pending,
           "stale NFC worker completion should leave the requeued request active");
    expect(!app.transport_nfc_state_storage.response_ready,
           "stale NFC worker completion should not publish its obsolete response");
    expect(!app.transport_auto_accept_transaction,
           "stale NFC worker completion should restore the auto-accept override");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap_control_end_clears_selected_applet(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t control_end_apdu[] = {0x80, ZF_NFC_INS_CTAP_CONTROL, 0x01, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    expect(app.transport_nfc_state_storage.applet_selected,
           "test should begin with the FIDO applet selected");

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, control_end_apdu,
                                        sizeof(control_end_apdu)),
           "NFCCTAP_CONTROL END should return success");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U && payload[0] == 0x90 && payload[1] == 0x00,
           "NFCCTAP_CONTROL END should return SW_SUCCESS");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "NFCCTAP_CONTROL END should clear FIDO applet selection");
    expect(test_log_contains("CTAP control END"),
           "NFCCTAP_CONTROL END should expose a clear status breadcrumb");

    test_nfc_deinit_app(&app);
}

static void test_nfc_u2f_version_returns_immediately(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t version_apdu[] = {0x00, U2F_CMD_VERSION, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    expect(zf_u2f_adapter_init(&app), "U2F should initialize before NFC VERSION");
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, version_apdu,
                                        sizeof(version_apdu)),
           "U2F VERSION APDU should complete immediately after SELECT");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 8, "U2F VERSION should return U2F_V2 plus SW_SUCCESS");
    expect(memcmp(payload, "U2F_V2", 6) == 0, "U2F VERSION should return U2F_V2 over NFC");
    expect(payload[6] == 0x90 && payload[7] == 0x00,
           "U2F VERSION over NFC should terminate with SW_SUCCESS");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending,
           "U2F VERSION over NFC should not leave a queued background request");
    expect(test_log_contains("U2F VERSION"),
           "U2F VERSION over NFC should expose the immediate-version breadcrumb");

    zf_u2f_adapter_deinit(&app);
    test_nfc_deinit_app(&app);
}

static void test_nfc_u2f_version_fallback_selects_fido_surface(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t version_apdu[] = {0x00, U2F_CMD_VERSION, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, version_apdu,
                                        sizeof(version_apdu)),
           "pre-SELECT U2F VERSION fallback should complete immediately");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 8U && memcmp(payload, "U2F_V2", 6) == 0 &&
               payload[6] == 0x90 && payload[7] == 0x00,
           "pre-SELECT U2F VERSION fallback should return U2F_V2 plus SW_SUCCESS");
    expect(app.transport_nfc_state_storage.applet_selected,
           "pre-SELECT U2F VERSION fallback should select the FIDO surface");
    expect(test_log_contains("U2F VERSION fallback"),
           "pre-SELECT U2F VERSION fallback should expose the fallback breadcrumb");

    test_nfc_deinit_app(&app);
}

static void test_nfc_u2f_disabled_rejects_immediately_without_processing(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t version_apdu[] = {0x00, U2F_CMD_VERSION, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.runtime_config.transport_mode = ZfTransportModeNfc;
    app.runtime_config.u2f_enabled = false;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    test_nfc_select_applet(&app);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, version_apdu,
                                        sizeof(version_apdu)),
           "disabled U2F should still return a status word over NFC");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2 && payload[0] == 0x6D && payload[1] == 0x00,
           "disabled U2F should reject NFC U2F APDUs with SW_INS_NOT_SUPPORTED");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending,
           "disabled U2F should not queue NFC background processing");

    test_nfc_deinit_app(&app);
}

static void test_nfc_u2f_check_only_authenticate_returns_immediately(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    uint8_t authenticate_apdu[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64] = {
        0x00,
        U2F_CMD_AUTHENTICATE,
        U2fCheckOnly,
        0x00,
        0x00,
        0x00,
        U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64,
    };
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    test_nfc_select_applet(&app);
    authenticate_apdu[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = 64;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, authenticate_apdu,
                                        sizeof(authenticate_apdu)),
           "U2F check-only AUTHENTICATE over NFC should complete immediately");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U,
           "U2F check-only AUTHENTICATE should return a status word without STATUS_UPDATE");
    expect(payload[0] == 0x69 && payload[1] == 0x85,
           "valid U2F check-only AUTHENTICATE should report user presence required");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending,
           "U2F check-only AUTHENTICATE must not leave a queued NFC request");
    expect(g_approval_request_count == 0U,
           "U2F check-only AUTHENTICATE must not ask for local approval");
    expect(test_log_contains("U2F check-only"),
           "U2F check-only AUTHENTICATE should expose the immediate U2F breadcrumb");

    zf_u2f_adapter_deinit(&app);
    test_nfc_deinit_app(&app);
}

static void test_nfc_u2f_register_returns_without_status_update(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    uint8_t register_apdu[5 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = {
        0x00, U2F_CMD_REGISTER, 0x00, 0x00, U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE,
    };
    static const uint8_t get_response_apdu[] = {0x00, ZF_NFC_INS_ISO_GET_RESPONSE, 0x00, 0x00,
                                                0x00};
    const uint8_t *payload = NULL;
    size_t payload_len = 0U;
    size_t guard = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    expect(zf_u2f_adapter_init(&app), "U2F should initialize before NFC REGISTER");
    test_nfc_select_applet(&app);
    memset(&register_apdu[5], 0xA5, U2F_CHALLENGE_SIZE);
    memset(&register_apdu[5 + U2F_CHALLENGE_SIZE], 0x5A, U2F_APP_ID_SIZE);

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, register_apdu,
                                        sizeof(register_apdu)),
           "U2F REGISTER over NFC should complete on the raw APDU path");
    payload = test_nfc_payload();
    payload_len = test_nfc_payload_len();
    expect(!(payload_len == 3U && payload[0] == ZF_NFC_STATUS_PROCESSING && payload[1] == 0x91 &&
             payload[2] == 0x00),
           "U2F REGISTER over NFC must not use CTAP STATUS_UPDATE polling");
    expect(!app.transport_nfc_state_storage.processing &&
               !app.transport_nfc_state_storage.request_pending,
           "U2F REGISTER over NFC must not leave a queued background request");
    expect(g_approval_request_count == 0U,
           "U2F REGISTER over NFC should use transport presence instead of approval UI");

    while (payload_len >= 2U && payload[payload_len - 2U] == 0x61U && guard++ < 8U) {
        expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage,
                                            get_response_apdu, sizeof(get_response_apdu)),
               "U2F REGISTER ISO response chain should continue via GET RESPONSE");
        payload = test_nfc_payload();
        payload_len = test_nfc_payload_len();
        expect(!(payload_len == 3U && payload[0] == ZF_NFC_STATUS_PROCESSING &&
                 payload[1] == 0x91 && payload[2] == 0x00),
               "U2F REGISTER response chaining must not use CTAP STATUS_UPDATE polling");
    }

    expect(payload_len >= 2U && payload[payload_len - 2U] == 0x90 &&
               payload[payload_len - 1U] == 0x00,
           "U2F REGISTER over NFC should finish with the U2F success status word");
    expect(test_log_contains("U2F immediate"),
           "U2F REGISTER over NFC should expose the immediate U2F breadcrumb");

    zf_u2f_adapter_deinit(&app);
    test_nfc_deinit_app(&app);
}

static void test_nfc_get_response_reports_remaining_bytes(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t get_response_apdu[] = {0x80, 0xC0, 0x00, 0x00, 0x0A};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    memset(app.transport_arena, 0xAB, zf_app_transport_arena_capacity(&app));
    app.transport_nfc_state_storage.response_ready = true;
    app.transport_nfc_state_storage.response_len = 300;
    app.transport_nfc_state_storage.response_offset = 0;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, get_response_apdu,
                                        sizeof(get_response_apdu)),
           "GET RESPONSE should return a partial chunk when Le is smaller than the response");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 12, "partial GET RESPONSE should return Le bytes plus SW");
    expect(payload[10] == 0x61 && payload[11] == 0x00,
           "partial GET RESPONSE should advertise remaining bytes with SW_BYTES_REMAINING");
    expect(app.transport_nfc_state_storage.response_offset == 10,
           "partial GET RESPONSE should advance the response offset");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap_get_response_ins_11_rejects_processing_without_advertised_support(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t get_response_apdu[] = {0x80, ZF_NFC_INS_CTAP_GET_RESPONSE, 0x00, 0x00,
                                                0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    app.transport_nfc_state_storage.processing = true;
    app.transport_nfc_state_storage.response_ready = false;
    app.transport_nfc_state_storage.ctap_get_response_supported = false;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, get_response_apdu,
                                        sizeof(get_response_apdu)),
           "CTAP GET_RESPONSE INS=0x11 should be rejected when support was not advertised");
    payload = test_nfc_payload();
    expect(
        test_nfc_payload_len() == 2U && payload[0] == 0x69 && payload[1] == 0x85,
        "CTAP GET_RESPONSE without advertised support should return SW_CONDITIONS_NOT_SATISFIED");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap_get_response_ins_11_rejects_invalid_p1_p2(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t rfu_p1_get_response_apdu[] = {0x80, ZF_NFC_INS_CTAP_GET_RESPONSE, 0x01,
                                                       0x00, 0x00};
    static const uint8_t rfu_p2_get_response_apdu[] = {0x80, ZF_NFC_INS_CTAP_GET_RESPONSE, 0x00,
                                                       0x01, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    app.transport_nfc_state_storage.processing = true;
    app.transport_nfc_state_storage.response_ready = false;
    app.transport_nfc_state_storage.ctap_get_response_supported = true;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage,
                                        rfu_p1_get_response_apdu, sizeof(rfu_p1_get_response_apdu)),
           "NFCCTAP_GETRESPONSE should reject RFU P1");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U && payload[0] == 0x6B && payload[1] == 0x00,
           "NFCCTAP_GETRESPONSE RFU P1 should return SW_WRONG_P1P2");

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage,
                                        rfu_p2_get_response_apdu, sizeof(rfu_p2_get_response_apdu)),
           "NFCCTAP_GETRESPONSE should reject RFU P2");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 2U && payload[0] == 0x6B && payload[1] == 0x00,
           "NFCCTAP_GETRESPONSE RFU P2 should return SW_WRONG_P1P2");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap_get_response_ins_11_reports_processing_with_advertised_support(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t get_response_apdu[] = {0x80, ZF_NFC_INS_CTAP_GET_RESPONSE, 0x00, 0x00,
                                                0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    app.transport_nfc_state_storage.processing = true;
    app.transport_nfc_state_storage.response_ready = false;
    app.transport_nfc_state_storage.ctap_get_response_supported = true;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, get_response_apdu,
                                        sizeof(get_response_apdu)),
           "CTAP GET_RESPONSE INS=0x11 should be accepted while processing");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 3U,
           "CTAP GET_RESPONSE INS=0x11 should return processing status plus SW");
    expect(payload[0] == ZF_NFC_STATUS_PROCESSING && payload[1] == 0x91 && payload[2] == 0x00,
           "CTAP GET_RESPONSE INS=0x11 should return STATUS_UPDATE while processing");

    test_nfc_deinit_app(&app);
}

static void test_nfc_busy_u2f_request_preserves_shared_arena(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t version_apdu[] = {0x00, U2F_CMD_VERSION, 0x00, 0x00, 0x00};
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    app.transport_arena[0] = 0xAB;
    app.transport_arena[1] = 0xCD;
    app.transport_nfc_state_storage.processing = true;
    app.transport_nfc_state_storage.request_pending = true;
    app.transport_nfc_state_storage.response_ready = false;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, version_apdu,
                                        sizeof(version_apdu)),
           "busy NFC U2F request should return a status-update response");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 3U,
           "busy NFC U2F request should return processing status plus SW");
    expect(payload[0] == ZF_NFC_STATUS_PROCESSING && payload[1] == 0x91 && payload[2] == 0x00,
           "busy NFC U2F request should report STATUS_UPDATE");
    expect(app.transport_arena[0] == 0xAB && app.transport_arena[1] == 0xCD,
           "busy NFC U2F request should not clobber the shared arena");

    test_nfc_deinit_app(&app);
}

static void test_nfc_ctap_get_response_ins_11_returns_ready_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    static const uint8_t get_response_apdu[] = {0x80, ZF_NFC_INS_CTAP_GET_RESPONSE, 0x00, 0x00,
                                                0x00};
    const uint8_t *payload = NULL;
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);

    app.transport_arena[0] = 0xAB;
    app.transport_arena[1] = 0xCD;
    app.transport_nfc_state_storage.response_ready = true;
    app.transport_nfc_state_storage.response_len = 2U;
    app.transport_nfc_state_storage.response_offset = 0U;

    expect(zf_transport_nfc_handle_apdu(&app, &app.transport_nfc_state_storage, get_response_apdu,
                                        sizeof(get_response_apdu)),
           "CTAP GET_RESPONSE INS=0x11 should return a ready CTAP response");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 4U,
           "CTAP GET_RESPONSE INS=0x11 should return response bytes plus SW");
    expect(payload[0] == 0xAB && payload[1] == 0xCD && payload[2] == 0x90 && payload[3] == 0x00,
           "CTAP GET_RESPONSE INS=0x11 should terminate the ready response with SW_SUCCESS");
    expect(!app.transport_nfc_state_storage.response_ready,
           "complete CTAP GET_RESPONSE should clear the queued response");
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);
    expect(zf_transport_nfc_replay_last_iso_response(&app.transport_nfc_state_storage),
           "complete CTAP GET_RESPONSE should remain replayable for reader R-NAK");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "complete CTAP GET_RESPONSE replay should resend the same ISO-DEP frame");

    test_nfc_deinit_app(&app);
}

static void test_nfc_field_off_marks_current_session_canceled(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    Iso14443_4aListenerEvent listener_event = {.type = Iso14443_4aListenerEventTypeFieldOff};
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    test_nfc_select_applet(&app);
    app.transport_nfc_state_storage.processing = true;
    app.transport_nfc_state_storage.request_pending = true;
    app.transport_nfc_state_storage.processing_session_id =
        app.transport_nfc_state_storage.session_id;

    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandSleep,
           "field-off should put the NFC listener to sleep");
    expect(zf_transport_nfc_poll_cbor_control(&app, app.transport_nfc_state_storage.session_id) ==
               ZF_CTAP_ERR_KEEPALIVE_CANCEL,
           "field-off should surface transport cancellation for the active NFC session");
    expect(g_cancel_pending_interaction_count == 1,
           "field-off should cancel any pending user interaction");
    expect(g_transport_connected_set_count == 1 && !g_last_transport_connected,
           "field-off should publish that the NFC transport disconnected");

    test_nfc_deinit_app(&app);
}

static void test_nfc_preselect_raw60_sends_iso_status(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x60}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "raw 0x60 should receive an unsupported status before SELECT");
    expect(g_nfc_tx_count == 1U && g_last_nfc_tx_len == 11U && g_last_nfc_tx[0] == 0x02 &&
               g_last_nfc_tx[1] == 0xAF && g_last_nfc_tx[2] == 0x04,
           "raw 0x60 should transmit status-first native DESFire version data inside an I-block");
    expect(test_log_contains("DESFire native version"),
           "raw 0x60 should expose the classifier breadcrumb");
    expect(g_transport_connected_set_count == 1U && g_last_transport_connected,
           "raw 0x60 ISO status handling should leave the NFC transport active");
    expect(g_cancel_pending_interaction_count == 0U,
           "raw 0x60 ISO status handling should not cancel pending NFC interaction");
    expect(!app.transport_nfc_state_storage.applet_selected,
           "raw native probe should not mark the FIDO applet selected");

    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);
    expect(zf_transport_nfc_replay_last_iso_response(&app.transport_nfc_state_storage),
           "first raw 0x60 response should remain replayable for discovery recovery");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "first raw 0x60 replay should resend the DESFire discovery response");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_duplicate_native_desfire_af_replays_previous_frame(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    uint8_t cached_af_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_af_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02, 0x60}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "initial native DESFire probe should start the shim sequence");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x03, 0xAF}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "first native DESFire AF should return the second shim frame");
    cached_af_response_len = g_last_nfc_tx_len;
    memcpy(cached_af_response, g_last_nfc_tx, cached_af_response_len);

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x03, 0xAF}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "duplicate native DESFire AF should replay instead of advancing the shim");
    expect(g_last_nfc_tx_len == cached_af_response_len &&
               memcmp(g_last_nfc_tx, cached_af_response, cached_af_response_len) == 0,
           "duplicate native DESFire AF should replay the previous AF response");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02, 0xAF}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "next fresh native DESFire AF should still advance the shim");
    expect(g_last_nfc_tx_len != cached_af_response_len ||
               memcmp(g_last_nfc_tx, cached_af_response, cached_af_response_len) != 0,
           "fresh native DESFire AF should return the final shim frame after the duplicate replay");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_repeated_native_desfire_probe_keeps_discovery_compatible(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    g_fake_tick = 1000U;

    for (size_t i = 0; i < 2U; ++i) {
        bit_buffer_reset(buffer);
        bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02, 0x60}, 2U);
        expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
               "first native DESFire discovery probes should remain compatible");
        expect(g_last_nfc_tx_len == 11U && (g_last_nfc_tx[0] & 0xFEU) == 0x02 &&
                   g_last_nfc_tx[1] == 0xAF,
               "allowed native DESFire discovery should return the version shim");
        g_fake_tick += 100U;
    }

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x02, 0x60}, 2U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "repeated native DESFire discovery should remain compatible with the known-good path");
    expect(g_nfc_tx_count == 3U && g_last_nfc_tx_len == 11U && (g_last_nfc_tx[0] & 0xFEU) == 0x02 &&
               g_last_nfc_tx[1] == 0xAF,
           "repeated native DESFire discovery should keep returning the version shim");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x02, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "native DESFire discovery should not block FIDO SELECT");
    expect(app.transport_nfc_state_storage.applet_selected,
           "FIDO SELECT after repeated DESFire discovery should select the applet");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_native_desfire_preserves_existing_fido_replay(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    static const uint8_t fido_response[] = {0x01, 0xA0};
    uint8_t cached_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_send_apdu_payload(&app.transport_nfc_state_storage, fido_response,
                                              sizeof(fido_response), ZF_NFC_SW_SUCCESS),
           "test should prime an existing FIDO replay response");
    cached_response_len = g_last_nfc_tx_len;
    memcpy(cached_response, g_last_nfc_tx, cached_response_len);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x60}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "native DESFire discovery should still be answered while a FIDO replay exists");
    expect(g_last_nfc_tx_len != cached_response_len ||
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) != 0,
           "native DESFire discovery should transmit its own response first");

    expect(zf_transport_nfc_replay_last_iso_response(&app.transport_nfc_state_storage),
           "native DESFire discovery should preserve the existing FIDO replay cache");
    expect(g_last_nfc_tx_len == cached_response_len &&
               memcmp(g_last_nfc_tx, cached_response, cached_response_len) == 0,
           "replay after native DESFire discovery should resend the prior FIDO response");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_wrapped_desfire_duplicate_replays_desfire_response(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(16U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    static const uint8_t fido_response[] = {0x01, 0xA0};
    static const uint8_t wrapped_desfire[] = {0x02, 0x90, 0x60, 0x00, 0x00, 0x00};
    uint8_t cached_fido_response[sizeof(g_last_nfc_tx)] = {0};
    uint8_t cached_desfire_response[sizeof(g_last_nfc_tx)] = {0};
    size_t cached_fido_response_len = 0U;
    size_t cached_desfire_response_len = 0U;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    expect(zf_transport_nfc_send_apdu_payload(&app.transport_nfc_state_storage, fido_response,
                                              sizeof(fido_response), ZF_NFC_SW_SUCCESS),
           "test should prime an existing FIDO replay response");
    cached_fido_response_len = g_last_nfc_tx_len;
    memcpy(cached_fido_response, g_last_nfc_tx, cached_fido_response_len);

    bit_buffer_append_bytes(buffer, wrapped_desfire, sizeof(wrapped_desfire));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "wrapped DESFire probe should receive its own response");
    cached_desfire_response_len = g_last_nfc_tx_len;
    memcpy(cached_desfire_response, g_last_nfc_tx, cached_desfire_response_len);
    expect(cached_desfire_response_len != cached_fido_response_len ||
               memcmp(cached_desfire_response, cached_fido_response, cached_fido_response_len) != 0,
           "wrapped DESFire response should differ from the primed FIDO replay");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer, wrapped_desfire, sizeof(wrapped_desfire));
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "duplicate wrapped DESFire probe should replay the DESFire response");
    expect(g_last_nfc_tx_len == cached_desfire_response_len &&
               memcmp(g_last_nfc_tx, cached_desfire_response, cached_desfire_response_len) == 0,
           "duplicate wrapped DESFire probe should not replay the older FIDO response");

    expect(zf_transport_nfc_replay_last_iso_response(&app.transport_nfc_state_storage),
           "wrapped DESFire duplicate replay should preserve the older FIDO replay cache");
    expect(g_last_nfc_tx_len == cached_fido_response_len &&
               memcmp(g_last_nfc_tx, cached_fido_response, cached_fido_response_len) == 0,
           "older FIDO replay cache should remain available after wrapped DESFire replay");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_raw60_does_not_make_next_framed_select_bare(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(32U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };
    const uint8_t *payload = NULL;

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x60}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "raw 0x60 should receive an unsupported status before SELECT");
    expect(g_nfc_tx_count == 1U && g_last_nfc_tx[0] == 0x02 && g_last_nfc_tx[1] == 0xAF &&
               g_last_nfc_tx[2] == 0x04,
           "raw 0x60 should be answered as status-first native DESFire version data");

    bit_buffer_reset(buffer);
    bit_buffer_append_bytes(buffer,
                            (const uint8_t[]){0x03, 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00,
                                              0x06, 0x47, 0x2F, 0x00, 0x01},
                            14U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "framed SELECT after raw 0x60 should still be accepted");
    expect(g_last_nfc_tx[0] == 0x03,
           "framed SELECT after raw 0x60 should receive an ISO-DEP I-block response");
    payload = test_nfc_payload();
    expect(test_nfc_payload_len() == 8U && memcmp(payload, "U2F_V2", 6) == 0 &&
               payload[6] == 0x90 && payload[7] == 0x00,
           "framed SELECT after raw 0x60 should not be sent as a bare APDU response");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_preselect_raw_native_probe_traces_halt(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x60}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "raw 0x60 should be observed before halt");

    listener_event.type = Iso14443_4aListenerEventTypeHalted;
    listener_event.data = NULL;
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandSleep,
           "halt after raw native probe should sleep the NFC listener");
    expect(g_last_transport_connected == false,
           "halt after raw 0x60 should publish that NFC disconnected");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}

static void test_nfc_selected_raw_native_probe_traces_selected_state(void) {
    ZerofidoApp app;
    FuriMutex mutex = {0};
    BitBuffer *buffer = bit_buffer_alloc(8U);
    Iso14443_4aListenerEventData event_data = {.buffer = buffer};
    Iso14443_4aListenerEvent listener_event = {
        .type = Iso14443_4aListenerEventTypeReceivedData,
        .data = &event_data,
    };
    NfcGenericEvent event = {
        .protocol = NfcProtocolIso14443_4a,
        .event_data = &listener_event,
    };

    test_reset();
    test_nfc_init_app(&app, &mutex);
    app.transport_nfc_state_storage.applet_selected = true;

    bit_buffer_append_bytes(buffer, (const uint8_t[]){0x60}, 1U);
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandContinue,
           "raw 0x60 should be observed after SELECT");
    expect(test_log_contains("DESFire native version"),
           "selected raw 0x60 should expose the terminal ISO status breadcrumb");
    expect(app.transport_nfc_state_storage.applet_selected,
           "selected raw 0x60 ISO status should preserve the selected applet state");

    listener_event.type = Iso14443_4aListenerEventTypeHalted;
    listener_event.data = NULL;
    expect(zf_transport_nfc_event_callback(event, &app) == NfcCommandSleep,
           "halt after selected raw 0x60 should sleep the NFC listener");
    expect(g_last_transport_connected == false,
           "halt after selected raw 0x60 should publish that NFC disconnected");

    bit_buffer_free(buffer);
    test_nfc_deinit_app(&app);
}
