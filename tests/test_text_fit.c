#include "geometry.h"
#include "i18n.h"
#define SDL_MAIN_HANDLED
#include <SDL2/SDL_ttf.h>
#ifdef main
#undef main
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int fits(const char *font_path,const char *text,int preferred,int minimum,int width,int height){
  for(int size=preferred;size>=minimum;--size){
    TTF_Font *font=TTF_OpenFont(font_path,size);assert(font);int w=0,h=0;
    int ok=TTF_SizeUTF8(font,text,&w,&h)==0&&w<=width-8&&h<=height-4;
    TTF_CloseFont(font);if(ok)return size;
  }
  return 0;
}

int main(void){
  const char *font=getenv("SUDOKURA_TEST_FONT");
  if(!font)font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  assert(TTF_Init()==0);
  const int sizes[][2]={{640,480},{1024,720},{1920,1080},{3440,1440},{360,640},{390,844},{412,915}};
  const TextKey actions[]={T_RESTART,T_MODE,T_HINT,T_NOTES,T_VERIFY,T_THEME,T_HELP,T_ABOUT,T_MENU};
  const TextKey primary[]={T_NEW_GAME,T_CONTINUE,T_DAILY,T_HELP,T_ABOUT,T_QUIT,T_CLASSIC,T_STRIKES,T_TIME_ATTACK,T_NEXT,T_RETRY,T_BACK,T_RESUME,T_PAUSED};
  for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++)for(int mode=0;mode<3;mode++){
    AppGeometry g;assert(geometry_compute(sizes[s][0],sizes[s][1],(GeometryMode)mode,&g));
    GeometryFonts tier=geometry_font_sizes(&g,sizes[s][0],sizes[s][1]);
    assert(tier.note>=10&&tier.control>=15&&tier.cell>=16&&tier.heading>=30);
    for(int language=0;language<LANG_COUNT;language++){
      int segment=g.language.w/3;
      assert(fits(font,language_name((Language)language),20,12,segment,g.language.h));
      for(int i=0;i<9;i++)assert(fits(font,tr((Language)language,actions[i]),tier.control,10,g.actions[i].w,g.actions[i].h));
      for(unsigned i=0;i<sizeof(primary)/sizeof(primary[0]);i++)assert(fits(font,tr((Language)language,primary[i]),tier.control,10,g.title_buttons[0].w,g.title_buttons[0].h));
      assert(fits(font,tr((Language)language,T_RESUME),tier.control,10,g.pause_button.w,g.pause_button.h));
      assert(fits(font,tr((Language)language,T_PAUSED),tier.heading,18,g.pause_heading.w,g.pause_heading.h));
      assert(fits(font,tr((Language)language,T_PALETTE),tier.control,10,g.palette_label.w,g.palette_label.h));
    }
  }
  TTF_Quit();puts("SDL_ttf text-fit tests passed for all languages and responsive tiers");return 0;
}
