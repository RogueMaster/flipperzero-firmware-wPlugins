#include "include/i18n/strings.h"
#include <assert.h>
#include <string.h>

int main(void) {
  impostor_locale_set(ImpostorLocaleEn);
  assert(strcmp(impostor_str(ImpostorStrMenuPlay), "Play") == 0);
  assert(strcmp(impostor_str(ImpostorStrRoleImpostor), "IMPOSTOR") == 0);

  impostor_locale_set(ImpostorLocaleEs);
  assert(strcmp(impostor_str(ImpostorStrMenuSettings), "Ajustes") == 0);

  impostor_locale_set((ImpostorLocale)99);
  assert(impostor_locale_get() == ImpostorLocaleEn);

  return 0;
}
