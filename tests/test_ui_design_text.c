#define SDL_MAIN_HANDLED
#include <SDL_ttf.h>
#ifdef main
#undef main
#endif
#include "geometry.h"
#include "i18n.h"
#include "ui_design.h"
#include "sudokura_sdl/ui_action_icons.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int wrapped_fit(const char *path,const char *text,int width,int height,
                        int preferred,int minimum) {
  static const int sizes[]={10,12,13,14,15,16,18,20,22,24,28,30,32,36,38,42,44,48,52,56,60,64,72};
  if(width<=8 || height<=2)return 0;
  for(int i=(int)(sizeof(sizes)/sizeof(sizes[0]))-1;i>=0;--i) {
    int size=sizes[i];
    if(size>preferred || size<minimum)continue;
    TTF_Font *font=TTF_OpenFont(path,size);assert(font);
    SDL_Surface *surface=TTF_RenderUTF8_Blended_Wrapped(font,text,
        (SDL_Color){255,255,255,255},(Uint32)(width-8));
    assert(surface);
    int fits=surface->w<=width-4 && surface->h<=height-2;
    SDL_FreeSurface(surface);TTF_CloseFont(font);
    if(fits)return size;
  }
  return 0;
}
static void check_button(const char *font,GeoRect rect,const char *text,
                         int preferred,int minimum) {
  for(int pressed=0;pressed<2;++pressed) {
    UiDesignLabel boxes=ui_design_label_layout(rect.w,rect.h-pressed,true);
    int ok=wrapped_fit(font,text,boxes.text.w,rect.h-pressed-6,preferred,minimum);
    if(!ok)fprintf(stderr,"Unified UI text does not fit: %s in %dx%d (pressed=%d)\n",
                   text,rect.w,rect.h,pressed);
    assert(ok);
  }
}
static void check_action(const char *font,GeoRect rect,const char *text,
                         int preferred,int minimum) {
  assert(rect.w>8 && rect.h>6);
  for(int pressed=0;pressed<2;++pressed) {
    UiDesignLabel boxes=ui_action_label_layout(rect.w,rect.h-pressed,true);
    int ok=wrapped_fit(font,text,boxes.text.w,boxes.text.h,preferred,minimum);
    if(!ok)fprintf(stderr,"Action label does not fit: %s in %dx%d (pressed=%d, min=%d)\n",
                   text,rect.w,rect.h,pressed,minimum);
    assert(ok);
  }
}
static void check_play(const char *font,int width,int height,bool mobile,Language language) {
  const PlayControl controls[]={PLAY_CONTROL_MENU,PLAY_CONTROL_PAUSE,PLAY_CONTROL_SAVE,
      PLAY_CONTROL_MORE,PLAY_CONTROL_UNDO,PLAY_CONTROL_REDO,PLAY_CONTROL_CLEAR,
      PLAY_CONTROL_NOTES,PLAY_CONTROL_RESTART,PLAY_CONTROL_HINT,PLAY_CONTROL_VERIFY,
      PLAY_CONTROL_SETTINGS,PLAY_CONTROL_HELP};
  const TextKey keys[]={T_MENU,T_PAUSE,T_SAVE,T_MORE,T_UNDO,T_REDO,T_CLEAR,T_NOTES,
      T_RESTART,T_HINT,T_VERIFY,T_SETTINGS,T_HELP};
  for(int mode=GEOMETRY_MODE_CLASSIC;mode<=GEOMETRY_MODE_TIME;++mode) {
    AppGeometry g;
    bool ok=mobile ? geometry_compute_mobile(width,height,(GeometryMode)mode,&g)
                   : geometry_compute(width,height,(GeometryMode)mode,&g);
    if(!ok)return;
    GeometryFonts f=geometry_font_sizes(&g,width,height);
    for(unsigned i=0;i<sizeof(controls)/sizeof(controls[0]);++i) {
      if((g.play_compact&&i>=8)||(!g.play_compact&&controls[i]==PLAY_CONTROL_MORE))continue;
      if(mobile&&height<600&&(controls[i]==PLAY_CONTROL_UNDO||controls[i]==PLAY_CONTROL_REDO))continue;
      const char *label=tr(language,mobile&&i==0 ? T_HOME_COMPACT : keys[i]);
      check_action(font,g.play_controls[controls[i]],label,f.control,mobile ? 13 : 10);
    }
  }
}
int main(void) {
  const char *font=getenv("SUDOKURA_TEST_FONT");
  if(!font)font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  assert(TTF_Init()==0);
  const int sizes[][2]={{360,600},{361,601},{390,844},{412,915},{600,320},
      {600,360},{800,480},{360,640},{640,480},{800,600},{960,720},
      {1280,960},{1920,1080},{2560,1440}};
  const TextKey links[]={T_REPOSITORY,T_WEBSITE,T_SUPPORT,T_PRIVACY_POLICY};
  for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);++s) {
    int width=sizes[s][0],height=sizes[s][1];
    for(int language=0;language<LANG_COUNT;++language) {
      GeometryInfo mobile;
      if(geometry_mobile_info(width,height,true,&mobile)) {
        for(int i=0;i<4;++i)check_button(font,mobile.links[i],tr((Language)language,links[i]),18,13);
      }
      AppGeometry desktop;
      if(geometry_compute(width,height,GEOMETRY_MODE_CLASSIC,&desktop)) {
        for(int i=0;i<4;++i)check_button(font,desktop.about_links[i],tr((Language)language,links[i]),18,10);
        GeoRect credits=desktop.about_credits;
        assert(wrapped_fit(font,tr_publisher_credit((Language)language),credits.w-16,
                            credits.h/2,16,10));
        assert(wrapped_fit(font,tr((Language)language,T_ABOUT_STACK),credits.w-16,
                            credits.h-credits.h/2,14,10));
      }
      check_play(font,width,height,false,(Language)language);
      check_play(font,width,height,true,(Language)language);
    }
  }
  for(int width=1;width<400;++width)for(int height=1;height<80;++height) {
    UiDesignLabel b=ui_action_label_layout(width,height,true);
    assert(b.text.x>=0 && b.text.y>=0 && b.text.w>=0 && b.text.h>=0);
    assert(b.text.x+b.text.w<=width && b.text.y+b.text.h<=height);
    if(b.icon.w)assert(b.icon.x+b.icon.w<b.text.x && b.icon.y+b.icon.h<=height);
  }
  /* Preserve the previously accepted one-line Catalan Home regression. */
  AppGeometry home;
  assert(geometry_compute_mobile(412,892,GEOMETRY_MODE_CLASSIC,&home));
  GeoRect control=home.home_mode[2];
  UiDesignLabel label=ui_design_label_layout(control.w,control.h,false);
  int one_line=0;
  const char *mode=tr(LANG_CA,T_TIME_ATTACK);
  for(int size=16;size>=13;--size) {
    TTF_Font *face=TTF_OpenFont(font,size);
    assert(face);
    int w=0,h=0;
    if(TTF_SizeUTF8(face,mode,&w,&h)==0 &&
       w<=label.text.w-8 && h<=control.h-10)one_line=size;
    TTF_CloseFont(face);
    if(one_line)break;
  }
  assert(one_line>=13);
  TTF_Quit();
  puts("Unified UI real SDL_ttf fit: Home/About, play actions, pressed state and EN/ES/CA: PASS");
  return 0;
}
