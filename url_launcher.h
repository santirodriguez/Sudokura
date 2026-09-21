#ifndef SUDOKURA_URL_LAUNCHER_H
#define SUDOKURA_URL_LAUNCHER_H

#include <stdbool.h>

typedef enum {
  URL_LAUNCH_OK = 0,
  URL_LAUNCH_INVALID,
  URL_LAUNCH_UNAVAILABLE,
  URL_LAUNCH_FAILED
} UrlLaunchResult;

bool url_launcher_is_allowed(const char *url);
UrlLaunchResult url_launcher_open(const char *url);

#if defined(SUDOKURA_URL_LAUNCHER_TESTING) && !defined(_WIN32)
char **url_launcher_test_host_environment(void);
void url_launcher_test_free_environment(char **environment);
#endif

#endif
