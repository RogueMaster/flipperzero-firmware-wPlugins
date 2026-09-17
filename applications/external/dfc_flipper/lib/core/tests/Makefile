# Host test suite for the DFC core.
#
# The core is platform-neutral; port/host supplies the platform interface so a
# workstation can run the same engine the firmware runs.
#
#   make test           all unit suites
#   make test SANITIZE=address,undefined       under ASan and UBSan (gcc or clang)
#   make test CC=clang SANITIZE=memory         under MSan (clang, Linux only)
#
# Executable names do not encode the sanitizer, so run make clean
# between configurations.
#
CORE  := ../src
PORT  := ../port
HOST  := ../port/host

CFLAGS  += -std=c11 -Wall -Wextra -I$(CORE) -I$(PORT) -I$(HOST) -Imunit

TINY_AES_DIR ?= ../../tiny-aes-c
TINY_DES_DIR ?= ../../tiny-des-c

ifeq ($(wildcard $(TINY_AES_DIR)/aes.c),)
$(error tiny-aes-c not found at $(TINY_AES_DIR); clone https://github.com/mistial-dev/tiny-aes-c or set TINY_AES_DIR)
endif
ifeq ($(wildcard $(TINY_DES_DIR)/des.c),)
$(error tiny-des-c not found at $(TINY_DES_DIR); clone https://github.com/mistial-dev/tiny-des-c or set TINY_DES_DIR)
endif
CFLAGS += -I$(TINY_AES_DIR) -I$(TINY_DES_DIR) \
	-DAES128=1 -DAES192=0 -DAES256=0 -DAES_ENABLE_CBC=1 -DAES_ENABLE_CTR=0 \
	-DDES_ENABLE_ECB=1 -DDES_ENABLE_CBC=1 -DDES_ENABLE_CTR=0 -DDES_ENABLE_TDES=1
CRYPTO_SRC := $(TINY_AES_DIR)/aes.c $(TINY_DES_DIR)/des.c

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
	test_dfc_blank_card test_dfc_picc_config test_dfc_d40_capture

# Debugging tools that drive the engine from a workstation.
TOOLS := dfc_probe_apdu dfc_virtual_picc_host

# Check that each reduced build profile compiles cleanly.
PROFILES := minimal_ev1 full_ev1 full_ev2 minimal_ev3

.PHONY: test profile-builds clean

# One pattern rule serves every suite: each links the whole core, so a suite
# cannot pass by omitting the translation unit it should have exercised.
$(TESTS): %: %.c dfc_virtual_picc_test_helpers.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TOOLS): %: %.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TESTS) $(TOOLS) profile-builds
	@for t in $(TESTS); do echo "=== $$t ==="; ./$$t || exit 1; done

profile-builds:
	@echo "=== reduced profiles compile ==="
	@for p in $(PROFILES); do \
		printf "  %-14s " "$$p"; \
		$(CC) $(CFLAGS) -fsyntax-only \
			-DDFC_BUILD_PROFILE=DFC_PROFILE_$$(echo $$p | tr a-z A-Z) $(CORE_SRC) || exit 1; \
		echo ok; \
	done

clean:
	rm -rf $(TESTS) $(TOOLS) *.dSYM
