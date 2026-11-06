#ifndef SUDOKURA_UI_DESIGN_DRAW_H
#define SUDOKURA_UI_DESIGN_DRAW_H

#include <SDL.h>
#include <math.h>
#include <stddef.h>
#include "ui_design.h"

/* Original vector paths in a 24-unit grid. No icon font, bitmap or network. */
typedef enum {
  UI_DESIGN_ICON_NONE, UI_DESIGN_ICON_REPOSITORY, UI_DESIGN_ICON_GLOBE,
  UI_DESIGN_ICON_HEART, UI_DESIGN_ICON_SHIELD, UI_DESIGN_ICON_BACK,
  UI_DESIGN_ICON_PLAY, UI_DESIGN_ICON_FOLDER, UI_DESIGN_ICON_TROPHY,
  UI_DESIGN_ICON_GRID, UI_DESIGN_ICON_BRAIN, UI_DESIGN_ICON_INFO,
  UI_DESIGN_ICON_HELP, UI_DESIGN_ICON_SETTINGS, UI_DESIGN_ICON_EXIT,
  UI_DESIGN_ICON_UP, UI_DESIGN_ICON_DOWN, UI_DESIGN_ICON_SOUND,
  UI_DESIGN_ICON_COUNT
} UiDesignIcon;

static inline SDL_Color ui_design_sdl_color(UiDesignColor c) {
  return (SDL_Color){c.r,c.g,c.b,c.a};
}
static inline float ui_design_pixel(SDL_Renderer *renderer) {
  float x = 1.0f, y = 1.0f;
  SDL_RenderGetScale(renderer, &x, &y);
  float scale = fmaxf(fabsf(x), fabsf(y));
  return scale > 0.01f ? 1.0f / scale : 1.0f;
}
static inline SDL_Color ui_design_gradient(UiDesignColor top, UiDesignColor bottom,
                                            float position) {
  float p = fmaxf(0.0f, fminf(1.0f, position));
  return ui_design_sdl_color(ui_design_mix(top, bottom, (unsigned)(p*255.0f)));
}

/* Convex fan plus a one-output-pixel coverage fringe; no retained textures. */
static inline void ui_design_round(SDL_Renderer *renderer, SDL_FRect box,
    float radius, UiDesignColor top, UiDesignColor bottom) {
  enum { ARC = 8, POINTS = 4*(ARC+1) };
  if (!renderer || box.w <= 0 || box.h <= 0) return;
  radius = fmaxf(0.0f, fminf(radius, fminf(box.w,box.h)*0.5f));
  const float pi = 3.14159265358979323846f;
  const float feather = ui_design_pixel(renderer)*0.75f;
  SDL_Vertex v[1+POINTS*2];
  int indices[POINTS*9], count=0;
  v[0]=(SDL_Vertex){{box.x+box.w/2,box.y+box.h/2},
                    ui_design_gradient(top,bottom,0.5f),{0,0}};
  for (int corner=0; corner<4; ++corner) {
    float cx=box.x+((corner==1 || corner==2) ? box.w-radius : radius);
    float cy=box.y+(corner>=2 ? box.h-radius : radius);
    float start=pi+(float)corner*pi/2;
    for (int j=0; j<=ARC; ++j) {
      float angle=start+(float)j*pi/(2*ARC);
      float nx=cosf(angle), ny=sinf(angle);
      int k=corner*(ARC+1)+j;
      SDL_FPoint p={cx+radius*nx,cy+radius*ny};
      SDL_Color c=ui_design_gradient(top,bottom,(p.y-box.y)/box.h);
      v[1+k]=(SDL_Vertex){p,c,{0,0}};
      c.a=0;
      v[1+POINTS+k]=(SDL_Vertex){{cx+(radius+feather)*nx,
                                 cy+(radius+feather)*ny},c,{0,0}};
    }
  }
  for (int k=0; k<POINTS; ++k) {
    int a=1+k,b=1+(k+1)%POINTS,oa=a+POINTS,ob=b+POINTS;
    indices[count++]=0; indices[count++]=a; indices[count++]=b;
    indices[count++]=a; indices[count++]=oa; indices[count++]=ob;
    indices[count++]=a; indices[count++]=ob; indices[count++]=b;
  }
  SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
  if (SDL_RenderGeometry(renderer,NULL,v,1+POINTS*2,indices,count)==0) return;
  /* Older backends retain readable surfaces rather than invisible controls. */
  int first=(int)floorf(box.y),last=(int)ceilf(box.y+box.h);
  for (int y=first; y<last; ++y) {
    float local=(float)y+0.5f-box.y, inset=0;
    float edge=fminf(local,box.h-local);
    if (edge<radius) {
      float d=radius-edge;
      inset=radius-sqrtf(fmaxf(0.0f,radius*radius-d*d));
    }
    SDL_Color c=ui_design_gradient(top,bottom,local/box.h);
    SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);
    SDL_RenderDrawLineF(renderer,box.x+inset,(float)y,
                        box.x+box.w-inset-1,(float)y);
  }
}
static inline void ui_design_panel(SDL_Renderer *renderer, SDL_Rect rect,
    float radius, UiDesignColor top, UiDesignColor bottom, UiDesignColor rim) {
  if (rect.w<3 || rect.h<3) return;
  SDL_FRect box={(float)rect.x,(float)rect.y,(float)rect.w,(float)rect.h};
  ui_design_round(renderer,box,radius,rim,rim);
  float edge=fmaxf(0.8f,ui_design_pixel(renderer));
  box.x+=edge;box.y+=edge;box.w-=2*edge;box.h-=2*edge;
  ui_design_round(renderer,box,fmaxf(0.0f,radius-edge),top,bottom);
}

/* A bounded joined stroke with subpixel edges. Input coordinates are logical. */
static inline void ui_design_stroke(SDL_Renderer *renderer,
    const SDL_FPoint *points,int n,bool closed,float width,UiDesignColor color) {
  enum { MAX_POINTS=96 };
  if (!renderer || !points || n<2 || n>MAX_POINTS || width<=0) return;
  SDL_Vertex v[MAX_POINTS*4];
  int indices[MAX_POINTS*18],count=0;
  float fringe=ui_design_pixel(renderer)*0.8f;
  SDL_Color ink=ui_design_sdl_color(color),clear=ink;clear.a=0;
  for (int i=0;i<n;++i) {
    int before=i>0 ? i-1 : (closed ? n-1 : 0);
    int after=i+1<n ? i+1 : (closed ? 0 : n-1);
    float ax=points[i].x-points[before].x,ay=points[i].y-points[before].y;
    float bx=points[after].x-points[i].x,by=points[after].y-points[i].y;
    float al=sqrtf(ax*ax+ay*ay),bl=sqrtf(bx*bx+by*by);
    if(al<0.001f){ax=bx;ay=by;al=bl;}
    if(bl<0.001f){bx=ax;by=ay;bl=al;}
    if(al<0.001f || bl<0.001f)return;
    ax/=al;ay/=al;bx/=bl;by/=bl;
    float nx=-ay-by,ny=ax+bx;
    float length=sqrtf(nx*nx+ny*ny);
    if(length<0.001f){nx=-by;ny=bx;}else{nx/=length;ny/=length;}
    float projection=nx*(-by)+ny*bx;
    float join=1.0f/fmaxf(0.4f,projection);
    float inner=width*0.5f*join,outer=(width*0.5f+fringe)*join;
    v[4*i]=(SDL_Vertex){{points[i].x-nx*outer,points[i].y-ny*outer},clear,{0,0}};
    v[4*i+1]=(SDL_Vertex){{points[i].x-nx*inner,points[i].y-ny*inner},ink,{0,0}};
    v[4*i+2]=(SDL_Vertex){{points[i].x+nx*inner,points[i].y+ny*inner},ink,{0,0}};
    v[4*i+3]=(SDL_Vertex){{points[i].x+nx*outer,points[i].y+ny*outer},clear,{0,0}};
  }
  int segments=closed ? n : n-1;
  for (int i=0;i<segments;++i) {
    int a=i*4,b=((i+1)%n)*4;
    for (int band=0;band<3;++band) {
      indices[count++]=a+band;indices[count++]=b+band;indices[count++]=b+band+1;
      indices[count++]=a+band;indices[count++]=b+band+1;indices[count++]=a+band+1;
    }
  }
  SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
  if (SDL_RenderGeometry(renderer,NULL,v,n*4,indices,count)!=0) {
    SDL_SetRenderDrawColor(renderer,ink.r,ink.g,ink.b,ink.a);
    SDL_RenderDrawLinesF(renderer,points,n);
    if (closed) SDL_RenderDrawLineF(renderer,points[n-1].x,points[n-1].y,
                                    points[0].x,points[0].y);
  }
  if (!closed) {
    float r=width*0.5f;
    for (int i=0;i<2;++i) {
      SDL_FPoint p=points[i ? n-1 : 0];
      ui_design_round(renderer,(SDL_FRect){p.x-r,p.y-r,width,width},r,color,color);
    }
  }
}
static inline void ui_design_icon_path(SDL_Renderer *renderer,SDL_Rect box,
    const SDL_FPoint *path,int count,bool closed,UiDesignColor color) {
  if (count<2 || count>96 || box.w<=0 || box.h<=0) return;
  float side=(float)(box.w<box.h ? box.w : box.h),scale=side/24.0f;
  float x=box.x+(box.w-side)/2.0f,y=box.y+(box.h-side)/2.0f;
  SDL_FPoint points[96];
  for (int i=0;i<count;++i) points[i]=(SDL_FPoint){x+path[i].x*scale,y+path[i].y*scale};
  ui_design_stroke(renderer,points,count,closed,1.7f*scale,color);
}
static inline void ui_design_icon_ellipse(SDL_Renderer *renderer,SDL_Rect box,
    float cx,float cy,float rx,float ry,UiDesignColor color) {
  SDL_FPoint points[40];
  for (int i=0;i<40;++i) {
    float a=(float)i*6.28318530718f/40.0f;
    points[i]=(SDL_FPoint){cx+cosf(a)*rx,cy+sinf(a)*ry};
  }
  ui_design_icon_path(renderer,box,points,40,true,color);
}
static inline void ui_design_icon_line(SDL_Renderer *renderer,SDL_Rect box,
    float x1,float y1,float x2,float y2,UiDesignColor color) {
  SDL_FPoint p[2]={{x1,y1},{x2,y2}};
  ui_design_icon_path(renderer,box,p,2,false,color);
}
static inline void ui_design_cubic(SDL_FPoint *out,int *count,
    SDL_FPoint a,SDL_FPoint b,SDL_FPoint c,SDL_FPoint d) {
  for (int i=0;i<8;++i) {
    float t=(float)i/8.0f,u=1.0f-t;
    out[(*count)++]=(SDL_FPoint){u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,
                               u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y};
  }
}
static inline void ui_design_icon(SDL_Renderer *r,SDL_Rect b,
                                   UiDesignIcon icon,UiDesignColor c) {
  if (!r || b.w<8 || b.h<8 || icon<=UI_DESIGN_ICON_NONE || icon>=UI_DESIGN_ICON_COUNT) return;
#define UIL(x1,y1,x2,y2) ui_design_icon_line(r,b,x1,y1,x2,y2,c)
#define UIP(points,closed) ui_design_icon_path(r,b,points,(int)(sizeof(points)/sizeof((points)[0])),closed,c)
  switch(icon) {
    case UI_DESIGN_ICON_REPOSITORY: {
      UIL(7,7,7,17);UIL(17,7,17,9);
      const SDL_FPoint join[]={{17,9},{16,11},{13,12},{7,12}};UIP(join,false);
      ui_design_icon_ellipse(r,b,7,4.5f,2.4f,2.4f,c);
      ui_design_icon_ellipse(r,b,7,19.5f,2.4f,2.4f,c);
      ui_design_icon_ellipse(r,b,17,4.5f,2.4f,2.4f,c);break;
    }
    case UI_DESIGN_ICON_GLOBE:
      ui_design_icon_ellipse(r,b,12,12,9,9,c);
      ui_design_icon_ellipse(r,b,12,12,4,9,c);
      UIL(3,12,21,12);break;
    case UI_DESIGN_ICON_HEART: {
      SDL_FPoint p[32];int n=0;
      ui_design_cubic(p,&n,(SDL_FPoint){12,20},(SDL_FPoint){7,16},(SDL_FPoint){2.5f,12},(SDL_FPoint){3,7});
      ui_design_cubic(p,&n,(SDL_FPoint){3,7},(SDL_FPoint){4,2},(SDL_FPoint){10,2},(SDL_FPoint){12,7});
      ui_design_cubic(p,&n,(SDL_FPoint){12,7},(SDL_FPoint){14,2},(SDL_FPoint){20,2},(SDL_FPoint){21,7});
      ui_design_cubic(p,&n,(SDL_FPoint){21,7},(SDL_FPoint){21.5f,12},(SDL_FPoint){17,16},(SDL_FPoint){12,20});
      ui_design_icon_path(r,b,p,n,true,c);break;
    }
    case UI_DESIGN_ICON_SHIELD: {
      const SDL_FPoint p[]={{12,2.5f},{16,4.5f},{21,6},{20,13},{17,18},{12,22},{7,18},{4,13},{3,6},{8,4.5f}};
      const SDL_FPoint check[]={{8,12},{11,15},{16,9}};UIP(p,true);UIP(check,false);break;
    }
    case UI_DESIGN_ICON_BACK: {
      const SDL_FPoint p[]={{10,5},{3,12},{10,19}};UIP(p,false);UIL(3,12,21,12);break;
    }
    case UI_DESIGN_ICON_PLAY: {
      const SDL_FPoint p[]={{8,4},{20,12},{8,20}};UIP(p,true);break;
    }
    case UI_DESIGN_ICON_FOLDER: {
      const SDL_FPoint p[]={{3,8},{3,5},{9,5},{12,8},{21,8},{21,19},{3,19}};
      UIP(p,true);UIL(3,9,21,9);break;
    }
    case UI_DESIGN_ICON_TROPHY: {
      const SDL_FPoint p[]={{7,3},{17,3},{17,9},{16,12},{12,14},{8,12},{7,9}};
      const SDL_FPoint left[]={{7,5},{3,5},{3,8},{4,10},{7,11}};
      const SDL_FPoint right[]={{17,5},{21,5},{21,8},{20,10},{17,11}};
      UIP(p,true);UIP(left,false);UIP(right,false);UIL(12,14,12,20);UIL(7,21,17,21);break;
    }
    case UI_DESIGN_ICON_GRID: {
      const SDL_FPoint p[]={{4,3},{20,3},{21,4},{21,20},{20,21},{4,21},{3,20},{3,4}};
      UIP(p,true);UIL(9,3,9,21);UIL(15,3,15,21);UIL(3,9,21,9);UIL(3,15,21,15);break;
    }
    case UI_DESIGN_ICON_BRAIN: {
      for(int side=0;side<2;++side) {
        SDL_FPoint p[40];int n=0;
        ui_design_cubic(p,&n,(SDL_FPoint){11,4},(SDL_FPoint){9,1},(SDL_FPoint){5,3},(SDL_FPoint){5,6});
        ui_design_cubic(p,&n,(SDL_FPoint){5,6},(SDL_FPoint){1,7},(SDL_FPoint){1,11},(SDL_FPoint){4,13});
        ui_design_cubic(p,&n,(SDL_FPoint){4,13},(SDL_FPoint){1,17},(SDL_FPoint){4,20},(SDL_FPoint){7,19});
        ui_design_cubic(p,&n,(SDL_FPoint){7,19},(SDL_FPoint){8,23},(SDL_FPoint){11,22},(SDL_FPoint){11,19});
        p[n++]=(SDL_FPoint){11,4};
        if(side)for(int i=0;i<n;++i)p[i].x=24-p[i].x;
        ui_design_icon_path(r,b,p,n,true,c);
      }
      UIL(5,7,8,9);UIL(4,13,7,12);UIL(7,19,8,16);
      UIL(19,7,16,9);UIL(20,13,17,12);UIL(17,19,16,16);break;
    }
    case UI_DESIGN_ICON_INFO:
      ui_design_icon_ellipse(r,b,12,12,9,9,c);UIL(12,11,12,17);UIL(10,17,14,17);
      ui_design_icon_ellipse(r,b,12,7,0.55f,0.55f,c);break;
    case UI_DESIGN_ICON_HELP: {
      ui_design_icon_ellipse(r,b,12,12,9,9,c);
      const SDL_FPoint p[]={{9,8},{10,6.5f},{13,6.5f},{15,8},{15,10},{12,12},{12,13.5f}};
      UIP(p,false);ui_design_icon_ellipse(r,b,12,17,0.5f,0.5f,c);break;
    }
    case UI_DESIGN_ICON_SETTINGS: {
      SDL_FPoint p[32];
      for(int i=0;i<32;++i) {
        float a=(float)i*6.28318530718f/32,rad=(i%4==1 || i%4==2)?9.5f:7.5f;
        p[i]=(SDL_FPoint){12+cosf(a)*rad,12+sinf(a)*rad};
      }
      UIP(p,true);ui_design_icon_ellipse(r,b,12,12,3.4f,3.4f,c);break;
    }
    case UI_DESIGN_ICON_EXIT: {
      const SDL_FPoint p[]={{10,3},{4,3},{4,21},{10,21}};
      const SDL_FPoint arrow[]={{16,7},{21,12},{16,17}};
      UIP(p,false);UIP(arrow,false);UIL(10,12,21,12);break;
    }
    case UI_DESIGN_ICON_UP: {
      const SDL_FPoint p[]={{5,15},{12,8},{19,15}};UIP(p,false);break;
    }
    case UI_DESIGN_ICON_DOWN: {
      const SDL_FPoint p[]={{5,9},{12,16},{19,9}};UIP(p,false);break;
    }
    case UI_DESIGN_ICON_SOUND: {
      const SDL_FPoint p[]={{3,9},{7,9},{12,5},{12,19},{7,15},{3,15}};
      const SDL_FPoint wave[]={{16,8},{18,10},{18,14},{16,16}};
      const SDL_FPoint outer[]={{19,5},{22,9},{22,15},{19,19}};
      UIP(p,true);UIP(wave,false);UIP(outer,false);break;
    }
    default: break;
  }
#undef UIP
#undef UIL
}
#endif
