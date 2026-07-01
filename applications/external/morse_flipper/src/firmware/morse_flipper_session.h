/*
 * Purpose: Reserve a narrow public seam for LCWO session helpers.
 * Owns: session module boundary documentation; implementation lives in the .c file.
 * Depends on: morse_flipper_session.c and trainer state.
 * Tests: tests/test_trainer.c covers the host trainer core.
 */

#pragma once

/*
 * Kept tiny for now. Session implementation lives in morse_flipper_session.c,
 * but this header keeps a narrow place for future declarations.
 */
