static SDL_Rect pressed_paint_rect(SDL_Rect r,bool pressed){if(pressed&&r.h>4){r.y+=2;r.h-=2;}return r;}

static void render_pause_overlay(Gfx*g,const UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();draw_rect(g->ren,0,0,g->width,g->height,th.bg);AppGeometry a;if(!get_geometry(g,ui,&a))return;GeometryFonts fonts=geometry_font_sizes(&a,g->width,g->height);
  draw_wordmark(g,sdl_rect(a.pause_logo));(void)draw_fitted_text(g,tr(ui->language,T_PAUSED),th.title,sdl_rect(a.pause_heading),fonts.heading,16);
  int mx=0,my=0;Uint32 mouse=SDL_GetMouseState(&mx,&my);SDL_Rect resume=sdl_rect(a.pause_buttons[0]),menu=sdl_rect(a.pause_buttons[1]);bool resume_pressed=point_in(resume,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT)),menu_pressed=point_in(menu,mx,my)&&(mouse&SDL_BUTTON(SDL_BUTTON_LEFT));SDL_Rect resume_paint=pressed_paint_rect(resume,resume_pressed),menu_paint=pressed_paint_rect(menu,menu_pressed);draw_round_rect(g->ren,resume_paint,7,resume_pressed?th.sel:th.title);(void)draw_fitted_text(g,tr(ui->language,T_RESUME),th.bg,resume_paint,fonts.control+2,10);draw_round_rect(g->ren,menu_paint,7,menu_pressed?th.sel:th.btn);(void)draw_fitted_text(g,tr(ui->language,T_MENU),th.btnfg,menu_paint,fonts.control,10);draw_focus_ring(g->ren,pause_keyboard_focus==0?resume:menu,th.sel_outline);
}

static void help_draw_scrolled_texture(Gfx*g,SDL_Texture*texture,
                                       SDL_Rect body,int virtual_y,
                                       int texture_w,int texture_h,
                                       int scroll_px){
  if(!texture||texture_w<=0||texture_h<=0)return;
  int screen_y=body.y+virtual_y-scroll_px;
  int source_y=0;
  int visible_h=texture_h;
  if(screen_y<body.y){
    source_y=body.y-screen_y;
    visible_h-=source_y;
    screen_y=body.y;
  }
  int bottom=body.y+body.h;
  if(screen_y+visible_h>bottom)visible_h=bottom-screen_y;
  if(visible_h<=0)return;
  SDL_Rect source=gfx_physical_rect(g,0,source_y,texture_w,visible_h);
  SDL_Rect dest={body.x,screen_y,texture_w,visible_h};
  SDL_RenderCopy(g->ren,texture,&source,&dest);
}

static void render_help(Gfx*g,UI*ui){
  Theme th=ui->dark_theme?theme_dark():theme_light();
  SDL_SetRenderDrawColor(g->ren,th.bg.r,th.bg.g,th.bg.b,255);
  SDL_RenderClear(g->ren);
  AppGeometry a;
  if(!get_geometry(g,ui,&a))return;
  SDL_Rect heading=sdl_rect(a.info_heading),body=sdl_rect(a.info_body);
  int tw=0,thh=0;
  SDL_Texture*T=render_text(g,g->font_big,tr(ui->language,T_HELP),th.title,&tw,&thh);
  if(T){
    SDL_Rect d={heading.x,heading.y,tw,thh};
    SDL_RenderCopy(g->ren,T,NULL,&d);
  }

  const TextKey titles[]={
    T_HELP_GOAL_TITLE,T_HELP_MODES_TITLE,T_HELP_CONTROLS_TITLE,
    T_HELP_HINTS_TITLE,T_HELP_SHORTCUTS_TITLE
  };
  const TextKey bodies[]={
    T_HELP_GOAL_BODY,T_HELP_MODES_BODY,T_HELP_CONTROLS_BODY,
    T_HELP_HINTS_BODY,T_HELP_SHORTCUTS_BODY
  };
  enum{HELP_SECTION_COUNT=5};
  SDL_Texture*title_texture[HELP_SECTION_COUNT]={0};
  SDL_Texture*body_texture[HELP_SECTION_COUNT]={0};
  int title_w[HELP_SECTION_COUNT]={0},title_h[HELP_SECTION_COUNT]={0};
  int body_w[HELP_SECTION_COUNT]={0},body_h[HELP_SECTION_COUNT]={0};
  int section_y[HELP_SECTION_COUNT]={0};
  int text_w=body.w-18;
  if(text_w<16)text_w=body.w;
  const char*modifier=input_primary_modifier_label();
  char shortcut_text[1200];
  snprintf(shortcut_text,sizeof shortcut_text,
           tr(ui->language,T_HELP_SHORTCUTS_BODY),
           modifier,modifier,modifier,modifier);

  int content_h=0;
  const int title_gap=5,section_gap=14;
  for(int i=0;i<HELP_SECTION_COUNT;i++){
    section_y[i]=content_h;
    title_texture[i]=render_text_wrapped(
        g,g->font_body,tr(ui->language,titles[i]),th.title,text_w,
        &title_w[i],&title_h[i]);
    const char*body_text=i==HELP_SECTION_COUNT-1
                              ?shortcut_text
                              :tr(ui->language,bodies[i]);
    body_texture[i]=render_text_wrapped(
        g,g->font_body,body_text,th.dim,text_w,&body_w[i],&body_h[i]);
    content_h+=title_h[i]+title_gap+body_h[i];
    if(i+1<HELP_SECTION_COUNT)content_h+=section_gap;
  }

  help_scroll_max_px=content_h>body.h?content_h-body.h:0;
  help_scroll_set(help_scroll_px);
  SDL_Rect previous;
  SDL_RenderGetClipRect(g->ren,&previous);
  SDL_RenderSetClipRect(g->ren,&body);
  for(int i=0;i<HELP_SECTION_COUNT;i++){
    int y=section_y[i];
    help_draw_scrolled_texture(g,title_texture[i],body,y,title_w[i],
                               title_h[i],help_scroll_px);
    y+=title_h[i]+title_gap;
    help_draw_scrolled_texture(g,body_texture[i],body,y,body_w[i],
                               body_h[i],help_scroll_px);
  }
  SDL_RenderSetClipRect(g->ren,previous.w||previous.h?&previous:NULL);

  if(help_scroll_max_px>0){
    SDL_Rect rail={body.x+body.w-5,body.y,4,body.h};
    SDL_Color rail_color=th.btn;rail_color.a=150;
    draw_round_rect(g->ren,rail,2,rail_color);
    int thumb_h=body.h*body.h/(content_h>0?content_h:body.h);
    if(thumb_h<28)thumb_h=28;
    if(thumb_h>body.h)thumb_h=body.h;
    int travel=body.h-thumb_h;
    int thumb_y=body.y+(help_scroll_max_px
        ?help_scroll_px*travel/help_scroll_max_px:0);
    draw_round_rect(g->ren,(SDL_Rect){rail.x,thumb_y,rail.w,thumb_h},2,
                    th.sel_outline);
  }
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
  ui_audio_control_cancel_interaction();
  const SDL_MessageBoxButtonData buttons[]={{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,cancel_label},{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,1,ok_label?ok_label:"OK"}};
  const SDL_MessageBoxColorScheme scheme={{{25,25,25},{200,200,200},{50,50,50},{60,120,220},{230,230,230}}};
  const SDL_MessageBoxData data={SDL_MESSAGEBOX_INFORMATION,win,title,msg,2,buttons,&scheme};int buttonid=-1;int status=SDL_ShowMessageBox(&data,&buttonid);window_focus_guard_reset();if(status<0)return false;return buttonid==1;
}

static int choice_box(SDL_Window*win,const char*title,const char*msg,
                      const char*back_label,const char*retry_label,
                      const char*discard_label){
  ui_audio_control_cancel_interaction();
  const SDL_MessageBoxButtonData buttons[]={
    {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,back_label},
    {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,1,retry_label},
    {0,2,discard_label}
  };
  const SDL_MessageBoxColorScheme scheme={{{25,25,25},{200,200,200},{50,50,50},{60,120,220},{230,230,230}}};
  const SDL_MessageBoxData data={SDL_MESSAGEBOX_WARNING,win,title,msg,3,buttons,&scheme};
  int buttonid=0;
  int status=SDL_ShowMessageBox(&data,&buttonid);
  window_focus_guard_reset();
  if(status<0)return 0;
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
static bool save_frame(Gfx*g,const char*path){
  if(!g||!g->ren||g->width<=0||g->height<=0)return false;
  int output_w=g->output_width>0?g->output_width:g->width;
  int output_h=g->output_height>0?g->output_height:g->height;
  SDL_Surface*physical=SDL_CreateRGBSurfaceWithFormat(
      0,output_w,output_h,32,SDL_PIXELFORMAT_ARGB8888);
  if(!physical)return false;
  bool read_ok=SDL_RenderReadPixels(g->ren,NULL,SDL_PIXELFORMAT_ARGB8888,
                                    physical->pixels,physical->pitch)==0;
  if(!read_ok){SDL_FreeSurface(physical);return false;}
  SDL_Surface*logical=physical;
  if(output_w!=g->width||output_h!=g->height){
    logical=SDL_CreateRGBSurfaceWithFormat(
        0,g->width,g->height,32,SDL_PIXELFORMAT_ARGB8888);
    if(!logical){SDL_FreeSurface(physical);return false;}
    SDL_Rect src={0,0,output_w,output_h};
    SDL_Rect dst={0,0,g->width,g->height};
    if(SDL_BlitScaled(physical,&src,logical,&dst)!=0){
      SDL_FreeSurface(logical);SDL_FreeSurface(physical);return false;
    }
  }
  bool ok=SDL_SaveBMP(logical,path)==0;
  if(logical!=physical)SDL_FreeSurface(logical);
  SDL_FreeSurface(physical);
  return ok;
}

static void gfx_close_fonts(Gfx*g){
  if(!g)return;
  text_cache_clear();
  for(int i=0;i<FONT_CACHE_COUNT;i++){
    if(g->fonts[i])TTF_CloseFont(g->fonts[i]);
    g->fonts[i]=NULL;
  }
  g->font_big=g->font_small=g->font_note=g->font_cell=g->font_body=NULL;
}

static bool gfx_open_fonts(Gfx*g){
  if(!g||!g->font_path[0])return false;
  gfx_close_fonts(g);
  for(int i=0;i<FONT_CACHE_COUNT;i++){
    int physical_size=desktop_scale_value(font_cache_sizes[i],
                                           g->metrics.scale_milli);
    g->fonts[i]=TTF_OpenFont(g->font_path,physical_size);
    if(!g->fonts[i]){
      fprintf(stderr,"font %d@%d: %s\n",font_cache_sizes[i],physical_size,
              TTF_GetError());
      gfx_close_fonts(g);
      return false;
    }
  }
  g->font_big=cached_font(g,44);g->font_small=cached_font(g,20);
  g->font_note=cached_font(g,12);g->font_cell=cached_font(g,28);
  g->font_body=cached_font(g,18);
  return true;
}

static bool gfx_refresh_metrics(Gfx*g){
  if(!g||!g->win||!g->ren)return false;
  int logical_w=0,logical_h=0,output_w=0,output_h=0;
  SDL_GetWindowSize(g->win,&logical_w,&logical_h);
  if(logical_w<=0||logical_h<=0)return false;
  if(SDL_GetRendererOutputSize(g->ren,&output_w,&output_h)!=0||
     output_w<=0||output_h<=0){
    output_w=logical_w;output_h=logical_h;
  }
  DesktopMetrics next;
  if(!desktop_metrics_compute(logical_w,logical_h,output_w,output_h,&next))
    return false;
  int old_scale=g->metrics.scale_milli;
  g->metrics=next;g->width=logical_w;g->height=logical_h;
  g->output_width=output_w;g->output_height=output_h;
  if(SDL_RenderSetLogicalSize(g->ren,logical_w,logical_h)!=0){
    fprintf(stderr,"SDL_RenderSetLogicalSize: %s\n",SDL_GetError());
    return false;
  }
  if(old_scale>0&&old_scale!=next.scale_milli&&g->font_path[0]&&
     !gfx_open_fonts(g))
    return false;
  g->dirty=true;
  return true;
}

static void gfx_capture_restored_bounds(Gfx*g){
  if(!g||!g->win)return;
  Uint32 flags=SDL_GetWindowFlags(g->win);
  if(flags&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_FULLSCREEN|
            SDL_WINDOW_FULLSCREEN_DESKTOP))return;
  int x=0,y=0,w=0,h=0;
  SDL_GetWindowPosition(g->win,&x,&y);SDL_GetWindowSize(g->win,&w,&h);
  if(w<=0||h<=0)return;
  g->restored_x=x;g->restored_y=y;g->restored_width=w;g->restored_height=h;
  g->restored_bounds_valid=true;
}

static void app_destroy_base_textures(Gfx*g){
  if(!g)return;
  if(g->wordmark){SDL_DestroyTexture(g->wordmark);g->wordmark=NULL;}
  for(int i=0;i<LANG_COUNT;i++){
    if(language_flags[i])SDL_DestroyTexture(language_flags[i]);
    language_flags[i]=NULL;
  }
}

static SDL_Texture*texture_from_rgba(SDL_Renderer*renderer,const unsigned char*rgba,unsigned int width,unsigned int height){SDL_Surface*s=SDL_CreateRGBSurfaceWithFormatFrom((void*)rgba,(int)width,(int)height,32,(int)width*4,SDL_PIXELFORMAT_RGBA32);if(!s)return NULL;SDL_Texture*t=SDL_CreateTextureFromSurface(renderer,s);SDL_FreeSurface(s);return t;}

static bool app_rebuild_base_textures(Gfx*g){
  if(!g||!g->ren)return false;
  app_destroy_base_textures(g);
  SDL_Surface*word=SDL_CreateRGBSurfaceWithFormatFrom(
      (void*)sudokura_wordmark_rgba,(int)sudokura_wordmark_width,
      (int)sudokura_wordmark_height,32,(int)sudokura_wordmark_width*4,
      SDL_PIXELFORMAT_RGBA32);
  if(word){g->wordmark=SDL_CreateTextureFromSurface(g->ren,word);SDL_FreeSurface(word);}
  language_flags[LANG_EN]=texture_from_rgba(g->ren,sudokura_flag_us_rgba,
      sudokura_flag_us_width,sudokura_flag_us_height);
  language_flags[LANG_ES]=texture_from_rgba(g->ren,sudokura_flag_ar_rgba,
      sudokura_flag_ar_width,sudokura_flag_ar_height);
  language_flags[LANG_CA]=texture_from_rgba(g->ren,sudokura_flag_ca_rgba,
      sudokura_flag_ca_width,sudokura_flag_ca_height);
  return g->wordmark&&language_flags[LANG_EN]&&language_flags[LANG_ES]&&
         language_flags[LANG_CA];
}

static void app_shutdown_base(Gfx*g){
  if(!g)return;
  app_destroy_base_textures(g);
  gfx_close_fonts(g);
  if(g->ren)SDL_DestroyRenderer(g->ren);
  if(g->win)SDL_DestroyWindow(g->win);
  memset(g,0,sizeof(*g));
  if(TTF_WasInit())TTF_Quit();
  SDL_Quit();
}

static bool app_init_base(Gfx*g,const char*font_cli){
  memset(g,0,sizeof(*g));g->width=SUDOKURA_DEFAULT_WINDOW_WIDTH;
  g->height=SUDOKURA_DEFAULT_WINDOW_HEIGHT;g->dirty=true;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){
    fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return false;
  }
  if(TTF_Init()!=0){
    fprintf(stderr,"TTF_Init: %s\n",TTF_GetError());app_shutdown_base(g);
    return false;
  }
  Uint32 window_flags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI;
  g->win=SDL_CreateWindow(SUDOKURA_NAME_VERSION,SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,g->width,g->height,window_flags);
  if(!g->win){
    fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());
    app_shutdown_base(g);return false;
  }
  SDL_SetWindowMinimumSize(g->win,360,480);
  g->ren=SDL_CreateRenderer(g->win,-1,
      SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(!g->ren)g->ren=SDL_CreateRenderer(g->win,-1,SDL_RENDERER_SOFTWARE);
  if(!g->ren){
    fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());
    app_shutdown_base(g);return false;
  }
  SDL_RendererInfo info;
  if(SDL_GetRendererInfo(g->ren,&info)==0)
    g->software_renderer=(info.flags&SDL_RENDERER_SOFTWARE)!=0;
  if(!gfx_refresh_metrics(g)){app_shutdown_base(g);return false;}

  SDL_Surface*icon=SDL_CreateRGBSurfaceWithFormatFrom(
      (void*)sudokura_icon_rgba,(int)sudokura_icon_width,
      (int)sudokura_icon_height,32,(int)sudokura_icon_width*4,
      SDL_PIXELFORMAT_RGBA32);
  if(icon){SDL_SetWindowIcon(g->win,icon);SDL_FreeSurface(icon);}

  char font_path[PATH_MAX]={0};
  const char*fpath=find_font_path_dynamic(font_path,font_cli);
  if(!fpath||!path_copy(g->font_path,sizeof(g->font_path),fpath)){
    fprintf(stderr,"%s\n",tr(LANG_EN,T_FONT_ERROR));
    app_shutdown_base(g);return false;
  }
  if(!gfx_open_fonts(g)){app_shutdown_base(g);return false;}
  if(!app_rebuild_base_textures(g)){
    fprintf(stderr,"presentation textures: %s\n",SDL_GetError());
    app_shutdown_base(g);return false;
  }
  gfx_capture_restored_bounds(g);
  return true;
}

static bool make_directory(const char*path){char part[PATH_MAX];size_t length=strlen(path);if(length==0||length>=sizeof(part))return false;if(!path_copy(part,sizeof(part),path))return false;for(size_t i=1;i<=length;i++)if(part[i]=='/'||part[i]=='\\'||part[i]=='\0'){char saved=part[i];part[i]='\0';if(part[0]){
#if defined(_WIN32)
  if(_mkdir(part)!=0&&errno!=EEXIST)return false;
#else
  if(mkdir(part,0755)!=0&&errno!=EEXIST)return false;
#endif
}part[i]=saved;}return true;}
static void apply_window_resize(Gfx*g,int requested_width,int requested_height){
  if(!g||!g->win)return;
  int width=0,height=0;
  bool changed=geometry_normalize_window_size(requested_width,requested_height,
                                               &width,&height);
  if(changed)SDL_SetWindowSize(g->win,width,height);
  (void)gfx_refresh_metrics(g);
  gfx_capture_restored_bounds(g);
}
