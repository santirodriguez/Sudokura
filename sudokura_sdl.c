/*  Sudokura v1.0 — Modern Sudoku in C + SDL2
    - Responsive UI (sidebar right or stacked)
    - Help & About (wrapped text + Back button, return to previous screen)
    - Notes (mode + per-subcell click)
    - Verify (rule conflicts only)
    - Classic / Strikes / Time Attack (confirm change mid-game)
    - Dark/Light theme
    - Cross-platform robust font discovery (Linux/macOS/Windows)
    Author: santirodriguez — https://santiagorodriguez.com
    License: GPLv3
    Build (Fedora/Debian/Arch):
      sudo dnf/apt/pacman install gcc SDL2-devel SDL2_ttf-devel pkgconf-pkg-config
      gcc -std=c11 -O2 -Wall -Wextra \
        sudokura_sdl.c -o sudokura \
        $(pkg-config --cflags --libs sdl2 SDL2_ttf) \
        -lm
*/

#if !defined(_WIN32)
  #ifndef _POSIX_C_SOURCE
  #define _POSIX_C_SOURCE 200809L
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#if defined(_WIN32)
  #include <windows.h>
  #include <io.h>
  #ifndef PATH_MAX
  #define PATH_MAX MAX_PATH
  #endif
#elif defined(__APPLE__)
  #include <dirent.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #include <mach-o/dyld.h>
#else
  #include <dirent.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

#include "game.h"
#include "geometry.h"
#include "version.h"
#include "i18n.h"
#include "assets/generated/window_icon.h"
#define N 9
#define NN 81
#define IDX(r,c) ((r)*9+(c))

/* =================== GUI & THEME =================== */
typedef struct { SDL_Window* win; SDL_Renderer* ren; TTF_Font* font_big; TTF_Font* font_small; int width, height; } Gfx;

typedef struct {
  SDL_Color bg, board, thin, thick, hover, sel, sel_outline, text_given, text_edit, text_wrong, boxhl, samehl, shadow, btn, btnfg, dim, title, palette_bg, palette_fg, conflict;
} Theme;

static Theme theme_dark(void){
  Theme t={
    .bg={16,18,26,255}, .board={26,30,44,255}, .thin={110,118,140,160}, .thick={150,180,255,220},
    .hover={39,46,66,160}, .sel={60,80,130,170}, .sel_outline={180,210,255,230},
    .text_given={245,248,252,255}, .text_edit={170,255,210,255}, .text_wrong={255,125,125,255},
    .boxhl={40,50,70,130}, .samehl={100,140,220,70}, .shadow={0,0,0,110},
    .btn={34,38,56,255}, .btnfg={230,230,235,255}, .dim={165,175,185,255}, .title={140,170,255,255},
    .palette_bg={32,36,54,255}, .palette_fg={230,230,235,255}, .conflict={220,60,60,80}
  };
  return t;
}
static Theme theme_light(void){
  Theme t={
    .bg={245,247,252,255}, .board={236,240,248,255}, .thin={150,160,175,200}, .thick={60,80,120,255},
    .hover={210,220,245,200}, .sel={180,200,250,160}, .sel_outline={35,55,120,230},
    .text_given={36,41,46,255}, .text_edit={0,110,85,255}, .text_wrong={200,40,40,255},
    .boxhl={200,210,235,160}, .samehl={60,100,200,50}, .shadow={0,0,0,60},
    .btn={220,226,240,255}, .btnfg={36,41,46,255}, .dim={90,100,120,255}, .title={40,70,160,255},
    .palette_bg={225,232,246,255}, .palette_fg={36,41,46,255}, .conflict={200,60,60,80}
  };
  return t;
}

typedef GameMode Mode;
typedef enum {SCR_TITLE=0, SCR_PLAY=1, SCR_END=2, SCR_HELP=3, SCR_ABOUT=4} Screen;
typedef enum {RES_NONE=0, RES_WIN=1, RES_LOSE=2} Result;

typedef struct {
  int sel_r, sel_c;
  bool notes_mode, strict_mode, paused, dark_theme;
  Language language;

  int mistakes, strikes, strikes_max;
  double start_t, pause_t0, paused_accum;
  double time_limit_s;

  char toast[96]; double toast_t0; bool toast_on;

  Screen screen; Screen prev_screen; Mode mode; Result result;
} UI;

static double now_s(void){ return SDL_GetTicks()*0.001; }
static double elapsed_time(const UI*ui){
  double now=now_s(); return ui->paused? (ui->pause_t0-ui->start_t-ui->paused_accum) : (now-ui->start_t-ui->paused_accum);
}
static void show_toast(UI*ui,const char*msg){ snprintf(ui->toast,sizeof(ui->toast),"%s",msg); ui->toast_t0=now_s(); ui->toast_on=true; }
static void set_mode_params(UI*ui){
  ui->strikes=0; ui->strikes_max=3;
  ui->time_limit_s = (ui->mode==MODE_TIME ? 10*60 : 0);
}

/* ===== Font discovery (robust, cross-platform) ===== */
static bool ends_withi(const char* s, const char* suf){
  size_t ns=strlen(s), ms=strlen(suf); if(ms>ns) return false;
  for(size_t i=0;i<ms;i++){
    char a=s[ns-ms+i], b=suf[i];
    if(a>='A'&&a<='Z') a=(char)(a-'A'+'a');
    if(b>='A'&&b<='Z') b=(char)(b-'A'+'a');
    if(a!=b) return false;
  }
  return true;
}
static bool try_open_font_path(const char* path){
  if(!path) return false;
  TTF_Font* tmp = TTF_OpenFont(path, 18);
  if(tmp){ TTF_CloseFont(tmp); return true; }
  return false;
}
static bool try_candidates(char out[PATH_MAX], const char* name){
  const char* dirs[16]; int n=0;
#if defined(_WIN32)
  dirs[n++]="C:\\\\Windows\\\\Fonts";
  const char* local= getenv("LOCALAPPDATA");
  if(local){ static char buf[PATH_MAX]; snprintf(buf,sizeof(buf),"%s\\\\Microsoft\\\\Windows\\\\Fonts",local); dirs[n++]=buf; }
#else
  dirs[n++]="/usr/share/fonts";
  dirs[n++]="/usr/local/share/fonts";
  const char* home=getenv("HOME");
  static char buf1[PATH_MAX], buf2[PATH_MAX];
  if(home){ snprintf(buf1,sizeof(buf1),"%s/.local/share/fonts",home); dirs[n++]=buf1; snprintf(buf2,sizeof(buf2),"%s/.fonts",home); dirs[n++]=buf2; }
# if defined(__APPLE__)
  dirs[n++]="/System/Library/Fonts";
  dirs[n++]="/Library/Fonts";
  if(home){ static char buf3[PATH_MAX]; snprintf(buf3,sizeof(buf3),"%s/Library/Fonts",home); dirs[n++]=buf3; }
# endif
#endif
  for(int i=0;i<n;i++){
    char p[PATH_MAX]; snprintf(p,sizeof(p), "%s/%s", dirs[i], name);
#if defined(_WIN32)
    for(char* c=p; *c; ++c) if(*c=='/') *c='\\';
#endif
    if(try_open_font_path(p)){ strncpy(out,p,PATH_MAX); out[PATH_MAX-1]=0; return true; }
  }
  return false;
}

#if defined(_WIN32)
static bool search_dir_win(const char* dir, char out[PATH_MAX]){
  char pattern[PATH_MAX]; snprintf(pattern,sizeof(pattern), "%s\\*.*", dir);
  WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pattern,&fd); if(h==INVALID_HANDLE_VALUE) return false;
  do{
    if(strcmp(fd.cFileName,".")==0 || strcmp(fd.cFileName,"..")==0) continue;
    char path[PATH_MAX]; snprintf(path,sizeof(path), "%s\\%s", dir, fd.cFileName);
    if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
      if(search_dir_win(path,out)){ FindClose(h); return true; }
    }else{
      if(ends_withi(path,".ttf") || ends_withi(path,".otf")){
        if(try_open_font_path(path)){ strncpy(out,path,PATH_MAX); out[PATH_MAX-1]=0; FindClose(h); return true; }
      }
    }
  }while(FindNextFileA(h,&fd));
  FindClose(h); return false;
}
#else
static bool search_dir_posix(const char* dir, char out[PATH_MAX]){
  DIR* d=opendir(dir); if(!d) return false;
  struct dirent* ent;
  while((ent=readdir(d))){
    if(ent->d_name[0]=='.') continue;
    char path[PATH_MAX]; snprintf(path,sizeof(path), "%s/%s", dir, ent->d_name);
    struct stat st; if(stat(path,&st)!=0) continue;
    if(S_ISDIR(st.st_mode)){
      if(search_dir_posix(path,out)){ closedir(d); return true; }
    }else{
      if(ends_withi(path,".ttf") || ends_withi(path,".otf")){
        if(try_open_font_path(path)){ strncpy(out,path,PATH_MAX); out[PATH_MAX-1]=0; closedir(d); return true; }
      }
    }
  }
  closedir(d); return false;
}
#endif

/* === NUEVO: helpers para localizar el directorio del ejecutable y probar fuentes locales === */
static bool get_exe_dir(char out[PATH_MAX]) {
#if defined(_WIN32)
  char buf[PATH_MAX]; DWORD n = GetModuleFileNameA(NULL, buf, PATH_MAX);
  if(n==0 || n>=PATH_MAX) return false;
  for(size_t i=0;i<n;i++) if(buf[i]=='/') buf[i]='\\';
  char *slash = strrchr(buf, '\\');
  if(!slash) return false;
  *slash = '\0';
  strncpy(out, buf, PATH_MAX); out[PATH_MAX-1]=0;
  return true;
#elif defined(__APPLE__)
  char buf[PATH_MAX]; uint32_t size = (uint32_t)sizeof(buf);
  if(_NSGetExecutablePath(buf, &size)!=0) return false;
  char *slash = strrchr(buf, '/');
  if(!slash) return false;
  *slash = '\0';
  strncpy(out, buf, PATH_MAX); out[PATH_MAX-1]=0;
  return true;
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
  if(n<=0) return false;
  buf[n] = 0;
  char *slash = strrchr(buf, '/');
  if(!slash) return false;
  *slash = '\0';
  strncpy(out, buf, PATH_MAX); out[PATH_MAX-1]=0;
  return true;
#endif
}

static bool try_in_dir(const char*dir, const char*name, char out[PATH_MAX]){
  if(!dir||!name) return false;
  char p[PATH_MAX];
#if defined(_WIN32)
  snprintf(p,sizeof(p), "%s\\%s", dir, name);
#else
  snprintf(p,sizeof(p), "%s/%s", dir, name);
#endif
  if(try_open_font_path(p)){ strncpy(out,p,PATH_MAX); out[PATH_MAX-1]=0; return true; }
  return false;
}

/* Reforzado: primero cwd y directorio del ejecutable; luego rutas del sistema */
static const char* find_font_path_dynamic(char out[PATH_MAX], const char* cli){
  /* 0) CLI explícito */
  if(cli && try_open_font_path(cli)){ strncpy(out,cli,PATH_MAX); out[PATH_MAX-1]=0; return out; }

  /* 1) Local (cwd + exe dir) */
  const char* local_first[] = {
    "NotoSans-Regular.ttf",
    "DejaVuSans.ttf",
    "DejaVuSans-Regular.ttf",
    "Arial.ttf"
  };
  char exedir[PATH_MAX]={0};
  bool have_exe_dir = get_exe_dir(exedir);
#if defined(__APPLE__)
  char resources[PATH_MAX]={0};
  if(have_exe_dir) snprintf(resources,sizeof(resources),"%s/../Resources",exedir);
#endif

  for(size_t i=0;i<sizeof(local_first)/sizeof(local_first[0]); ++i){
    /* cwd */
    if(try_open_font_path(local_first[i])){ strncpy(out,local_first[i],PATH_MAX); out[PATH_MAX-1]=0; return out; }
    /* exe dir */
    if(have_exe_dir && try_in_dir(exedir, local_first[i], out)) return out;
#if defined(__APPLE__)
    /* App bundle resources live beside Contents/MacOS. */
    if(have_exe_dir && try_in_dir(resources,local_first[i],out)) return out;
#endif
  }

  /* 2) Preferidas en ubicaciones conocidas del sistema */
  const char* preferred[]={
    "DejaVuSans.ttf","DejaVuSans-Regular.ttf","NotoSans-Regular.ttf","LiberationSans-Regular.ttf",
    "FreeSans.ttf","Arial.ttf","Ubuntu-R.ttf","Cantarell-VF.otf","SFNS.ttf"
  };
  for(size_t i=0;i<sizeof(preferred)/sizeof(preferred[0]);++i)
    if(try_candidates(out,preferred[i])) return out;

  /* 3) Escaneo profundo como último recurso */
#if defined(_WIN32)
  const char* roots[]={
    "C:\\\\Windows\\\\Fonts",
    getenv("LOCALAPPDATA")? getenv("LOCALAPPDATA") : ""
  };
  for(size_t i=0;i<sizeof(roots)/sizeof(roots[0]);++i){
    if(roots[i] && roots[i][0]){
      char start[PATH_MAX];
      if(i==1) snprintf(start,sizeof(start), "%s\\\\Microsoft\\\\Windows\\\\Fonts", roots[i]);
      else snprintf(start,sizeof(start), "%s", roots[i]);
      if(search_dir_win(start,out)) return out;
    }
  }
#else
  const char* roots[]={"/usr/share/fonts","/usr/local/share/fonts",
                       getenv("HOME")? getenv("HOME") : NULL,
#if defined(__APPLE__)
                       "/System/Library/Fonts","/Library/Fonts"
#endif
  };
  for(size_t i=0;i<sizeof(roots)/sizeof(roots[0]);++i){
    if(!roots[i]) continue;
    char start[PATH_MAX];
    if(roots[i]==getenv("HOME")){
      snprintf(start,sizeof(start), "%s/.local/share/fonts", roots[i]);
      if(search_dir_posix(start,out)) return out;
      snprintf(start,sizeof(start), "%s/.fonts", roots[i]);
      if(search_dir_posix(start,out)) return out;
#if defined(__APPLE__)
      snprintf(start,sizeof(start), "%s/Library/Fonts", roots[i]);
      if(search_dir_posix(start,out)) return out;
#endif
    }else{
      snprintf(start,sizeof(start), "%s", roots[i]);
      if(search_dir_posix(start,out)) return out;
    }
  }
#endif
  return NULL;
}

/* =================== DRAW HELPERS =================== */
static void draw_rect(SDL_Renderer*r,int x,int y,int w,int h, SDL_Color c){
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
  SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(r,&rc);
}
static void draw_line(SDL_Renderer*r,int x1,int y1,int x2,int y2, SDL_Color c){
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
  SDL_RenderDrawLine(r,x1,y1,x2,y2);
}
static SDL_Texture* render_text(Gfx*g, TTF_Font* f, const char* txt, SDL_Color col, int* w,int* h){
  SDL_Surface* s=TTF_RenderUTF8_Blended(f, txt, col);
  if(!s) return NULL;
  SDL_Texture* t=SDL_CreateTextureFromSurface(g->ren, s);
  if(w) *w=s->w;
  if(h) *h=s->h;
  SDL_FreeSurface(s); return t;
}
static SDL_Texture* render_text_wrapped(Gfx*g, TTF_Font* f, const char* txt, SDL_Color col, int wrap_w, int* w,int* h){
  SDL_Surface* s=TTF_RenderUTF8_Blended_Wrapped(f, txt, col, wrap_w);
  if(!s) return NULL;
  SDL_Texture* t=SDL_CreateTextureFromSurface(g->ren, s);
  if(w) *w=s->w;
  if(h) *h=s->h;
  SDL_FreeSurface(s); return t;
}
static bool point_in(SDL_Rect r,int x,int y){ return x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h; }
static GeometryMode geometry_mode(Mode mode){ return (GeometryMode)mode; }
static bool get_geometry(const Gfx*g,const UI*ui,AppGeometry*out){ return geometry_compute(g->width,g->height,geometry_mode(ui->mode),out); }
static SDL_Rect sdl_rect(GeoRect r){ return (SDL_Rect){r.x,r.y,r.w,r.h}; }
static SDL_Rect language_rect(const Gfx*g,const UI*ui){ AppGeometry a; return get_geometry(g,ui,&a)?sdl_rect(a.language):(SDL_Rect){0,0,0,0}; }
static void draw_language_selector(Gfx*g,const UI*ui){ Theme th=ui->dark_theme?theme_dark():theme_light(); SDL_Rect r=language_rect(g,ui); char label[64]; snprintf(label,sizeof label,"%s: %s (L)",tr(ui->language,T_LANGUAGE),language_name(ui->language)); draw_rect(g->ren,r.x,r.y,r.w,r.h,th.btn); int w,h; SDL_Texture*t=render_text(g,g->font_small,label,th.btnfg,&w,&h); if(t){SDL_Rect d={r.x+6,r.y+(r.h-h)/2,w,h};SDL_RenderCopy(g->ren,t,NULL,&d);SDL_DestroyTexture(t);}}

typedef struct { SDL_Rect board,side; AppGeometry model; } Layout;
typedef struct { SDL_Rect btn[9],pal[9],palette_label,progress; int count_btn,count_pal; } SidebarRects;
static Layout compute_layout(int W,int H,Mode mode){ Layout l; memset(&l,0,sizeof l); if(geometry_compute(W,H,geometry_mode(mode),&l.model)){l.board=sdl_rect(l.model.board);l.side=sdl_rect(l.model.sidebar);}return l; }
static void compute_sidebar_rects(const Layout*l,SidebarRects*r){ memset(r,0,sizeof *r);r->count_btn=r->count_pal=9;r->palette_label=sdl_rect(l->model.palette_label);r->progress=sdl_rect(l->model.progress);for(int i=0;i<9;i++){r->btn[i]=sdl_rect(l->model.actions[i]);r->pal[i]=sdl_rect(l->model.palette[i]);}}
static SDL_Rect back_rect(const Gfx*g,const UI*ui){AppGeometry a;return get_geometry(g,ui,&a)?sdl_rect(a.back_button):(SDL_Rect){0,0,0,0};}
static void draw_back_button(Gfx*g,const UI*ui,Theme th){SDL_Rect r=back_rect(g,ui);draw_rect(g->ren,r.x,r.y,r.w,r.h,th.btn);int w,h;SDL_Texture*t=render_text(g,g->font_small,tr(ui->language,T_BACK),th.btnfg,&w,&h);if(t){SDL_Rect d={r.x+10,r.y+(r.h-h)/2,w,h};SDL_RenderCopy(g->ren,t,NULL,&d);SDL_DestroyTexture(t);}}

/* =================== RENDER SCREENS =================== */
static void render_board_and_sidebar(Gfx*g,const Game*game, UI*ui){
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255);
  SDL_RenderClear(g->ren);

  Layout L = compute_layout(g->width,g->height,ui->mode);
  int gx=L.board.x, gy=L.board.y, side=L.board.w, cs=side/9;
  if(side <= 0 || cs <= 0) return;

  /* board bg + shadow */
  draw_rect(g->ren, gx-6, gy-6, side+12, side+12, th.shadow);
  draw_rect(g->ren, gx, gy, side, side, th.board);

  /* selection helpers */
  draw_rect(g->ren, gx, gy + ui->sel_r*cs, side, cs, th.boxhl);
  draw_rect(g->ren, gx + ui->sel_c*cs, gy, cs, side, th.boxhl);
  int br=(ui->sel_r/3)*3, bc=(ui->sel_c/3)*3;
  draw_rect(g->ren, gx + bc*cs, gy + br*cs, cs*3, cs*3, (SDL_Color){th.boxhl.r,th.boxhl.g,th.boxhl.b,(Uint8)(th.boxhl.a/2)});

  /* grid */
  for(int i=0;i<=9;i++){
    int x = gx + (side*i)/9;
    int y = gy + (side*i)/9;
    SDL_Color col = (i%3==0)?th.thick:th.thin;
    draw_line(g->ren, x, gy, x, gy+side, col);
    draw_line(g->ren, gx, y, gx+side, y, col);
  }

  /* cells + notes */
  int mx,my; SDL_GetMouseState(&mx,&my);
  int curV = game->puzzle[IDX(ui->sel_r,ui->sel_c)];
  for(int r=0;r<9;r++) for(int c=0;c<9;c++){
    int x=gx+c*cs, y=gy+r*cs;
    bool sel = (ui->sel_r==r && ui->sel_c==c);
    bool hover = (mx>=x && mx<x+cs && my>=y && my<y+cs);
    int v = game->puzzle[IDX(r,c)];

    if(curV && v==curV && !sel) draw_rect(g->ren, x+2,y+2, cs-4,cs-4, th.samehl);
    if(sel){
      double t = now_s(); double p = 0.65 + 0.35*(0.5*(1.0+sin(t*4.0)));
      draw_rect(g->ren, x+2,y+2, cs-4,cs-4, (SDL_Color){(Uint8)(th.sel.r*p),(Uint8)(th.sel.g*p),(Uint8)(th.sel.b*p),190});
      SDL_Color o=th.sel_outline;
      draw_line(g->ren, x+1,y+1, x+cs-2,y+1, o);
      draw_line(g->ren, x+cs-2,y+1, x+cs-2,y+cs-2, o);
      draw_line(g->ren, x+cs-2,y+cs-2, x+1,y+cs-2, o);
      draw_line(g->ren, x+1,y+cs-2, x+1,y+1, o);
    } else if(hover){
      draw_rect(g->ren, x+2,y+2, cs-4,cs-4, th.hover);
    }

    if(v && game_has_conflict(game,r,c)) draw_rect(g->ren, x+2,y+2, cs-4,cs-4, th.conflict);

    if(sel || ui->notes_mode){
      SDL_Color sgrid=(SDL_Color){th.thin.r,th.thin.g,th.thin.b, (Uint8)120};
      int sub=cs/3; for(int k=1;k<3;k++){
        draw_line(g->ren,x+k*sub,y+2,x+k*sub,y+cs-2,sgrid);
        draw_line(g->ren,x+2,y+k*sub,x+cs-2,y+k*sub,sgrid);
      }
    }

    if(v){
      SDL_Color col = game->fixed[IDX(r,c)]?th.text_given:th.text_edit;
      if(!game->fixed[IDX(r,c)] && v!=game->solution[IDX(r,c)]) col=th.text_wrong;
      char buf[2]={(char)('0'+v),0};
      int tw,thh; SDL_Texture*t=render_text(g,g->font_big,buf,col,&tw,&thh);
      if(t){ SDL_Rect d={ x + (cs-tw)/2, y + (cs-thh)/2, tw, thh }; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
    }else if(game->notes[IDX(r,c)]){
      for(int vv=1;vv<=9;vv++) if(game->notes[IDX(r,c)]&(1u<<vv)){
        char b[2]={(char)('0'+vv),0}; SDL_Color ncol=th.dim; int tw,thh;
        SDL_Texture*t=render_text(g,g->font_small,b,ncol,&tw,&thh);
        if(t){
          int sub=cs/3; int q=(vv-1)/3, qq=(vv-1)%3;
          int nx = x + qq*sub + (sub-tw)/2;
          int ny = y + q*sub  + (sub-thh)/2;
          SDL_Rect d={nx,ny,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t);
        }
      }
    }
  }

  /* sidebar text + buttons (using shared geometry) */
  SidebarRects R; compute_sidebar_rects(&L,&R);

  int sw=L.side.w;
  int tw,thh;

  /* Title and mode-specific HUD use the shared geometry. */
  SDL_Rect title_rect=sdl_rect(L.model.play_title);
  SDL_Texture* tt=render_text(g,g->font_small,SUDOKURA_NAME_VERSION,th.title,&tw,&thh);
  if(tt){SDL_Rect d={title_rect.x,title_rect.y+(title_rect.h-thh)/2,tw,thh};SDL_RenderCopy(g->ren,tt,NULL,&d);SDL_DestroyTexture(tt);}
  char buf[64];
  const char* modeName=tr(ui->language,ui->mode==MODE_CLASSIC?T_CLASSIC:ui->mode==MODE_STRIKES?T_STRIKES:T_TIME_ATTACK);
  snprintf(buf,sizeof(buf),"%s: %s",tr(ui->language,T_MODE),modeName);
  const char*huds[3]={buf,NULL,NULL}; char time_text[64],extra_text[64]; double el=elapsed_time(ui);
  snprintf(time_text,sizeof time_text,"%s: %02d:%02d",tr(ui->language,T_TIME),(int)(el/60),(int)fmod(el,60));huds[1]=time_text;
  if(ui->mode==MODE_TIME){int rem=(int)(ui->time_limit_s-el);if(rem<0)rem=0;snprintf(extra_text,sizeof extra_text,"%s: %02d:%02d",tr(ui->language,T_TARGET),rem/60,rem%60);huds[2]=extra_text;}
  else if(ui->mode==MODE_STRIKES){int left=ui->strikes_max-ui->strikes;if(left<0)left=0;snprintf(extra_text,sizeof extra_text,"%s: %d/%d",tr(ui->language,T_STRIKES_LEFT),left,ui->strikes_max);huds[2]=extra_text;}
  for(int i=0;i<L.model.hud_count;i++){SDL_Rect hr=sdl_rect(L.model.hud[i]);SDL_Texture*t=render_text(g,g->font_small,huds[i],th.dim,&tw,&thh);if(t){SDL_Rect d={hr.x,hr.y+(hr.h-thh)/2,tw,thh};SDL_RenderCopy(g->ren,t,NULL,&d);SDL_DestroyTexture(t);}}

  /* Buttons */
  const char* labels[]={tr(ui->language,T_NEW),tr(ui->language,T_MODE),tr(ui->language,T_HINT),tr(ui->language,T_NOTES),tr(ui->language,T_VERIFY),tr(ui->language,T_THEME),tr(ui->language,T_HELP),tr(ui->language,T_ABOUT),tr(ui->language,T_MENU)};
  for(int i=0;i<R.count_btn;i++){
    SDL_Rect rc=R.btn[i];
    draw_rect(g->ren,rc.x,rc.y,rc.w,rc.h,th.btn);
    SDL_Texture* t=render_text(g,g->font_small,labels[i], th.btnfg,&tw,&thh);
    if(t){ SDL_Rect d={rc.x+10,rc.y+(rc.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
  }

  /* Palette */
  SDL_Texture* tp=render_text(g,g->font_small,tr(ui->language,T_PALETTE), th.dim, &tw,&thh);
  if(tp){ SDL_Rect d={R.palette_label.x,R.palette_label.y+(R.palette_label.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,tp,NULL,&d); SDL_DestroyTexture(tp); }
  for(int n=1;n<=9;n++){
    SDL_Rect rc=R.pal[n-1];
    draw_rect(g->ren,rc.x,rc.y,rc.w,rc.h,th.palette_bg);
    char nb[2]={(char)('0'+n),0}; SDL_Texture* t=render_text(g,g->font_small,nb, th.palette_fg,&tw,&thh);
    if(t){ SDL_Rect d={rc.x+12,rc.y+(rc.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
  }

  /* bottom info */
  int filled=0; for(int i=0;i<81;i++) if(game->puzzle[i]) filled++;
  snprintf(buf,sizeof(buf),tr(ui->language,T_PROGRESS),(filled*100)/81,ui->mistakes);
  SDL_Texture* ts=render_text_wrapped(g,g->font_small, buf, th.dim, sw, &tw,&thh);
  if(ts){ SDL_Rect d={R.progress.x,R.progress.y+(R.progress.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,ts,NULL,&d); SDL_DestroyTexture(ts); }

  /* toast */
  if(ui->toast_on){
    double dt=now_s()-ui->toast_t0; if(dt>2.2) ui->toast_on=false;
    if(ui->toast_on){
      SDL_Texture* tt2=render_text(g,g->font_small,ui->toast,(SDL_Color){255,255,255,230},&tw,&thh);
      if(tt2){ SDL_Rect d={ gx + (side-tw)/2, gy- (thh+8), tw, thh }; draw_rect(g->ren,d.x-8,d.y-6,tw+16,thh+12,(SDL_Color){0,0,0,110}); SDL_RenderCopy(g->ren,tt2,NULL,&d); SDL_DestroyTexture(tt2); }
    }
  }
}

static void render_title(Gfx*g, UI*ui){
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255);
  SDL_RenderClear(g->ren);

  AppGeometry a; if(!get_geometry(g,ui,&a)) return;
  int tw,thh; SDL_Rect heading=sdl_rect(a.title_heading); SDL_Texture* T=render_text(g,g->font_big,SUDOKURA_NAME_VERSION,th.title,&tw,&thh);
  if(T){SDL_Rect d={heading.x+(heading.w-tw)/2,heading.y+(heading.h-thh)/2,tw,thh};SDL_RenderCopy(g->ren,T,NULL,&d);SDL_DestroyTexture(T);}
  SDL_Rect r_mode=sdl_rect(a.title_buttons[0]),r_start=sdl_rect(a.title_buttons[1]),r_help=sdl_rect(a.title_buttons[2]),r_about=sdl_rect(a.title_buttons[3]),r_quit=sdl_rect(a.title_buttons[4]);
  int bw=r_mode.w,bh=r_mode.h;

  draw_rect(g->ren,r_mode.x,r_mode.y,bw,bh,th.btn);
  const char* modeName=tr(ui->language,ui->mode==MODE_CLASSIC?T_CLASSIC:ui->mode==MODE_STRIKES?T_STRIKES:T_TIME_ATTACK);
  char mline[64]; snprintf(mline,sizeof(mline),"%s: %s",tr(ui->language,T_MODE),modeName);
  SDL_Texture* m1=render_text(g,g->font_small,mline, th.btnfg,&tw,&thh);
  if(m1){ SDL_Rect d={r_mode.x+12,r_mode.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,m1,NULL,&d); SDL_DestroyTexture(m1); }

  draw_rect(g->ren,r_start.x,r_start.y,bw,bh,(SDL_Color){(Uint8)(th.btn.r+10),(Uint8)(th.btn.g+10),(Uint8)(th.btn.b+10),th.btn.a});
  SDL_Texture* s=render_text(g,g->font_small,tr(ui->language,T_START), th.btnfg,&tw,&thh); if(s){ SDL_Rect d={r_start.x+12,r_start.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,s,NULL,&d); SDL_DestroyTexture(s); }

  draw_rect(g->ren,r_help.x,r_help.y,bw,bh,th.btn);
  SDL_Texture* h=render_text(g,g->font_small,tr(ui->language,T_HELP), th.btnfg,&tw,&thh); if(h){ SDL_Rect d={r_help.x+12,r_help.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,h,NULL,&d); SDL_DestroyTexture(h); }

  draw_rect(g->ren,r_about.x,r_about.y,bw,bh,th.btn);
  SDL_Texture* a=render_text(g,g->font_small,tr(ui->language,T_ABOUT), th.btnfg,&tw,&thh); if(a){ SDL_Rect d={r_about.x+12,r_about.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,a,NULL,&d); SDL_DestroyTexture(a); }

  draw_rect(g->ren,r_quit.x,r_quit.y,bw,bh,th.btn);
  SDL_Texture* q=render_text(g,g->font_small,tr(ui->language,T_QUIT), th.btnfg,&tw,&thh); if(q){ SDL_Rect d={r_quit.x+12,r_quit.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,q,NULL,&d); SDL_DestroyTexture(q); }
}

static void blit_wrapped(Gfx*g,int x,int y,int w,const char*text, SDL_Color col){
  int tw,thh; SDL_Texture* t=render_text_wrapped(g,g->font_small,text,col,w,&tw,&thh);
  if(t){ SDL_Rect d={x,y,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
}
static void render_help(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;SDL_Rect heading=sdl_rect(a.info_heading),body=sdl_rect(a.info_body);
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,tr(ui->language,T_HELP), th.title,&tw,&thh);
  if(T){ SDL_Rect d={heading.x,heading.y,tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }
  const char* text=tr(ui->language,T_HELP_BODY);blit_wrapped(g,body.x,body.y,body.w,text,th.dim);
  draw_back_button(g, ui, th);
}

static void render_about(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;SDL_Rect heading=sdl_rect(a.info_heading),body=sdl_rect(a.info_body);
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,tr(ui->language,T_ABOUT), th.title,&tw,&thh);
  if(T){ SDL_Rect d={heading.x,heading.y,tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }
  const char* text=tr(ui->language,T_ABOUT_BODY);blit_wrapped(g,body.x,body.y,body.w,text,th.dim);
  draw_back_button(g, ui, th);
}

static void render_end(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;SDL_Rect heading=sdl_rect(a.end_heading),summary=sdl_rect(a.end_summary);
  const char* title = tr(ui->language,ui->result==RES_WIN?T_YOU_WIN:T_YOU_LOSE);
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,title, (ui->result==RES_WIN? (SDL_Color){60,220,160,255} : (SDL_Color){220,80,80,255}), &tw,&thh);
  if(T){ SDL_Rect d={heading.x+(heading.w-tw)/2,heading.y+(heading.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }

  char line[64]; double el=elapsed_time(ui);
  snprintf(line,sizeof(line),"%s %02d:%02d   %s %d",tr(ui->language,T_TIME),(int)(el/60),(int)fmod(el,60),tr(ui->language,T_ERRORS),ui->mistakes);
  SDL_Texture* t=render_text(g,g->font_small,line, th.dim,&tw,&thh);
  if(t){ SDL_Rect d={summary.x+(summary.w-tw)/2,summary.y+(summary.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }

  SDL_Rect b1=sdl_rect(a.end_buttons[0]),b2=sdl_rect(a.end_buttons[1]);int bh=b1.h;
  draw_rect(g->ren,b1.x,b1.y,b1.w,b1.h, th.btn);
  SDL_Texture* s=render_text(g,g->font_small, tr(ui->language,ui->result==RES_WIN?T_NEXT:T_RETRY), th.btnfg,&tw,&thh);
  if(s){ SDL_Rect d={b1.x+12,b1.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,s,NULL,&d); SDL_DestroyTexture(s); }
  draw_rect(g->ren,b2.x,b2.y,b2.w,b2.h, th.btn);
  SDL_Texture* m=render_text(g,g->font_small,tr(ui->language,T_MENU), th.btnfg,&tw,&thh);
  if(m){ SDL_Rect d={b2.x+12,b2.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,m,NULL,&d); SDL_DestroyTexture(m); }
}

/* tiny confirm box */
static bool confirm_box(SDL_Window* win,const char*title,const char*msg,const char*ok_label,const char*cancel_label){
  const SDL_MessageBoxButtonData buttons[] = {
    {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,cancel_label},
    {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, ok_label?ok_label:"OK"}
  };
  const SDL_MessageBoxColorScheme scheme = {{
    {  25,  25,  25 }, { 200, 200, 200 }, {  50,  50,  50 }, {  60, 120, 220 }, { 230, 230, 230 }
  }};
  const SDL_MessageBoxData data = { SDL_MESSAGEBOX_INFORMATION, win, title, msg, 2, buttons, &scheme };
  int buttonid=-1; if(SDL_ShowMessageBox(&data,&buttonid)<0) return false;
  return buttonid==1;
}

/* =================== MAIN =================== */
int main(int argc,char**argv){
  const char* font_cli=NULL;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--font") && i+1<argc){ font_cli=argv[++i]; }
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){ fprintf(stderr,"SDL_Init: %s\n", SDL_GetError()); return 1; }
  if(TTF_Init()!=0){ fprintf(stderr,"TTF_Init: %s\n", TTF_GetError()); SDL_Quit(); return 1; }

  Gfx g={0}; g.width=1024; g.height=720;
  g.win = SDL_CreateWindow(SUDOKURA_NAME_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g.width, g.height, SDL_WINDOW_RESIZABLE);
  if(!g.win){fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());TTF_Quit();SDL_Quit();return 1;}
  SDL_SetWindowMinimumSize(g.win,640,480);
  g.ren = SDL_CreateRenderer(g.win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(!g.ren) g.ren=SDL_CreateRenderer(g.win,-1,SDL_RENDERER_SOFTWARE);
  if(!g.ren){fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());SDL_DestroyWindow(g.win);TTF_Quit();SDL_Quit();return 1;}
  SDL_Surface* icon=SDL_CreateRGBSurfaceWithFormatFrom((void*)sudokura_icon_rgba,(int)sudokura_icon_width,(int)sudokura_icon_height,32,(int)sudokura_icon_width*4,SDL_PIXELFORMAT_RGBA32);
  if(icon){ SDL_SetWindowIcon(g.win,icon); SDL_FreeSurface(icon); }

  /* robust font discovery */
  char font_path[PATH_MAX]={0};
  const char* fpath=find_font_path_dynamic(font_path, font_cli);
  if(!fpath){
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,SUDOKURA_NAME_VERSION,
      tr(LANG_EN,T_FONT_ERROR), g.win);
    SDL_DestroyRenderer(g.ren);SDL_DestroyWindow(g.win);TTF_Quit();SDL_Quit();
    return 1;
  }

  g.font_big   = TTF_OpenFont(fpath, 44);
  g.font_small = TTF_OpenFont(fpath, 20);
  if(!g.font_big || !g.font_small){
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,SUDOKURA_NAME_VERSION,tr(LANG_EN,T_FONT_OPEN_ERROR), g.win);
    if(g.font_big) TTF_CloseFont(g.font_big);
    if(g.font_small) TTF_CloseFont(g.font_small);
    SDL_DestroyRenderer(g.ren);SDL_DestroyWindow(g.win);TTF_Quit();SDL_Quit();
    return 1;
  }

  Game game; game_new(&game,(unsigned)time(NULL));

  UI ui; memset(&ui,0,sizeof(ui));
  ui.sel_r=4; ui.sel_c=4; ui.language=LANG_EN; ui.dark_theme=true; ui.mode=MODE_CLASSIC; ui.screen=SCR_TITLE; ui.prev_screen=SCR_TITLE;
  set_mode_params(&ui);

  bool running=true; SDL_Event e;
  while(running){
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT) running=false;
      else if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){ g.width=e.window.data1; g.height=e.window.data2; }
      else if(e.type==SDL_KEYDOWN){
        SDL_Keycode k=e.key.keysym.sym;
        if(k==SDLK_l){ ui.language=(Language)((ui.language+1)%LANG_COUNT); continue; }
        if(ui.screen==SCR_TITLE){
          if(k==SDLK_ESCAPE) running=false;
          else if(k==SDLK_RETURN){ ui.screen=SCR_PLAY; ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0; }
          else if(k==SDLK_t) ui.dark_theme=!ui.dark_theme;
          else if(k==SDLK_F1){ ui.prev_screen=SCR_TITLE; ui.screen=SCR_HELP; }
          else if(k==SDLK_F2){ ui.prev_screen=SCR_TITLE; ui.screen=SCR_ABOUT; }
          else if(k==SDLK_m){ ui.mode=(ui.mode+1)%3; set_mode_params(&ui); }
        } else if(ui.screen==SCR_HELP || ui.screen==SCR_ABOUT){
          if(k==SDLK_ESCAPE) ui.screen=ui.prev_screen;
        } else if(ui.screen==SCR_END){
          if(k==SDLK_ESCAPE) ui.screen=SCR_TITLE;
          else if(k==SDLK_RETURN){
            game_new(&game,(unsigned)time(NULL));
            ui.screen=SCR_PLAY; ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0;
          }
        } else { /* PLAY */
          bool shifted=(SDL_GetModState()&KMOD_SHIFT)!=0;
          int r=ui.sel_r, c=ui.sel_c, i=IDX(r,c);
          if(k==SDLK_ESCAPE) ui.screen=SCR_TITLE;
          else if(k==SDLK_UP||k==SDLK_w) ui.sel_r=(ui.sel_r+8)%9;
          else if(k==SDLK_DOWN||k==SDLK_s) ui.sel_r=(ui.sel_r+1)%9;
          else if(k==SDLK_LEFT||k==SDLK_a) ui.sel_c=(ui.sel_c+8)%9;
          else if(k==SDLK_RIGHT||k==SDLK_d) ui.sel_c=(ui.sel_c+1)%9;
          else if(k==SDLK_p){ if(!ui.paused){ ui.paused=true; ui.pause_t0=now_s(); } else { ui.paused=false; ui.paused_accum += now_s()-ui.pause_t0; } }
          else if(k==SDLK_t) ui.dark_theme=!ui.dark_theme;
          else if(k==SDLK_n){ ui.notes_mode=!ui.notes_mode; show_toast(&ui, tr(ui.language,ui.notes_mode?T_NOTES_ON:T_NOTES_OFF)); }
          else if(k==SDLK_m){ ui.strict_mode=!ui.strict_mode; show_toast(&ui, tr(ui.language,ui.strict_mode?T_STRICT:T_FREE)); }
          else if(k==SDLK_h){ if(game_hint(&game,r,c)) show_toast(&ui,tr(ui.language,T_HINT_USED)); }
          else if(k==SDLK_DELETE || k==SDLK_BACKSPACE || k==SDLK_0 || k==SDLK_KP_0){
            if(!game.fixed[i]){ game.puzzle[i]=0; game.notes[i]=0; }
          } else {
            int v=0; if(k>=SDLK_1 && k<=SDLK_9) v=(k-SDLK_0); else if(k>=SDLK_KP_1 && k<=SDLK_KP_9) v=(k-SDLK_KP_0);
            if(v>=1 && v<=9){
              if(ui.notes_mode || shifted){ game_toggle_note(&game,r,c,v); }
              else{
                if(game_place(&game,r,c,v, ui.strict_mode)){
                  if(v!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,tr(ui.language,T_WRONG)); }
                } else {
                  if(v!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,tr(ui.language,T_ILLEGAL)); }
                }
              }
            }
          }
        }
      }
      else if(e.type==SDL_MOUSEBUTTONDOWN){
        int x=e.button.x, y=e.button.y; bool right=(e.button.button==SDL_BUTTON_RIGHT);
        if(point_in(language_rect(&g,&ui),x,y)){ ui.language=(Language)((ui.language+1)%LANG_COUNT); continue; }

        if(ui.screen==SCR_TITLE){
          AppGeometry a;if(!get_geometry(&g,&ui,&a))continue;SDL_Rect r_mode=sdl_rect(a.title_buttons[0]),r_start=sdl_rect(a.title_buttons[1]),r_help=sdl_rect(a.title_buttons[2]),r_about=sdl_rect(a.title_buttons[3]),r_quit=sdl_rect(a.title_buttons[4]);
          if(point_in(r_mode,x,y)){ ui.mode=(ui.mode+1)%3; set_mode_params(&ui); }
          else if(point_in(r_start,x,y)){ ui.screen=SCR_PLAY; ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0; }
          else if(point_in(r_help,x,y)){ ui.prev_screen=SCR_TITLE; ui.screen=SCR_HELP; }
          else if(point_in(r_about,x,y)){ ui.prev_screen=SCR_TITLE; ui.screen=SCR_ABOUT; }
          else if(point_in(r_quit,x,y)) running=false;
        } else if(ui.screen==SCR_END){
          AppGeometry a;if(!get_geometry(&g,&ui,&a))continue;SDL_Rect b1=sdl_rect(a.end_buttons[0]),b2=sdl_rect(a.end_buttons[1]);
          if(point_in(b1,x,y)){ game_new(&game,(unsigned)time(NULL)); ui.screen=SCR_PLAY; ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0; }
          else if(point_in(b2,x,y)) ui.screen=SCR_TITLE;
        } else if(ui.screen==SCR_HELP || ui.screen==SCR_ABOUT){
          Theme th = ui.dark_theme?theme_dark():theme_light();
          (void)th;
          SDL_Rect r=back_rect(&g,&ui);
          if(point_in(r, x,y)) ui.screen=ui.prev_screen;
        } else {
          Layout L = compute_layout(g.width,g.height,ui.mode);
          int gx=L.board.x, gy=L.board.y, side=L.board.w, cs=side/9;
          if(side<=0||cs<=0) continue;
          if(x>=gx && x<gx+side && y>=gy && y<gy+side){
            int c=(x-gx)/cs, r=(y-gy)/cs; ui.sel_r=r; ui.sel_c=c;
            int i=IDX(r,c);
            if(!game.fixed[i]){
              int lx=x-(gx+c*cs), ly=y-(gy+r*cs);
              int sub=cs/3;if(sub<=0)continue;int qq=lx/sub,q=ly/sub;if(qq<0)qq=0;if(q<0)q=0;if(qq>2)qq=2;if(q>2)q=2;
              int vv=q*3+qq+1;
              if(right || ui.notes_mode){ game_toggle_note(&game,r,c,vv); }
            }
          }else{
            SidebarRects R; compute_sidebar_rects(&L,&R);

            if(point_in(R.btn[0],x,y)){ /* New */
              if(confirm_box(g.win,tr(ui.language,T_NEW_GAME),tr(ui.language,T_NEW_PROMPT),tr(ui.language,T_NEW),tr(ui.language,T_CANCEL))){
                game_new(&game,(unsigned)time(NULL)); ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0;
              }
            }
            else if(point_in(R.btn[1],x,y)){ /* Mode */
              const char* next=tr(ui.language,ui.mode==MODE_CLASSIC?T_STRIKES:ui.mode==MODE_STRIKES?T_TIME_ATTACK:T_CLASSIC);
              char msg[128]; snprintf(msg,sizeof(msg),tr(ui.language,T_CHANGE_PROMPT),next);
              if(confirm_box(g.win,tr(ui.language,T_CHANGE_MODE),msg,tr(ui.language,T_CHANGE),tr(ui.language,T_CANCEL))){
                ui.mode = (ui.mode+1)%3; set_mode_params(&ui);
                game_new(&game,(unsigned)time(NULL)); ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0;
              }
            }
            else if(point_in(R.btn[2],x,y)){ if(game_hint(&game,ui.sel_r,ui.sel_c)) show_toast(&ui,tr(ui.language,T_HINT_USED)); } /* Hint */
            else if(point_in(R.btn[3],x,y)){ ui.notes_mode=!ui.notes_mode; show_toast(&ui, tr(ui.language,ui.notes_mode?T_NOTES_ON:T_NOTES_OFF)); }
            else if(point_in(R.btn[4],x,y)){ int conf=game_conflict_count(&game); if(conf==0) show_toast(&ui,tr(ui.language,T_NO_CONFLICTS)); else { char m[32]; snprintf(m,sizeof(m),tr(ui.language,T_CONFLICTS),conf); show_toast(&ui,m);} }
            else if(point_in(R.btn[5],x,y)){ ui.dark_theme=!ui.dark_theme; }
            else if(point_in(R.btn[6],x,y)){ ui.prev_screen=SCR_PLAY; ui.screen=SCR_HELP; }
            else if(point_in(R.btn[7],x,y)){ ui.prev_screen=SCR_PLAY; ui.screen=SCR_ABOUT; }
            else if(point_in(R.btn[8],x,y)){ ui.screen=SCR_TITLE; }
            else{
              for(int n=1;n<=9;n++) if(point_in(R.pal[n-1],x,y)){
                int i=IDX(ui.sel_r,ui.sel_c);
                if(!game.fixed[i]){
                  if(ui.notes_mode){ game_toggle_note(&game,ui.sel_r,ui.sel_c,n); }
                  else{
                    if(game_place(&game,ui.sel_r,ui.sel_c,n, ui.strict_mode)){
                      if(n!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,tr(ui.language,T_WRONG)); }
                    }else{
                      if(n!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,tr(ui.language,T_ILLEGAL)); }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    /* win/lose checks */
    if(ui.screen==SCR_PLAY && !ui.paused){
      bool lose=false;
      lose=game_mode_lost(ui.mode,ui.strikes,ui.strikes_max,elapsed_time(&ui),ui.time_limit_s);
      if(game_is_solved(&game)){ ui.result=RES_WIN; ui.screen=SCR_END; }
      else if(lose){ ui.result=RES_LOSE; ui.screen=SCR_END; }
    }

    if(ui.screen==SCR_TITLE) render_title(&g,&ui);
    else if(ui.screen==SCR_HELP) render_help(&g,&ui);
    else if(ui.screen==SCR_ABOUT) render_about(&g,&ui);
    else if(ui.screen==SCR_END) render_end(&g,&ui);
    else render_board_and_sidebar(&g,&game,&ui);

    draw_language_selector(&g,&ui);
    SDL_RenderPresent(g.ren);
  }

  if(g.font_big) TTF_CloseFont(g.font_big);
  if(g.font_small) TTF_CloseFont(g.font_small);
  SDL_DestroyRenderer(g.ren); SDL_DestroyWindow(g.win);
  TTF_Quit(); SDL_Quit();
  return 0;
}
