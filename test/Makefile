# Host tests for Specter's pure logic. Nothing here touches the Flipper SDK, so
# the decision layers - what kind of emitter is this, is this room clean - can be
# pinned down on any machine before they ever reach hardware.
#
#   make -C test        build and run everything
#   make -C test clean

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g

TESTS   := build/test_classify build/test_survey build/test_scale

.PHONY: all clean
all: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

build/test_classify: test_classify.c ../helpers/emitter_classify.c ../helpers/emitter_classify.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_classify.c ../helpers/emitter_classify.c

build/test_survey: test_survey.c ../helpers/survey_verdict.c ../helpers/survey_verdict.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_survey.c ../helpers/survey_verdict.c

build/test_scale: test_scale.c ../helpers/field_scale.c ../helpers/field_scale.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ test_scale.c ../helpers/field_scale.c

clean:
	rm -rf build
