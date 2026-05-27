# NFC Stock Manager — host tests (domain + persistence + platform mocks)
PROJECT_NAME = nfc_stock_manager

FLIPPER_FIRMWARE_PATH ?= /home/<YOUR_PATH>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I. -DHOST_BUILD

SRC_DOMAIN = src/domain/stock.c
SRC_PERSIST = src/persistence/db.c src/persistence/fs_compat.c \
	src/persistence/stock_file.c
SRC_PLATFORM = src/platform/storage_helper.c

OBJS = stock.o stock_file.o db.o fs_compat.o storage_helper.o test_stock.o

.PHONY: all help test prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Unit tests (host)"
	@echo "  make prepare        - Symlink app into firmware applications_user"
	@echo "  make fap            - Clean firmware build + compile .fap"
	@echo "  make format         - clang-format tracked C/H"
	@echo "  make linter         - cppcheck"
	@echo "  make clean          - Remove local objects"
	@echo "  make clean_firmware - rm firmware build dir"

# Prefer tracked files in CI; locally use find if not in a git checkout.
FORMAT_FILES := $(shell git ls-files '*.c' '*.h' 2>/dev/null)
ifeq ($(strip $(FORMAT_FILES)),)
FORMAT_FILES := $(shell find . -type f \( -name '*.c' -o -name '*.h' \) ! -path './.git/*' | sort)
endif

format:
	clang-format -i $(FORMAT_FILES)

linter:
	cppcheck --enable=all -I. \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction:main.c \
		--suppress=constParameterCallback:src/scenes.c \
		.

test: $(OBJS)
	$(CC) $(CFLAGS) -o test_logic $(OBJS)
	./test_logic

stock.o: $(SRC_DOMAIN) include/stock.h include/clock.h
	$(CC) $(CFLAGS) -c $(SRC_DOMAIN) -o stock.o

stock_file.o: src/persistence/stock_file.c include/stock.h include/fs_compat.h
	$(CC) $(CFLAGS) -c src/persistence/stock_file.c -o stock_file.o

db.o: src/persistence/db.c include/db.h include/fs_compat.h include/stock.h
	$(CC) $(CFLAGS) -c src/persistence/db.c -o db.o

fs_compat.o: src/persistence/fs_compat.c include/fs_compat.h include/stock.h \
	include/clock.h
	$(CC) $(CFLAGS) -c src/persistence/fs_compat.c -o fs_compat.o

storage_helper.o: $(SRC_PLATFORM) include/storage_helper.h
	$(CC) $(CFLAGS) -c $(SRC_PLATFORM) -o storage_helper.o

test_stock.o: tests/test_stock.c include/stock.h include/storage_helper.h \
	include/db.h
	$(CC) $(CFLAGS) -c tests/test_stock.c -o test_stock.o

prepare:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		mkdir -p $(FLIPPER_FIRMWARE_PATH)/applications_user; \
		ln -sfn $(PWD) $(FLIPPER_FIRMWARE_PATH)/applications_user/$(PROJECT_NAME); \
		echo "Linked to $(FLIPPER_FIRMWARE_PATH)/applications_user/$(PROJECT_NAME)"; \
	else \
		echo "Firmware not found at $(FLIPPER_FIRMWARE_PATH)"; \
	fi

clean_firmware:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)/build" ]; then \
		rm -rf $(FLIPPER_FIRMWARE_PATH)/build; \
	fi

fap: prepare clean_firmware clean
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		cd $(FLIPPER_FIRMWARE_PATH) && ./fbt fap_nfc_stock_manager; \
	fi

clean:
	rm -f *.o test_logic
