# TagTinker - developer convenience Makefile
#
# One entry point for the three build products in this repository:
#   * Flipper Zero FAP         (application.fam at the repo root, built with ufbt)
#   * Cloudflare Worker        (cloud-plugins/, TypeScript bundled by wrangler)
#   * ESP32-S2 WiFi firmware   (esp32-wifi-fw/, ESP-IDF v5.2)
# plus a local server for the static Image Prep web tool (web-image-prep/).
#
# Usage:
#   make                       # list targets (same as `make help`)
#   make setup                 # one-time: .venv + ufbt SDK + worker node_modules
#   make build-all             # build FAP, worker and ESP32 firmware
#   make launch                # build + install + run the FAP on a USB-connected Flipper
#   make flash-esp ESP_PORT=/dev/cu.usbserial-XXXX
#   make build-esp IDF_PYTHON_ENV_PATH=$HOME/.espressif/python_env/idf5.2_py3.11_env
#     (ESP-IDF's export.sh picks the first python3 on PATH and expects a matching
#      ~/.espressif/python_env/idf5.2_pyX.Y_env. When exactly one such env exists
#      this Makefile exports IDF_PYTHON_ENV_PATH for you; set it explicitly if you
#      keep several, or keep them somewhere other than ~/.espressif.)
#
# Every variable in the "Tools" and "Locations" blocks can be overridden from
# the command line or the environment, e.g. `make build-esp IDF_PATH=/opt/esp-idf`.
# Use absolute paths for IDF_PATH.
#
# Portability: written for GNU Make 3.81 (the stock /usr/bin/make on macOS) as
# well as GNU Make 4.x. Do not introduce 4.x-only features (ONESHELL, the
# shell-assignment operator, the file/let/intcmp functions, undefine, private).
# Every recipe line runs in its own shell, so steps that must share state -
# sourcing ESP-IDF's export.sh and then calling idf.py - are chained with &&
# on one line.

SHELL := /bin/bash
.DEFAULT_GOAL := help

# --- Tools ------------------------------------------------------------------
VENV     ?= .venv
# Prefer the ufbt that `make setup` installs into the venv; else use PATH.
UFBT     ?= $(if $(wildcard $(VENV)/bin/ufbt),$(VENV)/bin/ufbt,ufbt)
PYTHON   ?= python3
NPM      ?= npm
NPX      ?= npx
IDF_PATH ?= $(HOME)/esp/esp-idf
export IDF_PATH
# Same chip the CI job builds for. Exporting it makes ESP-IDF refuse a stale
# sdkconfig that was generated for a different target instead of silently
# building it (sdkconfig.defaults pins the same value for clean builds).
IDF_TARGET ?= esp32s2
export IDF_TARGET

# ESP-IDF's export.sh detects `python3` from PATH and expects a virtualenv named
# after that interpreter (idf5.2_py3.14_env for Python 3.14). If install.sh ran
# with a different interpreter that env does not exist and export.sh fails even
# though a working idf5.2_py3.11_env sits right next to it. When exactly one 5.2
# env exists, point ESP-IDF at it; `?=` keeps any explicit override.
IDF_TOOLS_PATH ?= $(HOME)/.espressif
_idf_py_envs := $(wildcard $(IDF_TOOLS_PATH)/python_env/idf5.2_py*_env)
ifeq ($(words $(_idf_py_envs)),1)
IDF_PYTHON_ENV_PATH ?= $(_idf_py_envs)
endif
ifneq ($(IDF_PYTHON_ENV_PATH),)
export IDF_PYTHON_ENV_PATH
endif

# --- Locations / ports ------------------------------------------------------
ESP_PORT   ?=
WEB_PORT   ?= 8000
WEB_DIR    ?= web-image-prep
ESP_DIR    ?= esp32-wifi-fw
WORKER_DIR ?= cloud-plugins

# idf.py auto-detects the serial port when ESP_PORT is empty.
IDF_PORT_FLAG = $(if $(ESP_PORT),-p $(ESP_PORT),)

# Source ESP-IDF's export.sh in the *current* shell. Its chatty output goes to
# a temp log that is only printed if sourcing fails. Must be chained with &&
# on the same recipe line as the idf.py call that needs the environment.
IDF_ENV = idf_log=$$(mktemp) \
  && { . "$(IDF_PATH)/export.sh" >"$$idf_log" 2>&1 \
       || { cat "$$idf_log"; rm -f "$$idf_log"; \
            echo "error: sourcing $(IDF_PATH)/export.sh failed." >&2; \
            echo "       Fix: run '$(IDF_PATH)/install.sh esp32s2', or - if 'python3' on PATH is not the interpreter" >&2; \
            echo "       install.sh used - pass IDF_PYTHON_ENV_PATH=$(IDF_TOOLS_PATH)/python_env/idf5.2_pyX.Y_env" >&2; \
            exit 1; }; } \
  && rm -f "$$idf_log"

.PHONY: help setup build-fap build-worker build-esp build-all launch flash-esp \
        serve-web lint check-worker clean distclean check-esp-env check-root worker-deps

# --- Help -------------------------------------------------------------------
help: ## Show this help
	@echo "TagTinker developer targets  (GNU Make $(MAKE_VERSION))"
	@echo ""
	@grep -E '^[a-zA-Z0-9_-]+:.*## ' $(firstword $(MAKEFILE_LIST)) \
	  | awk 'BEGIN { FS = ":.*## " } { printf "  %-14s %s\n", $$1, $$2 }'
	@echo ""
	@echo "Variables (override on the command line, e.g. make flash-esp ESP_PORT=/dev/cu.usbserial-XXXX):"
	@echo "  UFBT=$(UFBT)  PYTHON=$(PYTHON)  NPM=$(NPM)"
	@echo "  IDF_PATH=$(IDF_PATH)  ESP_PORT=$(if $(ESP_PORT),$(ESP_PORT),<auto>)"
	@echo "  IDF_PYTHON_ENV_PATH=$(if $(IDF_PYTHON_ENV_PATH),$(IDF_PYTHON_ENV_PATH),<ESP-IDF default>)"
	@echo "  ESP_DIR=$(ESP_DIR)  WORKER_DIR=$(WORKER_DIR)  WEB_DIR=$(WEB_DIR)  WEB_PORT=$(WEB_PORT)"

# --- Setup ------------------------------------------------------------------
setup: ## One-time: create .venv with ufbt, fetch the Flipper SDK, npm ci the worker
	@test -x $(VENV)/bin/python || { echo ">> creating virtualenv $(VENV)"; $(PYTHON) -m venv $(VENV); }
	$(VENV)/bin/python -m pip install -q -U ufbt
	$(VENV)/bin/ufbt update
	cd $(WORKER_DIR) && $(NPM) ci

# Install worker dependencies only when node_modules is missing.
worker-deps:
	@test -d $(WORKER_DIR)/node_modules || { echo ">> $(WORKER_DIR)/node_modules missing, running npm ci"; cd $(WORKER_DIR) && $(NPM) ci; }

# clean/distclean delete cwd-relative paths; refuse to run anywhere but the repo root.
check-root:
	@test -f application.fam -a -d $(ESP_DIR) -a -d $(WORKER_DIR) || { \
	  echo "error: run make from the TagTinker repository root (or use make -C <repo>)" >&2; exit 1; }

# Fail early with a readable message when ESP-IDF is not where we expect it.
check-esp-env:
	@test -f "$(IDF_PATH)/export.sh" || { \
	  echo "error: ESP-IDF not found at '$(IDF_PATH)' (no export.sh there)." >&2; \
	  echo "       Install ESP-IDF v5.2 for the esp32s2 target, or point IDF_PATH at it:" >&2; \
	  echo "         make build-esp IDF_PATH=/path/to/esp-idf" >&2; \
	  exit 1; }

# --- Build ------------------------------------------------------------------
build-fap: ## Build the Flipper app -> dist/tagtinker.fap (+ dist/debug/tagtinker_d.elf)
	@# Bare `ufbt` (scons default targets) builds AND installs into dist/;
	@# `ufbt build` only builds/validates inside ~/.ufbt/build and never copies.
	$(UFBT)

build-worker: worker-deps ## Type-check and bundle the Cloudflare worker -> cloud-plugins/dist/index.js
	cd $(WORKER_DIR) && $(NPX) tsc --noEmit
	cd $(WORKER_DIR) && CI=true WRANGLER_SEND_METRICS=false $(NPM) run build

build-esp: check-esp-env ## Build the ESP32-S2 WiFi devboard firmware (ESP-IDF v5.2) -> esp32-wifi-fw/build/
	@echo ">> idf.py -C $(ESP_DIR) build   (IDF_PATH=$(IDF_PATH))"
	@$(IDF_ENV) && idf.py -C $(ESP_DIR) build

build-all: build-fap build-worker build-esp ## Build the FAP, the worker and the ESP32 firmware

# --- Run / flash ------------------------------------------------------------
launch: build-fap ## Build, install and run the FAP on the USB-connected Flipper (port auto-detected)
	$(UFBT) launch

flash-esp: check-esp-env build-esp ## Build and flash the ESP32-S2 firmware (ESP_PORT=/dev/... to pick a port)
	@echo ">> idf.py -C $(ESP_DIR) $(IDF_PORT_FLAG) flash   (IDF_PATH=$(IDF_PATH))"
	@$(IDF_ENV) && idf.py -C $(ESP_DIR) $(IDF_PORT_FLAG) flash

serve-web: ## Serve the Image Prep web tool locally (WEB_PORT=8000)
	@echo ">> serving $(WEB_DIR)/ at http://localhost:$(WEB_PORT)/   (Ctrl-C to stop)"
	$(PYTHON) -m http.server $(WEB_PORT) --directory $(WEB_DIR)

# --- Checks -----------------------------------------------------------------
lint: ## ADVISORY: run ufbt lint (clang-format). Fails on main by design; not a CI gate
	@echo ">> note: lint is advisory. The FAP sources use column-aligned macros/struct fields that"
	@echo ">>       clang-format rejects, and ufbt lint drops a generated .clang-format at the repo"
	@echo ">>       root (gitignored). Never run 'ufbt format' - it would rewrite the ESP32 sources too."
	$(UFBT) lint

check-worker: worker-deps ## Type-check the worker only (npx tsc --noEmit)
	cd $(WORKER_DIR) && $(NPX) tsc --noEmit

# --- Cleanup ----------------------------------------------------------------
clean: check-root ## Remove build outputs only (dist/, worker dist + .wrangler, ESP build/, generated .clang-format)
	rm -rf dist $(WORKER_DIR)/dist $(WORKER_DIR)/.wrangler $(ESP_DIR)/build
	rm -f .clang-format .vscode/compile_commands.json

distclean: check-root clean ## clean + remove .venv, worker node_modules and ESP sdkconfig (re-run 'make setup' afterwards)
	@echo "!! distclean: removing $(VENV)/, $(WORKER_DIR)/node_modules/ and $(ESP_DIR)/sdkconfig"
	rm -rf $(VENV) $(WORKER_DIR)/node_modules $(ESP_DIR)/sdkconfig
