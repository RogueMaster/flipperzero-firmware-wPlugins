# Host-side unit tests and static checks for the pure filter core.
#
# These build and run on any machine with a C compiler; no Flipper SDK required.
# The same sources (src/lib/*.c) are compiled into the Flipper app by ufbt.

CC ?= cc

# Strict warning set doubles as a linter: the pure core must compile warning-free.
WARNINGS := \
	-Wall -Wextra -Wpedantic -Werror \
	-Wshadow -Wconversion -Wsign-conversion \
	-Wcast-qual -Wcast-align -Wstrict-prototypes \
	-Wmissing-prototypes -Wold-style-definition

CFLAGS ?= -std=c11 -O2 -g
CFLAGS += $(WARNINGS) -I../src/core

CORE := ../src/core/sha1.c ../src/core/xiaomi_filter.c
SRC := $(CORE) test_xiaomi_filter.c
BIN := build/test_runner

.PHONY: test lint clean

# Build (which enforces the strict warning set) and run the tests.
test: $(BIN)
	@./$(BIN)

# Lint-only: compile every core translation unit with the strict warning set.
lint:
	@mkdir -p build
	@for f in $(CORE) test_xiaomi_filter.c; do \
		echo "lint $$f"; \
		$(CC) $(CFLAGS) -fsyntax-only $$f || exit 1; \
	done
	@echo "lint: clean"

$(BIN): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	@rm -rf build
