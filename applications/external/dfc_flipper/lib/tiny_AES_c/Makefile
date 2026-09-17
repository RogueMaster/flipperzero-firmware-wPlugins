# SPDX-FileCopyrightText: kokke
# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: Unlicense

CC ?= cc
CFLAGS ?= -Wall -Wextra -O2 -std=c99 -I.

AES_SBOX_MODE ?= secure
AES_WIDE_OPS ?= off
AES_ENABLE_CBC ?= 0
AES_ENABLE_ECB ?= 0
AES_ENABLE_CTR ?= 1
AES_ENABLE_OFB ?= 0
AES_ENABLE_GCM ?= 0
AES_ENABLE_CCM ?= 0
AES_ENABLE_EAX ?= 0
AES_ENABLE_EAX_PRIME ?= 0
AES_ENABLE_SIV ?= 0
AES_ENABLE_CMAC ?= 0
AES_CAVP ?= 0
AES_ZEROIZE ?= 1
AES_STRICT ?= 0
AES_TINY ?= 0
AES_GCM_GHASH_MODE ?= auto
BENCHMARK_BYTES ?= 16384
BENCHMARK_ITERATIONS ?= 100
TEST_BUILD_DIR ?= .tiny-aes-tests

ifeq ($(AES_SBOX_MODE),secure)
SBOX_DEFINITION = -DAES_SBOX_MODE=1
else ifeq ($(AES_SBOX_MODE),runtime)
SBOX_DEFINITION = -DAES_SBOX_MODE=2
else ifeq ($(AES_SBOX_MODE),fast)
SBOX_DEFINITION = -DAES_SBOX_MODE=3
else
$(error AES_SBOX_MODE must be secure, runtime, or fast)
endif

ifeq ($(AES_WIDE_OPS),off)
WIDE_DEFINITION = -DAES_WIDE_OPS=0
else ifeq ($(AES_WIDE_OPS),auto)
WIDE_DEFINITION = -DAES_WIDE_OPS=1
else
$(error AES_WIDE_OPS must be off or auto)
endif

MODE_DEFINITIONS = -DAES_ENABLE_CBC=$(AES_ENABLE_CBC) -DAES_ENABLE_ECB=$(AES_ENABLE_ECB) \
  -DAES_ENABLE_CTR=$(AES_ENABLE_CTR) -DAES_ENABLE_OFB=$(AES_ENABLE_OFB) -DAES_ENABLE_GCM=$(AES_ENABLE_GCM) \
  -DAES_ENABLE_CCM=$(AES_ENABLE_CCM) -DAES_ENABLE_EAX=$(AES_ENABLE_EAX) -DAES_ENABLE_EAX_PRIME=$(AES_ENABLE_EAX_PRIME) \
  -DAES_ENABLE_SIV=$(AES_ENABLE_SIV) -DAES_ENABLE_CMAC=$(AES_ENABLE_CMAC)
ifeq ($(AES_GCM_GHASH_MODE),auto)
GHASH_DEFINITION = -DAES_GCM_GHASH_MODE=0
GHASH_MODE = 0
else ifeq ($(AES_GCM_GHASH_MODE),bitwise)
GHASH_DEFINITION = -DAES_GCM_GHASH_MODE=1
GHASH_MODE = 1
else ifeq ($(AES_GCM_GHASH_MODE),wide)
GHASH_DEFINITION = -DAES_GCM_GHASH_MODE=2
GHASH_MODE = 2
else ifeq ($(AES_GCM_GHASH_MODE),table4)
GHASH_DEFINITION = -DAES_GCM_GHASH_MODE=3
GHASH_MODE = 3
else ifeq ($(AES_GCM_GHASH_MODE),fast-table)
GHASH_DEFINITION = -DAES_GCM_GHASH_MODE=4
GHASH_MODE = 4
else ifeq ($(AES_GCM_GHASH_MODE),hardware)
GHASH_DEFINITION = -DAES_GCM_GHASH_MODE=5
GHASH_MODE = 5
else
$(error AES_GCM_GHASH_MODE must be auto, bitwise, wide, table4, fast-table, or hardware)
endif
CONFIG_DEFINITIONS = $(MODE_DEFINITIONS) $(SBOX_DEFINITION) $(WIDE_DEFINITION) \
  $(GHASH_DEFINITION) -DAES_ZEROIZE=$(AES_ZEROIZE) -DAES_STRICT=$(AES_STRICT) \
  -DAES_TINY=$(AES_TINY)

CXX ?= c++
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++11 -I.

.PHONY: all clean size test benchmark test-cpp

all: aes.o

aes.o: aes.c aes.h
	$(CC) $(CFLAGS) $(CONFIG_DEFINITIONS) -c aes.c -o $@

size: aes.o
	size aes.o

benchmark: benchmark.c aes.c aes.h
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -DAES_ENABLE_CBC=0 -DAES_ENABLE_ECB=0 -DAES_ENABLE_CTR=0 -DAES_ENABLE_OFB=0 -DAES_ENABLE_GCM=1 -DAES128=1 -DAES_GCM_GHASH_MODE=$(GHASH_MODE) \
		-c aes.c -o $(TEST_BUILD_DIR)/benchmark-aes.o
	$(CC) $(CFLAGS) -DAES_ENABLE_CBC=0 -DAES_ENABLE_ECB=0 -DAES_ENABLE_CTR=0 -DAES_ENABLE_OFB=0 -DAES_ENABLE_GCM=1 -DAES128=1 -DAES_GCM_GHASH_MODE=$(GHASH_MODE) \
		-DBENCHMARK_BYTES=$(BENCHMARK_BYTES) -DBENCHMARK_ITERATIONS=$(BENCHMARK_ITERATIONS) \
		benchmark.c $(TEST_BUILD_DIR)/benchmark-aes.o -o $(TEST_BUILD_DIR)/benchmark
	$(TEST_BUILD_DIR)/benchmark

test-cpp: test_cpp.cpp aes.c aes.h aes.hpp
	mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -DAES_ENABLE_CBC=1 -DAES_ENABLE_ECB=1 -DAES_ENABLE_CTR=1 -DAES_ENABLE_OFB=1 -DAES_ENABLE_GCM=1 -DAES_ENABLE_CMAC=1 -DAES128=1 -c aes.c -o $(TEST_BUILD_DIR)/aes-cpp-core.o
	$(CXX) $(CXXFLAGS) -DAES_ENABLE_CBC=1 -DAES_ENABLE_ECB=1 -DAES_ENABLE_CTR=1 -DAES_ENABLE_OFB=1 -DAES_ENABLE_GCM=1 -DAES_ENABLE_CMAC=1 -DAES128=1 test_cpp.cpp $(TEST_BUILD_DIR)/aes-cpp-core.o -o $(TEST_BUILD_DIR)/test_cpp
	$(TEST_BUILD_DIR)/test_cpp

test: test-cpp
	@set -e; \
	mkdir -p $(TEST_BUILD_DIR); \
	$(CC) $(CFLAGS) -UAES_ENABLE_CBC -UAES_ENABLE_ECB -UAES_ENABLE_CTR -UAES_ENABLE_OFB -UAES_ENABLE_GCM -UAES_ENABLE_CCM -UAES_ENABLE_EAX -UAES_ENABLE_EAX_PRIME -UAES_ENABLE_SIV -UAES_ENABLE_CMAC -UAES_CAVP -UAES_GCM_GHASH_MODE -c munit.c -o $(TEST_BUILD_DIR)/munit.o; \
	build_and_run() { \
	  key=$$1; mode=$$2; cbc=$$3; ecb=$$4; ctr=$$5; ofb=$$6; gcm=$$7; ccm=$$8; eax=$$9; ghash=$${10}; sbox=$${11}; wide=$${12}; \
	  prime=0; siv=0; cmac=0; \
	  if [ "$${mode}" = eax-prime ]; then prime=1; fi; \
	  if [ "$${mode}" = siv ]; then siv=1; fi; \
	  if [ "$${mode}" = cmac ]; then cmac=1; fi; \
	  if [ "$${mode}" = all ]; then siv=1; cmac=1; fi; \
	  name=$${key}-$${mode}-gcm$${gcm}-ccm$${ccm}-eax$${eax}-eaxprime$${prime}-siv$${siv}-cmac$${cmac}-ghash$${ghash}-sbox$${sbox}-wide$${wide}-cavp$(AES_CAVP); \
	  common="$(CFLAGS) -DAES_ENABLE_CBC=$${cbc} -DAES_ENABLE_ECB=$${ecb} -DAES_ENABLE_CTR=$${ctr} -DAES_ENABLE_OFB=$${ofb} -DAES_ENABLE_GCM=$${gcm} -DAES_ENABLE_CCM=$${ccm} -DAES_ENABLE_EAX=$${eax} -DAES_ENABLE_EAX_PRIME=$${prime} -DAES_ENABLE_SIV=$${siv} -DAES_ENABLE_CMAC=$${cmac} -DAES_CAVP=$(AES_CAVP) -DAES$${key}=1 -DAES_SBOX_MODE=$${sbox} -DAES_WIDE_OPS=$${wide} -DAES_GCM_GHASH_MODE=$${ghash} -DCAVP_VECTOR_DIR=\"test_vectors/cavp\" -DEAX_VECTOR_FILE=\"test_vectors/eax/aes_eax_test.json\" -DSIV_VECTOR_FILE=\"test_vectors/siv/aead_aes_siv_cmac_test.json\" -DCMAC_WYCHEPROOF_FILE=\"test_vectors/cmac/aes_cmac_test.json\" -DCMAC_CAVP_DIR=\"test_vectors/cmac\""; \
	  if [ "$${cmac}" = 1 ]; then common="$${common} -DAES_CMAC_MIN_TAG_LEN=4"; fi; \
	  if [ "$${ghash}" = 5 ]; then common="$${common} -DAES_GCM_GHASH_HARDWARE_MULTIPLY=AES_CAVP_GHASH_HARDWARE_MULTIPLY"; fi; \
	  $(CC) $${common} -c aes.c -o $(TEST_BUILD_DIR)/aes-$${name}.o; \
	  $(CC) $${common} -c test.c -o $(TEST_BUILD_DIR)/test-$${name}.o; \
	  $(CC) $${common} -c cavp.c -o $(TEST_BUILD_DIR)/cavp-$${name}.o; \
	  $(CC) $${common} -c eax_test.c -o $(TEST_BUILD_DIR)/eax-test-$${name}.o; \
	  $(CC) $${common} -c siv_test.c -o $(TEST_BUILD_DIR)/siv-test-$${name}.o; \
	  $(CC) $${common} -c cmac_test.c -o $(TEST_BUILD_DIR)/cmac-test-$${name}.o; \
	  $(CC) $(CFLAGS) -o $(TEST_BUILD_DIR)/test-$${name} $(TEST_BUILD_DIR)/aes-$${name}.o $(TEST_BUILD_DIR)/test-$${name}.o $(TEST_BUILD_DIR)/cavp-$${name}.o $(TEST_BUILD_DIR)/eax-test-$${name}.o $(TEST_BUILD_DIR)/siv-test-$${name}.o $(TEST_BUILD_DIR)/cmac-test-$${name}.o $(TEST_BUILD_DIR)/munit.o; \
	  $(TEST_BUILD_DIR)/test-$${name}; \
	}; \
	for key in 128 192 256; do \
	  for mode in ecb cbc ctr ofb; do \
	    case $$mode in ecb) cbc=0; ecb=1; ctr=0; ofb=0;; cbc) cbc=1; ecb=0; ctr=0; ofb=0;; ctr) cbc=0; ecb=0; ctr=1; ofb=0;; ofb) cbc=0; ecb=0; ctr=0; ofb=1;; esac; \
	    for profile in "1 0" "2 0" "3 0" "1 1"; do set -- $$profile; build_and_run $$key $$mode $$cbc $$ecb $$ctr $$ofb 0 0 0 0 $$1 $$2; done; \
	  done; \
	  for profile in "1 0" "2 0" "3 0" "1 1"; do set -- $$profile; build_and_run $$key ccm 0 0 0 0 0 1 0 0 $$1 $$2; done; \
	  for profile in "1 0 0" "1 1 0" "1 0 1" "1 1 2" "1 0 3" "1 0 4" "1 0 5" "2 0 0" "3 0 0"; do set -- $$profile; build_and_run $$key gcm 0 0 0 0 1 0 0 $$3 $$1 $$2; done; \
	  for sbox in 1 2 3; do build_and_run $$key eax 0 0 0 0 0 0 1 0 $$sbox 0; done; \
	  if [ "$$key" -eq 128 ]; then for sbox in 1 2 3; do build_and_run $$key eax-prime 0 0 0 0 0 0 0 0 $$sbox 0; done; fi; \
	  for sbox in 1 2 3; do build_and_run $$key siv 0 0 0 0 0 0 0 0 $$sbox 0; done; \
	  for sbox in 1 2 3; do build_and_run $$key cmac 0 0 0 0 0 0 0 0 $$sbox 0; done; \
	  if [ "$(AES_CAVP)" -eq 0 ]; then build_and_run $$key all 1 1 1 1 1 1 1 0 1 1; fi; \
	done; \
	# Sparse config-profile builds (not multiplied across the mode matrix). \
	$(CC) $(CFLAGS) -UAES_ENABLE_CBC -UAES_ENABLE_ECB -UAES_ENABLE_CTR -UAES_ENABLE_OFB -UAES_ENABLE_GCM -UAES_ENABLE_CCM -UAES_ENABLE_EAX -UAES_ENABLE_EAX_PRIME -UAES_ENABLE_SIV -UAES_ENABLE_CMAC -c munit.c -o $(TEST_BUILD_DIR)/munit.o; \
	run_cfg() { \
	  name=$$1; shift; \
	  common="$(CFLAGS) $$* -DCAVP_VECTOR_DIR=\"test_vectors/cavp\" -DEAX_VECTOR_FILE=\"test_vectors/eax/aes_eax_test.json\" -DSIV_VECTOR_FILE=\"test_vectors/siv/aead_aes_siv_cmac_test.json\" -DCMAC_WYCHEPROOF_FILE=\"test_vectors/cmac/aes_cmac_test.json\" -DCMAC_CAVP_DIR=\"test_vectors/cmac\""; \
	  $(CC) $${common} -c aes.c -o $(TEST_BUILD_DIR)/aes-cfg-$${name}.o; \
	  $(CC) $${common} -c test.c -o $(TEST_BUILD_DIR)/test-cfg-$${name}.o; \
	  $(CC) $${common} -c cavp.c -o $(TEST_BUILD_DIR)/cavp-cfg-$${name}.o; \
	  $(CC) $${common} -c eax_test.c -o $(TEST_BUILD_DIR)/eax-cfg-$${name}.o; \
	  $(CC) $${common} -c siv_test.c -o $(TEST_BUILD_DIR)/siv-cfg-$${name}.o; \
	  $(CC) $${common} -c cmac_test.c -o $(TEST_BUILD_DIR)/cmac-cfg-$${name}.o; \
	  $(CC) $(CFLAGS) -o $(TEST_BUILD_DIR)/test-cfg-$${name} $(TEST_BUILD_DIR)/aes-cfg-$${name}.o $(TEST_BUILD_DIR)/test-cfg-$${name}.o $(TEST_BUILD_DIR)/cavp-cfg-$${name}.o $(TEST_BUILD_DIR)/eax-cfg-$${name}.o $(TEST_BUILD_DIR)/siv-cfg-$${name}.o $(TEST_BUILD_DIR)/cmac-cfg-$${name}.o $(TEST_BUILD_DIR)/munit.o; \
	  $(TEST_BUILD_DIR)/test-cfg-$${name}; \
	}; \
	run_cfg strict-ctr -DAES_ENABLE_CBC=0 -DAES_ENABLE_ECB=0 -DAES_ENABLE_CTR=1 -DAES_ENABLE_OFB=0 -DAES_ENABLE_GCM=0 -DAES_ENABLE_CCM=0 -DAES_ENABLE_EAX=0 -DAES_ENABLE_EAX_PRIME=0 -DAES_ENABLE_SIV=0 -DAES_ENABLE_CMAC=0 -DAES128=1 -DAES_STRICT=1 -DAES_ZEROIZE=1 -DAES_SBOX_MODE=1 -DAES_WIDE_OPS=0; \
	run_cfg zeroize-off -DAES_ENABLE_CBC=0 -DAES_ENABLE_ECB=0 -DAES_ENABLE_CTR=1 -DAES_ENABLE_OFB=0 -DAES_ENABLE_GCM=0 -DAES_ENABLE_CCM=0 -DAES_ENABLE_EAX=0 -DAES_ENABLE_EAX_PRIME=0 -DAES_ENABLE_SIV=0 -DAES_ENABLE_CMAC=0 -DAES128=1 -DAES_STRICT=0 -DAES_ZEROIZE=0 -DAES_SBOX_MODE=1 -DAES_WIDE_OPS=0; \
	run_cfg tiny-gcm -DAES_ENABLE_CBC=0 -DAES_ENABLE_ECB=0 -DAES_ENABLE_CTR=0 -DAES_ENABLE_OFB=0 -DAES_ENABLE_GCM=1 -DAES_ENABLE_CCM=0 -DAES_ENABLE_EAX=0 -DAES_ENABLE_EAX_PRIME=0 -DAES_ENABLE_SIV=0 -DAES_ENABLE_CMAC=0 -DAES128=1 -DAES_TINY=1 -DAES_GCM_GHASH_MODE=0 -DAES_SBOX_MODE=1 -DAES_WIDE_OPS=0; \
	run_cfg multi-gcm -DAES_ENABLE_CBC=0 -DAES_ENABLE_ECB=0 -DAES_ENABLE_CTR=0 -DAES_ENABLE_OFB=0 -DAES_ENABLE_GCM=1 -DAES_ENABLE_CCM=0 -DAES_ENABLE_EAX=0 -DAES_ENABLE_EAX_PRIME=0 -DAES_ENABLE_SIV=0 -DAES_ENABLE_CMAC=0 -DAES128=1 -DAES_GCM_GHASH_MODE=0 -DAES_SBOX_MODE=1 -DAES_WIDE_OPS=0 -DGCM_MULTI_KEY_TEST=1

clean:
	rm -f aes.o
	rm -rf $(TEST_BUILD_DIR)
