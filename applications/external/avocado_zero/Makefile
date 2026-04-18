# Host tests + FAP via fbt. Symlink under applications_user (matches apps_data path).
PROJECT_NAME = avocado_zero

FAP_APPID = flipper_avocado_zero

FLIPPER_FIRMWARE_PATH ?= /home/<YOUR_PATH>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Unit tests (domain rules on host)"
	@echo "  make prepare        - Symlink app into firmware applications_user"
	@echo "  make fap            - Clean firmware build + compile .fap"
	@echo "  make format         - clang-format"
	@echo "  make linter         - cppcheck"
	@echo "  make clean          - Remove local objects"
	@echo "  make clean_firmware - rm firmware build dir"

FORMAT_FILES := $(shell git ls-files '*.c' '*.h' 2>/dev/null)
ifeq ($(strip $(FORMAT_FILES)),)
FORMAT_FILES := $(shell find . -type f \( -name '*.c' -o -name '*.h' \) ! -path './.git/*' | sort)
endif

format:
	clang-format -i $(FORMAT_FILES)

linter:
	cppcheck --enable=all --inline-suppr -I. \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction:main.c \
		src/domain/avocado_rules.c src/app/avocado_session.c \
		src/platform/feedback_helper.c src/platform/storage_helper.c \
		src/ui/play_screen.c src/ui/welcome_screen.c main.c tests/test_avocado_rules.c

test: avocado_rules.o tests/test_avocado_rules.o
	$(CC) $(CFLAGS) -o test_avocado_rules avocado_rules.o tests/test_avocado_rules.o
	./test_avocado_rules

avocado_rules.o: src/domain/avocado_rules.c include/domain/avocado_rules.h include/domain/avocado_state.h
	$(CC) $(CFLAGS) -c src/domain/avocado_rules.c -o avocado_rules.o

tests/test_avocado_rules.o: tests/test_avocado_rules.c include/domain/avocado_rules.h include/domain/avocado_state.h
	$(CC) $(CFLAGS) -c tests/test_avocado_rules.c -o tests/test_avocado_rules.o

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
		cd $(FLIPPER_FIRMWARE_PATH) && ./fbt fap_$(FAP_APPID); \
	fi

clean:
	rm -f *.o tests/*.o test_avocado_rules
