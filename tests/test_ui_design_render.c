#define SDL_MAIN_HANDLED
#include <SDL.h>
#ifdef main
#undef main
#endif
#include "sudokura_sdl/ui_design_draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int changed_pixels(const unsigned char *pixels,int pitch,int scale,
                           SDL_Rect rect,UiDesignColor bg) {
  int count=0;
  for(int y=rect.y*scale;y<(rect.y+rect.h)*scale;++y)
    for(int x=rect.x*scale;x<(rect.x+rect.w)*scale;++x) {
      const unsigned char *p=pixels+y*pitch+x*4;
      if(p[0]!=bg.r || p[1]!=bg.g || p[2]!=bg.b)++count;
      assert(p[3]==255);
    }
  return count;
}
int main(int argc,char **argv) {
  enum { WIDTH=384,HEIGHT=320 };
  assert(SDL_Init(0)==0);
  for(int dark=0;dark<2;++dark)for(int scale=1;scale<=3;++scale) {
    UiDesignTheme t=ui_design_theme(dark!=0);
    /* Match the opaque display target; retain alpha only for source coverage. */
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,WIDTH*scale,HEIGHT*scale,
                                                       32,SDL_PIXELFORMAT_RGB888);
    assert(surface);
    SDL_Renderer *renderer=SDL_CreateSoftwareRenderer(surface);
    assert(renderer);
    assert(SDL_RenderSetScale(renderer,(float)scale,(float)scale)==0);
    SDL_SetRenderDrawColor(renderer,t.background_bottom.r,t.background_bottom.g,
                           t.background_bottom.b,255);
    SDL_RenderClear(renderer);
    for(int icon=1;icon<UI_DESIGN_ICON_COUNT;++icon) {
      int cell=icon-1;
      SDL_Rect box={cell%6*64+20,cell/6*64+20,24,24};
      ui_design_icon(renderer,box,(UiDesignIcon)icon,t.text);
    }
    for(int i=0;i<4;++i) {
      UiDesignState state={i==1,i==2,false,i==3};
      UiDesignButton b=ui_design_button_style(t,UI_DESIGN_PRIMARY,state);
      SDL_Rect rect={8+i*94,224,86,48};
      ui_design_panel(renderer,rect,12,b.top,b.bottom,b.border);
      ui_design_icon(renderer,(SDL_Rect){rect.x+31,rect.y+12,24,24},
                     UI_DESIGN_ICON_HEART,b.text);
    }
    SDL_RenderPresent(renderer);
    int pitch=WIDTH*scale*4;
    unsigned char *pixels=malloc((size_t)pitch*HEIGHT*scale);
    assert(pixels);
    assert(SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,pixels,pitch)==0);
    for(int icon=1;icon<UI_DESIGN_ICON_COUNT;++icon) {
      int cell=icon-1;
      SDL_Rect box={cell%6*64+20,cell/6*64+20,24,24};
      int changed=changed_pixels(pixels,pitch,scale,box,t.background_bottom);
      assert(changed>24*scale*scale);
      assert(changed<24*24*scale*scale);
      SDL_Rect margin={cell%6*64+4,cell/6*64+4,12,12};
      assert(changed_pixels(pixels,pitch,scale,margin,t.background_bottom)==0);
    }
    if(argc==2) {
      char file[256];
      int length=snprintf(file,sizeof(file),"%s-%s-%dx.bmp",argv[1],dark?"dark":"light",scale);
      assert(length>0 && (size_t)length<sizeof(file));
      assert(SDL_SaveBMP(surface,file)==0);
    }
    free(pixels);SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);
  }
  SDL_Quit();
  puts("Unified UI vector icons/surfaces, software renderer, light/dark, 1x/2x/3x: PASS");
  return 0;
}
