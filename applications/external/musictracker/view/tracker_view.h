#include <gui/view.h>
#include "../tracker_engine/tracker.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TrackerView TrackerView;

TrackerView* tracker_view_alloc();

void tracker_view_free(TrackerView* tracker_view);

View* tracker_view_get_view(TrackerView* tracker_view);

void tracker_view_set_song(TrackerView* tracker_view, const Song* song);

void tracker_view_set_position(TrackerView* tracker_view, uint8_t order_list_index, uint8_t row);

#ifdef __cplusplus
}
#endif
