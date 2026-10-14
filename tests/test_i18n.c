#include "i18n.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int valid_utf8(const unsigned char *s) {
  while (*s) {
    if (*s < 0x80) { ++s; continue; }
    int need = 0;
    if ((*s & 0xE0) == 0xC0) need = 1;
    else if ((*s & 0xF0) == 0xE0) need = 2;
    else if ((*s & 0xF8) == 0xF0) need = 3;
    else return 0;
    unsigned char lead = *s++;
    if ((need == 1 && lead < 0xC2) || (need == 3 && lead > 0xF4)) return 0;
    for (int i = 0; i < need; ++i)
      if ((s[i] & 0xC0) != 0x80) return 0;
    s += need;
  }
  return 1;
}

static int count_token(const char *text, const char *token) {
  int count = 0; size_t n = strlen(token);
  for (const char *p = text; (p = strstr(p, token)) != NULL; p += n) ++count;
  return count;
}

int main(void) {
  for (int language = 0; language < LANG_COUNT; ++language) {
    const char *name = language_name((Language)language);
    assert(name && *name && valid_utf8((const unsigned char *)name));
    for (int key = 0; key < T_COUNT; ++key) {
      const char *text = tr((Language)language, (TextKey)key);
      assert(text && *text);
      assert(valid_utf8((const unsigned char *)text));
      assert(strstr(text, "\xEF\xBF\xBD") == NULL);
    }
    assert(count_token(tr((Language)language, T_HELP_SHORTCUTS_BODY), "%s") == 4);
    assert(strcmp(tr((Language)language, T_ENABLED),
                  tr((Language)language, T_DISABLED)) != 0);
    assert(strcmp(tr((Language)language, T_MUTED),
                  tr((Language)language, T_DISABLED)) != 0);
    assert(strstr(tr((Language)language, T_HELP_GOAL_BODY), "9x9") != NULL);
    assert(*tr((Language)language, T_HELP_MODES_TITLE));
    assert(*tr((Language)language, T_HELP_CONTROLS_TITLE));
    assert(*tr((Language)language, T_HELP_HINTS_TITLE));
    assert(*tr((Language)language, T_HELP_SHORTCUTS_TITLE));
    assert(*tr((Language)language, T_GAMES));
    assert(*tr((Language)language, T_GAMES_EMPTY));
    assert(*tr((Language)language, T_GAMES_LOADING));
    assert(*tr((Language)language, T_GAMES_UNAVAILABLE));
    assert(*tr((Language)language, T_GAMES_LOAD));
    assert(*tr((Language)language, T_GAMES_REPLAY));
    assert(*tr((Language)language, T_GAMES_REPLAY_PENDING_OTHER));
    assert(*tr((Language)language, T_GAMES_REPLAY_PENDING_CONFLICT));
    assert(*tr((Language)language, T_GAMES_REPLAY_FINALIZE_FAILED));
    assert(*tr((Language)language, T_GAMES_DELETE));
    assert(*tr((Language)language, T_GAMES_DELETE_PROMPT));
    assert(*tr((Language)language, T_GAMES_DELETE_PENDING));
    assert(*tr((Language)language, T_GAMES_ACTION_FAILED));
    assert(*tr((Language)language, T_GAMES_PRACTICE));
    assert(*tr((Language)language, T_GAMES_SOURCE_DAILY));
    assert(*tr((Language)language, T_GAMES_FINISHED_AT));
    assert(*tr((Language)language, T_GAMES_UNKNOWN_DATE));
    assert(*tr((Language)language, T_GAMES_KEEP_PROMPT));
    assert(*tr((Language)language, T_GAMES_RETRY_SAVE_PROMPT));
    assert(*tr((Language)language, T_GAMES_SAFE_DISCARD_PROMPT));
    assert(*tr((Language)language, T_GAMES_ACTIVATE_RETRY_PROMPT));
    assert(*tr((Language)language, T_DAILY_SAVE_RETRY_PROMPT));
    assert(*tr((Language)language, T_DAILY_BACK_UNCONFIRMED));
    assert(strcmp(tr((Language)language, T_GAMES_RETRY_SAVE_PROMPT),
                  tr((Language)language, T_GAMES_SAFE_DISCARD_PROMPT)) != 0);
    assert(*tr((Language)language, T_GAMES_NEW_PROMPT));
    assert(*tr((Language)language, T_GAMES_RECOVER_LABEL));
    assert(*tr((Language)language, T_GAMES_RECOVER_HINT));
    assert(*tr((Language)language, T_GAMES_RECOVER_PROMPT));
    assert(*tr((Language)language, T_GAMES_RECOVERED));
    assert(*tr((Language)language, T_UNSAVED_HOME));
    assert(*tr((Language)language, T_PROFILE_RECOVER_IMPORT_PROMPT));
    assert(*tr((Language)language, T_PROFILE_RECOVER_COLLECTION_PROMPT));

    assert(*tr((Language)language, T_DAILY_CHECKING));
    assert(*tr((Language)language, T_DAILY_CHECK_CANCEL));
    assert(*tr((Language)language, T_DAILY_CHOOSE_TITLE));
    assert(*tr((Language)language, T_DAILY_CHOOSE_PROMPT));
    assert(*tr((Language)language, T_DAILY_READ_ERROR));
    assert(*tr((Language)language, T_DAILY_DUPLICATE));
    assert(*tr((Language)language, T_DAILY_DATE_CHANGED));
    assert(*tr((Language)language, T_DAILY_RECOVER_PREVIOUS));
    assert(strcmp(tr((Language)language, T_DAILY_OLDER),
                  tr((Language)language, T_DAILY_TODAY)) != 0);
    assert(strchr(tr((Language)language, T_DAILY_OLDER), '%') == NULL);
    assert(strchr(tr((Language)language, T_DAILY_TODAY), '%') == NULL);
    assert(*tr((Language)language, T_SAVE));
    assert(*tr((Language)language, T_SAVED));
    assert(strstr(tr((Language)language, T_HELP_SHORTCUTS_BODY),
                  "Ctrl/Cmd+S") != NULL);
    assert(strstr(tr((Language)language, T_HELP_SHORTCUTS_BODY),
                  "Ctrl/Cmd+O") != NULL);
  }
  puts("EN/ES/CA semantic states, structured Help and UTF-8 validation passed");
  return 0;
}
