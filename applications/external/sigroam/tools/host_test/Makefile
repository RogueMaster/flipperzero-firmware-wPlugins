# SigRoam host_test
#
# Usage: make -C tools/host_test
# Output: run_tests in this directory (run it from the repo root as
# ./tools/host_test/run_tests)
#
# ---------------------------------------------------------------------------
# Stub policy (zero stubs in this first batch; do not create stubs/)
#
# This framework ships with zero stubs in its first batch. If a future
# change ever needs a stub for a "thin wrapper carrying furi types",
# FuriWaitForever must be defined as 0xFFFFFFFFU -- confirmed on this
# machine's SDK at ~/.ufbt/current/sdk_headers/f7_sdk/furi/core/base.h:13,
# which reads:
#   FuriWaitForever = 0xFFFFFFFFU,
# A sibling project on the author's machine defines it in its own stubs/furi.h
# as #define FuriWaitForever 0U, which is wrong -- do not copy that value.
# Official 1.4.3's furi/core/mutex.c treats timeout==0 as a non-blocking
# try; copying that mistake would make this repo's future "zero-wait lock
# acquisition" tests meaningless.
# ---------------------------------------------------------------------------

# Relative to the directory this Makefile lives in (make -C tools/host_test).
# Not using abspath/MAKEFILE_LIST: the repo root directory name contains a
# space, and make 3.81 would split it into separate words.
SRC := ../../src

# selfcontained only tests headers and needs an explicit list. Add new ★
# headers here when you create them.
STAR_HEADERS := sr_types.h sr_source_codec.h sr_source.h sr_line.h sr_bloom.h sr_model.h sr_settings.h sr_parse_marauder.h sr_rx_policy.h sr_handshake.h sr_scan_ctl.h sr_rawlog.h sr_view_fmt.h sr_gps_sample.h sr_wait_stage.h sr_resync.h sr_alert.h sr_poi.h
STAR_PATHS   := $(addprefix $(SRC)/,$(STAR_HEADERS))

# guard does not use an allowlist: every sr_*.h / sr_*.c under src/ must be
# furi-free by default, with the sole exception of device-side files
# explicitly waived in NON_STAR.
# Rationale: an allowlist entry that never gets registered means a silent
# pass, which is exactly how ADR-003 would most easily get bypassed.
# Device-side waivers. sr_settings.* has been promoted to ★ (ADR-012); the
# file that still touches furi is the store layer.
NON_STAR := sr_io.h sr_io.c sr_worker.h sr_worker.c sr_settings_store.h sr_settings_store.c sr_notify.h sr_notify.c
# A broken glob (src/ renamed or moved) would make guard scan 0 files and
# still report OK -- that is the most insidious failure mode. A floor value
# guards against it. Raise the floor whenever you add a new ★ file.
GUARD_MIN := 23

# GUI dual-target banlist (V-059 ③: 11 symbols that are Official ABSENT /
# Momentum +).
# A broken glob that scans 0 files and still reports OK is the most
# insidious failure mode; a floor value guards against it.
GUI_SCAN_MIN := 8
GUI_BANNED := canvas_current_font_width canvas_draw_overlay canvas_draw_icon_animation_ex FontBatteryPercent view_set_ascii_callback elements_scrollbar_horizontal elements_scrollable_text_line_centered variable_item_list_set_header variable_item_list_get variable_item_set_item_label variable_item_set_locked
# P12 static guard (the T5.3 "SD full" tier). Scans every storage_file_write(
# call site in the repo and asserts that the return value is compared for
# equality on the same line -- when the SD card is full, write() can do a
# partial write and return a short byte count, and not checking for that is
# how data gets silently dropped (the Plan section 5 P12, and the project's
# "known pitfalls" notes).
#
# WARNING: this is a static guard, not a runtime test: the host has no
#    storage service, so a real "SD full" condition cannot be produced here.
#    What it actually catches is the real regression path of someone adding
#    a new write call later and forgetting to check the return value.
#    The one call site that currently exists, src/sr_settings_store.c:140,
#    already complies.
#
# Pass condition (fail-closed): the line must match storage_file_write(...) ==.
#   - A bare == is not enough: `if(x == 1) storage_file_write(f,b,n);` must
#     be judged FAIL.
#   - A call split across multiple lines is judged FAIL -- this is
#     intentional; collapse it to one line or review it by hand before
#     touching this rule. Do not loosen the regex just to turn it green.
# The two floor values each guard against a different silent failure: 0
# files scanned (glob broken / source moved), and 0 call sites found (the
# write-to-disk code disappeared entirely and the rule degenerates into
# always-OK).
STORAGE_SCAN_MIN  := 8
STORAGE_CALLS_MIN := 1

# Font guard (T4.11 / ADR-024). UI-SPEC section 2 used to say "set
# canvas_set_font exactly once for the whole file", but that stopped holding
# once F3 needed large digits -- once that mechanism was gone, a guard had
# to take its place.
#
# Rule: FontBigNumbers may only appear inside the body of
# sr_view_dash_put_big() in views/sr_view_dash.c, and within that range the
# number of times FontSecondary is restored must be >= the number of times
# it switches to FontBigNumbers.
#
# WARNING: this is a static guard with two known narrowed guarantees,
#    recorded here honestly instead of glossed over:
#    ① It cannot prove "the restore genuinely ran at runtime" (a branch
#        could skip the restore statement) -- the real evidence is track-B
#        screen witness B4: switching from Dash to Strm, the font is still
#        small. **The discriminating power has been measured (2026-08-31;
#        don't assume it's stronger than this)**: put_big() has 2 restore
#        sites; deleting 1 gives setsec=1, which is not less than setbig=1,
#        so it **stays green**; deleting both gives setsec=0, which goes
#        red. In other words it only catches "every restore statement was
#        deleted outright", not "one particular branch's restore was
#        deleted". Catching the latter needs control-flow analysis, which a
#        static grep fundamentally cannot do, and this rule does not
#        pretend otherwise.
#    ② It only matches the call form `canvas_set_font(... FontBigNumbers`
#        and not a bare symbol name -- writing the symbol name inside a
#        comment is legitimate (this target's first version even flagged
#        its own explanatory comment as FAIL), but
#        `Font f = FontBigNumbers; canvas_set_font(canvas, f);` can slip
#        past it. gui_portable is the same kind of regex guard with the
#        same limitation; no separate parser has been added just for this.
#    What it actually guards against is two real regression paths: someone
#    casually switching to the large font somewhere else, and a restore
#    statement being deleted outright.
#
# The two floor values each guard against a different silent failure: 0
# files scanned (glob broken), and 0 FontBigNumbers occurrences within the
# range (the large-font code disappeared entirely and the rule degenerates
# into always-OK).
FONT_SCAN_MIN := 8
FONT_BIG_MIN  := 1

CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -I. -I$(SRC)
# Apple's bundled AddressSanitizer aborts before main() runs whenever leak
# detection is requested.
# Address/UB checks remain enabled; this target makes no claim about leak
# detection.
ASAN_CFLAGS := -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined

# The ★ implementations under test are compiled straight into run_tests
# (zero stubs, see above).
HOST_SRCS := main.c test_source_iface.c test_line.c test_bloom.c test_model.c test_settings.c test_parse_marauder.c test_rx_policy.c test_handshake.c test_scan_ctl.c test_rawlog.c test_view_fmt.c test_gps_sample.c test_wait_stage.c test_resync.c test_alert.c test_poi.c $(SRC)/sr_line.c $(SRC)/sr_bloom.c $(SRC)/sr_model.c $(SRC)/sr_settings.c $(SRC)/sr_parse_marauder.c

.PHONY: all guard gui_portable storage_guard font_guard selfcontained asan clean

all: guard gui_portable storage_guard font_guard selfcontained run_tests
	./run_tests

run_tests: $(HOST_SRCS) sr_test.h $(STAR_PATHS)
	$(CC) $(CFLAGS) -o $@ $(HOST_SRCS)

guard:
	@fail=0; found=0; \
	for h in $(STAR_HEADERS); do \
	  test -f "$(SRC)/$$h" || { echo "guard: missing $(SRC)/$$h" >&2; exit 1; }; \
	done; \
	for f in $(SRC)/sr_*.h $(SRC)/sr_*.c; do \
	  test -e "$$f" || continue; \
	  base=`basename "$$f"`; \
	  case " $(NON_STAR) " in *" $$base "*) continue;; esac; \
	  found=`expr $$found + 1`; \
	  if grep -nE '#[[:space:]]*include[[:space:]]*[<"]furi' "$$f"; then \
	    echo "guard: FAIL $$f includes furi" >&2; \
	    fail=1; \
	  fi; \
	done; \
	if [ "$$found" -lt $(GUARD_MIN) ]; then \
	  echo "guard: FAIL only $$found ★ files found, expected at least $(GUARD_MIN) -- glob broken or src/ moved?" >&2; \
	  exit 1; \
	fi; \
	if [ "$$fail" -ne 0 ]; then exit 1; fi; \
	echo "guard: OK $$found ★ files, no furi include"

# Relative paths: the repo root directory name contains a space, so this
# avoids abspath. cwd is tools/host_test.
gui_portable:
	@fail=0; found=0; \
	for f in ../../src/*.c ../../src/*.h ../../scenes/*.c ../../scenes/*.h ../../views/*.c ../../views/*.h ../../*.c ../../*.h; do \
	  test -e "$$f" || continue; \
	  found=`expr $$found + 1`; \
	  for s in $(GUI_BANNED); do \
	    hits=`grep -nE '(^|[^a-zA-Z0-9_])'"$$s"'([^a-zA-Z0-9_]|$$)' "$$f" || true`; \
	    if [ -n "$$hits" ]; then \
	      echo "$$hits" | while IFS= read -r line; do \
	        echo "gui_portable: FAIL $$f:$$line  [$$s]"; \
	      done; \
	      fail=1; \
	    fi; \
	  done; \
	done; \
	if [ "$$found" -lt $(GUI_SCAN_MIN) ]; then \
	  echo "gui_portable: FAIL only $$found files found, expected at least $(GUI_SCAN_MIN) -- glob broken or sources moved?" >&2; \
	  exit 1; \
	fi; \
	if [ "$$fail" -ne 0 ]; then exit 1; fi; \
	echo "gui_portable: OK $$found files, no Momentum-only GUI symbols"

storage_guard:
	@fail=0; found=0; calls=0; \
	for f in ../../src/*.c ../../src/*.h ../../scenes/*.c ../../scenes/*.h ../../views/*.c ../../views/*.h ../../*.c ../../*.h; do \
	  test -e "$$f" || continue; \
	  found=`expr $$found + 1`; \
	  hits=`grep -nE 'storage_file_write[[:space:]]*\(' "$$f" || true`; \
	  test -n "$$hits" || continue; \
	  calls=`expr $$calls + \`printf '%s\\n' "$$hits" | wc -l\``; \
	  bad=`printf '%s\n' "$$hits" | grep -vE 'storage_file_write[[:space:]]*\(.*\)[[:space:]]*==' || true`; \
	  if [ -n "$$bad" ]; then \
	    printf '%s\n' "$$bad" | while IFS= read -r line; do \
	      echo "storage_guard: FAIL $$f:$$line  [return value not compared for equality against the write length]"; \
	    done; \
	    fail=1; \
	  fi; \
	done; \
	if [ "$$found" -lt $(STORAGE_SCAN_MIN) ]; then \
	  echo "storage_guard: FAIL only $$found files found, expected at least $(STORAGE_SCAN_MIN) -- glob broken or sources moved?" >&2; \
	  exit 1; \
	fi; \
	if [ "$$calls" -lt $(STORAGE_CALLS_MIN) ]; then \
	  echo "storage_guard: FAIL only $$calls storage_file_write call sites found, expected at least $(STORAGE_CALLS_MIN) -- write path deleted or renamed?" >&2; \
	  exit 1; \
	fi; \
	if [ "$$fail" -ne 0 ]; then exit 1; fi; \
	echo "storage_guard: OK $$found files / $$calls storage_file_write call sites, every return value checked"

font_guard:
	@fail=0; found=0; big=0; \
	src=../../views/sr_view_dash.c; \
	test -f "$$src" || { echo "font_guard: FAIL cannot find $$src" >&2; exit 1; }; \
	range=`awk '/^static int32_t sr_view_dash_put_big\(/{s=NR} s && /^\}/{print s" "NR; exit}' "$$src"`; \
	if [ -z "$$range" ]; then \
	  echo "font_guard: FAIL cannot locate the body range of sr_view_dash_put_big() -- renamed or deleted?" >&2; \
	  exit 1; \
	fi; \
	lo=`echo "$$range" | cut -d" " -f1`; \
	hi=`echo "$$range" | cut -d" " -f2`; \
	for f in ../../src/*.c ../../src/*.h ../../scenes/*.c ../../scenes/*.h ../../views/*.c ../../views/*.h ../../*.c ../../*.h; do \
	  test -e "$$f" || continue; \
	  found=`expr $$found + 1`; \
	  hits=`grep -nE 'canvas_set_font[[:space:]]*\([^)]*FontBigNumbers' "$$f" || true`; \
	  test -n "$$hits" || continue; \
	  for ln in `printf '%s\n' "$$hits" | cut -d: -f1`; do \
	    if [ "$$f" = "$$src" ] && [ "$$ln" -ge "$$lo" ] && [ "$$ln" -le "$$hi" ]; then \
	      big=`expr $$big + 1`; \
	    else \
	      echo "font_guard: FAIL $$f:$$ln  [FontBigNumbers is only allowed inside sr_view_dash_put_big()]"; \
	      fail=1; \
	    fi; \
	  done; \
	done; \
	setbig=`sed -n "$$lo,$$hi p" "$$src" | grep -cE 'canvas_set_font\([[:space:]]*canvas,[[:space:]]*FontBigNumbers' || true`; \
	setsec=`sed -n "$$lo,$$hi p" "$$src" | grep -cE 'canvas_set_font\([[:space:]]*canvas,[[:space:]]*FontSecondary' || true`; \
	if [ "$$setsec" -lt "$$setbig" ]; then \
	  echo "font_guard: FAIL FontSecondary restored $$setsec times < FontBigNumbers set $$setbig times in range -- a restore path is missing" >&2; \
	  fail=1; \
	fi; \
	if [ "$$found" -lt $(FONT_SCAN_MIN) ]; then \
	  echo "font_guard: FAIL only $$found files found, expected at least $(FONT_SCAN_MIN) -- glob broken or sources moved?" >&2; \
	  exit 1; \
	fi; \
	if [ "$$big" -lt $(FONT_BIG_MIN) ]; then \
	  echo "font_guard: FAIL only $$big FontBigNumbers found in range, expected at least $(FONT_BIG_MIN) -- big-font code deleted?" >&2; \
	  exit 1; \
	fi; \
	if [ "$$fail" -ne 0 ]; then exit 1; fi; \
	echo "font_guard: OK $$found files / big font only inside put_big() at $$big sites / restored $$setsec times"

selfcontained:
	@set -e; \
	tmpdir=`mktemp -d`; \
	trap 'rm -rf "$$tmpdir"' EXIT; \
	for h in $(STAR_HEADERS); do \
	  printf '%s\n' "#include \"$$h\"" > "$$tmpdir/tu.c"; \
	  $(CC) $(CFLAGS) -c "$$tmpdir/tu.c" -o "$$tmpdir/$$h.o"; \
	  echo "selfcontained: $$h OK"; \
	done

asan: guard gui_portable storage_guard font_guard selfcontained
	$(CC) $(CFLAGS) $(ASAN_CFLAGS) -o run_tests_asan $(HOST_SRCS)
	ASAN_OPTIONS=detect_leaks=0 ./run_tests_asan

clean:
	rm -f run_tests run_tests_asan
	rm -rf run_tests.dSYM run_tests_asan.dSYM
