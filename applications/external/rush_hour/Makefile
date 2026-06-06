PROJECT_NAME = flipper_tutu
FAP_APPID = flipper_tutu

# Local override: create a gitignored `local.mk` with your real path, e.g.
#   FLIPPER_FIRMWARE_PATH = /home/you/flipperzero-firmware
# The committed default below is a placeholder on purpose — never commit a real path.
-include local.mk
FLIPPER_FIRMWARE_PATH ?= <Path>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test prepare fap clean clean_firmware format linter levels
all: test

# --- host tests ---
BOARD_OBJS = board.o test_board.o
PROGRESS_OBJS = progress.o test_progress.o
LEVELS_OBJS = levels.o board.o test_levels_data.o

test: test_board test_progress test_levels_data
	./test_board && ./test_progress && ./test_levels_data

test_board: $(BOARD_OBJS)
	$(CC) $(CFLAGS) -o test_board $(BOARD_OBJS)
test_progress: $(PROGRESS_OBJS)
	$(CC) $(CFLAGS) -o test_progress $(PROGRESS_OBJS)
test_levels_data: $(LEVELS_OBJS)
	$(CC) $(CFLAGS) -o test_levels_data $(LEVELS_OBJS)

board.o: src/domain/board.c include/domain/board.h
	$(CC) $(CFLAGS) -c src/domain/board.c -o board.o
progress.o: src/persistence/progress.c include/persistence/progress.h
	$(CC) $(CFLAGS) -c src/persistence/progress.c -o progress.o
levels.o: src/data/levels.c include/data/levels.h src/data/levels_data.h
	$(CC) $(CFLAGS) -c src/data/levels.c -o levels.o
test_board.o: tests/test_board.c
	$(CC) $(CFLAGS) -c tests/test_board.c -o test_board.o
test_progress.o: tests/test_progress.c
	$(CC) $(CFLAGS) -c tests/test_progress.c -o test_progress.o
test_levels_data.o: tests/test_levels_data.c
	$(CC) $(CFLAGS) -c tests/test_levels_data.c -o test_levels_data.o

# --- regenerate embedded bank from committed tools/levels.json ---
levels:
	node tools/gen-levels-header.mjs

# --- format / lint ---
FORMAT_FILES := $(shell git ls-files '*.c' '*.h' 2>/dev/null | grep -v 'levels_data\.h')
format:
	clang-format -i $(FORMAT_FILES)
linter:
	cppcheck --enable=all --inline-suppr -I. \
	  --suppress=missingIncludeSystem \
	  --suppress=unusedFunction:main.c \
	  --suppress=unusedFunction:src/app/tutu_app.c \
	  src/domain/board.c src/data/levels.c src/persistence/progress.c \
	  src/platform/storage_port.c src/app/tutu_app.c main.c

# --- build the .fap ---
prepare:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		mkdir -p $(FLIPPER_FIRMWARE_PATH)/applications_user; \
		ln -sfn $(PWD) $(FLIPPER_FIRMWARE_PATH)/applications_user/$(PROJECT_NAME); \
	fi
clean_firmware:
	@[ -d "$(FLIPPER_FIRMWARE_PATH)/build" ] && rm -rf $(FLIPPER_FIRMWARE_PATH)/build || true
fap: prepare clean_firmware clean
	@cd $(FLIPPER_FIRMWARE_PATH) && ./fbt fap_$(FAP_APPID)
clean:
	rm -f *.o tests/*.o test_board test_progress test_levels_data
