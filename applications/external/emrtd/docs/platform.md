# Platform findings (verified empirically, 2026-09-20)

1. FAP + `fap_libs=["mbedtls"]` links `mbedtls_ecp_mul`, `mbedtls_ecp_muladd`,
   `mbedtls_ecp_point_read/write_binary`, `mbedtls_ecp_check_pubkey`, the whole
   `mbedtls_mpi_*` public API, AES, DES/3DES, SHA-1, SHA-256. APPCHK passes.
2. `mbedtls_mpi_exp_mod` / `mbedtls_mpi_core_exp_mod` CANNOT be used: they pull
   `mbedtls_mpi_core_montmul` -> `mbedtls_ct_memcpy_if`, which is undefined in
   libmbedtls.a and absent from the firmware API table. APPCHK fails the build.
   => PACE with DH over MODP groups (param ids 0..2) is impossible. ECDH only.
   => RSA is impossible anyway (MBEDTLS_RSA_C off).
3. Brainpool curves are NOT compiled into the shipped mbedtls, but a hand
   populated `mbedtls_ecp_group` works. Verified against ICAO 9303-11 appendix G
   (generic mapping + key agreement on brainpoolP256r1).
   - `grp->id = MBEDTLS_ECP_DP_NONE`, `grp->modp = NULL` (generic reduction).
   - cofactor `h` left at 0 so `mbedtls_ecp_group_free()` releases the MPIs;
     `h` takes no part in the arithmetic.
   - the generator must be seeded with `mbedtls_ecp_set_zero()` first, because
     `mbedtls_ecp_point_read_binary()` asks the group for its curve type and
     that answer is read off the generator.
4. `MBEDTLS_ECP_MAX_BITS` is 256 with the firmware config (only secp256r1
   enabled). `COMB_MAX_D` sizes a stack array in `ecp_mul_comb()`, so curves
   above 256 bits must be refused. Covers ids 8..13; ids 14..18 are listed but
   rejected with a named error.
5. AES-CMAC is absent from the firmware mbedtls (no `MBEDTLS_CMAC_C`).
   Implemented on `mbedtls_aes_crypt_ecb`, verified against RFC 4493 and
   NIST SP 800-38B D.2/D.3.
6. APPCHK does validate FAP imports and fails the build on an unresolvable one.
7. SDK: ufbt 0.2.6, firmware 1.4.3, API 87.1, mbedtls 3.6.2.
8. `mbedtls_ecp_gen_privkey` links cleanly in a FAP (verified with APPCHK): it does not
   reach `exp_mod`. It is therefore the right way to draw a PACE ephemeral scalar, because
   it produces a value in the correct range for the group without a reject loop.
9. FAP size for a minimal app pulling in ECP + bignum + AES + DES: about 21 KB. A FAP is
   loaded into RAM, so this counts against the heap the read itself needs.

## Radio timing (verified empirically, 2026-09-21)

10. The firmware's ISO 14443-4A poller gives a card a frame waiting time of
    **1620 carrier cycles - 120 microseconds** - for every I-block whenever the
    card's ATS carries no TB1. `iso14443_4a_get_fwt_fc_max()` starts from
    `ISO14443_4A_FDT_DEFAULT_FC`, which is `#define`d to
    `ISO14443_3A_FDT_POLL_FC` (1620), and replaces it only when `tl > 1`,
    `t0 & TB1` and `fwi != 0x0F` all hold. 1620 cycles is the ISO 14443-3 poll
    frame delay time, not a frame waiting time; it is forty times below the
    ISO 14443-4 default and no smart card can answer anything inside it.
    Source: `lib/nfc/protocols/iso14443_4a/iso14443_4a.c:14,240-256` and
    `lib/nfc/protocols/iso14443_3a/iso14443_3a.h:16`, identical in the official
    `release` tree and in Unleashed.
11. The same poller allows **40000 cycles (2.95 ms)** for the answer to RATS,
    fixed, at `iso14443_4a_poller_i.c:41` with the constant at
    `iso14443_4a_poller_i.h:13`. ISO/IEC 14443-4 allows a card 65536 cycles
    before FWI is known, so a fully conformant chip can lose this race.
12. Neither value is reachable from an application: `iso14443_4a_poller_send_block()`
    takes no timeout argument. `iso14443_3a_poller_send_standard_frame()` does,
    and it is exported and enabled in both API tables (87.1 and 88.9). Running
    the block transmission protocol on top of it - RATS, block numbering,
    chaining both ways, S(WTX), retransmission - is therefore the only way to
    choose the waiting time, and it is what `transport/emrtd_isodep.c` does.
    The Python reference implementation reached the same conclusion
    independently and asks the Flipper's `raw` command for 4000000 cycles
    (295 ms) on every frame.
13. Owning that layer also removes a difference between the two firmwares.
    Unleashed rewrote `iso14443_4a_poller_send_block()` to use
    `iso14443_4_layer_encode_command()` / `decode_response()` and added
    `iso14443_4a_poller_send_block_pwt_ext()`; the official tree still uses
    `encode_block()` / `decode_block()`. Nothing above the transport now
    depends on which of the two is installed.
14. The type B poller is not affected: `iso14443_4b_poller_send_block()` gets
    its waiting time from `iso14443_3b_get_fwt_fc_max()`, which reads the ATQB
    protocol info. That path still uses the firmware's own implementation.

## Memory (verified empirically, 2026-09-21)

15. **The heap is 190,144 bytes**, not the hundred kilobytes this project used
    to say. `__heap_start__` is `0x20001540` and `__heap_end__` is
    `0x2002fc00`; the pair is in the literal pool of
    `__furi_crash_implementation` at `0x08012980`, which prints the difference
    as the total, and a crash dump from a real device confirms it
    (`heap total: 190144`). The 1,024 bytes up to `_stack_end` at `0x20030000`
    are the main stack, not heap.
16. **The application's own image is half of it.** A FAP is loaded into RAM
    section by section, each as one contiguous block: `.text` 67,888,
    `.rodata` 23,712, `.bss` 3,200, and the rest 48, for 94,848 bytes. The
    `.text` block alone is a 35 per cent contiguous demand on the whole heap.
    Rank any size reduction by what it takes off `.text` first.
17. **`pvPortMalloc` never returns NULL.** Both of its failure exits - not
    enough free in total at `0x801486c`, and no single block large enough at
    `0x801488a` - reach `0x801481a`, which loads `"out of memory"` and falls
    into `__furi_crash_implementation`. Every `if(p != NULL)` after an
    allocation is therefore unreachable on this firmware. A shortage can only
    be handled by asking `memmgr_heap_get_max_free_block()` **before**
    allocating, which is what the read scene and the two large mid-read
    buffers now do.
18. **There are two different "out of memory" screens**, and which one appears
    says where the failure was. `"out of memory"` in lower case is the crash
    above: it prints the crashing thread's name, r0 to r11, LR, the stack
    watermark and the heap figures **to the log UART on pins 13 and 14**, then
    stores the message pointer in an RTC backup register and reboots.
    `"Error: Out of Memory - Not enough RAM to run the app"` with a `Reboot`
    button is the loader dialog: `elf_load_section_data` checks
    `memmgr_heap_get_max_free_block() >= sh_size + 1024` before each section
    and gives up cleanly. The first means the application was running; the
    second means it never started.
19. **`furi_hal_usb_is_locked()` means "an RPC session is open"**, precisely.
    Scanning the whole firmware for callers of `furi_hal_usb_lock`
    (`0x08010490`) finds exactly one: `rpc_cli_command_start_session` at
    `0x0808692a`. So it distinguishes lab.flipper.net, qFlipper or the mobile
    app from a cable that is only charging - which
    `furi_hal_power_is_charging()` cannot. It allocates an event flag and
    blocks on the USB thread, so it belongs on a failure path, not a hot one.
20. **A connected computer costs about 20.2 kB of heap**, in three layers that
    arrive separately: opening the serial port at all starts a CLI shell with
    a 4 kB thread stack (about 5.6 kB); `start_rpc_session` adds a second 4 kB
    command thread, a 3 kB session worker and 51 handler records (about
    11.8 kB); and the screen mirror adds a framebuffer, a protobuf message and
    a 1 kB thread (about 2.9 kB). lab.flipper.net starts the screen mirror by
    itself - its landing page is the Device page, whose `onMounted` calls
    `startScreenStream()` as soon as RPC is up.

## Radio errors (read from the source, 2026-09-24)

21. **A damaged answer is reported as a card that is not there.**
    `iso14443_3a_poller_process_error()` in
    `lib/nfc/protocols/iso14443_3a/iso14443_3a_poller_i.c` of the 1.4.3 tree
    maps `NfcErrorTimeout` to `Iso14443_3aErrorTimeout` and every other
    `NfcError` - an incomplete frame, a data format error, a FIFO overflow - to
    `Iso14443_3aErrorNotPresent`. Only a bad CRC on a frame that arrived whole
    comes back as `Iso14443_3aErrorWrongCrc`. So a chip that answered at the
    edge of the field is named as one that is not present, and this reader,
    which maps a timeout and "not present" alike to "The document moved
    away", can only tell the two apart by the trace's radio codes: `7` is a
    timeout, `1` not present, `6` a bad CRC. Both are recovered from with
    R(NAK) rather than by sending the command again - see
    `EMRTD_ISODEP_RETRIES` in `transport/emrtd_isodep.h` for why.

## The allocator (read from the firmware, 2026-09-24)

22. **Nothing collects garbage and nothing compacts.** The allocator is
    FreeRTOS heap_4 as `furi/core/memmgr_heap.c` extends it (`pvPortMalloc`
    at `0x08014750`, `vPortFree` at `0x08014a4c`). It allocates first fit from
    a free list kept in address order and merges a freed block with any free
    neighbour. A live block never moves. So a heap split by one long-lived
    block stays split until that block is freed, and a block that is never
    freed stays allocated until the device restarts. Closing an application
    does not change that. `loader_do_app_closed` (`loader.c:751-784`) joins the
    thread, passes the FAP to `flipper_application_free`, which frees its
    sections and its thread, and logs `Application stopped. Free heap: %zu` at
    Info. Nothing the application allocated itself is released there.
    `memmgr_alloc_from_pool` hands out blocks that are never freed, and falls
    back to `malloc` once its pool is full; nothing here uses it.
23. **The allocator zeroes a block both when it hands it out and when it gets
    it back.** `pvPortMalloc` ends in `memset(p, 0, size)`
    (`memmgr_heap.c:467`, `0x08014a08`), with no condition. `vPortFree` clears
    the whole payload before the block goes back on the free list
    (`memmgr_heap.c:502`, `0x08014a8e`). The clear on free is
    `configHEAP_CLEAR_MEMORY_ON_FREE`, which `targets/f7/inc/FreeRTOSConfig.h`
    sets to 1 in official 1.4.3 and in both the `release` and the `dev`
    branches of Unleashed and Momentum. All of them carry the same
    `memmgr_heap.c`, byte for byte. Nothing freed on this device carries its
    contents into the next allocation, whoever makes it, and a build with the
    flag off would still hand out nothing but zeroes. A restart is not a free:
    `Reset_Handler` (`0x08011b9c`) zeroes `.bss` and `MB_MEM2` but not the
    heap. So what was live when the device crashed or rebooted stays in RAM
    until it is overwritten, hidden from new allocations only by the clear on
    `malloc`. The allocator cannot clear memory that is still in use either:
    a buffer that is still allocated, or the frames a running thread has
    returned from, which stay on its stack until something overwrites them.
    That is the work `emrtd_secure_wipe()` still does on its own. Before a
    `free()`, the same wipe is only insurance against a build with the flag
    off. See [security.md](security.md).
24. **`realloc` never grows a block in place, and `FuriString` goes through
    it.** `realloc` at `0x0801416c` allocates the new size, copies, and frees
    the old block, so growing a buffer needs both blocks at the same time. It
    copies the *new* size (`memcpy(p, ptr, size)` in `memmgr.c`). A block
    that grows therefore reads past the end of the old one, and until it is
    overwritten, the tail of the new block holds a copy of whatever lay next
    to the old one. Every `furi_string_*` call that makes a string longer
    reaches `m_str1ng_fit2size` (`0x08015a10`). When a string outgrows its
    buffer, that function asks for one and a half times the length now
    needed. A string of up to six characters is held inside the `FuriString`
    itself and allocates nothing. `furi_string_reset` (`0x08015f92`) calls
    `string_clear`, which frees the buffer, so an emptied string does not
    keep its capacity. `furi_string_reserve` allocates exactly what it is
    asked for. A string built from small appends is reallocated each time it
    outgrows its buffer, and each time it briefly holds the old and the new
    buffer together. By this rule, a 4 KB report appended 128 bytes at a time
    takes seven allocations, six of them while the previous one is still
    held. At the worst step it asks for 8,258 bytes at once, and it ends in a
    4,993 byte buffer. That is why `emrtd_scene_saved_detail` reserves what
    the report can reach before it reads.
25. **Heap Trace counts what was not freed; it frees nothing, and in 1.4.3 it
    watches one thread.** Settings -> System -> Heap Trace offers `None` and
    `Main` (`heap_trace_mode_text` is `{"None", "Main"}`). `sysctl heap_track`
    accepts the same two. The usage line it prints on a bad argument lists
    `<none|main|tree|all>`, but the handler takes only the first two. With
    `Main` set, `loader_start_app_thread` calls
    `furi_thread_enable_heap_trace` on the application's thread. When that
    thread ends, after the entry point and the FAP's destructors have run,
    `furi_thread_body` waits 33 ms and logs
    `<name> allocation balance: <bytes>`: at Info when the balance is zero,
    at Error when it is not. It then calls `memmgr_heap_disable_thread_trace`,
    which frees its own bookkeeping (the `free` at `0x0801461a` follows
    `MemmgrHeapAllocDict_array_list_pair_clear`) and none of the blocks it
    counted. The record is kept for the thread that allocates. The balance is
    the sum of the blocks in it, headers included, whose header still says
    allocated when the thread ends (`memmgr_heap.c:224-253`). A block freed
    by another thread therefore does not count, but nothing `EmrtdWorker` or
    the NFC stack's thread allocates is in the record at all.
    `furi_thread_init_common` knows two more modes: `Tree`, where a new thread
    inherits tracing from the thread that creates it, and `All`. Neither the
    menu nor the CLI can select them.
26. **How to check on the device that a read leaks nothing.**
    - **Settings:** in Settings -> System, set Log Level to `Info`, Heap
      Trace to `Main` and Log Device to `USART` (the default). In Settings ->
      Expansion Modules, set Listen UART to `None`.
    - **Log:** read it from the UART: pin 13 (TX) and ground. Unlike a USB
      session, which starts threads of its own (item 20), this allocates
      nothing. Listen UART has to be off because by default it listens on
      pin 14, and a pulse there starts a worker of its own and takes the UART
      from the log.
    - **Cycles:** open the application, read a document, visit every result
      screen and close it. Do this three times.
    - **What to expect:** every close should log `allocation balance: 0` for
      the application's thread. Cycles 2 and 3 should also log the same
      `Application stopped. Free heap`, and that comparison is the only check
      that covers every thread.
    - **Reading the comparison:** compare 2 with 3 rather than 1 with 2, so
      that whatever the firmware allocates once on first use is not counted
      against the application. The figure covers the whole device and another
      service can move it, so a difference counts as a leak only if it comes
      back on every cycle.
    - **Tracing the worker thread:** a throwaway build can call
      `furi_thread_enable_heap_trace(worker->thread)` before
      `furi_thread_start()` in `emrtd_worker_start()`. The function is
      exported to applications, and the thread then logs a balance of its own
      when it ends.
    - **Tracing the NFC stack's thread as well:** such a build can call the
      exported `furi_hal_rtc_set_heap_track_mode(FuriHalRtcHeapTrackModeTree)`
      at the top of `emrtd_app()`, with Heap Trace already at `Main` when the
      application was opened so that its own thread is traced. The mode is
      read when a thread is created, so `EmrtdWorker` and the thread
      `nfc_alloc()` starts both inherit tracing from the application's
      thread. The mode is kept in an RTC register that
      survives a restart, so set it back to `Main` before leaving.
