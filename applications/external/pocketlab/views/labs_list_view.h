#pragma once

#include <gui/view.h>

typedef struct LabsListView LabsListView;

typedef void (*LabsListViewCallback)(void* context, uint32_t index);

LabsListView* labs_list_view_alloc(void);

void labs_list_view_free(LabsListView* instance);

View* labs_list_view_get_view(LabsListView* instance);

void labs_list_view_set_callback(
    LabsListView* instance,
    LabsListViewCallback callback,
    void* context);

/** Set which labs are completed and where the cursor should start. */
void labs_list_view_configure(LabsListView* instance, uint32_t completed_mask, uint32_t selected);

/** Current cursor position, so the scene can remember it. */
uint32_t labs_list_view_get_selected(LabsListView* instance);
