// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file flock_detail_view.h
 * The Flock/ALPR detection detail screen.
 *
 * Was a Widget with one text-scroll element until v0.49. That could only render
 * a string, so the signal strength had to be raw "-82" text while the list rows
 * one screen back showed graphical bars -- two notations for one field, which is
 * what GitHub issue #5 flagged. A custom canvas view draws the SAME
 * ui_signal_bars() the list uses, and lays each field out on its own labelled
 * row instead of letting a run-on line wrap mid-word ("via be / acon").
 *
 * It also renders straight from app->flock[app->selected] every frame, so RSSI
 * and the sighting count update live while the parent scan scene keeps running
 * behind it.
 */
#pragma once

#include <gui/view.h>

typedef struct FlockDetailView FlockDetailView;

/** Fired by the OK (Mark), Right (Lock In) and Left (Remove) buttons. */
typedef void (*FlockDetailActionCallback)(void* context);

FlockDetailView* flock_detail_view_alloc(void);
void flock_detail_view_free(FlockDetailView* v);
View* flock_detail_view_get_view(FlockDetailView* v);

/** Bind the ReconApp the view renders from. */
void flock_detail_view_set_app(FlockDetailView* v, void* app);

/**
 * Wire the three buttons. Any callback may be NULL.
 *
 * `del_cb` fires only after the user has confirmed the in-view removal prompt,
 * so it can delete without asking again.
 */
void flock_detail_view_set_callbacks(
    FlockDetailView* v,
    FlockDetailActionCallback mark_cb,
    FlockDetailActionCallback lock_cb,
    FlockDetailActionCallback del_cb,
    void* context);

/** Scroll back to the top (call on scene enter, so a new selection starts fresh). */
void flock_detail_view_reset(FlockDetailView* v);

/** Mark the view dirty so the next tick repaints it. */
void flock_detail_view_refresh(FlockDetailView* v);
