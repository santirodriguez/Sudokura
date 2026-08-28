static SDL_Rect pressed_paint_rect(SDL_Rect r,bool pressed){if(pressed&&r.h>4){r.y+=2;r.h-=2;}return r;}

static void draw_home_button(Gfx*g,Theme th,SDL_Rect r,const char*label,bool primary,bool disabled,bool focused,int preferred){
  int mx=0,my=0;Uint32 mouse=SDL_GetMouseState(&mx,&my);bool hover=!disabled&&point_in(r,mx,my),pressed=hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Color bg=primary?th.title:(pressed?th.sel:hover?th.hover:th.btn);SDL_Color fg=primary?th.bg:(disabled?th.dim:th.btnfg);
  if(disabled)bg=th.palette_bg;
  SDL_Rect paint=pressed_paint_rect(r,pressed&&!disabled);draw_round_rect(g->ren,paint,7,bg);
  (void)draw_fitted_text(g,label,fg,paint,preferred,10);
  if(focused&&!disabled)draw_focus_ring(g->ren,r,th.sel_outline);
}

static void render_title(Gfx*g,UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);SDL_RenderClear(g->ren);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts fonts=geometry_font_sizes(&a,g->width,g->height);draw_wordmark(g,sdl_rect(a.home_logo));
  (void)draw_fitted_text(g,tr(ui->language,T_MODE),th.dim,sdl_rect(a.home_mode_label),fonts.help,10);
  int mx=0,my=0;Uint32 mouse=SDL_GetMouseState(&mx,&my);
  for(int i=0;i<GEOMETRY_HOME_SEGMENT_COUNT;i++){
    SDL_Rect r=sdl_rect(a.home_mode[i]);bool active=i==(int)ui->mode,hover=point_in(r,mx,my),pressed=hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Color bg=active?th.title:(pressed?th.sel:hover?th.hover:th.btn);SDL_Color fg=active?th.bg:th.btnfg;SDL_Rect paint=pressed_paint_rect(r,pressed);
    draw_round_rect(g->ren,paint,7,bg);(void)draw_fitted_text(g,tr(ui->language,mode_key((Mode)i)),fg,paint,fonts.control,10);
    if(home_focus==HOME_FOCUS_MODE&&active)draw_focus_ring(g->ren,r,th.sel_outline);
  }
  (void)draw_fitted_text(g,tr(ui->language,T_DIFFICULTY),th.dim,sdl_rect(a.home_difficulty_label),fonts.help,10);
  for(int i=0;i<GEOMETRY_HOME_SEGMENT_COUNT;i++){
    SDL_Rect r=sdl_rect(a.home_difficulty[i]);bool active=i==(int)home_difficulty,hover=point_in(r,mx,my),pressed=hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Color bg=active?th.title:(pressed?th.sel:hover?th.hover:th.btn);SDL_Color fg=active?th.bg:th.btnfg;SDL_Rect paint=pressed_paint_rect(r,pressed);
    draw_round_rect(g->ren,paint,7,bg);(void)draw_fitted_text(g,difficulty_name(ui->language,(GameDifficulty)i),fg,paint,fonts.control,10);
    if(home_focus==HOME_FOCUS_DIFFICULTY&&active)draw_focus_ring(g->ren,r,th.sel_outline);
  }

  draw_home_button(g,th,sdl_rect(a.home_primary[0]),tr(ui->language,T_NEW_GAME),true,false,home_focus==HOME_FOCUS_NEW,fonts.control);
  draw_home_button(g,th,sdl_rect(a.home_primary[1]),tr(ui->language,T_CONTINUE),false,!ui->has_session,home_focus==HOME_FOCUS_CONTINUE,fonts.control);
  SDL_Rect daily=sdl_rect(a.home_primary[2]);bool daily_focus=home_focus==HOME_FOCUS_DAILY;bool daily_hover=point_in(daily,mx,my),daily_pressed=daily_hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Rect daily_paint=pressed_paint_rect(daily,daily_pressed);draw_round_rect(g->ren,daily_paint,7,daily_pressed?th.sel:daily_hover?th.hover:th.btn);
  SDL_Rect daily_main={daily_paint.x,daily_paint.y+2,daily_paint.w,daily_paint.h*3/5};SDL_Rect daily_sub={daily_paint.x,daily_paint.y+daily_paint.h*3/5-2,daily_paint.w,daily_paint.h*2/5};
  (void)draw_fitted_text(g,tr(ui->language,T_DAILY),th.btnfg,daily_main,fonts.control,10);char daily_detail[64];snprintf(daily_detail,sizeof daily_detail,"%s · %s",tr(ui->language,T_CLASSIC),tr(ui->language,T_MEDIUM));int sub_pref=fonts.help>12?fonts.help-4:12;(void)draw_fitted_text(g,daily_detail,th.dim,daily_sub,sub_pref,10);if(daily_focus)draw_focus_ring(g->ren,daily,th.sel_outline);

  const TextKey secondary_keys[GEOMETRY_HOME_SECONDARY_COUNT]={T_HELP,T_ABOUT,T_THEME,T_QUIT};
  for(int i=0;i<GEOMETRY_HOME_SECONDARY_COUNT;i++){
    char label[64];const char*text=tr(ui->language,secondary_keys[i]);
    if(i==2){snprintf(label,sizeof label,"%s: %s",tr(ui->language,T_THEME),tr(ui->language,ui->dark_theme?T_DARK:T_LIGHT));text=label;}
    int focus=HOME_FOCUS_HELP+i;draw_home_button(g,th,sdl_rect(a.home_secondary[i]),text,false,false,home_focus==focus,fonts.control);
  }
}

static void render_pause_overlay(Gfx*g,const UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();draw_rect(g->ren,0,0,g->width,g->height,th.bg);AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts fonts=geometry_font_sizes(&a,g->width,g->height);
  draw_wordmark(g,sdl_rect(a.pause_logo));(void)draw_fitted_text(g,tr(ui->language,T_PAUSED),th.title,sdl_rect(a.pause_heading),fonts.heading,16);
  int mx=0,my=0;Uint32 mouse=SDL_GetMouseState(&mx,&my);SDL_Rect resume=sdl_rect(a.pause_buttons[0]),menu=sdl_rect(a.pause_buttons[1]);bool resume_pressed=point_in(resume,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT)),menu_pressed=point_in(menu,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Rect resume_paint=pressed_paint_rect(resume,resume_pressed),menu_paint=pressed_paint_rect(menu,menu_pressed);draw_round_rect(g->ren,resume_paint,7,resume_pressed?th.sel:th.title);(void)draw_fitted_text(g,tr(ui->language,T_RESUME),th.bg,resume_paint,fonts.control+2,10);draw_round_rect(g->ren,menu_paint,7,menu_pressed?th.sel:th.btn);(void)draw_fitted_text(g,tr(ui->language,T_MENU),th.btnfg,menu_paint,fonts.control,10);
}

static void blit_wrapped(Gfx*g,int x,int y,int w,int max_h,const char*text,SDL_Color col){int tw,thh;SDL_Texture*t=render_text_wrapped(g,g->font_body,text,col,w,&tw,&thh);if(t){int visible=thh<max_h?thh:max_h;SDL_Rect source={0,0,tw,visible},d={x,y,tw,visible};SDL_RenderCopy(g->ren,t,&source,&d);SDL_DestroyTexture(t);}}
static void render_help(Gfx*g,UI*ui){Theme th=ui->dark_theme?theme_dark():theme_light();SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);SDL_RenderClear(g->ren);AppGeometry a;if(!get_geometry(g,ui,&a))return;SDL_Rect heading=sdl_rect(a.info_heading),body=sdl_rect(a.info_body);int tw,thh;SDL_Texture*T=render_text(g,g->font_big,tr(ui->language,T_HELP),th.title,&tw,&thh);if(T){SDL_Rect d={heading.x,heading.y,tw,thh};SDL_RenderCopy(g->ren,T,NULL,&d);SDL_DestroyTexture(T);}blit_wrapped(g,body.x,body.y,body.w,body.h,tr(ui->language,T_HELP_BODY),th.dim);draw_back_button(g,ui,th);}

static void render_about(Gfx*g,UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);SDL_RenderClear(g->ren);AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts fonts=geometry_font_sizes(&a,g->width,g->height);
  SDL_Rect logo=sdl_rect(a.about_logo),body=sdl_rect(a.about_body),meta=sdl_rect(a.about_meta);SDL_Rect last=sdl_rect(a.about_links[GEOMETRY_ABOUT_LINK_COUNT-1]);SDL_Rect card={logo.x-12,logo.y-8,logo.w+24,last.y+last.h-logo.y+16};draw_round_rect(g->ren,card,12,th.palette_bg);draw_wordmark(g,logo);
  int version_h=fonts.body+10;if(version_h>body.h/3)version_h=body.h/3;SDL_Rect version_row={body.x,body.y,body.w,version_h};(void)draw_fitted_text(g,SUDOKURA_NAME_VERSION,th.title,version_row,fonts.body,12);SDL_Rect copy={body.x,body.y+version_h+4,body.w,body.h-version_h-4};if(copy.h>0)blit_wrapped(g,copy.x,copy.y,copy.w,copy.h,tr(ui->language,T_ABOUT_BODY),th.btnfg);
  const TextKey meta_keys[3]={T_ABOUT_SEEDS,T_ABOUT_STACK,T_ABOUT_MUSIC};int row_h=meta.h/3;for(int i=0;i<3;i++){SDL_Rect row={meta.x,meta.y+i*row_h,meta.w,row_h};(void)draw_fitted_text(g,tr(ui->language,meta_keys[i]),i==0?th.title:th.dim,row,fonts.help,10);}
  const TextKey link_keys[GEOMETRY_ABOUT_LINK_COUNT]={T_GITHUB,T_REPOSITORY,T_WEBSITE};int mx=0,my=0;Uint32 mouse=SDL_GetMouseState(&mx,&my);for(int i=0;i<GEOMETRY_ABOUT_LINK_COUNT;i++){SDL_Rect r=sdl_rect(a.about_links[i]);bool hover=point_in(r,mx,my),pressed=hover&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Rect paint=pressed_paint_rect(r,pressed);draw_round_rect(g->ren,paint,7,pressed?th.sel:hover?th.hover:th.btn);char label[64];snprintf(label,sizeof label,"%d · %s",i+1,tr(ui->language,link_keys[i]));(void)draw_fitted_text(g,label,th.btnfg,paint,fonts.control,10);}
  draw_back_button(g,ui,th);
}

static void render_end(Gfx*g,const Game*game,UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);SDL_RenderClear(g->ren);if(ui->result==RES_WIN)draw_victory_animation(g,th);
  AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts fonts=geometry_font_sizes(&a,g->width,g->height);draw_wordmark(g,sdl_rect(a.end_logo));SDL_Rect heading=sdl_rect(a.end_heading),summary=sdl_rect(a.end_summary);
  const char*title=tr(ui->language,ui->result==RES_WIN?T_YOU_WIN:T_YOU_LOSE);SDL_Color title_color=ui->result==RES_WIN?(SDL_Color){60,220,160,255}:(SDL_Color){220,80,80,255};(void)draw_fitted_text(g,title,title_color,heading,fonts.heading,16);
  const char*mode_name=ui->is_daily?tr(ui->language,T_DAILY_SHORT):tr(ui->language,mode_key(ui->mode));const char*diff=difficulty_name(ui->language,game->difficulty);double el=elapsed_time(ui);char lines[3][128];
  snprintf(lines[0],sizeof lines[0],"%s · %s",mode_name,diff);snprintf(lines[1],sizeof lines[1],"%s %02d:%02d   ·   %s %d",tr(ui->language,T_TIME),(int)(el/60),(int)fmod(el,60),tr(ui->language,T_ERRORS),ui->mistakes);snprintf(lines[2],sizeof lines[2],"%s: %d",tr(ui->language,T_HINTS),game_hint_count(game));
  int row_h=summary.h/3;for(int i=0;i<3;i++){SDL_Rect row={summary.x,summary.y+i*row_h,summary.w,row_h};(void)draw_fitted_text(g,lines[i],i==0?th.btnfg:th.dim,row,fonts.body,10);}
  int mx=0,my=0;Uint32 mouse=SDL_GetMouseState(&mx,&my);SDL_Rect b1=sdl_rect(a.end_buttons[0]),b2=sdl_rect(a.end_buttons[1]);bool p1=point_in(b1,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT)),p2=point_in(b2,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Rect b1p=pressed_paint_rect(b1,p1),b2p=pressed_paint_rect(b2,p2);draw_round_rect(g->ren,b1p,7,p1?th.sel:th.title);(void)draw_fitted_text(g,tr(ui->language,ui->result==RES_WIN?T_NEXT:T_RETRY),th.bg,b1p,fonts.control+2,10);draw_round_rect(g->ren,b2p,7,p2?th.sel:th.btn);(void)draw_fitted_text(g,tr(ui->language,T_MENU),th.btnfg,b2p,fonts.control,10);
}

static bool confirm_box(SDL_Window*win,const char*title,const char*msg,const char*ok_label,const char*cancel_label){
  const SDL_MessageBoxButtonData buttons[]={{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,cancel_label},{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,1,ok_label?ok_label:"OK"}};
  const SDL_MessageBoxColorScheme scheme={{{25,25,25},{200,200,200},{50,50,50},{60,120,220},{230,230,230}}};
  const SDL_MessageBoxData data={SDL_MESSAGEBOX_INFORMATION,win,title,msg,2,buttons,&scheme};int buttonid=-1;if(SDL_ShowMessageBox(&data,&buttonid)<0)return false;return buttonid==1;
}

static void app_render(Gfx*g,const Game*game,UI*ui){update_font_tiers(g,ui);if(ui->screen==SCR_TITLE)render_title(g,ui);else if(ui->screen==SCR_HELP)render_help(g,ui);else if(ui->screen==SCR_ABOUT)render_about(g,ui);else if(ui->screen==SCR_END)render_end(g,game,ui);else render_board_and_sidebar(g,game,ui);draw_language_selector(g,ui);}
static void app_frame(Gfx*g,const Game*game,UI*ui){app_render(g,game,ui);SDL_RenderPresent(g->ren);}
static bool save_frame(Gfx*g,const char*path){SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,g->width,g->height,32,SDL_PIXELFORMAT_ARGB8888);if(!s)return false;bool ok=SDL_RenderReadPixels(g->ren,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)==0&&SDL_SaveBMP(s,path)==0;SDL_FreeSurface(s);return ok;}

static void app_shutdown(Gfx*g){
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
static bool app_init(Gfx*g,const char*font_cli){
  memset(g,0,sizeof(*g));g->width=1024;g->height=720;SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return false;}if(TTF_Init()!=0){fprintf(stderr,"TTF_Init: %s\n",TTF_GetError());app_shutdown(g);return false;}
  g->win=SDL_CreateWindow(SUDOKURA_NAME_VERSION,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,g->width,g->height,SDL_WINDOW_RESIZABLE);if(!g->win){fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());app_shutdown(g);return false;}SDL_SetWindowMinimumSize(g->win,360,480);g->ren=SDL_CreateRenderer(g->win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!g->ren)g->ren=SDL_CreateRenderer(g->win,-1,SDL_RENDERER_SOFTWARE);if(!g->ren){fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());app_shutdown(g);return false;}
  SDL_Surface*icon=SDL_CreateRGBSurfaceWithFormatFrom((void*)sudokura_icon_rgba,(int)sudokura_icon_width,(int)sudokura_icon_height,32,(int)sudokura_icon_width*4,SDL_PIXELFORMAT_RGBA32);if(icon){SDL_SetWindowIcon(g->win,icon);SDL_FreeSurface(icon);}SDL_Surface*word=SDL_CreateRGBSurfaceWithFormatFrom((void*)sudokura_wordmark_rgba,(int)sudokura_wordmark_width,(int)sudokura_wordmark_height,32,(int)sudokura_wordmark_width*4,SDL_PIXELFORMAT_RGBA32);if(word){g->wordmark=SDL_CreateTextureFromSurface(g->ren,word);SDL_FreeSurface(word);}if(!g->wordmark){fprintf(stderr,"wordmark texture: %s\n",SDL_GetError());app_shutdown(g);return false;}
  language_flags[LANG_EN]=texture_from_rgba(g->ren,sudokura_flag_us_rgba,sudokura_flag_us_width,sudokura_flag_us_height);language_flags[LANG_ES]=texture_from_rgba(g->ren,sudokura_flag_ar_rgba,sudokura_flag_ar_width,sudokura_flag_ar_height);language_flags[LANG_CA]=texture_from_rgba(g->ren,sudokura_flag_ca_rgba,sudokura_flag_ca_width,sudokura_flag_ca_height);
  char font_path[PATH_MAX]={0};const char*fpath=find_font_path_dynamic(font_path,font_cli);if(!fpath){fprintf(stderr,"%s\n",tr(LANG_EN,T_FONT_ERROR));app_shutdown(g);return false;}for(int i=0;i<FONT_CACHE_COUNT;i++){g->fonts[i]=TTF_OpenFont(fpath,font_cache_sizes[i]);if(!g->fonts[i]){fprintf(stderr,"font %d: %s\n",font_cache_sizes[i],TTF_GetError());app_shutdown(g);return false;}}
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
