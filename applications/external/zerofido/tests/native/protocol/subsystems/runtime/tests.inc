static void test_runtime_config_load_defaults_auto_accept_off(void) {
    ZfRuntimeConfig config = {.auto_accept_requests = true, .fido2_enabled = false};

    zf_runtime_config_load_defaults(&config);

    expect(!config.auto_accept_requests, "runtime config defaults should keep auto-accept off");
    expect(config.fido2_enabled, "runtime config defaults should keep FIDO2 enabled");
    expect(config.fido2_profile == ZfFido2ProfileCtap2_0,
           "runtime config defaults should use the CTAP 2.0 profile");
    expect(config.attestation_mode == ZfAttestationModeNone,
           "runtime config defaults should use none attestation");
}

static void test_runtime_config_persist_round_trips_auto_accept_setting(void) {
    Storage storage = {0};
    ZfRuntimeConfig saved = {0};
    ZfRuntimeConfig loaded = {0};

    test_storage_reset();
    zf_runtime_config_load_defaults(&saved);
    saved.auto_accept_requests = true;
    saved.fido2_enabled = false;

    expect(zf_runtime_config_persist(&storage, &saved),
           "persisting runtime config should succeed in native storage");

    zf_runtime_config_load(&storage, &loaded);
    expect(loaded.auto_accept_requests,
           "runtime config load should restore persisted auto-accept setting");
    expect(loaded.fido2_enabled,
           "runtime config load should ignore stale persisted FIDO2 disabled state");
}

static void test_runtime_config_persist_round_trips_transport_mode(void) {
    Storage storage = {0};
    ZfRuntimeConfig saved = {0};
    ZfRuntimeConfig loaded = {0};

    test_storage_reset();
    zf_runtime_config_load_defaults(&saved);
    saved.transport_mode = ZfTransportModeNfc;

    expect(zf_runtime_config_persist(&storage, &saved),
           "persisting NFC transport config should succeed in native storage");

    zf_runtime_config_load(&storage, &loaded);
    expect(loaded.transport_mode == ZfTransportModeNfc,
           "runtime config load should restore the persisted NFC transport mode");
}

static void test_runtime_config_persist_round_trips_fido2_profile(void) {
    Storage storage = {0};
    ZfRuntimeConfig saved = {0};
    ZfRuntimeConfig loaded = {0};

    test_storage_reset();
    zf_runtime_config_load_defaults(&saved);
    saved.fido2_profile = ZfFido2ProfileCtap2_1Experimental;

    expect(zf_runtime_config_persist(&storage, &saved),
           "persisting experimental FIDO2 profile should succeed in native storage");

    zf_runtime_config_load(&storage, &loaded);
    expect(loaded.fido2_profile == ZfFido2ProfileCtap2_1Experimental,
           "runtime config load should restore the persisted FIDO2 profile");
}

static void test_runtime_config_persist_round_trips_attestation_mode(void) {
    Storage storage = {0};
    ZfRuntimeConfig saved = {0};
    ZfRuntimeConfig loaded = {0};

    test_storage_reset();
    zf_runtime_config_load_defaults(&saved);
    saved.attestation_mode = ZfAttestationModeNone;

    expect(zf_runtime_config_persist(&storage, &saved),
           "persisting none attestation mode should succeed in native storage");

    zf_runtime_config_load(&storage, &loaded);
    expect(loaded.attestation_mode == ZfAttestationModeNone,
           "runtime config load should restore the persisted attestation mode");
}

static void test_runtime_config_set_attestation_mode_updates_capabilities(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.pin_state.pin_set = true;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_runtime_config_set_attestation_mode(&app, &storage, ZfAttestationModeNone),
           "runtime config should persist the selected attestation mode");
    expect(app.runtime_config.attestation_mode == ZfAttestationModeNone,
           "runtime config should apply the selected attestation mode");
    expect(app.capabilities.attestation_mode == ZfAttestationModeNone,
           "resolved capabilities should expose the selected attestation mode");
    expect(strcmp(zf_attestation_mode_name(app.runtime_config.attestation_mode), "None") == 0,
           "attestation label should expose none mode");
}

static void test_runtime_config_set_fido2_profile_updates_capabilities(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.pin_state.pin_set = true;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_runtime_config_set_fido2_profile(&app, &storage, ZfFido2ProfileCtap2_1Experimental),
           "runtime config should persist the user-selected FIDO2 profile");
    expect(app.runtime_config.fido2_profile == ZfFido2ProfileCtap2_1Experimental,
           "runtime config should apply the selected FIDO2 profile");
    expect(app.capabilities.advertise_fido_2_1,
           "experimental FIDO2 profile should advertise FIDO_2_1");
    expect(app.capabilities.pin_uv_auth_protocol_2_enabled,
           "experimental FIDO2 profile should enable protocol 2 advertisement");
    expect(app.capabilities.selection_enabled,
           "experimental FIDO2 profile should enable authenticatorSelection");
    expect(strcmp(zf_fido2_profile_name(app.runtime_config.fido2_profile), "2.1 exp") == 0,
           "FIDO2 profile label should expose the experimental profile");
}

static void test_runtime_config_set_fido2_profile_requires_pin_for_2_1(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(!zf_runtime_config_set_fido2_profile(&app, &storage, ZfFido2ProfileCtap2_1Experimental),
           "runtime config should reject CTAP 2.1 profile while PIN is unset");
    expect(app.runtime_config.fido2_profile == ZfFido2ProfileCtap2_0,
           "failed PIN-gated profile switch should keep CTAP 2.0");
    expect(!app.capabilities.advertise_fido_2_1,
           "failed PIN-gated profile switch should not advertise CTAP 2.1");
}

static void test_runtime_config_apply_downgrades_2_1_when_pin_is_unset(void) {
    ZerofidoApp app = {0};
    ZfRuntimeConfig config = {0};

    zf_runtime_config_load_defaults(&config);
    config.fido2_profile = ZfFido2ProfileCtap2_1Experimental;
    zf_runtime_config_apply(&app, &config);

    expect(app.runtime_config.fido2_profile == ZfFido2ProfileCtap2_1Experimental,
           "runtime config apply should preserve the requested CTAP 2.1 profile");
    expect(app.capabilities.fido2_profile == ZfFido2ProfileCtap2_0,
           "resolved capabilities should downgrade CTAP 2.1 when PIN is unset");
    expect(!app.capabilities.advertise_fido_2_1,
           "downgraded capabilities should not advertise CTAP 2.1");
    expect(!app.capabilities.selection_enabled,
           "downgraded capabilities should keep authenticatorSelection disabled");
}

static void test_runtime_config_pin_refresh_restores_requested_2_1_profile(void) {
    ZerofidoApp app = {0};
    ZfRuntimeConfig config = {0};

    zf_runtime_config_load_defaults(&config);
    config.fido2_profile = ZfFido2ProfileCtap2_1Experimental;
    zf_runtime_config_apply(&app, &config);

    app.pin_state.pin_set = true;
    zf_runtime_config_refresh_capabilities(&app);
    expect(app.runtime_config.fido2_profile == ZfFido2ProfileCtap2_1Experimental,
           "PIN refresh should keep the requested CTAP 2.1 profile");
    expect(app.capabilities.fido2_profile == ZfFido2ProfileCtap2_1Experimental,
           "PIN refresh should restore effective CTAP 2.1 capabilities");
    expect(app.capabilities.advertise_fido_2_1,
           "PIN refresh should advertise CTAP 2.1 after PIN is set");

    app.pin_state.pin_set = false;
    zf_runtime_config_refresh_capabilities(&app);
    expect(app.runtime_config.fido2_profile == ZfFido2ProfileCtap2_1Experimental,
           "clearing PIN should not erase the requested CTAP 2.1 profile");
    expect(app.capabilities.fido2_profile == ZfFido2ProfileCtap2_0,
           "clearing PIN should downgrade only effective capabilities");
}

static void test_runtime_config_persist_preserves_requested_2_1_after_pin_clear(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfRuntimeConfig config = {0};
    ZfRuntimeConfig loaded = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&config);
    config.fido2_profile = ZfFido2ProfileCtap2_1Experimental;
    zf_runtime_config_apply(&app, &config);

    expect(zf_runtime_config_set_auto_accept_requests(&app, &storage, true),
           "persisting another setting after PIN clear should succeed");
    zf_runtime_config_load(&storage, &loaded);
    expect(loaded.fido2_profile == ZfFido2ProfileCtap2_1Experimental,
           "persisting another setting should not write the downgraded profile");
    expect(loaded.auto_accept_requests,
           "persisting another setting should still update that setting");
}

static void test_runtime_config_load_invalid_file_falls_back_to_defaults(void) {
    Storage storage = {0};
    ZfRuntimeConfig loaded = {.auto_accept_requests = true, .fido2_enabled = false};
    TestStorageFile *file = NULL;

    test_storage_reset();
    file = test_storage_file_slot(ZF_RUNTIME_CONFIG_FILE_PATH, true);
    expect(file != NULL, "allocate runtime config storage slot");
    file->exists = true;
    file->size = 1;
    file->data[0] = 0xFF;

    zf_runtime_config_load(&storage, &loaded);
    expect(!loaded.auto_accept_requests,
           "invalid runtime config file should fall back to defaults");
    expect(loaded.fido2_enabled,
           "invalid runtime config file should restore default FIDO2 availability");
}

static void test_runtime_config_load_rejects_unsupported_version(void) {
    Storage storage = {0};
    ZfRuntimeConfig loaded = {.auto_accept_requests = true, .fido2_enabled = false};
    TestStorageFile *file = NULL;
    ZfRuntimeConfigFileRecord record = {
        .magic = ZF_RUNTIME_CONFIG_FILE_MAGIC,
        .version = 5U,
        .flags = ZF_RUNTIME_CONFIG_FLAG_AUTO_ACCEPT_REQUESTS,
        .transport_mode = ZfTransportModeUsbHid,
        .fido2_profile = ZfFido2ProfileCtap2_0,
    };

    test_storage_reset();
    file = test_storage_file_slot(ZF_RUNTIME_CONFIG_FILE_PATH, true);
    expect(file != NULL, "allocate runtime config storage slot for unsupported version");
    file->exists = true;
    file->size = sizeof(record);
    memcpy(file->data, &record, sizeof(record));

    zf_runtime_config_load(&storage, &loaded);
    expect(!loaded.auto_accept_requests,
           "unsupported runtime config version should fall back to defaults");
    expect(loaded.fido2_enabled,
           "unsupported runtime config version should keep default FIDO2 availability");
}

static void test_runtime_config_set_auto_accept_preserves_runtime_state_on_persist_failure(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    g_storage_fail_write_match = "runtime_config.tmp";

    expect(!zf_runtime_config_set_auto_accept_requests(&app, &storage, true),
           "runtime config mutation should fail when persistence fails");
    expect(!app.runtime_config.auto_accept_requests,
           "failed runtime config persist should keep auto-accept disabled");
    expect(!app.capabilities.auto_accept_requests,
           "failed runtime config persist should keep resolved auto-accept disabled");
}

static void test_runtime_config_set_fido2_preserves_runtime_state_on_persist_failure(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    g_storage_fail_write_match = "runtime_config.tmp";

    expect(!zf_runtime_config_set_fido2_enabled(&app, &storage, false),
           "runtime config mutation should fail when FIDO2 persist fails");
    expect(app.runtime_config.fido2_enabled,
           "failed runtime config persist should keep FIDO2 enabled");
    expect(app.capabilities.fido2_enabled,
           "failed runtime config persist should keep resolved FIDO2 enabled");
}

static void test_runtime_config_set_fido2_profile_preserves_runtime_state_on_persist_failure(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.pin_state.pin_set = true;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    g_storage_fail_write_match = "runtime_config.tmp";

    expect(!zf_runtime_config_set_fido2_profile(&app, &storage, ZfFido2ProfileCtap2_1Experimental),
           "runtime config profile mutation should fail when persistence fails");
    expect(app.runtime_config.fido2_profile == ZfFido2ProfileCtap2_0,
           "failed profile persist should keep the CTAP 2.0 profile");
    expect(!app.capabilities.advertise_fido_2_1,
           "failed profile persist should keep FIDO_2_1 disabled");
}

static void test_runtime_config_set_attestation_preserves_runtime_state_on_persist_failure(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    g_storage_fail_write_match = "runtime_config.tmp";

    expect(!zf_runtime_config_set_attestation_mode(&app, &storage, ZfAttestationModePacked),
           "runtime config attestation mutation should fail when persistence fails");
    expect(app.runtime_config.attestation_mode == ZfAttestationModeNone,
           "failed attestation persist should keep none mode");
    expect(app.capabilities.attestation_mode == ZfAttestationModeNone,
           "failed attestation persist should keep resolved none mode");
}

static void run_transport_mode_switch_failure_preserves_persisted_mode(
    ZfTransportMode from_mode, ZfTransportMode to_mode, const ZfTransportAdapterOps *adapter) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    FuriMutex ui_mutex = {0};
    ZfRuntimeConfig loaded = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.running = true;
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.runtime_config.transport_mode = from_mode;
    zf_runtime_config_apply(&app, &app.runtime_config);
    app.transport_adapter = adapter;
    expect(zf_runtime_config_persist(&storage, &app.runtime_config),
           "test setup should persist the original transport mode");
    g_thread_alloc_fail = true;

    expect(!zf_app_lifecycle_set_transport_mode(&app, &storage, to_mode),
           "transport mode switch should fail when the new worker cannot be allocated");
    expect(app.runtime_config.transport_mode == from_mode,
           "failed transport switch should restore the original runtime mode");

    zf_runtime_config_load(&storage, &loaded);
    expect(loaded.transport_mode == from_mode,
           "failed transport switch should keep the original persisted mode");
    expect(g_thread_start_count == 0,
           "failed transport switch should not start a worker when allocation fails");
}

static void test_transport_mode_switch_failure_preserves_persisted_mode(void) {
    run_transport_mode_switch_failure_preserves_persisted_mode(
        ZfTransportModeUsbHid, ZfTransportModeNfc, &zf_transport_usb_hid_adapter);
    run_transport_mode_switch_failure_preserves_persisted_mode(
        ZfTransportModeNfc, ZfTransportModeUsbHid, &zf_transport_nfc_adapter);
}
