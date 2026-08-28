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

static int wrapped_fits(const char *font_path,const char *text,int size,int width,int height){
  TTF_Font *font=TTF_OpenFont(font_path,size);assert(font);SDL_Color color={255,255,255,255};SDL_Surface *surface=TTF_RenderUTF8_Blended_Wrapped(font,text,color,(Uint32)width);assert(surface);int ok=surface->w<=width&&surface->h<=height;SDL_FreeSurface(surface);TTF_CloseFont(font);return ok;
}

static void assert_rect(GeoRect r,int width,int height){assert(geometry_rect_in_bounds(r,width,height));}

int main(void){
  const char *font=getenv("SUDOKURA_TEST_FONT");
  if(!font)font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  assert(TTF_Init()==0);
  const int sizes[][2]={{640,480},{1024,720},{1920,1080},{2560,1440},{3440,1440},{360,640},{390,844},{412,915}};
  const TextKey actions[]={T_RESTART,T_MODE,T_HINT,T_NOTES,T_VERIFY,T_SOUND,T_HELP,T_ABOUT,T_MENU};
  const TextKey modes[]={T_CLASSIC,T_STRIKES,T_TIME_ATTACK};
  const TextKey difficulties[]={T_EASY,T_MEDIUM,T_HARD};
  const TextKey about_meta[]={T_ABOUT_SEEDS,T_ABOUT_STACK,T_ABOUT_MUSIC};
  const TextKey about_links[]={T_GITHUB,T_REPOSITORY,T_WEBSITE};
  for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++)for(int mode=0;mode<3;mode++){
    int width=sizes[s][0],height=sizes[s][1];AppGeometry g;assert(geometry_compute(width,height,(GeometryMode)mode,&g));assert(geometry_play_valid(&g,width,height));
    GeometryFonts tier=geometry_font_sizes(&g,width,height);assert(tier.note>=10&&tier.control>=15&&tier.cell>=16&&tier.heading>=30);
    assert_rect(g.play_language,width,height);assert_rect(g.screen_language,width,height);assert_rect(g.home_logo,width,height);assert_rect(g.home_mode_label,width,height);assert_rect(g.home_difficulty_label,width,height);
    for(int i=0;i<3;i++){assert_rect(g.home_mode[i],width,height);assert_rect(g.home_difficulty[i],width,height);}
    for(int i=0;i<GEOMETRY_HOME_PRIMARY_COUNT;i++)assert_rect(g.home_primary[i],width,height);
    for(int i=0;i<GEOMETRY_HOME_SECONDARY_COUNT;i++)assert_rect(g.home_secondary[i],width,height);
    assert_rect(g.info_heading,width,height);assert_rect(g.info_body,width,height);assert_rect(g.about_logo,width,height);assert_rect(g.about_body,width,height);assert_rect(g.about_meta,width,height);
    for(int i=0;i<GEOMETRY_ABOUT_LINK_COUNT;i++)assert_rect(g.about_links[i],width,height);
    for(int i=0;i<GEOMETRY_PAUSE_BUTTON_COUNT;i++)assert_rect(g.pause_buttons[i],width,height);
    for(int i=0;i<GEOMETRY_END_BUTTON_COUNT;i++)assert_rect(g.end_buttons[i],width,height);
    assert_rect(g.end_summary,width,height);assert_rect(g.progress,width,height);
    for(int i=0;i<9;i++){assert_rect(g.actions[i],width,height);assert_rect(g.palette[i],width,height);assert(g.actions[i].h>=40);assert(g.palette[i].h>=28);}
    if(width<640)for(int i=1;i<9;i++)assert(g.palette[i].y==g.palette[0].y);

    for(int language=0;language<LANG_COUNT;language++){
      int play_segment=g.play_language.w/3-40,screen_segment=g.screen_language.w/3-40;
      assert(fits(font,language_name((Language)language),tier.help,10,play_segment,g.play_language.h));
      assert(fits(font,language_name((Language)language),tier.help,10,screen_segment,g.screen_language.h));
      assert(fits(font,tr((Language)language,T_MODE),tier.help,10,g.home_mode_label.w,g.home_mode_label.h));
      assert(fits(font,tr((Language)language,T_DIFFICULTY),tier.help,10,g.home_difficulty_label.w,g.home_difficulty_label.h));
      for(int i=0;i<3;i++){
        assert(fits(font,tr((Language)language,modes[i]),tier.control,10,g.home_mode[i].w,g.home_mode[i].h));
        assert(fits(font,tr((Language)language,difficulties[i]),tier.control,10,g.home_difficulty[i].w,g.home_difficulty[i].h));
      }
      assert(fits(font,tr((Language)language,T_NEW_GAME),tier.control,10,g.home_primary[0].w,g.home_primary[0].h));
      assert(fits(font,tr((Language)language,T_CONTINUE),tier.control,10,g.home_primary[1].w,g.home_primary[1].h));
      assert(fits(font,tr((Language)language,T_DAILY),tier.control,10,g.home_primary[2].w,g.home_primary[2].h*3/5));
      assert(fits(font,tr((Language)language,T_HELP),tier.control,10,g.home_secondary[0].w,g.home_secondary[0].h));
      assert(fits(font,tr((Language)language,T_ABOUT),tier.control,10,g.home_secondary[1].w,g.home_secondary[1].h));
      assert(fits(font,tr((Language)language,T_QUIT),tier.control,10,g.home_secondary[3].w,g.home_secondary[3].h));
      char theme[96];snprintf(theme,sizeof theme,"%s: %s",tr((Language)language,T_THEME),tr((Language)language,T_DARK));assert(fits(font,theme,tier.control,10,g.home_secondary[2].w,g.home_secondary[2].h));
      for(int i=0;i<9;i++)assert(fits(font,tr((Language)language,actions[i]),tier.control,10,g.actions[i].w,g.actions[i].h));
      char sample[128];
      snprintf(sample,sizeof sample,"%s: %s",tr((Language)language,T_MODE),tr((Language)language,T_TIME_ATTACK));assert(fits(font,sample,tier.hud,10,g.hud[0].w,g.hud[0].h));
      snprintf(sample,sizeof sample,"%s: %s",tr((Language)language,T_DIFFICULTY),tr((Language)language,T_MEDIUM));assert(fits(font,sample,tier.hud,10,g.hud[1].w,g.hud[1].h));
      snprintf(sample,sizeof sample,"%s: 99:59",tr((Language)language,T_TIME));assert(fits(font,sample,tier.hud,10,g.hud[2].w,g.hud[2].h));
      snprintf(sample,sizeof sample,"%s: 99",tr((Language)language,T_ERRORS));assert(fits(font,sample,tier.hud,10,g.hud[3].w,g.hud[3].h));
      assert(fits(font,tr((Language)language,T_STRICT),tier.hud,10,g.hud[4].w,g.hud[4].h));
      if(g.hud_count>5){snprintf(sample,sizeof sample,"%s: 3/3",tr((Language)language,T_STRIKES_LEFT));assert(fits(font,sample,tier.hud,10,g.hud[5].w,g.hud[5].h));}
      snprintf(sample,sizeof sample,"%s 100%%",tr((Language)language,T_PROGRESS));assert(fits(font,sample,tier.hud,10,g.progress.w,g.progress.h));
      assert(fits(font,tr((Language)language,T_RESUME),tier.control+2,10,g.pause_buttons[0].w,g.pause_buttons[0].h));
      assert(fits(font,tr((Language)language,T_MENU),tier.control,10,g.pause_buttons[1].w,g.pause_buttons[1].h));
      assert(fits(font,tr((Language)language,T_PAUSED),tier.heading,16,g.pause_heading.w,g.pause_heading.h));
      assert(fits(font,tr((Language)language,T_NEXT),tier.control+2,10,g.end_buttons[0].w,g.end_buttons[0].h));
      assert(fits(font,tr((Language)language,T_RETRY),tier.control+2,10,g.end_buttons[0].w,g.end_buttons[0].h));
      assert(fits(font,tr((Language)language,T_MENU),tier.control,10,g.end_buttons[1].w,g.end_buttons[1].h));
      assert(wrapped_fits(font,tr((Language)language,T_HELP_BODY),tier.body,g.info_body.w,g.info_body.h));
      int about_text_h=g.about_body.h-(tier.body+10)-4;assert(about_text_h>0);assert(wrapped_fits(font,tr((Language)language,T_ABOUT_BODY),tier.body,g.about_body.w,about_text_h));
      int meta_h=g.about_meta.h/3;for(int i=0;i<3;i++)assert(fits(font,tr((Language)language,about_meta[i]),tier.help,10,g.about_meta.w,meta_h));
      for(int i=0;i<GEOMETRY_ABOUT_LINK_COUNT;i++){char label[96];snprintf(label,sizeof label,"%d · %s",i+1,tr((Language)language,about_links[i]));assert(fits(font,label,tier.control,10,g.about_links[i].w,g.about_links[i].h));}
    }
  }
  TTF_Quit();puts("SDL_ttf text-fit tests passed for flags, Home, HUD, Help, About, pause, result and responsive XL tiers");return 0;
}
