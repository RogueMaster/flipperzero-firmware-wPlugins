#include "include/domain/player_roster.h"
#include <string.h>

void player_roster_remove_at(char names[][IMPOSTOR_MAX_NAME_LEN],
                             uint8_t *name_count, uint8_t index) {
  if (!name_count || *name_count == 0u || index >= *name_count) {
    return;
  }
  for (uint8_t j = index; j + 1u < *name_count; ++j) {
    memcpy(names[j], names[j + 1u], IMPOSTOR_MAX_NAME_LEN);
  }
  memset(names[*name_count - 1u], 0, IMPOSTOR_MAX_NAME_LEN);
  (*name_count)--;
}
