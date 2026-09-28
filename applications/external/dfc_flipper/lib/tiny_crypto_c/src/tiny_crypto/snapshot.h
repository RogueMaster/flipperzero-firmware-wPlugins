/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_SNAPSHOT_H_
#define TINY_CRYPTO_SNAPSHOT_H_

/* Stores manage these states while holding the application's lock. */
typedef enum {
  TC_SNAPSHOT_FREE, TC_SNAPSHOT_PREPARED, TC_SNAPSHOT_CURRENT, TC_SNAPSHOT_RETIRED
} TC_snapshot_state;

#endif
