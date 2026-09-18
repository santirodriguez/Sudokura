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
  int year = 0, month = 0, day = 0;
  if (!local_date(&year, &month, &day)) return;
  victory_animation_reset();
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
      (void)app_open_aux(ui, SCR_HELP);
      break;
    case HOME_FOCUS_ABOUT:
      (void)app_open_aux(ui, SCR_ABOUT);
      break;
    case HOME_FOCUS_THEME:
      ui->dark_theme = !ui->dark_theme;
      (void)save_preferences(p, ui);
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

static bool open_global_info_key(Persistence *persistence, const Game *game,
                                 UI *ui, SDL_Keycode key) {
  InputInfoShortcut shortcut = input_info_shortcut(key);
  if (shortcut == INPUT_INFO_NONE) return false;
  Screen target =
      shortcut == INPUT_INFO_HELP ? SCR_HELP : SCR_ABOUT;

  ui_button_click();
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

static void apply_window_preferences(Gfx *g, const Preferences *preferences) {
  if (!g || !g->win || !preferences) return;
  if (preferences->window_width >= 320 && preferences->window_height >= 240)
    SDL_SetWindowSize(g->win, (int)preferences->window_width,
                     (int)preferences->window_height);
  if (preferences->window_x != INT32_MIN &&
      preferences->window_y != INT32_MIN)
    SDL_SetWindowPosition(g->win, preferences->window_x,
                         preferences->window_y);
  if (preferences->window_maximized)
    SDL_MaximizeWindow(g->win);
  SDL_PumpEvents();
  (void)SDL_GetRendererOutputSize(g->ren, &g->width, &g->height);
}

static void capture_window_preferences(Persistence *p, Gfx *g, UI *ui) {
  if (!p || !p->enabled || !g || !g->win || !ui) return;
  int x = 0, y = 0, width = 0, height = 0;
  SDL_GetWindowPosition(g->win, &x, &y);
  SDL_GetWindowSize(g->win, &width, &height);
  Uint32 flags = SDL_GetWindowFlags(g->win);
  p->preferences.window_x = x;
  p->preferences.window_y = y;
  if (width >= 320 && height >= 240) {
    p->preferences.window_width = (uint32_t)width;
    p->preferences.window_height = (uint32_t)height;
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

int main(int argc, char **argv) {
  const char *font_cli = NULL;
  const char *screenshot_dir = NULL;
  bool smoke_test = false;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--font") && i + 1 < argc)
      font_cli = argv[++i];
    else if (!strcmp(argv[i], "--smoke-test"))
      smoke_test = true;
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
      const int sizes[][2] = {{640, 480},  {800, 600},  {1024, 720},
                              {1366, 768}, {1920, 1080}, {2560, 1440},
                              {3440, 1440}, {360, 640}, {390, 844},
                              {412, 915}};
      const char *names[] = {"title", "play", "help", "about",
                             "result", "pause", "audio"};
      if (!make_directory(screenshot_dir)) {
        fprintf(stderr, "cannot create screenshot directory: %s\n",
                screenshot_dir);
        app_shutdown(&g);
        return 1;
      }
      for (unsigned n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
        for (int variant = 0; variant < 7; ++variant) {
          SDL_SetWindowMinimumSize(g.win, 1, 1);
          SDL_SetWindowSize(g.win, sizes[n][0], sizes[n][1]);
          SDL_PumpEvents();
          SDL_GetRendererOutputSize(g.ren, &g.width, &g.height);
          if (g.width != sizes[n][0] || g.height != sizes[n][1]) {
            fprintf(stderr, "renderer refused %dx%d (got %dx%d)\n",
                    sizes[n][0], sizes[n][1], g.width, g.height);
            app_shutdown(&g);
            return 1;
          }
          ui.language = (Language)((n + variant) % LANG_COUNT);
          ui.dark_theme = ((n + variant) & 1) == 0;
          ui.result = RES_NONE;
          ui.pause_reasons = 0;
          ui.is_daily = false;
          audio_settings_open = false;
          victory_animation_reset();
          if (variant == 0)
            ui.screen = SCR_TITLE;
          else if (variant == 1)
            ui.screen = SCR_PLAY;
          else if (variant == 2)
            ui.screen = SCR_HELP;
          else if (variant == 3)
            ui.screen = SCR_ABOUT;
          else if (variant == 4) {
            ui.screen = SCR_END;
            ui.result = RES_WIN;
          } else if (variant == 5) {
            ui.screen = SCR_PLAY;
            ui.pause_reasons = PAUSE_MANUAL;
          } else {
            ui.screen = APP_SCREEN_SETTINGS;
            ui.prev_screen = SCR_PLAY;
            ui.pause_reasons = PAUSE_MODAL;
            audio_settings_open = true;
          }
          app_render_ui(&g, &game, &ui);
          char path[PATH_MAX], filename[160];
          int filename_length = snprintf(
              filename, sizeof(filename), "%dx%d-%s-%s-%s.bmp", g.width,
              g.height, names[variant], language_name(ui.language),
              ui.dark_theme ? "dark" : "light");
          if (filename_length < 0 ||
              (size_t)filename_length >= sizeof(filename) ||
              !path_join(path, sizeof(path), screenshot_dir, filename, '/')) {
            fprintf(stderr, "screenshot path is too long: %s\n",
                    screenshot_dir);
            app_shutdown(&g);
            return 1;
          }
          if (!save_frame(&g, path)) {
            fprintf(stderr, "screenshot: %s\n", SDL_GetError());
            app_shutdown(&g);
            return 1;
          }
        }
      }
      audio_settings_open = false;
      printf("rendered 70 UI review screenshots to %s\n", screenshot_dir);
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
  ui.strict_mode = persistence.preferences.strict_mode;
  ui.mode = persistence.preferences.mode;
  home_difficulty = persistence.preferences.difficulty;
  set_mode_params(&ui);
  apply_window_preferences(&g, &persistence.preferences);
  ui.has_session = session_can_continue(&persistence.saved);

  bool running = true;
  SDL_Event e;
  while (running) {
    (void)settle_terminal(&persistence, &game, &ui);
    while (ui_poll_event(&e)) {
      if (e.type != SDL_QUIT &&
          settle_terminal(&persistence, &game, &ui))
        continue;
      if (e.type == SDL_QUIT) {
        if (request_exit(&persistence, &game, &ui, &g))
          running = false;
        continue;
      }
      if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          apply_window_resize(&g, e.window.data1, e.window.data2);
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
          else if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
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
            ui_button_click();
            victory_animation_reset();
            go_home(&persistence, &game, &ui);
          } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            ui_button_click();
            victory_animation_reset();
            if (ui.result == RES_WIN) {
              if (replace_saved_session_confirmed(&persistence, &ui, &g, false))
                (void)start_new_game_with_ui(&persistence, &game, &ui, &g);
            } else if (restart_current_session_confirmed(
                           &persistence, &game, &ui, &g)) {
              restart_current_session(&persistence, &game, &ui);
            }
          }
          continue;
        }
        if (ui.pause_reasons) {
          if ((ui.pause_reasons & PAUSE_USER_VISIBLE) &&
              (k == SDLK_p || k == SDLK_RETURN || k == SDLK_KP_ENTER)) {
            ui_button_click();
            if (resume_user_pause(&ui))
              (void)checkpoint_current_session(&persistence, &game, &ui);
          } else if (k == SDLK_ESCAPE) {
            ui_button_click();
            go_home(&persistence, &game, &ui);
          }
          continue;
        }

        bool shifted = (mod & KMOD_SHIFT) != 0;
        int r = ui.sel_r, c = ui.sel_c;
        if (k == SDLK_ESCAPE) {
          ui_button_click();
          go_home(&persistence, &game, &ui);
        } else if (k == SDLK_UP || k == SDLK_w)
          ui.sel_r = (ui.sel_r + 8) % 9;
        else if (k == SDLK_DOWN || k == SDLK_s)
          ui.sel_r = (ui.sel_r + 1) % 9;
        else if (k == SDLK_LEFT || k == SDLK_a)
          ui.sel_c = (ui.sel_c + 8) % 9;
        else if (k == SDLK_RIGHT || k == SDLK_d)
          ui.sel_c = (ui.sel_c + 1) % 9;
        else if (k == SDLK_p) {
          ui_button_click();
          clock_pause(&ui, PAUSE_MANUAL);
          (void)checkpoint_current_session(&persistence, &game, &ui);
        } else if (k == SDLK_t) {
          ui_button_click();
          ui.dark_theme = !ui.dark_theme;
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
          if (apply_hint_action(&game, &ui, r, c))
            (void)stage_current_session(&persistence, &game, &ui);
        } else {
          int value = input_digit_value(k, e.key.keysym.scancode);
          if (k == SDLK_DELETE || k == SDLK_BACKSPACE || value == 0) {
            if (apply_player_input(&game, &ui, r, c, 0, false))
              (void)stage_current_session(&persistence, &game, &ui);
          } else if (value >= 1 && value <= 9 &&
                     apply_player_input(&game, &ui, r, c, value,
                                        ui.notes_mode || shifted)) {
            (void)stage_current_session(&persistence, &game, &ui);
          }
        }
        continue;
      }

      if (e.type == SDL_MOUSEBUTTONDOWN) {
        int x = e.button.x, y = e.button.y;
        bool left = input_mouse_button_is_primary(e.button.button);
        bool right = e.button.button == SDL_BUTTON_RIGHT;
        if (!left && !right) continue;
        if (left && storage_retry_hit(&g, x, y)) {
          ui_button_click();
          (void)persistence_flush(&persistence);
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
            (void)app_open_aux(&ui, SCR_HELP);
          } else if (point_in(sdl_rect(a.home_secondary[1]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_ABOUT;
            (void)app_open_aux(&ui, SCR_ABOUT);
          } else if (point_in(sdl_rect(a.home_secondary[2]), x, y)) {
            ui_button_click();
            home_focus = HOME_FOCUS_THEME;
            ui.dark_theme = !ui.dark_theme;
            (void)save_preferences(&persistence, &ui);
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
                open_about_url(&g, &ui, i);
                opened = true;
                break;
              }
            }
            if (opened) continue;
          }
          if (point_in(back_rect(&g, &ui), x, y)) {
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
                ui_button_click();
                if (resume_user_pause(&ui))
                  (void)checkpoint_current_session(&persistence, &game, &ui);
              } else if (point_in(sdl_rect(a.pause_buttons[1]), x, y)) {
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
                apply_player_input(&game, &ui, r, c, value, true))
              (void)stage_current_session(&persistence, &game, &ui);
          }
        } else {
          if (!left) continue;
          SidebarRects controls;
          compute_sidebar_rects(&layout, &controls);
          if (layout.model.hud_count > 4 &&
              point_in(sdl_rect(layout.model.hud[4]), x, y)) {
            ui_button_click();
            ui.strict_mode = !ui.strict_mode;
            show_toast(&ui,
                       tr(ui.language, ui.strict_mode ? T_STRICT : T_FREE));
            (void)save_preferences(&persistence, &ui);
            (void)stage_current_session(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_MENU], x, y)) {
            ui_button_click();
            go_home(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_PAUSE], x, y)) {
            ui_button_click();
            clock_pause(&ui, PAUSE_MANUAL);
            (void)checkpoint_current_session(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_RESTART], x, y)) {
            ui_button_click();
            if (restart_current_session_confirmed(&persistence, &game, &ui, &g)) {
              victory_animation_reset();
              restart_current_session(&persistence, &game, &ui);
            }
          } else if (point_in(controls.btn[PLAY_ACTION_HINT], x, y)) {
            ui_button_click();
            if (apply_hint_action(&game, &ui, ui.sel_r, ui.sel_c))
              (void)stage_current_session(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_NOTES], x, y)) {
            ui_button_click();
            ui.notes_mode = !ui.notes_mode;
            show_toast(&ui,
                       tr(ui.language, ui.notes_mode ? T_NOTES_ON : T_NOTES_OFF));
            (void)stage_current_session(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_VERIFY], x, y)) {
            ui_button_click();
            int conflicts = apply_verify_action(&game, &ui);
            if (conflicts == 0) {
              show_toast(&ui, tr(ui.language, T_NO_CONFLICTS));
            } else {
              char message[32];
              snprintf(message, sizeof(message), tr(ui.language, T_CONFLICTS),
                       conflicts);
              show_toast(&ui, message);
            }
          } else if (point_in(controls.btn[PLAY_ACTION_AUDIO], x, y)) {
            ui_button_click();
            ui_audio_open(&persistence, &game, &ui);
          } else if (point_in(controls.btn[PLAY_ACTION_HELP], x, y)) {
            ui_button_click();
            open_info_from_play(&persistence, &game, &ui, SCR_HELP);
          } else if (point_in(controls.btn[PLAY_ACTION_ABOUT], x, y)) {
            ui_button_click();
            open_info_from_play(&persistence, &game, &ui, SCR_ABOUT);
          } else {
            for (int number = 1; number <= 9; ++number) {
              if (point_in(controls.pal[number - 1], x, y)) {
                if (!palette_number_complete(&game, ui.mode, number)) {
                  if (apply_player_input(&game, &ui, ui.sel_r, ui.sel_c, number,
                                         ui.notes_mode))
                    (void)stage_current_session(&persistence, &game, &ui);
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

    (void)settle_terminal(&persistence, &game, &ui);
    uint64_t checkpoint_now = now_ms();
    if (ui.session_open && ui.result == RES_NONE && ui.pause_reasons == 0 &&
        checkpoint_now - persistence.last_periodic_checkpoint_ms >=
            SUDOKURA_AUTOSAVE_MAX_MS) {
      (void)checkpoint_current_session(&persistence, &game, &ui);
    } else if (save_policy_checkpoint_due(&persistence.save_policy,
                                          checkpoint_now)) {
      (void)persistence_flush(&persistence);
    }
    app_frame(&g, &game, &ui);
  }

  app_shutdown(&g);
  return 0;
}
