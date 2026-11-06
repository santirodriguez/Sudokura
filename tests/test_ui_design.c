#include "ui_design.h"
#include "i18n.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static double linear(unsigned channel) {
  double s=(double)channel/255.0;
  return s<=0.04045?s/12.92:pow((s+0.055)/1.055,2.4);
}
static double luminance(UiDesignColor c) {
  return 0.2126*linear(c.r)+0.7152*linear(c.g)+0.0722*linear(c.b);
}
static double contrast(UiDesignColor a,UiDesignColor b) {
  double x=luminance(a),y=luminance(b);
  return (fmax(x,y)+0.05)/(fmin(x,y)+0.05);
}
static void test_themes(void) {
  for(int dark=0;dark<2;++dark) {
    UiDesignTheme t=ui_design_theme(dark!=0);
    const UiDesignColor surfaces[]={t.background_top,t.background_bottom,t.surface,t.surface_top};
    for(unsigned i=0;i<sizeof(surfaces)/sizeof(surfaces[0]);++i) {
      assert(contrast(t.text,surfaces[i])>=4.5);
      assert(contrast(t.muted,surfaces[i])>=4.5);
      assert(contrast(t.focus,surfaces[i])>=3.0);
    }
    assert(contrast(t.on_primary,t.primary)>=4.5);
    assert(contrast(t.on_primary,t.primary_top)>=4.5);
    for(int role=UI_DESIGN_SECONDARY;role<=UI_DESIGN_CELEBRATION;++role) {
      for(unsigned flags=0;flags<16;++flags) {
        UiDesignState s={(flags&1)!=0,(flags&2)!=0,(flags&4)!=0,(flags&8)!=0};
        UiDesignButton b=ui_design_button_style(t,(UiDesignRole)role,s);
        assert(b.displacement==((s.pressed&&!s.disabled)?1:0));
        assert(b.focus==(s.focused&&!s.disabled));
        if(!s.disabled) {
          if (contrast(b.text,b.top)<4.5 || contrast(b.text,b.bottom)<4.5) fprintf(stderr,"contrast dark=%d role=%d flags=%u top=%.3f bottom=%.3f\n",dark,role,flags,contrast(b.text,b.top),contrast(b.text,b.bottom));
          assert(contrast(b.text,b.top)>=4.5);
          assert(contrast(b.text,b.bottom)>=4.5);
        }
      }
    }
  }
}
static void test_label_bounds(void) {
  for(int w=1;w<=900;++w)for(int h=1;h<=100;++h)for(int icon=0;icon<2;++icon) {
    UiDesignLabel b=ui_design_label_layout(w,h,icon!=0);
    assert(b.text.w>=0&&b.text.h>=0);
    assert(b.text.x>=0&&b.text.y>=0);
    assert(b.text.x+b.text.w<=w&&b.text.y+b.text.h<=h);
    if(b.icon.w) {
      assert(b.icon.x>=0&&b.icon.y>=0);
      assert(b.icon.x+b.icon.w<b.text.x);
      assert(b.icon.y+b.icon.h<=h);
      assert(b.icon.w==b.icon.h&&b.icon.w>=20);
    }
  }
  assert(ui_design_label_layout(160,48,true).icon.w==24);
  assert(ui_design_label_layout(161,48,true).text.w==111);
  assert(ui_design_label_layout(90,48,true).icon.w==0);
  assert(ui_design_label_layout(360,0,true).text.h==0);
}
static void test_credit(void) {
  for(int language=0;language<LANG_COUNT;++language) {
    const char *credit=tr_publisher_credit((Language)language);
    assert(strstr(credit,"Caja Personal"));
    assert(strstr(credit,"GPLv3"));
    assert(!strstr(credit,"santirodriguez"));
  }
  assert(strcmp(tr_publisher_credit((Language)-1),tr_publisher_credit(LANG_EN))==0);
  assert(strcmp(tr_publisher_credit((Language)LANG_COUNT),tr_publisher_credit(LANG_EN))==0);
}
int main(void) {
  test_themes();test_label_bounds();test_credit();
  puts("Unified UI tokens, contrast, button states, label bounds and credits: PASS");
  return 0;
}
