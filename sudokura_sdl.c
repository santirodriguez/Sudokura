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
#else
  #include <dirent.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

/* =================== SUDOKU CORE =================== */
#define N 9
#define NN (N*N)
static inline int IDX(int r,int c){ return r*N + c; }

typedef struct {
  int puzzle[NN];
  int solution[NN];
  unsigned char fixed[NN];
  uint16_t notes[NN]; /* bitmask: bit v (1..9) */
} Game;

static inline bool row_has(const int*b,int r,int v){ for(int c=0;c<9;c++) if(b[IDX(r,c)]==v) return true; return false; }
static inline bool col_has(const int*b,int c,int v){ for(int r=0;r<9;r++) if(b[IDX(r,c)]==v) return true; return false; }
static inline bool box_has(const int*b,int r,int c,int v){
  int br=(r/3)*3, bc=(c/3)*3;
  for(int rr=0;rr<3;rr++) for(int cc=0;cc<3;cc++)
    if(b[IDX(br+rr,bc+cc)]==v) return true;
  return false;
}
static inline bool can_place_local(const int*b,int r,int c,int v){
  return !row_has(b,r,v)&&!col_has(b,c,v)&&!box_has(b,r,c,v);
}
static void shuffle(int *a,int n){ for(int i=n-1;i>0;--i){ int j=rand()%(i+1); int t=a[i]; a[i]=a[j]; a[j]=t; } }

static int find_mrv(const int*b){
  int best=-1, bc=10;
  for(int i=0;i<81;i++){
    if(b[i]) continue;
    int r=i/9,c=i%9, cnt=0;
    for(int v=1;v<=9;v++) if(can_place_local(b,r,c,v)) cnt++;
    if(cnt<bc){ bc=cnt; best=i; if(cnt==1) break; }
  }
  return best;
}
static bool rec_first(int*b,int*out){
  int i=find_mrv(b); if(i<0){ memcpy(out,b,81*sizeof(int)); return true; }
  int r=i/9,c=i%9,opts[9],o=0; for(int v=1;v<=9;v++) if(can_place_local(b,r,c,v)) opts[o++]=v;
  for(int k=0;k<o;k++){ b[i]=opts[k]; if(rec_first(b,out)) return true; b[i]=0; } return false;
}
static int count_limit(int*b,int limit){
  int i=find_mrv(b); if(i<0) return 1;
  int r=i/9,c=i%9,opts[9],o=0; for(int v=1;v<=9;v++) if(can_place_local(b,r,c,v)) opts[o++]=v;
  int tot=0; for(int k=0;k<o;k++){ b[i]=opts[k]; int got=count_limit(b,limit-tot); tot+=got; if(tot>=limit){ b[i]=0; return tot; } b[i]=0; } return tot;
}
static bool unique_solution(const int*puz,int*out_sol){
  int tmp[81]; memcpy(tmp,puz,sizeof(tmp)); if(!rec_first(tmp,out_sol)) return false;
  memcpy(tmp,puz,sizeof(tmp)); return count_limit(tmp,2)==1;
}

/* solved grid via pattern + shuffles */
static void make_solved(int*out){
  int rows[9]={0,1,2,3,4,5,6,7,8}, cols[9]={0,1,2,3,4,5,6,7,8}, nums[9]={1,2,3,4,5,6,7,8,9};
  int band[3]={0,1,2}; shuffle(band,3);
  int rin[3][3]={{0,1,2},{3,4,5},{6,7,8}}; for(int b=0;b<3;b++) shuffle(rin[b],3);
  int p=0; for(int b0=0;b0<3;b0++){ int bi=band[b0]; for(int i=0;i<3;i++) rows[p++]=rin[bi][i]; }
  int stack_[3]={0,1,2}; shuffle(stack_,3);
  int cin[3][3]={{0,1,2},{3,4,5},{6,7,8}}; for(int s=0;s<3;s++) shuffle(cin[s],3);
  p=0; for(int s0=0;s0<3;s0++){ int si=stack_[s0]; for(int i=0;i<3;i++) cols[p++]=cin[si][i]; }
  shuffle(nums,9);
  for(int r=0;r<9;r++) for(int c=0;c<9;c++){
    int r2=rows[r], c2=cols[c];
    int base=(r2*3 + r2/3 + c2) % 9;
    out[IDX(r,c)]=nums[base];
  }
}

/* remove clues to medium difficulty; unique-solution enforced */
static void remove_to_medium(int*grid){
  int minC=32,maxC=38;
  int pos[81]; for(int i=0;i<81;i++) pos[i]=i; shuffle(pos,81);
  int clues=81;
  for(int k=0;k<81;k++){
    int i=pos[k]; int r=i/9,c=i%9; int j=IDX(8-r,8-c);
    if(grid[i]==0 && grid[j]==0) continue;
    int bi=grid[i], bj=grid[j]; grid[i]=0; if(j!=i) grid[j]=0;
    int tmp[81]; memcpy(tmp,grid,sizeof(tmp)); int sol[81];
    bool ok=unique_solution(tmp,sol);
    int delta=(j==i)?1:2;
    if(!ok || (clues-delta)<minC){ grid[i]=bi; if(j!=i) grid[j]=bj; }
    else{ clues-=delta; if(clues<=maxC){ if(rand()%3==0) break; } }
  }
}

static void new_game(Game*g,unsigned seed){
  if(seed) srand(seed); else srand((unsigned)time(NULL));
  int solved[81]; make_solved(solved);
  int puzzle[81]; memcpy(puzzle,solved,sizeof(puzzle));
  remove_to_medium(puzzle);
  int finalSol[81];
  if(!unique_solution(puzzle,finalSol)){
    int safe[81]={
      5,3,0, 0,7,0, 0,0,0,
      6,0,0, 1,9,5, 0,0,0,
      0,9,8, 0,0,0, 0,6,0,
      8,0,0, 0,6,0, 0,0,3,
      4,0,0, 8,0,3, 0,0,1,
      7,0,0, 0,2,0, 0,0,6,
      0,6,0, 0,0,0, 2,8,0,
      0,0,0, 4,1,9, 0,0,5,
      0,0,0, 0,8,0, 0,7,9
    };
    memcpy(puzzle,safe,sizeof(safe)); unique_solution(puzzle,finalSol);
  }
  for(int i=0;i<81;i++){ g->puzzle[i]=puzzle[i]; g->solution[i]=finalSol[i]; g->fixed[i]=(puzzle[i]!=0); g->notes[i]=0; }
}
static bool is_solved(const Game*g){
  for(int i=0;i<81;i++){ if(g->puzzle[i]==0) return false; if(g->puzzle[i]!=g->solution[i]) return false; } return true;
}
static bool place(Game*g,int r,int c,int v,bool strict){
  if(g->fixed[IDX(r,c)]) return false;
  if(v==0){ g->puzzle[IDX(r,c)]=0; g->notes[IDX(r,c)]=0; return true; }
  if(strict && !can_place_local(g->puzzle,r,c,v)) return false;
  g->puzzle[IDX(r,c)]=v; g->notes[IDX(r,c)]=0; return true;
}
static bool give_hint(Game*g,int r,int c){
  if(g->fixed[IDX(r,c)]) return false;
  int corr=g->solution[IDX(r,c)];
  if(g->puzzle[IDX(r,c)]==corr) return false;
  g->puzzle[IDX(r,c)]=corr; g->notes[IDX(r,c)]=0; return true;
}
static bool has_conflict(const Game*g,int rr,int cc,int v){
  if(v==0) return false;
  for(int c=0;c<9;c++) if(c!=cc && g->puzzle[IDX(rr,c)]==v) return true;
  for(int r=0;r<9;r++) if(r!=rr && g->puzzle[IDX(r,cc)]==v) return true;
  int br=(rr/3)*3, bc=(cc/3)*3;
  for(int r=0;r<3;r++) for(int c=0;c<3;c++){
    int R=br+r, C=bc+c; if(R==rr&&C==cc) continue; if(g->puzzle[IDX(R,C)]==v) return true;
  } return false;
}
static int count_conflicts(const Game*g){
  int cnt=0; for(int r=0;r<9;r++) for(int c=0;c<9;c++){ int v=g->puzzle[IDX(r,c)]; if(v && has_conflict(g,r,c,v)) cnt++; } return cnt;
}

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

typedef enum {MODE_CLASSIC=0, MODE_STRIKES=1, MODE_TIME=2} Mode;
typedef enum {SCR_TITLE=0, SCR_PLAY=1, SCR_END=2, SCR_HELP=3, SCR_ABOUT=4} Screen;
typedef enum {RES_NONE=0, RES_WIN=1, RES_LOSE=2} Result;

typedef struct {
  int sel_r, sel_c;
  bool notes_mode, strict_mode, paused, dark_theme;

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
static const char* find_font_path_dynamic(char out[PATH_MAX], const char* cli){
  if(cli && try_open_font_path(cli)){ strncpy(out,cli,PATH_MAX); out[PATH_MAX-1]=0; return out; }
  const char* preferred[]={
    "DejaVuSans.ttf","DejaVuSans-Regular.ttf","NotoSans-Regular.ttf","LiberationSans-Regular.ttf",
    "FreeSans.ttf","Arial.ttf","Ubuntu-R.ttf","Cantarell-VF.otf","SFNS.ttf"
  };
  for(size_t i=0;i<sizeof(preferred)/sizeof(preferred[0]);++i)
    if(try_candidates(out,preferred[i])) return out;
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

/* Responsive layout */
typedef struct { SDL_Rect board; SDL_Rect side; bool right; } Layout;
static Layout compute_layout(int W,int H){
  Layout L; int margin=18; int gap=16; int minCell=28; int minSide = 9*minCell;

  if(W >= 980){
    int sidebarW=260;
    int side = H - 2*margin;
    int usableW = W - 2*margin - sidebarW - gap;
    if(side > usableW) side = usableW;
    if(side < minSide) side=minSide;
    side = (side/9)*9;
    L.board = (SDL_Rect){ margin, (H - side)/2, side, side };
    L.side  = (SDL_Rect){ L.board.x + L.board.w + gap, L.board.y, sidebarW, side };
    L.right = true;
  }else{
    int side = W - 2*margin;
    if(side < minSide) side = minSide;
    if(side > H - (margin*3 + 280)) side = H - (margin*3 + 280);
    if(side < minSide) side = minSide;
    side = (side/9)*9;
    L.board = (SDL_Rect){ (W - side)/2, margin, side, side };
    L.side  = (SDL_Rect){ margin, L.board.y + L.board.h + gap, W - 2*margin, 260 };
    L.right = false;
  }
  return L;
}

/* ===== Sidebar geometry: ONE source of truth for render + clicks ===== */
typedef struct {
  SDL_Rect btn[9];   /* New, Mode, Hint, Notes, Verify, Theme, Help, About, Menu */
  SDL_Rect pal[9];   /* numbers 1..9 */
  int count_btn;
  int count_pal;
  int title_h, info_h, info_lines;
} SidebarRects;

static void compute_sidebar_rects(const Layout*L, const UI*ui, SidebarRects*R){
  const int sx=L->side.x, sy=L->side.y, sw=L->side.w;
  const int title_h=46, info_h=24, gap_small=4, gap_big=8;
  int info_lines = 2; /* Mode + Time */
  if(ui->mode==MODE_TIME)     info_lines += 1;
  if(ui->mode==MODE_STRIKES)  info_lines += 1;

  int y = sy + title_h + gap_small + info_lines*(info_h + gap_small) + gap_big;
  int bw = sw;
  const int bh = 34;

  for(int i=0;i<9;i++){ R->btn[i] = (SDL_Rect){sx, y, bw, bh}; y += bh + gap_big; }
  y += 4 + info_h; /* "Palette" label + gap */
  for(int n=0;n<9;n++){ R->pal[n] = (SDL_Rect){sx, y, bw, bh}; y += bh + 6; }

  R->count_btn=9; R->count_pal=9; R->title_h=title_h; R->info_h=info_h; R->info_lines=info_lines;
}

/* Back button geometry + draw */
static SDL_Rect back_rect(const Gfx* g){
  int bw=140, bh=40, x=40, y= g->height - bh - 40;
  SDL_Rect r={x,y,bw,bh}; return r;
}
static void draw_back_button(Gfx*g, Theme th){
  SDL_Rect r = back_rect(g);
  draw_rect(g->ren, r.x, r.y, r.w, r.h, th.btn);
  int tw,thh; SDL_Texture* t=render_text(g,g->font_small,"Back", th.btnfg,&tw,&thh);
  if(t){ SDL_Rect d={r.x+12,r.y+(r.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
}

/* =================== RENDER SCREENS =================== */
static void render_board_and_sidebar(Gfx*g,const Game*game, UI*ui){
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255);
  SDL_RenderClear(g->ren);

  Layout L = compute_layout(g->width,g->height);
  int gx=L.board.x, gy=L.board.y, side=L.board.w, cs=side/9;

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

    if(v && has_conflict(game,r,c,v)) draw_rect(g->ren, x+2,y+2, cs-4,cs-4, th.conflict);

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
  SidebarRects R; compute_sidebar_rects(&L, ui, &R);

  int sx=L.side.x, sy=L.side.y, sw=L.side.w;
  int tw,thh;

  /* Title */
  SDL_Texture* tt=render_text(g,g->font_big,"Sudokura v1.0", th.title,&tw,&thh);
  if(tt){ SDL_Rect d={sx,sy,tw,thh}; SDL_RenderCopy(g->ren,tt,NULL,&d); SDL_DestroyTexture(tt); }

  /* HUD lines (fixed spacing to match compute_sidebar_rects) */
  const int line_y0 = sy + R.title_h + 0;
  int y=line_y0;
  char buf[64];
  const char* modeName=(ui->mode==MODE_CLASSIC?"Classic": ui->mode==MODE_STRIKES?"Strikes":"Time Attack");
  snprintf(buf,sizeof(buf),"Mode: %s", modeName);
  SDL_Texture* t1=render_text(g,g->font_small,buf, th.dim,&tw,&thh);
  if(t1){ SDL_Rect d={sx, y + (R.info_h-thh)/2, tw, thh}; SDL_RenderCopy(g->ren,t1,NULL,&d); SDL_DestroyTexture(t1); }
  y += R.info_h + 4;

  double el=elapsed_time(ui);
  snprintf(buf,sizeof(buf),"Time: %02d:%02d", (int)(el/60),(int)fmod(el,60));
  SDL_Texture* t2=render_text(g,g->font_small,buf, th.dim,&tw,&thh);
  if(t2){ SDL_Rect d={sx, y + (R.info_h-thh)/2, tw, thh}; SDL_RenderCopy(g->ren,t2,NULL,&d); SDL_DestroyTexture(t2); }
  y += R.info_h + 4;

  if(ui->mode==MODE_TIME){
    int rem=(int)(ui->time_limit_s-el); if(rem<0) rem=0;
    snprintf(buf,sizeof(buf),"Target: %02d:%02d", rem/60,rem%60);
    SDL_Texture* t=render_text(g,g->font_small,buf, th.dim,&tw,&thh);
    if(t){ SDL_Rect d={sx, y + (R.info_h-thh)/2, tw, thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
    y += R.info_h + 4;
  }
  if(ui->mode==MODE_STRIKES){
    int left=ui->strikes_max-ui->strikes; if(left<0) left=0;
    snprintf(buf,sizeof(buf),"Strikes left: %d/%d", left, ui->strikes_max);
    SDL_Texture* t=render_text(g,g->font_small,buf, th.dim,&tw,&thh);
    if(t){ SDL_Rect d={sx, y + (R.info_h-thh)/2, tw, thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
  }

  /* Buttons */
  const char* labels[]={"New","Mode","Hint","Notes (N/Shift)","Verify","Theme","Help","About","Menu"};
  for(int i=0;i<R.count_btn;i++){
    SDL_Rect rc=R.btn[i];
    draw_rect(g->ren,rc.x,rc.y,rc.w,rc.h,th.btn);
    SDL_Texture* t=render_text(g,g->font_small,labels[i], th.btnfg,&tw,&thh);
    if(t){ SDL_Rect d={rc.x+10,rc.y+(rc.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
  }

  /* Palette */
  SDL_Texture* tp=render_text(g,g->font_small,"Palette", th.dim, &tw,&thh);
  if(tp){ SDL_Rect d={sx, R.pal[0].y - (thh+6), tw, thh}; SDL_RenderCopy(g->ren,tp,NULL,&d); SDL_DestroyTexture(tp); }
  for(int n=1;n<=9;n++){
    SDL_Rect rc=R.pal[n-1];
    draw_rect(g->ren,rc.x,rc.y,rc.w,rc.h,th.palette_bg);
    char nb[2]={(char)('0'+n),0}; SDL_Texture* t=render_text(g,g->font_small,nb, th.palette_fg,&tw,&thh);
    if(t){ SDL_Rect d={rc.x+12,rc.y+(rc.h-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
  }

  /* bottom info */
  int filled=0; for(int i=0;i<81;i++) if(game->puzzle[i]) filled++;
  snprintf(buf,sizeof(buf),"Progress %d%%   Errors %d", (filled*100)/81, ui->mistakes);
  SDL_Texture* ts=render_text_wrapped(g,g->font_small, buf, th.dim, sw, &tw,&thh);
  if(ts){ SDL_Rect d={sx, R.pal[8].y + R.pal[8].h + 6, tw, thh}; SDL_RenderCopy(g->ren,ts,NULL,&d); SDL_DestroyTexture(ts); }

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

  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,"Sudokura v1.0", th.title,&tw,&thh);
  if(T){ SDL_Rect d={ (g->width-tw)/2, g->height/2-160, tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }

  int bx=g->width/2-180, by=g->height/2-60, bw=360, bh=42, gap=12;
  SDL_Rect r_mode={bx,by,bw,bh}; by+=bh+gap;
  SDL_Rect r_start={bx,by,bw,bh}; by+=bh+gap;
  SDL_Rect r_help={bx,by,bw,bh}; by+=bh+gap;
  SDL_Rect r_about={bx,by,bw,bh}; by+=bh+gap;
  SDL_Rect r_quit={bx,by,bw,bh};

  draw_rect(g->ren,r_mode.x,r_mode.y,bw,bh,th.btn);
  const char* modeName=(ui->mode==MODE_CLASSIC?"Classic": ui->mode==MODE_STRIKES?"Strikes":"Time Attack");
  char mline[64]; snprintf(mline,sizeof(mline),"Mode: %s", modeName);
  SDL_Texture* m1=render_text(g,g->font_small,mline, th.btnfg,&tw,&thh);
  if(m1){ SDL_Rect d={r_mode.x+12,r_mode.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,m1,NULL,&d); SDL_DestroyTexture(m1); }

  draw_rect(g->ren,r_start.x,r_start.y,bw,bh,(SDL_Color){(Uint8)(th.btn.r+10),(Uint8)(th.btn.g+10),(Uint8)(th.btn.b+10),th.btn.a});
  SDL_Texture* s=render_text(g,g->font_small,"Start", th.btnfg,&tw,&thh); if(s){ SDL_Rect d={r_start.x+12,r_start.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,s,NULL,&d); SDL_DestroyTexture(s); }

  draw_rect(g->ren,r_help.x,r_help.y,bw,bh,th.btn);
  SDL_Texture* h=render_text(g,g->font_small,"Help (F1)", th.btnfg,&tw,&thh); if(h){ SDL_Rect d={r_help.x+12,r_help.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,h,NULL,&d); SDL_DestroyTexture(h); }

  draw_rect(g->ren,r_about.x,r_about.y,bw,bh,th.btn);
  SDL_Texture* a=render_text(g,g->font_small,"About (F2)", th.btnfg,&tw,&thh); if(a){ SDL_Rect d={r_about.x+12,r_about.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,a,NULL,&d); SDL_DestroyTexture(a); }

  draw_rect(g->ren,r_quit.x,r_quit.y,bw,bh,th.btn);
  SDL_Texture* q=render_text(g,g->font_small,"Quit", th.btnfg,&tw,&thh); if(q){ SDL_Rect d={r_quit.x+12,r_quit.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,q,NULL,&d); SDL_DestroyTexture(q); }
}

static void blit_wrapped(Gfx*g,int x,int y,int w,const char*text, SDL_Color col){
  int tw,thh; SDL_Texture* t=render_text_wrapped(g,g->font_small,text,col,w,&tw,&thh);
  if(t){ SDL_Rect d={x,y,tw,thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }
}
static void render_help(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,"Help", th.title,&tw,&thh);
  if(T){ SDL_Rect d={60,40,tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }
  int x=60, y=40+thh+12, w=g->width-120;
  const char* text =
    "- Goal: fill the 9x9 grid so each row, column and 3x3 box contains 1..9 with no repeats.\n"
    "- Modes: Classic (no limits). Strikes (3 wrong moves = lose). Time Attack (solve under 10:00).\n"
    "- Select a cell with mouse or arrows (WASD). Place with keys 1..9 or the palette.\n"
    "- Notes: press N to toggle Notes mode or hold Shift while typing numbers.\n"
    "         You can also note with the mouse: click a sub-cell (the 3x3 mini-grid inside the cell).\n"
    "- Hint: fills the selected cell with the correct answer.\n"
    "- Verify: checks conflicts against Sudoku rules (rows/cols/boxes). It does not reveal the solution.\n"
    "- Strict mode (M): blocks illegal placements. Free mode allows them (they still count as mistakes).\n"
    "- Theme (T) toggles dark/light. Pause (P) pauses the timer. ESC or Back returns.";
  blit_wrapped(g,x,y,w,text, th.dim);
  draw_back_button(g, th);
}

static void render_about(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,"About", th.title,&tw,&thh);
  if(T){ SDL_Rect d={60,40,tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }
  int x=60, y=40+thh+12, w=g->width-120;
  const char* text =
    "Sudokura v1.0 — a compact, modern Sudoku written in C + SDL2.\n"
    "Author: santirodriguez — https://santiagorodriguez.com\n\n"
    "License: GNU General Public License v3. You can redistribute it and/or modify it under the terms of the GPLv3.\n"
    "See <https://www.gnu.org/licenses/> for details.\n\n"
    "Press ESC or Back to return.";
  blit_wrapped(g,x,y,w,text, th.dim);
  draw_back_button(g, th);
}

static void render_end(Gfx*g, UI*ui){
  Theme th = ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255); SDL_RenderClear(g->ren);
  const char* title = (ui->result==RES_WIN? "You Win" : "You Lose");
  int tw,thh; SDL_Texture* T=render_text(g,g->font_big,title, (ui->result==RES_WIN? (SDL_Color){60,220,160,255} : (SDL_Color){220,80,80,255}), &tw,&thh);
  if(T){ SDL_Rect d={ (g->width-tw)/2, 120, tw,thh}; SDL_RenderCopy(g->ren,T,NULL,&d); SDL_DestroyTexture(T); }

  char line[64]; double el=elapsed_time(ui);
  snprintf(line,sizeof(line),"Time %02d:%02d   Errors %d", (int)(el/60),(int)fmod(el,60), ui->mistakes);
  SDL_Texture* t=render_text(g,g->font_small,line, th.dim,&tw,&thh);
  if(t){ SDL_Rect d={ (g->width-tw)/2, 160+thh, tw, thh}; SDL_RenderCopy(g->ren,t,NULL,&d); SDL_DestroyTexture(t); }

  int bx=g->width/2-160, by=260, bw=320, bh=40, gap=12;
  SDL_Rect b1={bx,by,bw,bh}; by+=bh+gap; SDL_Rect b2={bx,by,bw,bh};
  draw_rect(g->ren,b1.x,b1.y,b1.w,b1.h, th.btn);
  SDL_Texture* s=render_text(g,g->font_small, (ui->result==RES_WIN? "Next" : "Retry"), th.btnfg,&tw,&thh);
  if(s){ SDL_Rect d={b1.x+12,b1.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,s,NULL,&d); SDL_DestroyTexture(s); }
  draw_rect(g->ren,b2.x,b2.y,b2.w,b2.h, th.btn);
  SDL_Texture* m=render_text(g,g->font_small,"Menu", th.btnfg,&tw,&thh);
  if(m){ SDL_Rect d={b2.x+12,b2.y+(bh-thh)/2,tw,thh}; SDL_RenderCopy(g->ren,m,NULL,&d); SDL_DestroyTexture(m); }
}

/* tiny confirm box */
static bool confirm_box(SDL_Window* win, const char* title, const char* msg, const char* ok_label){
  const SDL_MessageBoxButtonData buttons[] = {
    {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
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
  if(TTF_Init()!=0){ fprintf(stderr,"TTF_Init: %s\n", TTF_GetError()); return 1; }

  Gfx g={0}; g.width=1024; g.height=720;
  g.win = SDL_CreateWindow("Sudokura v1.0", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g.width, g.height, SDL_WINDOW_RESIZABLE);
  g.ren = SDL_CreateRenderer(g.win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(!g.win||!g.ren){ fprintf(stderr,"SDL window/renderer failed\n"); return 1; }

  /* robust font discovery */
  char font_path[PATH_MAX]={0};
  const char* fpath=find_font_path_dynamic(font_path, font_cli);
  if(!fpath){
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Sudokura v1.0",
      "Could not find a usable TTF/OTF font.\n"
      "Install any TrueType/OpenType font and try again,\n"
      "or run: ./sudokura --font /path/to/font.ttf", g.win);
    return 1;
  }

  g.font_big   = TTF_OpenFont(fpath, 44);
  g.font_small = TTF_OpenFont(fpath, 20);
  if(!g.font_big || !g.font_small){
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Sudokura v1.0","TTF_OpenFont failed with the chosen font.", g.win);
    return 1;
  }

  Game game; new_game(&game,(unsigned)time(NULL));

  UI ui; memset(&ui,0,sizeof(ui));
  ui.sel_r=4; ui.sel_c=4; ui.dark_theme=true; ui.mode=MODE_CLASSIC; ui.screen=SCR_TITLE; ui.prev_screen=SCR_TITLE;
  set_mode_params(&ui);

  bool running=true; SDL_Event e;
  while(running){
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT) running=false;
      else if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){ g.width=e.window.data1; g.height=e.window.data2; }
      else if(e.type==SDL_KEYDOWN){
        SDL_Keycode k=e.key.keysym.sym;
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
            new_game(&game,(unsigned)time(NULL));
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
          else if(k==SDLK_n){ ui.notes_mode=!ui.notes_mode; show_toast(&ui, ui.notes_mode?"Notes ON":"Notes OFF"); }
          else if(k==SDLK_m){ ui.strict_mode=!ui.strict_mode; show_toast(&ui, ui.strict_mode?"Strict":"Free"); }
          else if(k==SDLK_h){ if(give_hint(&game,r,c)) show_toast(&ui,"Hint used"); }
          else if(k==SDLK_DELETE || k==SDLK_BACKSPACE || k==SDLK_0 || k==SDLK_KP_0){
            if(!game.fixed[i]){ game.puzzle[i]=0; game.notes[i]=0; }
          } else {
            int v=0; if(k>=SDLK_1 && k<=SDLK_9) v=(k-SDLK_0); else if(k>=SDLK_KP_1 && k<=SDLK_KP_9) v=(k-SDLK_KP_0);
            if(v>=1 && v<=9){
              if(ui.notes_mode || shifted){ if(!game.fixed[i] && game.puzzle[i]==0) game.notes[i]^=(1u<<v); }
              else{
                if(place(&game,r,c,v, ui.strict_mode)){
                  if(v!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,"Wrong"); }
                } else {
                  if(v!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,"Illegal"); }
                }
              }
            }
          }
        }
      }
      else if(e.type==SDL_MOUSEBUTTONDOWN){
        int x=e.button.x, y=e.button.y; bool right=(e.button.button==SDL_BUTTON_RIGHT);

        if(ui.screen==SCR_TITLE){
          int bx=g.width/2-180, by=g.height/2-60, bw=360, bh=42, gap=12;
          SDL_Rect r_mode={bx,by,bw,bh}; by+=bh+gap; SDL_Rect r_start={bx,by,bw,bh}; by+=bh+gap;
          SDL_Rect r_help={bx,by,bw,bh}; by+=bh+gap; SDL_Rect r_about={bx,by,bw,bh}; by+=bh+gap; SDL_Rect r_quit={bx,by,bw,bh};
          if(point_in(r_mode,x,y)){ ui.mode=(ui.mode+1)%3; set_mode_params(&ui); }
          else if(point_in(r_start,x,y)){ ui.screen=SCR_PLAY; ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0; }
          else if(point_in(r_help,x,y)){ ui.prev_screen=SCR_TITLE; ui.screen=SCR_HELP; }
          else if(point_in(r_about,x,y)){ ui.prev_screen=SCR_TITLE; ui.screen=SCR_ABOUT; }
          else if(point_in(r_quit,x,y)) return 0;
        } else if(ui.screen==SCR_END){
          int bx=g.width/2-160, by=260, bw=320, bh=40, gap=12;
          SDL_Rect b1={bx,by,bw,bh}; by+=bh+gap; SDL_Rect b2={bx,by,bw,bh};
          if(point_in(b1,x,y)){ new_game(&game,(unsigned)time(NULL)); ui.screen=SCR_PLAY; ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0; }
          else if(point_in(b2,x,y)) ui.screen=SCR_TITLE;
        } else if(ui.screen==SCR_HELP || ui.screen==SCR_ABOUT){
          Theme th = ui.dark_theme?theme_dark():theme_light();
          (void)th;
          SDL_Rect r = back_rect(&g);
          if(point_in(r, x,y)) ui.screen=ui.prev_screen;
        } else {
          Layout L = compute_layout(g.width,g.height);
          int gx=L.board.x, gy=L.board.y, side=L.board.w, cs=side/9;
          if(x>=gx && x<gx+side && y>=gy && y<gy+side){
            int c=(x-gx)/cs, r=(y-gy)/cs; ui.sel_r=r; ui.sel_c=c;
            int i=IDX(r,c);
            if(!game.fixed[i]){
              int lx=x-(gx+c*cs), ly=y-(gy+r*cs);
              int sub=cs/3, qq=lx/sub, q=ly/sub; if(qq<0) qq=0; if(q<0) q=0; if(qq>2) qq=2; if(q>2) q=2;
              int vv=q*3+qq+1;
              if(right || ui.notes_mode){ if(game.puzzle[i]==0) game.notes[i]^=(1u<<vv); }
            }
          }else{
            SidebarRects R; compute_sidebar_rects(&L, &ui, &R);

            if(point_in(R.btn[0],x,y)){ /* New */
              if(confirm_box(g.win,"New game","Start a new game? Current progress will be lost.","New")){
                new_game(&game,(unsigned)time(NULL)); ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0;
              }
            }
            else if(point_in(R.btn[1],x,y)){ /* Mode */
              const char* next = (ui.mode==MODE_CLASSIC?"Strikes": ui.mode==MODE_STRIKES?"Time Attack":"Classic");
              char msg[128]; snprintf(msg,sizeof(msg),"Change mode to %s?\nThis will start a new game.", next);
              if(confirm_box(g.win,"Change mode",msg,"Change")){
                ui.mode = (ui.mode+1)%3; set_mode_params(&ui);
                new_game(&game,(unsigned)time(NULL)); ui.start_t=now_s(); ui.paused=false; ui.paused_accum=0; ui.mistakes=0; ui.strikes=0;
              }
            }
            else if(point_in(R.btn[2],x,y)){ if(give_hint(&game,ui.sel_r,ui.sel_c)) show_toast(&ui,"Hint used"); } /* Hint */
            else if(point_in(R.btn[3],x,y)){ ui.notes_mode=!ui.notes_mode; show_toast(&ui, ui.notes_mode?"Notes ON":"Notes OFF"); }
            else if(point_in(R.btn[4],x,y)){ int conf=count_conflicts(&game); if(conf==0) show_toast(&ui,"No conflicts"); else { char m[32]; snprintf(m,sizeof(m),"Conflicts: %d",conf); show_toast(&ui,m);} }
            else if(point_in(R.btn[5],x,y)){ ui.dark_theme=!ui.dark_theme; }
            else if(point_in(R.btn[6],x,y)){ ui.prev_screen=SCR_PLAY; ui.screen=SCR_HELP; }
            else if(point_in(R.btn[7],x,y)){ ui.prev_screen=SCR_PLAY; ui.screen=SCR_ABOUT; }
            else if(point_in(R.btn[8],x,y)){ ui.screen=SCR_TITLE; }
            else{
              for(int n=1;n<=9;n++) if(point_in(R.pal[n-1],x,y)){
                int i=IDX(ui.sel_r,ui.sel_c);
                if(!game.fixed[i]){
                  if(ui.notes_mode){ if(game.puzzle[i]==0) game.notes[i]^=(1u<<n); }
                  else{
                    if(place(&game,ui.sel_r,ui.sel_c,n, ui.strict_mode)){
                      if(n!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,"Wrong"); }
                    }else{
                      if(n!=game.solution[i]){ ui.mistakes++; if(ui.mode==MODE_STRIKES) ui.strikes++; show_toast(&ui,"Illegal"); }
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
      if(ui.mode==MODE_TIME && ui.time_limit_s>0 && elapsed_time(&ui)>ui.time_limit_s) lose=true;
      if(ui.mode==MODE_STRIKES && ui.strikes>=ui.strikes_max) lose=true;
      if(is_solved(&game)){ ui.result=RES_WIN; ui.screen=SCR_END; }
      else if(lose){ ui.result=RES_LOSE; ui.screen=SCR_END; }
    }

    if(ui.screen==SCR_TITLE) render_title(&g,&ui);
    else if(ui.screen==SCR_HELP) render_help(&g,&ui);
    else if(ui.screen==SCR_ABOUT) render_about(&g,&ui);
    else if(ui.screen==SCR_END) render_end(&g,&ui);
    else render_board_and_sidebar(&g,&game,&ui);

    SDL_RenderPresent(g.ren);
  }

  if(g.font_big) TTF_CloseFont(g.font_big);
  if(g.font_small) TTF_CloseFont(g.font_small);
  SDL_DestroyRenderer(g.ren); SDL_DestroyWindow(g.win);
  TTF_Quit(); SDL_Quit();
  return 0;
}
