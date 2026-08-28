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
#include <string.h>

static int fits(const char *font_path,const char *text,int preferred,int minimum,int width,int height){
  for(int size=preferred;size>=minimum;--size){
    TTF_Font *font=TTF_OpenFont(font_path,size);assert(font);int w=0,h=0;
    int ok=TTF_SizeUTF8(font,text,&w,&h)==0&&w<=width-8&&h<=height-4;
    TTF_CloseFont(font);if(ok)return size;
  }
  return 0;
}

static void assert_rect(GeoRect r,int width,int height){assert(geometry_rect_in_bounds(r,width,height));}

int main(void){
  const char *font=getenv("SUDOKURA_TEST_FONT");
  if(!font)font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  assert(TTF_Init()==0);
  const int sizes[][2]={{640,480},{1024,720},{1920,1080},{3440,1440},{360,640},{390,844},{412,915}};
  const TextKey actions[]={T_RESTART,T_MODE,T_HINT,T_NOTES,T_VERIFY,T_SOUND,T_HELP,T_ABOUT,T_MENU};
  const TextKey modes[]={T_CLASSIC,T_STRIKES,T_TIME_ATTACK};
  const TextKey difficulties[]={T_EASY,T_MEDIUM,T_HARD};
  for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++)for(int mode=0;mode<3;mode++){
    int width=sizes[s][0],height=sizes[s][1];AppGeometry g;assert(geometry_compute(width,height,(GeometryMode)mode,&g));assert(geometry_play_valid(&g,width,height));
    GeometryFonts tier=geometry_font_sizes(&g,width,height);assert(tier.note>=10&&tier.control>=15&&tier.cell>=16&&tier.heading>=30);
    assert_rect(g.play_language,width,height);assert_rect(g.screen_language,width,height);assert_rect(g.home_logo,width,height);assert_rect(g.home_mode_label,width,height);assert_rect(g.home_difficulty_label,width,height);
    for(int i=0;i<3;i++){assert_rect(g.home_mode[i],width,height);assert_rect(g.home_difficulty[i],width,height);}
    for(int i=0;i<GEOMETRY_HOME_PRIMARY_COUNT;i++)assert_rect(g.home_primary[i],width,height);
    for(int i=0;i<GEOMETRY_HOME_SECONDARY_COUNT;i++)assert_rect(g.home_secondary[i],width,height);
    for(int i=0;i<GEOMETRY_PAUSE_BUTTON_COUNT;i++)assert_rect(g.pause_buttons[i],width,height);
    for(int i=0;i<GEOMETRY_END_BUTTON_COUNT;i++)assert_rect(g.end_buttons[i],width,height);
    assert_rect(g.end_summary,width,height);assert_rect(g.progress,width,height);
    for(int i=0;i<9;i++){assert_rect(g.actions[i],width,height);assert_rect(g.palette[i],width,height);assert(g.actions[i].h>=40);assert(g.palette[i].h>=28);}
    if(width<640)for(int i=1;i<9;i++)assert(g.palette[i].y==g.palette[0].y);

    for(int language=0;language<LANG_COUNT;language++){
      int play_segment=g.play_language.w/3-40,screen_segment=g.screen_language.w/3-40;
      assert(fits(font,language_name((Language)language),16,10,play_segment,g.play_language.h));
      assert(fits(font,language_name((Language)language),16,10,screen_segment,g.screen_language.h));
      assert(fits(font,tr((Language)language,T_MODE),15,10,g.home_mode_label.w,g.home_mode_label.h));
      assert(fits(font,tr((Language)language,T_DIFFICULTY),15,10,g.home_difficulty_label.w,g.home_difficulty_label.h));
      for(int i=0;i<3;i++){
        assert(fits(font,tr((Language)language,modes[i]),18,10,g.home_mode[i].w,g.home_mode[i].h));
        assert(fits(font,tr((Language)language,difficulties[i]),18,10,g.home_difficulty[i].w,g.home_difficulty[i].h));
      }
      assert(fits(font,tr((Language)language,T_NEW_GAME),20,10,g.home_primary[0].w,g.home_primary[0].h));
      assert(fits(font,tr((Language)language,T_CONTINUE),20,10,g.home_primary[1].w,g.home_primary[1].h));
      assert(fits(font,tr((Language)language,T_DAILY),18,10,g.home_primary[2].w,g.home_primary[2].h*3/5));
      assert(fits(font,tr((Language)language,T_HELP),20,10,g.home_secondary[0].w,g.home_secondary[0].h));
      assert(fits(font,tr((Language)language,T_ABOUT),20,10,g.home_secondary[1].w,g.home_secondary[1].h));
      assert(fits(font,tr((Language)language,T_QUIT),20,10,g.home_secondary[3].w,g.home_secondary[3].h));
      char theme[96];snprintf(theme,sizeof theme,"%s: %s",tr((Language)language,T_THEME),tr((Language)language,T_DARK));assert(fits(font,theme,20,10,g.home_secondary[2].w,g.home_secondary[2].h));
      for(int i=0;i<9;i++)assert(fits(font,tr((Language)language,actions[i]),tier.control,10,g.actions[i].w,g.actions[i].h));
      char sample[128];
      snprintf(sample,sizeof sample,"%s: %s",tr((Language)language,T_MODE),tr((Language)language,T_TIME_ATTACK));assert(fits(font,sample,16,10,g.hud[0].w,g.hud[0].h));
      snprintf(sample,sizeof sample,"%s: %s",tr((Language)language,T_DIFFICULTY),tr((Language)language,T_MEDIUM));assert(fits(font,sample,16,10,g.hud[1].w,g.hud[1].h));
      snprintf(sample,sizeof sample,"%s: 99:59",tr((Language)language,T_TIME));assert(fits(font,sample,16,10,g.hud[2].w,g.hud[2].h));
      snprintf(sample,sizeof sample,"%s: 99",tr((Language)language,T_ERRORS));assert(fits(font,sample,16,10,g.hud[3].w,g.hud[3].h));
      assert(fits(font,tr((Language)language,T_STRICT),16,10,g.hud[4].w,g.hud[4].h));
      if(g.hud_count>5){snprintf(sample,sizeof sample,"%s: 3/3",tr((Language)language,T_STRIKES_LEFT));assert(fits(font,sample,16,10,g.hud[5].w,g.hud[5].h));}
      snprintf(sample,sizeof sample,"%s 100%%",tr((Language)language,T_PROGRESS));assert(fits(font,sample,16,10,g.progress.w,g.progress.h));
      assert(fits(font,tr((Language)language,T_RESUME),22,10,g.pause_buttons[0].w,g.pause_buttons[0].h));
      assert(fits(font,tr((Language)language,T_MENU),20,10,g.pause_buttons[1].w,g.pause_buttons[1].h));
      assert(fits(font,tr((Language)language,T_PAUSED),tier.heading,16,g.pause_heading.w,g.pause_heading.h));
      assert(fits(font,tr((Language)language,T_NEXT),22,10,g.end_buttons[0].w,g.end_buttons[0].h));
      assert(fits(font,tr((Language)language,T_RETRY),22,10,g.end_buttons[0].w,g.end_buttons[0].h));
      assert(fits(font,tr((Language)language,T_MENU),20,10,g.end_buttons[1].w,g.end_buttons[1].h));
    }
  }
  TTF_Quit();puts("SDL_ttf text-fit tests passed for flags, Home, HUD, pause, result and responsive tiers");return 0;
}
