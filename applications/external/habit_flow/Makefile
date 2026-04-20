PROJECT_NAME = habit_flow

FAP_APPID = flipper_habit_flow

FLIPPER_FIRMWARE_PATH ?= /home/endika/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Host unit tests (habit + date logic)"
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
		--suppress=constParameterPointer:src/app/hf_views.c \
		--suppress=unusedFunction:main.c \
		src/domain/habit.c src/domain/habit_date.c \
		src/persistence/habit_store.c src/platform/hf_rtc.c \
		src/application/hf_session_service.c \
		src/app/habitflow_app.c src/app/hf_views.c main.c tests/test_habit.c

OBJS = habit_date.o habit.o test_habit.o

test: $(OBJS)
	$(CC) $(CFLAGS) -o test_habit $(OBJS)
	./test_habit

habit_date.o: src/domain/habit_date.c include/domain/habit_date.h
	$(CC) $(CFLAGS) -c src/domain/habit_date.c -o habit_date.o

habit.o: src/domain/habit.c include/domain/habit.h
	$(CC) $(CFLAGS) -c src/domain/habit.c -o habit.o

test_habit.o: tests/test_habit.c include/domain/habit.h include/domain/habit_date.h
	$(CC) $(CFLAGS) -c tests/test_habit.c -o test_habit.o

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
	rm -f *.o tests/*.o test_habit
