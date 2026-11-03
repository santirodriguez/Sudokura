#include "accessibility.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  static AccessibilitySnapshot published,live;
  AccessibilityBinding binding={11,12,13,1};
  accessibility_begin(&published,binding);
  AccessibilityNode node={.id=1,.x=1,.y=2,.w=48,.h=48,
    .actions=ACCESSIBILITY_ACTIVATE,.enabled=true,.label="Start"};
  assert(accessibility_add(&published,&node));
  live=published;
  AccessibilityCommand command={binding,1,ACCESSIBILITY_ACTIVATE};
  assert(accessibility_resolve(&live,command));
  assert(accessibility_snapshot_equal(&published,&live));
  ++live.binding.generation;assert(!accessibility_resolve(&live,command));
  live=published;++live.binding.attempt_lo;
  assert(!accessibility_resolve(&live,command));
  live=published;++live.binding.screen;
  assert(!accessibility_resolve(&live,command));
  live=published;live.nodes[0].enabled=false;
  assert(!accessibility_resolve(&live,command));
  assert(!accessibility_snapshot_equal(&published,&live));
  live=published;command.action=ACCESSIBILITY_SCROLL_FORWARD;
  assert(!accessibility_resolve(&live,command));
  command.action=3;assert(!accessibility_resolve(&live,command));
  command.action=0;assert(!accessibility_resolve(&live,command));
  assert(!accessibility_add(&live,&node));assert(!live.valid);
  accessibility_begin(&live,binding);
  for(int id=1;id<=ACCESSIBILITY_NODE_LIMIT;++id) {
    node.id=id;assert(accessibility_add(&live,&node));
  }
  ++node.id;assert(!accessibility_add(&live,&node));assert(!live.valid);
  accessibility_begin(&live,binding);memset(node.label,'x',sizeof node.label);
  assert(!accessibility_add(&live,&node));assert(!live.valid);
  puts("Accessibility snapshot bounds, invalidation and action permissions passed");
  return 0;
}
