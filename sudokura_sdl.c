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
  #include <direct.h>
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
#include <errno.h>

#include "game.h"
#include "geometry.h"
#include "version.h"
#include "i18n.h"
#include "assets/generated/window_icon.h"
#include "assets/generated/wordmark.h"
#define N 9
#define NN 81
#define IDX(r,c) ((r)*9+(c))

/* =================== GUI & THEME =================== */
enum { FONT_CACHE_COUNT=17 };
static const int font_cache_sizes[FONT_CACHE_COUNT]={10,12,13,14,15,16,18,20,24,28,30,32,36,38,42,44,48};
typedef struct { SDL_Window* win; SDL_Renderer* ren; TTF_Font* fonts[FONT_CACHE_COUNT]; TTF_Font* font_big; TTF_Font* font_small; TTF_Font* font_note; TTF_Font* font_cell; TTF_Font* font_body; SDL_Texture* wordmark; int width, height; } Gfx;

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
static bool path_copy(char*destination,size_t destination_size,const char*source){
  if(!destination||destination_size==0||!source)return false;
  size_t source_size=strlen(source)+1;
  if(source_size>destination_size){destination[0]='\0';return false;}
  memcpy(destination,source,source_size);
  return true;
}

static bool path_join(char*destination,size_t destination_size,const char*directory,const char*component,char separator){
  if(!destination||destination_size==0||!directory||!component)return false;
  size_t directory_length=strlen(directory),component_length=strlen(component);
  bool needs_separator=directory_length>0&&directory[directory_length-1]!=separator;
  if(directory_length>=destination_size){destination[0]='\0';return false;}
  size_t available=destination_size-directory_length-1;
  if((needs_separator&&available==0)||component_length>available-(needs_separator?1u:0u)){
    destination[0]='\0';return false;
  }
  memcpy(destination,directory,directory_length);
  size_t offset=directory_length;
  if(needs_separator)destination[offset++]=separator;
  memcpy(destination+offset,component,component_length+1);
  return true;
}

static bool path_append(char*destination,size_t destination_size,const char*component,char separator){
  char original[PATH_MAX];
  if(!path_copy(original,sizeof(original),destination))return false;
  return path_join(destination,destination_size,original,component,separator);
}

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
  if(local){ static char buf[PATH_MAX]; if(path_copy(buf,sizeof(buf),local)&&path_append(buf,sizeof(buf),"Microsoft\\Windows\\Fonts",'\\'))dirs[n++]=buf; }
#else
  dirs[n++]="/usr/share/fonts";
  dirs[n++]="/usr/local/share/fonts";
  const char* home=getenv("HOME");
  static char buf1[PATH_MAX], buf2[PATH_MAX];
  if(home){ if(path_copy(buf1,sizeof(buf1),home)&&path_append(buf1,sizeof(buf1),".local/share/fonts",'/'))dirs[n++]=buf1; if(path_join(buf2,sizeof(buf2),home,".fonts",'/'))dirs[n++]=buf2; }
# if defined(__APPLE__)
  dirs[n++]="/System/Library/Fonts";
  dirs[n++]="/Library/Fonts";
  if(home){ static char buf3[PATH_MAX]; if(path_join(buf3,sizeof(buf3),home,"Library/Fonts",'/'))dirs[n++]=buf3; }
# endif
#endif
  for(int i=0;i<n;i++){
    char p[PATH_MAX];
#if defined(_WIN32)
    if(!path_join(p,sizeof(p),dirs[i],name,'\\'))continue;
#endif
#if !defined(_WIN32)
    if(!path_join(p,sizeof(p),dirs[i],name,'/'))continue;
#endif
    if(try_open_font_path(p)&&path_copy(out,PATH_MAX,p))return true;
  }
  return false;
}

#if defined(_WIN32)
static bool search_dir_win(const char* dir, char out[PATH_MAX]){
  char pattern[PATH_MAX]; if(!path_join(pattern,sizeof(pattern),dir,"*.*",'\\'))return false;
  WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pattern,&fd); if(h==INVALID_HANDLE_VALUE) return false;
  do{
    if(strcmp(fd.cFileName,".")==0 || strcmp(fd.cFileName,"..")==0) continue;
    char path[PATH_MAX]; if(!path_join(path,sizeof(path),dir,fd.cFileName,'\\'))continue;
    if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
      if(search_dir_win(path,out)){ FindClose(h); return true; }
    }else{
      if(ends_withi(path,".ttf") || ends_withi(path,".otf")){
        if(try_open_font_path(path)&&path_copy(out,PATH_MAX,path)){ FindClose(h); return true; }
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
    char path[PATH_MAX]; if(!path_join(path,sizeof(path),dir,ent->d_name,'/'))continue;
    struct stat st; if(stat(path,&st)!=0) continue;
    if(S_ISDIR(st.st_mode)){
      if(search_dir_posix(path,out)){ closedir(d); return true; }
    }else{
      if(ends_withi(path,".ttf") || ends_withi(path,".otf")){
        if(try_open_font_path(path)&&path_copy(out,PATH_MAX,path)){ closedir(d); return true; }
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
  return path_copy(out,PATH_MAX,buf);
#elif defined(__APPLE__)
  char buf[PATH_MAX]; uint32_t size = (uint32_t)sizeof(buf);
  if(_NSGetExecutablePath(buf, &size)!=0) return false;
  char *slash = strrchr(buf, '/');
  if(!slash) return false;
  *slash = '\0';
  return path_copy(out,PATH_MAX,buf);
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
  if(n<=0) return false;
  buf[n] = 0;
  char *slash = strrchr(buf, '/');
  if(!slash) return false;
  *slash = '\0';
  return path_copy(out,PATH_MAX,buf);
#endif
}

static bool try_in_dir(const char*dir, const char*name, char out[PATH_MAX]){
  if(!dir||!name) return false;
  char p[PATH_MAX];
#if defined(_WIN32)
  if(!path_join(p,sizeof(p),dir,name,'\\'))return false;
#else
  if(!path_join(p,sizeof(p),dir,name,'/'))return false;
#endif
  if(try_open_font_path(p)&&path_copy(out,PATH_MAX,p))return true;
  return false;
}

/* Reforzado: primero cwd y directorio del ejecutable; luego rutas del sistema */
static const char* find_font_path_dynamic(char out[PATH_MAX], const char* cli){
  /* 0) CLI explícito */
  if(cli && try_open_font_path(cli)&&path_copy(out,PATH_MAX,cli))return out;

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
  bool have_resources=have_exe_dir&&path_join(resources,sizeof(resources),exedir,"../Resources",'/');
#endif

  for(size_t i=0;i<sizeof(local_first)/sizeof(local_first[0]); ++i){
    /* cwd */
    if(try_open_font_path(local_first[i])&&path_copy(out,PATH_MAX,local_first[i]))return out;
    /* exe dir */
    if(have_exe_dir && try_in_dir(exedir, local_first[i], out)) return out;
#if defined(__APPLE__)
    /* App bundle resources live beside Contents/MacOS. */
    if(have_resources && try_in_dir(resources,local_first[i],out)) return out;
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
      bool valid=i==1?path_join(start,sizeof(start),roots[i],"Microsoft\\Windows\\Fonts",'\\'):path_copy(start,sizeof(start),roots[i]);
      if(!valid)continue;
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
      if(path_join(start,sizeof(start),roots[i],".local/share/fonts",'/')&&search_dir_posix(start,out)) return out;
      if(path_join(start,sizeof(start),roots[i],".fonts",'/')&&search_dir_posix(start,out)) return out;
#if defined(__APPLE__)
      if(path_join(start,sizeof(start),roots[i],"Library/Fonts",'/')&&search_dir_posix(start,out)) return out;
#endif
    }else{
      if(path_copy(start,sizeof(start),roots[i])&&search_dir_posix(start,out)) return out;
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
static void draw_round_rect(SDL_Renderer*r,SDL_Rect rc,int radius,SDL_Color c){
  if(radius<1){draw_rect(r,rc.x,rc.y,rc.w,rc.h,c);return;}
  if(radius*2>rc.w)radius=rc.w/2;
  if(radius*2>rc.h)radius=rc.h/2;
  draw_rect(r,rc.x+radius,rc.y,rc.w-radius*2,rc.h,c);
  draw_rect(r,rc.x,rc.y+radius,rc.w,rc.h-radius*2,c);
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
  for(int y=0;y<radius;y++)for(int x=0;x<radius;x++)if((x-radius)*(x-radius)+(y-radius)*(y-radius)<=radius*radius){
    SDL_RenderDrawPoint(r,rc.x+x,rc.y+y);SDL_RenderDrawPoint(r,rc.x+rc.w-1-x,rc.y+y);
    SDL_RenderDrawPoint(r,rc.x+x,rc.y+rc.h-1-y);SDL_RenderDrawPoint(r,rc.x+rc.w-1-x,rc.y+rc.h-1-y);
  }
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
static TTF_Font* cached_font(Gfx*g,int requested){int best=0;for(int i=1;i<FONT_CACHE_COUNT;i++)if(abs(font_cache_sizes[i]-requested)<abs(font_cache_sizes[best]-requested))best=i;return g->fonts[best];}
static void update_font_tiers(Gfx*g,const UI*ui){AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts f=geometry_font_sizes(&a,g->width,g->height);g->font_big=cached_font(g,f.heading);g->font_small=cached_font(g,f.control);g->font_note=cached_font(g,f.note);g->font_cell=cached_font(g,f.cell);g->font_body=cached_font(g,f.body);}
static SDL_Rect language_rect(const Gfx*g,const UI*ui){ AppGeometry a; return get_geometry(g,ui,&a)?sdl_rect(a.language):(SDL_Rect){0,0,0,0}; }
static bool draw_fitted_text(Gfx*g,const char*text,SDL_Color color,SDL_Rect r,int preferred,int minimum){for(int i=FONT_CACHE_COUNT-1;i>=0;i--){if(font_cache_sizes[i]>preferred||font_cache_sizes[i]<minimum)continue;int w=0,h=0;if(TTF_SizeUTF8(g->fonts[i],text,&w,&h)==0&&w<=r.w-8&&h<=r.h-4){SDL_Texture*t=render_text(g,g->fonts[i],text,color,&w,&h);if(!t)return false;SDL_Rect d={r.x+(r.w-w)/2,r.y+(r.h-h)/2,w,h};SDL_RenderCopy(g->ren,t,NULL,&d);SDL_DestroyTexture(t);return true;}}return false;}
static void draw_centered_text(Gfx*g,TTF_Font*f,const char*text,SDL_Color color,SDL_Rect r){(void)f;(void)draw_fitted_text(g,text,color,r,20,12);}
static SDL_Rect language_segment(const Gfx*g,const UI*ui,int index){SDL_Rect r=language_rect(g,ui);int x0=r.x+r.w*index/LANG_COUNT,x1=r.x+r.w*(index+1)/LANG_COUNT;return(SDL_Rect){x0,r.y,x1-x0,r.h};}
static void draw_language_selector(Gfx*g,const UI*ui){Theme th=ui->dark_theme?theme_dark():theme_light();for(int i=0;i<LANG_COUNT;i++){SDL_Rect r=language_segment(g,ui,i);SDL_Color bg=i==(int)ui->language?th.title:th.btn;draw_rect(g->ren,r.x,r.y,r.w,r.h,bg);draw_centered_text(g,g->font_small,language_name((Language)i),i==(int)ui->language?th.bg:th.btnfg,r);}}
static void draw_wordmark(Gfx*g,SDL_Rect box){if(!g->wordmark)return;double scale=(double)box.w/sudokura_wordmark_width;if(sudokura_wordmark_height*scale>box.h)scale=(double)box.h/sudokura_wordmark_height;SDL_Rect d={box.x+(box.w-(int)(sudokura_wordmark_width*scale))/2,box.y+(box.h-(int)(sudokura_wordmark_height*scale))/2,(int)(sudokura_wordmark_width*scale),(int)(sudokura_wordmark_height*scale)};SDL_RenderCopy(g->ren,g->wordmark,NULL,&d);}

typedef struct { SDL_Rect board,side; AppGeometry model; } Layout;
typedef struct { SDL_Rect btn[9],pal[9],palette_label,progress; int count_btn,count_pal; } SidebarRects;
static Layout compute_layout(int W,int H,Mode mode){ Layout l; memset(&l,0,sizeof l); if(geometry_compute(W,H,geometry_mode(mode),&l.model)){l.board=sdl_rect(l.model.board);l.side=sdl_rect(l.model.sidebar);}return l; }
static void compute_sidebar_rects(const Layout*l,SidebarRects*r){ memset(r,0,sizeof *r);r->count_btn=r->count_pal=9;r->palette_label=sdl_rect(l->model.palette_label);r->progress=sdl_rect(l->model.progress);for(int i=0;i<9;i++){r->btn[i]=sdl_rect(l->model.actions[i]);r->pal[i]=sdl_rect(l->model.palette[i]);}}
static SDL_Rect back_rect(const Gfx*g,const UI*ui){AppGeometry a;return get_geometry(g,ui,&a)?sdl_rect(a.back_button):(SDL_Rect){0,0,0,0};}
static void draw_back_button(Gfx*g,const UI*ui,Theme th){SDL_Rect r=back_rect(g,ui);draw_round_rect(g->ren,r,7,th.btn);(void)draw_fitted_text(g,tr(ui->language,T_BACK),th.btnfg,r,20,10);}

/* =================== RENDER SCREENS =================== */
static void render_board_and_sidebar(Gfx*g,const Game*game, UI*ui){
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255);
  SDL_RenderClear(g->ren);

  Layout L = compute_layout(g->width,g->height,ui->mode);
  int gx=L.board.x, gy=L.board.y, side=L.board.w, cs=side/9;
  if(side <= 0 || cs <= 0) return;

  /* board bg + shadow */
  draw_round_rect(g->ren,(SDL_Rect){gx-6,gy-6,side+12,side+12},10,th.shadow);
  draw_round_rect(g->ren,(SDL_Rect){L.side.x-6,L.side.y-6,L.side.w+12,L.side.h+12},10,th.shadow);
  draw_round_rect(g->ren,L.side,8,th.btn);
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
      int tw,thh; SDL_Texture*t=render_text(g,g->font_cell,buf,col,&tw,&thh);
      if(t){ SDL_Rect d={ x + (cs-tw)/2, y + (cs-thh)/2, tw, thh }; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
    }else if(game->notes[IDX(r,c)]){
      for(int vv=1;vv<=9;vv++) if(game->notes[IDX(r,c)]&(1u<<vv)){
        char b[2]={(char)('0'+vv),0}; SDL_Color ncol=th.dim; int tw,thh;
        SDL_Texture*t=render_text(g,g->font_note,b,ncol,&tw,&thh);
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
  draw_wordmark(g,title_rect);
  char buf[64];
  const char* modeName=tr(ui->language,ui->mode==MODE_CLASSIC?T_CLASSIC:ui->mode==MODE_STRIKES?T_STRIKES:T_TIME_ATTACK);
  snprintf(buf,sizeof(buf),"%s: %s",tr(ui->language,T_MODE),modeName);
  const char*huds[3]={buf,NULL,NULL}; char time_text[64],extra_text[64]; double el=elapsed_time(ui);
  snprintf(time_text,sizeof time_text,"%s: %02d:%02d",tr(ui->language,T_TIME),(int)(el/60),(int)fmod(el,60));huds[1]=time_text;
  if(ui->mode==MODE_TIME){int rem=(int)(ui->time_limit_s-el);if(rem<0)rem=0;snprintf(extra_text,sizeof extra_text,"%s: %02d:%02d",tr(ui->language,T_TARGET),rem/60,rem%60);huds[2]=extra_text;}
  else if(ui->mode==MODE_STRIKES){int left=ui->strikes_max-ui->strikes;if(left<0)left=0;snprintf(extra_text,sizeof extra_text,"%s: %d/%d",tr(ui->language,T_STRIKES_LEFT),left,ui->strikes_max);huds[2]=extra_text;}
  for(int i=0;i<L.model.hud_count;i++){SDL_Rect hr=sdl_rect(L.model.hud[i]);draw_round_rect(g->ren,hr,hr.h/2,th.palette_bg);(void)draw_fitted_text(g,huds[i],th.dim,hr,18,10);}

  /* Buttons */
  const char* labels[]={tr(ui->language,T_NEW),tr(ui->language,T_MODE),tr(ui->language,T_HINT),tr(ui->language,T_NOTES),tr(ui->language,T_VERIFY),tr(ui->language,T_THEME),tr(ui->language,T_HELP),tr(ui->language,T_ABOUT),tr(ui->language,T_MENU)};
  for(int i=0;i<R.count_btn;i++){
    SDL_Rect rc=R.btn[i];
    int mx,my;Uint32 mouse=SDL_GetMouseState(&mx,&my);bool hover=point_in(rc,mx,my);bool pressed=hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));bool active=i==3&&ui->notes_mode;
    SDL_Color color=active?th.title:pressed?th.sel:hover?th.hover:(i==0?th.title:th.btn);
    draw_round_rect(g->ren,rc,7,color);(void)draw_fitted_text(g,labels[i],active?th.bg:th.btnfg,rc,20,10);
  }

  /* Palette */
  (void)draw_fitted_text(g,tr(ui->language,T_PALETTE),th.dim,R.palette_label,16,10);
  for(int n=1;n<=9;n++){
    SDL_Rect rc=R.pal[n-1];
    int mx,my;Uint32 mouse=SDL_GetMouseState(&mx,&my);bool hover=point_in(rc,mx,my);bool selected=game->puzzle[IDX(ui->sel_r,ui->sel_c)]==n;SDL_Color color=selected?th.title:(hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT)))?th.sel:hover?th.hover:th.palette_bg;
    draw_round_rect(g->ren,rc,7,color);char nb[2]={(char)('0'+n),0};(void)draw_fitted_text(g,nb,th.palette_fg,rc,24,12);
  }

  /* bottom info */
  int filled=0; for(int i=0;i<81;i++) if(game->puzzle[i]) filled++;
  snprintf(buf,sizeof(buf),tr(ui->language,T_PROGRESS),(filled*100)/81,ui->mistakes);
  (void)sw;(void)draw_fitted_text(g,buf,th.dim,R.progress,16,10);

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
  SDL_Rect heading=sdl_rect(a.title_heading); draw_wordmark(g,heading);
  SDL_Rect r_mode=sdl_rect(a.title_buttons[0]),r_start=sdl_rect(a.title_buttons[1]),r_help=sdl_rect(a.title_buttons[2]),r_about=sdl_rect(a.title_buttons[3]),r_quit=sdl_rect(a.title_buttons[4]);
  draw_round_rect(g->ren,r_mode,7,th.btn);
  const char* modeName=tr(ui->language,ui->mode==MODE_CLASSIC?T_CLASSIC:ui->mode==MODE_STRIKES?T_STRIKES:T_TIME_ATTACK);
  char mline[64]; snprintf(mline,sizeof(mline),"%s: %s",tr(ui->language,T_MODE),modeName);
  (void)draw_fitted_text(g,mline,th.btnfg,r_mode,20,10);

  draw_round_rect(g->ren,r_start,7,th.title);(void)draw_fitted_text(g,tr(ui->language,T_START),th.bg,r_start,22,10);

  draw_round_rect(g->ren,r_help,7,th.btn);(void)draw_fitted_text(g,tr(ui->language,T_HELP),th.btnfg,r_help,20,10);

  draw_round_rect(g->ren,r_about,7,th.btn);(void)draw_fitted_text(g,tr(ui->language,T_ABOUT),th.btnfg,r_about,20,10);

  draw_round_rect(g->ren,r_quit,7,th.btn);(void)draw_fitted_text(g,tr(ui->language,T_QUIT),th.btnfg,r_quit,20,10);
}

static void blit_wrapped(Gfx*g,int x,int y,int w,int max_h,const char*text, SDL_Color col){
  int tw,thh; SDL_Texture* t=render_text_wrapped(g,g->font_body,text,col,w,&tw,&thh);
  if(t){int visible=thh<max_h?thh:max_h;SDL_Rect source={0,0,tw,visible},d={x,y,tw,visible};SDL_RenderCopy(g->ren,t,&source,&d);SDL_DestroyTexture(t);}
}
static void render_help(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;SDL_Rect heading=sdl_rect(a.info_heading),body=sdl_rect(a.info_body);
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,tr(ui->language,T_HELP), th.title,&tw,&thh);
  if(T){ SDL_Rect d={heading.x,heading.y,tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }
  const char* text=tr(ui->language,T_HELP_BODY);blit_wrapped(g,body.x,body.y,body.w,body.h,text,th.dim);
  draw_back_button(g, ui, th);
}

static void render_about(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;SDL_Rect heading=sdl_rect(a.info_heading),body=sdl_rect(a.info_body);
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,tr(ui->language,T_ABOUT), th.title,&tw,&thh);
  if(T){ SDL_Rect d={heading.x,heading.y,tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }
  const char* text=tr(ui->language,T_ABOUT_BODY);blit_wrapped(g,body.x,body.y,body.w,body.h,text,th.dim);
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
  (void)draw_fitted_text(g,line,th.dim,summary,20,10);

  SDL_Rect b1=sdl_rect(a.end_buttons[0]),b2=sdl_rect(a.end_buttons[1]);
  draw_round_rect(g->ren,b1,7,th.title);(void)draw_fitted_text(g,tr(ui->language,ui->result==RES_WIN?T_NEXT:T_RETRY),th.bg,b1,22,10);
  draw_round_rect(g->ren,b2,7,th.btn);(void)draw_fitted_text(g,tr(ui->language,T_MENU),th.btnfg,b2,20,10);
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

static void app_render(Gfx*g,const Game*game,UI*ui){
  update_font_tiers(g,ui);
  if(ui->screen==SCR_TITLE)render_title(g,ui);
  else if(ui->screen==SCR_HELP)render_help(g,ui);
  else if(ui->screen==SCR_ABOUT)render_about(g,ui);
  else if(ui->screen==SCR_END)render_end(g,ui);
  else render_board_and_sidebar(g,game,ui);
  draw_language_selector(g,ui);
}

static void app_frame(Gfx*g,const Game*game,UI*ui){app_render(g,game,ui);SDL_RenderPresent(g->ren);}

static bool save_frame(Gfx*g,const char*path){
  SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,g->width,g->height,32,SDL_PIXELFORMAT_ARGB8888);
  if(!s)return false;
  bool ok=SDL_RenderReadPixels(g->ren,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)==0&&SDL_SaveBMP(s,path)==0;
  SDL_FreeSurface(s);return ok;
}

static void app_shutdown(Gfx*g){
  if(!g)return;
  if(g->wordmark)SDL_DestroyTexture(g->wordmark);
  for(int i=0;i<FONT_CACHE_COUNT;i++)if(g->fonts[i])TTF_CloseFont(g->fonts[i]);
  if(g->ren)SDL_DestroyRenderer(g->ren);
  if(g->win)SDL_DestroyWindow(g->win);
  memset(g,0,sizeof(*g));
  if(TTF_WasInit())TTF_Quit();
  SDL_Quit();
}

static bool app_init(Gfx*g,const char*font_cli){
  memset(g,0,sizeof(*g));g->width=1024;g->height=720;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return false;}
  if(TTF_Init()!=0){fprintf(stderr,"TTF_Init: %s\n",TTF_GetError());app_shutdown(g);return false;}
  g->win=SDL_CreateWindow(SUDOKURA_NAME_VERSION,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,g->width,g->height,SDL_WINDOW_RESIZABLE);
  if(!g->win){fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());app_shutdown(g);return false;}
  SDL_SetWindowMinimumSize(g->win,360,480);
  g->ren=SDL_CreateRenderer(g->win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(!g->ren)g->ren=SDL_CreateRenderer(g->win,-1,SDL_RENDERER_SOFTWARE);
  if(!g->ren){fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());app_shutdown(g);return false;}
  SDL_Surface*icon=SDL_CreateRGBSurfaceWithFormatFrom((void*)sudokura_icon_rgba,(int)sudokura_icon_width,(int)sudokura_icon_height,32,(int)sudokura_icon_width*4,SDL_PIXELFORMAT_RGBA32);
  if(icon){SDL_SetWindowIcon(g->win,icon);SDL_FreeSurface(icon);}
  SDL_Surface*word=SDL_CreateRGBSurfaceWithFormatFrom((void*)sudokura_wordmark_rgba,(int)sudokura_wordmark_width,(int)sudokura_wordmark_height,32,(int)sudokura_wordmark_width*4,SDL_PIXELFORMAT_RGBA32);
  if(word){g->wordmark=SDL_CreateTextureFromSurface(g->ren,word);SDL_FreeSurface(word);}
  if(!g->wordmark){fprintf(stderr,"wordmark texture: %s\n",SDL_GetError());app_shutdown(g);return false;}
  char font_path[PATH_MAX]={0};const char*fpath=find_font_path_dynamic(font_path,font_cli);
  if(!fpath){fprintf(stderr,"%s\n",tr(LANG_EN,T_FONT_ERROR));app_shutdown(g);return false;}
  for(int i=0;i<FONT_CACHE_COUNT;i++){
    g->fonts[i]=TTF_OpenFont(fpath,font_cache_sizes[i]);
    if(!g->fonts[i]){fprintf(stderr,"font %d: %s\n",font_cache_sizes[i],TTF_GetError());app_shutdown(g);return false;}
  }
  g->font_big=cached_font(g,44);g->font_small=cached_font(g,20);g->font_note=cached_font(g,12);g->font_cell=cached_font(g,28);g->font_body=cached_font(g,18);
  return true;
}

static bool make_directory(const char*path){
  char part[PATH_MAX];size_t length=strlen(path);if(length==0||length>=sizeof(part))return false;
  if(!path_copy(part,sizeof(part),path))return false;
  for(size_t i=1;i<=length;i++)if(part[i]=='/'||part[i]=='\\'||part[i]=='\0'){
    char saved=part[i];part[i]='\0';
    if(part[0]){
#if defined(_WIN32)
      if(_mkdir(part)!=0&&errno!=EEXIST)return false;
#else
      if(mkdir(part,0755)!=0&&errno!=EEXIST)return false;
#endif
    }
    part[i]=saved;
  }
  return true;
}

static void apply_window_resize(Gfx*g,int requested_width,int requested_height){
  int width=0,height=0;
  bool changed=geometry_normalize_window_size(requested_width,requested_height,&width,&height);
  if(changed)SDL_SetWindowSize(g->win,width,height);
  SDL_GetWindowSize(g->win,&g->width,&g->height);
}

/* =================== MAIN =================== */
int main(int argc,char**argv){
  const char* font_cli=NULL;
  const char* screenshot_dir=NULL; bool smoke_test=false;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--font") && i+1<argc){ font_cli=argv[++i]; }
    else if(!strcmp(argv[i],"--smoke-test")){smoke_test=true;}
    else if(!strcmp(argv[i],"--render-screenshots")&&i+1<argc){screenshot_dir=argv[++i];}
  }

  Gfx g;
  if(!app_init(&g,font_cli))return 1;

  Game game; game_new(&game,(unsigned)time(NULL));

  UI ui; memset(&ui,0,sizeof(ui));
  ui.sel_r=4; ui.sel_c=4; ui.language=LANG_EN; ui.dark_theme=true; ui.mode=MODE_CLASSIC; ui.screen=SCR_TITLE; ui.prev_screen=SCR_TITLE;
  set_mode_params(&ui);

  if(smoke_test||screenshot_dir){
    game_new(&game,110u);
    ui.screen=SCR_TITLE;app_frame(&g,&game,&ui);
    ui.screen=SCR_PLAY;app_frame(&g,&game,&ui);
    if(screenshot_dir){
      const int sizes[][2]={{640,480},{800,600},{1024,720},{1366,768},{1920,1080},{2560,1440},{3440,1440},{360,640},{390,844},{412,915}};
      const Screen screens[]={SCR_TITLE,SCR_PLAY,SCR_HELP,SCR_END};const char*names[]={"title","play","help","result"};
      if(!make_directory(screenshot_dir)){fprintf(stderr,"cannot create screenshot directory: %s\n",screenshot_dir);app_shutdown(&g);return 1;}
      for(unsigned n=0;n<sizeof(sizes)/sizeof(sizes[0]);n++)for(int s=0;s<4;s++){
        SDL_SetWindowMinimumSize(g.win,1,1);SDL_SetWindowSize(g.win,sizes[n][0],sizes[n][1]);SDL_PumpEvents();SDL_GetRendererOutputSize(g.ren,&g.width,&g.height);
        if(g.width!=sizes[n][0]||g.height!=sizes[n][1]){fprintf(stderr,"renderer refused %dx%d (got %dx%d)\n",sizes[n][0],sizes[n][1],g.width,g.height);app_shutdown(&g);return 1;}
        ui.screen=screens[s];ui.language=(Language)((n+s)%LANG_COUNT);ui.dark_theme=((n+s)&1)==0;ui.result=RES_WIN;
        app_render(&g,&game,&ui);char path[PATH_MAX],filename[160];
        int filename_length=snprintf(filename,sizeof(filename),"%dx%d-%s-%s-%s.bmp",g.width,g.height,names[s],language_name(ui.language),ui.dark_theme?"dark":"light");
        if(filename_length<0||(size_t)filename_length>=sizeof(filename)||!path_join(path,sizeof(path),screenshot_dir,filename,'/')){fprintf(stderr,"screenshot path is too long: %s\n",screenshot_dir);app_shutdown(&g);return 1;}
        if(!save_frame(&g,path)){fprintf(stderr,"screenshot: %s\n",SDL_GetError());app_shutdown(&g);return 1;}
      }
      printf("rendered 40 UI review screenshots to %s\n",screenshot_dir);
    }
    app_shutdown(&g);return 0;
  }

  bool running=true; SDL_Event e;
  while(running){
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT) running=false;
      else if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){apply_window_resize(&g,e.window.data1,e.window.data2);}
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
        if(point_in(language_rect(&g,&ui),x,y)){for(int language=0;language<LANG_COUNT;language++)if(point_in(language_segment(&g,&ui,language),x,y))ui.language=(Language)language;continue;}

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

    app_frame(&g,&game,&ui);
  }

  app_shutdown(&g);
  return 0;
}
