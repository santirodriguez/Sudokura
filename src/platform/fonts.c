#include "platform/fonts.h"

#include <SDL2/SDL_ttf.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#elif defined(__APPLE__)
#include <dirent.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool ends_withi(const char *s, const char *suffix) {
  size_t ns = strlen(s);
  size_t ms = strlen(suffix);
  if (ms > ns) {
    return false;
  }
  for (size_t i = 0; i < ms; ++i) {
    char a = s[ns - ms + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') {
      a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = (char)(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

static bool try_open_font_path(const char *path) {
  if (!path) {
    return false;
  }
  TTF_Font *tmp = TTF_OpenFont(path, 18);
  if (tmp) {
    TTF_CloseFont(tmp);
    return true;
  }
  return false;
}

static bool try_candidates(char out[PATH_MAX], const char *name) {
  const char *dirs[16];
  int n = 0;
#if defined(_WIN32)
  dirs[n++] = "C:\\Windows\\Fonts";
  const char *local = getenv("LOCALAPPDATA");
  if (local) {
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s\\Microsoft\\Windows\\Fonts", local);
    dirs[n++] = buf;
  }
#else
  dirs[n++] = "/usr/share/fonts";
  dirs[n++] = "/usr/local/share/fonts";
  const char *home = getenv("HOME");
  static char buf1[PATH_MAX];
  static char buf2[PATH_MAX];
  if (home) {
    snprintf(buf1, sizeof(buf1), "%s/.local/share/fonts", home);
    dirs[n++] = buf1;
    snprintf(buf2, sizeof(buf2), "%s/.fonts", home);
    dirs[n++] = buf2;
  }
#if defined(__APPLE__)
  dirs[n++] = "/System/Library/Fonts";
  dirs[n++] = "/Library/Fonts";
  if (home) {
    static char buf3[PATH_MAX];
    snprintf(buf3, sizeof(buf3), "%s/Library/Fonts", home);
    dirs[n++] = buf3;
  }
#endif
#endif
  for (int i = 0; i < n; ++i) {
    char p[PATH_MAX];
#if defined(_WIN32)
    snprintf(p, sizeof(p), "%s\\%s", dirs[i], name);
#else
    snprintf(p, sizeof(p), "%s/%s", dirs[i], name);
#endif
#if defined(_WIN32)
    for (char *c = p; *c; ++c) {
      if (*c == '/') {
        *c = '\\';
      }
    }
#endif
    if (try_open_font_path(p)) {
      strncpy(out, p, PATH_MAX);
      out[PATH_MAX - 1] = 0;
      return true;
    }
  }
  return false;
}

#if defined(_WIN32)
static bool search_dir_win(const char *dir, char out[PATH_MAX]) {
  char pattern[PATH_MAX];
  snprintf(pattern, sizeof(pattern), "%s\\*.*", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }
  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..")==0) {
      continue;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (search_dir_win(path, out)) {
        FindClose(h);
        return true;
      }
    } else if (ends_withi(path, ".ttf") || ends_withi(path, ".otf")) {
      if (try_open_font_path(path)) {
        strncpy(out, path, PATH_MAX);
        out[PATH_MAX - 1] = 0;
        FindClose(h);
        return true;
      }
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  return false;
}
#else
static bool search_dir_posix(const char *dir, char out[PATH_MAX]) {
  DIR *d = opendir(dir);
  if (!d) {
    return false;
  }
  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.') {
      continue;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
    struct stat st;
    if (stat(path, &st) != 0) {
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      if (search_dir_posix(path, out)) {
        closedir(d);
        return true;
      }
    } else if (ends_withi(path, ".ttf") || ends_withi(path, ".otf")) {
      if (try_open_font_path(path)) {
        strncpy(out, path, PATH_MAX);
        out[PATH_MAX - 1] = 0;
        closedir(d);
        return true;
      }
    }
  }
  closedir(d);
  return false;
}
#endif

static bool get_exe_dir(char out[PATH_MAX]) {
#if defined(_WIN32)
  char buf[PATH_MAX];
  DWORD n = GetModuleFileNameA(NULL, buf, PATH_MAX);
  if (n == 0 || n >= PATH_MAX) {
    return false;
  }
  for (DWORD i = 0; i < n; ++i) {
    if (buf[i] == '/') {
      buf[i] = '\\';
    }
  }
  char *slash = strrchr(buf, '\\');
  if (!slash) {
    return false;
  }
  *slash = '\0';
  strncpy(out, buf, PATH_MAX);
  out[PATH_MAX - 1] = 0;
  return true;
#elif defined(__APPLE__)
  char buf[PATH_MAX];
  uint32_t size = (uint32_t)sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) != 0) {
    return false;
  }
  char *slash = strrchr(buf, '/');
  if (!slash) {
    return false;
  }
  *slash = '\0';
  strncpy(out, buf, PATH_MAX);
  out[PATH_MAX - 1] = 0;
  return true;
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) {
    return false;
  }
  buf[n] = 0;
  char *slash = strrchr(buf, '/');
  if (!slash) {
    return false;
  }
  *slash = '\0';
  strncpy(out, buf, PATH_MAX);
  out[PATH_MAX - 1] = 0;
  return true;
#endif
}

static bool try_in_dir(const char *dir, const char *name, char out[PATH_MAX]) {
  if (!dir || !name) {
    return false;
  }
  char p[PATH_MAX];
#if defined(_WIN32)
  snprintf(p, sizeof(p), "%s\\%s", dir, name);
#else
  snprintf(p, sizeof(p), "%s/%s", dir, name);
#endif
  if (try_open_font_path(p)) {
    strncpy(out, p, PATH_MAX);
    out[PATH_MAX - 1] = 0;
    return true;
  }
  return false;
}

const char *find_font_path_dynamic(char out[PATH_MAX], const char *cli_override) {
  if (cli_override && try_open_font_path(cli_override)) {
    strncpy(out, cli_override, PATH_MAX);
    out[PATH_MAX - 1] = 0;
    return out;
  }

  const char *local_first[] = {"NotoSans-Regular.ttf", "DejaVuSans.ttf",
                               "DejaVuSans-Regular.ttf", "Arial.ttf"};
  char exe_dir[PATH_MAX] = {0};
  bool have_exe_dir = get_exe_dir(exe_dir);
  for (size_t i = 0; i < sizeof(local_first) / sizeof(local_first[0]); ++i) {
    if (try_open_font_path(local_first[i])) {
      strncpy(out, local_first[i], PATH_MAX);
      out[PATH_MAX - 1] = 0;
      return out;
    }
    if (have_exe_dir && try_in_dir(exe_dir, local_first[i], out)) {
      return out;
    }
  }

  const char *preferred[] = {"DejaVuSans.ttf",        "DejaVuSans-Regular.ttf",
                              "NotoSans-Regular.ttf", "LiberationSans-Regular.ttf",
                              "FreeSans.ttf",         "Arial.ttf",
                              "Ubuntu-R.ttf",         "Cantarell-VF.otf",
                              "SFNS.ttf"};
  for (size_t i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i) {
    if (try_candidates(out, preferred[i])) {
      return out;
    }
  }

#if defined(_WIN32)
  const char *roots[] = {"C:\\Windows\\Fonts", getenv("LOCALAPPDATA")};
  for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
    if (roots[i] && roots[i][0]) {
      char start[PATH_MAX];
      if (i == 1) {
        snprintf(start, sizeof(start), "%s\\Microsoft\\Windows\\Fonts", roots[i]);
      } else {
        snprintf(start, sizeof(start), "%s", roots[i]);
      }
      if (search_dir_win(start, out)) {
        return out;
      }
    }
  }
#else
  const char *home = getenv("HOME");
  const char *roots[] = {"/usr/share/fonts", "/usr/local/share/fonts", home,
#if defined(__APPLE__)
                          "/System/Library/Fonts", "/Library/Fonts"
#endif
  };
  for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
    if (!roots[i]) {
      continue;
    }
    char start[PATH_MAX];
    if (home && roots[i] == home) {
      snprintf(start, sizeof(start), "%s/.local/share/fonts", home);
      if (search_dir_posix(start, out)) {
        return out;
      }
      snprintf(start, sizeof(start), "%s/.fonts", home);
      if (search_dir_posix(start, out)) {
        return out;
      }
#if defined(__APPLE__)
      snprintf(start, sizeof(start), "%s/Library/Fonts", home);
      if (search_dir_posix(start, out)) {
        return out;
      }
#endif
    } else {
      snprintf(start, sizeof(start), "%s", roots[i]);
      if (search_dir_posix(start, out)) {
        return out;
      }
    }
  }
#endif
  return NULL;
}
