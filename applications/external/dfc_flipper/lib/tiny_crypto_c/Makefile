# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: GPL-2.0-or-later

BUILD_DIR ?= build
CMAKE ?= cmake
CTEST ?= ctest
CMAKE_BUILD_TYPE ?= Release
ifeq ($(shell uname -s),Darwin)
# Apple's /usr/bin/gcc is a Clang driver. Prefer versioned Homebrew GCC names
# so a local multi-compiler run really exercises two compiler families.
TOOLCHAINS ?= gcc-16:g++-16 gcc-15:g++-15 gcc-14:g++-14 gcc-13:g++-13 clang:clang++
else
TOOLCHAINS ?= gcc:g++ clang:clang++
endif

# Forward every TINY_CRYPTO_* command-line variable to CMake. This keeps Make
# as a convenient frontend without maintaining a second option system.
TINY_CRYPTO_VARIABLES := $(filter TINY_CRYPTO_%,$(.VARIABLES))
TINY_CRYPTO_CACHE_ARGS := $(foreach name,$(TINY_CRYPTO_VARIABLES),-D$(name)=$($(name)))

.PHONY: all configure test test-compilers test-full test-sanitize test-sanitize-full test-msan test-msan-full test-cpp benchmark size regenerate-vectors install clean

all: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		$(TINY_CRYPTO_CACHE_ARGS) $(CMAKE_ARGS)

test: all
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

test-compilers:
	@tested=0; failed=0; \
	for pair in $(TOOLCHAINS); do \
		cc=$${pair%%:*}; cxx=$${pair#*:}; \
		if command -v $$cc >/dev/null 2>&1 && command -v $$cxx >/dev/null 2>&1; then \
			echo "Testing with $$cc / $$cxx"; \
			if ! $(MAKE) test BUILD_DIR=$(BUILD_DIR)-$$cc CC=$$cc CXX=$$cxx; then failed=1; fi; \
			tested=$$((tested + 1)); \
		else \
			echo "Skipping unavailable toolchain $$cc / $$cxx"; \
		fi; \
	done; \
	test $$tested -gt 0 && test $$failed -eq 0

test-full:
	$(MAKE) test BUILD_DIR=$(BUILD_DIR)-full TINY_CRYPTO_TEST_FULL=ON

test-sanitize:
	$(MAKE) all BUILD_DIR=$(BUILD_DIR)-sanitize \
		TINY_CRYPTO_SANITIZE=address,undefined CMAKE_BUILD_TYPE=Debug
	$(CTEST) --test-dir $(BUILD_DIR)-sanitize --output-on-failure -LE extended

test-sanitize-full:
	$(MAKE) test BUILD_DIR=$(BUILD_DIR)-sanitize-full \
		TINY_CRYPTO_SANITIZE=address,undefined CMAKE_BUILD_TYPE=Debug

# MemorySanitizer needs clang on Linux; macOS clang does not offer it.
test-msan:
	$(MAKE) all BUILD_DIR=$(BUILD_DIR)-msan \
		TINY_CRYPTO_SANITIZE=memory CMAKE_BUILD_TYPE=Debug
	$(CTEST) --test-dir $(BUILD_DIR)-msan --output-on-failure -LE extended

test-msan-full:
	$(MAKE) test BUILD_DIR=$(BUILD_DIR)-msan-full \
		TINY_CRYPTO_SANITIZE=memory CMAKE_BUILD_TYPE=Debug

test-cpp: all
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure -R test_cpp

benchmark: configure
	$(CMAKE) --build $(BUILD_DIR) --target benchmark

.PHONY: benchmark-report benchmark-report-check
benchmark-report:
	python3 tools/benchmark_report.py --build-dir $(BUILD_DIR)-benchmark-report

benchmark-report-check:
	python3 tools/benchmark_report.py --build-dir $(BUILD_DIR)-benchmark-report --check

size: configure
	$(CMAKE) --build $(BUILD_DIR) --target size

regenerate-vectors:
	python3 tools/generate_hash_vectors.py
	python3 tools/generate_des_vectors.py
	python3 tools/generate_des_edge_vectors.py
	python3 tools/generate_kdf_vectors.py

install: all
	$(CMAKE) --install $(BUILD_DIR) $(INSTALL_ARGS)

clean:
	$(CMAKE) -E remove_directory $(BUILD_DIR)
