/* =================== MAIN =================== */
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
  ui.sel_r=4;ui.sel_c=4;ui.language=LANG_EN;ui.dark_theme=true;ui.mode=MODE_CLASSIC;ui.screen=SCR_TITLE;ui.prev_screen=SCR_TITLE;ui.running_since=now_s();set_mode_params(&ui);

  if(smoke_test||screenshot_dir){
    game_new(&game,110u);ui.screen=SCR_TITLE;app_frame(&g,&game,&ui);ui.screen=SCR_PLAY;app_frame(&g,&game,&ui);
    if(screenshot_dir){
      const int sizes[][2]={{640,480},{800,600},{1024,720},{1366,768},{1920,1080},{2560,1440},{3440,1440},{360,640},{390,844},{412,915}};
      const Screen screens[]={SCR_TITLE,SCR_PLAY,SCR_HELP,SCR_END};const char*names[]={"title","play","help","result"};
      if(!make_directory(screenshot_dir)){fprintf(stderr,"cannot create screenshot directory: %s\n",screenshot_dir);app_shutdown(&g);return 1;}
      for(unsigned n=0;n<sizeof(sizes)/sizeof(sizes[0]);n++)for(int screen=0;screen<4;screen++){
        SDL_SetWindowMinimumSize(g.win,1,1);SDL_SetWindowSize(g.win,sizes[n][0],sizes[n][1]);SDL_PumpEvents();SDL_GetRendererOutputSize(g.ren,&g.width,&g.height);
        if(g.width!=sizes[n][0]||g.height!=sizes[n][1]){fprintf(stderr,"renderer refused %dx%d (got %dx%d)\n",sizes[n][0],sizes[n][1],g.width,g.height);app_shutdown(&g);return 1;}
        ui.screen=screens[screen];ui.language=(Language)((n+screen)%LANG_COUNT);ui.dark_theme=((n+screen)&1)==0;ui.result=RES_WIN;
        app_render(&g,&game,&ui);char path[PATH_MAX],filename[160];
        int filename_length=snprintf(filename,sizeof(filename),"%dx%d-%s-%s-%s.bmp",g.width,g.height,names[screen],language_name(ui.language),ui.dark_theme?"dark":"light");
        if(filename_length<0||(size_t)filename_length>=sizeof(filename)||!path_join(path,sizeof(path),screenshot_dir,filename,'/')){fprintf(stderr,"screenshot path is too long: %s\n",screenshot_dir);app_shutdown(&g);return 1;}
        if(!save_frame(&g,path)){fprintf(stderr,"screenshot: %s\n",SDL_GetError());app_shutdown(&g);return 1;}
      }
      printf("rendered 40 UI review screenshots to %s\n",screenshot_dir);
    }
    app_shutdown(&g);return 0;
  }

  Persistence persistence;persistence_init(&persistence);
  ui.language=persistence.preferences.language;ui.dark_theme=persistence.preferences.dark_theme;
  ui.strict_mode=persistence.preferences.strict_mode;ui.mode=persistence.preferences.mode;set_mode_params(&ui);
  ui.has_session=persistence.enabled&&session_validate(&persistence.saved);

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
        SDL_Keycode k=e.key.keysym.sym;
        if(k==SDLK_l){ui.language=(Language)((ui.language+1)%LANG_COUNT);(void)save_preferences(&persistence,&ui);continue;}
        if(ui.screen==SCR_TITLE){
          if(k==SDLK_ESCAPE)running=false;
          else if(k==SDLK_RETURN){
            if(!ui.has_session||!saved_has_progress(&persistence.saved)||confirm_box(g.win,tr(ui.language,T_NEW_GAME),tr(ui.language,T_NEW_PROMPT),tr(ui.language,T_NEW),tr(ui.language,T_CANCEL)))start_new_session(&persistence,&game,&ui);
          }else if(k==SDLK_c&&ui.has_session)restore_saved_session(&persistence,&game,&ui);
          else if(k==SDLK_d){
            int year=0,month=0,day=0;if(local_date(&year,&month,&day)){
              if(saved_daily_matches(&persistence,year,month,day))restore_saved_session(&persistence,&game,&ui);
              else if(!ui.has_session||!saved_has_progress(&persistence.saved)||confirm_box(g.win,tr(ui.language,T_NEW_GAME),tr(ui.language,T_NEW_PROMPT),tr(ui.language,T_NEW),tr(ui.language,T_CANCEL)))
                (void)start_daily_session(&persistence,&game,&ui,year,month,day);
            }
          }else if(k==SDLK_t){ui.dark_theme=!ui.dark_theme;(void)save_preferences(&persistence,&ui);}
          else if(k==SDLK_F1){ui.prev_screen=SCR_TITLE;ui.screen=SCR_HELP;}
          else if(k==SDLK_F2){ui.prev_screen=SCR_TITLE;ui.screen=SCR_ABOUT;}
          else if(k==SDLK_m){ui.mode=(Mode)((ui.mode+1)%3);persistence.preferences.mode=ui.mode;set_mode_params(&ui);(void)save_preferences(&persistence,&ui);}
          continue;
        }
        if(ui.screen==SCR_HELP||ui.screen==SCR_ABOUT){if(k==SDLK_ESCAPE)return_from_info(&persistence,&game,&ui);continue;}
        if(ui.screen==SCR_END){
          if(k==SDLK_ESCAPE)go_home(&persistence,&game,&ui);
          else if(k==SDLK_RETURN){if(ui.result==RES_WIN)start_new_session(&persistence,&game,&ui);else restart_current_session(&persistence,&game,&ui);}
          continue;
        }
        if(ui.pause_reasons){
          if((ui.pause_reasons&PAUSE_MANUAL)&&(k==SDLK_p||k==SDLK_RETURN)){clock_resume(&ui,PAUSE_MANUAL);(void)save_current_session(&persistence,&game,&ui);}
          else if(k==SDLK_ESCAPE)go_home(&persistence,&game,&ui);
          continue;
        }
        bool shifted=(SDL_GetModState()&KMOD_SHIFT)!=0;int r=ui.sel_r,c=ui.sel_c;
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
          for(int language=0;language<LANG_COUNT;language++)if(point_in(language_segment(&g,&ui,language),x,y)){ui.language=(Language)language;(void)save_preferences(&persistence,&ui);break;}
          continue;
        }
        if(ui.screen==SCR_TITLE){
          AppGeometry a;if(!get_geometry(&g,&ui,&a))continue;
          if(point_in(sdl_rect(a.title_buttons[0]),x,y)){ui.mode=(Mode)((ui.mode+1)%3);persistence.preferences.mode=ui.mode;set_mode_params(&ui);(void)save_preferences(&persistence,&ui);}
          else if(point_in(sdl_rect(a.title_buttons[1]),x,y)){
            if(!ui.has_session||!saved_has_progress(&persistence.saved)||confirm_box(g.win,tr(ui.language,T_NEW_GAME),tr(ui.language,T_NEW_PROMPT),tr(ui.language,T_NEW),tr(ui.language,T_CANCEL)))start_new_session(&persistence,&game,&ui);
          }else if(point_in(sdl_rect(a.title_buttons[2]),x,y)&&ui.has_session)restore_saved_session(&persistence,&game,&ui);
          else if(point_in(sdl_rect(a.title_buttons[3]),x,y)){
            int year=0,month=0,day=0;if(local_date(&year,&month,&day)){
              if(saved_daily_matches(&persistence,year,month,day))restore_saved_session(&persistence,&game,&ui);
              else if(!ui.has_session||!saved_has_progress(&persistence.saved)||confirm_box(g.win,tr(ui.language,T_NEW_GAME),tr(ui.language,T_NEW_PROMPT),tr(ui.language,T_NEW),tr(ui.language,T_CANCEL)))
                (void)start_daily_session(&persistence,&game,&ui,year,month,day);
            }
          }else if(point_in(sdl_rect(a.title_buttons[4]),x,y)){ui.prev_screen=SCR_TITLE;ui.screen=SCR_HELP;}
          else if(point_in(sdl_rect(a.title_buttons[5]),x,y)){ui.prev_screen=SCR_TITLE;ui.screen=SCR_ABOUT;}
          else if(point_in(sdl_rect(a.title_buttons[6]),x,y))running=false;
          continue;
        }
        if(ui.screen==SCR_END){
          AppGeometry a;if(!get_geometry(&g,&ui,&a))continue;
          if(point_in(sdl_rect(a.end_buttons[0]),x,y)){if(ui.result==RES_WIN)start_new_session(&persistence,&game,&ui);else restart_current_session(&persistence,&game,&ui);}
          else if(point_in(sdl_rect(a.end_buttons[1]),x,y))go_home(&persistence,&game,&ui);
          continue;
        }
        if(ui.screen==SCR_HELP||ui.screen==SCR_ABOUT){if(point_in(back_rect(&g,&ui),x,y))return_from_info(&persistence,&game,&ui);continue;}
        if(ui.pause_reasons){
          if(ui.pause_reasons&PAUSE_MANUAL){AppGeometry a;if(get_geometry(&g,&ui,&a)&&point_in(sdl_rect(a.pause_button),x,y)){clock_resume(&ui,PAUSE_MANUAL);(void)save_current_session(&persistence,&game,&ui);}}
          continue;
        }
        Layout L=compute_layout(g.width,g.height,ui.mode);int gx=L.board.x,gy=L.board.y,side=L.board.w,cs=side/9;if(side<=0||cs<=0)continue;
        if(x>=gx&&x<gx+side&&y>=gy&&y<gy+side){
          int c=(x-gx)/cs,r=(y-gy)/cs;ui.sel_r=r;ui.sel_c=c;int i=IDX(r,c);
          if(!game.fixed[i]){int lx=x-(gx+c*cs),ly=y-(gy+r*cs),sub=cs/3;if(sub<=0)continue;int qq=lx/sub,q=ly/sub;if(qq<0)qq=0;if(q<0)q=0;if(qq>2)qq=2;if(q>2)q=2;int value=q*3+qq+1;if((right||ui.notes_mode)&&apply_player_input(&game,&ui,r,c,value,true))(void)save_current_session(&persistence,&game,&ui);}
        }else{
          SidebarRects R;compute_sidebar_rects(&L,&R);
          if(point_in(R.btn[0],x,y)){
            if(confirm_box(g.win,tr(ui.language,T_RESTART),tr(ui.language,T_RESTART_PROMPT),tr(ui.language,T_RESTART),tr(ui.language,T_CANCEL)))restart_current_session(&persistence,&game,&ui);
          }else if(point_in(R.btn[1],x,y)){
            Mode next=(Mode)((ui.mode+1)%3);const char*next_name=tr(ui.language,next==MODE_CLASSIC?T_CLASSIC:next==MODE_STRIKES?T_STRIKES:T_TIME_ATTACK);char msg[128];snprintf(msg,sizeof(msg),tr(ui.language,T_CHANGE_PROMPT),next_name);
            if(confirm_box(g.win,tr(ui.language,T_CHANGE_MODE),msg,tr(ui.language,T_CHANGE),tr(ui.language,T_CANCEL))){ui.mode=next;persistence.preferences.mode=next;ui.is_daily=false;ui.daily_year=ui.daily_month=ui.daily_day=0;(void)save_preferences(&persistence,&ui);game_new_difficulty(&game,next_seed(),persistence.preferences.difficulty);ui.sel_r=4;ui.sel_c=4;begin_fresh_play(&ui);(void)save_current_session(&persistence,&game,&ui);}
          }else if(point_in(R.btn[2],x,y)){if(game_hint(&game,ui.sel_r,ui.sel_c)){show_toast(&ui,tr(ui.language,T_HINT_USED));(void)save_current_session(&persistence,&game,&ui);}}
          else if(point_in(R.btn[3],x,y)){ui.notes_mode=!ui.notes_mode;show_toast(&ui,tr(ui.language,ui.notes_mode?T_NOTES_ON:T_NOTES_OFF));(void)save_current_session(&persistence,&game,&ui);}
          else if(point_in(R.btn[4],x,y)){int conflicts=game_conflict_count(&game);if(conflicts==0)show_toast(&ui,tr(ui.language,T_NO_CONFLICTS));else{char message[32];snprintf(message,sizeof(message),tr(ui.language,T_CONFLICTS),conflicts);show_toast(&ui,message);}}
          else if(point_in(R.btn[5],x,y)){ui.dark_theme=!ui.dark_theme;(void)save_preferences(&persistence,&ui);}
          else if(point_in(R.btn[6],x,y))open_info_from_play(&persistence,&game,&ui,SCR_HELP);
          else if(point_in(R.btn[7],x,y))open_info_from_play(&persistence,&game,&ui,SCR_ABOUT);
          else if(point_in(R.btn[8],x,y))go_home(&persistence,&game,&ui);
          else for(int n=1;n<=9;n++)if(point_in(R.pal[n-1],x,y)&&apply_player_input(&game,&ui,ui.sel_r,ui.sel_c,n,ui.notes_mode)){(void)save_current_session(&persistence,&game,&ui);break;}
        }
      }
    }

    if(ui.screen==SCR_PLAY&&ui.pause_reasons==0){
      bool solved=game_is_solved(&game);bool lost=game_mode_lost(ui.mode,ui.strikes,ui.strikes_max,elapsed_time(&ui),ui.time_limit_s);
      if(solved||lost){clock_pause(&ui,PAUSE_END);ui.result=solved?RES_WIN:RES_LOSE;ui.screen=SCR_END;(void)save_current_session(&persistence,&game,&ui);}
    }
    if(ui.session_open&&ui.result==RES_NONE&&ui.pause_reasons==0&&now_s()-persistence.last_checkpoint>=10.0)(void)save_current_session(&persistence,&game,&ui);
    app_frame(&g,&game,&ui);
  }

  if(ui.session_open)(void)save_current_session(&persistence,&game,&ui);
  app_shutdown(&g);return 0;
}

