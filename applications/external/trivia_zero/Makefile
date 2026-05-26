# Host tests + FAP via fbt. Symlink under applications_user (matches apps_data path).
PROJECT_NAME = trivia_zero

FAP_APPID = flipper_trivia_zero

FLIPPER_FIRMWARE_PATH ?= /home/endika/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test test_version test_category test_anti_repeat test_history_buffer test_question_pool test_strings test_settings_storage test_pack_reader test_pack_integration test_question_view_layout prepare fap clean clean_firmware format linter pack py-install py-test py-lint py-format py-typecheck

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Host unit tests"
	@echo "  make prepare        - Symlink app into firmware applications_user"
	@echo "  make fap            - Clean firmware build + compile .fap"
	@echo "  make format         - clang-format"
	@echo "  make linter         - cppcheck"
	@echo "  make py-install     - pip install Python pipeline deps (in tools/.venv)"
	@echo "  make py-test        - pytest the Python pipeline"
	@echo "  make py-lint        - ruff check the Python pipeline"
	@echo "  make py-format      - ruff format the Python pipeline"
	@echo "  make py-typecheck   - mypy --strict the Python pipeline"
	@echo "  make pack           - build data/trivia_{es,en}.{tsv,idx}"
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
		--suppress=unusedFunction:src/platform/random_port.c \
		--suppress=unusedFunction:src/infrastructure/pack_reader.c \
		src/app/trivia_zero_app.c src/domain/category.c src/domain/anti_repeat.c src/domain/history_buffer.c src/domain/question_pool.c src/i18n/strings.c src/infrastructure/settings_storage.c src/infrastructure/pack_reader.c src/platform/random_port.c src/ui/question_view.c main.c \
		tests/test_version.c tests/test_category.c tests/test_anti_repeat.c tests/test_history_buffer.c tests/test_question_pool.c tests/test_strings.c tests/test_settings_storage.c tests/test_pack_reader.c tests/test_pack_integration.c tests/test_question_view_layout.c tests/embedded_pack_stub.c

test: test_version test_category test_anti_repeat test_history_buffer test_question_pool test_strings test_settings_storage test_pack_reader test_pack_integration test_question_view_layout

test_version: tests/test_version.o
	$(CC) $(CFLAGS) -o test_version tests/test_version.o
	./test_version

tests/test_version.o: tests/test_version.c include/version.h
	$(CC) $(CFLAGS) -c tests/test_version.c -o tests/test_version.o

test_category: category.o tests/test_category.o
	$(CC) $(CFLAGS) -o test_category category.o tests/test_category.o
	./test_category

category.o: src/domain/category.c include/domain/category.h
	$(CC) $(CFLAGS) -c src/domain/category.c -o category.o

tests/test_category.o: tests/test_category.c include/domain/category.h
	$(CC) $(CFLAGS) -c tests/test_category.c -o tests/test_category.o

test_anti_repeat: anti_repeat.o tests/test_anti_repeat.o
	$(CC) $(CFLAGS) -o test_anti_repeat anti_repeat.o tests/test_anti_repeat.o
	./test_anti_repeat

anti_repeat.o: src/domain/anti_repeat.c include/domain/anti_repeat.h
	$(CC) $(CFLAGS) -c src/domain/anti_repeat.c -o anti_repeat.o

tests/test_anti_repeat.o: tests/test_anti_repeat.c include/domain/anti_repeat.h
	$(CC) $(CFLAGS) -c tests/test_anti_repeat.c -o tests/test_anti_repeat.o

test_history_buffer: history_buffer.o tests/test_history_buffer.o
	$(CC) $(CFLAGS) -o test_history_buffer history_buffer.o tests/test_history_buffer.o
	./test_history_buffer

history_buffer.o: src/domain/history_buffer.c include/domain/history_buffer.h
	$(CC) $(CFLAGS) -c src/domain/history_buffer.c -o history_buffer.o

tests/test_history_buffer.o: tests/test_history_buffer.c include/domain/history_buffer.h
	$(CC) $(CFLAGS) -c tests/test_history_buffer.c -o tests/test_history_buffer.o

test_question_pool: question_pool.o anti_repeat.o tests/test_question_pool.o
	$(CC) $(CFLAGS) -o test_question_pool question_pool.o anti_repeat.o tests/test_question_pool.o
	./test_question_pool

question_pool.o: src/domain/question_pool.c include/domain/question_pool.h include/domain/anti_repeat.h
	$(CC) $(CFLAGS) -c src/domain/question_pool.c -o question_pool.o

tests/test_question_pool.o: tests/test_question_pool.c include/domain/question_pool.h include/domain/anti_repeat.h
	$(CC) $(CFLAGS) -c tests/test_question_pool.c -o tests/test_question_pool.o

test_strings: strings.o tests/test_strings.o
	$(CC) $(CFLAGS) -o test_strings strings.o tests/test_strings.o
	./test_strings

strings.o: src/i18n/strings.c include/i18n/strings.h include/domain/category.h
	$(CC) $(CFLAGS) -c src/i18n/strings.c -o strings.o

tests/test_strings.o: tests/test_strings.c include/i18n/strings.h include/domain/category.h
	$(CC) $(CFLAGS) -c tests/test_strings.c -o tests/test_strings.o

test_settings_storage: settings_storage.o tests/test_settings_storage.o
	$(CC) $(CFLAGS) -o test_settings_storage settings_storage.o tests/test_settings_storage.o
	./test_settings_storage

settings_storage.o: src/infrastructure/settings_storage.c include/infrastructure/settings_storage.h include/domain/category.h
	$(CC) $(CFLAGS) -c src/infrastructure/settings_storage.c -o settings_storage.o

tests/test_settings_storage.o: tests/test_settings_storage.c include/infrastructure/settings_storage.h include/domain/category.h
	$(CC) $(CFLAGS) -c tests/test_settings_storage.c -o tests/test_settings_storage.o

test_pack_reader: pack_reader.o tests/embedded_pack_stub.o tests/test_pack_reader.o
	$(CC) $(CFLAGS) -o test_pack_reader pack_reader.o tests/embedded_pack_stub.o tests/test_pack_reader.o
	./test_pack_reader

pack_reader.o: src/infrastructure/pack_reader.c include/infrastructure/pack_reader.h include/domain/category.h include/data/embedded_pack.h
	$(CC) $(CFLAGS) -c src/infrastructure/pack_reader.c -o pack_reader.o

tests/embedded_pack_stub.o: tests/embedded_pack_stub.c include/data/embedded_pack.h
	$(CC) $(CFLAGS) -c tests/embedded_pack_stub.c -o tests/embedded_pack_stub.o

tests/test_pack_reader.o: tests/test_pack_reader.c include/infrastructure/pack_reader.h include/domain/category.h
	$(CC) $(CFLAGS) -c tests/test_pack_reader.c -o tests/test_pack_reader.o

test_pack_integration: pack_reader.o tests/embedded_pack_stub.o tests/test_pack_integration.o
	$(CC) $(CFLAGS) -o test_pack_integration pack_reader.o tests/embedded_pack_stub.o tests/test_pack_integration.o
	./test_pack_integration

tests/test_pack_integration.o: tests/test_pack_integration.c include/infrastructure/pack_reader.h
	$(CC) $(CFLAGS) -c tests/test_pack_integration.c -o tests/test_pack_integration.o

test_question_view_layout: question_view.o tests/test_question_view_layout.o
	$(CC) $(CFLAGS) -o test_question_view_layout question_view.o tests/test_question_view_layout.o
	./test_question_view_layout

question_view.o: src/ui/question_view.c include/ui/question_view.h include/infrastructure/pack_reader.h include/domain/category.h include/i18n/strings.h
	$(CC) $(CFLAGS) -c src/ui/question_view.c -o question_view.o

tests/test_question_view_layout.o: tests/test_question_view_layout.c include/ui/question_view.h include/infrastructure/pack_reader.h
	$(CC) $(CFLAGS) -c tests/test_question_view_layout.c -o tests/test_question_view_layout.o

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

fap: pack prepare clean_firmware clean
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		cd $(FLIPPER_FIRMWARE_PATH) && ./fbt fap_$(FAP_APPID); \
	fi

clean:
	rm -f *.o tests/*.o test_version test_category test_anti_repeat test_history_buffer test_question_pool test_strings test_settings_storage test_pack_reader test_pack_integration test_question_view_layout

PY_VENV = tools/.venv
PY = $(PY_VENV)/bin/python
PIP = $(PY_VENV)/bin/pip

$(PY_VENV)/bin/activate: tools/requirements-dev.txt
	python3 -m venv $(PY_VENV)
	$(PIP) install --upgrade pip
	$(PIP) install -r tools/requirements-dev.txt
	@touch $(PY_VENV)/bin/activate

py-install: $(PY_VENV)/bin/activate

py-test: py-install
	cd tools && ../$(PY) -m pytest

py-lint: py-install
	cd tools && ../$(PY) -m ruff check .

py-format: py-install
	cd tools && ../$(PY) -m ruff format .

py-typecheck: py-install
	cd tools && ../$(PY) -m mypy --strict trivia_pack

pack: py-install
	cd tools && ../$(PY) build_pack.py
