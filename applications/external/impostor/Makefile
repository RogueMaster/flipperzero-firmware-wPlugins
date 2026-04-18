# Host tests + FAP via fbt.
PROJECT_NAME = impostor_game

FAP_APPID = flipper_impostor_game

FLIPPER_FIRMWARE_PATH ?= /home/<YOUR_PATH>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test test_strings test_game_rules test_session test_player_roster \
	prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Host unit tests (strings, game_rules, session, player_roster)"
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
		--suppress=constParameterCallback \
		src/i18n/strings.c src/domain/game_rules.c src/domain/session.c \
		src/domain/word_bank.c src/domain/player_roster.c \
		src/app/impostor_app.c src/app/role_card_text.c \
		src/infrastructure/saved_games.c src/infrastructure/settings_storage.c \
		src/ui/pass_view.c src/ui/card_view.c src/platform/random_port.c main.c \
		tests/test_strings.c tests/test_game_rules.c tests/test_session.c \
		tests/test_player_roster.c

test: test_strings test_game_rules test_session test_player_roster

test_strings: strings.o tests/test_strings.o
	$(CC) $(CFLAGS) -o test_strings strings.o tests/test_strings.o
	./test_strings

strings.o: src/i18n/strings.c include/i18n/strings.h
	$(CC) $(CFLAGS) -c src/i18n/strings.c -o strings.o

tests/test_strings.o: tests/test_strings.c include/i18n/strings.h
	$(CC) $(CFLAGS) -c tests/test_strings.c -o tests/test_strings.o

test_game_rules: game_rules.o tests/test_game_rules.o
	$(CC) $(CFLAGS) -o test_game_rules game_rules.o tests/test_game_rules.o
	./test_game_rules

game_rules.o: src/domain/game_rules.c include/domain/game_rules.h include/domain/game_limits.h
	$(CC) $(CFLAGS) -c src/domain/game_rules.c -o game_rules.o

tests/test_game_rules.o: tests/test_game_rules.c include/domain/game_rules.h
	$(CC) $(CFLAGS) -c tests/test_game_rules.c -o tests/test_game_rules.o

test_session: session.o game_rules.o tests/test_session.o
	$(CC) $(CFLAGS) -o test_session session.o game_rules.o tests/test_session.o
	./test_session

session.o: src/domain/session.c include/domain/session.h include/domain/game_rules.h
	$(CC) $(CFLAGS) -c src/domain/session.c -o session.o

tests/test_session.o: tests/test_session.c include/domain/session.h
	$(CC) $(CFLAGS) -c tests/test_session.c -o tests/test_session.o

test_player_roster: player_roster.o tests/test_player_roster.o
	$(CC) $(CFLAGS) -o test_player_roster player_roster.o tests/test_player_roster.o
	./test_player_roster

player_roster.o: src/domain/player_roster.c include/domain/player_roster.h \
		include/domain/game_limits.h
	$(CC) $(CFLAGS) -c src/domain/player_roster.c -o player_roster.o

tests/test_player_roster.o: tests/test_player_roster.c include/domain/player_roster.h
	$(CC) $(CFLAGS) -c tests/test_player_roster.c -o tests/test_player_roster.o

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
	rm -f *.o tests/*.o test_strings test_game_rules test_session test_player_roster
