# Host-side unit tests for Flipper Recon pure-logic modules.
# Run with: make -C test

CC ?= cc
CFLAGS ?= -I. -std=gnu11 -Wall -Wextra -g -O1

SRC = \
	../models/recon_types.c \
	../models/asset.c \
	../models/evidence.c \
	../models/relation.c \
	../models/session.c \
	../modules/asset_manager.c \
	../modules/graph_engine.c \
	test_recon.c

BIN = recon_tests

.PHONY: test clean
test: $(BIN)
	./$(BIN)

$(BIN): $(SRC) furi.h furi_hal_rtc.h
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	rm -f $(BIN)
