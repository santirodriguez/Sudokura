#include "library_paging.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
  assert(!library_page_enabled(0, 0, 5, 1));
  assert(!library_page_enabled(1, 0, 5, -1));
  assert(!library_page_enabled(1, 0, 5, 1));
  assert(!library_page_enabled(5, 2, 5, 1));
  assert(!library_page_enabled(5, 2, 5, -1));
  assert(!library_page_enabled(8, 8, 5, 1));
  assert(!library_page_enabled(8, 0, 0, 1));
  assert(library_page_target(1000, 0, 5, 1) == 5);
  assert(library_page_target(1000, 5, 5, -1) == 0);
  assert(library_page_target(1000, 999, 5, -1) == 994);
  assert(library_page_target(6, 0, 5, 1) == 5);
  assert(library_page_target(6, 5, 5, 1) == 5);
  assert(library_page_target(6, 3, 5, 1) == 5);
  assert(library_page_target(6, 1, 5, -1) == 0);
  assert(!library_page_enabled(6, 0, 5, -1));
  assert(!library_page_enabled(6, 5, 5, 1));
  puts("Library page boundary tests passed");
  return 0;
}
