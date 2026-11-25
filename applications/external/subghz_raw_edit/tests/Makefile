# Host tests of the signal editing code (no Flipper SDK needed): make -C tests
CC ?= cc
CFLAGS += -std=gnu2x -g -O1 -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas -DHOST_TEST -I.
SRC = host.c test.c ../src/load.c ../src/work.c ../src/edit_ops.c ../src/view.c ../src/sel_info.c

test: run_tests
	./run_tests

run_tests: $(SRC) host.h ../src/app.h
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

clean:
	rm -rf run_tests run_tests.exe out

.PHONY: test clean
