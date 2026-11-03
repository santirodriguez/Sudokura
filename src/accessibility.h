#ifndef SUDOKURA_ACCESSIBILITY_H
#define SUDOKURA_ACCESSIBILITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Published values only. This contract has no solution, persistence or SDL
   pointers. The owner builds a complete snapshot before handing it to Java. */
enum { ACCESSIBILITY_NODE_LIMIT=192, ACCESSIBILITY_LABEL_LIMIT=512 };
enum { ACCESSIBILITY_ACTIVATE=1u, ACCESSIBILITY_SCROLL_FORWARD=2u,
       ACCESSIBILITY_SCROLL_BACKWARD=4u };
typedef struct {
  uint64_t generation, attempt_hi, attempt_lo;
  int screen;
} AccessibilityBinding;
typedef struct {
  int id, x, y, w, h;
  unsigned actions;
  bool enabled, selected;
  char label[ACCESSIBILITY_LABEL_LIMIT];
} AccessibilityNode;
typedef struct {
  AccessibilityBinding binding;
  size_t count;
  bool valid;
  AccessibilityNode nodes[ACCESSIBILITY_NODE_LIMIT];
} AccessibilitySnapshot;
typedef struct {
  AccessibilityBinding binding;
  int node_id;
  unsigned action;
} AccessibilityCommand;

static inline bool accessibility_binding_equal(AccessibilityBinding a,
                                               AccessibilityBinding b) {
  return a.generation==b.generation && a.attempt_hi==b.attempt_hi &&
         a.attempt_lo==b.attempt_lo && a.screen==b.screen;
}
static inline void accessibility_begin(AccessibilitySnapshot *s,
                                        AccessibilityBinding binding) {
  memset(s,0,sizeof(*s));s->binding=binding;s->valid=binding.generation!=0;
}
static inline bool accessibility_add(AccessibilitySnapshot *s,
                                      const AccessibilityNode *n) {
  if (!s || !s->valid) return false;
  if (!n || n->id<=0 || n->w<=0 || n->h<=0 || n->x<0 || n->y<0 ||
      !n->label[0] || !memchr(n->label,0,sizeof(n->label)) ||
      (n->actions & ~7u) || s->count>=ACCESSIBILITY_NODE_LIMIT) {
    s->valid=false;return false;
  }
  for (size_t i=0;i<s->count;++i) if (s->nodes[i].id==n->id) {
    s->valid=false;return false;
  }
  s->nodes[s->count++]=*n;return true;
}
/* Rebuild/reconcile on the SDL owner immediately before this check. An old
   Java snapshot never grants permission to act on a different live state. */
static inline const AccessibilityNode *accessibility_resolve(
    const AccessibilitySnapshot *live, AccessibilityCommand command) {
  if (!live || !live->valid || !command.action ||
      (command.action & (command.action-1u)) ||
      !accessibility_binding_equal(live->binding,command.binding)) return NULL;
  for (size_t i=0;i<live->count;++i) {
    const AccessibilityNode *n=&live->nodes[i];
    if (n->id==command.node_id && n->enabled &&
        (n->actions & command.action)) return n;
  }
  return NULL;
}
static inline bool accessibility_snapshot_equal(const AccessibilitySnapshot *a,
                                                 const AccessibilitySnapshot *b) {
  if (!a || !b || a->valid!=b->valid || a->count!=b->count ||
      !accessibility_binding_equal(a->binding,b->binding)) return false;
  for (size_t i=0;i<a->count;++i) {
    const AccessibilityNode *x=&a->nodes[i],*y=&b->nodes[i];
    if (x->id!=y->id || x->x!=y->x || x->y!=y->y || x->w!=y->w ||
        x->h!=y->h || x->actions!=y->actions || x->enabled!=y->enabled ||
        x->selected!=y->selected || strcmp(x->label,y->label)) return false;
  }
  return true;
}
#endif
