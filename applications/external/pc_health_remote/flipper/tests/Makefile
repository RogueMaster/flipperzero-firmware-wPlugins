CFLAGS ?= -Wall -Wextra -Werror -std=c11 -O1 -g -fsanitize=address,undefined
test: test_core
	./test_core
test_core: test_core.c ../protocol.c ../alerts.c ../advice.c ../protocol.h ../alerts.h
	$(CC) $(CFLAGS) -o $@ test_core.c ../protocol.c ../alerts.c ../advice.c
clean:
	rm -f test_core
