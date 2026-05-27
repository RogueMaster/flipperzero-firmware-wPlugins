static void test_transport_reclaims_lru_cid_when_table_is_full(void) {
    ZfTransportState transport = {0};

    test_reset();
    for (size_t i = 0; i < ZF_MAX_ALLOCATED_CIDS; ++i) {
        g_fake_tick = 100U + (uint32_t)i;
        expect(zf_transport_remember_cid(&transport, 0x01020300U + (uint32_t)i),
               "fill transport CID table");
    }

    g_fake_tick = 500U;
    expect(zf_transport_remember_cid(&transport, 0xAABBCCDDU),
           "remembering a new CID should reclaim the least-recently-used slot when full");
    expect(transport.allocated_count == ZF_MAX_ALLOCATED_CIDS,
           "CID table should stay bounded after reclaiming a full table");
    expect(!zf_transport_cid_is_allocated(&transport, 0x01020300U),
           "the least-recently-used CID should be reclaimed first");
    expect(zf_transport_cid_is_allocated(&transport, 0xAABBCCDDU),
           "new CID should be tracked after reclaiming an old slot");
}

static void test_transport_allocate_cid_reclaims_lru_when_table_is_full(void) {
    ZfTransportState transport = {0};
    uint32_t allocated_cid = 0;

    test_reset();
    for (size_t i = 0; i < ZF_MAX_ALLOCATED_CIDS; ++i) {
        g_fake_tick = 200U + (uint32_t)i;
        expect(zf_transport_remember_cid(&transport, 0x02030400U + (uint32_t)i),
               "fill allocator CID table");
    }

    g_random_values[0] = 0xCAFEBABEU;
    g_random_count = 1;
    g_fake_tick = 800U;
    expect(zf_transport_allocate_cid(&transport, &allocated_cid),
           "broadcast INIT should reclaim a least-recently-used CID when the table is full");
    expect(allocated_cid == 0xCAFEBABEU, "allocator should return the newly assigned CID");
    expect(!zf_transport_cid_is_allocated(&transport, 0x02030400U),
           "allocator should reclaim the oldest tracked CID first");
    expect(zf_transport_cid_is_allocated(&transport, 0xCAFEBABEU),
           "reclaimed allocation should remember the new CID");
}

static void test_transport_cancel_marks_processing_request_without_pending_approval(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t cancel_packet[7] = {0};
    uint32_t actions = 0;

    test_reset();
    app.transport_state = &transport;
    transport.processing = true;
    transport.cmd = ZF_CTAPHID_CBOR;
    transport.cid = 0x01020304U;

    memcpy(cancel_packet, &transport.cid, sizeof(transport.cid));
    cancel_packet[4] = ZF_CTAPHID_CANCEL;

    expect(zf_transport_handle_init_command(&app, &transport, transport.cid, ZF_CTAPHID_CANCEL, 0,
                                            cancel_packet, sizeof(cancel_packet), &actions),
           "matching CANCEL should be handled while a CBOR request is processing");
    expect(transport.processing_cancel_requested,
           "CANCEL should mark the outstanding CBOR request as cancelled");
    expect(actions & ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION,
           "CANCEL should request pending-interaction dismissal");
    expect(zf_transport_usb_hid_poll_cbor_control(&app, transport.cid) ==
               ZF_CTAP_ERR_KEEPALIVE_CANCEL,
           "polling should surface a previously latched cancel request");
}

static void test_transport_cancel_from_other_cid_does_not_cancel_pending_approval(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t cancel_packet[7] = {0};
    uint32_t other_cid = 0xAABBCCDDU;
    uint32_t actions = 0;

    test_reset();
    app.transport_state = &transport;
    transport.processing = true;
    transport.cmd = ZF_CTAPHID_CBOR;
    transport.cid = 0x01020304U;

    memcpy(cancel_packet, &other_cid, sizeof(other_cid));
    cancel_packet[4] = ZF_CTAPHID_CANCEL;

    expect(zf_transport_handle_init_command(&app, &transport, other_cid, ZF_CTAPHID_CANCEL, 0,
                                            cancel_packet, sizeof(cancel_packet), &actions),
           "non-matching CANCEL should be handled without affecting the active channel");
    expect(!transport.processing_cancel_requested,
           "non-matching CANCEL should not cancel the active CBOR request");
    expect((actions & ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION) == 0,
           "non-matching CANCEL should not dismiss another channel's pending approval");
    expect(zf_transport_usb_hid_poll_cbor_control(&app, transport.cid) == ZF_CTAP_SUCCESS,
           "active channel should not observe cancel from another CID");
}

static void test_transport_active_other_cid_short_init_returns_busy(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[5] = {0};
    uint32_t cid = 0x0A0B0C0DU;

    test_reset();
    transport.active = true;
    transport.cid = 0x01020304U;
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_INIT;

    zf_transport_handle_init_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response_len == sizeof(g_last_hid_response),
           "busy init should emit a single HID error frame");
    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "busy init should return a HID error response");
    expect(g_last_hid_response[7] == ZF_HID_ERR_CHANNEL_BUSY,
           "busy init should classify the packet as channel busy");
}

static void test_transport_processing_other_cid_short_init_returns_busy(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[5] = {0};
    uint32_t cid = 0x0A0B0C0DU;

    test_reset();
    transport.processing = true;
    transport.cid = 0x01020304U;
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_INIT;

    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response_len == sizeof(g_last_hid_response),
           "busy processing init should emit a single HID error frame");
    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "busy processing init should return a HID error response");
    expect(g_last_hid_response[7] == ZF_HID_ERR_CHANNEL_BUSY,
           "busy processing init should classify the packet as channel busy");
}

static void test_transport_processing_broadcast_init_recovers_channel(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[15] = {0};
    static const uint8_t nonce[8] = {0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97};
    const uint32_t cid = 0x01020304U;
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;
    uint32_t assigned_cid = 0U;

    test_reset();
    transport.processing = true;
    transport.cid = cid;
    transport.cmd = ZF_CTAPHID_MSG;
    g_random_values[0] = 0x55667788U;
    g_random_count = 1;

    memcpy(packet, &broadcast_cid, sizeof(broadcast_cid));
    packet[4] = ZF_CTAPHID_INIT;
    packet[5] = 0x00;
    packet[6] = 0x08;
    memcpy(&packet[7], nonce, sizeof(nonce));

    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_INIT,
           "broadcast INIT should recover from a processing U2F transaction");
    expect(memcmp(&g_last_hid_response[7], nonce, sizeof(nonce)) == 0,
           "broadcast INIT recovery should echo the request nonce");
    memcpy(&assigned_cid, &g_last_hid_response[15], sizeof(assigned_cid));
    expect(assigned_cid == 0x55667788U, "broadcast INIT recovery should allocate a fresh channel");
    expect(!transport.processing, "broadcast INIT recovery should stop the stale transaction");
    expect(transport.processing_cancel_requested,
           "broadcast INIT recovery should mark the stale transaction canceled");
}

static void test_usb_restore_keeps_previous_config_after_restore_failure(void) {
    ZerofidoApp app = {0};

    test_reset();
    app.previous_usb = &g_previous_usb_config;
    g_usb_set_config_result = false;

    zf_transport_restore_usb(&app);

    expect(g_usb_set_config_count == 1, "restore should attempt the previous USB config");
    expect(g_last_usb_set_config == &g_previous_usb_config,
           "restore should pass the saved USB config back to HAL");
    expect(app.previous_usb == &g_previous_usb_config,
           "failed restore should keep previous USB config for a retry");
    expect(strcmp(g_last_status_text, "USB restore failed") == 0,
           "failed restore should publish a status message");

    g_usb_set_config_result = true;
    zf_transport_restore_usb(&app);

    expect(g_usb_set_config_count == 2, "retry should attempt USB restore again");
    expect(app.previous_usb == NULL, "successful restore should clear previous USB config");
}

static void test_usb_worker_polls_while_idle_and_assembling(void) {
    ZfTransportState transport = {0};

    expect(zf_transport_worker_next_timeout(&transport) == ZF_WORKER_POLL_MS,
           "idle USB worker should poll for queued HID requests without a wakeup");
    transport.active = true;
    expect(zf_transport_worker_next_timeout(&transport) == ZF_WORKER_POLL_MS,
           "active USB assembly still needs timeout polling");
}

static void test_nfc_worker_waits_forever_while_idle(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_thread_flag_results[0] = ZF_NFC_WORKER_EVT_STOP;
    g_thread_flag_result_count = 1;

    expect(zf_transport_nfc_worker(&app) == 0, "NFC worker should exit cleanly on stop");
    expect(g_last_thread_wait_timeout == FuriWaitForever,
           "idle NFC worker should wait indefinitely for request or stop events");
}

static void test_nfc_worker_releases_transport_arena_on_listener_alloc_failure(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_nfc_listener_alloc_result = false;

    expect(zf_transport_nfc_worker(&app) == 0, "NFC worker should exit cleanly on listener OOM");
    expect(app.transport_arena == NULL, "NFC listener failure should release the transport arena");
    expect(app.transport_arena_size == 0,
           "NFC listener failure should clear the transport arena size");
    expect(strcmp(g_last_status_text, "NFC listener failed") == 0,
           "NFC listener failure should publish a status message");
}

static void test_nfc_worker_releases_transport_arena_on_resource_alloc_failure(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_nfc_alloc_result = false;

    expect(zf_transport_nfc_worker(&app) == 0, "NFC worker should exit cleanly on resource OOM");
    expect(app.transport_arena == NULL, "NFC resource failure should release the transport arena");
    expect(app.transport_arena_size == 0,
           "NFC resource failure should clear the transport arena size");
    expect(strcmp(g_last_status_text, "NFC init failed") == 0,
           "NFC resource failure should publish a status message");
}

static void test_transport_worker_processes_disconnect_when_coalesced_with_interaction(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_cancel_pending_interaction_result = true;
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    g_thread_flag_results[0] = ZF_WORKER_EVT_APPROVAL | ZF_WORKER_EVT_DISCONNECT;
    g_thread_flag_results[1] = ZF_WORKER_EVT_STOP;
    g_thread_flag_result_count = 2;

    expect(zf_transport_usb_hid_worker(&app) == 0, "worker should exit cleanly in native harness");
    expect(g_cancel_pending_interaction_count >= 1,
           "disconnect work should not be dropped when interaction shares the wakeup");
    expect(g_transport_connected_set_count >= 1,
           "disconnect work should still update transport connection state");
    expect(!g_last_transport_connected, "disconnect work should mark transport as disconnected");
}

static void test_wait_for_interaction_processes_disconnect_when_coalesced_with_completion(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    FuriSemaphore done = {0};
    bool approved = false;

    test_reset();
    app.transport_state = &transport;
    app.approval.done = &done;
    app.approval.state = ZfApprovalApproved;
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    g_cancel_pending_interaction_result = true;
    g_semaphore_results[0] = 1;
    g_semaphore_results[1] = FuriStatusOk;
    g_semaphore_result_count = 2;
    g_thread_flag_results[0] = ZF_WORKER_EVT_APPROVAL | ZF_WORKER_EVT_DISCONNECT;
    g_thread_flag_result_count = 1;

    expect(zf_transport_usb_hid_wait_for_interaction(&app, 0x01020304, &approved),
           "interaction wait should still complete in native harness");
    expect(approved, "interaction wait should preserve the approved state");
    expect(g_cancel_pending_interaction_count == 1,
           "interaction wakeups should still drain disconnect work while waiting");
    expect(g_transport_connected_set_count == 1,
           "interaction wait should still update transport state for disconnects");
    expect(!g_last_transport_connected, "interaction wait should mark transport as disconnected");
}

static void test_wait_for_interaction_sends_immediate_keepalive(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    FuriSemaphore done = {0};
    bool approved = false;
    const uint32_t cid = 0x01020304;

    test_reset();
    app.transport_state = &transport;
    app.approval.done = &done;
    app.approval.state = ZfApprovalApproved;
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    transport.cmd = ZF_CTAPHID_CBOR;
    g_semaphore_results[0] = 1;
    g_semaphore_results[1] = FuriStatusOk;
    g_semaphore_result_count = 2;

    expect(zf_transport_usb_hid_wait_for_interaction(&app, cid, &approved),
           "interaction wait should complete after immediate keepalive");
    expect(approved, "interaction wait should preserve approved result after keepalive");
    expect(g_last_hid_response_len == ZF_CTAPHID_PACKET_SIZE,
           "immediate keepalive should emit one HID frame");
    expect(memcmp(g_last_hid_response, &cid, sizeof(cid)) == 0,
           "keepalive frame should target the active CID");
    expect(g_last_hid_response[4] == ZF_CTAPHID_KEEPALIVE,
           "interaction wait should emit CTAPHID_KEEPALIVE immediately");
    expect(g_last_hid_response[5] == 0 && g_last_hid_response[6] == 1,
           "keepalive frame should advertise a one-byte payload");
    expect(g_last_hid_response[7] == ZF_KEEPALIVE_UPNEEDED,
           "interaction wait should use STATUS_UPNEEDED for touch prompts");
    expect(g_last_thread_wait_timeout == ZF_KEEPALIVE_INTERVAL_MS,
           "interaction wait should sleep only until the next keepalive interval");
}

static void test_wait_for_interaction_repeats_keepalive_on_timeout(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    FuriSemaphore done = {0};
    bool approved = false;
    const uint32_t cid = 0x01020304U;

    test_reset();
    app.transport_state = &transport;
    app.approval.done = &done;
    app.approval.state = ZfApprovalApproved;
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    transport.cmd = ZF_CTAPHID_CBOR;
    g_semaphore_results[0] = 1;
    g_semaphore_results[1] = FuriStatusOk;
    g_semaphore_result_count = 2;

    expect(zf_transport_usb_hid_wait_for_interaction(&app, cid, &approved),
           "interaction wait should complete after a timeout-driven keepalive retry");
    expect(approved, "interaction wait should preserve approved result after timeout retry");
    expect(g_hid_response_count == 2,
           "timeout-driven keepalive should emit an immediate and a repeated keepalive");
    expect(g_last_thread_wait_timeout == ZF_KEEPALIVE_INTERVAL_MS,
           "interaction wait should use the keepalive interval as its timeout");
}

static void test_wait_for_interaction_omits_keepalive_for_u2f_msg(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    FuriSemaphore done = {0};
    bool approved = false;

    test_reset();
    app.transport_state = &transport;
    app.approval.done = &done;
    app.approval.state = ZfApprovalApproved;
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    transport.cmd = ZF_CTAPHID_MSG;
    g_semaphore_results[0] = 1;
    g_semaphore_results[1] = FuriStatusOk;
    g_semaphore_result_count = 2;

    expect(zf_transport_usb_hid_wait_for_interaction(&app, 0x01020304U, &approved),
           "U2F MSG interaction wait should complete without keepalive");
    expect(approved, "U2F MSG interaction wait should preserve approved result");
    expect(g_hid_response_count == 0,
           "U2F MSG approval wait should not interleave CTAPHID_KEEPALIVE frames");
}

static void test_transport_fragment_expires_on_session_tick(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    const uint32_t cid = 0x01020304U;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid),
           "seed CID before fragmented PING for timeout handling");

    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_PING;
    packet[5] = 0x00;
    packet[6] = 0x3C;
    memset(&packet[7], 0xAA, sizeof(packet) - 7);

    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(transport.active, "fragmented PING should remain active until continuation arrives");
    g_fake_tick = transport.last_activity + ZF_ASSEMBLY_TIMEOUT_MS;
    zf_transport_session_tick(&transport, g_fake_tick);
    expect(!transport.active, "session tick should expire fragmented assemblies");
    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "expired assembly should emit a HID error response");
    expect(g_last_hid_response[7] == ZF_HID_ERR_MSG_TIMEOUT,
           "expired assembly should report MSG_TIMEOUT");
}

static void test_transport_request_wakeup_processes_one_complete_transaction(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    static const uint8_t nonce_a[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    static const uint8_t nonce_b[8] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;

    test_reset();

    memcpy(g_hid_request_packets[0], &broadcast_cid, sizeof(broadcast_cid));
    g_hid_request_packets[0][4] = ZF_CTAPHID_INIT;
    g_hid_request_packets[0][5] = 0x00;
    g_hid_request_packets[0][6] = 0x08;
    memcpy(&g_hid_request_packets[0][7], nonce_a, sizeof(nonce_a));
    g_hid_request_packet_lens[0] = 15;

    memcpy(g_hid_request_packets[1], &broadcast_cid, sizeof(broadcast_cid));
    g_hid_request_packets[1][4] = ZF_CTAPHID_INIT;
    g_hid_request_packets[1][5] = 0x00;
    g_hid_request_packets[1][6] = 0x08;
    memcpy(&g_hid_request_packets[1][7], nonce_b, sizeof(nonce_b));
    g_hid_request_packet_lens[1] = 15;

    g_hid_request_count = 2;

    zf_transport_handle_request(&app, &transport, ZF_WORKER_EVT_REQUEST, packet);

    expect(g_hid_request_index == 1,
           "request wakeup should leave later complete transactions for a later worker pass");
    expect(g_hid_response_count == 1, "one worker pass should produce one complete HID response");
    expect(g_last_hid_response[4] == ZF_CTAPHID_INIT,
           "the first queued packet should complete normally");
    expect(memcmp(&g_last_hid_response[7], nonce_a, sizeof(nonce_a)) == 0,
           "the first INIT response should echo the first queued request nonce");
    expect(g_delay_ms_count == 0, "INIT responses should not use the U2F MSG response settle");
}

static void test_transport_request_without_wakeup_marks_connected_and_processes_packet(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    static const uint8_t nonce[8] = {0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A};
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;

    test_reset();

    memcpy(g_hid_request_packets[0], &broadcast_cid, sizeof(broadcast_cid));
    g_hid_request_packets[0][4] = ZF_CTAPHID_INIT;
    g_hid_request_packets[0][5] = 0x00;
    g_hid_request_packets[0][6] = 0x08;
    memcpy(&g_hid_request_packets[0][7], nonce, sizeof(nonce));
    g_hid_request_packet_lens[0] = 15;
    g_hid_request_count = 1;

    zf_transport_handle_request(&app, &transport, 0, packet);

    expect(g_transport_connected_set_count == 1,
           "observed HID traffic should mark transport connected even without a request event");
    expect(g_last_transport_connected,
           "observed HID traffic should switch the UI state to connected");
    expect(g_hid_response_count == 1,
           "observed HID traffic should still be processed without a request event");
    expect(g_last_hid_response[4] == ZF_CTAPHID_INIT,
           "fallback request processing should complete the INIT transaction");
    expect(memcmp(&g_last_hid_response[7], nonce, sizeof(nonce)) == 0,
           "fallback request processing should echo the INIT nonce");
}

static void test_transport_worker_idle_poll_processes_immediate_u2f_invalid_cla(void) {
    ZerofidoApp app = {0};
    static const uint8_t nonce[8] = {0x1B, 0x61, 0x8C, 0x6B, 0xC8, 0xC7, 0x07, 0x42};
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;
    const uint32_t cid = 0x9DD5F6E9U;
    const uint8_t request[] = {0x6D, U2F_CMD_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00};

    test_reset();
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    g_random_values[0] = cid;
    g_random_count = 1;

    memcpy(g_hid_request_packets[0], &broadcast_cid, sizeof(broadcast_cid));
    g_hid_request_packets[0][4] = ZF_CTAPHID_INIT;
    g_hid_request_packets[0][5] = 0x00;
    g_hid_request_packets[0][6] = 0x08;
    memcpy(&g_hid_request_packets[0][7], nonce, sizeof(nonce));
    g_hid_request_packet_lens[0] = 15;

    memcpy(g_hid_request_packets[1], &cid, sizeof(cid));
    g_hid_request_packets[1][4] = ZF_CTAPHID_MSG;
    g_hid_request_packets[1][5] = 0x00;
    g_hid_request_packets[1][6] = sizeof(request);
    memcpy(&g_hid_request_packets[1][7], request, sizeof(request));
    g_hid_request_packet_lens[1] = 7 + sizeof(request);
    g_hid_request_count = 2;

    g_thread_flag_results[0] = FuriFlagErrorTimeout;
    g_thread_flag_results[1] = FuriFlagErrorTimeout;
    g_thread_flag_results[2] = ZF_WORKER_EVT_STOP;
    g_thread_flag_result_count = 3;

    expect(zf_transport_usb_hid_worker(&app) == 0, "worker should exit cleanly in native harness");
    expect(g_hid_request_index == g_hid_request_count,
           "idle polls should drain the queued INIT and immediate U2F MSG over separate passes");
    expect(g_hid_response_count == 2,
           "idle poll should answer both the INIT and invalid-CLA U2F MSG");
    expect(g_last_hid_response_len == ZF_CTAPHID_PACKET_SIZE,
           "invalid-CLA U2F MSG should emit a HID response frame");
    expect(memcmp(g_last_hid_response, &cid, sizeof(cid)) == 0,
           "invalid-CLA U2F MSG should reply on the assigned CID");
    expect(g_last_hid_response[4] == ZF_CTAPHID_MSG,
           "invalid-CLA U2F APDU should respond on CTAPHID_MSG");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x02,
           "invalid-CLA U2F APDU should return a two-byte status word");
    expect(g_last_hid_response[7] == 0x6E && g_last_hid_response[8] == 0x00,
           "invalid-CLA U2F APDU should return SW_CLA_NOT_SUPPORTED");
    expect(g_delay_ms_count == 1 && g_last_delay_ms == 50U,
           "U2F MSG responses should keep a USB turn-around settle before sending");
    expect(g_last_thread_wait_timeout == ZF_WORKER_POLL_MS,
           "worker should use a bounded poll timeout while idle");
}

static void test_transport_worker_idle_poll_processes_immediate_u2f_version_data(void) {
    ZerofidoApp app = {0};
    static const uint8_t nonce[8] = {0x36, 0xBB, 0x45, 0xBF, 0x98, 0xB6, 0x3C, 0xF0};
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;
    const uint32_t cid = 0xF0EE7690U;
    const uint8_t request[] = {
        0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x22, 0x59, 0x38, 0xE9, 0x9C, 0xC9, 0x69, 0x5E,
        0x75, 0x61, 0x77, 0xE6, 0x71, 0x37, 0xB5, 0x88, 0xD2, 0xE5, 0x4A, 0x5E, 0xCA, 0xEF,
        0x4A, 0x29, 0x1E, 0x7B, 0xA1, 0x11, 0x5D, 0x0C, 0xA5, 0xE2, 0x70, 0xAD, 0x92,
    };

    test_reset();
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    g_random_values[0] = cid;
    g_random_count = 1;

    memcpy(g_hid_request_packets[0], &broadcast_cid, sizeof(broadcast_cid));
    g_hid_request_packets[0][4] = ZF_CTAPHID_INIT;
    g_hid_request_packets[0][5] = 0x00;
    g_hid_request_packets[0][6] = 0x08;
    memcpy(&g_hid_request_packets[0][7], nonce, sizeof(nonce));
    g_hid_request_packet_lens[0] = 15;

    memcpy(g_hid_request_packets[1], &cid, sizeof(cid));
    g_hid_request_packets[1][4] = ZF_CTAPHID_MSG;
    g_hid_request_packets[1][5] = 0x00;
    g_hid_request_packets[1][6] = sizeof(request);
    memcpy(&g_hid_request_packets[1][7], request, sizeof(request));
    g_hid_request_packet_lens[1] = 7 + sizeof(request);
    g_hid_request_count = 2;

    g_thread_flag_results[0] = FuriFlagErrorTimeout;
    g_thread_flag_results[1] = FuriFlagErrorTimeout;
    g_thread_flag_results[2] = ZF_WORKER_EVT_STOP;
    g_thread_flag_result_count = 3;

    expect(zf_transport_usb_hid_worker(&app) == 0, "worker should exit cleanly in native harness");
    expect(g_hid_request_index == g_hid_request_count,
           "idle polls should drain the queued INIT and U2F VERSION-with-data MSG");
    expect(g_hid_response_count == 2,
           "idle poll should answer both the INIT and VERSION-with-data U2F MSG");
    expect(g_last_hid_response_len == ZF_CTAPHID_PACKET_SIZE,
           "VERSION-with-data U2F MSG should emit a HID response frame");
    expect(memcmp(g_last_hid_response, &cid, sizeof(cid)) == 0,
           "VERSION-with-data U2F MSG should reply on the assigned CID");
    expect(g_last_hid_response[4] == ZF_CTAPHID_MSG,
           "VERSION-with-data U2F APDU should respond on CTAPHID_MSG");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x02,
           "VERSION-with-data U2F APDU should return a two-byte status word");
    expect(g_last_hid_response[7] == 0x67 && g_last_hid_response[8] == 0x00,
           "VERSION-with-data U2F APDU should return SW_WRONG_LENGTH");
}

static void test_transport_request_wakeup_invalid_seq_allows_followup_init(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    static const uint8_t nonce[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    const uint32_t cid = 0x01020304U;
    const uint32_t other_cid = 0x0A0B0C0DU;
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid), "seed CID before queued fragmented PING");
    expect(zf_transport_remember_cid(&transport, other_cid),
           "seed second CID before queued fragmented PING");
    g_random_values[0] = 0x55667788U;
    g_random_count = 1;

    memcpy(g_hid_request_packets[0], &cid, sizeof(cid));
    g_hid_request_packets[0][4] = ZF_CTAPHID_PING;
    g_hid_request_packets[0][5] = 0x04;
    g_hid_request_packets[0][6] = 0x00;
    memset(&g_hid_request_packets[0][7], 0xAA, ZF_CTAPHID_PACKET_SIZE - 7);
    g_hid_request_packet_lens[0] = ZF_CTAPHID_PACKET_SIZE;

    memcpy(g_hid_request_packets[1], &cid, sizeof(cid));
    g_hid_request_packets[1][4] = 0x00;
    memset(&g_hid_request_packets[1][5], 0xBB, ZF_CTAPHID_PACKET_SIZE - 5);
    g_hid_request_packet_lens[1] = ZF_CTAPHID_PACKET_SIZE;

    memcpy(g_hid_request_packets[2], &other_cid, sizeof(other_cid));
    g_hid_request_packets[2][4] = 0x02;
    memset(&g_hid_request_packets[2][5], 0xCC, ZF_CTAPHID_PACKET_SIZE - 5);
    g_hid_request_packet_lens[2] = ZF_CTAPHID_PACKET_SIZE;

    memcpy(g_hid_request_packets[3], &broadcast_cid, sizeof(broadcast_cid));
    g_hid_request_packets[3][4] = ZF_CTAPHID_INIT;
    g_hid_request_packets[3][5] = 0x00;
    g_hid_request_packets[3][6] = 0x08;
    memcpy(&g_hid_request_packets[3][7], nonce, sizeof(nonce));
    g_hid_request_packet_lens[3] = 15;

    g_hid_request_count = 4;

    zf_transport_handle_request(&app, &transport, ZF_WORKER_EVT_REQUEST, packet);

    expect(g_hid_request_index < g_hid_request_count,
           "request wakeup should leave later queued packets for a later worker pass");

    while (g_hid_request_index < g_hid_request_count) {
        zf_transport_handle_request(&app, &transport, ZF_WORKER_EVT_REQUEST, packet);
    }

    expect(g_last_hid_response[4] == ZF_CTAPHID_INIT,
           "follow-up INIT should succeed after queued invalid sequence reset");
    expect(memcmp(&g_last_hid_response[7], nonce, sizeof(nonce)) == 0,
           "follow-up INIT should echo its nonce after queued invalid sequence reset");
}

static void test_transport_same_cid_init_like_invalid_seq_allows_followup_init(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    static const uint8_t nonce[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    const uint32_t cid = 0x01020304U;
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid), "seed CID before fragmented PING");
    g_random_values[0] = 0x55667788U;
    g_random_count = 1;

    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_PING;
    packet[5] = 0x04;
    packet[6] = 0x00;
    memset(&packet[7], 0xAA, sizeof(packet) - 7);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(transport.active, "fragmented PING should leave the assembly active");

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = 0x82;
    packet[5] = 0x00;
    packet[6] = 0x00;
    zf_transport_session_handle_packet(&app, &transport, packet, 7, NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "unexpected init-like packet should return CTAPHID_ERROR");
    expect(g_last_hid_response[7] == ZF_HID_ERR_INVALID_SEQ,
           "unexpected init-like packet on the active CID should return ERR_INVALID_SEQ");
    expect(!transport.active,
           "unexpected init-like packet on the active CID should reset the assembly");

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &broadcast_cid, sizeof(broadcast_cid));
    packet[4] = ZF_CTAPHID_INIT;
    packet[5] = 0x00;
    packet[6] = 0x08;
    memcpy(&packet[7], nonce, sizeof(nonce));
    zf_transport_session_handle_packet(&app, &transport, packet, 15, NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_INIT,
           "follow-up INIT should succeed after unexpected init-like invalid sequence");
    expect(memcmp(&g_last_hid_response[7], nonce, sizeof(nonce)) == 0,
           "follow-up INIT should echo the nonce after unexpected init-like invalid sequence");
}

static void test_transport_out_of_order_continuation_returns_invalid_seq(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t init_packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    uint8_t cont_packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    const uint32_t cid = 0x01020304U;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid), "seed CID before fragmented PING");

    memcpy(init_packet, &cid, sizeof(cid));
    init_packet[4] = ZF_CTAPHID_PING;
    init_packet[5] = 0x00;
    init_packet[6] = 0x3C;
    memset(&init_packet[7], 0xAA, sizeof(init_packet) - 7);
    zf_transport_session_handle_packet(&app, &transport, init_packet, sizeof(init_packet), NULL);

    expect(transport.active, "fragmented PING should leave transport waiting for continuations");
    expect(transport.next_seq == 0, "first continuation should expect sequence zero");

    memcpy(cont_packet, &cid, sizeof(cid));
    cont_packet[4] = 0x01;
    memset(&cont_packet[5], 0xBB, sizeof(cont_packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, cont_packet, sizeof(cont_packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "out-of-order continuation should return CTAPHID_ERROR");
    expect(g_last_hid_response[7] == ZF_HID_ERR_INVALID_SEQ,
           "out-of-order continuation should return ERR_INVALID_SEQ");
    expect(!transport.active, "out-of-order continuation should reset the in-flight assembly");
}

static void test_transport_large_ping_out_of_order_continuation_returns_invalid_seq(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    const uint32_t cid = 0x01020304U;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid), "seed CID before large fragmented PING");

    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_PING;
    packet[5] = 0x04;
    packet[6] = 0x00;
    memset(&packet[7], 0xAA, sizeof(packet) - 7);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(transport.active,
           "large fragmented PING should leave transport waiting for continuations");
    expect(transport.received_len == 57, "large fragmented PING should capture the init payload");
    expect(transport.next_seq == 0, "large fragmented PING should expect sequence zero first");

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = 0x00;
    memset(&packet[5], 0xBB, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(transport.active, "first large continuation should keep the assembly active");
    expect(transport.received_len == 116, "first large continuation should extend the payload");
    expect(transport.next_seq == 1, "second large continuation should expect sequence one");

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = 0x02;
    memset(&packet[5], 0xCC, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "large out-of-order continuation should return CTAPHID_ERROR");
    expect(g_last_hid_response[7] == ZF_HID_ERR_INVALID_SEQ,
           "large out-of-order continuation should return ERR_INVALID_SEQ");
    expect(!transport.active,
           "large out-of-order continuation should reset the in-flight assembly");
}

static void
test_transport_large_ping_other_cid_out_of_order_continuation_returns_invalid_seq(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    const uint32_t cid = 0x01020304U;
    const uint32_t other_cid = 0x0A0B0C0DU;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid), "seed CID before large fragmented PING");
    expect(zf_transport_remember_cid(&transport, other_cid),
           "seed second CID before large fragmented PING");

    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_PING;
    packet[5] = 0x04;
    packet[6] = 0x00;
    memset(&packet[7], 0xAA, sizeof(packet) - 7);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = 0x00;
    memset(&packet[5], 0xBB, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(transport.active, "first large continuation should keep the assembly active");
    expect(transport.next_seq == 1, "second large continuation should expect sequence one");

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &other_cid, sizeof(other_cid));
    packet[4] = 0x02;
    memset(&packet[5], 0xCC, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "other-CID out-of-order continuation should return CTAPHID_ERROR");
    expect(g_last_hid_response[7] == ZF_HID_ERR_INVALID_SEQ,
           "other-CID out-of-order continuation should prioritize ERR_INVALID_SEQ");
    expect(!transport.active,
           "other-CID out-of-order continuation should reset the active assembly");
}

static void test_transport_large_ping_other_cid_invalid_seq_allows_followup_init(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    static const uint8_t nonce[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    const uint32_t cid = 0x01020304U;
    const uint32_t other_cid = 0x0A0B0C0DU;
    const uint32_t broadcast_cid = ZF_BROADCAST_CID;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid), "seed CID before fragmented PING");
    expect(zf_transport_remember_cid(&transport, other_cid),
           "seed second CID before fragmented PING");
    g_random_values[0] = 0x55667788U;
    g_random_count = 1;

    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_PING;
    packet[5] = 0x04;
    packet[6] = 0x00;
    memset(&packet[7], 0xAA, sizeof(packet) - 7);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = 0x00;
    memset(&packet[5], 0xBB, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &other_cid, sizeof(other_cid));
    packet[4] = 0x02;
    memset(&packet[5], 0xCC, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(!transport.active, "invalid sequence should clear the active assembly");

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &broadcast_cid, sizeof(broadcast_cid));
    packet[4] = ZF_CTAPHID_INIT;
    packet[5] = 0x00;
    packet[6] = 0x08;
    memcpy(&packet[7], nonce, sizeof(nonce));
    zf_transport_session_handle_packet(&app, &transport, packet, 15, NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_INIT,
           "broadcast INIT should succeed after invalid sequence reset");
    expect(memcmp(&g_last_hid_response[7], nonce, sizeof(nonce)) == 0,
           "broadcast INIT should echo the request nonce after invalid sequence reset");
}

static void test_transport_processing_same_cid_continuation_returns_invalid_seq(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    const uint32_t cid = 0x01020304U;

    test_reset();
    transport.processing = true;
    transport.cid = cid;
    transport.cmd = ZF_CTAPHID_PING;

    memcpy(packet, &cid, sizeof(cid));
    packet[4] = 0x00;
    memset(&packet[5], 0xDD, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "same-CID continuation during processing should return CTAPHID_ERROR");
    expect(g_last_hid_response[7] == ZF_HID_ERR_INVALID_SEQ,
           "same-CID continuation during processing should return ERR_INVALID_SEQ");
    expect(transport.processing,
           "same-CID continuation during processing should not drop the active transaction");
}

static void test_transport_processing_other_cid_continuation_returns_invalid_seq(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE] = {0};
    const uint32_t cid = 0x01020304U;
    const uint32_t other_cid = 0x0A0B0C0DU;

    test_reset();
    transport.processing = true;
    transport.cid = cid;
    transport.cmd = ZF_CTAPHID_PING;

    memcpy(packet, &other_cid, sizeof(other_cid));
    packet[4] = 0x00;
    memset(&packet[5], 0xEE, sizeof(packet) - 5);
    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "other-CID continuation during processing should return CTAPHID_ERROR");
    expect(g_last_hid_response[7] == ZF_HID_ERR_INVALID_SEQ,
           "other-CID continuation during processing should prioritize ERR_INVALID_SEQ");
    expect(transport.processing,
           "other-CID continuation during processing should not drop the active transaction");
}

static void test_transport_lock_zero_returns_success(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t packet[8] = {0};
    const uint32_t cid = 0x01020304U;

    test_reset();
    expect(zf_transport_remember_cid(&transport, cid), "seed CID for lock request");

    memcpy(packet, &cid, sizeof(cid));
    packet[4] = ZF_CTAPHID_LOCK;
    packet[5] = 0x00;
    packet[6] = 0x01;
    packet[7] = 0x00;

    zf_transport_session_handle_packet(&app, &transport, packet, sizeof(packet), NULL);

    expect(g_last_hid_response_len == sizeof(g_last_hid_response),
           "lock response should emit one HID frame");
    expect(memcmp(g_last_hid_response, &cid, sizeof(cid)) == 0,
           "lock response should target the request CID");
    expect(g_last_hid_response[4] == ZF_CTAPHID_LOCK,
           "lock request should reply with CTAPHID_LOCK");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x00,
           "lock response should have an empty payload");
}

static void test_transport_lock_blocks_other_cid_until_released(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    uint8_t lock_packet[8] = {0};
    uint8_t init_packet[15] = {0};
    static const uint8_t nonce[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    const uint32_t lock_cid = 0x01020304U;
    const uint32_t other_cid = 0x11121314U;
    uint32_t echoed_new_cid = 0;

    test_reset();
    expect(zf_transport_remember_cid(&transport, lock_cid), "seed lock owner CID");
    expect(zf_transport_remember_cid(&transport, other_cid), "seed second CID");

    memcpy(lock_packet, &lock_cid, sizeof(lock_cid));
    lock_packet[4] = ZF_CTAPHID_LOCK;
    lock_packet[5] = 0x00;
    lock_packet[6] = 0x01;
    lock_packet[7] = 0x08;
    zf_transport_session_handle_packet(&app, &transport, lock_packet, sizeof(lock_packet), NULL);
    expect(g_last_hid_response[4] == ZF_CTAPHID_LOCK, "lock acquisition should succeed");

    memcpy(init_packet, &other_cid, sizeof(other_cid));
    init_packet[4] = ZF_CTAPHID_INIT;
    init_packet[5] = 0x00;
    init_packet[6] = 0x08;
    memcpy(&init_packet[7], nonce, sizeof(nonce));
    zf_transport_session_handle_packet(&app, &transport, init_packet, sizeof(init_packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_ERROR,
           "INIT on another CID should fail while the device is locked");
    expect(g_last_hid_response[7] == ZF_HID_ERR_CHANNEL_BUSY,
           "locked transport should report channel busy to other CIDs");

    lock_packet[7] = 0x00;
    zf_transport_session_handle_packet(&app, &transport, lock_packet, sizeof(lock_packet), NULL);
    expect(g_last_hid_response[4] == ZF_CTAPHID_LOCK, "lock release should succeed");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x00,
           "lock release response should have an empty payload");

    zf_transport_session_handle_packet(&app, &transport, init_packet, sizeof(init_packet), NULL);

    expect(g_last_hid_response[4] == ZF_CTAPHID_INIT,
           "INIT on the other CID should succeed after releasing the lock");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 17,
           "INIT success should advertise the standard 17-byte payload");
    expect(memcmp(&g_last_hid_response[7], nonce, sizeof(nonce)) == 0,
           "INIT success should echo the request nonce");
    memcpy(&echoed_new_cid, &g_last_hid_response[15], sizeof(echoed_new_cid));
    expect(echoed_new_cid == other_cid,
           "INIT on an allocated CID should report that CID as the active channel");
    expect(g_last_hid_response[19] == 2, "INIT success should report CTAPHID interface version 2");
}

static void test_transport_disconnect_keeps_allocated_cids(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};

    test_reset();
    expect(zf_transport_remember_cid(&transport, 0x01020304U),
           "seed an allocated CID before disconnect");

    zf_transport_handle_worker_flags(&app, &transport, ZF_WORKER_EVT_DISCONNECT);

    expect(zf_transport_cid_is_allocated(&transport, 0x01020304U),
           "disconnect handling should not forget allocated CIDs");
}

static void test_transport_connect_keeps_allocated_cids(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};

    test_reset();
    expect(zf_transport_remember_cid(&transport, 0x01020304U),
           "seed an allocated CID before connect");

    zf_transport_handle_worker_flags(&app, &transport, ZF_WORKER_EVT_CONNECT);

    expect(zf_transport_cid_is_allocated(&transport, 0x01020304U),
           "connect handling should not forget allocated CIDs");
}

static void test_transport_worker_syncs_initial_hal_connection_state(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_hid_connected = true;
    g_thread_flag_results[0] = ZF_WORKER_EVT_STOP;
    g_thread_flag_result_count = 1;

    expect(zf_transport_usb_hid_worker(&app) == 0, "worker should exit cleanly in native harness");
    expect(g_transport_connected_true_count >= 1,
           "worker should publish an initial connected state when the HAL is already connected");
}

static void test_transport_dispatch_cbor_preserves_response_buffer(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    const uint32_t cid = 0x01020304U;
    const uint8_t request[] = {ZfCtapeCmdGetInfo};

    test_reset();
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    zf_transport_dispatch_complete_message(&app, &transport, cid, ZfTransportProtocolKindCtap2,
                                           request, sizeof(request));

    expect(g_last_hid_response_len == ZF_CTAPHID_PACKET_SIZE,
           "CBOR dispatch should emit a HID response frame");
    expect(g_last_hid_response[4] == ZF_CTAPHID_CBOR,
           "GetInfo should respond on the CBOR command channel");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x02,
           "GetInfo should return the stub CTAP payload length");
    expect(g_last_hid_response[7] == ZF_CTAP_SUCCESS,
           "GetInfo should return a CTAP success status byte");
}

static void test_transport_dispatch_u2f_version_with_data_returns_wrong_length(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    const uint32_t cid = 0x01020304U;
    uint8_t request[5 + 64] = {0};

    test_reset();
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_u2f_adapter_init(&app), "U2F init should succeed before transport MSG test");

    request[0] = 0x00;
    request[1] = U2F_CMD_VERSION;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = 64;
    memset(&request[5], 0xA5, 64);

    zf_transport_dispatch_complete_message(&app, &transport, cid, ZfTransportProtocolKindU2f,
                                           request, sizeof(request));

    expect(g_last_hid_response_len == ZF_CTAPHID_PACKET_SIZE,
           "invalid VERSION APDU should emit a HID response frame");
    expect(memcmp(g_last_hid_response, &cid, sizeof(cid)) == 0,
           "invalid VERSION APDU should reply on the request CID");
    expect(g_last_hid_response[4] == ZF_CTAPHID_MSG,
           "invalid VERSION APDU should still respond on CTAPHID_MSG");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x02,
           "invalid VERSION APDU should return a two-byte status word");
    expect(g_last_hid_response[7] == 0x67 && g_last_hid_response[8] == 0x00,
           "invalid VERSION APDU should return SW_WRONG_LENGTH");

    zf_u2f_adapter_deinit(&app);
}

static void
test_transport_dispatch_u2f_version_extended_length_with_data_returns_wrong_length(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    const uint32_t cid = 0x01020304U;
    uint8_t request[] = {
        0x00, 0x03, 0x00, 0x00, 0x00, 0x28, 0x2c, 0x7c, 0xd7, 0xb3, 0x5e, 0x16,
        0xfa, 0x3c, 0xc2, 0x57, 0xb0, 0x5e, 0xaf, 0xa7, 0xc7, 0xcc, 0x77, 0x92,
        0xf1, 0xef, 0x37, 0xe3, 0x1b, 0x1e, 0x4c, 0x77, 0x38, 0xac, 0x41, 0x08,
        0x44, 0x3b, 0x4a, 0x46, 0x72, 0x2d, 0xee, 0x2b, 0xea, 0x32, 0x00, 0x00,
    };

    test_reset();
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    expect(zf_u2f_adapter_init(&app),
           "transport extended-length invalid VERSION test should initialize U2F");

    zf_transport_dispatch_complete_message(&app, &transport, cid, ZfTransportProtocolKindU2f,
                                           request, sizeof(request));

    expect(g_last_hid_response_len == ZF_CTAPHID_PACKET_SIZE,
           "extended-length invalid VERSION APDU should emit a HID response frame");
    expect(memcmp(g_last_hid_response, &cid, sizeof(cid)) == 0,
           "extended-length invalid VERSION APDU should reply on the request CID");
    expect(g_last_hid_response[4] == ZF_CTAPHID_MSG,
           "extended-length invalid VERSION APDU should still respond on CTAPHID_MSG");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x02,
           "extended-length invalid VERSION APDU should return a two-byte status word");
    expect(g_last_hid_response[7] == 0x67 && g_last_hid_response[8] == 0x00,
           "extended-length invalid VERSION APDU should return SW_WRONG_LENGTH");

    zf_u2f_adapter_deinit(&app);
}

static void test_transport_dispatch_u2f_version_without_live_backend_returns_version(void) {
    ZerofidoApp app = {0};
    ZfTransportState transport = {0};
    const uint32_t cid = 0x01020304U;
    uint8_t request[5] = {0};

    test_reset();
    app.transport_adapter = &zf_transport_usb_hid_adapter;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    request[0] = 0x00;
    request[1] = U2F_CMD_VERSION;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = 0x00;

    zf_transport_dispatch_complete_message(&app, &transport, cid, ZfTransportProtocolKindU2f,
                                           request, sizeof(request));

    expect(g_last_hid_response_len == ZF_CTAPHID_PACKET_SIZE,
           "VERSION APDU without live backend should emit a HID response frame");
    expect(memcmp(g_last_hid_response, &cid, sizeof(cid)) == 0,
           "VERSION APDU without live backend should reply on the request CID");
    expect(g_last_hid_response[4] == ZF_CTAPHID_MSG,
           "VERSION APDU without live backend should still respond on CTAPHID_MSG");
    expect(g_last_hid_response[5] == 0x00 && g_last_hid_response[6] == 0x08,
           "VERSION APDU without live backend should return an 8-byte payload");
    expect(memcmp(&g_last_hid_response[7], "U2F_V2", 6) == 0,
           "VERSION APDU without live backend should return U2F_V2");
    expect(g_last_hid_response[13] == 0x90 && g_last_hid_response[14] == 0x00,
           "VERSION APDU without live backend should return SW_NO_ERROR");
}
