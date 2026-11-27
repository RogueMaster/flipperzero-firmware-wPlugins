#!/usr/bin/env python3
# ci_check_stack.py — loop-task stack budget check. CI runs it after each build.
#
# Everything the firmware does at run time happens on the Arduino loop task:
# CAN dispatch, the HTTP server, the WebSocket server. v2.16-beta.37 and .38
# overflowed its stack whenever a browser opened the dashboard: loop() ->
# web_dashboard_update() -> WebSockets -> ws_event() -> build_json() ->
# snprintf("%f") needed about 8.8 KB of an 8 KB stack, and nothing noticed
# before release. This adds up the frames along the deepest known call chains
# and fails when one gets too close to the configured stack size.
#
#   pio run -e m5stack-atom
#   python3 ci_check_stack.py .pio/build/m5stack-atom
#
# Inputs:
#   <build_dir>/src/*.su   per-function frame sizes, written by GCC beside each
#                          object (-fstack-usage, build_src_flags in
#                          platformio.ini)
#   .firmware/main.cpp     SET_LOOP_TASK_STACK_SIZE(...); without it the
#                          Arduino default applies
#   .firmware/*            the names of variadic functions, which the .su
#                          files leave out, and the feature markers below
#
# A chain is the sum of its functions' frames plus fixed allowances for the
# library and libc code in between, which has no .su here. A function the table
# names but the .su files do not have fails the check: it was renamed, removed
# or inlined into its caller, and the table has to follow. --allow-missing
# skips such a chain instead; that is for running against an old tag and CI
# never passes it.
#
# The chains only cover the functions they name, so a large frame moved into a
# new helper would drop out of the sums. Any frame of _TRIPWIRE bytes or more
# that is on none of the chains checked for this build fails the check too.
import argparse
import os
import re
import sys

# CONFIG_ARDUINO_LOOP_STACK_SIZE in the pinned Arduino core (ESP32 and S3).
_DEFAULT_STACK = 8192

# Share of the stack a chain may use. The rest has to hold what the sums leave
# out: the coprocessor save area FreeRTOS keeps at the top of the stack (100
# bytes on the ESP32, 320 on the S3), loopTask(), one interrupt frame (192
# bytes) and libc's bigint helpers below _dtoa_r (about 0.1 KB, 0.5 KB when one
# has to malloc). On the 8192-byte stack the old releases had, the dashboard
# connect chain sums to 92% for beta.36, which ran in the field, and to 107%
# for beta.37 and beta.38, which rebooted.
_FAIL_FRACTION = 0.93
_WARN_FRACTION = 0.80

# Bytes for code on a chain that has no .su under <build_dir>/src. Measured on
# espressif32@6.9.0 with WebSockets 2.7.3; the library and libc frames are the
# same on the ESP32 and ESP32-S3 envs. Re-measure after bumping either pin:
# library frames from a build with PLATFORMIO_BUILD_FLAGS=-fstack-usage (.su
# files appear under <build_dir>/lib*/), libc and ESP-IDF frames from
# `xtensa-esp32-elf-objdump -d firmware.elf`, the `entry a1, N` at each symbol.
#
# From "Serial printf" on, each number is the deepest path the ELF's call graph
# shows below the named call, with virtual calls and the function-pointer
# tables in between (stdio, VFS, FATFS diskio, the LittleFS block device, the
# flash chip driver, std::function) followed by hand, and the larger of the
# ESP32 and S3 builds. Error logs are counted where the failure can happen in
# the field (a socket error, SD or flash I/O, a bad OTA image): ESP_LOGE ->
# vprintf is 1888 bytes, Arduino's log_e 1232. Argument checks, allocation
# failures, branches the callers' arguments rule out, abort and unwind are left
# out.
_ALLOWANCES = {
    # snprintf 192 + _svfprintf_r 784 + _dtoa_r 160, from the ELF.
    "printf %f": 1136,
    # WebSocketsServer::loop 32 + WebSocketsServerCore::loop 32 +
    # handleClientData 64 + handleHeader 128 + runCbEvent 48 + the
    # std::function invoker 32, from the library's .su files.
    "WebSockets connect": 336,
    # WebSocketsServer::loop 32 + WebSocketsServerCore::loop 32 +
    # handleClientData 64 + handleWebsocket 32 + handleWebsocketCb 64; the
    # frame header is then read in up to three parts (a masked frame with a
    # 16-bit length), each nesting handleWebsocketWaitFor 64 + readCb 32 +
    # std::function call 48 + 32 + handleWebsocketCb 64 = 240; then readCb 32 +
    # std::function call 48 + 32 + handleWebsocketPayloadCb 48 +
    # messageReceived 32 + runCbEvent 48 + invoker 32.
    "WebSockets message": 1216,
    # WebSocketsServer::loop 32 + WebSocketsServerCore::loop 32 +
    # handleClientData 64 + handleHeader 128 + execHttpHeaderValidation 80 +
    # the std::function invoker 80 (it takes two Strings by value), from the
    # library's .su files.
    "WebSockets handshake": 416,
    # WebServer::handleClient 80 + _handleRequest 112 +
    # FunctionRequestHandler::handle 64 + std::function call 32 + 32, from the
    # framework library's .su files.
    "WebServer": 320,
    # Estimate. Preferences -> NVS -> SPI flash is precompiled ESP-IDF, so this
    # is the deepest path the m5stack-atom ELF's call graph shows below
    # prefs_save(): Preferences::begin() -> nvs_flash_init_partition ->
    # nvs::Storage::init -> ... -> esp_flash_read and its error log, 2256
    # bytes, with the virtual calls in nvs:: and the flash driver followed by
    # hand and the abort and unwind paths left out. Preferences::put*() comes
    # to 2128 the same way; a write that hits no error path is nearer 1.4 KB.
    "NVS write": 2304,
    # Print::printf 160 + vsnprintf 64 + _vsnprintf_r 144 + _svfprintf_r 784 +
    # __ssprint_r 48 + memmove 32. The UART or USB write after it is
    # shallower: 672 below HardwareSerial::write() and HWCDC::write().
    "Serial printf": 1232,
    # WiFiClient::read 64 + WiFiClientRxBuffer::read 32 + fillBuffer 48 + its
    # log_e on a socket error 1232, the deepest WiFiClient call; write() and
    # setNoDelay() come to 1328 and 1376 the same way.
    "TCP socket": 1376,
    # WebServer::requestAuthentication 144 + send 32 + 64 + sendContent 32 +
    # 32 + WiFiClient::write 96 + its log_e 1232. A plain send() is 1520, and
    # authenticate() with a Digest header (md5str + sprintf) 1600.
    "HTTP reply": 1632,
    # WebServer::handleClient 80 + _parseRequest 256 + _parseForm 272 +
    # _uploadWriteByte 64 + FunctionRequestHandler::upload 64 + std::function
    # call 32 + 32: the /update upload callback runs below the form parser.
    "WebServer upload": 800,
    # UpdateClass::end 96 + _verifyEnd 32 + esp_ota_set_boot_partition 96 +
    # image_validate 304 + esp_image_verify 32 + image_load 80 +
    # process_checksum 96 + bootloader_flash_read 32 + spi_flash_read 32 +
    # esp_flash_read 64 + spi_flash_chip_generic_read 128 + its ESP_LOGE 1888.
    # Update.write() comes to 2336 and Update.begin() to 1296.
    "OTA flash": 2880,
    # esp_ota_mark_app_valid_cancel_rollback 32 + esp_ota_current_ota_is_workable
    # 96 + rewrite_ota_seq 32 + esp_partition_write 32 +
    # esp_flash_write_encrypted 128 + esp_flash_read_encrypted 48 +
    # spi_flash_chip_generic_read 128 + its ESP_LOGE 1888 (2336 on the S3).
    "OTA confirm": 2384,
    # fs::FS::remove 32 + VFSImpl::remove 144 + unlink 32 + 32 +
    # vfs_littlefs_unlink 304 + lfs_remove 128 + lfs_fs_forceconsistency 96,
    # then a metadata compaction (lfs_dir_commit 32 + lfs_fs_deorphan 176 +
    # lfs_dir_orphaningcommit 176 + relocatingcommit 144 + split 96 + compact
    # 160 + traverse 224 + 64, commitattr 80 + commitprog 48 + lfs_bd_prog 48 +
    # lfs_bd_flush 48 + lfs_bd_cmp 80) down to the flash read (lfs_bd_read 64 +
    # 48 + esp_partition_read 48 + esp_flash_read 64 + chip read 128) and its
    # ESP_LOGE 1888: 4384 on the LittleFS builds, the deepest file operation
    # there. FATFS on SD is lower ("SD card"), so this covers both backends.
    "black-box storage": 4384,
    # fs::FS::open(String) 32 + open 48 + VFSImpl::open 144 + VFSImpl::mkdir
    # 144 + VFSFileImpl() 64 + stat 32 + 32 + vfs_fat_stat 368 + f_stat 624
    # (FATFS keeps the long file name on the stack) + follow_path 64 + dir_find
    # 64 + 32 + 32 + 48 + 32 + 32 + ff_disk_write 32 + ff_sd_write 64 +
    # sdWriteSectors 64 + sdTransaction 32 + sdCommand 96 + sdSelectCard 48 +
    # its log_e when the card does not answer 1232.
    "SD card": 3360,
    # Mcp2515Driver::send 64 + MCP2515::sendMessage 48 + 64 + readRegister 32
    # + startSPI 48 + SPIClass::beginTransaction 48 + spiTransaction 32 + the
    # SPI bus mutex 432. TwaiDriver::send() is 560.
    "CAN send": 768,
    # wifi_init 64 + wifi_ap_init 80 + WiFi.softAP 352 + enableAP 32 + mode 48
    # + wifiLowLevelInit 416, then esp_wifi_init() into the Wi-Fi and PHY
    # libraries: 3248 on the S3, 3152 on the ESP32.
    "Wi-Fi start": 3248,
}

# Code that only some boards compile. A build has a feature when it compiled
# any of the feature's marker functions, each defined under the #if that brings
# the feature in, and a chain tied to a feature is checked only on the builds
# that have it. With several markers, one being inlined away does not hide the
# feature (a chain naming it fails instead); sleep_tick() is NOINLINE. Every
# marker has to stay defined in the sources, so a rename cannot quietly switch
# its chains off.
_FEATURES = {
    # blackbox.cpp with the LittleFS or SD backend; the RAM backend has none.
    "black box on disk": ("bb_drop_oldest", "backend_stream_body", "bb_scan_count"),
    # can_dump.cpp under BOARD_LILYGO: CAN dump, syslog and format on SD.
    "SD card": ("rm_recursive", "open_new_file", "syslog_open"),
    # main.cpp under BOARD_LILYGO.
    "deep sleep": ("sleep_tick",),
}

# (name, functions with a frame in the .su files, allowances[, feature]).
# prefs_save() is the deepest call in process_frame(), short of the SD dump,
# and in button_tick(). In web_command() deleting black-box files on disk goes
# deeper. A Wi-Fi save restarts from the caller once the reply is out
# (restart_for_wifi()), and the OTA confirm ahead of it goes as deep.
_CHAINS = [
    ("dashboard connect",
     ["loop", "web_dashboard_update", "ws_event", "build_json"],
     ["WebSockets connect", "printf %f"]),
    # The Origin check on each handshake header, which logs a refusal.
    ("dashboard handshake",
     ["loop", "web_dashboard_update", "ws_header_ok"],
     ["WebSockets handshake", "Serial printf"]),
    ("dashboard command",
     ["loop", "web_dashboard_update", "ws_event", "web_command", "prefs_save"],
     ["WebSockets message", "NVS write"]),
    ("dashboard Wi-Fi save",
     ["loop", "web_dashboard_update", "ws_event", "restart_for_wifi", "ota_verify_confirm"],
     ["WebSockets message", "OTA confirm"]),
    ("dashboard mark",
     ["loop", "web_dashboard_update", "ws_event", "web_command", "blackbox_mark",
      "blackbox_arm"],
     ["WebSockets message", "Serial printf"]),
    ("dashboard delete",
     ["loop", "web_dashboard_update", "ws_event", "web_command",
      "blackbox_delete_all", "backend_delete", "bb_scan_count"],
     ["WebSockets message", "black-box storage"], "black box on disk"),
    ("HTTP status",
     ["loop", "web_dashboard_update", "handle_status", "build_json"],
     ["WebServer", "printf %f"]),
    ("HTTP command",
     ["loop", "web_dashboard_update", "handle_cmd", "web_command", "prefs_save"],
     ["WebServer", "NVS write"]),
    # The Origin check ahead of POST /api/cmd and /sdformat, which logs and
    # answers a refusal; handle_cmd() has the larger frame.
    ("HTTP POST origin",
     ["loop", "web_dashboard_update", "handle_cmd", "post_from_dashboard"],
     ["WebServer", "HTTP reply"]),
    ("black-box list",
     ["loop", "web_dashboard_update", "handle_blackbox_list", "blackbox_list_json"],
     ["WebServer", "black-box storage"], "black box on disk"),
    ("black-box download",
     ["loop", "web_dashboard_update", "handle_blackbox_get", "blackbox_stream_body",
      "backend_stream_body"],
     ["WebServer", "black-box storage"], "black box on disk"),
    # download_auth_ok() runs ahead of both /blackbox handlers; /get has the
    # larger frame.
    ("black-box auth",
     ["loop", "web_dashboard_update", "handle_blackbox_get", "download_auth_ok"],
     ["WebServer", "HTTP reply"]),
    # The Host check ahead of /api/status, /api/aux and both /blackbox
    # handlers, which answers a refusal; /blackbox/get has the largest frame.
    ("HTTP Host check",
     ["loop", "web_dashboard_update", "handle_blackbox_get", "host_ours"],
     ["WebServer", "HTTP reply"]),
    ("OTA upload",
     ["loop", "web_dashboard_update", "handle_ota_upload"],
     ["WebServer upload", "OTA flash"]),
    # The deepest of the require_admin_auth() routes (/auth, /restart,
    # /sdformat, /update).
    ("OTA upload auth",
     ["loop", "web_dashboard_update", "handle_ota_upload", "require_admin_auth"],
     ["WebServer upload", "HTTP reply"]),
    ("CAN stream accept",
     ["loop", "web_dashboard_update", "http_can_stream_update", "accept_client"],
     ["TCP socket"]),
    # Whose request it is, from the device's names (own_names(): IP text).
    ("CAN stream access",
     ["loop", "web_dashboard_update", "http_can_stream_update", "accept_client",
      "web_dashboard_stream_access"],
     ["Serial printf"]),
    # rm_recursive() recurses once per path level, at most RM_MAX_DEPTH (8)
    # deep in can_dump.cpp; the layout it writes is three (/dumps/<n>/<file>),
    # the rest is room for a tree someone else put on the card.
    ("SD format",
     ["loop", "web_dashboard_update", "handle_sdformat", "sd_format_card"]
     + ["rm_recursive"] * 8,
     ["WebServer", "SD card"], "SD card"),
    ("CAN frame",
     ["loop", "drain_can_bus_index", "process_frame", "prefs_save"],
     ["NVS write"]),
    ("CAN frame, SD dump",
     ["loop", "drain_can_bus_index", "process_frame", "can_dump_record",
      "open_new_file"],
     ["SD card"], "SD card"),
    ("black-box flush",
     ["loop", "blackbox_tick", "do_flush"],
     ["Serial printf"]),
    # On disk do_flush() saves a step per call; the deepest step drops an old
    # event to make room.
    ("black-box flush, disk",
     ["loop", "blackbox_tick", "do_flush", "bb_drop_oldest"],
     ["black-box storage"], "black box on disk"),
    ("bus-off",
     ["loop", "blackbox_busoff", "blackbox_arm"],
     ["Serial printf"]),
    ("button",
     ["loop", "button_tick", "prefs_save"],
     ["NVS write"]),
    ("continuous AP",
     ["loop", "continuous_ap_tick", "continuous_ap_hw3_legacy_try_start_attempt",
      "gear_sequence_tick", "send_gear_position_for_sequence", "send_on_bus"],
     ["CAN send"]),
    # 12V-feed / reset log (#218). power_log_tick() commits to NVS from the
    # loop, but only once the bus has gone quiet; the write is as deep as any
    # other NVS write.
    ("power log",
     ["loop", "power_log_tick", "power_log_commit"],
     ["NVS write"]),
    ("deep sleep",
     ["loop", "sleep_tick", "ota_verify_confirm"],
     ["OTA confirm"], "deep sleep"),
    # On the way into deep sleep the 12V checkpoint is flushed if it was not
    # committed yet.
    ("deep sleep, 12V log",
     ["loop", "sleep_tick", "power_log_note_deep_sleep", "power_log_commit"],
     ["NVS write"], "deep sleep"),
    # setup() runs on the loop task too, before the first loop().
    ("boot",
     ["setup"],
     ["Wi-Fi start"]),
    ("boot, disk",
     ["setup", "blackbox_init", "bb_drop_orphans"],
     ["black-box storage"], "black box on disk"),
]

# A frame this large (bytes) that is on no chain checked for this build fails
# the check. One FSDState copy is 792 bytes: every function holding one (832 to
# 880) or a buffer of 256 bytes or more (accept_client 544, backend_stream_body
# 640) is above this, and the largest frame below it is 320.
_TRIPWIRE = 512

# Frames at or above _TRIPWIRE that need no chain, each with the reason it
# cannot run deep on the loop task. Nothing needs it today.
_KNOWN_SAFE = {
}


def _say(text):
    print("[stack] %s" % text)


def _annotate(kind, text):
    # GitHub Actions turns these lines into annotations on the run.
    if os.environ.get("GITHUB_ACTIONS") == "true":
        for raw, escaped in (("%", "%25"), ("\r", "%0D"), ("\n", "%0A")):
            text = text.replace(raw, escaped)
        print("::%s title=Loop-task stack budget::%s" % (kind, text))


def loop_stack_size(main_cpp):
    """Bytes of loop-task stack main.cpp asks for, or the Arduino default."""
    with open(main_cpp, encoding="utf-8", errors="replace") as f:
        text = f.read()
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    sizes = []
    for rest in re.findall(r"\bSET_LOOP_TASK_STACK_SIZE\b([^\n]*)", text):
        # "(16 * 1024);" -> 16384. Anything else is an error, not the default.
        m = re.fullmatch(r"\s*\((.*)\)\s*;?\s*", rest)
        size = 1
        for factor in m.group(1).split("*") if m else [""]:
            n = re.fullmatch(r"\s*(0[xX][0-9a-fA-F]+|[1-9][0-9]*)[uUlL]*\s*", factor)
            if not n:
                raise ValueError("cannot evaluate SET_LOOP_TASK_STACK_SIZE%s in %s"
                                 % (rest.rstrip(), main_cpp))
            size *= int(n.group(1), 0)
        sizes.append(size)
    # More than one (an #if per board): hold every env to the smallest.
    return min(sizes) if sizes else _DEFAULT_STACK


def rm_max_depth(src_dir):
    """RM_MAX_DEPTH from can_dump.cpp, or None when the file has none."""
    try:
        with open(os.path.join(src_dir, "can_dump.cpp"), encoding="utf-8", errors="replace") as f:
            m = re.search(r"^#define\s+RM_MAX_DEPTH\s+(\d+)[uU]?\b", f.read(), re.M)
    except OSError:
        return None
    return int(m.group(1)) if m else None


def _bare_name(signature):
    # "void process_frame(CanBusId, const CanFrame&)" -> "process_frame".
    # C objects carry the bare name already.
    head = signature.split("(", 1)[0].split()
    return head[-1].lstrip("*&") if head else ""


def _name_from_source(path, line, col):
    # GCC 8 writes only ")" for a variadic function: take the name at the
    # line and column it gives.
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            for _ in range(line - 1):
                f.readline()
            text = f.readline()
    except OSError:
        return ""
    m = re.match(r"([A-Za-z_]\w*)\s*\(", text[col - 1:])
    return m.group(1) if m else ""


def read_frames(build_dir, src_dir):
    """({function: [(bytes, qualifier, where), ...]}, files read) from <build_dir>/src."""
    su_root = os.path.join(build_dir, "src")
    frames = {}
    files = 0
    for root, _dirs, names in os.walk(su_root):
        for name in sorted(names):
            if not name.endswith(".su"):
                continue
            rel = os.path.relpath(os.path.join(root, name), su_root)
            # A cached build dir keeps the .su of a source file that has since
            # been deleted or renamed. Its frames are not in this firmware.
            source = os.path.join(src_dir, rel[:-len(".su")])
            if not os.path.isfile(source):
                _say("ignoring %s: its source file is gone" % rel)
                continue
            files += 1
            with open(os.path.join(root, name), encoding="utf-8", errors="replace") as f:
                for line in f:
                    # file:line:col:signature<TAB>bytes<TAB>static|dynamic|dynamic,bounded
                    fields = line.rstrip("\n").split("\t")
                    where = fields[0].split(":", 3)
                    if len(fields) != 3 or len(where) != 4 or not fields[1].isdigit():
                        continue
                    fn = _bare_name(where[3])
                    if where[3].endswith("cpp)"):
                        # "(static initializers for .firmware/x.cpp)", cut short
                        fn = "(static initializers, %s)" % where[0]
                    elif where[3] == ")":
                        fn = (_name_from_source(source, int(where[1]), int(where[2]))
                              if where[1].isdigit() and where[2].isdigit() else "") or \
                            "(variadic function, %s:%s)" % (where[0], where[1])
                    frames.setdefault(fn, []).append(
                        (int(fields[1]), fields[2], "%s:%s" % (where[0], where[1])))
    return frames, files


def _defined_in_sources(src_dir, names):
    """The subset of names that still appear as a function in the sources."""
    text = []
    for root, _dirs, files in os.walk(src_dir):
        for name in files:
            if name.endswith((".c", ".cpp", ".h", ".hpp", ".inc")):
                with open(os.path.join(root, name), encoding="utf-8", errors="replace") as f:
                    text.append(f.read())
    text = "\n".join(text)
    return {n for n in names if re.search(r"\b%s\s*\(" % re.escape(n), text)}


def check(build_dir, src_dir, allow_missing):
    """Print the budget table; return the list of failures (empty = pass)."""
    main_cpp = os.path.join(src_dir, "main.cpp")
    try:
        stack = loop_stack_size(main_cpp)
    except OSError as e:
        return ["cannot read %s: %s" % (main_cpp, e.strerror)]
    except ValueError as e:
        return [str(e)]

    frames, files = read_frames(build_dir, src_dir)
    su_glob = os.path.join(build_dir, "src", "*.su")
    if not files:
        return ["no .su files in %s. Build the env first; -fstack-usage comes "
                "from build_src_flags in platformio.ini, and objects built "
                "without it need a `pio run -t clean`" % su_glob]
    if not frames:
        return ["no frame could be parsed from the %d .su file(s) in %s"
                % (files, su_glob)]

    errors = []
    notes = []
    # The SD format chain counts one rm_recursive() frame per level it may go.
    depth = rm_max_depth(src_dir)
    counted = max(chain[1].count("rm_recursive") for chain in _CHAINS)
    if depth is not None and depth != counted:
        errors.append("RM_MAX_DEPTH is %d in can_dump.cpp but the SD format chain "
                      "counts %d rm_recursive() frames: update _CHAINS in %s"
                      % (depth, counted, os.path.basename(__file__)))
    markers = {m for found in _FEATURES.values() for m in found}
    gone = sorted(markers - _defined_in_sources(src_dir, markers))
    if gone:
        text = ("feature marker %s no longer defined under %s: renamed or removed, "
                "update _FEATURES in %s" % (", ".join("%s()" % m for m in gone),
                                            src_dir, os.path.basename(__file__)))
        (notes if allow_missing else errors).append(text)
    present = {feature for feature, found in _FEATURES.items()
               if any(m in frames for m in found)}
    _say("features: %s" % ", ".join(
        "%s %s" % (feature, "yes" if feature in present else "no")
        for feature in _FEATURES))

    sizes = {}
    for chain in _CHAINS:
        for fn in chain[1]:
            if fn in sizes or fn not in frames:
                continue
            # GCC lists a clone (.part, .constprop) under the function's own
            # name, and a .part nests inside it: count them all.
            sizes[fn] = sum(hit[0] for hit in frames[fn])
            if len(frames[fn]) > 1:
                _say("note: %s() has %d frames in the .su files, summed"
                     % (fn, len(frames[fn])))
            for _size, qualifier, _where in frames[fn]:
                if qualifier not in ("static", "dynamic,bounded"):
                    errors.append(
                        "%s() has a %s frame (alloca or a variable-length "
                        "array): the %d bytes in its .su are not an upper bound"
                        % (fn, qualifier, sizes[fn]))

    limit = int(stack * _FAIL_FRACTION)
    warn = int(stack * _WARN_FRACTION)
    _say("loop task stack %d bytes, chain limit %d (%d%%), warning from %d (%d%%)"
         % (stack, limit, round(_FAIL_FRACTION * 100), warn, round(_WARN_FRACTION * 100)))
    _say("frames: %s" % ", ".join("%s %d" % item for item in sizes.items()))
    _say("%-22s %6s  %8s" % ("chain", "bytes", "of stack"))

    warnings = []
    covered = set()
    evaluated = 0
    for chain in _CHAINS:
        name, functions, allowances = chain[:3]
        feature = chain[3] if len(chain) > 3 else None
        if feature and feature not in present:
            _say("%-22s %6s  %8s  not in this build (no %s)" % (name, "-", "-", feature))
            continue
        missing = ["%s()" % fn for fn in functions if fn not in sizes]
        if missing:
            text = "no stack frame for %s in %s" % (", ".join(missing), su_glob)
            if allow_missing:
                _say("%-22s %6s  %8s  skipped: %s" % (name, "-", "-", text))
            else:
                _say("%-22s %6s  %8s" % (name, "?", "?"))
                errors.append(
                    "%s: %s. Renamed, removed or inlined into its caller: "
                    "update _CHAINS in %s" % (name, text, os.path.basename(__file__)))
            continue
        covered.update(functions)
        parts = [(fn, sizes[fn]) for fn in functions]
        parts += [(allowance, _ALLOWANCES[allowance]) for allowance in allowances]
        total = sum(size for _label, size in parts)
        evaluated += 1
        _say("%-22s %6d  %7.1f%%" % (name, total, 100.0 * total / stack))
        text = ("%s needs %d bytes, %.1f%% of the %d-byte loop task stack "
                "(limit %d, %d%%): %s"
                % (name, total, 100.0 * total / stack, stack, limit,
                   round(_FAIL_FRACTION * 100),
                   " + ".join("%s %d" % part for part in parts)))
        if total > limit:
            errors.append(text + ". Shrink a frame on that chain or raise "
                                 "SET_LOOP_TASK_STACK_SIZE in main.cpp")
        elif total > warn:
            warnings.append(text)
    if not evaluated:
        errors.append("no chain could be evaluated")

    # Large frames the chains above do not account for.
    others = sorted(((sum(hit[0] for hit in hits), fn, max(hits)[2])
                     for fn, hits in frames.items() if fn not in covered),
                    reverse=True)
    _say("largest frames on no chain: %s"
         % ", ".join("%s %d" % (fn, size) for size, fn, _where in others[:5]))
    for size, fn, where in others:
        if size < _TRIPWIRE or fn in _KNOWN_SAFE:
            continue
        text = ("%s has a %d-byte frame and is on no chain checked for "
                "this build (tripwire %d). Add the call chain it runs on to "
                "_CHAINS, shrink the frame, or list it in _KNOWN_SAFE with the "
                "reason it cannot run deep on the loop task"
                % (fn if fn.startswith("(") else "%s() (%s)" % (fn, where), size, _TRIPWIRE))
        (notes if allow_missing else errors).append(text)

    for text in notes:
        _say("note: %s" % text)
    for text in warnings:
        _say("WARNING: %s" % text)
        _annotate("warning", text)
    return errors


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(
        description="Fail when a loop-task call chain gets too close to the "
                    "loop task's stack size, or a large frame is on no chain.")
    ap.add_argument("build_dir", help="PlatformIO build dir, .pio/build/<env>")
    ap.add_argument("--src-dir", default=os.path.join(here, ".firmware"),
                    help="firmware sources (default: .firmware beside this script)")
    ap.add_argument("--allow-missing", action="store_true",
                    help="skip a chain whose functions this tree does not have, "
                         "and only report large frames on no chain (old tags only)")
    args = ap.parse_args()

    errors = check(args.build_dir, args.src_dir, args.allow_missing)
    for text in errors:
        _say("FAIL: %s" % text)
        _annotate("error", text)
    if errors:
        return 1
    _say("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
