#ifndef SUDOKURA_LIBRARY_PAGING_H
#define SUDOKURA_LIBRARY_PAGING_H

#include <stdbool.h>
#include <stddef.h>

/* PageUp/PageDown and visible buttons share the same bounded selection rule. */
static inline bool library_page_enabled(size_t count, size_t selected,
                                        size_t visible, int direction) {
  return direction != 0 && visible > 0 && count > visible &&
         selected < count &&
         (direction < 0 ? selected > 0 : selected + 1u < count);
}

static inline size_t library_page_target(size_t count, size_t selected,
                                         size_t visible, int direction) {
  if (!library_page_enabled(count, selected, visible, direction))
    return selected;
  if (direction < 0) return selected > visible ? selected - visible : 0;
  return count - 1u - selected <= visible ? count - 1u : selected + visible;
}

#endif
