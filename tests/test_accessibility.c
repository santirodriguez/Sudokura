#include "accessibility.h"
#include "accessibility_labels.h"
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
  for (int language=0;language<LANG_COUNT;++language) {
    const TextKey reader_keys[]={T_HELP_TOUCH_BODY,T_HELP_GOAL_BODY,T_HELP_MODES_BODY,
        T_HELP_TOUCH_HINTS,T_HELP_TOUCH_SETTINGS};
    for (size_t i=0;i<sizeof(reader_keys)/sizeof(reader_keys[0]);++i)
      assert(strlen(tr((Language)language,reader_keys[i]))<ACCESSIBILITY_LABEL_LIMIT);
    char label[ACCESSIBILITY_LABEL_LIMIT];
    accessibility_cell_label(label,(Language)language,8,7,0,false,
        (1u<<1)|(1u<<9),true,true);
    assert(strstr(label,"9") && strstr(label,"8"));
    assert(strstr(label,tr((Language)language,T_NOTES)));
    assert(strstr(label,tr((Language)language,T_A11Y_SELECTED)));
    assert(strstr(label,tr((Language)language,T_CONFLICTS)));
    accessibility_cell_label(label,(Language)language,0,0,7,true,0,false,false);
    assert(strstr(label,tr((Language)language,T_A11Y_GIVEN)));
    assert(!strstr(label,tr((Language)language,T_CONFLICTS)));
    assert(!strstr(label,tr((Language)language,T_NOTES)));
  }
  puts("Accessibility snapshot bounds, invalidation and action permissions passed");
  return 0;
}
