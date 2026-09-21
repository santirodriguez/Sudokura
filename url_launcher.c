#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "url_launcher.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const char *const allowed_urls[] = {
    "https://github.com/santirodriguez",
    "https://github.com/santirodriguez/Sudokura",
    "https://santiagorodriguez.com",
    "https://doi.org/10.1002/gps.5085",
    "https://santiagorodriguez.com/donate/",
    "https://santiagorodriguez.com/donaciones/",
    "https://santiagorodriguez.com/donacions/",
};

bool url_launcher_is_allowed(const char *url) {
  if (!url || !url[0]) return false;
  for (size_t i = 0; i < sizeof(allowed_urls) / sizeof(allowed_urls[0]); ++i)
    if (strcmp(url, allowed_urls[i]) == 0) return true;
  return false;
}

#if defined(_WIN32)

#include <windows.h>
#include <shellapi.h>

UrlLaunchResult url_launcher_open(const char *url) {
  if (!url_launcher_is_allowed(url)) return URL_LAUNCH_INVALID;
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url, -1,
                                  NULL, 0);
  if (count <= 0) return URL_LAUNCH_FAILED;
  wchar_t *wide = (wchar_t *)calloc((size_t)count, sizeof(*wide));
  if (!wide) return URL_LAUNCH_FAILED;
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url, -1, wide,
                          count) != count) {
    free(wide);
    return URL_LAUNCH_FAILED;
  }
  HINSTANCE result = ShellExecuteW(NULL, L"open", wide, NULL, NULL,
                                   SW_SHOWNORMAL);
  free(wide);
  return (INT_PTR)result > 32 ? URL_LAUNCH_OK : URL_LAUNCH_FAILED;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern char **environ;

static bool env_has_key(const char *entry, const char *key) {
  size_t n = strlen(key);
  return strncmp(entry, key, n) == 0 && entry[n] == '=';
}

static bool appimage_private_key(const char *entry) {
  static const char *const keys[] = {
      "APPDIR", "APPIMAGE", "ARGV0", "OWD", "LIBDECOR_PLUGIN_DIR",
      "SUDOKURA_APPIMAGE", "SUDOKURA_HOST_LD_LIBRARY_PATH",
      "SUDOKURA_HOST_LD_LIBRARY_PATH_SET",
  };
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
    if (env_has_key(entry, keys[i])) return true;
  return false;
}

static char *copy_string(const char *value) {
  size_t n = strlen(value) + 1;
  char *copy = (char *)malloc(n);
  if (copy) memcpy(copy, value, n);
  return copy;
}

static char **host_environment(void) {
  bool appimage = getenv("SUDOKURA_APPIMAGE") != NULL;
  bool restore_ld = appimage &&
      getenv("SUDOKURA_HOST_LD_LIBRARY_PATH_SET") != NULL &&
      strcmp(getenv("SUDOKURA_HOST_LD_LIBRARY_PATH_SET"), "1") == 0;
  const char *host_ld = getenv("SUDOKURA_HOST_LD_LIBRARY_PATH");
  size_t count = 0;
  while (environ[count]) ++count;
  char **result = (char **)calloc(count + 2, sizeof(*result));
  if (!result) return NULL;

  size_t used = 0;
  for (size_t i = 0; i < count; ++i) {
    if (appimage &&
        (appimage_private_key(environ[i]) ||
         env_has_key(environ[i], "LD_LIBRARY_PATH")))
      continue;
    result[used] = copy_string(environ[i]);
    if (!result[used]) goto fail;
    ++used;
  }
  if (restore_ld) {
    if (!host_ld) host_ld = "";
    const char prefix[] = "LD_LIBRARY_PATH=";
    size_t n = sizeof(prefix) - 1 + strlen(host_ld) + 1;
    result[used] = (char *)malloc(n);
    if (!result[used]) goto fail;
    snprintf(result[used], n, "%s%s", prefix, host_ld);
    ++used;
  }
  result[used] = NULL;
  return result;

fail:
  for (size_t i = 0; i < used; ++i) free(result[i]);
  free(result);
  return NULL;
}

static void free_environment(char **environment) {
  if (!environment) return;
  for (size_t i = 0; environment[i]; ++i) free(environment[i]);
  free(environment);
}

#if defined(SUDOKURA_URL_LAUNCHER_TESTING)
char **url_launcher_test_host_environment(void) {
  return host_environment();
}

void url_launcher_test_free_environment(char **environment) {
  free_environment(environment);
}
#endif

#if !defined(__APPLE__)
static bool executable_path(const char *name, char *out, size_t out_size) {
  const char *path = getenv("PATH");
  if (!path || !path[0]) path = "/usr/local/bin:/usr/bin:/bin";
  const char *part = path;
  while (part) {
    const char *separator = strchr(part, ':');
    size_t length = separator ? (size_t)(separator - part) : strlen(part);
    const char *directory = length ? part : ".";
    int written = length
                      ? snprintf(out, out_size, "%.*s/%s", (int)length,
                                 directory, name)
                      : snprintf(out, out_size, "./%s", name);
    if (written > 0 && (size_t)written < out_size && access(out, X_OK) == 0)
      return true;
    part = separator ? separator + 1 : NULL;
  }
  return false;
}
#endif

static UrlLaunchResult launch_detached(const char *program, char *const argv[],
                                       char *const environment[]) {
  int status_pipe[2];
  if (pipe(status_pipe) != 0) return URL_LAUNCH_FAILED;
  int descriptor_flags = fcntl(status_pipe[1], F_GETFD);
  if (descriptor_flags < 0 ||
      fcntl(status_pipe[1], F_SETFD, descriptor_flags | FD_CLOEXEC) != 0) {
    close(status_pipe[0]);
    close(status_pipe[1]);
    return URL_LAUNCH_FAILED;
  }

  pid_t child = fork();
  if (child < 0) {
    close(status_pipe[0]);
    close(status_pipe[1]);
    return URL_LAUNCH_FAILED;
  }
  if (child == 0) {
    close(status_pipe[0]);
    pid_t detached = fork();
    if (detached < 0) {
      int error = errno;
      ssize_t reported = write(status_pipe[1], &error, sizeof(error));
      (void)reported;
      _exit(127);
    }
    if (detached > 0) _exit(0);
    (void)setsid();
    execve(program, argv, environment);
    int error = errno;
    ssize_t reported = write(status_pipe[1], &error, sizeof(error));
    (void)reported;
    _exit(127);
  }

  close(status_pipe[1]);
  int wait_status = 0;
  while (waitpid(child, &wait_status, 0) < 0 && errno == EINTR) {}
  int launch_error = 0;
  ssize_t bytes;
  do {
    bytes = read(status_pipe[0], &launch_error, sizeof(launch_error));
  } while (bytes < 0 && errno == EINTR);
  close(status_pipe[0]);
  return bytes == 0 ? URL_LAUNCH_OK : URL_LAUNCH_FAILED;
}

UrlLaunchResult url_launcher_open(const char *url) {
  if (!url_launcher_is_allowed(url)) return URL_LAUNCH_INVALID;

  char program[PATH_MAX];
  char *arguments[4] = {NULL, NULL, NULL, NULL};
#if defined(__APPLE__)
  const char *candidate = "/usr/bin/open";
  if (access(candidate, X_OK) != 0) return URL_LAUNCH_UNAVAILABLE;
  snprintf(program, sizeof(program), "%s", candidate);
  arguments[0] = program;
  arguments[1] = (char *)url;
#else
  if (executable_path("xdg-open", program, sizeof(program))) {
    arguments[0] = program;
    arguments[1] = (char *)url;
  } else if (executable_path("gio", program, sizeof(program))) {
    arguments[0] = program;
    arguments[1] = "open";
    arguments[2] = (char *)url;
  } else {
    return URL_LAUNCH_UNAVAILABLE;
  }
#endif

  char **environment = host_environment();
  if (!environment) return URL_LAUNCH_FAILED;
  UrlLaunchResult result =
      launch_detached(program, arguments, environment);
  free_environment(environment);
  return result;
}

#endif
