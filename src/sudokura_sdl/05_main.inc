/* =================== MAIN =================== */
static bool home_focus_available(const UI*ui,int focus){return focus!=HOME_FOCUS_CONTINUE||ui->has_session;}
static void home_move_focus(const UI*ui,int direction){
  int next=home_focus;do{next=(next+direction+HOME_FOCUS_COUNT)%HOME_FOCUS_COUNT;}while(!home_focus_available(ui,next));home_focus=next;
}
static void home_set_language(Persistence*p,UI*ui,Language language){ui->language=language;(void)save_preferences(p,ui);}
static void home_cycle_language(Persistence*p,UI*ui,int delta){int next=((int)ui->language+delta+LANG_COUNT)%LANG_COUNT;home_set_language(p,ui,(Language)next);}
static void home_set_mode(Persistence*p,UI*ui,Mode mode){ui->mode=mode;p->preferences.mode=mode;set_mode_params(ui);(void)save_preferences(p,ui);}
static void home_cycle_mode(Persistence*p,UI*ui,int delta){home_set_mode(p,ui,(Mode)(((int)ui->mode+delta+3)%3));}
static void home_set_difficulty(Persistence*p,UI*ui,GameDifficulty difficulty){home_difficulty=difficulty;p->preferences.difficulty=difficulty;(void)save_preferences(p,ui);}
static void home_cycle_difficulty(Persistence*p,UI*ui,int delta){home_set_difficulty(p,ui,(GameDifficulty)(((int)home_difficulty+delta+DIFFICULTY_COUNT)%DIFFICULTY_COUNT));}
static bool replace_saved_session_confirmed(Persistence*p,UI*ui,Gfx*g){return !ui->has_session||!saved_has_progress(&p->saved)||confirm_box(g->win,tr(ui->language,T_NEW_GAME),tr(ui->language,T_NEW_PROMPT),tr(ui->language,T_NEW),tr(ui->language,T_CANCEL));}
static void home_new_game(Persistence*p,Game*game,UI*ui,Gfx*g){if(replace_saved_session_confirmed(p,ui,g)){victory_animation_reset();start_new_session(p,game,ui);}}
static void home_daily_game(Persistence*p,Game*game,UI*ui,Gfx*g){
  int year=0,month=0,day=0;if(!local_date(&year,&month,&day))return;victory_animation_reset();
  if(saved_daily_matches(p,year,month,day)){restore_saved_session(p,game,ui);return;}
  if(replace_saved_session_confirmed(p,ui,g))(void)start_daily_session(p,game,ui,year,month,day);
}
static void home_activate(Persistence*p,Game*game,UI*ui,Gfx*g,bool*running){
  switch(home_focus){
    case HOME_FOCUS_LANGUAGE:home_cycle_language(p,ui,1);break;
    case HOME_FOCUS_MODE:home_cycle_mode(p,ui,1);break;
    case HOME_FOCUS_DIFFICULTY:home_cycle_difficulty(p,ui,1);break;
    case HOME_FOCUS_NEW:home_new_game(p,game,ui,g);break;
    case HOME_FOCUS_CONTINUE:if(ui->has_session){victory_animation_reset();restore_saved_session(p,game,ui);}break;
    case HOME_FOCUS_DAILY:home_daily_game(p,game,ui,g);break;
    case HOME_FOCUS_HELP:ui->prev_screen=SCR_TITLE;ui->screen=SCR_HELP;break;
    case HOME_FOCUS_ABOUT:ui->prev_screen=SCR_TITLE;ui->screen=SCR_ABOUT;break;
    case HOME_FOCUS_THEME:ui->dark_theme=!ui->dark_theme;(void)save_preferences(p,ui);break;
    case HOME_FOCUS_QUIT:*running=false;break;
    default:break;
  }
}

int main(int argc,char**argv){
  const char*font_cli=NULL;const char*screenshot_dir=NULL;bool smoke_test=false;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--font")&&i+1<argc)font_cli=argv[++i];
    else if(!strcmp(argv[i],"--smoke-test"))smoke_test=true;
    else if(!strcmp(argv[i],"--render-screenshots")&&i+1<argc)screenshot_dir=argv[++i];
  }

  Gfx g;if(!app_init(&g,font_cli))return 1;
  Game game;memset(&game,0,sizeof(game));
  UI ui;memset(&ui,0,sizeof(ui));
  ui.sel_r=4;ui.sel_c=4;ui.language=LANG_EN;ui.dark_theme=true;ui.mode=MODE_CLASSIC;ui.screen=SCR_TITLE;ui.prev_screen=SCR_TITLE;ui.running_since=now_s();set_mode_params(&ui);home_difficulty=DIFFICULTY_MEDIUM;

  if(smoke_test||screenshot_dir){
    game_new(&game,110u);ui.has_session=true;ui.screen=SCR_TITLE;app_frame(&g,&game,&ui);ui.screen=SCR_PLAY;app_frame(&g,&game,&ui);
    if(screenshot_dir){
      const int sizes[][2]={{640,480},{800,600},{1024,720},{1366,768},{1920,1080},{2560,1440},{3440,1440},{360,640},{390,844},{412,915}};
      const char*names[]={"title","play","help","result","pause"};
      if(!make_directory(screenshot_dir)){fprintf(stderr,"cannot create screenshot directory: %s\n",screenshot_dir);app_shutdown(&g);return 1;}
      for(unsigned n=0;n<sizeof(sizes)/sizeof(sizes[0]);n++)for(int variant=0;variant<5;variant++){
        SDL_SetWindowMinimumSize(g.win,1,1);SDL_SetWindowSize(g.win,sizes[n][0],sizes[n][1]);SDL_PumpEvents();SDL_GetRendererOutputSize(g.ren,&g.width,&g.height);
        if(g.width!=sizes[n][0]||g.height!=sizes[n][1]){fprintf(stderr,"renderer refused %dx%d (got %dx%d)\n",sizes[n][0],sizes[n][1],g.width,g.height);app_shutdown(&g);return 1;}
        ui.language=(Language)((n+variant)%LANG_COUNT);ui.dark_theme=((n+variant)&1)==0;ui.result=RES_NONE;ui.pause_reasons=0;ui.is_daily=false;victory_animation_reset();
        if(variant==0)ui.screen=SCR_TITLE;else if(variant==1)ui.screen=SCR_PLAY;else if(variant==2)ui.screen=SCR_HELP;else if(variant==3){ui.screen=SCR_END;ui.result=RES_WIN;}else{ui.screen=SCR_PLAY;ui.pause_reasons=PAUSE_MANUAL;}
        app_render(&g,&game,&ui);char path[PATH_MAX],filename[160];int filename_length=snprintf(filename,sizeof(filename),"%dx%d-%s-%s-%s.bmp",g.width,g.height,names[variant],language_name(ui.language),ui.dark_theme?"dark":"light");
        if(filename_length<0||(size_t)filename_length>=sizeof(filename)||!path_join(path,sizeof(path),screenshot_dir,filename,'/')){fprintf(stderr,"screenshot path is too long: %s\n",screenshot_dir);app_shutdown(&g);return 1;}
        if(!save_frame(&g,path)){fprintf(stderr,"screenshot: %s\n",SDL_GetError());app_shutdown(&g);return 1;}
      }
      printf("rendered 50 UI review screenshots to %s\n",screenshot_dir);
    }
    app_shutdown(&g);return 0;
  }

  Persistence persistence;persistence_init(&persistence);
  ui.language=persistence.preferences.language;ui.dark_theme=persistence.preferences.dark_theme;ui.strict_mode=persistence.preferences.strict_mode;ui.mode=persistence.preferences.mode;home_difficulty=persistence.preferences.difficulty;set_mode_params(&ui);ui.has_session=persistence.enabled&&session_validate(&persistence.saved);

  bool running=true;SDL_Event e;
  while(running){
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT){if(ui.session_open)(void)save_current_session(&persistence,&game,&ui);running=false;continue;}
      if(e.type==SDL_WINDOWEVENT){
        if(e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)apply_window_resize(&g,e.window.data1,e.window.data2);
        else if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&ui.session_open&&ui.result==RES_NONE){clock_pause(&ui,PAUSE_FOCUS);(void)save_current_session(&persistence,&game,&ui);}
        else if(e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED&&ui.session_open&&ui.result==RES_NONE){clock_resume(&ui,PAUSE_FOCUS);(void)save_current_session(&persistence,&game,&ui);}
        continue;
      }
      if(e.type==SDL_KEYDOWN){
        SDL_Keycode k=e.key.keysym.sym;SDL_Keymod mod=SDL_GetModState();
        if(k==SDLK_l){home_cycle_language(&persistence,&ui,1);continue;}
        if(ui.screen==SCR_TITLE){
          if(k==SDLK_ESCAPE)running=false;
          else if(k==SDLK_TAB)home_move_focus(&ui,(mod&KMOD_SHIFT)?-1:1);
          else if(k==SDLK_LEFT){if(home_focus==HOME_FOCUS_LANGUAGE)home_cycle_language(&persistence,&ui,-1);else if(home_focus==HOME_FOCUS_MODE)home_cycle_mode(&persistence,&ui,-1);else if(home_focus==HOME_FOCUS_DIFFICULTY)home_cycle_difficulty(&persistence,&ui,-1);else home_move_focus(&ui,-1);}
          else if(k==SDLK_RIGHT){if(home_focus==HOME_FOCUS_LANGUAGE)home_cycle_language(&persistence,&ui,1);else if(home_focus==HOME_FOCUS_MODE)home_cycle_mode(&persistence,&ui,1);else if(home_focus==HOME_FOCUS_DIFFICULTY)home_cycle_difficulty(&persistence,&ui,1);else home_move_focus(&ui,1);}
          else if(k==SDLK_UP)home_move_focus(&ui,-1);
          else if(k==SDLK_DOWN)home_move_focus(&ui,1);
          else if(k==SDLK_RETURN||k==SDLK_KP_ENTER)home_activate(&persistence,&game,&ui,&g,&running);
          else if(k==SDLK_c&&ui.has_session){victory_animation_reset();restore_saved_session(&persistence,&game,&ui);}
          else if(k==SDLK_d)home_daily_game(&persistence,&game,&ui,&g);
          else if(k==SDLK_t){ui.dark_theme=!ui.dark_theme;(void)save_preferences(&persistence,&ui);}
          else if(k==SDLK_F1){ui.prev_screen=SCR_TITLE;ui.screen=SCR_HELP;}
          else if(k==SDLK_F2){ui.prev_screen=SCR_TITLE;ui.screen=SCR_ABOUT;}
          else if(k==SDLK_m)home_cycle_mode(&persistence,&ui,1);
          continue;
        }
        if(ui.screen==SCR_HELP||ui.screen==SCR_ABOUT){if(k==SDLK_ESCAPE)return_from_info(&persistence,&game,&ui);continue;}
        if(ui.screen==SCR_END){
          if(k==SDLK_ESCAPE){victory_animation_reset();go_home(&persistence,&game,&ui);}
          else if(k==SDLK_RETURN||k==SDLK_KP_ENTER){victory_animation_reset();if(ui.result==RES_WIN)start_new_session(&persistence,&game,&ui);else restart_current_session(&persistence,&game,&ui);}
          continue;
        }
        if(ui.pause_reasons){
          if((ui.pause_reasons&PAUSE_MANUAL)&&(k==SDLK_p||k==SDLK_RETURN||k==SDLK_KP_ENTER)){clock_resume(&ui,PAUSE_MANUAL);(void)save_current_session(&persistence,&game,&ui);}
          else if(k==SDLK_ESCAPE)go_home(&persistence,&game,&ui);
          continue;
        }
        bool shifted=(mod&KMOD_SHIFT)!=0;int r=ui.sel_r,c=ui.sel_c;
        if(k==SDLK_ESCAPE)go_home(&persistence,&game,&ui);
        else if(k==SDLK_UP||k==SDLK_w)ui.sel_r=(ui.sel_r+8)%9;
        else if(k==SDLK_DOWN||k==SDLK_s)ui.sel_r=(ui.sel_r+1)%9;
        else if(k==SDLK_LEFT||k==SDLK_a)ui.sel_c=(ui.sel_c+8)%9;
        else if(k==SDLK_RIGHT||k==SDLK_d)ui.sel_c=(ui.sel_c+1)%9;
        else if(k==SDLK_p){clock_pause(&ui,PAUSE_MANUAL);(void)save_current_session(&persistence,&game,&ui);}
        else if(k==SDLK_t){ui.dark_theme=!ui.dark_theme;(void)save_preferences(&persistence,&ui);}
        else if(k==SDLK_n){ui.notes_mode=!ui.notes_mode;show_toast(&ui,tr(ui.language,ui.notes_mode?T_NOTES_ON:T_NOTES_OFF));(void)save_current_session(&persistence,&game,&ui);}
        else if(k==SDLK_m){ui.strict_mode=!ui.strict_mode;show_toast(&ui,tr(ui.language,ui.strict_mode?T_STRICT:T_FREE));(void)save_preferences(&persistence,&ui);(void)save_current_session(&persistence,&game,&ui);}
        else if(k==SDLK_h){if(game_hint(&game,r,c)){show_toast(&ui,tr(ui.language,T_HINT_USED));(void)save_current_session(&persistence,&game,&ui);}}
        else if(k==SDLK_DELETE||k==SDLK_BACKSPACE||k==SDLK_0||k==SDLK_KP_0){if(apply_player_input(&game,&ui,r,c,0,false))(void)save_current_session(&persistence,&game,&ui);}
        else{int v=0;if(k>=SDLK_1&&k<=SDLK_9)v=k-SDLK_0;else if(k>=SDLK_KP_1&&k<=SDLK_KP_9)v=k-SDLK_KP_0;if(v>=1&&v<=9&&apply_player_input(&game,&ui,r,c,v,ui.notes_mode||shifted))(void)save_current_session(&persistence,&game,&ui);}
        continue;
      }
      if(e.type==SDL_MOUSEBUTTONDOWN){
        int x=e.button.x,y=e.button.y;bool right=e.button.button==SDL_BUTTON_RIGHT;
        if(point_in(language_rect(&g,&ui),x,y)){
          for(int language=0;language<LANG_COUNT;language++)if(point_in(language_segment(&g,&ui,language),x,y)){home_set_language(&persistence,&ui,(Language)language);if(ui.screen==SCR_TITLE)home_focus=HOME_FOCUS_LANGUAGE;break;}
          continue;
        }
        if(ui.screen==SCR_TITLE){
          AppGeometry a;if(!get_geometry(&g,&ui,&a))continue;
          bool handled=false;
          for(int i=0;i<GEOMETRY_HOME_SEGMENT_COUNT;i++)if(point_in(sdl_rect(a.home_mode[i]),x,y)){home_focus=HOME_FOCUS_MODE;home_set_mode(&persistence,&ui,(Mode)i);handled=true;break;}
          if(handled)continue;
          for(int i=0;i<GEOMETRY_HOME_SEGMENT_COUNT;i++)if(point_in(sdl_rect(a.home_difficulty[i]),x,y)){home_focus=HOME_FOCUS_DIFFICULTY;home_set_difficulty(&persistence,&ui,(GameDifficulty)i);handled=true;break;}
          if(handled)continue;
          if(point_in(sdl_rect(a.home_primary[0]),x,y)){home_focus=HOME_FOCUS_NEW;home_new_game(&persistence,&game,&ui,&g);}
          else if(point_in(sdl_rect(a.home_primary[1]),x,y)&&ui.has_session){home_focus=HOME_FOCUS_CONTINUE;victory_animation_reset();restore_saved_session(&persistence,&game,&ui);}
          else if(point_in(sdl_rect(a.home_primary[2]),x,y)){home_focus=HOME_FOCUS_DAILY;home_daily_game(&persistence,&game,&ui,&g);}
          else if(point_in(sdl_rect(a.home_secondary[0]),x,y)){home_focus=HOME_FOCUS_HELP;ui.prev_screen=SCR_TITLE;ui.screen=SCR_HELP;}
          else if(point_in(sdl_rect(a.home_secondary[1]),x,y)){home_focus=HOME_FOCUS_ABOUT;ui.prev_screen=SCR_TITLE;ui.screen=SCR_ABOUT;}
          else if(point_in(sdl_rect(a.home_secondary[2]),x,y)){home_focus=HOME_FOCUS_THEME;ui.dark_theme=!ui.dark_theme;(void)save_preferences(&persistence,&ui);}
          else if(point_in(sdl_rect(a.home_secondary[3]),x,y)){home_focus=HOME_FOCUS_QUIT;running=false;}
          continue;
        }
        if(ui.screen==SCR_END){
          AppGeometry a;if(!get_geometry(&g,&ui,&a))continue;
          if(point_in(sdl_rect(a.end_buttons[0]),x,y)){victory_animation_reset();if(ui.result==RES_WIN)start_new_session(&persistence,&game,&ui);else restart_current_session(&persistence,&game,&ui);}
          else if(point_in(sdl_rect(a.end_buttons[1]),x,y)){victory_animation_reset();go_home(&persistence,&game,&ui);}
          continue;
        }
        if(ui.screen==SCR_HELP||ui.screen==SCR_ABOUT){if(point_in(back_rect(&g,&ui),x,y))return_from_info(&persistence,&game,&ui);continue;}
        if(ui.pause_reasons){
          if(ui.pause_reasons&PAUSE_MANUAL){AppGeometry a;if(get_geometry(&g,&ui,&a)){if(point_in(sdl_rect(a.pause_buttons[0]),x,y)){clock_resume(&ui,PAUSE_MANUAL);(void)save_current_session(&persistence,&game,&ui);}else if(point_in(sdl_rect(a.pause_buttons[1]),x,y))go_home(&persistence,&game,&ui);}}
          continue;
        }
        Layout L=compute_layout(g.width,g.height,ui.mode);int gx=L.board.x,gy=L.board.y,side=L.board.w,cs=side/9;if(side<=0||cs<=0)continue;
        if(x>=gx&&x<gx+side&&y>=gy&&y<gy+side){
          int c=(x-gx)/cs,r=(y-gy)/cs;ui.sel_r=r;ui.sel_c=c;int i=IDX(r,c);
          if(!game.fixed[i]){int lx=x-(gx+c*cs),ly=y-(gy+r*cs),sub=cs/3;if(sub<=0)continue;int qq=lx/sub,q=ly/sub;if(qq<0)qq=0;if(q<0)q=0;if(qq>2)qq=2;if(q>2)q=2;int value=q*3+qq+1;if((right||ui.notes_mode)&&apply_player_input(&game,&ui,r,c,value,true))(void)save_current_session(&persistence,&game,&ui);}
        }else{
          SidebarRects R;compute_sidebar_rects(&L,&R);
          if(L.model.hud_count>4&&point_in(sdl_rect(L.model.hud[4]),x,y)){ui.strict_mode=!ui.strict_mode;show_toast(&ui,tr(ui.language,ui.strict_mode?T_STRICT:T_FREE));(void)save_preferences(&persistence,&ui);(void)save_current_session(&persistence,&game,&ui);}
          else if(point_in(R.btn[0],x,y)){if(confirm_box(g.win,tr(ui.language,T_RESTART),tr(ui.language,T_RESTART_PROMPT),tr(ui.language,T_RESTART),tr(ui.language,T_CANCEL))){victory_animation_reset();restart_current_session(&persistence,&game,&ui);}}
          else if(point_in(R.btn[1],x,y)){
            Mode next=(Mode)((ui.mode+1)%3);const char*next_name=tr(ui.language,mode_key(next));char msg[128];snprintf(msg,sizeof(msg),tr(ui.language,T_CHANGE_PROMPT),next_name);
            if(confirm_box(g.win,tr(ui.language,T_CHANGE_MODE),msg,tr(ui.language,T_CHANGE),tr(ui.language,T_CANCEL))){ui.mode=next;persistence.preferences.mode=next;ui.is_daily=false;ui.daily_year=ui.daily_month=ui.daily_day=0;(void)save_preferences(&persistence,&ui);game_new_difficulty(&game,next_seed(),game.difficulty);ui.sel_r=4;ui.sel_c=4;begin_fresh_play(&ui);(void)save_current_session(&persistence,&game,&ui);}
          }else if(point_in(R.btn[2],x,y)){if(game_hint(&game,ui.sel_r,ui.sel_c)){show_toast(&ui,tr(ui.language,T_HINT_USED));(void)save_current_session(&persistence,&game,&ui);}}
          else if(point_in(R.btn[3],x,y)){ui.notes_mode=!ui.notes_mode;show_toast(&ui,tr(ui.language,ui.notes_mode?T_NOTES_ON:T_NOTES_OFF));(void)save_current_session(&persistence,&game,&ui);}
          else if(point_in(R.btn[4],x,y)){int conflicts=game_conflict_count(&game);if(conflicts==0)show_toast(&ui,tr(ui.language,T_NO_CONFLICTS));else{char message[32];snprintf(message,sizeof(message),tr(ui.language,T_CONFLICTS),conflicts);show_toast(&ui,message);}}
          else if(point_in(R.btn[5],x,y)){ui.dark_theme=!ui.dark_theme;(void)save_preferences(&persistence,&ui);}
          else if(point_in(R.btn[6],x,y))open_info_from_play(&persistence,&game,&ui,SCR_HELP);
          else if(point_in(R.btn[7],x,y))open_info_from_play(&persistence,&game,&ui,SCR_ABOUT);
          else if(point_in(R.btn[8],x,y))go_home(&persistence,&game,&ui);
          else for(int n=1;n<=9;n++)if(point_in(R.pal[n-1],x,y)){if(!palette_number_complete(&game,ui.mode,n)&&apply_player_input(&game,&ui,ui.sel_r,ui.sel_c,n,ui.notes_mode))(void)save_current_session(&persistence,&game,&ui);break;}
        }
      }
    }

    if(ui.screen==SCR_PLAY&&ui.pause_reasons==0){
      bool solved=game_is_solved(&game);bool lost=game_mode_lost(ui.mode,ui.strikes,ui.strikes_max,elapsed_time(&ui),ui.time_limit_s);
      if(solved||lost){clock_pause(&ui,PAUSE_END);ui.result=solved?RES_WIN:RES_LOSE;ui.screen=SCR_END;if(solved)victory_animation_start();else victory_animation_reset();(void)save_current_session(&persistence,&game,&ui);}
    }
    if(ui.session_open&&ui.result==RES_NONE&&ui.pause_reasons==0&&now_s()-persistence.last_checkpoint>=10.0)(void)save_current_session(&persistence,&game,&ui);
    app_frame(&g,&game,&ui);
  }

  if(ui.session_open)(void)save_current_session(&persistence,&game,&ui);
  app_shutdown(&g);
  return 0;
}
