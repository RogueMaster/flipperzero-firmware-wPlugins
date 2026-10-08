# Differential test against RPCS3's own Infinity code.
# Needs: a checkout of RPCS3 (sparse is fine), g++, gcc, libssl-dev.
#   make RPCS3=/path/to/rpcs3
RPCS3 ?= $(error set RPCS3=/path/to/rpcs3-checkout)
SRC := ..
CFLAGS := -std=c11 -O1 -fsanitize=address,undefined -I$(SRC)

all: build/diff_test
	./build/diff_test

ref_generated.inc: extract_ref.py
	python3 extract_ref.py $(RPCS3) $@

build/%.o: $(SRC)/%.c
	mkdir -p build
	gcc $(CFLAGS) -c $< -o $@

build/diff_test: diff_test.cpp ref_generated.inc build/inf_crypto.o build/infinity_core.o build/infinity_figures.o
	g++ -std=c++20 -O1 -fsanitize=address,undefined -I. -I$(SRC) diff_test.cpp build/*.o -lcrypto -o $@
