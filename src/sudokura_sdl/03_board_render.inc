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

static void render_pause_overlay(Gfx*g,const UI*ui);

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
  const char* labels[]={tr(ui->language,T_RESTART),tr(ui->language,T_MODE),tr(ui->language,T_HINT),tr(ui->language,T_NOTES),tr(ui->language,T_VERIFY),tr(ui->language,T_THEME),tr(ui->language,T_HELP),tr(ui->language,T_ABOUT),tr(ui->language,T_MENU)};
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
  snprintf(buf,sizeof(buf),tr(ui->language,T_PROGRESS),game_progress_percent(game),ui->mistakes);
  (void)sw;(void)draw_fitted_text(g,buf,th.dim,R.progress,16,10);

  /* toast */
  if(ui->toast_on){
    double dt=now_s()-ui->toast_t0; if(dt>2.2) ui->toast_on=false;
    if(ui->toast_on){
      SDL_Texture* tt2=render_text(g,g->font_small,ui->toast,(SDL_Color){255,255,255,230},&tw,&thh);
      if(tt2){ SDL_Rect d={ gx + (side-tw)/2, gy- (thh+8), tw, thh }; draw_rect(g->ren,d.x-8,d.y-6,tw+16,thh+12,(SDL_Color){0,0,0,110}); SDL_RenderCopy(g->ren,tt2,NULL,&d); SDL_DestroyTexture(tt2); }
    }
  }
  if(ui->pause_reasons&PAUSE_MANUAL)render_pause_overlay(g,ui);
}

