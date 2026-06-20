
#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>

typedef struct {
    char network[16]; // "192.168.1.0"
    char broadcast[16]; // "192.168.1.255"
    char subnet_mask[16]; // "255.255.255.0"
    uint8_t cidr; // 24  (to display "/24")
    uint32_t host_count;
} ResultViewModel;

View* result_view_alloc(void);
void result_view_free(View* view);
