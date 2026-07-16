# Compiles the real autobaud engine for the host and runs it against synthetic
# UART waveforms. See host_autobaud_test.c for what and why.
#
#   make -C test

CC      ?= cc
CFLAGS  := -std=gnu2x -Wall -Wextra -Werror -O1 -g
CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  += -I stub -I ..

SRC := host_autobaud_test.c ../helpers/autobaud.c ../helpers/baud_table.c
BIN := host_autobaud_test

.PHONY: all run clean
all: run

$(BIN): $(SRC) stub/*.h ../helpers/autobaud.h ../helpers/baud_table.h
	$(CC) $(CFLAGS) $(SRC) -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BIN) $(BIN).dSYM
