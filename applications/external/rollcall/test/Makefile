# Compiles the real logic files for the host and runs them against synthetic
# input - no Flipper, no radio. See each test file for what it covers and why.
#
#   make -C test

CC     ?= cc
CFLAGS := -std=gnu2x -Wall -Wextra -Werror -O1 -g
CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS += -I stub -I ..

ANALYZER_SRC := host_analyzer_test.c ../helpers/analyzer.c
PARSE_SRC    := host_parse_test.c ../helpers/rc_parse.c

BINS := host_analyzer_test host_parse_test

.PHONY: all run clean
all: run

host_analyzer_test: $(ANALYZER_SRC) stub/*.h ../helpers/analyzer.h ../helpers/rc_radio.h
	$(CC) $(CFLAGS) $(ANALYZER_SRC) -o $@

host_parse_test: $(PARSE_SRC) ../helpers/rc_parse.h
	$(CC) $(CFLAGS) $(PARSE_SRC) -o $@

run: $(BINS)
	./host_analyzer_test
	@echo
	./host_parse_test

clean:
	rm -rf $(BINS) *.dSYM
