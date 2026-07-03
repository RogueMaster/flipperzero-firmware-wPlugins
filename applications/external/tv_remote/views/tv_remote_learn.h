/**
 * @file views/tv_remote_learn.h
 * @brief IR learning view – records a signal for each TV button sequentially.
 */

#pragma once

#include <gui/view.h>

typedef struct TvRemoteApp TvRemoteApp;

/**
 * @brief Allocate the learn view and bind it to the app context.
 * @param app  Application context that owns the view.
 * @return Newly allocated View (caller must free with tv_remote_learn_view_free).
 */
View* tv_remote_learn_view_alloc(TvRemoteApp* app);

/**
 * @brief Free the learn view previously created by tv_remote_learn_view_alloc.
 * @param view  View to free.
 */
void tv_remote_learn_view_free(View* view);
