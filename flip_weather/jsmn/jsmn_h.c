#include <jsmn/jsmn_h.h>
FuriString *char_to_furi_string(const char *str)
{
    FuriString *furi_str = furi_string_alloc();
    if (!furi_str)
    {
        return NULL;
    }
    for (size_t i = 0; i < strlen(str); i++)
    {
        furi_string_push_back(furi_str, str[i]);
    }
    return furi_str;
}
bool jsmn_memory_check(size_t heap_size) { return memmgr_get_free_heap() > (heap_size + 1024); }