# Path to your Flipper Zero firmware directory
FLIPPER_FIRMWARE_PATH ?= /home/<YOUR_PATH>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Available commands:"
	@echo "  make test           - Run unit tests for the logic"
	@echo "  make prepare        - Link this app to the Flipper firmware source"
	@echo "  make fap            - Clean firmware build and build the Flipper Zero application"
	@echo "  make format         - Run clang-format to format code"
	@echo "  make linter         - Run cppcheck to analyze code"
	@echo "  make clean          - Remove local compiled files"
	@echo "  make clean_firmware - Remove firmware build directory"

format:
	clang-format -i *.c *.h tests/*.c

linter:
	cppcheck --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction --suppress=missingInclude .

test: logic.o tests/test_logic.o
	$(CC) $(CFLAGS) -o test_logic logic.o tests/test_logic.o
	./test_logic

prepare:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		mkdir -p $(FLIPPER_FIRMWARE_PATH)/applications_user; \
		ln -sfn $(PWD) $(FLIPPER_FIRMWARE_PATH)/applications_user/sub_duplicate_finder; \
		echo "App linked to $(FLIPPER_FIRMWARE_PATH)/applications_user/sub_duplicate_finder"; \
	else \
		echo "Error: Flipper firmware path not found at $(FLIPPER_FIRMWARE_PATH)."; \
	fi

clean_firmware:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)/build" ]; then \
		echo "Cleaning firmware build directory..."; \
		rm -rf $(FLIPPER_FIRMWARE_PATH)/build; \
	fi

fap: prepare clean_firmware clean
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		cd $(FLIPPER_FIRMWARE_PATH) && ./fbt fap_sub_dup_finder; \
	fi

logic.o: logic.c logic.h
	$(CC) $(CFLAGS) -c logic.c

tests/test_logic.o: tests/test_logic.c logic.h
	$(CC) $(CFLAGS) -c tests/test_logic.c -o tests/test_logic.o

clean:
	rm -f *.o tests/*.o test_logic
