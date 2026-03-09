all: gitsub asn1 build

gitsub:
	git submodule update --init --recursive

asn1:
	asn1c -S ./lib/asn1_skeletons -D ./lib/asn1 -no-gen-example -no-gen-OER -no-gen-PER -pdu=all seader.asn1

build:
	ufbt

test-host:
	mkdir -p build/host_tests
	cc -std=c11 -Wall -Wextra -Werror -DSEADER_HOST_TEST -Ilib/host_tests/vendor/munit -Ilib/host_tests -I. \
		lib/host_tests/vendor/munit/munit.c \
		lib/host_tests/test_main.c \
		lib/host_tests/test_lrc.c \
		lib/host_tests/test_t1_existing.c \
		lib/host_tests/t1_test_stubs.c \
		lib/host_tests/bit_buffer_mock.c \
		lrc.c \
		t_1.c \
		-o build/host_tests/seader_tests
	./build/host_tests/seader_tests

launch:
	ufbt launch

format:
	ufbt format

clean:
	rm -rf dist
	rm -rf build/host_tests
