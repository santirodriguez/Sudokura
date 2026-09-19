/* =================== MAIN =================== */
static bool home_focus_available(const UI *ui, int focus) {
  return focus != HOME_FOCUS_CONTINUE || ui->has_session;
}

static void home_move_focus(const UI *ui, int direction) {
  int next = home_focus;
  do {
    next = (next + direction + HOME_FOCUS_COUNT) % HOME_FOCUS_COUNT;
  } while (!home_focus_available(ui, next));
  home_focus = next;
}

static void ui_button_click(void) { audio_play_effect(AUDIO_EFFECT_CLICK); }

typedef enum {
  SAVE_GATE_BACK = 0,
  SAVE_GATE_SAVED,
  SAVE_GATE_DISCARD
} SaveGateResult;

static bool request_exit(Persistence *p, const Game *game, UI *ui, Gfx *g);

static SaveGateResult resolve_unsaved(Persistence *p, Gfx *g, UI *ui,
                                      unsigned dirty_mask,
                                      TextKey prompt_key) {
  if (!p || !g || !ui ||
      (p->save_policy.dirty_mask & dirty_mask) == 0)
    return SAVE_GATE_SAVED;
  if (persistence_flush(p)) return SAVE_GATE_SAVED;

  for (;;) {
    int choice = choice_box(g->win, tr(ui->language, T_UNSAVED_TITLE),
                            tr(ui->language, prompt_key),
                            tr(ui->language, T_BACK),
                            tr(ui->language, T_RETRY_SAVE),
                            tr(ui->language, T_DISCARD));
    if (choice == 0) return SAVE_GATE_BACK;
    if (choice == 2) return SAVE_GATE_DISCARD;
    if (persistence_flush(p)) return SAVE_GATE_SAVED;
  }
}

static void home_set_language(Persistence *p, UI *ui, Language language) {
  ui->language = language;
  (void)save_preferences(p, ui);
}

static void home_cycle_language(Persistence *p, UI *ui, int delta) {
  int next = ((int)ui->language + delta + LANG_COUNT) % LANG_COUNT;
  home_set_language(p, ui, (Language)next);
}

static void home_set_mode(Persistence *p, UI *ui, Mode mode) {
  ui->mode = mode;
  p->preferences.mode = mode;
  set_mode_params(ui);
  (void)save_preferences(p, ui);
}

static void home_cycle_mode(Persistence *p, UI *ui, int delta) {
  home_set_mode(p, ui, (Mode)(((int)ui->mode + delta + 3) % 3));
}

static void home_set_difficulty(Persistence *p, UI *ui,
                                GameDifficulty difficulty) {
  home_difficulty = difficulty;
  p->preferences.difficulty = difficulty;
  (void)save_preferences(p, ui);
}

static void home_cycle_difficulty(Persistence *p, UI *ui, int delta) {
  home_set_difficulty(
      p, ui,
      (GameDifficulty)(((int)home_difficulty + delta + DIFFICULTY_COUNT) %
                       DIFFICULTY_COUNT));
}

static void verify_current_entries(Persistence *p, Game *game, UI *ui) {
  if (!game || !ui) return;
  int incorrect = apply_verify_action(game, ui);
  if (p) (void)stage_current_session(p, game, ui);
  if (incorrect == 0) {
    show_toast(ui, tr(ui->language, T_NO_CONFLICTS));
  } else {
    char message[64];
    snprintf(message, sizeof(message), tr(ui->language, T_CONFLICTS),
             incorrect);
    show_toast(ui, message);
  }
}

static bool apply_current_logic_hint(Persistence *p, Game *game, UI *ui) {
  if (!apply_logical_hint_action(game, ui)) return false;
  if (p) (void)stage_current_session(p, game, ui);
  return true;
}

static bool reveal_selected_cell(Persistence *p, Game *game, UI *ui) {
  if (!game || !ui) return false;
  if (!apply_reveal_action(game, ui, ui->sel_r, ui->sel_c)) return false;
  if (p) (void)stage_current_session(p, game, ui);
  return true;
}

static bool replace_saved_session_confirmed(Persistence *p, UI *ui, Gfx *g,
                                            bool daily) {
  if (!p) return true;
  unsigned dirty_bit = daily ? SAVE_DIRTY_DAILY : SAVE_DIRTY_NORMAL;
  SaveGateResult gate =
      resolve_unsaved(p, g, ui, dirty_bit, T_UNSAVED_REPLACE);
  if (gate == SAVE_GATE_BACK) return false;
  if (gate == SAVE_GATE_DISCARD) return true;

  const ProfileSlot *slot = daily ? &p->profile.daily : &p->profile.normal;
  const SessionState *saved = profile_slot_session(slot);
  return !saved || !session_has_meaningful_progress(saved) ||
         confirm_box(g->win, tr(ui->language, T_NEW_GAME),
                     tr(ui->language, T_NEW_PROMPT), tr(ui->language, T_NEW),
                     tr(ui->language, T_CANCEL));
}

static bool restart_current_session_confirmed(Persistence *p,
                                              const Game *game, UI *ui,
                                              Gfx *g) {
  if (!p || !game || !ui || !g) return false;
  unsigned dirty_bit = ui->is_daily ? SAVE_DIRTY_DAILY : SAVE_DIRTY_NORMAL;
  SaveGateResult gate =
      resolve_unsaved(p, g, ui, dirty_bit, T_UNSAVED_REPLACE);
  if (gate == SAVE_GATE_BACK) return false;

  if (!game_has_meaningful_progress(game, ui->mistakes, ui->strikes))
    return true;
  clock_pause(ui, PAUSE_MODAL);
  bool confirmed =
      confirm_box(g->win, tr(ui->language, T_RESTART),
                  tr(ui->language, T_RESTART_PROMPT),
                  tr(ui->language, T_RESTART), tr(ui->language, T_CANCEL));
  clock_resume(ui, PAUSE_MODAL);
  return confirmed;
}

typedef struct {
  Gfx *gfx;
  const Game *game;
  UI *ui;
  unsigned rendered_attempt;
} GenerationUiContext;

static bool generation_ui_continue(void *userdata, unsigned attempt,
                                   unsigned max_attempts) {
  GenerationUiContext *context = (GenerationUiContext *)userdata;
  if (!context || !context->gfx || !context->ui) return false;

  SDL_PumpEvents();
  if (SDL_HasEvent(SDL_QUIT)) return false;
  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  if (keys && keys[SDL_SCANCODE_ESCAPE]) return false;

  unsigned shown_attempt = attempt + 1u;
  if (context->rendered_attempt != shown_attempt) {
    context->rendered_attempt = shown_attempt;
    context->ui->generation_attempt = shown_attempt;
    context->ui->generation_max_attempts = max_attempts;
    app_frame(context->gfx, context->game, context->ui);
  }
  return true;
}

static GameGenerationResult generate_normal_with_ui(
    Persistence *p, Game *game, UI *ui, Gfx *g) {
  if (!p || !game || !ui || !g) return GAME_GENERATION_INVALID;
  GenerationUiContext context = {g, game, ui, 0};
  GameGenerationControl control = {generation_ui_continue, &context};
  ui->generating = true;
  ui->generation_attempt = 1;
  ui->generation_max_attempts =
      game_generation_attempt_budget(p->preferences.difficulty);
  app_frame(g, game, ui);
  GameGenerationResult result = start_new_session(p, game, ui, &control);
  if (result == GAME_GENERATION_CANCELLED &&
      !SDL_HasEvent(SDL_QUIT))
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
  ui->generating = false;
  ui->generation_attempt = 0;
  ui->generation_max_attempts = 0;
  return result;
}

static GameGenerationResult generate_daily_with_ui(
    Persistence *p, Game *game, UI *ui, Gfx *g,
    int year, int month, int day) {
  if (!p || !game || !ui || !g) return GAME_GENERATION_INVALID;
  GenerationUiContext context = {g, game, ui, 0};
  GameGenerationControl control = {generation_ui_continue, &context};
  ui->generating = true;
  ui->generation_attempt = 1;
  ui->generation_max_attempts =
      game_generation_attempt_budget(DIFFICULTY_MEDIUM);
  app_frame(g, game, ui);
  GameGenerationResult result =
      start_daily_session(p, game, ui, year, month, day, &control);
  if (result == GAME_GENERATION_CANCELLED &&
      !SDL_HasEvent(SDL_QUIT))
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
  ui->generating = false;
  ui->generation_attempt = 0;
  ui->generation_max_attempts = 0;
  return result;
}

static bool start_new_game_with_ui(Persistence *p, Game *game, UI *ui,
                                   Gfx *g) {
  for (;;) {
    GameGenerationResult result = generate_normal_with_ui(p, game, ui, g);
    if (result == GAME_GENERATION_OK) return true;
    if (result == GAME_GENERATION_CANCELLED) {
      show_toast(ui, tr(ui->language, T_GENERATION_CANCELLED));
      return false;
    }
    if (result != GAME_GENERATION_EXHAUSTED) {
      (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     tr(ui->language, T_GENERATION_FAILED),
                                     tr(ui->language, T_PROFILE_IO_ERROR),
                                     g->win);
      return false;
    }
    bool retry = confirm_box(g->win, tr(ui->language, T_GENERATION_FAILED),
                             tr(ui->language, T_GENERATION_FAILED_BODY),
                             tr(ui->language, T_RETRY),
                             tr(ui->language, T_CANCEL));
    if (!retry) return false;
  }
}

static bool start_daily_game_with_ui(Persistence *p, Game *game, UI *ui,
                                     Gfx *g, int year, int month, int day) {
  GameGenerationResult result =
      generate_daily_with_ui(p, game, ui, g, year, month, day);
  if (result == GAME_GENERATION_OK) return true;
  if (result == GAME_GENERATION_CANCELLED) {
    show_toast(ui, tr(ui->language, T_GENERATION_CANCELLED));
    return false;
  }
  (void)SDL_ShowSimpleMessageBox(
      SDL_MESSAGEBOX_WARNING, tr(ui->language, T_GENERATION_FAILED),
      tr(ui->language, T_GENERATION_FAILED_BODY), g->win);
  return false;
}

static void home_new_game(Persistence *p, Game *game, UI *ui, Gfx *g) {
  if (replace_saved_session_confirmed(p, ui, g, false)) {
    victory_animation_reset();
    (void)start_new_game_with_ui(p, game, ui, g);
  }
}

static void home_daily_game(Persistence *p, Game *game, UI *ui, Gfx *g) {
  if (!p || !game || !ui || !g) return;
  victory_animation_reset();

  const SessionState *saved = profile_slot_session(&p->profile.daily);
  if (saved && session_can_continue(saved)) {
    restore_saved_session(p, game, ui, true);
    return;
  }

  int year = 0, month = 0, day = 0;
  if (!local_date(&year, &month, &day)) return;
  if (saved_daily_matches(p, year, month, day)) {
    restore_saved_session(p, game, ui, true);
    return;
  }
  if (replace_saved_session_confirmed(p, ui, g, true))
    (void)start_daily_game_with_ui(p, game, ui, g, year, month, day);
}

static void home_activate(Persistence *p, Game *game, UI *ui, Gfx *g,
                          bool *running) {
  ui_button_click();
  switch (home_focus) {
    case HOME_FOCUS_LANGUAGE:
      home_cycle_language(p, ui, 1);
      break;
    case HOME_FOCUS_MODE:
      home_cycle_mode(p, ui, 1);
      break;
    case HOME_FOCUS_DIFFICULTY:
      home_cycle_difficulty(p, ui, 1);
      break;
    case HOME_FOCUS_NEW:
      home_new_game(p, game, ui, g);
      break;
    case HOME_FOCUS_CONTINUE:
      if (ui->has_session) {
        victory_animation_reset();
        restore_saved_session(p, game, ui, false);
      }
      break;
    case HOME_FOCUS_DAILY:
      home_daily_game(p, game, ui, g);
      break;
    case HOME_FOCUS_HELP:
      info_keyboard_focus=INFO_FOCUS_BACK;
      (void)app_open_aux(ui, SCR_HELP);
      break;
    case HOME_FOCUS_ABOUT:
      info_keyboard_focus=INFO_FOCUS_BACK;
      (void)app_open_aux(ui, SCR_ABOUT);
      break;
    case HOME_FOCUS_SETTINGS:
      ui_settings_open(p, game, ui);
      break;
    case HOME_FOCUS_QUIT:
      if (request_exit(p, game, ui, g)) *running = false;
      break;
    default:
      break;
  }
}

static const char *about_url(int index) {
  static const char *const urls[3] = {
      "https://github.com/santirodriguez",
      "https://github.com/santirodriguez/Sudokura",
      "https://santiagorodriguez.com",
  };
  return index >= 0 && index < 3 ? urls[index] : NULL;
}

static void open_about_url(Gfx *g, UI *ui, int index) {
  const char *url = about_url(index);
  if (!url) return;
  ui_button_click();
  if (SDL_OpenURL(url) != 0)
    (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                   tr(ui->language, T_ABOUT),
                                   tr(ui->language, T_LINK_ERROR), g->win);
}

static void info_focus_reset(void){info_keyboard_focus=INFO_FOCUS_BACK;help_scroll_reset();}
static void info_move_focus(const UI*ui,int direction){
  int count=ui&&ui->screen==SCR_ABOUT?INFO_FOCUS_COUNT:1;
  info_keyboard_focus=input_focus_step(info_keyboard_focus,count,direction);
}
static bool activate_info_focus(Persistence*p,const Game*game,UI*ui,Gfx*g){
  if(!ui||!g)return false;
  if(info_keyboard_focus==INFO_FOCUS_BACK){ui_button_click();return_from_info(p,game,ui);return true;}
  if(ui->screen!=SCR_ABOUT)return false;
  if(info_keyboard_focus==INFO_FOCUS_STUDY){ui_open_url("https://doi.org/10.1002/gps.5085");return true;}
  int link=info_keyboard_focus-INFO_FOCUS_LINK_0;
  if(link>=0&&link<3){open_about_url(g,ui,link);return true;}
  if(link==3){ui_open_url(ui_support_url(ui->language));return true;}
  return false;
}
static void activate_result_focus(Persistence*p,Game*game,UI*ui,Gfx*g){
  ui_button_click();victory_animation_reset();
  if(result_keyboard_focus==1){go_home(p,game,ui);return;}
  if(ui->result==RES_WIN){if(replace_saved_session_confirmed(p,ui,g,false))(void)start_new_game_with_ui(p,game,ui,g);}
  else if(restart_current_session_confirmed(p,game,ui,g))restart_current_session(p,game,ui);
}
static void activate_pause_focus(Persistence*p,const Game*game,UI*ui){
  ui_button_click();
  if(pause_keyboard_focus==0){if(resume_user_pause(ui))(void)checkpoint_current_session(p,game,ui);}
  else go_home(p,game,ui);
}
static bool activate_hint_focus(Persistence*p,Game*game,UI*ui){
  if(!ui||!ui->hint_preview_active)return false;
  if(hint_keyboard_focus==HINT_FOCUS_CANCEL){ui_button_click();clear_hint_preview(ui);return true;}
  if(hint_keyboard_focus==HINT_FOCUS_REVEAL){ui_button_click();(void)reveal_selected_cell(p,game,ui);return true;}
  ui_button_click();
  if(ui->hint_preview.status==HUMAN_HINT_LOGICAL)(void)apply_current_logic_hint(p,game,ui);
  else verify_current_entries(p,game,ui);
  return true;
}
static bool activate_play_focus(Persistence*p,Game*game,UI*ui,Gfx*g){
  if(!play_focus_enabled(game,ui,play_keyboard_focus))return false;
  switch(play_keyboard_focus){
    case PLAY_FOCUS_MENU:ui_button_click();go_home(p,game,ui);return true;
    case PLAY_FOCUS_PAUSE:ui_button_click();pause_keyboard_focus=0;clock_pause(ui,PAUSE_MANUAL);(void)checkpoint_current_session(p,game,ui);return true;
    case PLAY_FOCUS_UNDO:
    case PLAY_FOCUS_REDO:{AppActionKind kind=play_keyboard_focus==PLAY_FOCUS_REDO?APP_ACTION_REDO:APP_ACTION_UNDO;ui_button_click();if(apply_history_action(game,ui,kind)){clear_hint_preview(ui);(void)stage_current_session(p,game,ui);}return true;}
    case PLAY_FOCUS_RESTART:ui_button_click();if(restart_current_session_confirmed(p,game,ui,g)){victory_animation_reset();restart_current_session(p,game,ui);}return true;
    case PLAY_FOCUS_CLEAR:ui_button_click();if(clear_selected_available(game,ui)&&apply_player_input(game,ui,ui->sel_r,ui->sel_c,0,false)){clear_hint_preview(ui);(void)stage_current_session(p,game,ui);}return true;
    case PLAY_FOCUS_HINT:ui_button_click();(void)prepare_hint_preview(game,ui);if(ui->hint_preview_active)hint_focus_reset(ui);return true;
    case PLAY_FOCUS_NOTES:ui_button_click();ui->notes_mode=!ui->notes_mode;show_toast(ui,tr(ui->language,ui->notes_mode?T_NOTES_ON:T_NOTES_OFF));(void)stage_current_session(p,game,ui);return true;
    case PLAY_FOCUS_VERIFY:ui_button_click();verify_current_entries(p,game,ui);return true;
    case PLAY_FOCUS_SETTINGS:ui_button_click();ui_settings_open(p,game,ui);return true;
    case PLAY_FOCUS_HELP:ui_button_click();info_focus_reset();open_info_from_play(p,game,ui,SCR_HELP);return true;
    case PLAY_FOCUS_STRICT:ui_button_click();ui->strict_mode=!ui->strict_mode;show_toast(ui,tr(ui->language,ui->strict_mode?T_STRICT:T_FREE));(void)save_preferences(p,ui);(void)stage_current_session(p,game,ui);return true;
    case PLAY_FOCUS_SAVE_RETRY:ui_button_click();(void)persistence_flush(p);return true;
    default:
      if(play_keyboard_focus>=PLAY_FOCUS_NUMBER_1){int number=play_keyboard_focus-PLAY_FOCUS_NUMBER_1+1;if(apply_player_input(game,ui,ui->sel_r,ui->sel_c,number,ui->notes_mode)){clear_hint_preview(ui);(void)stage_current_session(p,game,ui);}return true;}
      return false;
  }
}

static bool open_global_info_key(Persistence *persistence, const Game *game,
                                 UI *ui, SDL_Keycode key) {
  InputInfoShortcut shortcut = input_info_shortcut(key);
  if (shortcut == INPUT_INFO_NONE) return false;
  Screen target =
      shortcut == INPUT_INFO_HELP ? SCR_HELP : SCR_ABOUT;

  ui_button_click();
  info_focus_reset();
  if (ui->screen == target) return true;
  if (ui->screen == SCR_PLAY)
    open_info_from_play(persistence, game, ui, target);
  else
    (void)app_open_aux(ui, target);
  return true;
}

static bool settle_terminal(Persistence *persistence, Game *game, UI *ui) {
  if (!ui || ui->screen != SCR_PLAY || ui->result != RES_NONE ||
      ui->pause_reasons != 0)
    return false;
  AppActionOutcome outcome = app_check_terminal(game, ui, elapsed_time(ui));
  if (!outcome.terminal) return false;
  finish_terminal_action(ui);
  result_keyboard_focus=0;
  (void)checkpoint_current_session(persistence, game, ui);
  return true;
}

static const char *persistence_startup_error(const Persistence *p,
                                             Language language) {
  if (!p) return tr(language, T_PROFILE_IO_ERROR);
  if (p->lock_status == STORE_LOCK_BUSY)
    return tr(language, T_PROFILE_IN_USE);
  if (p->lock_status != STORE_LOCK_ACQUIRED)
    return tr(language, T_PROFILE_IO_ERROR);
  if (p->profile_status == STORE_CORRUPT)
    return tr(language, T_PROFILE_CORRUPT);
  if (p->profile_status == STORE_INCOMPATIBLE)
    return tr(language, T_PROFILE_INCOMPATIBLE);
  if (p->profile_status != STORE_OK)
    return tr(language, T_PROFILE_IO_ERROR);
  return NULL;
}

static bool saved_window_display(const Preferences *preferences,
                                 int *display_index,
                                 SDL_Rect *usable_out) {
  if (!preferences || preferences->window_x == INT32_MIN ||
      preferences->window_y == INT32_MIN)
    return false;
  int count = SDL_GetNumVideoDisplays();
  for (int i = 0; i < count; ++i) {
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(i, &usable) != 0) continue;
    int probe_w=(int)preferences->window_width;
    int probe_h=(int)preferences->window_height;
    if(probe_w>usable.w)probe_w=usable.w;
    if(probe_h>usable.h)probe_h=usable.h;
    DesktopRect saved = {
        preferences->window_x, preferences->window_y, probe_w, probe_h};
    DesktopRect display = {usable.x, usable.y, usable.w, usable.h};
    if (desktop_rect_has_visible_area(saved, display, 64)) {
      if (display_index) *display_index = i;
      if (usable_out) *usable_out = usable;
      return true;
    }
  }
  return false;
}

static void apply_window_preferences(Gfx *g, const Preferences *preferences) {
  if (!g || !g->win || !preferences) return;
  int display = 0;
  SDL_Rect usable = {0, 0, 0, 0};
  bool saved_visible = saved_window_display(preferences, &display, &usable);
  if (!saved_visible && SDL_GetDisplayUsableBounds(display, &usable) != 0)
    usable = (SDL_Rect){0, 0, 1920, 1080};

  int width = (int)preferences->window_width;
  int height = (int)preferences->window_height;
  (void)geometry_normalize_window_size(width, height, &width, &height);
  if (usable.w > 0 && width > usable.w) width = usable.w;
  if (usable.h > 0 && height > usable.h) height = usable.h;
  (void)geometry_normalize_window_size(width, height, &width, &height);
  SDL_SetWindowSize(g->win, width, height);

  if (saved_visible) {
    SDL_SetWindowPosition(g->win, preferences->window_x,
                         preferences->window_y);
  } else {
    SDL_SetWindowPosition(g->win, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                          SDL_WINDOWPOS_CENTERED_DISPLAY(display));
  }
  SDL_PumpEvents();
  (void)gfx_refresh_metrics(g);
  gfx_capture_restored_bounds(g);
  if (preferences->window_maximized) {
    SDL_MaximizeWindow(g->win);
    SDL_PumpEvents();
    (void)gfx_refresh_metrics(g);
  }
}

static void capture_window_preferences(Persistence *p, Gfx *g, UI *ui) {
  if (!p || !p->enabled || !g || !g->win || !ui) return;
  gfx_capture_restored_bounds(g);
  Uint32 flags = SDL_GetWindowFlags(g->win);
  if (g->restored_bounds_valid) {
    p->preferences.window_x = g->restored_x;
    p->preferences.window_y = g->restored_y;
    p->preferences.window_width = (uint32_t)g->restored_width;
    p->preferences.window_height = (uint32_t)g->restored_height;
  }
  p->preferences.window_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
  (void)save_preferences(p, ui);
}

static bool request_exit(Persistence *p, const Game *game, UI *ui, Gfx *g) {
  if (!p || !ui || !g) return true;
  if (ui->session_open && game) (void)stage_current_session(p, game, ui);
  capture_window_preferences(p, g, ui);
  SaveGateResult gate =
      resolve_unsaved(p, g, ui, UINT_MAX, T_UNSAVED_EXIT);
  return gate != SAVE_GATE_BACK;
}

static double wall_clock_ms(void){
  Uint64 frequency=SDL_GetPerformanceFrequency();
  if(frequency==0)return 0.0;
  Uint64 counter=SDL_GetPerformanceCounter();
  return (double)counter*1000.0/(double)frequency;
}

static void sort_doubles(double*values,int count){
  for(int i=1;i<count;i++){
    double value=values[i];int j=i-1;
    while(j>=0&&values[j]>value){values[j+1]=values[j];j--;}
    values[j+1]=value;
  }
}

static long desktop_benchmark_rss_kb(void){
#if defined(__linux__)
  FILE*f=fopen("/proc/self/statm","r");
  if(!f)return -1;
  long total=0,resident=0;
  int ok=fscanf(f,"%ld %ld",&total,&resident);
  fclose(f);
  if(ok!=2)return -1;
  long page=sysconf(_SC_PAGESIZE);
  if(page<=0)return -1;
  return resident*(page/1024);
#else
  return -1;
#endif
}

static int desktop_wait_timeout_ms(const Gfx*g,const UI*ui){
  if(!g||!ui)return 250;
  if(victory_animation_active||ui->toast_on)return g->software_renderer?34:16;
  if(ui->screen==SCR_PLAY&&ui->session_open&&ui->result==RES_NONE&&
     !app_pause_hides_play(ui)&&!ui->reduced_motion)
    return g->software_renderer?34:16;
  if(ui->screen==SCR_PLAY&&ui->session_open&&ui->result==RES_NONE&&
     ui->pause_reasons==0)
    return 250;
  return 250;
}

static bool desktop_dynamic_frame_due(const UI*ui){
  if(!ui)return false;
  if(victory_animation_active||ui->toast_on)return true;
  if(ui->screen==SCR_PLAY&&ui->session_open&&ui->result==RES_NONE&&
     !app_pause_hides_play(ui)&&!ui->reduced_motion)
    return true;
  if(ui->screen==SCR_PLAY&&ui->session_open&&ui->result==RES_NONE&&
     ui->pause_reasons==0)
    return true;
  return false;
}

static int benchmark_editable_cell(const Game*game){
  if(!game)return -1;
  for(int i=0;i<81;i++)if(!game->fixed[i])return i;
  return -1;
}

static int run_desktop_benchmark(Gfx*g,Game*game,UI*ui,double process_start_ms){
  if(!g||!game||!ui)return 1;
  ui->screen=SCR_TITLE;ui->prev_screen=SCR_TITLE;ui->reduced_motion=true;
  app_frame(g,game,ui);
  double startup_ms=wall_clock_ms()-process_start_ms;

  const GameDifficulty difficulties[3]={
      DIFFICULTY_EASY,DIFFICULTY_MEDIUM,DIFFICULTY_HARD};
  const uint64_t seed_base[3]={
      UINT64_C(0x60010001),UINT64_C(0x60020002),UINT64_C(0x60030003)};
  double generation_samples[3][5];
  double generation_p50[3]={0,0,0};
  double generation_max[3]={0,0,0};
  Game benchmark_game;
  memset(&benchmark_game,0,sizeof(benchmark_game));
  for(int difficulty=0;difficulty<3;difficulty++){
    for(int sample=0;sample<5;sample++){
      Game generated;
      uint64_t seed=seed_base[difficulty]+(uint64_t)sample*UINT64_C(0x101);
      double start=wall_clock_ms();
      if(!game_new_difficulty_revision(&generated,seed,difficulties[difficulty],
                                       SUDOKURA_GENERATOR_REVISION)){
        fprintf(stderr,"desktop benchmark generation failed at %d/%d\n",
                difficulty,sample);
        return 1;
      }
      generation_samples[difficulty][sample]=wall_clock_ms()-start;
      if(difficulty==1&&sample==2)benchmark_game=generated;
    }
    sort_doubles(generation_samples[difficulty],5);
    generation_p50[difficulty]=generation_samples[difficulty][2];
    generation_max[difficulty]=generation_samples[difficulty][4];
  }

  *game=benchmark_game;
  ui->mode=MODE_CLASSIC;ui->strict_mode=false;ui->sel_r=4;ui->sel_c=4;
  ui->is_daily=false;ui->result=RES_NONE;ui->pause_reasons=0;
  begin_fresh_play(ui);ui->reduced_motion=true;
  int cell=benchmark_editable_cell(game);
  if(cell<0)return 1;
  ui->sel_r=cell/9;ui->sel_c=cell%9;
  app_frame(g,game,ui);

  double input_samples[31];
  for(int i=0;i<31;i++){
    GameEdit edit;
    int value=game->solution[cell];
    double start=wall_clock_ms();
    GameInputResult result=game_apply_input_recorded(
        game,cell/9,cell%9,value,false,false,false,&edit);
    if(result!=GAME_INPUT_CORRECT)return 1;
    app_frame(g,game,ui);
    input_samples[i]=wall_clock_ms()-start;
    if(!game_apply_edit(game,&edit,false))return 1;
  }
  sort_doubles(input_samples,31);

  ProfileData profile;
  profile_defaults(&profile);
  SessionState session;
  memset(&session,0,sizeof(session));
  session.game=*game;session.mode=MODE_CLASSIC;
  session.selected_row=ui->sel_r;session.selected_column=ui->sel_c;
  session.status=SESSION_ACTIVE;session.elapsed_ms=UINT64_C(123456);
  if(!profile_slot_set_runtime(&profile.normal,&session,false))return 1;
  const char*save_path=".sudokura-desktop-benchmark.dat";
  const char*backup_path=".sudokura-desktop-benchmark.dat.bak";
  (void)store_remove_file(save_path);(void)store_remove_file(backup_path);
  double save_samples[7];
  for(int i=0;i<7;i++){
    double start=wall_clock_ms();
    if(profile_save_runtime_file(save_path,backup_path,&profile)!=STORE_OK)
      return 1;
    save_samples[i]=wall_clock_ms()-start;
  }
  sort_doubles(save_samples,7);
  (void)store_remove_file(save_path);(void)store_remove_file(backup_path);

  ui->screen=SCR_TITLE;ui->session_open=false;g->dirty=true;app_frame(g,game,ui);
  clock_t cpu_start=clock();double idle_start=wall_clock_ms();
  while(wall_clock_ms()-idle_start<1000.0){
    SDL_Event event;
    int timeout=desktop_wait_timeout_ms(g,ui);
    double remaining=1000.0-(wall_clock_ms()-idle_start);
    if(remaining<timeout)timeout=(int)remaining;
    if(timeout<1)timeout=1;
    if(SDL_WaitEventTimeout(&event,timeout)){
      while(SDL_PollEvent(&event)){}
    }
    app_tick(g,game,ui);
  }
  double idle_wall=wall_clock_ms()-idle_start;
  double idle_cpu_ms=(double)(clock()-cpu_start)*1000.0/CLOCKS_PER_SEC;
  double idle_cpu_percent=idle_wall>0.0?idle_cpu_ms*100.0/idle_wall:0.0;

  long rss_before=desktop_benchmark_rss_kb();
  long rss_after_warmup=-1,rss_after_repeat=-1;
  const int sizes[][2]={{640,480},{1024,768},{1366,768},{360,640}};
  for(int pass=0;pass<2;pass++){
    for(int i=0;i<48;i++){
      ui->language=(Language)(i%LANG_COUNT);ui->dark_theme=(i&1)!=0;
      SDL_SetWindowSize(g->win,sizes[i%4][0],sizes[i%4][1]);
      SDL_PumpEvents();
      apply_window_resize(g,sizes[i%4][0],sizes[i%4][1]);
      SDL_Event discarded;
      while(SDL_PollEvent(&discarded)){}
      app_frame(g,game,ui);
    }
    if(pass==0)rss_after_warmup=desktop_benchmark_rss_kb();
    else rss_after_repeat=desktop_benchmark_rss_kb();
  }

  printf("desktop_benchmark_version=2\n");
  printf("startup_first_home_ms=%.3f\n",startup_ms);
  printf("input_render_p50_ms=%.3f\n",input_samples[15]);
  printf("idle_cpu_percent=%.3f\n",idle_cpu_percent);
  printf("generation_easy_p50_ms=%.3f\n",generation_p50[0]);
  printf("generation_easy_max_ms=%.3f\n",generation_max[0]);
  printf("generation_medium_p50_ms=%.3f\n",generation_p50[1]);
  printf("generation_medium_max_ms=%.3f\n",generation_max[1]);
  printf("generation_hard_p50_ms=%.3f\n",generation_p50[2]);
  printf("generation_hard_max_ms=%.3f\n",generation_max[2]);
  printf("save_p50_ms=%.3f\n",save_samples[3]);
  printf("rss_before_kb=%ld\n",rss_before);
  printf("rss_after_warmup_kb=%ld\n",rss_after_warmup);
  printf("rss_after_repeat_kb=%ld\n",rss_after_repeat);
  printf("rss_repeat_delta_kb=%ld\n",
         rss_after_warmup>=0&&rss_after_repeat>=0
             ?rss_after_repeat-rss_after_warmup:-1L);
  printf("text_cache_entries=%d\n",text_cache_entry_count());
  printf("text_cache_capacity=%d\n",TEXT_CACHE_CAPACITY);
  printf("renderer=%s\n",g->software_renderer?"software":"accelerated");
  printf("scale_milli=%d\n",g->metrics.scale_milli);
  return 0;
}

int main(int argc, char **argv) {
  double process_start_ms=wall_clock_ms();
  const char *font_cli = NULL;
  const char *screenshot_dir = NULL;
  bool smoke_test = false;
  bool desktop_benchmark = false;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--font") && i + 1 < argc)
      font_cli = argv[++i];
    else if (!strcmp(argv[i], "--smoke-test"))
      smoke_test = true;
    else if (!strcmp(argv[i], "--desktop-benchmark"))
      desktop_benchmark = true;
    else if (!strcmp(argv[i], "--render-screenshots") && i + 1 < argc)
      screenshot_dir = argv[++i];
  }

  Gfx g;
  if (!app_init(&g, font_cli)) return 1;
  Game game;
  memset(&game, 0, sizeof(game));
  UI ui;
  app_state_init(&ui);
  app_clock_reset(&ui, now_ms());
  set_mode_params(&ui);
  home_difficulty = DIFFICULTY_MEDIUM;

  if(desktop_benchmark){
    int result=run_desktop_benchmark(&g,&game,&ui,process_start_ms);
    app_shutdown(&g);
    return result;
  }

  if (smoke_test || screenshot_dir) {
    if (!game_new(&game, 110u)) {
      fprintf(stderr, "smoke fixture generation failed\n");
      app_shutdown(&g);
      return 1;
    }
    ui.has_session = true;
    ui.screen = SCR_TITLE;
    app_frame(&g, &game, &ui);
    ui.screen = SCR_PLAY;
    app_frame(&g, &game, &ui);
    if (screenshot_dir) {
      enum {
        REVIEW_HOME_EMPTY=0, REVIEW_HOME_SAVE, REVIEW_NOTES, REVIEW_CONFLICT,
        REVIEW_HINT, REVIEW_SAVE_ERROR, REVIEW_PAUSE, REVIEW_LOSS, REVIEW_WIN,
        REVIEW_HELP, REVIEW_SETTINGS, REVIEW_ABOUT, REVIEW_STATE_COUNT
      };
      const int sizes[][2]={{640,480},{1024,768},{1366,768},{360,640},{1920,1080}};
      const char*state_names[REVIEW_STATE_COUNT]={
        "home-empty","home-save","notes","conflict","hint","save-error",
        "pause","loss","win","help","settings","about"
      };
      if(!make_directory(screenshot_dir)){
        fprintf(stderr,"cannot create screenshot directory: %s\n",screenshot_dir);
        app_shutdown(&g);return 1;
      }
      char manifest_path[PATH_MAX];
      if(!path_join(manifest_path,sizeof(manifest_path),screenshot_dir,"MANIFEST.tsv",'/')){
        app_shutdown(&g);return 1;
      }
      FILE*manifest=fopen(manifest_path,"wb");
      if(!manifest){fprintf(stderr,"cannot create UI review manifest\n");app_shutdown(&g);return 1;}
      fprintf(manifest,"file\twidth\theight\tstate\tlanguage\ttheme\tfocus\tfx\tfy\tfw\tfh\tscroll\tscroll_max\n");

      Game review_base=game;
      Persistence review_persistence;memset(&review_persistence,0,sizeof(review_persistence));
      save_policy_init(&review_persistence.save_policy);
      review_persistence.enabled=true;
      audio_persistence=&review_persistence;audio_gfx=&g;audio_ui=&ui;

      for(unsigned n=0;n<sizeof(sizes)/sizeof(sizes[0]);++n){
        for(int state=0;state<REVIEW_STATE_COUNT;++state){
          SDL_SetWindowMinimumSize(g.win,1,1);SDL_SetWindowSize(g.win,sizes[n][0],sizes[n][1]);
          SDL_PumpEvents();
          if(!gfx_refresh_metrics(&g)){fclose(manifest);app_shutdown(&g);return 1;}
          if(g.width!=sizes[n][0]||g.height!=sizes[n][1]){
            fprintf(stderr,"renderer refused %dx%d (got %dx%d)\n",sizes[n][0],sizes[n][1],g.width,g.height);
            fclose(manifest);app_shutdown(&g);return 1;
          }

          game=review_base;app_state_init(&ui);set_mode_params(&ui);
          ui.session_open=true;ui.has_session=false;
          ui.language=(Language)((n+state)%LANG_COUNT);
          ui.dark_theme=((n*REVIEW_STATE_COUNT+state)&1)==0;
          ui.reduced_motion=true;
          settings_open=false;settings_slider_drag=SETTINGS_DRAG_NONE;
          help_scroll_reset();
          info_keyboard_focus=INFO_FOCUS_BACK;result_keyboard_focus=0;
          pause_keyboard_focus=0;hint_keyboard_focus=HINT_FOCUS_PRIMARY;
          play_keyboard_focus=-1;
          save_policy_init(&review_persistence.save_policy);

          const char*focus="none";SDL_Rect focus_rect={0,0,0,0};
          AppGeometry geometry;
          if(!get_geometry(&g,&ui,&geometry)){fclose(manifest);app_shutdown(&g);return 1;}

          switch(state){
            case REVIEW_HOME_EMPTY:
              ui.screen=SCR_TITLE;home_focus=HOME_FOCUS_NEW;focus="new-game";
              focus_rect=sdl_rect(geometry.home_primary[0]);break;
            case REVIEW_HOME_SAVE:
              ui.screen=SCR_TITLE;ui.has_session=true;ui.normal_resume.present=true;
              ui.normal_resume.can_continue=true;ui.normal_resume.mode=MODE_CLASSIC;
              ui.normal_resume.difficulty=DIFFICULTY_MEDIUM;ui.normal_resume.elapsed_ms=754000;
              home_focus=HOME_FOCUS_CONTINUE;focus="continue";
              focus_rect=sdl_rect(geometry.home_primary[1]);break;
            case REVIEW_NOTES:
              ui.screen=SCR_PLAY;ui.notes_mode=true;ui.sel_r=0;ui.sel_c=0;
              game.fixed[0]=0;game.puzzle[0]=0;game.notes[0]=(1u<<1)|(1u<<3)|(1u<<7);
              play_keyboard_focus=PLAY_FOCUS_NOTES;focus="notes";
              focus_rect=play_focus_rect(&g,&ui,play_keyboard_focus);break;
            case REVIEW_CONFLICT:
              ui.screen=SCR_PLAY;ui.sel_r=0;ui.sel_c=1;
              game.fixed[0]=game.fixed[1]=0;game.puzzle[0]=5;game.puzzle[1]=5;
              play_keyboard_focus=PLAY_FOCUS_VERIFY;focus="verify";
              focus_rect=play_focus_rect(&g,&ui,play_keyboard_focus);break;
            case REVIEW_HINT:
              ui.screen=SCR_PLAY;ui.hint_preview_active=true;
              ui.hint_preview.status=HUMAN_HINT_STALLED;ui.hint_verify_required=false;
              hint_keyboard_focus=HINT_FOCUS_REVEAL;focus="hint-reveal";
              focus_rect=hint_focus_rect(&g,&ui,hint_keyboard_focus);break;
            case REVIEW_SAVE_ERROR:
              ui.screen=SCR_PLAY;
              save_policy_mark(&review_persistence.save_policy,SAVE_DIRTY_NORMAL,now_ms());
              save_policy_record_result(&review_persistence.save_policy,STORE_IO_ERROR,now_ms());
              play_keyboard_focus=PLAY_FOCUS_SAVE_RETRY;focus="retry-save";
              focus_rect=storage_banner_layout(&g,&ui).retry;break;
            case REVIEW_PAUSE:
              ui.screen=SCR_PLAY;ui.pause_reasons=PAUSE_MANUAL;pause_keyboard_focus=1;focus="pause-menu";
              focus_rect=sdl_rect(geometry.pause_buttons[1]);break;
            case REVIEW_LOSS:
              ui.screen=SCR_END;ui.result=RES_LOSE;result_keyboard_focus=0;focus="retry";
              focus_rect=sdl_rect(geometry.end_buttons[0]);break;
            case REVIEW_WIN:
              ui.screen=SCR_END;ui.result=RES_WIN;result_keyboard_focus=1;focus="home";
              focus_rect=sdl_rect(geometry.end_buttons[1]);break;
            case REVIEW_HELP:
              ui.screen=SCR_HELP;info_keyboard_focus=INFO_FOCUS_BACK;focus="back";
              focus_rect=back_rect(&g,&ui);break;
            case REVIEW_SETTINGS:{
              ui.screen=APP_SCREEN_SETTINGS;ui.prev_screen=SCR_PLAY;ui.pause_reasons=PAUSE_MODAL;
              settings_open=true;settings_keyboard_focus=SETTINGS_FOCUS_MUSIC;focus="music";
              SettingsPanelLayout p=ui_settings_panel_layout(&g);
              focus_rect=ui_settings_focus_rect(p,settings_keyboard_focus);break;
            }
            case REVIEW_ABOUT:
              ui.screen=SCR_ABOUT;info_keyboard_focus=INFO_FOCUS_LINK_0;focus="about-github";
              focus_rect=sdl_rect(geometry.about_links[0]);break;
          }

          app_render_ui(&g,&game,&ui);
          char filename[192],path[PATH_MAX];
          snprintf(filename,sizeof(filename),"%dx%d-%s-%s-%s.bmp",g.width,g.height,
                   state_names[state],language_name(ui.language),ui.dark_theme?"dark":"light");
          if(!path_join(path,sizeof(path),screenshot_dir,filename,'/')||!save_frame(&g,path)){
            fclose(manifest);app_shutdown(&g);return 1;
          }
          fprintf(manifest,"%s\t%d\t%d\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n",
                  filename,g.width,g.height,state_names[state],language_name(ui.language),
                  ui.dark_theme?"dark":"light",focus,focus_rect.x,focus_rect.y,
                  focus_rect.w,focus_rect.h,help_scroll_px,help_scroll_max_px);
        }
      }

      const int audit_w=1024,audit_h=768;
      const int audit_states[3]={REVIEW_HELP,REVIEW_SETTINGS,REVIEW_ABOUT};
      for(int language=0;language<LANG_COUNT;++language)for(int dark=0;dark<2;++dark)
        for(int a=0;a<3;++a){
          int state=audit_states[a];
          SDL_SetWindowMinimumSize(g.win,1,1);SDL_SetWindowSize(g.win,audit_w,audit_h);
          SDL_PumpEvents();
          if(!gfx_refresh_metrics(&g)){fclose(manifest);app_shutdown(&g);return 1;}
          game=review_base;app_state_init(&ui);set_mode_params(&ui);
          ui.language=(Language)language;ui.dark_theme=dark!=0;ui.reduced_motion=true;
          ui.session_open=true;settings_open=false;help_scroll_reset();info_keyboard_focus=INFO_FOCUS_BACK;
          AppGeometry geometry;if(!get_geometry(&g,&ui,&geometry)){fclose(manifest);app_shutdown(&g);return 1;}
          const char*focus="back";SDL_Rect focus_rect=back_rect(&g,&ui);
          const char*audit_state=state_names[state];
          if(state==REVIEW_HELP){
            ui.screen=SCR_HELP;
            app_render_ui(&g,&game,&ui);
            help_scroll_set(help_scroll_max_px);
            audit_state="help-bottom";
          }
          else if(state==REVIEW_SETTINGS){
            ui.screen=APP_SCREEN_SETTINGS;ui.prev_screen=SCR_PLAY;ui.pause_reasons=PAUSE_MODAL;
            settings_open=true;settings_keyboard_focus=SETTINGS_FOCUS_REDUCED_MOTION;
            SettingsPanelLayout p=ui_settings_panel_layout(&g);
            focus="reduced-motion";focus_rect=ui_settings_focus_rect(p,settings_keyboard_focus);
          }else{
            ui.screen=SCR_ABOUT;info_keyboard_focus=INFO_FOCUS_LINK_3;
            focus="about-support";focus_rect=sdl_rect(geometry.about_links[3]);
          }
          app_render_ui(&g,&game,&ui);
          char filename[192],path[PATH_MAX];
          snprintf(filename,sizeof(filename),"audit-%s-%s-%s.bmp",audit_state,
                   language_name(ui.language),ui.dark_theme?"dark":"light");
          if(!path_join(path,sizeof(path),screenshot_dir,filename,'/')||!save_frame(&g,path)){
            fclose(manifest);app_shutdown(&g);return 1;
          }
          fprintf(manifest,"%s\t%d\t%d\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n",
                  filename,g.width,g.height,audit_state,language_name(ui.language),
                  ui.dark_theme?"dark":"light",focus,focus_rect.x,focus_rect.y,
                  focus_rect.w,focus_rect.h,help_scroll_px,help_scroll_max_px);
        }
      fclose(manifest);settings_open=false;audio_persistence=NULL;
      printf("rendered 78 diagnostic UI review screenshots to %s\n",screenshot_dir);
    }
    app_shutdown(&g);
    return 0;
  }

  Persistence persistence;
  persistence_init(&persistence);
  ui.language = persistence.preferences.language;
  if (persistence.profile_status == STORE_CORRUPT &&
      persistence_previous_available(&persistence)) {
    bool recover = confirm_box(g.win, tr(ui.language, T_STORAGE_ERROR),
                               tr(ui.language, T_PROFILE_RECOVER_PROMPT),
                               tr(ui.language, T_RECOVER),
                               tr(ui.language, T_QUIT));
    if (!recover) {
      app_shutdown(&g);
      return 1;
    }
    if (!persistence_recover_previous(&persistence)) {
      (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     tr(ui.language, T_STORAGE_ERROR),
                                     tr(ui.language, T_PROFILE_IO_ERROR),
                                     g.win);
      app_shutdown(&g);
      return 1;
    }
    ui.language = persistence.preferences.language;
    audio_apply_preferences(&persistence);
  }
  const char *startup_error =
      persistence_startup_error(&persistence, ui.language);
  if (startup_error) {
    (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                   tr(ui.language, T_STORAGE_ERROR),
                                   startup_error, g.win);
    app_shutdown(&g);
    return 1;
  }
  ui.dark_theme = persistence.preferences.dark_theme;
  ui.reduced_motion = persistence.preferences.reduced_motion;
  ui.strict_mode = persistence.preferences.strict_mode;
  ui.auto_remove_peer_notes = persistence.preferences.auto_remove_peer_notes;
  ui.mode = persistence.preferences.mode;
  home_difficulty = persistence.preferences.difficulty;
  set_mode_params(&ui);
  apply_window_preferences(&g, &persistence.preferences);
  refresh_resume_info(&persistence, &ui);
  home_focus = ui.normal_resume.can_continue
                   ? HOME_FOCUS_CONTINUE
                   : HOME_FOCUS_NEW;

  bool running = true;
  SDL_Event e;
  while (running) {
    if(settle_terminal(&persistence,&game,&ui))g.dirty=true;
    int wait_ms=g.dirty?0:desktop_wait_timeout_ms(&g,&ui);
    for(int have_event=ui_wait_event_timeout(&e,wait_ms);
        have_event;have_event=ui_poll_event(&e)){
      g.dirty=true;
      if (e.type != SDL_QUIT &&
          settle_terminal(&persistence, &game, &ui))
        continue;
      if (e.type == SDL_QUIT) {
        if (request_exit(&persistence, &game, &ui, &g))
          running = false;
        continue;
      }
      if (e.type == SDL_RENDER_DEVICE_RESET ||
          e.type == SDL_RENDER_TARGETS_RESET) {
        (void)gfx_refresh_metrics(&g);
        if (!app_rebuild_renderer_resources(&g))
          fprintf(stderr, "renderer resource rebuild failed: %s\n",
                  SDL_GetError());
        continue;
      }
      if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          apply_window_resize(&g, e.window.data1, e.window.data2);
          capture_window_preferences(&persistence, &g, &ui);
        } else if (e.window.event == SDL_WINDOWEVENT_MOVED ||
                   e.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
                   e.window.event == SDL_WINDOWEVENT_RESTORED) {
          (void)gfx_refresh_metrics(&g);
          gfx_capture_restored_bounds(&g);
          capture_window_preferences(&persistence, &g, &ui);
#if SDL_VERSION_ATLEAST(2,0,18)
        } else if (e.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
          (void)gfx_refresh_metrics(&g);
          if (!app_rebuild_renderer_resources(&g))
            fprintf(stderr, "display-scale resource rebuild failed: %s\n",
                    SDL_GetError());
          capture_window_preferences(&persistence, &g, &ui);
#endif
        } else if (e.window.event == SDL_WINDOWEVENT_EXPOSED) {
          g.dirty = true;
        } else if ((e.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                    e.window.event == SDL_WINDOWEVENT_MINIMIZED ||
                    e.window.event == SDL_WINDOWEVENT_HIDDEN) &&
                   ui.session_open && ui.result == RES_NONE) {
          if (clock_pause(&ui, PAUSE_FOCUS))
            (void)checkpoint_current_session(&persistence, &game, &ui);
        }
        /* Focus recovery intentionally leaves PAUSE_FOCUS set. The board stays
           hidden until the player explicitly resumes, so a stopped Time
           Attack clock cannot be used to inspect the puzzle. */
        continue;
      }
      if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        SDL_Keymod mod = SDL_GetModState();
        if (persistence_has_save_error(&persistence) && k == SDLK_r) {
          ui_button_click();
          (void)persistence_flush(&persistence);
          continue;
        }
        if (open_global_info_key(&persistence, &game, &ui, k))
          continue;
        if (k == SDLK_l) {
          ui_button_click();
          home_cycle_language(&persistence, &ui, 1);
          continue;
        }
        if (ui.screen == SCR_TITLE) {
          if (k == SDLK_ESCAPE) {
            if (request_exit(&persistence, &game, &ui, &g))
              running = false;
          }
          else if (k == SDLK_TAB)
            home_move_focus(&ui, (mod & KMOD_SHIFT) ? -1 : 1);
          else if (k == SDLK_LEFT) {
            if (home_focus == HOME_FOCUS_LANGUAGE) {
              ui_button_click();
              home_cycle_language(&persistence, &ui, -1);
            } else if (home_focus == HOME_FOCUS_MODE) {
              ui_button_click();
              home_cycle_mode(&persistence, &ui, -1);
            } else if (home_focus == HOME_FOCUS_DIFFICULTY) {
              ui_button_click();
              home_cycle_difficulty(&persistence, &ui, -1);
            } else {
              home_move_focus(&ui, -1);
            }
          } else if (k == SDLK_RIGHT) {
            if (home_focus == HOME_FOCUS_LANGUAGE) {
              ui_button_click();
              home_cycle_language(&persistence, &ui, 1);
            } else if (home_focus == HOME_FOCUS_MODE) {
              ui_button_click();
              home_cycle_mode(&persistence, &ui, 1);
            } else if (home_focus == HOME_FOCUS_DIFFICULTY) {
              ui_button_click();
              home_cycle_difficulty(&persistence, &ui, 1);
            } else {
              home_move_focus(&ui, 1);
            }
          } else if (k == SDLK_UP)
            home_move_focus(&ui, -1);
          else if (k == SDLK_DOWN)
            home_move_focus(&ui, 1);
          else if (input_activation_key(k))
            home_activate(&persistence, &game, &ui, &g, &running);
          else if (k == SDLK_c && ui.has_session) {
            ui_button_click();
            victory_animation_reset();
            restore_saved_session(&persistence, &game, &ui, false);
          } else if (k == SDLK_d) {
            ui_button_click();
            home_daily_game(&persistence, &game, &ui, &g);
          } else if (k == SDLK_t) {
            ui_button_click();
            ui.dark_theme = !ui.dark_theme;
            (void)save_preferences(&persistence, &ui);
          } else if (k == SDLK_m) {
            ui_button_click();
            home_cycle_mode(&persistence, &ui, 1);
          }
          continue;
        }
        if (ui.screen == SCR_HELP || ui.screen == SCR_ABOUT) {
          if (k == SDLK_ESCAPE) {
            ui_button_click();
            return_from_info(&persistence, &game, &ui);
          } else if (ui.screen == SCR_HELP &&
                     (k == SDLK_UP || k == SDLK_DOWN ||
                      k == SDLK_PAGEUP || k == SDLK_PAGEDOWN ||
                      k == SDLK_HOME || k == SDLK_END)) {
            if(k==SDLK_HOME)help_scroll_set(0);
            else if(k==SDLK_END)help_scroll_set(help_scroll_max_px);
            else if(k==SDLK_PAGEUP)help_scroll_by(-220);
            else if(k==SDLK_PAGEDOWN)help_scroll_by(220);
            else help_scroll_by(k==SDLK_UP?-44:44);
          } else if (k == SDLK_TAB) {
            info_move_focus(&ui,(mod&KMOD_SHIFT)?-1:1);
          } else if (ui.screen == SCR_ABOUT &&
                     (k == SDLK_LEFT || k == SDLK_RIGHT ||
                      k == SDLK_UP || k == SDLK_DOWN)) {
            int direction=(k==SDLK_LEFT||k==SDLK_UP)?-1:1;
            info_move_focus(&ui,direction);
          } else if (input_activation_key(k)) {
            (void)activate_info_focus(&persistence,&game,&ui,&g);
          } else if (ui.screen == SCR_ABOUT) {
            if (k == SDLK_1 || k == SDLK_KP_1)
              open_about_url(&g, &ui, 0);
            else if (k == SDLK_2 || k == SDLK_KP_2)
              open_about_url(&g, &ui, 1);
            else if (k == SDLK_3 || k == SDLK_KP_3)
              open_about_url(&g, &ui, 2);
          }
          continue;
        }
        if (ui.screen == SCR_END) {
          if (k == SDLK_ESCAPE) {
            ui_button_click();victory_animation_reset();go_home(&persistence,&game,&ui);
          } else if (k == SDLK_TAB || k == SDLK_LEFT || k == SDLK_RIGHT ||
                     k == SDLK_UP || k == SDLK_DOWN) {
            int direction=(k==SDLK_LEFT||k==SDLK_UP||(k==SDLK_TAB&&(mod&KMOD_SHIFT)))?-1:1;
            result_keyboard_focus=input_focus_step(result_keyboard_focus,2,direction);
          } else if (input_activation_key(k)) {
            activate_result_focus(&persistence,&game,&ui,&g);
          }
          continue;
        }
        if (ui.pause_reasons) {
          if ((ui.pause_reasons & PAUSE_USER_VISIBLE) &&
              (k == SDLK_TAB || k == SDLK_LEFT || k == SDLK_RIGHT ||
               k == SDLK_UP || k == SDLK_DOWN)) {
            int direction=(k==SDLK_LEFT||k==SDLK_UP||(k==SDLK_TAB&&(mod&KMOD_SHIFT)))?-1:1;
            pause_keyboard_focus=input_focus_step(pause_keyboard_focus,2,direction);
          } else if ((ui.pause_reasons & PAUSE_USER_VISIBLE) && input_activation_key(k)) {
            activate_pause_focus(&persistence,&game,&ui);
          } else if ((ui.pause_reasons & PAUSE_USER_VISIBLE) && k == SDLK_p) {
            pause_keyboard_focus=0;activate_pause_focus(&persistence,&game,&ui);
          } else if (k == SDLK_ESCAPE) {
            ui_button_click();go_home(&persistence,&game,&ui);
          }
          continue;
        }

        bool shifted = (mod & KMOD_SHIFT) != 0;
        int r = ui.sel_r, c = ui.sel_c;
        InputPlayShortcut play_shortcut = input_play_shortcut(k, mod);
        if (play_shortcut == INPUT_PLAY_UNDO ||
            play_shortcut == INPUT_PLAY_REDO) {
          AppActionKind kind = play_shortcut == INPUT_PLAY_REDO
                                   ? APP_ACTION_REDO
                                   : APP_ACTION_UNDO;
          ui_button_click();
          if (apply_history_action(&game, &ui, kind)) {
            clear_hint_preview(&ui);
            (void)stage_current_session(&persistence, &game, &ui);
          }
          continue;
        }
        if (play_shortcut == INPUT_PLAY_VERIFY) {
          ui_button_click();
          verify_current_entries(&persistence, &game, &ui);
          continue;
        }
        if (ui.hint_preview_active) {
          if (k == SDLK_TAB) {
            hint_move_focus(&ui, shifted ? -1 : 1);
            continue;
          }
          if (input_activation_key(k)) {
            (void)activate_hint_focus(&persistence, &game, &ui);
            continue;
          }
          if (k == SDLK_ESCAPE) {
            ui_button_click();
            clear_hint_preview(&ui);
            continue;
          }
          if (k == SDLK_h && shifted) {
            ui_button_click();
            (void)reveal_selected_cell(&persistence, &game, &ui);
            continue;
          }
          if (k == SDLK_h) {
            ui_button_click();
            if (ui.hint_preview.status == HUMAN_HINT_LOGICAL)
              (void)apply_current_logic_hint(&persistence, &game, &ui);
            else if (ui.hint_preview.status == HUMAN_HINT_INVALID ||
                     ui.hint_preview.status == HUMAN_HINT_STALLED)
              verify_current_entries(&persistence, &game, &ui);
            continue;
          }
        }
        if (k == SDLK_TAB) {
          play_move_focus(&game, &ui, shifted ? -1 : 1);
          continue;
        }
        if (input_activation_key(k) && play_keyboard_focus >= 0) {
          if (activate_play_focus(&persistence, &game, &ui, &g)) continue;
        }
        if (k == SDLK_ESCAPE) {
          ui_button_click();
          go_home(&persistence, &game, &ui);
        } else if (k == SDLK_UP || k == SDLK_w) {
          play_keyboard_focus=-1;ui.sel_r = (ui.sel_r + 8) % 9;
        } else if (k == SDLK_DOWN || k == SDLK_s) {
          play_keyboard_focus=-1;ui.sel_r = (ui.sel_r + 1) % 9;
        } else if (k == SDLK_LEFT || k == SDLK_a) {
          play_keyboard_focus=-1;ui.sel_c = (ui.sel_c + 8) % 9;
        } else if (k == SDLK_RIGHT || k == SDLK_d) {
          play_keyboard_focus=-1;ui.sel_c = (ui.sel_c + 1) % 9;
        }
        else if (k == SDLK_p) {
          ui_button_click();
          pause_keyboard_focus=0;
          clock_pause(&ui, PAUSE_MANUAL);
          (void)checkpoint_current_session(&persistence, &game, &ui);
        } else if (k == SDLK_t) {
          ui_button_click();
          ui.dark_theme = !ui.dark_theme;
          (void)save_preferences(&persistence, &ui);
        } else if (k == SDLK_n && shifted) {
          ui_button_click();
          ui.auto_remove_peer_notes = !ui.auto_remove_peer_notes;
          show_toast(&ui, tr(ui.language, ui.auto_remove_peer_notes
                                            ? T_AUTO_NOTES_ON
                                            : T_AUTO_NOTES_OFF));
          (void)save_preferences(&persistence, &ui);
        } else if (k == SDLK_n) {
          ui_button_click();
          ui.notes_mode = !ui.notes_mode;
          show_toast(&ui,
                     tr(ui.language, ui.notes_mode ? T_NOTES_ON : T_NOTES_OFF));
          (void)stage_current_session(&persistence, &game, &ui);
        } else if (k == SDLK_m) {
          ui_button_click();
          ui.strict_mode = !ui.strict_mode;
          show_toast(&ui,
                     tr(ui.language, ui.strict_mode ? T_STRICT : T_FREE));
          (void)save_preferences(&persistence, &ui);
          (void)stage_current_session(&persistence, &game, &ui);
        } else if (k == SDLK_h) {
          ui_button_click();
          (void)prepare_hint_preview(&game, &ui);
          if(ui.hint_preview_active)hint_focus_reset(&ui);
        } else {
          int value = input_digit_value(k, e.key.keysym.scancode);
          if (k == SDLK_DELETE || k == SDLK_BACKSPACE || value == 0) {
            if (apply_player_input(&game, &ui, r, c, 0, false)) {
              clear_hint_preview(&ui);
              (void)stage_current_session(&persistence, &game, &ui);
            }
          } else if (value >= 1 && value <= 9 &&
                     apply_player_input(&game, &ui, r, c, value,
                                        ui.notes_mode || shifted)) {
            clear_hint_preview(&ui);
            (void)stage_current_session(&persistence, &game, &ui);
          }
        }
        continue;
      }

      if (e.type == SDL_MOUSEWHEEL && ui.screen == SCR_HELP) {
        int wheel=e.wheel.y;
        if(e.wheel.direction==SDL_MOUSEWHEEL_FLIPPED)wheel=-wheel;
        help_scroll_by(-wheel*48);
        continue;
      }

      if (e.type == SDL_MOUSEBUTTONDOWN) {
        int x = e.button.x, y = e.button.y;
        bool left = input_mouse_button_is_primary(e.button.button);
        bool right = e.button.button == SDL_BUTTON_RIGHT;
        if (!left && !right) continue;
        if (left && storage_retry_hit(&g, &ui, x, y)) {
          play_keyboard_focus=PLAY_FOCUS_SAVE_RETRY;
          ui_button_click();
          (void)persistence_flush(&persistence);
          continue;
        }
        if (left && ui.hint_preview_active) {
          if (hint_apply_hit(&g, &ui, x, y)) {
            hint_keyboard_focus=HINT_FOCUS_PRIMARY;
            ui_button_click();
            (void)apply_current_logic_hint(&persistence, &game, &ui);
            continue;
          }
          if (hint_verify_hit(&g, &ui, x, y)) {
            hint_keyboard_focus=HINT_FOCUS_PRIMARY;
            ui_button_click();
            verify_current_entries(&persistence, &game, &ui);
            continue;
          }
          if (hint_reveal_hit(&g, &ui, x, y)) {
            hint_keyboard_focus=HINT_FOCUS_REVEAL;
            ui_button_click();
            (void)reveal_selected_cell(&persistence, &game, &ui);
            continue;
          }
          if (hint_cancel_hit(&g, &ui, x, y)) {
            hint_keyboard_focus=HINT_FOCUS_CANCEL;
            ui_button_click();
            clear_hint_preview(&ui);
            continue;
          }
          if (hint_panel_blocks_point(&g, &ui, x, y))
            continue;
        }
        if (point_in(language_rect(&g, &ui), x, y)) {
          if (!left) continue;
          for (int language = 0; language < LANG_COUNT; ++language) {
            if (point_in(language_segment(&g, &ui, language), x, y)) {
              ui_button_click();
              home_set_language(&persistence, &ui, (Language)language);
              if (ui.screen == SCR_TITLE) home_focus = HOME_FOCUS_LANGUAGE;
              break;
            }
          }
          continue;
        }
        if (ui.screen == SCR_TITLE) {
          if (!left) continue;
          AppGeometry a;
          if (!get_geometry(&g, &ui, &a)) continue;
          bool handled = false;
          for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
            if (point_in(sdl_rect(a.home_mode[i]), x, y)) {
              ui_button_click();
              home_focus = HOME_FOCUS_MODE;
              home_set_mode(&persistence, &ui, (Mode)i);
              handled = true;
              break;
            }
          }
          if (handled) continue;
          for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
            if (point_in(sdl_rect(a.home_difficulty[i]), x, y)) {
              ui_button_click();
              home_focus = HOME_FOCUS_DIFFICULTY;
              home_set_difficulty(&persistence, &ui, (GameDifficulty)i);
              handled = true;
              break;
            }
          }
          if (handled) continue;
          if (point_in(sdl_rect(a.home_primary[0]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_NEW;
            home_new_game(&persistence, &game, &ui, &g);
          } else if (point_in(sdl_rect(a.home_primary[1]), x, y) &&
                     ui.has_session) {
            ui_button_click();
            home_focus = HOME_FOCUS_CONTINUE;
            victory_animation_reset();
            restore_saved_session(&persistence, &game, &ui, false);
          } else if (point_in(sdl_rect(a.home_primary[2]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_DAILY;
            home_daily_game(&persistence, &game, &ui, &g);
          } else if (point_in(sdl_rect(a.home_secondary[0]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_HELP;
            info_focus_reset();
            (void)app_open_aux(&ui, SCR_HELP);
          } else if (point_in(sdl_rect(a.home_secondary[1]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_ABOUT;
            info_focus_reset();
            (void)app_open_aux(&ui, SCR_ABOUT);
          } else if (point_in(sdl_rect(a.home_secondary[2]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_SETTINGS;
            ui_settings_open(&persistence, &game, &ui);
          } else if (point_in(sdl_rect(a.home_secondary[3]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_QUIT;
            if (request_exit(&persistence, &game, &ui, &g))
              running = false;
          }
          continue;
        }
        if (ui.screen == SCR_END) {
          if (!left) continue;
          AppGeometry a;
          if (!get_geometry(&g, &ui, &a)) continue;
          if (point_in(sdl_rect(a.end_buttons[0]), x, y)) {
            result_keyboard_focus=0;
            ui_button_click();
            victory_animation_reset();
            if (ui.result == RES_WIN) {
              if (replace_saved_session_confirmed(&persistence, &ui, &g, false))
                (void)start_new_game_with_ui(&persistence, &game, &ui, &g);
            } else if (restart_current_session_confirmed(
                           &persistence, &game, &ui, &g)) {
              restart_current_session(&persistence, &game, &ui);
            }
          } else if (point_in(sdl_rect(a.end_buttons[1]), x, y)) {
            result_keyboard_focus=1;
            ui_button_click();
            victory_animation_reset();
            go_home(&persistence, &game, &ui);
          }
          continue;
        }
        if (ui.screen == SCR_HELP || ui.screen == SCR_ABOUT) {
          if (!left) continue;
          AppGeometry a;
          if (!get_geometry(&g, &ui, &a)) continue;
          if (ui.screen == SCR_ABOUT) {
            bool opened = false;
            for (int i = 0; i < 3; ++i) {
              if (point_in(sdl_rect(a.about_links[i]), x, y)) {
                info_keyboard_focus=INFO_FOCUS_LINK_0+i;
                open_about_url(&g, &ui, i);
                opened = true;
                break;
              }
            }
            if (opened) continue;
          }
          if (point_in(back_rect(&g, &ui), x, y)) {
            info_keyboard_focus=INFO_FOCUS_BACK;
            ui_button_click();
            return_from_info(&persistence, &game, &ui);
          }
          continue;
        }
        if (ui.pause_reasons) {
          if (left && (ui.pause_reasons & PAUSE_USER_VISIBLE)) {
            AppGeometry a;
            if (get_geometry(&g, &ui, &a)) {
              if (point_in(sdl_rect(a.pause_buttons[0]), x, y)) {
                pause_keyboard_focus=0;
                ui_button_click();
                if (resume_user_pause(&ui))
                  (void)checkpoint_current_session(&persistence, &game, &ui);
              } else if (point_in(sdl_rect(a.pause_buttons[1]), x, y)) {
                pause_keyboard_focus=1;
                ui_button_click();
                go_home(&persistence, &game, &ui);
              }
            }
          }
          continue;
        }

        Layout layout = compute_layout(g.width, g.height, ui.mode);
        int gx = layout.board.x, gy = layout.board.y, side = layout.board.w;
        int cell = side / 9;
        if (side <= 0 || cell <= 0) continue;
        if (x >= gx && x < gx + side && y >= gy && y < gy + side) {
          play_keyboard_focus=-1;
          int c = (x - gx) / cell, r = (y - gy) / cell;
          ui.sel_r = r;
          ui.sel_c = c;
          int index = IDX(r, c);
          if (!game.fixed[index]) {
            int lx = x - (gx + c * cell), ly = y - (gy + r * cell);
            int sub = cell / 3;
            if (sub <= 0) continue;
            int column = lx / sub, row = ly / sub;
            if (column < 0) column = 0;
            if (row < 0) row = 0;
            if (column > 2) column = 2;
            if (row > 2) row = 2;
            int value = row * 3 + column + 1;
            if ((right || ui.notes_mode) &&
                apply_player_input(&game, &ui, r, c, value, true)) {
              clear_hint_preview(&ui);
              (void)stage_current_session(&persistence, &game, &ui);
            }
          }
        } else {
          if (!left) continue;
          SidebarRects controls;
          compute_sidebar_rects(&layout, &controls);
          if (layout.model.hud_count > 4 &&
              point_in(sdl_rect(layout.model.hud[4]), x, y)) {
            play_keyboard_focus=PLAY_FOCUS_STRICT;
            ui_button_click();
            ui.strict_mode = !ui.strict_mode;
            show_toast(&ui,
                       tr(ui.language, ui.strict_mode ? T_STRICT : T_FREE));
            (void)save_preferences(&persistence, &ui);
            (void)stage_current_session(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_MENU], x, y)) {
            SDL_Rect menu_rect, pause_rect;
            menu_pause_rects(controls.btn[PLAY_ACTION_MENU],
                             &menu_rect, &pause_rect);
            if (point_in(menu_rect, x, y))
              play_keyboard_focus=PLAY_FOCUS_MENU;
            else if (point_in(pause_rect, x, y))
              play_keyboard_focus=PLAY_FOCUS_PAUSE;
            ui_button_click();
            if (point_in(menu_rect, x, y)) {
              go_home(&persistence, &game, &ui);
            } else if (point_in(pause_rect, x, y)) {
              pause_keyboard_focus=0;
              clock_pause(&ui, PAUSE_MANUAL);
              (void)checkpoint_current_session(&persistence, &game, &ui);
            }
          } else if (point_in(controls.btn[PLAY_ACTION_PAUSE], x, y)) {
            SDL_Rect undo_rect, redo_rect;
            undo_redo_rects(controls.btn[PLAY_ACTION_PAUSE],
                            &undo_rect, &redo_rect);
            bool undo_hit=point_in(undo_rect,x,y);
            AppActionKind kind = undo_hit ? APP_ACTION_UNDO : APP_ACTION_REDO;
            play_keyboard_focus=undo_hit?PLAY_FOCUS_UNDO:PLAY_FOCUS_REDO;
            ui_button_click();
            if (apply_history_action(&game, &ui, kind)) {
              clear_hint_preview(&ui);
              (void)stage_current_session(&persistence, &game, &ui);
            }
          } else if (point_in(controls.btn[PLAY_ACTION_RESTART], x, y)) {
            SDL_Rect restart_rect, clear_rect;
            restart_clear_rects(controls.btn[PLAY_ACTION_RESTART],
                                &restart_rect, &clear_rect);
            bool clear_hit=point_in(clear_rect,x,y);
            play_keyboard_focus=clear_hit?PLAY_FOCUS_CLEAR:PLAY_FOCUS_RESTART;
            ui_button_click();
            if (clear_hit) {
              if (clear_selected_available(&game, &ui)) {
                if (apply_player_input(&game, &ui, ui.sel_r, ui.sel_c, 0,
                                       false)) {
                  clear_hint_preview(&ui);
                  (void)stage_current_session(&persistence, &game, &ui);
                }
              } else {
                audio_play_effect(AUDIO_EFFECT_BLOCKED);
              }
            } else if (point_in(restart_rect, x, y) &&
                       restart_current_session_confirmed(
                           &persistence, &game, &ui, &g)) {
              victory_animation_reset();
              restart_current_session(&persistence, &game, &ui);
            }
          } else if (point_in(controls.btn[PLAY_ACTION_HINT], x, y)) {
            play_keyboard_focus=PLAY_FOCUS_HINT;
            ui_button_click();
            (void)prepare_hint_preview(&game, &ui);
            if(ui.hint_preview_active)hint_focus_reset(&ui);
          } else if (point_in(controls.btn[PLAY_ACTION_NOTES], x, y)) {
            play_keyboard_focus=PLAY_FOCUS_NOTES;
            ui_button_click();
            ui.notes_mode = !ui.notes_mode;
            show_toast(&ui,
                       tr(ui.language, ui.notes_mode ? T_NOTES_ON : T_NOTES_OFF));
            (void)stage_current_session(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_VERIFY], x, y)) {
            play_keyboard_focus=PLAY_FOCUS_VERIFY;
            ui_button_click();
            verify_current_entries(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_AUDIO], x, y)) {
            play_keyboard_focus=PLAY_FOCUS_SETTINGS;
            ui_button_click();
            ui_settings_open(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_HELP], x, y)) {
            play_keyboard_focus=PLAY_FOCUS_HELP;
            ui_button_click();
            info_focus_reset();
            open_info_from_play(&persistence, &game, &ui, SCR_HELP);
          } else {
            for (int number = 1; number <= 9; ++number) {
              if (point_in(controls.pal[number - 1], x, y)) {
                play_keyboard_focus=PLAY_FOCUS_NUMBER_1+number-1;
                if (!palette_number_complete(&game, ui.mode, number)) {
                  if (apply_player_input(&game, &ui, ui.sel_r, ui.sel_c, number,
                                         ui.notes_mode)) {
                    clear_hint_preview(&ui);
                    (void)stage_current_session(&persistence, &game, &ui);
                  }
                } else {
                  audio_play_effect(AUDIO_EFFECT_BLOCKED);
                }
                break;
              }
            }
          }
        }
      }
    }

    if(settle_terminal(&persistence,&game,&ui))g.dirty=true;
    uint64_t checkpoint_now = now_ms();
    if (ui.session_open && ui.result == RES_NONE && ui.pause_reasons == 0 &&
        checkpoint_now - persistence.last_periodic_checkpoint_ms >=
            SUDOKURA_AUTOSAVE_MAX_MS) {
      (void)checkpoint_current_session(&persistence, &game, &ui);
      g.dirty=true;
    } else if (save_policy_checkpoint_due(&persistence.save_policy,
                                          checkpoint_now)) {
      (void)persistence_flush(&persistence);
      g.dirty=true;
    }
    if(desktop_dynamic_frame_due(&ui))g.dirty=true;
    if(g.dirty)app_frame(&g,&game,&ui);
    else app_tick(&g,&game,&ui);
  }

  app_shutdown(&g);
  return 0;
}
