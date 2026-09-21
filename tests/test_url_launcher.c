#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "url_launcher.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
static const char *environment_value(char *const environment[],
                                     const char *key) {
  size_t length = strlen(key);
  for (size_t i = 0; environment && environment[i]; ++i)
    if (strncmp(environment[i], key, length) == 0 &&
        environment[i][length] == '=')
      return environment[i] + length + 1;
  return NULL;
}
#endif

int main(void) {
  assert(url_launcher_is_allowed("https://github.com/santirodriguez"));
  assert(url_launcher_is_allowed(
      "https://github.com/santirodriguez/Sudokura"));
  assert(url_launcher_is_allowed("https://santiagorodriguez.com"));
  assert(url_launcher_is_allowed("https://doi.org/10.1002/gps.5085"));
  assert(url_launcher_is_allowed(
      "https://santiagorodriguez.com/donaciones/"));

  assert(!url_launcher_is_allowed(NULL));
  assert(!url_launcher_is_allowed(""));
  assert(!url_launcher_is_allowed("http://github.com/santirodriguez"));
  assert(!url_launcher_is_allowed(
      "https://github.com/santirodriguez/Sudokura?unexpected=1"));
  assert(!url_launcher_is_allowed(
      "https://github.com/santirodriguez.evil.example"));
  assert(url_launcher_open("https://example.invalid") == URL_LAUNCH_INVALID);

#if !defined(_WIN32)
  assert(setenv("SUDOKURA_APPIMAGE", "1", 1) == 0);
  assert(setenv("SUDOKURA_HOST_LD_LIBRARY_PATH_SET", "1", 1) == 0);
  assert(setenv("SUDOKURA_HOST_LD_LIBRARY_PATH", "/host/libs", 1) == 0);
  assert(setenv("LD_LIBRARY_PATH", "/appdir/usr/lib", 1) == 0);
  assert(setenv("APPDIR", "/appdir", 1) == 0);
  assert(setenv("LIBDECOR_PLUGIN_DIR", "/appdir/decor", 1) == 0);
  assert(setenv("SUDOKURA_URL_TEST_PRESERVE", "yes", 1) == 0);
  char **environment = url_launcher_test_host_environment();
  assert(environment != NULL);
  const char *host_ld = environment_value(environment, "LD_LIBRARY_PATH");
  const char *preserved =
      environment_value(environment, "SUDOKURA_URL_TEST_PRESERVE");
  assert(host_ld != NULL);
  assert(strcmp(host_ld, "/host/libs") == 0);
  assert(environment_value(environment, "APPDIR") == NULL);
  assert(environment_value(environment, "LIBDECOR_PLUGIN_DIR") == NULL);
  assert(environment_value(environment, "SUDOKURA_APPIMAGE") == NULL);
  assert(preserved != NULL);
  assert(strcmp(preserved, "yes") == 0);
  url_launcher_test_free_environment(environment);
#endif

  puts("URL launcher allowlist and host-environment policy passed");
  return 0;
}
