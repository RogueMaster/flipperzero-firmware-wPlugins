# Compiles the real grading engine for the host and checks every threshold and
# boundary. The grade is the product's whole output, so it gets verified on
# every push. See host_grade_test.c.
#
#   make -C test

CC      ?= cc
CFLAGS  := -std=gnu2x -Wall -Wextra -Werror -O1 -g
CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  += -I ..

SRC := host_grade_test.c ../helpers/fdy_grade.c
BIN := host_grade_test

.PHONY: all run clean
all: run

$(BIN): $(SRC) ../helpers/fdy_grade.h
	$(CC) $(CFLAGS) $(SRC) -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BIN) $(BIN).dSYM
