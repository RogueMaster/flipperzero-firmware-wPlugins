/* Counts what the app allocates, so a test can measure a long exchange.
 *
 * The app's sources see these through the macros in the furi shim. This file
 * does not, so it can reach the real allocator.
 */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

unsigned seos_test_allocation_count(void);
size_t seos_test_allocation_live(void);
void seos_test_allocation_reset(void);
void* seos_test_malloc(size_t size);
void* seos_test_calloc(size_t count, size_t size);
void seos_test_free(void* ptr);

static unsigned count;
static size_t live;

/* The size goes in a header ahead of what the caller sees, so a free knows
 * how much it is giving back. */
typedef struct {
    size_t size;
} Header;

void* seos_test_malloc(size_t size) {
    Header* header = malloc(sizeof(Header) + size);
    if(!header) return NULL;

    header->size = size;
    count++;
    live += size;
    return header + 1;
}

void* seos_test_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void* ptr = seos_test_malloc(total);
    if(ptr) memset(ptr, 0, total);
    return ptr;
}

void seos_test_free(void* ptr) {
    if(!ptr) return;

    Header* header = ((Header*)ptr) - 1;
    live -= header->size;
    free(header);
}

unsigned seos_test_allocation_count(void) {
    return count;
}

size_t seos_test_allocation_live(void) {
    return live;
}

void seos_test_allocation_reset(void) {
    count = 0;
    live = 0;
}
