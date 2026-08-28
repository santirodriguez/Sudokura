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
#include "session.h"
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
enum {
  PAUSE_MANUAL = 1u << 0,
  PAUSE_FOCUS = 1u << 1,
  PAUSE_MODAL = 1u << 2,
  PAUSE_HOME = 1u << 3,
  PAUSE_END = 1u << 4
};

typedef struct {
  int sel_r, sel_c;
  bool notes_mode, strict_mode, dark_theme;
  Language language;

  int mistakes, strikes, strikes_max;
  double elapsed_base, running_since;
  double time_limit_s;
  unsigned pause_reasons;

  bool session_open, has_session, is_daily;
  int daily_year, daily_month, daily_day;

  char toast[96]; double toast_t0; bool toast_on;

  Screen screen; Screen prev_screen; Mode mode; Result result;
} UI;

typedef struct {
  bool enabled;
  Preferences preferences;
  SessionState saved;
  char session_path[PATH_MAX];
  char preferences_path[PATH_MAX];
  double last_checkpoint;
} Persistence;

static double now_s(void){ return SDL_GetTicks()*0.001; }
static bool clock_running(const UI*ui){
  return ui&&ui->session_open&&ui->screen==SCR_PLAY&&ui->pause_reasons==0;
}
static double elapsed_time(const UI*ui){
  if(!ui)return 0;
  double elapsed=ui->elapsed_base;
  if(clock_running(ui))elapsed+=now_s()-ui->running_since;
  return elapsed<0?0:elapsed;
}
static void clock_pause(UI*ui,unsigned reason){
  if(!ui||!reason)return;
  if(clock_running(ui))ui->elapsed_base+=now_s()-ui->running_since;
  ui->pause_reasons|=reason;
}
static void clock_resume(UI*ui,unsigned reason){
  if(!ui||!reason)return;
  ui->pause_reasons&=~reason;
  if(clock_running(ui))ui->running_since=now_s();
}
static void clock_reset(UI*ui){
  if(!ui)return;
  ui->elapsed_base=0;
  ui->pause_reasons=0;
  ui->running_since=now_s();
}
static void show_toast(UI*ui,const char*msg){ snprintf(ui->toast,sizeof(ui->toast),"%s",msg); ui->toast_t0=now_s(); ui->toast_on=true; }
static void set_mode_params(UI*ui){
  ui->strikes_max=3;
  ui->time_limit_s = (ui->mode==MODE_TIME ? 10*60 : 0);
}
static void begin_fresh_play(UI*ui){
  ui->screen=SCR_PLAY; ui->prev_screen=SCR_PLAY; ui->result=RES_NONE;
  ui->mistakes=0; ui->strikes=0; ui->notes_mode=false; ui->session_open=true;
  set_mode_params(ui); clock_reset(ui);
}
static bool apply_player_input(Game*game,UI*ui,int row,int column,int value,bool notes_mode){
  int i=IDX(row,column);
  GameInputResult result=game_apply_input(game,row,column,value,notes_mode,ui->strict_mode);
  if(result==GAME_INPUT_WRONG){
    ui->mistakes++; if(ui->mode==MODE_STRIKES)ui->strikes++; show_toast(ui,tr(ui->language,T_WRONG));
  }else if(result==GAME_INPUT_STRICT_REJECTED && value>=1 && value<=9 && value!=game->solution[i]){
    ui->mistakes++; if(ui->mode==MODE_STRIKES)ui->strikes++; show_toast(ui,tr(ui->language,T_ILLEGAL));
  }
  return result!=GAME_INPUT_NO_CHANGE&&result!=GAME_INPUT_LOCKED;
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


static uint64_t next_seed(void){
  static uint64_t sequence=UINT64_C(0x9e3779b97f4a7c15);
  sequence+=UINT64_C(0x9e3779b97f4a7c15);
  uint64_t wall=(uint64_t)time(NULL);
  uint64_t perf=(uint64_t)SDL_GetPerformanceCounter();
  return (wall<<32)^perf^sequence^(perf>>17);
}

static bool local_date(int*year,int*month,int*day){
  if(!year||!month||!day)return false;
  time_t current=time(NULL);struct tm*local=localtime(&current);if(!local)return false;
  *year=local->tm_year+1900;*month=local->tm_mon+1;*day=local->tm_mday;return true;
}

static bool pref_path(char out[PATH_MAX],const char*base,const char*leaf){
  if(!out||!base||!leaf)return false;
  int n=snprintf(out,PATH_MAX,"%s%s",base,leaf);
  return n>=0&&n<PATH_MAX;
}

static void persistence_init(Persistence*p){
  if(!p)return;
  memset(p,0,sizeof(*p));preferences_defaults(&p->preferences);
  char*base=SDL_GetPrefPath("santirodriguez","Sudokura");
  if(!base)return;
  bool paths_ok=pref_path(p->session_path,base,"session.dat")&&
                pref_path(p->preferences_path,base,"preferences.dat");
  SDL_free(base);if(!paths_ok)return;p->enabled=true;
  StoreStatus pref_status=preferences_load_file(p->preferences_path,&p->preferences);
  if(pref_status==STORE_CORRUPT){
    (void)store_quarantine_corrupt(p->preferences_path);preferences_defaults(&p->preferences);
  }else if(pref_status==STORE_IO_ERROR){
    preferences_defaults(&p->preferences);
  }
  StoreStatus session_status=session_load_file(p->session_path,&p->saved);
  if(session_status==STORE_CORRUPT)(void)store_quarantine_corrupt(p->session_path);
  if(session_status!=STORE_OK)memset(&p->saved,0,sizeof(p->saved));
  p->last_checkpoint=now_s();
}

static bool save_preferences(Persistence*p,const UI*ui){
  if(!p||!p->enabled||!ui)return false;
  p->preferences.language=ui->language;
  p->preferences.dark_theme=ui->dark_theme;
  p->preferences.strict_mode=ui->strict_mode;
  return preferences_save_file(p->preferences_path,&p->preferences);
}

static SessionStatus runtime_session_status(const Game*game,const UI*ui){
  if(game_is_solved(game))return SESSION_WON;
  if(game_mode_lost(ui->mode,ui->strikes,ui->strikes_max,elapsed_time(ui),ui->time_limit_s))return SESSION_LOST;
  return SESSION_ACTIVE;
}

static void capture_session(SessionState*out,const Game*game,const UI*ui){
  if(!out||!game||!ui)return;
  memset(out,0,sizeof(*out));out->game=*game;
  out->mode=ui->mode;out->selected_row=ui->sel_r;out->selected_column=ui->sel_c;
  out->notes_mode=ui->notes_mode;out->strict_mode=ui->strict_mode;
  out->manual_paused=(ui->pause_reasons&PAUSE_MANUAL)!=0;
  out->status=runtime_session_status(game,ui);out->mistakes=ui->mistakes;out->strikes=ui->strikes;
  double elapsed=elapsed_time(ui);if(elapsed<0)elapsed=0;
  out->elapsed_ms=(uint64_t)(elapsed*1000.0+0.5);
  out->is_daily=ui->is_daily;out->daily_year=ui->daily_year;
  out->daily_month=ui->daily_month;out->daily_day=ui->daily_day;
}

static bool save_current_session(Persistence*p,const Game*game,UI*ui){
  if(!p||!p->enabled||!game||!ui||!ui->session_open)return false;
  SessionState state;capture_session(&state,game,ui);
  if(!session_save_file(p->session_path,&state))return false;
  p->saved=state;ui->has_session=true;p->last_checkpoint=now_s();return true;
}

static bool saved_has_progress(const SessionState*state){
  if(!state||!session_validate(state))return false;
  if(state->status!=SESSION_ACTIVE||state->elapsed_ms||state->mistakes||state->strikes)return true;
  for(int i=0;i<81;i++)if(state->game.puzzle[i]!=state->game.initial[i]||state->game.hinted[i]||state->game.notes[i])return true;
  return false;
}

static void restore_saved_session(Persistence*p,Game*game,UI*ui){
  if(!p||!game||!ui||!session_validate(&p->saved))return;
  const SessionState*s=&p->saved;*game=s->game;ui->mode=s->mode;set_mode_params(ui);
  ui->sel_r=s->selected_row;ui->sel_c=s->selected_column;ui->notes_mode=s->notes_mode;
  ui->strict_mode=s->strict_mode;ui->mistakes=s->mistakes;ui->strikes=s->strikes;
  ui->elapsed_base=(double)s->elapsed_ms/1000.0;ui->running_since=now_s();
  ui->pause_reasons=s->manual_paused?PAUSE_MANUAL:0;ui->session_open=true;ui->has_session=true;
  ui->is_daily=s->is_daily;ui->daily_year=s->daily_year;ui->daily_month=s->daily_month;ui->daily_day=s->daily_day;
  if(s->status==SESSION_WON){ui->result=RES_WIN;ui->screen=SCR_END;ui->pause_reasons|=PAUSE_END;}
  else if(s->status==SESSION_LOST){ui->result=RES_LOSE;ui->screen=SCR_END;ui->pause_reasons|=PAUSE_END;}
  else{ui->result=RES_NONE;ui->screen=SCR_PLAY;}
  ui->prev_screen=ui->screen;p->last_checkpoint=now_s();
}

static void start_new_session(Persistence*p,Game*game,UI*ui){
  ui->mode=p->preferences.mode;ui->strict_mode=p->preferences.strict_mode;
  game_new_difficulty(game,next_seed(),p->preferences.difficulty);
  ui->is_daily=false;ui->daily_year=ui->daily_month=ui->daily_day=0;ui->sel_r=4;ui->sel_c=4;
  begin_fresh_play(ui);(void)save_current_session(p,game,ui);
}

static bool start_daily_session(Persistence*p,Game*game,UI*ui,int year,int month,int day){
  if(!game_new_daily(game,year,month,day))return false;
  ui->mode=MODE_CLASSIC;ui->strict_mode=p->preferences.strict_mode;
  ui->is_daily=true;ui->daily_year=year;ui->daily_month=month;ui->daily_day=day;ui->sel_r=4;ui->sel_c=4;
  begin_fresh_play(ui);(void)save_current_session(p,game,ui);return true;
}

static bool saved_daily_matches(const Persistence*p,int year,int month,int day){
  return p&&session_validate(&p->saved)&&p->saved.is_daily&&
         p->saved.daily_year==year&&p->saved.daily_month==month&&p->saved.daily_day==day;
}

static void restart_current_session(Persistence*p,Game*game,UI*ui){
  game_restart(game);ui->sel_r=4;ui->sel_c=4;begin_fresh_play(ui);
  (void)save_current_session(p,game,ui);
}

static void go_home(Persistence*p,const Game*game,UI*ui){
  if(ui->session_open){clock_pause(ui,PAUSE_HOME);(void)save_current_session(p,game,ui);}
  ui->session_open=false;ui->screen=SCR_TITLE;ui->prev_screen=SCR_TITLE;ui->result=RES_NONE;
  ui->pause_reasons=0;ui->mode=p->preferences.mode;ui->strict_mode=p->preferences.strict_mode;
  ui->notes_mode=false;set_mode_params(ui);
}

static void open_info_from_play(Persistence*p,const Game*game,UI*ui,Screen screen){
  clock_pause(ui,PAUSE_MODAL);(void)save_current_session(p,game,ui);
  ui->prev_screen=SCR_PLAY;ui->screen=screen;
}

static void return_from_info(Persistence*p,const Game*game,UI*ui){
  if(ui->prev_screen==SCR_PLAY){ui->screen=SCR_PLAY;clock_resume(ui,PAUSE_MODAL);(void)save_current_session(p,game,ui);}
  else ui->screen=ui->prev_screen;
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
