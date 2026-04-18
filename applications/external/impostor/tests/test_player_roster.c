#include "include/domain/player_roster.h"
#include <assert.h>
#include <string.h>

int main(void) {
  char names[4][IMPOSTOR_MAX_NAME_LEN];
  memset(names, 0, sizeof(names));
  strcpy(names[0], "a");
  strcpy(names[1], "b");
  strcpy(names[2], "c");
  uint8_t n = 3;

  player_roster_remove_at(names, &n, 1);
  assert(n == 2);
  assert(strcmp(names[0], "a") == 0);
  assert(strcmp(names[1], "c") == 0);
  assert(names[2][0] == '\0');

  player_roster_remove_at(names, NULL, 0);
  player_roster_remove_at(names, &n, 10);
  assert(n == 2);

  return 0;
}
