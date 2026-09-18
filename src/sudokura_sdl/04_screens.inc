static SDL_Rect pressed_paint_rect(SDL_Rect r,bool pressed){if(pressed&&r.h>4){r.y+=2;r.h-=2;}return r;}

static void render_pause_overlay(Gfx*g,const UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();draw_rect(g->ren,0,0,g->width,g->height,th.bg);AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts fonts=geometry_font_sizes(&a,g->width,g->height);
  draw_wordmark(g,sdl_rect(a.pause_logo));(void)draw_fitted_text(g,tr(ui->language,T_PAUSED),th.title,sdl_rect(a.pause_heading),fonts.heading,16);
  int mx=0,my=0;Uint32 mouse=SDL_GetMouseState(&mx,&my);SDL_Rect resume=sdl_rect(a.pause_buttons[0]),menu=sdl_rect(a.pause_buttons[1]);bool resume_pressed=point_in(resume,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT)),menu_pressed=point_in(menu,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Rect resume_paint=pressed_paint_rect(resume,resume_pressed),menu_paint=pressed_paint_rect(menu,menu_pressed);draw_round_rect(g->ren,resume_paint,7,resume_pressed?th.sel:th.title);(void)draw_fitted_text(g,tr(ui->language,T_RESUME),th.bg,resume_paint,fonts.control+2,10);draw_round_rect(g->ren,menu_paint,7,menu_pressed?th.sel:th.btn);(void)draw_fitted_text(g,tr(ui->language,T_MENU),th.btnfg,menu_paint,fonts.control,10);draw_focus_ring(g->ren,pause_keyboard_focus==0?resume:menu,th.sel_outline);
}

static void blit_wrapped(Gfx*g,int x,int y,int w,int max_h,const char*text,SDL_Color col){int tw,thh;SDL_Texture*t=render_text_wrapped(g,g->font_body,text,col,w,&tw,&thh);if(t){int visible=thh<max_h?thh:max_h;SDL_Rect source={0,0,tw,visible},d={x,y,tw,visible};SDL_RenderCopy(g->ren,t,&source,&d);SDL_DestroyTexture(t);}}
static void render_help(Gfx*g,UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);
  SDL_RenderClear(g->ren);
  AppGeometry a;
  if(!get_geometry(g,ui,&a))return;
  SDL_Rect heading=sdl_rect(a.info_heading),body=sdl_rect(a.info_body);
  int tw,thh;
  SDL_Texture*T=render_text(g,g->font_big,tr(ui->language,T_HELP),th.title,&tw,&thh);
  if(T){
    SDL_Rect d={heading.x,heading.y,tw,thh};
    SDL_RenderCopy(g->ren,T,NULL,&d);
    SDL_DestroyTexture(T);
  }
  char help_text[2048];
  const char*modifier=input_primary_modifier_label();
  snprintf(help_text,sizeof help_text,tr(ui->language,T_HELP_BODY),
           modifier,modifier,modifier,modifier);
  blit_wrapped(g,body.x,body.y,body.w,body.h,help_text,th.dim);
  draw_back_button(g,ui,th,info_keyboard_focus==INFO_FOCUS_BACK);
}

static void render_end(Gfx*g,const Game*game,UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);
  SDL_RenderClear(g->ren);
  if(ui->result==RES_WIN)draw_victory_animation(g,th);
  AppGeometry a;
  if(!get_geometry(g,ui,&a))return;
  GeometryFonts fonts=geometry_font_sizes(&a,g->width,g->height);
  draw_wordmark(g,sdl_rect(a.end_logo));
  SDL_Rect heading=sdl_rect(a.end_heading),summary=sdl_rect(a.end_summary);
  const char*title=tr(ui->language,ui->result==RES_WIN?T_YOU_WIN:T_YOU_LOSE);
  SDL_Color title_color=ui->result==RES_WIN
                            ?(SDL_Color){60,220,160,255}
                            :(SDL_Color){220,80,80,255};
  (void)draw_fitted_text(g,title,title_color,heading,fonts.heading,16);

  const char*mode_name=ui->is_daily
                            ?tr(ui->language,T_DAILY_SHORT)
                            :tr(ui->language,mode_key(ui->mode));
  const char*diff=difficulty_name(ui->language,game->difficulty);
  double el=elapsed_time(ui);
  int minutes=(int)(el/60);
  int seconds=(int)fmod(el,60);
  char lines[4][160];
  snprintf(lines[0],sizeof lines[0],"%s · %s",mode_name,diff);
  if(ui->mode==MODE_STRIKES){
    snprintf(lines[1],sizeof lines[1],"%s %02d:%02d · %s %d/%d",
             tr(ui->language,T_TIME),minutes,seconds,
             tr(ui->language,T_STRIKES),ui->strikes,ui->strikes_max);
  }else if(ui->mode==MODE_TIME){
    snprintf(lines[1],sizeof lines[1],"%s %02d:%02d · %s %d",
             tr(ui->language,T_TIME),minutes,seconds,
             tr(ui->language,T_ERRORS),ui->mistakes);
  }else{
    snprintf(lines[1],sizeof lines[1],"%s %02d:%02d",
             tr(ui->language,T_TIME),minutes,seconds);
  }
  if(ui->assisted)
    snprintf(lines[2],sizeof lines[2],"%s: %d · %s",
             tr(ui->language,T_HINTS),game_hint_count(game),
             tr(ui->language,T_ASSISTED));
  else
    snprintf(lines[2],sizeof lines[2],"%s: %d",
             tr(ui->language,T_HINTS),game_hint_count(game));

  if(ui->result_best_time_available){
    unsigned long long best_s=
        (unsigned long long)(ui->result_best_time_ms/1000u);
    snprintf(lines[3],sizeof lines[3],"%s: %u · %s %02llu:%02llu",
             tr(ui->language,T_COMPLETED),ui->result_finished_count,
             tr(ui->language,T_BEST),best_s/60u,best_s%60u);
  }else{
    snprintf(lines[3],sizeof lines[3],"%s: %u",
             tr(ui->language,T_COMPLETED),ui->result_finished_count);
  }

  int row_h=summary.h/4;
  for(int i=0;i<4;i++){
    SDL_Rect row={summary.x,summary.y+i*row_h,summary.w,
                  i==3?summary.h-i*row_h:row_h};
    (void)draw_fitted_text(g,lines[i],i==0?th.btnfg:th.dim,row,fonts.body,9);
  }

  int mx=0,my=0;
  Uint32 mouse=SDL_GetMouseState(&mx,&my);
  SDL_Rect b1=sdl_rect(a.end_buttons[0]),b2=sdl_rect(a.end_buttons[1]);
  bool p1=point_in(b1,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));
  bool p2=point_in(b2,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Rect b1p=pressed_paint_rect(b1,p1),b2p=pressed_paint_rect(b2,p2);
  draw_round_rect(g->ren,b1p,7,p1?th.sel:th.title);
  TextKey primary=ui->result==RES_WIN
                      ?(ui->is_daily?T_ANOTHER_NORMAL:T_ANOTHER_GAME)
                      :T_RETRY_SAME;
  (void)draw_fitted_text(g,tr(ui->language,primary),th.bg,b1p,
                         fonts.control+2,9);
  draw_round_rect(g->ren,b2p,7,p2?th.sel:th.btn);
  (void)draw_fitted_text(g,tr(ui->language,T_HOME),th.btnfg,b2p,
                         fonts.control,9);
  draw_focus_ring(g->ren,result_keyboard_focus==0?b1:b2,th.sel_outline);
}

static bool confirm_box(SDL_Window*win,const char*title,const char*msg,const char*ok_label,const char*cancel_label){
  const SDL_MessageBoxButtonData buttons[]={{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,cancel_label},{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,1,ok_label?ok_label:"OK"}};
  const SDL_MessageBoxColorScheme scheme={{{25,25,25},{200,200,200},{50,50,50},{60,120,220},{230,230,230}}};
  const SDL_MessageBoxData data={SDL_MESSAGEBOX_INFORMATION,win,title,msg,2,buttons,&scheme};int buttonid=-1;if(SDL_ShowMessageBox(&data,&buttonid)<0)return false;return buttonid==1;
}

static int choice_box(SDL_Window*win,const char*title,const char*msg,
                      const char*back_label,const char*retry_label,
                      const char*discard_label){
  const SDL_MessageBoxButtonData buttons[]={
    {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,back_label},
    {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,1,retry_label},
    {0,2,discard_label}
  };
  const SDL_MessageBoxColorScheme scheme={{{25,25,25},{200,200,200},{50,50,50},{60,120,220},{230,230,230}}};
  const SDL_MessageBoxData data={SDL_MESSAGEBOX_WARNING,win,title,msg,3,buttons,&scheme};
  int buttonid=0;
  if(SDL_ShowMessageBox(&data,&buttonid)<0)return 0;
  return buttonid;
}

static void app_render_base(Gfx*g,const Game*game,UI*ui){
  update_font_tiers(g,ui);
  if(ui->screen==SCR_TITLE){
    Theme th=ui->dark_theme?theme_dark():theme_light();
    SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);
    SDL_RenderClear(g->ren);
  }else if(ui->screen==SCR_HELP){
    render_help(g,ui);
  }else if(ui->screen==SCR_ABOUT||ui->screen==APP_SCREEN_SETTINGS){
    Theme th=ui->dark_theme?theme_dark():theme_light();
    SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);
    SDL_RenderClear(g->ren);
  }else if(ui->screen==SCR_END){
    render_end(g,game,ui);
  }else{
    render_board_and_sidebar(g,game,ui);
  }
  if(ui->screen==SCR_TITLE)draw_language_selector(g,ui);
}
static bool save_frame(Gfx*g,const char*path){SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,g->width,g->height,32,SDL_PIXELFORMAT_ARGB8888);if(!s)return false;bool ok=SDL_RenderReadPixels(g->ren,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)==0&&SDL_SaveBMP(s,path)==0;SDL_FreeSurface(s);return ok;}

static void app_shutdown_base(Gfx*g){
  if(!g)return;
  if(g->wordmark)SDL_DestroyTexture(g->wordmark);
  for(int i=0;i<LANG_COUNT;i++){
    if(language_flags[i])SDL_DestroyTexture(language_flags[i]);
    language_flags[i]=NULL;
  }
  for(int i=0;i<FONT_CACHE_COUNT;i++)if(g->fonts[i])TTF_CloseFont(g->fonts[i]);
  if(g->ren)SDL_DestroyRenderer(g->ren);
  if(g->win)SDL_DestroyWindow(g->win);
  memset(g,0,sizeof(*g));
  if(TTF_WasInit())TTF_Quit();
  SDL_Quit();
}
static SDL_Texture*texture_from_rgba(SDL_Renderer*renderer,const unsigned char*rgba,unsigned int width,unsigned int height){SDL_Surface*s=SDL_CreateRGBSurfaceWithFormatFrom((void*)rgba,(int)width,(int)height,32,(int)width*4,SDL_PIXELFORMAT_RGBA32);if(!s)return NULL;SDL_Texture*t=SDL_CreateTextureFromSurface(renderer,s);SDL_FreeSurface(s);return t;}
static bool app_init_base(Gfx*g,const char*font_cli){
  memset(g,0,sizeof(*g));g->width=1024;g->height=720;SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return false;}if(TTF_Init()!=0){fprintf(stderr,"TTF_Init: %s\n",TTF_GetError());app_shutdown_base(g);return false;}
  g->win=SDL_CreateWindow(SUDOKURA_NAME_VERSION,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,g->width,g->height,SDL_WINDOW_RESIZABLE);if(!g->win){fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());app_shutdown_base(g);return false;}SDL_SetWindowMinimumSize(g->win,360,480);g->ren=SDL_CreateRenderer(g->win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!g->ren)g->ren=SDL_CreateRenderer(g->win,-1,SDL_RENDERER_SOFTWARE);if(!g->ren){fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());app_shutdown_base(g);return false;}
  SDL_Surface*icon=SDL_CreateRGBSurfaceWithFormatFrom((void*)sudokura_icon_rgba,(int)sudokura_icon_width,(int)sudokura_icon_height,32,(int)sudokura_icon_width*4,SDL_PIXELFORMAT_RGBA32);if(icon){SDL_SetWindowIcon(g->win,icon);SDL_FreeSurface(icon);}SDL_Surface*word=SDL_CreateRGBSurfaceWithFormatFrom((void*)sudokura_wordmark_rgba,(int)sudokura_wordmark_width,(int)sudokura_wordmark_height,32,(int)sudokura_wordmark_width*4,SDL_PIXELFORMAT_RGBA32);if(word){g->wordmark=SDL_CreateTextureFromSurface(g->ren,word);SDL_FreeSurface(word);}if(!g->wordmark){fprintf(stderr,"wordmark texture: %s\n",SDL_GetError());app_shutdown_base(g);return false;}
  language_flags[LANG_EN]=texture_from_rgba(g->ren,sudokura_flag_us_rgba,sudokura_flag_us_width,sudokura_flag_us_height);language_flags[LANG_ES]=texture_from_rgba(g->ren,sudokura_flag_ar_rgba,sudokura_flag_ar_width,sudokura_flag_ar_height);language_flags[LANG_CA]=texture_from_rgba(g->ren,sudokura_flag_ca_rgba,sudokura_flag_ca_width,sudokura_flag_ca_height);
  char font_path[PATH_MAX]={0};const char*fpath=find_font_path_dynamic(font_path,font_cli);if(!fpath){fprintf(stderr,"%s\n",tr(LANG_EN,T_FONT_ERROR));app_shutdown_base(g);return false;}for(int i=0;i<FONT_CACHE_COUNT;i++){g->fonts[i]=TTF_OpenFont(fpath,font_cache_sizes[i]);if(!g->fonts[i]){fprintf(stderr,"font %d: %s\n",font_cache_sizes[i],TTF_GetError());app_shutdown_base(g);return false;}}
  g->font_big=cached_font(g,44);g->font_small=cached_font(g,20);g->font_note=cached_font(g,12);g->font_cell=cached_font(g,28);g->font_body=cached_font(g,18);return true;
}

static bool make_directory(const char*path){char part[PATH_MAX];size_t length=strlen(path);if(length==0||length>=sizeof(part))return false;if(!path_copy(part,sizeof(part),path))return false;for(size_t i=1;i<=length;i++)if(part[i]=='/'||part[i]=='\\'||part[i]=='\0'){char saved=part[i];part[i]='\0';if(part[0]){
#if defined(_WIN32)
  if(_mkdir(part)!=0&&errno!=EEXIST)return false;
#else
  if(mkdir(part,0755)!=0&&errno!=EEXIST)return false;
#endif
}part[i]=saved;}return true;}
static void apply_window_resize(Gfx*g,int requested_width,int requested_height){int width=0,height=0;bool changed=geometry_normalize_window_size(requested_width,requested_height,&width,&height);if(changed)SDL_SetWindowSize(g->win,width,height);SDL_GetWindowSize(g->win,&g->width,&g->height);}
