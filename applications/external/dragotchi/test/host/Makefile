CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O0 -g -I. -Istub -I../../src
SRCDIR = ../../src
LOGIC = $(SRCDIR)/random_generator.c $(SRCDIR)/care.c $(SRCDIR)/game_model.c $(SRCDIR)/needs.c $(SRCDIR)/states.c $(SRCDIR)/discipline.c $(SRCDIR)/evolution.c $(SRCDIR)/game_logic.c $(SRCDIR)/hunt.c $(SRCDIR)/economy.c $(SRCDIR)/expedition.c
STUBS = furi_stub.c hunt_hw_stub.c
TESTS = test_runner.c test_rng.c test_model.c test_needs.c test_states.c test_discipline.c test_evolution.c test_integration.c test_save.c test_overnight.c test_hunt.c test_expedition.c
BUILD = build

$(BUILD)/tests: $(TESTS) $(LOGIC) $(STUBS) tests.h test_util.h rng_control.h stub/furi.h stub/furi_hal.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(TESTS) $(LOGIC) $(STUBS) -o $(BUILD)/tests

.PHONY: run clean
run: $(BUILD)/tests
	./$(BUILD)/tests
clean:
	rm -rf $(BUILD)
