PROJECT_NAME = gurpil
FAP_APPID = flipper_gurpil

# Local override: create a gitignored `local.mk` with your real path, e.g.
#   FLIPPER_FIRMWARE_PATH = /home/you/flipperzero-firmware
# The committed default below is a placeholder on purpose — never commit a real path.
-include local.mk
FLIPPER_FIRMWARE_PATH ?= <Path>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Host unit tests (domain logic)"
	@echo "  make prepare        - Symlink app into firmware applications_user"
	@echo "  make fap            - Clean firmware build + compile .fap"
	@echo "  make format         - clang-format"
	@echo "  make linter         - cppcheck"
	@echo "  make clean          - Remove local objects"
	@echo "  make clean_firmware - rm firmware build dir"

# --- host tests: compile domain + test TU with gcc, run the binary ---
# Each tests/test_*.c is its own suite with its own `main`, so each links into its own
# binary (test_gurpil_<suite>); `make test` runs all of them. Add a new suite by appending
# its .o files to OBJS_<SUITE> and its binary to TEST_BINS, mirroring the pairs below.
OBJS_SMOKE = version_info.o test_smoke.o
OBJS_SHAPES = shapes.o test_shapes.o
OBJS_TERRAIN = terrain.o test_terrain.o
OBJS_SPEED_RAMP = speed_ramp.o test_speed_ramp.o
OBJS_SIM = terrain.o shapes.o speed_ramp.o sim.o test_sim.o
OBJS_ENDLESS = endless.o test_endless.o
OBJS_RECORD = record.o test_record.o
OBJS_GAME = terrain.o shapes.o speed_ramp.o sim.o endless.o game.o test_game.o
OBJS_RENDER_MAP = render_map.o test_render_map.o
TEST_BINS = test_gurpil_smoke test_gurpil_shapes test_gurpil_terrain test_gurpil_speed_ramp test_gurpil_sim test_gurpil_endless test_gurpil_record test_gurpil_game test_gurpil_render_map

test: $(TEST_BINS)
	./test_gurpil_smoke
	./test_gurpil_shapes
	./test_gurpil_terrain
	./test_gurpil_speed_ramp
	./test_gurpil_sim
	./test_gurpil_endless
	./test_gurpil_record
	./test_gurpil_game
	./test_gurpil_render_map

test_gurpil_smoke: $(OBJS_SMOKE)
	$(CC) $(CFLAGS) -o test_gurpil_smoke $(OBJS_SMOKE)

test_gurpil_shapes: $(OBJS_SHAPES)
	$(CC) $(CFLAGS) -o test_gurpil_shapes $(OBJS_SHAPES)

test_gurpil_terrain: $(OBJS_TERRAIN)
	$(CC) $(CFLAGS) -o test_gurpil_terrain $(OBJS_TERRAIN)

test_gurpil_speed_ramp: $(OBJS_SPEED_RAMP)
	$(CC) $(CFLAGS) -o test_gurpil_speed_ramp $(OBJS_SPEED_RAMP)

test_gurpil_sim: $(OBJS_SIM)
	$(CC) $(CFLAGS) -o test_gurpil_sim $(OBJS_SIM)

test_gurpil_endless: $(OBJS_ENDLESS)
	$(CC) $(CFLAGS) -o test_gurpil_endless $(OBJS_ENDLESS)

test_gurpil_record: $(OBJS_RECORD)
	$(CC) $(CFLAGS) -o test_gurpil_record $(OBJS_RECORD)

test_gurpil_game: $(OBJS_GAME)
	$(CC) $(CFLAGS) -o test_gurpil_game $(OBJS_GAME)

test_gurpil_render_map: $(OBJS_RENDER_MAP)
	$(CC) $(CFLAGS) -o test_gurpil_render_map $(OBJS_RENDER_MAP)

version_info.o: src/domain/version_info.c include/domain/version_info.h
	$(CC) $(CFLAGS) -c src/domain/version_info.c -o version_info.o

test_smoke.o: tests/test_smoke.c include/domain/version_info.h
	$(CC) $(CFLAGS) -c tests/test_smoke.c -o test_smoke.o

shapes.o: src/domain/shapes.c include/domain/shapes.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c src/domain/shapes.c -o shapes.o

test_shapes.o: tests/test_shapes.c include/domain/shapes.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c tests/test_shapes.c -o test_shapes.o

terrain.o: src/domain/terrain.c include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c src/domain/terrain.c -o terrain.o

test_terrain.o: tests/test_terrain.c include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c tests/test_terrain.c -o test_terrain.o

speed_ramp.o: src/domain/speed_ramp.c include/domain/speed_ramp.h
	$(CC) $(CFLAGS) -c src/domain/speed_ramp.c -o speed_ramp.o

test_speed_ramp.o: tests/test_speed_ramp.c include/domain/speed_ramp.h
	$(CC) $(CFLAGS) -c tests/test_speed_ramp.c -o test_speed_ramp.o

sim.o: src/domain/sim.c include/domain/sim.h include/domain/shapes.h include/domain/speed_ramp.h include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c src/domain/sim.c -o sim.o

test_sim.o: tests/test_sim.c include/domain/sim.h include/domain/shapes.h include/domain/speed_ramp.h include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c tests/test_sim.c -o test_sim.o

endless.o: src/domain/endless.c include/domain/endless.h
	$(CC) $(CFLAGS) -c src/domain/endless.c -o endless.o

test_endless.o: tests/test_endless.c include/domain/endless.h
	$(CC) $(CFLAGS) -c tests/test_endless.c -o test_endless.o

record.o: src/domain/record.c include/domain/record.h
	$(CC) $(CFLAGS) -c src/domain/record.c -o record.o

test_record.o: tests/test_record.c include/domain/record.h
	$(CC) $(CFLAGS) -c tests/test_record.c -o test_record.o

game.o: src/application/game.c include/application/game.h include/domain/sim.h include/domain/endless.h include/domain/shapes.h include/domain/speed_ramp.h include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c src/application/game.c -o game.o

test_game.o: tests/test_game.c include/application/game.h include/domain/sim.h include/domain/endless.h include/domain/shapes.h include/domain/speed_ramp.h include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c tests/test_game.c -o test_game.o

render_map.o: src/ui/render_map.c include/ui/render_map.h include/domain/shapes.h include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c src/ui/render_map.c -o render_map.o

test_render_map.o: tests/test_render_map.c include/ui/render_map.h include/domain/shapes.h include/domain/terrain.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c tests/test_render_map.c -o test_render_map.o

# --- format / lint ---
FORMAT_FILES := $(shell git ls-files '*.c' '*.h' 2>/dev/null)
ifeq ($(strip $(FORMAT_FILES)),)
FORMAT_FILES := $(shell find . -type f \( -name '*.c' -o -name '*.h' \) ! -path './.git/*' | sort)
endif

format:
	clang-format -i $(FORMAT_FILES)

# unusedFunction suppression: main.c's entry point (gurpil_app) is only called by the firmware
# loader, which cppcheck can't see from this host-analyzed source set. Every other furi-facing
# function (platform_random_seed, best_store_load/save, gurpil_render, the render_map helpers)
# now has a real caller inside this same set — src/app/gurpil_app.c wires them all into the main
# loop — so no other suppression is needed.
linter:
	cppcheck --enable=all --inline-suppr -I. \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction:main.c \
		src/domain/version_info.c src/domain/shapes.c src/domain/terrain.c \
		src/domain/speed_ramp.c src/domain/sim.c \
		src/domain/endless.c src/domain/record.c \
		src/application/game.c \
		src/platform/random_port.c \
		src/persistence/best_store.c \
		src/ui/render_map.c \
		src/views/game_view.c \
		src/views/gurpil_game_view.c \
		src/scenes/gurpil_scene.c \
		src/scenes/gurpil_scene_menu.c \
		src/scenes/gurpil_scene_game.c \
		src/scenes/gurpil_scene_how_to_play.c \
		src/scenes/gurpil_scene_credits.c \
		src/app/gurpil_app.c main.c \
		tests/test_smoke.c tests/test_shapes.c tests/test_terrain.c \
		tests/test_speed_ramp.c tests/test_sim.c \
		tests/test_endless.c tests/test_record.c tests/test_game.c tests/test_render_map.c

# --- build the .fap via the firmware tree (ufbt/fbt; not available in this sandbox) ---
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
	rm -f *.o tests/*.o test_gurpil test_gurpil_smoke test_gurpil_shapes test_gurpil_terrain test_gurpil_speed_ramp test_gurpil_sim test_gurpil_endless test_gurpil_record test_gurpil_game test_gurpil_render_map
