/*
 * (c) 2017      Marcos Del Sol Vives
 *
 * SPDX-License-Identifier: MIT
 */

#include <version.h>
#include <stdio.h>

const char * nfc3d_version_fork() {
	// TODO: maybe this should go in another file?
	return "socram";
}

uint32_t nfc3d_version_commit() {
	return 0;
}

uint32_t nfc3d_version_build() {
	return 0;
}
