#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/** LoadingCancellable anonymous structure */
typedef struct LoadingCancellable LoadingCancellable;

/** Allocate and initialize
 *
 * This View used to show system is doing some processing
 *
 * @return     LoadingCancellable View instance
 */
LoadingCancellable* loading_cancellable_alloc(void);

/** Deinitialize and free LoadingCancellable View
 *
 * @param      instance  LoadingCancellable instance
 */
void loading_cancellable_free(LoadingCancellable* instance);

/** Get LoadingCancellable view
 *
 * @param      instance  LoadingCancellable instance
 *
 * @return     View instance that can be used for embedding
 */
View* loading_cancellable_get_view(LoadingCancellable* instance);

#ifdef __cplusplus
}
#endif
