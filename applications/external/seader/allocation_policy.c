#include "allocation_policy.h"

#include <stdint.h>

bool seader_size_multiply_checked(size_t count, size_t size, size_t* out) {
    if(!out) {
        return false;
    }

    if(count != 0U && size > SIZE_MAX / count) {
        return false;
    }

    *out = count * size;
    return true;
}
