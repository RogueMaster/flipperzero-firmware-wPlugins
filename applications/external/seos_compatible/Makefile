# Host test suite.
#
#   make test-host   build and run the suite on this machine
#
# The app's own sources are compiled directly against the shims in
# lib/host_tests, so the tests exercise the same code the firmware runs.
# mbedTLS 4 dropped DES, so a 3.x install is preferred when one is present.

HOST_TESTS := lib/host_tests
MUNIT      := $(HOST_TESTS)/vendor/munit

MBEDTLS_PREFIX := $(firstword $(wildcard /opt/homebrew/opt/mbedtls@3 /usr/local/opt/mbedtls@3))

HOST_TEST_CFLAGS := -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -g -I. -I$(HOST_TESTS) -I$(MUNIT) -Ible_shared
HOST_TEST_LDFLAGS := -lmbedcrypto
ifneq ($(MBEDTLS_PREFIX),)
HOST_TEST_CFLAGS  += -I$(MBEDTLS_PREFIX)/include
HOST_TEST_LDFLAGS += -L$(MBEDTLS_PREFIX)/lib
endif

HOST_TEST_SUPPORT := \
	$(MUNIT)/munit.c \
	$(HOST_TESTS)/bit_buffer_mock.c \
	$(HOST_TESTS)/furi_hal_mock.c \
	$(HOST_TESTS)/test_helpers.c \
	$(HOST_TESTS)/allocation_counter.c \
	$(HOST_TESTS)/furi_string_mock.c \
	$(HOST_TESTS)/flipper_format_mock.c \
	$(HOST_TESTS)/keys_stub.c

HOST_TEST_SOURCES := \
	$(HOST_TESTS)/test_main.c \
	$(HOST_TESTS)/test_allocation.c \
	$(HOST_TESTS)/test_credential_file.c \
	$(HOST_TESTS)/test_tlv.c \
	$(HOST_TESTS)/test_iso14443_4.c \
	$(HOST_TESTS)/test_cmac.c \
	$(HOST_TESTS)/test_kdf.c \
	$(HOST_TESTS)/test_secure_messaging.c \
	$(HOST_TESTS)/test_large_messages.c \
	$(HOST_TESTS)/test_protocol.c \
	$(HOST_TESTS)/test_reader_parse.c \
	$(HOST_TESTS)/test_sio_collect.c \
	$(HOST_TESTS)/test_sm_command.c \
	$(HOST_TESTS)/test_write_response.c \
	$(HOST_TESTS)/test_ble_policy.c \
	$(HOST_TESTS)/test_ble_framing.c \
	$(HOST_TESTS)/test_session_vectors.c \
	$(HOST_TESTS)/test_emulated_card.c \
	$(HOST_TESTS)/test_select_adf.c

HOST_TEST_APP_SOURCES := \
	seos_tlv.c \
	seos_sio_collect.c \
	seos_credential_parse.c \
	seos_iso14443_4.c \
	cmac.c \
	seos_common.c \
	secure_messaging.c \
	seos_protocol.c \
	seos_sm_command.c \
	seos_ble_policy.c \
	ble_shared/seos_ble_framing.c \
	memmem.c

# Coverage is measured over the app's own sources only: the shims and the test
# files are scaffolding, and counting them would flatter the number.
COVERAGE_DIR := build/coverage
COVERAGE_FLAGS := -fprofile-instr-generate -fcoverage-mapping

.PHONY: test-host test-asan check-plugin-api coverage clean-host

test-host:
	@mkdir -p build/host_tests
	$(CC) $(HOST_TEST_CFLAGS) \
		$(HOST_TEST_SUPPORT) $(HOST_TEST_SOURCES) $(HOST_TEST_APP_SOURCES) \
		-o build/host_tests/seos_tests $(HOST_TEST_LDFLAGS)
	./build/host_tests/seos_tests

# The plugins resolve the app's symbols at load time through the table in
# seos_app_api_table_i.h. A symbol they call that is missing from it builds and
# links clean and fails only on the device, when the stack is first used, so
# check the two agree here.
check-plugin-api:
	@python3 -c 'import re,glob,sys;\
table=set(re.findall(r"API_(?:METHOD|VARIABLE)\(\s*(\w+)", open("seos_app_api_table_i.h").read()));\
hdrs=["seos_protocol.h","seos_sm_command.h","seos_common.h","secure_messaging.h","keys.h","seos_sm_event_ui.h"];\
decl=set();\
[decl.update(re.findall(r"^(?:extern\s+)?(?:const\s+)?[\w]+[\s\*]+(\w+)\s*\(", open(h).read(), re.M)) for h in hdrs];\
[decl.update(re.findall(r"^extern\s+(?:const\s+)?\w+\s+\**(\w+)\s*[\[;]", open(h).read(), re.M)) for h in hdrs];\
used=set();\
[used.update(re.findall(r"\b([A-Za-z_]\w*)\b", open(f).read())) for f in glob.glob("ble_*_fal/*.c")+glob.glob("ble_shared/*.c")];\
kw={"void","int","char","bool","long","short","float","double","unsigned","signed","typedef","return","if","while","for","sizeof","struct","enum","union","static","inline"};\
 missing=sorted((decl & used) - table - kw);\
sys.exit(print("Plugins call these, but the app does not export them:") or print("\n".join("  "+m for m in missing)) or 1) if missing else print("plugin API table covers every symbol the plugins call")'

# The truncation sweeps only prove a parser returns false. Under the sanitiser
# they also prove it did not read past the buffer to decide that.
test-asan:
	@mkdir -p build/host_tests
	$(CC) $(HOST_TEST_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(HOST_TEST_SUPPORT) $(HOST_TEST_SOURCES) $(HOST_TEST_APP_SOURCES) \
		-o build/host_tests/seos_tests_asan $(HOST_TEST_LDFLAGS)
	./build/host_tests/seos_tests_asan

coverage:
	@mkdir -p $(COVERAGE_DIR)
	$(CC) $(HOST_TEST_CFLAGS) $(COVERAGE_FLAGS) \
		$(HOST_TEST_SUPPORT) $(HOST_TEST_SOURCES) $(HOST_TEST_APP_SOURCES) \
		-o $(COVERAGE_DIR)/seos_tests $(HOST_TEST_LDFLAGS)
	@cd $(COVERAGE_DIR) && LLVM_PROFILE_FILE=seos.profraw ./seos_tests > /dev/null
	@xcrun llvm-profdata merge -sparse $(COVERAGE_DIR)/seos.profraw -o $(COVERAGE_DIR)/seos.profdata
	@xcrun llvm-cov report $(COVERAGE_DIR)/seos_tests \
		-instr-profile=$(COVERAGE_DIR)/seos.profdata $(HOST_TEST_APP_SOURCES)

coverage-detail:
	@xcrun llvm-cov show $(COVERAGE_DIR)/seos_tests \
		-instr-profile=$(COVERAGE_DIR)/seos.profdata $(HOST_TEST_APP_SOURCES) \
		-show-line-counts-or-regions | less -R

clean-host:
	rm -rf build/host_tests $(COVERAGE_DIR)
