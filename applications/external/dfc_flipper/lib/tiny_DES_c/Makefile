# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: Unlicense

CC ?= gcc
PYTHON ?= python3
CFLAGS ?= -Wall -Wextra -O2 -std=c99 -I.

# Product defaults: CTR + Triple-DES only
DES_ENABLE_ECB ?= 0
DES_ENABLE_CBC ?= 0
DES_ENABLE_CTR ?= 1
DES_ENABLE_OFB ?= 0
DES_ENABLE_CFB1 ?= 0
DES_ENABLE_CFB8 ?= 0
DES_ENABLE_CFB64 ?= 0
DES_ENABLE_TDES ?= 1
DES_ENABLE_CMAC ?= 0
DES_ZEROIZE ?= 1
DES_STRICT ?= 0
DES_CAVP ?= 0

MODE_DEFINITIONS = \
  -DDES_ENABLE_ECB=$(DES_ENABLE_ECB) \
  -DDES_ENABLE_CBC=$(DES_ENABLE_CBC) \
  -DDES_ENABLE_CTR=$(DES_ENABLE_CTR) \
  -DDES_ENABLE_OFB=$(DES_ENABLE_OFB) \
  -DDES_ENABLE_CFB1=$(DES_ENABLE_CFB1) \
  -DDES_ENABLE_CFB8=$(DES_ENABLE_CFB8) \
  -DDES_ENABLE_CFB64=$(DES_ENABLE_CFB64) \
  -DDES_ENABLE_TDES=$(DES_ENABLE_TDES) \
  -DDES_ENABLE_CMAC=$(DES_ENABLE_CMAC) \
  -DDES_ZEROIZE=$(DES_ZEROIZE) \
  -DDES_STRICT=$(DES_STRICT)

# Full-feature definitions for the unit-test binary (until multi-config matrix)
TEST_DEFINITIONS = \
  -DDES_ENABLE_ECB=1 \
  -DDES_ENABLE_CBC=1 \
  -DDES_ENABLE_CTR=1 \
  -DDES_ENABLE_OFB=1 \
  -DDES_ENABLE_CFB1=1 \
  -DDES_ENABLE_CFB8=1 \
  -DDES_ENABLE_CFB64=1 \
  -DDES_ENABLE_TDES=1 \
  -DDES_ENABLE_CMAC=1 \
  -DDES_ZEROIZE=$(DES_ZEROIZE) \
  -DDES_STRICT=$(DES_STRICT)

ifeq ($(DES_CAVP),1)
TEST_DEFINITIONS += -DDES_CAVP=1
TEST_OBJS_EXTRA = cavp.o
else
TEST_DEFINITIONS += -DDES_CAVP=0
TEST_OBJS_EXTRA =
endif

CAVP_CONFIG = .des_cavp_$(DES_CAVP)
TEST_BUILD_DIR ?= .tiny-des-tests

.PHONY: all clean test test-sparse size benchmark

all: des.o

des.o: des.c des.h
	$(CC) $(CFLAGS) $(MODE_DEFINITIONS) -c des.c -o $@

size: des.o
	size des.o

benchmark: benchmark.c des.c des.h
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(MODE_DEFINITIONS) -c des.c -o $(TEST_BUILD_DIR)/benchmark-des.o
	$(CC) $(CFLAGS) $(MODE_DEFINITIONS) \
	  -DBENCHMARK_BYTES=$(or $(BENCHMARK_BYTES),16384) \
	  -DBENCHMARK_ITERATIONS=$(or $(BENCHMARK_ITERATIONS),100) \
	  benchmark.c $(TEST_BUILD_DIR)/benchmark-des.o -o $(TEST_BUILD_DIR)/benchmark
	$(TEST_BUILD_DIR)/benchmark

# Unit tests always link a full-mode library object so every suite compiles.
$(TEST_BUILD_DIR)/des-test.o: des.c des.h
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DEFINITIONS) -c des.c -o $@

$(TEST_BUILD_DIR)/munit.o: munit.c munit.h
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -c munit.c -o $@

$(TEST_BUILD_DIR)/edge_vectors.o: test_edge_vectors.c des.h edge_vectors.h munit.h
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DEFINITIONS) -c test_edge_vectors.c -o $@

$(TEST_BUILD_DIR)/test.o: test.c des.h munit.h test_vectors.h $(CAVP_CONFIG)
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DEFINITIONS) -c test.c -o $@

$(CAVP_CONFIG):
	@touch $@

$(TEST_BUILD_DIR)/cavp.o: cavp.c des.h munit.h
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DEFINITIONS) -c cavp.c -o $@

# Checked-in headers are the default; regenerate only on demand (needs cryptography/OpenSSL).
.PHONY: regenerate-vectors
regenerate-vectors:
	$(PYTHON) generate_test_vectors.py
	$(PYTHON) generate_edge_vectors.py

test: $(TEST_BUILD_DIR)/des-test.o $(TEST_BUILD_DIR)/munit.o $(TEST_BUILD_DIR)/test.o $(TEST_BUILD_DIR)/edge_vectors.o
	@set -e; \
	objs="$(TEST_BUILD_DIR)/des-test.o $(TEST_BUILD_DIR)/munit.o $(TEST_BUILD_DIR)/test.o $(TEST_BUILD_DIR)/edge_vectors.o"; \
	if [ "$(DES_CAVP)" = "1" ]; then \
	  $(CC) $(CFLAGS) $(TEST_DEFINITIONS) -c cavp.c -o $(TEST_BUILD_DIR)/cavp.o; \
	  objs="$$objs $(TEST_BUILD_DIR)/cavp.o"; \
	fi; \
	$(CC) $(CFLAGS) -o $(TEST_BUILD_DIR)/test_des $$objs; \
	$(TEST_BUILD_DIR)/test_des; \
	$(MAKE) test-sparse

# Sparse configuration builds (not multiplied across a full matrix).
test-sparse:
	@set -e; \
	mkdir -p $(TEST_BUILD_DIR); \
	$(CC) $(CFLAGS) -c munit.c -o $(TEST_BUILD_DIR)/munit.o; \
	run_cfg() { \
	  name=$$1; shift; \
	  common="$(CFLAGS) $$*"; \
	  echo "=== sparse config: $$name ==="; \
	  $(CC) $${common} -c des.c -o $(TEST_BUILD_DIR)/des-cfg-$${name}.o; \
	  $(CC) $${common} -c test.c -o $(TEST_BUILD_DIR)/test-cfg-$${name}.o; \
	  $(CC) $${common} -c test_edge_vectors.c -o $(TEST_BUILD_DIR)/edge-cfg-$${name}.o; \
	  $(CC) $${common} -c cavp.c -o $(TEST_BUILD_DIR)/cavp-cfg-$${name}.o; \
	  $(CC) $(CFLAGS) -o $(TEST_BUILD_DIR)/test-cfg-$${name} \
	    $(TEST_BUILD_DIR)/des-cfg-$${name}.o $(TEST_BUILD_DIR)/test-cfg-$${name}.o \
	    $(TEST_BUILD_DIR)/edge-cfg-$${name}.o $(TEST_BUILD_DIR)/cavp-cfg-$${name}.o \
	    $(TEST_BUILD_DIR)/munit.o; \
	  $(TEST_BUILD_DIR)/test-cfg-$${name}; \
	}; \
	run_cfg default-product \
	  -DDES_ENABLE_ECB=0 -DDES_ENABLE_CBC=0 -DDES_ENABLE_CTR=1 -DDES_ENABLE_OFB=0 \
	  -DDES_ENABLE_CFB1=0 -DDES_ENABLE_CFB8=0 -DDES_ENABLE_CFB64=0 -DDES_ENABLE_TDES=1 \
	  -DDES_ENABLE_CMAC=0 -DDES_ZEROIZE=1 -DDES_STRICT=0 -DDES_CAVP=0; \
	run_cfg cmac-only \
	  -DDES_ENABLE_ECB=0 -DDES_ENABLE_CBC=0 -DDES_ENABLE_CTR=0 -DDES_ENABLE_OFB=0 \
	  -DDES_ENABLE_CFB1=0 -DDES_ENABLE_CFB8=0 -DDES_ENABLE_CFB64=0 -DDES_ENABLE_TDES=0 \
	  -DDES_ENABLE_CMAC=1 -DDES_ZEROIZE=1 -DDES_STRICT=0 -DDES_CAVP=0; \
	run_cfg strict-ctr \
	  -DDES_ENABLE_ECB=0 -DDES_ENABLE_CBC=0 -DDES_ENABLE_CTR=1 -DDES_ENABLE_OFB=0 \
	  -DDES_ENABLE_CFB1=0 -DDES_ENABLE_CFB8=0 -DDES_ENABLE_CFB64=0 -DDES_ENABLE_TDES=0 \
	  -DDES_ENABLE_CMAC=0 -DDES_ZEROIZE=1 -DDES_STRICT=1 -DDES_CAVP=0; \
	run_cfg zeroize-off \
	  -DDES_ENABLE_ECB=0 -DDES_ENABLE_CBC=0 -DDES_ENABLE_CTR=1 -DDES_ENABLE_OFB=0 \
	  -DDES_ENABLE_CFB1=0 -DDES_ENABLE_CFB8=0 -DDES_ENABLE_CFB64=0 -DDES_ENABLE_TDES=1 \
	  -DDES_ENABLE_CMAC=0 -DDES_ZEROIZE=0 -DDES_STRICT=0 -DDES_CAVP=0; \
	run_cfg ecb-cbc-single \
	  -DDES_ENABLE_ECB=1 -DDES_ENABLE_CBC=1 -DDES_ENABLE_CTR=0 -DDES_ENABLE_OFB=0 \
	  -DDES_ENABLE_CFB1=0 -DDES_ENABLE_CFB8=0 -DDES_ENABLE_CFB64=0 -DDES_ENABLE_TDES=0 \
	  -DDES_ENABLE_CMAC=0 -DDES_ZEROIZE=1 -DDES_STRICT=0 -DDES_CAVP=0

clean:
	rm -f des.o .des_cavp_*
	rm -rf $(TEST_BUILD_DIR)
