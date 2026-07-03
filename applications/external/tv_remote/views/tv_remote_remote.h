/**
 * @file views/tv_remote_remote.h
 * @brief Remote control view – sends learned IR signals via the Flipper's buttons.
 */

#pragma once

#include <gui/view.h>

typedef struct TvRemoteApp TvRemoteApp;

/**
 * @brief Allocate the remote control view and bind it to the app context.
 * @param app  Application context that owns the view.
 * @return Newly allocated View (caller must free with tv_remote_remote_view_free).
 */
View* tv_remote_remote_view_alloc(TvRemoteApp* app);

/**
 * @brief Free the remote control view.
 * @param view  View to free.
 */
void tv_remote_remote_view_free(View* view);
