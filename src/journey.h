#ifndef SUDOKURA_JOURNEY_H
#define SUDOKURA_JOURNEY_H

#include "game_library.h"

/* Replay ancestry, not matching seeds, defines a journey. Missing ancestors
   retain their stable parent ID; a bounded walk refuses cycles. */
static inline GameAttemptId journey_root(const GameLibraryEntry *entries,
                                          size_t count, size_t index) {
  GameAttemptId own=entries[index].id, root=own;
  const GameLibraryEntry *current=&entries[index];
  for (size_t depth=0;depth<count;++depth) {
    if (!game_attempt_id_valid(current->parent)) return root;
    root=current->parent;
    if (game_attempt_id_equal(root,own)) return own;
    size_t next=count;
    for (size_t i=0;i<count;++i)
      if (game_attempt_id_equal(entries[i].id,root)) { next=i;break; }
    if (next==count) return root;
    const ProfileResult *a=&entries[index].result,*b=&entries[next].result;
    if (a->seed!=b->seed || a->generator_revision!=b->generator_revision ||
        a->difficulty!=b->difficulty || a->mode!=b->mode) return own;
    current=&entries[next];
  }
  return own;
}

#endif
