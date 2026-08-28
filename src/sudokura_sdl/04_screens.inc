static void render_title(Gfx*g, UI*ui){
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r,th.bg.g,th.bg.b,255);SDL_RenderClear(g->ren);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;
  SDL_Rect heading=sdl_rect(a.title_heading);draw_wordmark(g,heading);
  const char*mode_name=tr(ui->language,ui->mode==MODE_CLASSIC?T_CLASSIC:ui->mode==MODE_STRIKES?T_STRIKES:T_TIME_ATTACK);
  char mode_line[64];snprintf(mode_line,sizeof(mode_line),"%s: %s",tr(ui->language,T_MODE),mode_name);
  const TextKey keys[GEOMETRY_TITLE_BUTTON_COUNT]={T_MODE,T_NEW_GAME,T_CONTINUE,T_DAILY,T_HELP,T_ABOUT,T_QUIT};
  for(int i=0;i<GEOMETRY_TITLE_BUTTON_COUNT;i++){
    SDL_Rect r=sdl_rect(a.title_buttons[i]);bool primary=i==1;bool disabled=i==2&&!ui->has_session;
    SDL_Color bg=primary?th.title:th.btn;SDL_Color fg=primary?th.bg:(disabled?th.dim:th.btnfg);
    draw_round_rect(g->ren,r,7,bg);
    const char*label=i==0?mode_line:tr(ui->language,keys[i]);
    (void)draw_fitted_text(g,label,fg,r,primary?22:20,10);
  }
}

static void render_pause_overlay(Gfx*g,const UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();
  draw_rect(g->ren,0,0,g->width,g->height,th.bg);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;
  SDL_Rect heading=sdl_rect(a.pause_heading),button=sdl_rect(a.pause_button);
  (void)draw_fitted_text(g,tr(ui->language,T_PAUSED),th.title,heading,36,16);
  draw_round_rect(g->ren,button,7,th.title);
  (void)draw_fitted_text(g,tr(ui->language,T_RESUME),th.bg,button,22,10);
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

