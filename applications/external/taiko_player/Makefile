PYTHON := .venv/bin/python
UFBT := .venv/bin/ufbt
MOMENTUM_HOME := $(CURDIR)/.ufbt/momentum
MOMENTUM_SDK := https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-009/flipper-z-f7-sdk-mntm-009.zip
MOMENTUM_012_HOME := $(CURDIR)/.ufbt/momentum-012
MOMENTUM_012_SDK := https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-012/flipper-z-f7-sdk-mntm-012.zip
FIRMWARE ?= official
VERSION ?= 0.5.1
TOOLCHAIN_HOME := $(HOME)/.ufbt
ifeq ($(FIRMWARE),official)
SDK_HOME := $(TOOLCHAIN_HOME)
else ifeq ($(FIRMWARE),momentum)
SDK_HOME := $(MOMENTUM_HOME)
else ifeq ($(FIRMWARE),momentum-012)
SDK_HOME := $(MOMENTUM_012_HOME)
else
$(error Unknown FIRMWARE '$(FIRMWARE)'; choose official, momentum or momentum-012)
endif
SDK_ENV := UFBT_HOME="$(SDK_HOME)" FBT_TOOLCHAIN_PATH="$(TOOLCHAIN_HOME)"
SDK_PYTHON := $(TOOLCHAIN_HOME)/toolchain/current/bin/python3

.PHONY: all setup build songs sfx package release test momentum-sdk momentum momentum-012-sdk momentum-012 launch upload-songs upload-sfx
all: package

setup:
	python3 -m venv .venv
	$(PYTHON) -m pip install -r requirements-dev.txt
	$(UFBT) update --channel=release

build:
	$(SDK_ENV) $(UFBT)
	mkdir -p dist/$(FIRMWARE)
	cp dist/taiko_player.fap dist/$(FIRMWARE)/taiko_player.fap

songs:
	$(PYTHON) scripts/prepare_songs.py

sfx:
	$(PYTHON) scripts/prepare_sfx.py

package: build songs sfx
	mkdir -p sdcard/apps/Media
	cp dist/taiko_player.fap sdcard/apps/Media/taiko_player.fap

# Build sequentially because uFBT variants share dist/taiko_player.fap.
release: test
	$(MAKE) build FIRMWARE=official
	$(MAKE) build FIRMWARE=momentum
	$(MAKE) build FIRMWARE=momentum-012
	$(PYTHON) scripts/package_release.py --version $(VERSION)

test:
	$(PYTHON) scripts/test_core.py

# Separate SDK state keeps the official SDK available. Both use uFBT's shared toolchain.
momentum-sdk:
	UFBT_HOME="$(MOMENTUM_HOME)" FBT_TOOLCHAIN_PATH="$(HOME)/.ufbt" $(UFBT) update --hw-target=f7 --url=$(MOMENTUM_SDK)

momentum:
	$(MAKE) build FIRMWARE=momentum

momentum-012-sdk:
	UFBT_HOME="$(MOMENTUM_012_HOME)" FBT_TOOLCHAIN_PATH="$(TOOLCHAIN_HOME)" $(UFBT) update --hw-target=f7 --url=$(MOMENTUM_012_SDK)

momentum-012:
	$(MAKE) build FIRMWARE=momentum-012

upload-songs: songs
	$(SDK_PYTHON) $(TOOLCHAIN_HOME)/current/scripts/storage.py send sdcard/apps_data/taiko_player /ext/apps_data/taiko_player

upload-sfx: sfx
	$(SDK_PYTHON) $(TOOLCHAIN_HOME)/current/scripts/storage.py send sdcard/apps_data/taiko_player/sfx /ext/apps_data/taiko_player/sfx

launch: build
	$(SDK_PYTHON) $(TOOLCHAIN_HOME)/current/scripts/runfap.py --sources dist/taiko_player.fap --targets /ext/apps/Media/taiko_player.fap
