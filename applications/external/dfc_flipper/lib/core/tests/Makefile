# Host test suite for the DFC core.
#
# The core is platform-neutral; port/host supplies the platform interface so a
# workstation can run the same engine the firmware runs.
#
#   make test           all unit suites with the selected crypto backend
#   make test-backends  all unit suites with mbedTLS, then tiny_crypto_c
#   make test SANITIZE=address,undefined       under ASan and UBSan (gcc or clang)
#   make test CC=clang SANITIZE=memory         under MSan (clang, Linux only)
#   make test RELEASE=1 optimised with NDEBUG, as a shipped library is built
#
# Executable names do not encode the sanitizer, so run make clean
# between configurations.
#
CORE  := ../src
PORT  := ../port
HOST  := ../port/host

CFLAGS  += -std=c11 -Wall -Wextra -I$(CORE) -I$(PORT) -I$(HOST) -Imunit

# A release build compiles assertions out. The suite must pass either way, so
# no assertion may carry an effect the engine depends on.
RELEASE ?=
ifneq ($(strip $(RELEASE)),)
CFLAGS  += -DNDEBUG -O2
endif

CRYPTO_BACKEND  ?= mbedtls
TINY_CRYPTO_DIR ?= ../../tiny_crypto_c

ifeq ($(CRYPTO_BACKEND),mbedtls)
CFLAGS  += -DDFC_CRYPTO_BACKEND_MBEDTLS=1
LDFLAGS += -lmbedcrypto
MBEDTLS_PREFIX := $(firstword $(wildcard /opt/homebrew/opt/mbedtls@3 /usr/local/opt/mbedtls@3))
ifneq ($(MBEDTLS_PREFIX),)
CFLAGS  += -I$(MBEDTLS_PREFIX)/include
LDFLAGS += -L$(MBEDTLS_PREFIX)/lib
endif
else ifeq ($(CRYPTO_BACKEND),tiny)
ifeq ($(wildcard $(TINY_CRYPTO_DIR)/src/common.c),)
$(error tiny_crypto_c not found at $(TINY_CRYPTO_DIR); set TINY_CRYPTO_DIR)
endif
CFLAGS += -DDFC_CRYPTO_BACKEND_TINY=1 -I$(TINY_CRYPTO_DIR)/src \
	-DTC_ENABLE_AES=1 -DTC_AES_ENABLE_CBC=1 -DTC_AES_ENABLE_CTR=0 \
	-DTC_ENABLE_DES=1 -DTC_DES_ENABLE_ECB=1 -DTC_DES_ENABLE_CBC=1 -DTC_DES_ENABLE_CTR=0 \
	-DTC_DES_ENABLE_TDES=1 -DTC_ENABLE_SHA256=0
CRYPTO_SRC := $(TINY_CRYPTO_DIR)/src/aes.c $(TINY_CRYPTO_DIR)/src/des.c $(TINY_CRYPTO_DIR)/src/common.c
else
$(error CRYPTO_BACKEND must be 'mbedtls' or 'tiny')
endif

# Sanitizers: empty (off), "address,undefined" (gcc or clang), or "memory"
# (clang on Linux). munit forks each case, so a sanitizer abort fails that
# case and its report is printed with the failure.
SANITIZE ?=
ifneq ($(strip $(SANITIZE)),)
SAN_FLAGS := -fsanitize=$(SANITIZE) -fno-omit-frame-pointer -g -O1
ifneq ($(findstring undefined,$(SANITIZE)),)
SAN_FLAGS += -fno-sanitize-recover=all
endif
ifneq ($(findstring memory,$(SANITIZE)),)
SAN_FLAGS += -fsanitize-memory-track-origins
endif
CFLAGS  += $(SAN_FLAGS)
LDFLAGS += -fsanitize=$(SANITIZE)
endif

CORE_SRC := $(wildcard $(CORE)/*.c)
HOST_SRC := $(HOST)/dfc_port_host.c $(HOST)/dfc_bytebuf_host.c
COMMON   := $(CORE_SRC) $(HOST_SRC) $(CRYPTO_SRC) munit/munit.c

TESTS := test_dfc_crypto test_cmac test_dfc_session test_dfc_secure_messaging test_dfc_desfire_protocol \
	test_dfc_virtual_picc_core test_dfc_virtual_picc_secure \
	test_dfc_virtual_picc_probe_vectors test_dfc_virtual_picc_lifecycle_vectors \
	test_dfc_virtual_picc_mutation_vectors test_dfc_virtual_picc_multi_app_vectors \
	test_dfc_virtual_picc_value_vectors test_dfc_ev1_conformance \
	test_dfc_file_pool test_dfc_key_pool test_dfc_der test_dfc_profile test_dfc_text \
	test_dfc_examples \
	test_dfc_blank_card test_dfc_picc_config test_dfc_d40_capture \
	test_dfc_command test_dfc_reader

TESTS += test_dfc_ev2_capabilities test_dfc_static_signature

# Debugging tools that drive the engine from a workstation.
TOOLS := dfc_probe_apdu dfc_virtual_picc_host

# Check that each reduced build profile compiles cleanly.
PROFILES := minimal_ev1 full_ev1 full_ev2 minimal_ev3

# Link each build role under every profile, then prove from the symbol table
# that a role carries none of the parts it omits.
ROLES := target host simulator
ROLE_PROFILES := $(PROFILES) full_ev3
ROLE_DIR := role-build
ROLE_FORBIDDEN_target := dfc_text_ dfc_reader_ dfc_command_
ROLE_FORBIDDEN_host := dfc_emulator_ dfc_virtual_picc_
ROLE_FORBIDDEN_simulator :=

.PHONY: test test-backends test-ev2-profile test-auth-profile profile-builds role-builds clean

# One pattern rule serves every suite: each links the whole core, so a suite
# cannot pass by omitting the translation unit it should have exercised.
$(TESTS): %: %.c dfc_virtual_picc_test_helpers.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TOOLS): %: %.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TESTS) $(TOOLS) profile-builds role-builds test-ev2-profile test-auth-profile
	@for t in $(TESTS); do echo "=== $$t ==="; ./$$t || exit 1; done

test_dfc_static_signature_ev2: test_dfc_static_signature.c dfc_virtual_picc_test_helpers.c $(COMMON)
	$(CC) $(CFLAGS) -DDFC_BUILD_PROFILE=DFC_PROFILE_FULL_EV2 -o $@ $^ $(LDFLAGS)

test-ev2-profile: test_dfc_static_signature_ev2
	./test_dfc_static_signature_ev2

test_dfc_der_no_iso: test_dfc_der.c dfc_virtual_picc_test_helpers.c $(COMMON)
	$(CC) $(CFLAGS) -DDFC_ENABLE_ISO7816_AUTH=0 -o $@ $^ $(LDFLAGS)

test-auth-profile: test_dfc_der_no_iso
	./test_dfc_der_no_iso /dfc_der/model-validation

test-backends:
	@$(MAKE) clean
	@$(MAKE) test CRYPTO_BACKEND=mbedtls
	@$(MAKE) clean
	@$(MAKE) test CRYPTO_BACKEND=tiny

profile-builds:
	@echo "=== reduced profiles compile ==="
	@for p in $(PROFILES); do \
		printf "  %-14s " "$$p"; \
		$(CC) $(CFLAGS) -fsyntax-only \
			-DDFC_BUILD_PROFILE=DFC_PROFILE_$$(echo $$p | tr a-z A-Z) $(CORE_SRC) || exit 1; \
		echo ok; \
	done

role-builds:
	@echo "=== build roles link ==="
	@for r in $(ROLES); do for p in $(ROLE_PROFILES); do \
		printf "  %-10s %-14s " "$$r" "$$p"; \
		d=$(ROLE_DIR)/$$r-$$p; rm -rf $$d; mkdir -p $$d; \
		flags="-DDFC_BUILD_ROLE=DFC_ROLE_$$(echo $$r | tr a-z A-Z) -DDFC_BUILD_PROFILE=DFC_PROFILE_$$(echo $$p | tr a-z A-Z)"; \
		for src in $(CORE_SRC); do \
			$(CC) $(CFLAGS) $$flags -c $$src -o $$d/$$(basename $$src .c).o || exit 1; \
		done; \
		$(CC) $(CFLAGS) $$flags -o $$d/smoke dfc_role_smoke.c $$d/*.o $(HOST_SRC) $(CRYPTO_SRC) $(LDFLAGS) || exit 1; \
		case $$r in target) forbidden="$(ROLE_FORBIDDEN_target)";; host) forbidden="$(ROLE_FORBIDDEN_host)";; *) forbidden="$(ROLE_FORBIDDEN_simulator)";; esac; \
		for sym in $$forbidden; do \
			if nm -g $$d/*.o | grep -E " T _?$$sym" >/dev/null; then \
				echo "carries $$sym"; exit 1; \
			fi; \
		done; \
		./$$d/smoke || exit 1; \
		echo "ok ($$(cat $$d/*.o | wc -c | tr -d ' ') object bytes)"; \
	done; done

clean:
	rm -rf $(TESTS) $(TOOLS) test_dfc_static_signature_ev2 test_dfc_der_no_iso *.dSYM $(ROLE_DIR)
