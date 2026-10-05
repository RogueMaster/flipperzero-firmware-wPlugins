# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Filip Sedivy
#
# Host build of the eMRTD test suite.
#
# The protocol and cryptographic layers of the application are free of Flipper
# dependencies, so they compile natively and can be checked against the
# published ICAO Doc 9303 test vectors. mbed TLS is built here from the same
# release the firmware ships, with the firmware's own configuration file
# (lib/flipper_mbedtls_cfg.h, a verbatim copy of the SDK's lib/mbedtls_cfg.h),
# so that the
# host sees exactly the feature set the device has - brainpool curves off,
# CMAC absent, MBEDTLS_ECP_MAX_BITS at 256.

# Override to point the drift check at a specific SDK copy of mbedtls_cfg.h.
SDK_CFG         ?=
# STRICT=1 turns "no SDK to compare against" from a skip into a failure, which
# is what continuous integration wants: a check that quietly compares nothing
# reads exactly like a check that passed.
STRICT          ?= 0

MBEDTLS_VERSION := v3.6.2
DEPS_DIR        := .deps
MBEDTLS_DIR     := $(DEPS_DIR)/mbedtls
BUILD_DIR       := .build

ROOT := ../..

# Application sources under test, and the simulated chip they are driven
# against. Extended as the layers land.
APP_SRC := \
	$(ROOT)/emrtd_error.c \
	$(ROOT)/crypto/emrtd_crypto.c \
	$(ROOT)/crypto/emrtd_mac.c \
	$(ROOT)/crypto/emrtd_kdf.c \
	$(ROOT)/crypto/emrtd_ec.c \
	$(ROOT)/crypto/emrtd_rng.c \
	$(ROOT)/crypto/emrtd_sm.c \
	$(ROOT)/crypto/emrtd_bac.c \
	$(ROOT)/crypto/emrtd_pace.c \
	$(ROOT)/protocol/emrtd_tlv.c \
	$(ROOT)/protocol/emrtd_apdu.c \
	$(ROOT)/protocol/emrtd_files.c \
	$(ROOT)/protocol/emrtd_mrz.c \
	$(ROOT)/protocol/emrtd_lds.c \
	$(ROOT)/protocol/emrtd_security_info.c \
	$(ROOT)/transport/emrtd_transceiver.c \
	$(ROOT)/transport/emrtd_isodep.c \
	$(ROOT)/access/emrtd_access.c \
	$(ROOT)/access/emrtd_access_bac.c \
	$(ROOT)/access/emrtd_access_pace.c \
	$(ROOT)/sim/emrtd_sim.c

TEST_SRC := \
	emrtd_test.c \
	test_mac.c \
	test_kdf.c \
	test_ec.c \
	test_tlv.c \
	test_apdu.c \
	test_isodep.c \
	test_files.c \
	test_mrz.c \
	test_lds.c \
	test_security_info.c \
	test_sm.c \
	test_bac.c \
	test_pace.c \
	test_access.c \
	test_session.c

# The translation units of lib/mbedtls.scons that the application actually
# reaches. The firmware also builds ecdsa, md5 and ripemd160 into its archive,
# but nothing here references them and the linker would then demand the ASN.1
# writer they pull in. constant_time.c is added because the host links every
# object explicitly instead of letting the archive resolve on demand.
MBEDTLS_SRC := \
	$(MBEDTLS_DIR)/library/aes.c \
	$(MBEDTLS_DIR)/library/bignum.c \
	$(MBEDTLS_DIR)/library/bignum_core.c \
	$(MBEDTLS_DIR)/library/constant_time.c \
	$(MBEDTLS_DIR)/library/des.c \
	$(MBEDTLS_DIR)/library/ecp.c \
	$(MBEDTLS_DIR)/library/ecp_curves.c \
	$(MBEDTLS_DIR)/library/platform_util.c \
	$(MBEDTLS_DIR)/library/sha1.c \
	$(MBEDTLS_DIR)/library/sha256.c

CFLAGS ?= -O1 -g
CFLAGS += -std=gnu11 -Wall -Wextra -Werror -Wno-unused-function
# Record which headers each object was built from, so that changing one
# rebuilds what depends on it. Without this a change to emrtd_error.h leaves
# objects holding the old enumerator values and the suite fails somewhere far
# from the edit, which is a slow way to learn about a stale build.
CFLAGS += -MMD -MP
CFLAGS += -DEMRTD_HOST_BUILD=1
CFLAGS += -DMBEDTLS_CONFIG_FILE='"flipper_mbedtls_cfg.h"'
CFLAGS += -I. -Ilib -I$(MBEDTLS_DIR)/include -I$(MBEDTLS_DIR)/library

# The vendored mbed TLS is third party code; do not hold it to -Werror.
MBEDTLS_CFLAGS := $(filter-out -Werror,$(CFLAGS)) -Wno-error

SAN ?= -fsanitize=address,undefined -fno-omit-frame-pointer

# LeakSanitizer only exists on Linux, which is where CI runs the leak gate.
ifeq ($(shell uname -s),Linux)
ASAN_OPTIONS ?= detect_leaks=1
else
ASAN_OPTIONS ?= detect_leaks=0
endif

BIN := $(BUILD_DIR)/emrtd_tests

.PHONY: test build clean distclean deps config-drift gcc-check

# Both entry points fetch the dependency first and then re-enter, because a
# pattern rule only matches when its prerequisite already exists. On a fresh
# clone the mbed TLS sources are not there yet, so a single pass would decide
# there is no rule to build .build/mbedtls_aes.o and stop - which is exactly
# what a clean checkout in continuous integration does.
test: deps
	@$(MAKE) --no-print-directory $(BIN)
	ASAN_OPTIONS=$(ASAN_OPTIONS) UBSAN_OPTIONS=halt_on_error=1 $(BIN)

build: deps
	@$(MAKE) --no-print-directory $(BIN)

MBEDTLS_OBJ := $(patsubst $(MBEDTLS_DIR)/library/%.c,$(BUILD_DIR)/mbedtls_%.o,$(MBEDTLS_SRC))
APP_OBJ     := $(patsubst $(ROOT)/%.c,$(BUILD_DIR)/app_%.o,$(APP_SRC))
TEST_OBJ    := $(patsubst %.c,$(BUILD_DIR)/test_host_%.o,$(TEST_SRC))

$(BIN): $(MBEDTLS_OBJ) $(APP_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^

# mbed TLS is third party code, so it is built without -Werror.
$(BUILD_DIR)/mbedtls_%.o: $(MBEDTLS_DIR)/library/%.c | $(BUILD_DIR)
	$(CC) $(MBEDTLS_CFLAGS) $(SAN) -c -o $@ $<

$(BUILD_DIR)/app_%.o: $(ROOT)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN) -c -o $@ $<

$(BUILD_DIR)/test_host_%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(MBEDTLS_OBJ:.o=.d) $(APP_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

deps: $(MBEDTLS_DIR)/.stamp

$(MBEDTLS_DIR)/.stamp:
	mkdir -p $(DEPS_DIR)
	git clone --depth 1 --branch $(MBEDTLS_VERSION) \
		https://github.com/Mbed-TLS/mbedtls.git $(MBEDTLS_DIR)
	touch $@

# Fails if the vendored copy of the firmware configuration has drifted from the
# SDK that ufbt deployed. Skipped when no SDK is present.
config-drift:
	@sdk="$(SDK_CFG)"; \
	if [ -z "$$sdk" ]; then \
		for c in "$${UFBT_HOME:-$$HOME/.ufbt}/current/sdk_headers/f7_sdk/lib/mbedtls_cfg.h" \
		         "$$HOME/.ufbt/current/sdk_headers/f7_sdk/lib/mbedtls_cfg.h"; do \
			[ -f "$$c" ] && sdk="$$c" && break; \
		done; \
	fi; \
	if [ -n "$$sdk" ] && [ -f "$$sdk" ]; then \
		if diff -q "$$sdk" lib/flipper_mbedtls_cfg.h >/dev/null; then \
			echo "mbedtls config matches the deployed SDK"; \
		else \
			echo "lib/flipper_mbedtls_cfg.h has drifted from $$sdk"; \
			diff -u lib/flipper_mbedtls_cfg.h "$$sdk" || true; \
			exit 1; \
		fi; \
	else \
		if [ "$(STRICT)" = "1" ]; then \
			echo "no ufbt SDK found, so there is nothing to compare against."; \
			echo "looked in:"; \
			echo "  $${UFBT_HOME:-$$HOME/.ufbt}/current/sdk_headers/f7_sdk/lib/mbedtls_cfg.h"; \
			echo "  $$HOME/.ufbt/current/sdk_headers/f7_sdk/lib/mbedtls_cfg.h"; \
			echo "pass SDK_CFG=<path> to point at it directly."; \
			exit 1; \
		fi; \
		echo "no ufbt SDK deployed, skipping the drift check"; \
	fi

# A second opinion from GCC.
#
# Continuous integration compiles this suite with GCC; a developer on macOS
# compiles it with clang, and the two do not warn about the same things. GCC's
# -Wstringop-truncation, for one, has no clang equivalent, so a truncating
# strncpy passes locally and fails the build on push.
#
# There is no host GCC here, but the Flipper toolchain ships a real one for
# ARM, and a warning from the middle end does not care what it is generating
# code for. This compiles every translation unit with it, discards the object,
# and reports what only GCC would have said. It is not a substitute for the
# real build - it cannot link or run - but it catches the class of failure
# that otherwise only appears in CI.
GCC_CHECK ?= $(HOME)/.ufbt/toolchain/current/bin/arm-none-eabi-gcc

# The object goes to /dev/null, so the dependency file would go to /dev/null.d,
# which the system will not allow. Nothing here is being kept, so drop the
# dependency generation for this pass.
GCC_CHECK_CFLAGS := $(filter-out -MMD -MP,$(CFLAGS))

gcc-check: deps
	@if [ ! -x "$(GCC_CHECK)" ]; then \
		echo "no GCC at $(GCC_CHECK); run ufbt once to deploy the toolchain,"; \
		echo "or pass GCC_CHECK=<path to a gcc>."; \
		exit 1; \
	fi
	@echo "checking $(words $(APP_SRC) $(TEST_SRC)) files with $$($(GCC_CHECK) -dumpversion)"
	@fail=0; \
	for f in $(APP_SRC) $(TEST_SRC); do \
		if ! $(GCC_CHECK) -c -o /dev/null -O1 $(GCC_CHECK_CFLAGS) -Werror "$$f" 2>/tmp/emrtd-gcc-check.log; then \
			echo "--- $$f"; cat /tmp/emrtd-gcc-check.log; fail=1; \
		fi; \
	done; \
	rm -f /tmp/emrtd-gcc-check.log; \
	if [ $$fail -ne 0 ]; then exit 1; fi; \
	echo "GCC is happy with all of them"

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf $(DEPS_DIR)
