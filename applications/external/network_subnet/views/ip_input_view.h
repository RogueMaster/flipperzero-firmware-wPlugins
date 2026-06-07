
#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>

typedef struct {
    uint8_t octets_and_cidr_digits[14]; // each digit in ip/cidr format
    uint8_t active_digit; // 0 ... 14 (12 of ip + 2 of CIDR)
} IpInputViewModel;

View* ip_input_view_alloc(void* context);
void ip_input_view_free(View* view);
