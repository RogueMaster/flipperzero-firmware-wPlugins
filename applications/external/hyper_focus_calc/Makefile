# HyperFocus Calc — host tests + FAP via fbt.
PROJECT_NAME = hyperfocus_calc

FAP_APPID = flipper_hyper_focus_calc

FLIPPER_FIRMWARE_PATH ?= /home/<YOUR_PATH>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.
LDFLAGS = -lm

.PHONY: all help test prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Unit tests (hyperfocal + sensor CoC on host)"
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
		--suppress=unusedFunction:src/domain/hyperfocal.c \
		--suppress=unusedFunction:src/domain/sensor_data.c \
		src/domain/hyperfocal.c src/domain/sensor_data.c tests/test_hyperfocal.c

OBJS = sensor_data.o hyperfocal.o test_hyperfocal.o

test: $(OBJS)
	$(CC) $(CFLAGS) -o test_hyperfocal $(OBJS) $(LDFLAGS)
	./test_hyperfocal

sensor_data.o: src/domain/sensor_data.c include/domain/sensor_data.h
	$(CC) $(CFLAGS) -c src/domain/sensor_data.c -o sensor_data.o

hyperfocal.o: src/domain/hyperfocal.c include/domain/hyperfocal.h include/domain/sensor_data.h
	$(CC) $(CFLAGS) -c src/domain/hyperfocal.c -o hyperfocal.o

test_hyperfocal.o: tests/test_hyperfocal.c include/domain/hyperfocal.h include/domain/sensor_data.h
	$(CC) $(CFLAGS) -c tests/test_hyperfocal.c -o test_hyperfocal.o

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
	rm -f *.o tests/*.o test_hyperfocal
