#ifndef SUDOKURA_ACCESSIBILITY_LABELS_H
#define SUDOKURA_ACCESSIBILITY_LABELS_H
#include <stdio.h>
#include "accessibility.h"
#include "i18n.h"

/* Only caller-supplied visible data: no Game/solution pointer in this seam. */
static inline void accessibility_cell_label(char out[ACCESSIBILITY_LABEL_LIMIT],
    Language language,int row,int column,int value,bool given,uint16_t notes,
    bool selected,bool conflict) {
  char position[80],content[80],note_text[48]="";
  (void)snprintf(position,sizeof(position),tr(language,T_A11Y_CELL_FMT),row+1,column+1);
  if (value) (void)snprintf(content,sizeof(content),"%s %d",
      tr(language,given ? T_A11Y_GIVEN : T_A11Y_ENTERED),value);
  else (void)snprintf(content,sizeof(content),"%s",tr(language,T_A11Y_EMPTY));
  if (!value && notes) {
    size_t used=0;
    for (int n=1;n<=9;++n) if (notes & (1u<<n))
      used+=(size_t)snprintf(note_text+used,sizeof(note_text)-used," %d",n);
  }
  (void)snprintf(out,ACCESSIBILITY_LABEL_LIMIT,"%s. %s%s%s%s%s%s%s%s",
      position,content,note_text[0]?". ":"",note_text[0]?tr(language,T_NOTES):"",
      note_text,selected?". ":"",selected?tr(language,T_A11Y_SELECTED):"",
      conflict?". ":"",conflict?tr(language,T_CONFLICTS):"");
}
#endif
