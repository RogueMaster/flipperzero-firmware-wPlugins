/*
 * (c) 2015-2017 Marcos Del Sol Vives
 * (c) 2016      javiMaD
 *
 * SPDX-License-Identifier: MIT
 */

#include "util.h"
#include <stdint.h>

void printhex(void * data, size_t size) {
	size_t i;
	uint8_t * bytes = (uint8_t *) data;
	for (i = 0; i < size; i++) {
		if ((i % 16) > 0) {
			printf(" ");
		}
		printf("%02X", bytes[i]);
		if ((i % 16) == 15) {
			printf("\n");
		}
	}
	if ((i % 16) != 15) {
		printf("\n");
	}
}

