# Compiles the real engines for the host and runs them against synthetic input.
# See the individual test files for what each one covers and why.
#
#   make -C test

CC      ?= cc
CFLAGS  := -std=gnu2x -Wall -Wextra -Werror -O1 -g
CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  += -I stub -I ..

AUTOBAUD_SRC := host_autobaud_test.c ../helpers/autobaud.c ../helpers/baud_table.c
TRIGGER_SRC  := host_trigger_test.c ../helpers/trigger.c
STATS_SRC    := host_stats_test.c ../helpers/sessionstats.c

BINS := host_autobaud_test host_trigger_test host_stats_test

.PHONY: all run clean
all: run

host_autobaud_test: $(AUTOBAUD_SRC) stub/*.h ../helpers/autobaud.h ../helpers/baud_table.h
	$(CC) $(CFLAGS) $(AUTOBAUD_SRC) -o $@

host_trigger_test: $(TRIGGER_SRC) stub/*.h ../helpers/trigger.h
	$(CC) $(CFLAGS) $(TRIGGER_SRC) -o $@

host_stats_test: $(STATS_SRC) stub/*.h ../helpers/sessionstats.h
	$(CC) $(CFLAGS) $(STATS_SRC) -o $@

run: $(BINS)
	./host_autobaud_test
	./host_trigger_test
	./host_stats_test

clean:
	rm -rf $(BINS) *.dSYM
