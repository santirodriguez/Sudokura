/* =================== DRAW HELPERS =================== */
static void draw_rect(SDL_Renderer*r,int x,int y,int w,int h,SDL_Color c){
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
  SDL_Rect rc={x,y,w,h};SDL_RenderFillRect(r,&rc);
}
static void draw_round_rect(SDL_Renderer*r,SDL_Rect rc,int radius,SDL_Color c){
  if(radius<1){draw_rect(r,rc.x,rc.y,rc.w,rc.h,c);return;}
  if(radius*2>rc.w)radius=rc.w/2;
  if(radius*2>rc.h)radius=rc.h/2;
  draw_rect(r,rc.x+radius,rc.y,rc.w-radius*2,rc.h,c);
  draw_rect(r,rc.x,rc.y+radius,rc.w,rc.h-radius*2,c);
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
  for(int y=0;y<radius;y++)for(int x=0;x<radius;x++)
    if((x-radius)*(x-radius)+(y-radius)*(y-radius)<=radius*radius){
      SDL_RenderDrawPoint(r,rc.x+x,rc.y+y);
      SDL_RenderDrawPoint(r,rc.x+rc.w-1-x,rc.y+y);
      SDL_RenderDrawPoint(r,rc.x+x,rc.y+rc.h-1-y);
      SDL_RenderDrawPoint(r,rc.x+rc.w-1-x,rc.y+rc.h-1-y);
    }
}
static void draw_line(SDL_Renderer*r,int x1,int y1,int x2,int y2,SDL_Color c){
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
  SDL_RenderDrawLine(r,x1,y1,x2,y2);
}
static void draw_focus_ring(SDL_Renderer*r,SDL_Rect rc,SDL_Color c){
  for(int inset=0;inset<2;inset++){
    int x1=rc.x-inset,y1=rc.y-inset,x2=rc.x+rc.w-1+inset,y2=rc.y+rc.h-1+inset;
    draw_line(r,x1,y1,x2,y1,c);draw_line(r,x2,y1,x2,y2,c);
    draw_line(r,x2,y2,x1,y2,c);draw_line(r,x1,y2,x1,y1,c);
  }
}
static SDL_Texture*render_text(Gfx*g,TTF_Font*f,const char*txt,SDL_Color col,int*w,int*h){
  SDL_Surface*s=TTF_RenderUTF8_Blended(f,txt,col);if(!s)return NULL;
  SDL_Texture*t=SDL_CreateTextureFromSurface(g->ren,s);if(w)*w=s->w;if(h)*h=s->h;SDL_FreeSurface(s);return t;
}
static SDL_Texture*render_text_wrapped(Gfx*g,TTF_Font*f,const char*txt,SDL_Color col,int wrap_w,int*w,int*h){
  SDL_Surface*s=TTF_RenderUTF8_Blended_Wrapped(f,txt,col,wrap_w);if(!s)return NULL;
  SDL_Texture*t=SDL_CreateTextureFromSurface(g->ren,s);if(w)*w=s->w;if(h)*h=s->h;SDL_FreeSurface(s);return t;
}
static bool point_in(SDL_Rect r,int x,int y){return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
static GeometryMode geometry_mode(Mode mode){return(GeometryMode)mode;}
static bool get_geometry(const Gfx*g,const UI*ui,AppGeometry*out){return geometry_compute(g->width,g->height,geometry_mode(ui->mode),out);}
static SDL_Rect sdl_rect(GeoRect r){return(SDL_Rect){r.x,r.y,r.w,r.h};}
static TTF_Font*cached_font(Gfx*g,int requested){int best=0;for(int i=1;i<FONT_CACHE_COUNT;i++)if(abs(font_cache_sizes[i]-requested)<abs(font_cache_sizes[best]-requested))best=i;return g->fonts[best];}
static void update_font_tiers(Gfx*g,const UI*ui){AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts f=geometry_font_sizes(&a,g->width,g->height);g->font_big=cached_font(g,f.heading);g->font_small=cached_font(g,f.control);g->font_note=cached_font(g,f.note);g->font_cell=cached_font(g,f.cell);g->font_body=cached_font(g,f.body);}
static bool draw_fitted_text(Gfx*g,const char*text,SDL_Color color,SDL_Rect r,int preferred,int minimum){
  for(int i=FONT_CACHE_COUNT-1;i>=0;i--){
    if(font_cache_sizes[i]>preferred||font_cache_sizes[i]<minimum)continue;
    int w=0,h=0;
    if(TTF_SizeUTF8(g->fonts[i],text,&w,&h)==0&&w<=r.w-8&&h<=r.h-4){
      SDL_Texture*t=render_text(g,g->fonts[i],text,color,&w,&h);if(!t)return false;
      SDL_Rect d={r.x+(r.w-w)/2,r.y+(r.h-h)/2,w,h};SDL_RenderCopy(g->ren,t,NULL,&d);SDL_DestroyTexture(t);return true;
    }
  }
  return false;
}
static void draw_centered_text(Gfx*g,TTF_Font*f,const char*text,SDL_Color color,SDL_Rect r){(void)f;(void)draw_fitted_text(g,text,color,r,20,10);}

enum{
  HOME_FOCUS_LANGUAGE=0,HOME_FOCUS_MODE,HOME_FOCUS_DIFFICULTY,
  HOME_FOCUS_NEW,HOME_FOCUS_CONTINUE,HOME_FOCUS_DAILY,
  HOME_FOCUS_HELP,HOME_FOCUS_ABOUT,HOME_FOCUS_THEME,HOME_FOCUS_QUIT,
  HOME_FOCUS_COUNT
};
static int home_focus=HOME_FOCUS_NEW;
static GameDifficulty home_difficulty=DIFFICULTY_MEDIUM;
static SDL_Texture*language_flags[LANG_COUNT]={NULL,NULL,NULL};
static bool victory_animation_active=false;
static double victory_animation_t0=0.0;

static TextKey mode_key(Mode mode){return mode==MODE_CLASSIC?T_CLASSIC:mode==MODE_STRIKES?T_STRIKES:T_TIME_ATTACK;}
static TextKey difficulty_key(GameDifficulty difficulty){return difficulty==DIFFICULTY_EASY?T_EASY:difficulty==DIFFICULTY_HARD?T_HARD:T_MEDIUM;}
static const char*difficulty_name(Language language,GameDifficulty difficulty){return tr(language,difficulty_key(difficulty));}
static SDL_Color hint_text_color(const UI*ui){return ui->dark_theme?(SDL_Color){255,210,120,255}:(SDL_Color){155,95,0,255};}
static int game_hint_count(const Game*game){int count=0;if(!game)return 0;for(int i=0;i<81;i++)if(game->hinted[i])count++;return count;}
static bool palette_number_complete(const Game*game,int number){
  if(!game||number<1||number>9)return false;
  int correct=0;
  for(int i=0;i<81;i++)if(game->solution[i]==number&&game->puzzle[i]==number)correct++;
  return correct==9;
}
static void victory_animation_start(void){victory_animation_active=true;victory_animation_t0=now_s();}
static void victory_animation_reset(void){victory_animation_active=false;victory_animation_t0=0.0;}
static void draw_victory_animation(Gfx*g,Theme th){
  if(!victory_animation_active)return;
  double t=now_s()-victory_animation_t0;
  if(t>2.6){victory_animation_reset();return;}
  for(int i=0;i<22;i++){
    double speed=70.0+(i%5)*18.0;int x=(i*97+31)%g->width;
    int y=(int)fmod(t*speed+i*43.0,(double)(g->height+50))-25;
    int size=4+(i%3)*2;SDL_Color c=(i%2)?th.title:(SDL_Color){60,220,160,210};
    c.a=(Uint8)(210.0*(1.0-t/2.8));draw_rect(g->ren,x,y,size,size*2,c);
  }
}

static SDL_Rect language_rect(const Gfx*g,const UI*ui){
  AppGeometry a;if(!get_geometry(g,ui,&a))return(SDL_Rect){0,0,0,0};
  bool play=ui->screen==SCR_PLAY&&!(ui->pause_reasons&PAUSE_MANUAL);
  return sdl_rect(play?a.play_language:a.screen_language);
}
static SDL_Rect language_segment(const Gfx*g,const UI*ui,int index){SDL_Rect r=language_rect(g,ui);int x0=r.x+r.w*index/LANG_COUNT,x1=r.x+r.w*(index+1)/LANG_COUNT;return(SDL_Rect){x0,r.y,x1-x0,r.h};}
static void draw_language_selector(Gfx*g,const UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();int mx=0,my=0;SDL_GetMouseState(&mx,&my);
  for(int i=0;i<LANG_COUNT;i++){
    SDL_Rect outer=language_segment(g,ui,i),r={outer.x+2,outer.y+2,outer.w-4,outer.h-4};
    bool active=i==(int)ui->language,hover=point_in(outer,mx,my);
    SDL_Color bg=active?th.title:(hover?th.hover:th.btn);SDL_Color fg=active?th.bg:th.btnfg;
    draw_round_rect(g->ren,r,6,bg);
    int flag_h=r.h-12;if(flag_h>20)flag_h=20;if(flag_h<12)flag_h=12;int flag_w=flag_h*4/3;
    int left=r.x+7;SDL_Rect flag={left,r.y+(r.h-flag_h)/2,flag_w,flag_h};
    if(language_flags[i])SDL_RenderCopy(g->ren,language_flags[i],NULL,&flag);
    SDL_Rect text={flag.x+flag.w+5,r.y,r.x+r.w-(flag.x+flag.w+8),r.h};
    if(text.w<24)text=(SDL_Rect){r.x+4,r.y,r.w-8,r.h};
    (void)draw_fitted_text(g,language_name((Language)i),fg,text,16,10);
    if(ui->screen==SCR_TITLE&&home_focus==HOME_FOCUS_LANGUAGE&&active)draw_focus_ring(g->ren,r,th.sel_outline);
  }
}
static void draw_wordmark(Gfx*g,SDL_Rect box){
  if(!g->wordmark)return;
  double scale=(double)box.w/sudokura_wordmark_width;
  if(sudokura_wordmark_height*scale>box.h)scale=(double)box.h/sudokura_wordmark_height;
  SDL_Rect d={box.x+(box.w-(int)(sudokura_wordmark_width*scale))/2,box.y+(box.h-(int)(sudokura_wordmark_height*scale))/2,(int)(sudokura_wordmark_width*scale),(int)(sudokura_wordmark_height*scale)};
  SDL_RenderCopy(g->ren,g->wordmark,NULL,&d);
}

typedef struct{SDL_Rect board,side;AppGeometry model;}Layout;
typedef struct{SDL_Rect btn[9],pal[9],palette_label,progress;int count_btn,count_pal;}SidebarRects;
static Layout compute_layout(int W,int H,Mode mode){Layout l;memset(&l,0,sizeof l);if(geometry_compute(W,H,geometry_mode(mode),&l.model)){l.board=sdl_rect(l.model.board);l.side=sdl_rect(l.model.sidebar);}return l;}
static void compute_sidebar_rects(const Layout*l,SidebarRects*r){memset(r,0,sizeof*r);r->count_btn=r->count_pal=9;r->palette_label=sdl_rect(l->model.palette_label);r->progress=sdl_rect(l->model.progress);for(int i=0;i<9;i++){r->btn[i]=sdl_rect(l->model.actions[i]);r->pal[i]=sdl_rect(l->model.palette[i]);}}
static SDL_Rect back_rect(const Gfx*g,const UI*ui){AppGeometry a;return get_geometry(g,ui,&a)?sdl_rect(a.back_button):(SDL_Rect){0,0,0,0};}
static void draw_back_button(Gfx*g,const UI*ui,Theme th){SDL_Rect r=back_rect(g,ui);draw_round_rect(g->ren,r,7,th.btn);(void)draw_fitted_text(g,tr(ui->language,T_BACK),th.btnfg,r,20,10);}

static void render_pause_overlay(Gfx*g,const UI*ui);

/* =================== RENDER PLAY =================== */
static void render_board_and_sidebar(Gfx*g,const Game*game,UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);SDL_RenderClear(g->ren);
  Layout L=compute_layout(g->width,g->height,ui->mode);int gx=L.board.x,gy=L.board.y,side=L.board.w,cs=side/9;if(side<=0||cs<=0)return;
  draw_round_rect(g->ren,(SDL_Rect){gx-6,gy-6,side+12,side+12},10,th.shadow);
  draw_round_rect(g->ren,(SDL_Rect){L.side.x-6,L.side.y-6,L.side.w+12,L.side.h+12},10,th.shadow);
  draw_round_rect(g->ren,L.side,8,th.btn);draw_rect(g->ren,gx,gy,side,side,th.board);

  draw_rect(g->ren,gx,gy+ui->sel_r*cs,side,cs,th.boxhl);draw_rect(g->ren,gx+ui->sel_c*cs,gy,cs,side,th.boxhl);
  int br=(ui->sel_r/3)*3,bc=(ui->sel_c/3)*3;draw_rect(g->ren,gx+bc*cs,gy+br*cs,cs*3,cs*3,(SDL_Color){th.boxhl.r,th.boxhl.g,th.boxhl.b,(Uint8)(th.boxhl.a/2)});
  for(int i=0;i<=9;i++){int x=gx+(side*i)/9,y=gy+(side*i)/9;SDL_Color col=i%3==0?th.thick:th.thin;draw_line(g->ren,x,gy,x,gy+side,col);draw_line(g->ren,gx,y,gx+side,y,col);}

  int mx,my;SDL_GetMouseState(&mx,&my);int curV=game->puzzle[IDX(ui->sel_r,ui->sel_c)];
  for(int r=0;r<9;r++)for(int c=0;c<9;c++){
    int x=gx+c*cs,y=gy+r*cs,i=IDX(r,c);bool sel=ui->sel_r==r&&ui->sel_c==c;bool hover=mx>=x&&mx<x+cs&&my>=y&&my<y+cs;int v=game->puzzle[i];
    if(curV&&v==curV&&!sel)draw_rect(g->ren,x+2,y+2,cs-4,cs-4,th.samehl);
    if(sel){double t=now_s(),p=0.65+0.35*(0.5*(1.0+sin(t*4.0)));draw_rect(g->ren,x+2,y+2,cs-4,cs-4,(SDL_Color){(Uint8)(th.sel.r*p),(Uint8)(th.sel.g*p),(Uint8)(th.sel.b*p),190});SDL_Color o=th.sel_outline;draw_line(g->ren,x+1,y+1,x+cs-2,y+1,o);draw_line(g->ren,x+cs-2,y+1,x+cs-2,y+cs-2,o);draw_line(g->ren,x+cs-2,y+cs-2,x+1,y+cs-2,o);draw_line(g->ren,x+1,y+cs-2,x+1,y+1,o);}else if(hover)draw_rect(g->ren,x+2,y+2,cs-4,cs-4,th.hover);
    if(v&&game_has_conflict(game,r,c))draw_rect(g->ren,x+2,y+2,cs-4,cs-4,th.conflict);
    if(sel||ui->notes_mode){SDL_Color sgrid=(SDL_Color){th.thin.r,th.thin.g,th.thin.b,120};int sub=cs/3;for(int k=1;k<3;k++){draw_line(g->ren,x+k*sub,y+2,x+k*sub,y+cs-2,sgrid);draw_line(g->ren,x+2,y+k*sub,x+cs-2,y+k*sub,sgrid);}}
    if(v){
      bool wrong=!game->fixed[i]&&v!=game->solution[i];SDL_Color col=wrong?th.text_wrong:(game->hinted[i]?hint_text_color(ui):(game->fixed[i]?th.text_given:th.text_edit));
      if(game->hinted[i]){SDL_Color marker=hint_text_color(ui);draw_rect(g->ren,x+cs-7,y+3,4,4,marker);}
      char b[2]={(char)('0'+v),0};int tw,thh;SDL_Texture*t=render_text(g,g->font_cell,b,col,&tw,&thh);if(t){SDL_Rect d={x+(cs-tw)/2,y+(cs-thh)/2,tw,thh};SDL_RenderCopy(g->ren,t,NULL,&d);SDL_DestroyTexture(t);}
    }else if(game->notes[i]){
      for(int vv=1;vv<=9;vv++)if(game->notes[i]&(1u<<vv)){char b[2]={(char)('0'+vv),0};int tw,thh;SDL_Texture*t=render_text(g,g->font_note,b,th.dim,&tw,&thh);if(t){int sub=cs/3,q=(vv-1)/3,qq=(vv-1)%3;SDL_Rect d={x+qq*sub+(sub-tw)/2,y+q*sub+(sub-thh)/2,tw,thh};SDL_RenderCopy(g->ren,t,NULL,&d);SDL_DestroyTexture(t);}}
    }
  }

  SidebarRects R;compute_sidebar_rects(&L,&R);int tw,thh;SDL_Rect title_rect=sdl_rect(L.model.play_title);draw_wordmark(g,title_rect);
  char hud[GEOMETRY_HUD_COUNT][80]={{0}};double el=elapsed_time(ui);const char*mode_name=tr(ui->language,mode_key(ui->mode));
  if(ui->is_daily)snprintf(hud[0],sizeof hud[0],"%s",tr(ui->language,T_DAILY_SHORT));else snprintf(hud[0],sizeof hud[0],"%s: %s",tr(ui->language,T_MODE),mode_name);
  snprintf(hud[1],sizeof hud[1],"%s: %s",tr(ui->language,T_DIFFICULTY),difficulty_name(ui->language,game->difficulty));
  snprintf(hud[2],sizeof hud[2],"%s: %02d:%02d",tr(ui->language,T_TIME),(int)(el/60),(int)fmod(el,60));
  snprintf(hud[3],sizeof hud[3],"%s: %d",tr(ui->language,T_ERRORS),ui->mistakes);
  snprintf(hud[4],sizeof hud[4],"%s",tr(ui->language,ui->strict_mode?T_STRICT:T_FREE));
  if(ui->mode==MODE_TIME){int rem=(int)(ui->time_limit_s-el);if(rem<0)rem=0;snprintf(hud[5],sizeof hud[5],"%s: %02d:%02d",tr(ui->language,T_TARGET),rem/60,rem%60);}else if(ui->mode==MODE_STRIKES){int left=ui->strikes_max-ui->strikes;if(left<0)left=0;snprintf(hud[5],sizeof hud[5],"%s: %d/%d",tr(ui->language,T_STRIKES_LEFT),left,ui->strikes_max);}
  SDL_GetMouseState(&mx,&my);
  for(int i=0;i<L.model.hud_count;i++){SDL_Rect hr=sdl_rect(L.model.hud[i]);bool strict_chip=i==4,hover=strict_chip&&point_in(hr,mx,my);SDL_Color bg=strict_chip&&ui->strict_mode?th.title:(hover?th.hover:th.palette_bg);SDL_Color fg=strict_chip&&ui->strict_mode?th.bg:th.dim;draw_round_rect(g->ren,hr,hr.h/2,bg);(void)draw_fitted_text(g,hud[i],fg,hr,16,10);}

  const char*labels[]={tr(ui->language,T_RESTART),tr(ui->language,T_MODE),tr(ui->language,T_HINT),tr(ui->language,T_NOTES),tr(ui->language,T_VERIFY),tr(ui->language,T_THEME),tr(ui->language,T_HELP),tr(ui->language,T_ABOUT),tr(ui->language,T_MENU)};
  for(int i=0;i<R.count_btn;i++){SDL_Rect rc=R.btn[i];Uint32 mouse=SDL_GetMouseState(&mx,&my);bool hover=point_in(rc,mx,my),pressed=hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT)),active=i==3&&ui->notes_mode;SDL_Color color=active?th.title:pressed?th.sel:hover?th.hover:(i==0?th.title:th.btn);draw_round_rect(g->ren,rc,7,color);(void)draw_fitted_text(g,labels[i],active?th.bg:th.btnfg,rc,20,10);}

  (void)draw_fitted_text(g,tr(ui->language,T_PALETTE),th.dim,R.palette_label,16,10);
  for(int n=1;n<=9;n++){
    SDL_Rect rc=R.pal[n-1];Uint32 mouse=SDL_GetMouseState(&mx,&my);bool hover=point_in(rc,mx,my),complete=palette_number_complete(game,n),selected=game->puzzle[IDX(ui->sel_r,ui->sel_c)]==n;
    SDL_Color color=complete?th.btn:(selected?th.title:(hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT)))?th.sel:hover?th.hover:th.palette_bg);SDL_Color fg=complete?th.dim:th.palette_fg;
    draw_round_rect(g->ren,rc,7,color);char nb[2]={(char)('0'+n),0};(void)draw_fitted_text(g,nb,fg,rc,24,12);
    if(complete){int x1=rc.x+rc.w-16,y1=rc.y+rc.h-11;draw_line(g->ren,x1,y1,x1+4,y1+4,th.dim);draw_line(g->ren,x1+4,y1+4,x1+10,y1-4,th.dim);}
  }

  int progress=game_progress_percent(game);draw_round_rect(g->ren,R.progress,R.progress.h/2,th.palette_bg);int inner_w=R.progress.w-4;if(inner_w<0)inner_w=0;int filled=inner_w*progress/100;if(filled>0)draw_round_rect(g->ren,(SDL_Rect){R.progress.x+2,R.progress.y+2,filled,R.progress.h-4},(R.progress.h-4)/2,th.title);
  char progress_text[64];snprintf(progress_text,sizeof progress_text,"%s %d%%",tr(ui->language,T_PROGRESS),progress);(void)draw_fitted_text(g,progress_text,progress>52?th.bg:th.dim,R.progress,16,10);

  if(ui->toast_on){double dt=now_s()-ui->toast_t0;if(dt>2.2)ui->toast_on=false;if(ui->toast_on){SDL_Texture*tt=render_text(g,g->font_small,ui->toast,(SDL_Color){255,255,255,230},&tw,&thh);if(tt){SDL_Rect d={gx+(side-tw)/2,gy-(thh+8),tw,thh};draw_rect(g->ren,d.x-8,d.y-6,tw+16,thh+12,(SDL_Color){0,0,0,110});SDL_RenderCopy(g->ren,tt,NULL,&d);SDL_DestroyTexture(tt);}}}
  if(ui->pause_reasons&PAUSE_MANUAL)render_pause_overlay(g,ui);
}
