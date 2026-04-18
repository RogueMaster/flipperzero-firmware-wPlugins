#include "include/app/role_card_text.h"
#include "include/domain/word_bank.h"
#include "include/i18n/strings.h"
#include <stdio.h>

void role_card_format(char *buf, size_t buf_len, const GameSession *session) {
  if (!buf || buf_len == 0u || !session) {
    return;
  }
  const char *word = NULL;
  const char *hint = NULL;
  word_bank_get_pair(session->setup.word_index, impostor_locale_get(), &word,
                     &hint);
  if (session_reveal_is_impostor(session)) {
    snprintf(buf, buf_len, "\e#%s\e#\n\n%s\n%s\n%s",
             impostor_str(ImpostorStrRoleImpostor),
             impostor_str(ImpostorStrRoleHintLabel), hint,
             impostor_str(ImpostorStrRoleCardFooter));
  } else {
    snprintf(buf, buf_len, "%s\n\n\e#%s\e#\n%s",
             impostor_str(ImpostorStrRoleWordLabel), word,
             impostor_str(ImpostorStrRoleCardFooter));
  }
}
