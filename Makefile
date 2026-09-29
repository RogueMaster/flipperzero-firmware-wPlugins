.PHONY: test

test:
	cc -std=c11 -Wall -Wextra -Werror cribbage.c tests/test_scoring.c -o /tmp/cribbage_scoring_tests
	/tmp/cribbage_scoring_tests
