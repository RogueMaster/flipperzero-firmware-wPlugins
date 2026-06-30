#include <flip_world.h>
#include <flip_storage/storage.h>
char *fps_choices_str[] = {"30", "60", "120", "240"};
uint8_t fps_index = 0;
char *yes_or_no_choices[] = {"No", "Yes"};
uint8_t screen_always_on_index = 1;
uint8_t sound_on_index = 1;
uint8_t vibration_on_index = 1;
char *player_sprite_choices[] = {"naked", "sword", "axe", "bow"};
uint8_t player_sprite_index = 1;
char *vgm_levels[] = {"-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
uint8_t vgm_x_index = 2;
uint8_t vgm_y_index = 2;
uint8_t game_mode_index = 0;
float atof_(const char *nptr) { return (float)strtod(nptr, NULL); }
float atof_furi(const FuriString *nptr) { return atof_(furi_string_get_cstr(nptr)); }
bool is_str(const char *src, const char *dst) { return strcmp(src, dst) == 0; }
bool is_enough_heap(size_t heap_size, bool check_blocks)
{
    const size_t min_heap = heap_size + 1024; // 1KB buffer
    const size_t min_free = memmgr_get_free_heap();
    if (min_free < min_heap)
    {
        FURI_LOG_E(TAG, "Not enough heap memory: There are %zu bytes free.", min_free);
        return false;
    }
    if (check_blocks)
    {
        const size_t max_free_block = memmgr_heap_get_max_free_block();
        if (max_free_block < min_heap)
        {
            FURI_LOG_E(TAG, "Not enough free blocks: %zu bytes", max_free_block);
            return false;
        }
    }
    return true;
}
