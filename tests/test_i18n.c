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
    assert(count_token(tr((Language)language, T_HELP_BODY), "%s") == 4);
  }
  puts("EN/ES/CA localization coverage and UTF-8 validation passed");
  return 0;
}
