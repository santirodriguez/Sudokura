/* Normalize physical keypad input and intercept UI-only controls before the
   main gameplay event loop. */
static const char *ui_support_url(Language language) {
  static const char *const urls[LANG_COUNT] = {
      "https://santiagorodriguez.com/donate/",
      "https://santiagorodriguez.com/donaciones/",
      "https://santiagorodriguez.com/donacions/",
  };
  if (language < 0 || language >= LANG_COUNT) language = LANG_EN;
  return urls[language];
}

static void ui_open_url(const char *url) {
  if (!url || !audio_gfx || !audio_game || !audio_ui) return;
  audio_play_effect(AUDIO_EFFECT_CLICK);
  if (url_launcher_open(url) != URL_LAUNCH_OK) {
    (void)show_simple_message_box(audio_gfx, audio_game, audio_ui,
                                  SDL_MESSAGEBOX_WARNING,
                                  tr(audio_ui->language, T_ABOUT),
                                  tr(audio_ui->language, T_LINK_ERROR));
  }
}

static bool ui_keypad_scancode(SDL_Scancode scancode) {
  switch (scancode) {
    case SDL_SCANCODE_KP_0:
    case SDL_SCANCODE_KP_1:
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_KP_3:
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_KP_5:
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_KP_7:
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_KP_9:
      return true;
    default:
      return false;
  }
}

static int ui_slider_value(SDL_Rect track, int x) {
  int rail_x = track.x + 6;
  int rail_w = track.w - 12;
  if (rail_w <= 0) return 0;
  return ui_clamp_percent((x - rail_x) * 100 / rail_w);
}

static bool ui_handle_settings_panel_event(SDL_Event *event) {
  if (!settings_open || !audio_gfx || !audio_ui) return false;

  if (event->type == SDL_KEYDOWN) {
    SDL_Keycode key = event->key.keysym.sym;
    if (event->key.repeat &&
        !input_key_repeat_allowed(key, event->key.keysym.scancode))
      return true;

    InputInfoShortcut shortcut = input_info_shortcut(key);
    if (shortcut != INPUT_INFO_NONE) {
      audio_play_effect(AUDIO_EFFECT_CLICK);
      settings_open = false;
      info_keyboard_focus = INFO_FOCUS_BACK;
      (void)app_open_aux(audio_ui,
                         shortcut == INPUT_INFO_HELP ? SCR_HELP : SCR_ABOUT);
      return true;
    }

    if (key == SDLK_TAB || key == SDLK_UP || key == SDLK_DOWN) {
      int direction =
          key == SDLK_UP || (key == SDLK_TAB &&
                             (event->key.keysym.mod & KMOD_SHIFT) != 0)
              ? -1
              : 1;
      settings_keyboard_focus =
          (SettingsKeyboardFocus)input_focus_step(
              settings_keyboard_focus, SETTINGS_FOCUS_COUNT, direction);
      audio_play_effect(AUDIO_EFFECT_CLICK);
      return true;
    }

    if (key == SDLK_LEFT || key == SDLK_RIGHT) {
      int direction = key == SDLK_LEFT ? -1 : 1;
      switch (settings_keyboard_focus) {
        case SETTINGS_FOCUS_THEME:
          if ((key == SDLK_LEFT && audio_ui->dark_theme) ||
              (key == SDLK_RIGHT && !audio_ui->dark_theme))
            ui_settings_toggle_theme();
          break;
        case SETTINGS_FOCUS_LANGUAGE:
          ui_settings_cycle_language(direction);
          break;
        case SETTINGS_FOCUS_SOUND: {
          bool enabled = audio_is_enabled();
          if ((key == SDLK_LEFT && enabled) ||
              (key == SDLK_RIGHT && !enabled))
            audio_toggle(audio_persistence);
          break;
        }
        case SETTINGS_FOCUS_MUSIC:
          ui_audio_set_music_level(audio_music_volume() + direction * 5);
          break;
        case SETTINGS_FOCUS_FX:
          ui_audio_set_fx_level(audio_fx_volume() + direction * 5);
          audio_play_effect(AUDIO_EFFECT_POSITIVE);
          break;
        case SETTINGS_FOCUS_REDUCED_MOTION:
          if ((key == SDLK_LEFT && audio_ui->reduced_motion) ||
              (key == SDLK_RIGHT && !audio_ui->reduced_motion))
            ui_settings_toggle_reduced_motion();
          break;
        case SETTINGS_FOCUS_AUTO_NOTES:
          if ((key == SDLK_LEFT && audio_ui->auto_remove_peer_notes) ||
              (key == SDLK_RIGHT && !audio_ui->auto_remove_peer_notes))
            ui_settings_toggle_auto_notes();
          break;
        case SETTINGS_FOCUS_BACK:
        case SETTINGS_FOCUS_COUNT:
        default:
          break;
      }
      return true;
    }

    if (input_activation_key(key)) {
      switch (settings_keyboard_focus) {
        case SETTINGS_FOCUS_THEME:
          ui_settings_toggle_theme();
          break;
        case SETTINGS_FOCUS_LANGUAGE:
          ui_settings_cycle_language(1);
          break;
        case SETTINGS_FOCUS_SOUND:
          audio_toggle(audio_persistence);
          break;
        case SETTINGS_FOCUS_REDUCED_MOTION:
          ui_settings_toggle_reduced_motion();
          break;
        case SETTINGS_FOCUS_AUTO_NOTES:
          ui_settings_toggle_auto_notes();
          break;
        case SETTINGS_FOCUS_BACK:
          ui_settings_close(audio_persistence, NULL, audio_ui);
          break;
        case SETTINGS_FOCUS_MUSIC:
        case SETTINGS_FOCUS_FX:
        case SETTINGS_FOCUS_COUNT:
        default:
          break;
      }
      return true;
    }

    if (key == SDLK_v) {
      audio_toggle(audio_persistence);
      return true;
    }
    if (key == SDLK_ESCAPE) {
      ui_settings_close(audio_persistence, NULL, audio_ui);
      return true;
    }
    return true;
  }

  if (event->type == SDL_MOUSEMOTION && settings_slider_drag != SETTINGS_DRAG_NONE) {
    if ((event->motion.state & SDL_BUTTON_LMASK) == 0) {
      settings_slider_drag = SETTINGS_DRAG_NONE;
      return true;
    }
    SettingsPanelLayout layout = ui_settings_panel_layout(audio_gfx);
    if (settings_slider_drag == SETTINGS_DRAG_MUSIC)
      ui_audio_set_music_level(ui_slider_value(layout.music_track, event->motion.x));
    else
      ui_audio_set_fx_level(ui_slider_value(layout.fx_track, event->motion.x));
    return true;
  }
  if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
    if (settings_slider_drag != SETTINGS_DRAG_NONE) {
      settings_slider_drag = SETTINGS_DRAG_NONE;
      return true;
    }
  }
  if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
    if (event->button.button != SDL_BUTTON_LEFT) return true;
    SettingsPanelLayout layout = ui_settings_panel_layout(audio_gfx);
    int x = event->button.x, y = event->button.y;
    if (point_in(layout.theme, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_THEME;
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_settings_toggle_theme();
    } else if (point_in(layout.language, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_LANGUAGE;
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_settings_cycle_language(1);
    } else if (point_in(layout.sound, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_SOUND;
      audio_toggle(audio_persistence);
    } else if (point_in(layout.music_minus, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_MUSIC;
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_audio_set_music_level(audio_music_volume() - 5);
    } else if (point_in(layout.music_plus, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_MUSIC;
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_audio_set_music_level(audio_music_volume() + 5);
    } else if (point_in(layout.music_track, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_MUSIC;
      settings_slider_drag = SETTINGS_DRAG_MUSIC;
      ui_audio_set_music_level(ui_slider_value(layout.music_track, x));
    } else if (point_in(layout.fx_minus, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_FX;
      ui_audio_set_fx_level(audio_fx_volume() - 5);
      audio_play_effect(AUDIO_EFFECT_POSITIVE);
    } else if (point_in(layout.fx_plus, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_FX;
      ui_audio_set_fx_level(audio_fx_volume() + 5);
      audio_play_effect(AUDIO_EFFECT_POSITIVE);
    } else if (point_in(layout.fx_track, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_FX;
      settings_slider_drag = SETTINGS_DRAG_FX;
      ui_audio_set_fx_level(ui_slider_value(layout.fx_track, x));
      audio_play_effect(AUDIO_EFFECT_POSITIVE);
    } else if (point_in(layout.reduced_motion, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_REDUCED_MOTION;
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_settings_toggle_reduced_motion();
    } else if (point_in(layout.auto_notes, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_AUTO_NOTES;
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_settings_toggle_auto_notes();
    } else if (point_in(layout.back, x, y)) {
      settings_keyboard_focus = SETTINGS_FOCUS_BACK;
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_settings_close(audio_persistence, NULL, audio_ui);
    }
    return true;
  }

  if (event->type == SDL_QUIT || event->type == SDL_WINDOWEVENT) return false;
  return true;
}

static bool ui_handle_global_audio_event(SDL_Event *event) {
  if (!event || !audio_gfx || !audio_ui) return false;
  bool control_available = ui_audio_control_reconcile(audio_ui);
  AudioControlGeometry geo = ui_audio_control_geometry(audio_gfx, audio_ui);
  SDL_Rect button = sdl_rect(geo.button);
  SDL_Rect music = sdl_rect(geo.music);
  SDL_Rect fx = sdl_rect(geo.fx);

  if (event->type == SDL_KEYDOWN) {
    if (event->key.repeat) return false;
    InputAudioShortcut shortcut =
        input_audio_shortcut(event->key.keysym.sym, event->key.keysym.mod,
                             audio_control_popup_open, control_available);
    switch (shortcut) {
      case INPUT_AUDIO_MASTER:
        audio_control_popup_open = false;
        audio_toggle(audio_persistence);
        return true;
      case INPUT_AUDIO_POPUP:
        audio_control_popup_open = !audio_control_popup_open;
        return true;
      case INPUT_AUDIO_MUSIC:
        audio_toggle_music_mute(audio_persistence);
        return true;
      case INPUT_AUDIO_FX:
        audio_toggle_fx_mute(audio_persistence);
        return true;
      case INPUT_AUDIO_CLOSE:
        audio_control_popup_open = false;
        return true;
      case INPUT_AUDIO_NONE:
      default:
        break;
    }
  }

  if (!control_available) return false;

  if (event->type == SDL_MOUSEBUTTONDOWN &&
      event->button.button == SDL_BUTTON_LEFT) {
    int x = event->button.x, y = event->button.y;
    if (point_in(button, x, y)) {
      audio_control_pressed = true;
      audio_control_pressed_at_ms = now_ms();
      return true;
    }
  }
  if (event->type == SDL_MOUSEBUTTONUP &&
      event->button.button == SDL_BUTTON_LEFT && !audio_control_pressed) {
    int x = event->button.x, y = event->button.y;
    if (audio_control_popup_open) {
      if (point_in(music, x, y)) {
        audio_toggle_music_mute(audio_persistence);
        return true;
      }
      if (point_in(fx, x, y)) {
        audio_toggle_fx_mute(audio_persistence);
        return true;
      }
      audio_control_popup_open = false;
      return true;
    }
  }

  if (event->type == SDL_MOUSEBUTTONUP &&
      event->button.button == SDL_BUTTON_LEFT && audio_control_pressed) {
    uint64_t held_ms = now_ms() - audio_control_pressed_at_ms;
    bool inside = point_in(button, event->button.x, event->button.y);
    audio_control_pressed = false;
    if (!inside) return true;
    if (input_audio_press_is_long(held_ms)) {
      audio_control_popup_open = true;
    } else {
      audio_control_popup_open = false;
      audio_toggle(audio_persistence);
    }
    return true;
  }

  return false;
}

/* Resolve ordinary controls before dispatch. Mouse-up remains a mouse-up;
   no queued event is rewritten or replayed as a press on a new screen. */
static uint64_t ui_pointer_generation;
static bool ui_pointer_busy(void);
static uint64_t ui_pointer_context[15];
static bool ui_pointer_mouse_captured;
static int ui_pointer_origin_x, ui_pointer_origin_y;
static PointerTarget ui_pointer_resolve(int x, int y);

typedef enum {
  UI_SCROLL_NONE,
  UI_SCROLL_HELP,
  UI_SCROLL_LIBRARY
} UiScrollKind;

static TouchInput ui_touch_input;
static TouchScroll ui_scroll_gesture;
static UiScrollKind ui_scroll_kind;
static bool ui_touch_replayed;
static bool ui_touch_board_pending;
static bool ui_touch_board_tap;
static uint64_t ui_touch_board_generation;
static int ui_touch_board_row, ui_touch_board_col;
static size_t ui_scroll_start_index;
static GameAttemptId ui_scroll_start_id;
static bool ui_scroll_started_selection, ui_scroll_preview_pending;

static void ui_scroll_cancel(void) {
  touch_scroll_cancel(&ui_scroll_gesture);
  ui_scroll_kind = UI_SCROLL_NONE;
  ui_scroll_started_selection = false;
}

static void ui_touch_cancel(void) {
  touch_input_cancel(&ui_touch_input);
  ui_touch_board_pending = false;
  ui_touch_board_tap = false;
  ui_touch_replayed = false;
  ui_scroll_cancel();
  settings_slider_drag = SETTINGS_DRAG_NONE;
  audio_control_pressed = false;
}

static void ui_pointer_cancel(void) {
  pointer_capture_cancel(&ui_pointer_capture);
  if (ui_pointer_mouse_captured) (void)SDL_CaptureMouse(SDL_FALSE);
  ui_pointer_mouse_captured = false;
}

static void ui_pointer_reconcile(void) {
  if (!audio_ui || !audio_gfx) { ui_pointer_cancel(); return; }
  if (ui_more_open && (audio_ui->screen != SCR_PLAY ||
                       audio_ui->result != RES_NONE ||
                       audio_gfx->width >= 640))
    ui_more_close(audio_ui);
  uint64_t context[] = {
      audio_ui->screen, audio_ui->pause_reasons, audio_ui->result,
      settings_open, audio_control_popup_open, audio_ui->hint_preview_active,
      audio_ui->generating, (uint64_t)audio_gfx->width,
      (uint64_t)audio_gfx->height, audio_ui->language,
      games_browser.scanning, games_browser.detail_open, games_browser.count,
      ui_pointer_busy(), ui_more_open};
  if (memcmp(context, ui_pointer_context, sizeof(context)) != 0) {
    ui_pointer_cancel();
    ui_touch_cancel();
    memcpy(ui_pointer_context, context, sizeof(context));
    ++ui_pointer_generation;
  }
  if (ui_pointer_capture.active &&
      !pointer_target_equal(ui_pointer_capture.target,
                            ui_pointer_resolve(ui_pointer_origin_x,
                                               ui_pointer_origin_y)))
    ui_pointer_cancel();
}

static PointerTarget ui_pointer_target(int action, SDL_Rect rect) {
  return (PointerTarget){action, ui_pointer_generation, 0, 0,
                          rect.x, rect.y, rect.w, rect.h};
}

static PointerTarget ui_pointer_resolve(int x, int y) {
  if (!audio_gfx || !audio_ui || !audio_game || audio_ui->generating)
    return (PointerTarget){0};
  Gfx *gfx = audio_gfx;
  UI *ui = audio_ui;
#define TARGET(id, rect, enabled) do { \
  SDL_Rect hit_rect = (rect); \
  if ((enabled) && point_in(hit_rect, x, y)) \
    return ui_pointer_target((id), hit_rect); \
} while (0)
  if (ui_pointer_busy())
    return ui_pointer_target(4, (SDL_Rect){0, 0, gfx->width, gfx->height});
  if (ui_more_open) {
    AppGeometry geometry;
    if (get_geometry(gfx,ui,&geometry)) {
      for (int i=0;i<GEOMETRY_MORE_COUNT;++i)
        TARGET(160+i,sdl_rect(geometry.more_items[i]),true);
    }
    /* Outside press/release dismisses without reaching underlying UI. */
    return ui_pointer_target(166,(SDL_Rect){0,0,gfx->width,gfx->height});
  }
  if (audio_control_popup_open) {
    AudioControlGeometry a = ui_audio_control_geometry(gfx, ui);
    TARGET(1, sdl_rect(a.music), true);
    TARGET(2, sdl_rect(a.fx), true);
    return ui_pointer_target(3, (SDL_Rect){0, 0, gfx->width, gfx->height});
  }
  if (settings_open) {
    SettingsPanelLayout a = ui_settings_panel_layout(gfx);
    SDL_Rect controls[] = {a.theme, a.language, a.sound, a.music_minus,
        a.music_plus, a.fx_minus, a.fx_plus, a.reduced_motion,
        a.auto_notes, a.back};
    for (int i = 0; i < 10; ++i) TARGET(10 + i, controls[i], true);
    return (PointerTarget){0};
  }
  if (ui->screen == APP_SCREEN_GAMES) {
    GamesBrowserLayout a = games_browser_layout(gfx);
    TARGET(30, a.back, true);
    if (games_browser.scanning) return (PointerTarget){0};
    if (!games_browser.detail_open) {
      SDL_Rect pages[] = {a.page_previous,a.page_next};
      for (int i=0;i<2;++i) {
        if (games_browser_focus_enabled(5+i) && point_in(pages[i],x,y)) {
          PointerTarget target=ui_pointer_target(34+i,pages[i]);
          GameAttemptId id=games_browser.rows[games_browser.selected].entry.id;
          target.key_hi=id.high;target.key_lo=id.low;
          return target;
        }
      }
    }
    SDL_Rect controls[] = {a.primary, a.replay, a.remove};
    const int focus[] = {1, 3, 4};
    for (int i = 0; i < 3; ++i) {
      if (games_browser_focus_enabled(focus[i]) && point_in(controls[i], x, y)) {
        PointerTarget target = ui_pointer_target(31 + i, controls[i]);
        GameAttemptId id = games_browser.rows[games_browser.selected].entry.id;
        target.key_hi = id.high; target.key_lo = id.low;
        return target;
      }
    }
    if (!games_browser.detail_open && point_in(a.list, x, y)) {
      int row = (y - a.list.y) / a.row_height;
      size_t index = games_browser.scroll + (size_t)row;
      if (row < a.visible && index < games_browser.count) {
        SDL_Rect rect = games_browser_row_rect(a, row);
        PointerTarget target = ui_pointer_target(40, rect);
        GameAttemptId id = games_browser.rows[index].entry.id;
        target.key_hi = id.high; target.key_lo = id.low;
        return target;
      }
    }
    return (PointerTarget){0};
  }
  TARGET(50, storage_banner_layout(gfx, ui).retry,
         audio_persistence && persistence_has_save_error(audio_persistence));
  if (hint_panel_interactive(ui)) {
    for (int i = 0; i < HINT_FOCUS_COUNT; ++i)
      TARGET(60 + i, hint_focus_rect(gfx, ui, (HintKeyboardFocus)i),
             hint_focus_enabled(ui, (HintKeyboardFocus)i));
    if (hint_panel_blocks_point(gfx, ui, x, y)) return (PointerTarget){0};
  }
  for (int i = 0; i < LANG_COUNT; ++i)
    TARGET(70 + i, language_segment(gfx, ui, i), true);
  AppGeometry a;
  if (!get_geometry(gfx, ui, &a)) return (PointerTarget){0};
  if (ui->screen == SCR_TITLE) {
    for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
      TARGET(80 + i, sdl_rect(a.home_mode[i]), true);
      TARGET(90 + i, sdl_rect(a.home_difficulty[i]), true);
    }
    for (int i = 0; i < 3; ++i)
      TARGET(100 + i, sdl_rect(a.home_primary[i]), i != 1 || ui->has_session);
    for (int i = 0; i < 5; ++i)
      TARGET(110 + i, sdl_rect(a.home_secondary[i]), true);
  } else if (ui->screen == SCR_END) {
    for (int i = 0; i < 2; ++i) TARGET(120 + i, sdl_rect(a.end_buttons[i]), true);
  } else if (ui->screen == SCR_HELP || ui->screen == SCR_ABOUT) {
    TARGET(130, back_rect(gfx, ui), true);
    if (ui->screen == SCR_ABOUT) {
      for (int i = 0; i < 4; ++i) TARGET(131 + i, sdl_rect(a.about_links[i]), true);
      TARGET(135, sdl_rect(a.about_study_link), true);
    }
  } else if (ui->pause_reasons) {
    if (ui->pause_reasons & PAUSE_USER_VISIBLE)
      for (int i = 0; i < 2; ++i) TARGET(140 + i, sdl_rect(a.pause_buttons[i]), true);
  } else if (ui->screen == SCR_PLAY) {
    for (int i = 0; i < PLAY_FOCUS_COUNT; ++i)
      TARGET(200 + i, play_focus_rect(gfx, ui, i),
             play_focus_enabled(audio_game, ui, i));
  }
#undef TARGET
  return (PointerTarget){0};
}

/* Scroll owns only the Help body or an enabled library row. Action buttons,
   Back, paging, and disabled/scanning list surfaces never start a drag. */
static void ui_scroll_start(int x, int y) {
  if (!audio_gfx || !audio_ui || settings_open || ui_more_open ||
      audio_control_popup_open)
    return;
  if (audio_ui->screen == SCR_HELP) {
    AppGeometry geometry;
    if (get_geometry(audio_gfx, audio_ui, &geometry) &&
        point_in(sdl_rect(geometry.info_body), x, y)) {
      ui_scroll_kind = UI_SCROLL_HELP;
      touch_scroll_begin(&ui_scroll_gesture, x, y);
    }
  } else if (audio_ui->screen == APP_SCREEN_GAMES && audio_persistence &&
             !games_browser.scanning && !games_browser.detail_open &&
             games_browser.count > 0) {
    GamesBrowserLayout layout = games_browser_layout(audio_gfx);
    if (!point_in(layout.list, x, y) || layout.row_height <= 0) return;
    int row = (y - layout.list.y) / layout.row_height;
    size_t index = games_browser.scroll + (size_t)row;
    if (row >= layout.visible || index >= games_browser.count) return;
    ui_scroll_kind = UI_SCROLL_LIBRARY;
    ui_scroll_start_index = index;
    ui_scroll_start_id = games_browser.rows[index].entry.id;
    touch_scroll_begin(&ui_scroll_gesture, x, y);
  }
}

/* Preview loading is deferred until the current SDL event batch is drained,
   rather than decoding a game record for every finger/mouse motion. */
static void ui_scroll_flush_preview(void) {
  if (!ui_scroll_preview_pending) return;
  ui_scroll_preview_pending = false;
  if (audio_gfx && audio_persistence && audio_ui &&
      audio_ui->screen == APP_SCREEN_GAMES && !games_browser.scanning &&
      !games_browser.detail_open && games_browser.count)
    games_browser_preview_selected(audio_persistence);
}

static bool ui_scroll_motion(int x, int y) {
  if (ui_scroll_kind == UI_SCROLL_NONE) return false;
  if (ui_scroll_kind == UI_SCROLL_LIBRARY &&
      (games_browser.scanning || games_browser.detail_open ||
       ui_scroll_start_index >= games_browser.count ||
       !game_attempt_id_equal(
           games_browser.rows[ui_scroll_start_index].entry.id,
           ui_scroll_start_id))) {
    ui_scroll_cancel();
    ui_pointer_cancel();
    return true;
  }
  TouchScrollResult motion = touch_scroll_motion(&ui_scroll_gesture, x, y,
                                                    ui_touch_slop);
  if (!motion.scrolling) return false;
  ui_pointer_cancel();
  if (ui_scroll_kind == UI_SCROLL_HELP) {
    help_scroll_by(-motion.delta_y);
  } else if (ui_scroll_kind == UI_SCROLL_LIBRARY && audio_gfx) {
    if (!ui_scroll_started_selection) {
      games_browser.selected = ui_scroll_start_index;
      games_browser.focus = 0;
      games_browser_ensure_visible(audio_gfx);
      ui_scroll_started_selection = true;
      ui_scroll_preview_pending = true;
    }
    GamesBrowserLayout layout = games_browser_layout(audio_gfx);
    int steps = touch_scroll_rows(&ui_scroll_gesture, motion.delta_y,
                                 layout.row_height);
    if (steps != 0) {
      int64_t next = (int64_t)games_browser.selected - steps;
      if (next < 0) next = 0;
      if ((uint64_t)next >= games_browser.count)
        next = (int64_t)games_browser.count - 1;
      if ((size_t)next != games_browser.selected) {
        games_browser.selected = (size_t)next;
        games_browser_ensure_visible(audio_gfx);
        ui_scroll_preview_pending = true;
      }
    }
  }
  return true;
}

static bool ui_touch_board_cell(int x, int y, int *row, int *column) {
  if (!audio_gfx || !audio_ui || audio_ui->screen != SCR_PLAY ||
      settings_open || ui_more_open || audio_control_popup_open ||
      !app_play_interactions_allowed(audio_ui) ||
      hint_panel_blocks_point(audio_gfx, audio_ui, x, y))
    return false;
  Layout layout = compute_layout(audio_gfx->width, audio_gfx->height,
                                 audio_ui->mode);
  if (!point_in(layout.board, x, y)) return false;
  int side = layout.board.w;
  int cell = side / 9;
  if (cell <= 0) return false;
  int r = (y - layout.board.y) / cell;
  int c = (x - layout.board.x) / cell;
  if (r < 0 || r >= 9 || c < 0 || c >= 9) return false;
  if (row) *row = r;
  if (column) *column = c;
  return true;
}

/* Translate an accepted finger sample in place for the EXISTING semantic
   mouse handler. No event is enqueued or dispatched a second time. */
static bool ui_touch_prepare(SDL_Event *event) {
  ui_touch_replayed = false;
  ui_touch_board_tap = false;
  if (event->type == SDL_MOUSEMOTION &&
      event->motion.which == SDL_TOUCH_MOUSEID) return false;
  if ((event->type == SDL_MOUSEBUTTONDOWN ||
       event->type == SDL_MOUSEBUTTONUP) &&
      event->button.which == SDL_TOUCH_MOUSEID) return false;
  if (event->type == SDL_MOUSEBUTTONDOWN &&
      event->button.button == SDL_BUTTON_LEFT)
    ui_touch_cancel(); /* A real mouse has priority over an active finger. */

  if (event->type != SDL_FINGERDOWN && event->type != SDL_FINGERMOTION &&
      event->type != SDL_FINGERUP)
    return true;
  if (!audio_gfx || !audio_gfx->win || !audio_ui) return false;
  if (event->tfinger.touchId == SDL_MOUSE_TOUCHID) return false;
  ui_pointer_reconcile();
  SDL_TouchFingerEvent finger = event->tfinger;
  TouchInputPhase phase = event->type == SDL_FINGERDOWN ? TOUCH_INPUT_DOWN :
                          event->type == SDL_FINGERMOTION ? TOUCH_INPUT_MOTION :
                          TOUCH_INPUT_UP;
  TouchSample sample = {
      .touch_id = finger.touchId, .finger_id = finger.fingerId,
      .window_id = finger.windowID, .x = finger.x, .y = finger.y};
  TouchInputResult result = touch_input_event(
      &ui_touch_input, phase, sample, SDL_GetWindowID(audio_gfx->win),
      audio_gfx->width, audio_gfx->height, ui_touch_slop);
  if (!result.accepted) return false;
  if (!result.valid) {
    ui_pointer_cancel();
    ui_touch_cancel();
    return false;
  }
  if (phase == TOUCH_INPUT_DOWN) {
    ui_touch_board_pending =
        ui_touch_board_cell(result.x, result.y,
                            &ui_touch_board_row, &ui_touch_board_col);
    ui_touch_board_generation = ui_pointer_generation;
    if (ui_touch_board_pending) return false;
  } else if (ui_touch_board_pending) {
    if (phase == TOUCH_INPUT_MOTION) return false;
    ui_touch_board_pending = false;
    int row = -1, column = -1;
    if (!result.tap || ui_touch_board_generation != ui_pointer_generation ||
        !ui_touch_board_cell(result.x, result.y, &row, &column) ||
        row != ui_touch_board_row || column != ui_touch_board_col)
      return false;
    /* The existing board path acts on a down; only release a valid tap there. */
    ui_touch_board_tap = true;
    phase = TOUCH_INPUT_DOWN;
  } else if (phase == TOUCH_INPUT_MOTION) {
    if (result.slop_exceeded && !ui_scroll_gesture.active &&
        settings_slider_drag == SETTINGS_DRAG_NONE) {
      ui_pointer_cancel();
      audio_control_pressed = false;
    }
  } else if (!result.tap && settings_slider_drag == SETTINGS_DRAG_NONE) {
    ui_pointer_cancel();
    ui_scroll_cancel();
    audio_control_pressed = false;
    return false;
  }

  SDL_Event mapped = {0};
  ui_touch_replayed = true;
  if (phase == TOUCH_INPUT_MOTION) {
    mapped.type = SDL_MOUSEMOTION;
    mapped.motion.windowID = finger.windowID;
    mapped.motion.state = SDL_BUTTON_LMASK;
    mapped.motion.x = result.x;
    mapped.motion.y = result.y;
  } else {
    mapped.type = phase == TOUCH_INPUT_DOWN ? SDL_MOUSEBUTTONDOWN
                                             : SDL_MOUSEBUTTONUP;
    mapped.button.windowID = finger.windowID;
    mapped.button.button = SDL_BUTTON_LEFT;
    mapped.button.x = result.x;
    mapped.button.y = result.y;
  }
  *event = mapped;
  return true;
}

/* True means the original event may continue to its existing handler. */
static bool ui_pointer_prepare(SDL_Event *event) {
  ui_pointer_reconcile();
  if (event->type == SDL_WINDOWEVENT || event->type == SDL_QUIT ||
      event->type == SDL_RENDER_DEVICE_RESET || event->type == SDL_RENDER_TARGETS_RESET ||
      event->type == SDL_APP_WILLENTERBACKGROUND ||
      event->type == SDL_APP_DIDENTERBACKGROUND ||
      event->type == SDL_APP_TERMINATING ||
      event->type == SDL_KEYDOWN || event->type == SDL_MOUSEWHEEL) {
    ui_pointer_cancel();
    ui_touch_cancel();
    if (event->type == SDL_WINDOWEVENT) {
      settings_slider_drag = SETTINGS_DRAG_NONE;
      if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
          event->window.event == SDL_WINDOWEVENT_RESIZED ||
          event->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
          event->window.event == SDL_WINDOWEVENT_HIDDEN ||
          event->window.event == SDL_WINDOWEVENT_MINIMIZED)
        ui_more_close(audio_ui);
    }
  }
  if (event->type == SDL_MOUSEMOTION) {
    if (!(event->motion.state & SDL_BUTTON_LMASK)) {
      ui_pointer_cancel();
      ui_scroll_cancel();
    } else if (ui_scroll_motion(event->motion.x, event->motion.y)) {
      return false;
    } else {
      pointer_capture_motion(&ui_pointer_capture, event->motion.x,
                             event->motion.y);
    }
    return true;
  }
  if ((event->type != SDL_MOUSEBUTTONDOWN && event->type != SDL_MOUSEBUTTONUP) ||
      event->button.button != SDL_BUTTON_LEFT || !audio_gfx || !audio_ui)
    return true;
  int x = event->button.x, y = event->button.y;
  if (event->type == SDL_MOUSEBUTTONDOWN) {
    ui_pointer_cancel();
    ui_scroll_cancel();
    ui_scroll_start(x, y);
    /* The speaker owns its existing short/long-press gesture. */
    if (!ui_more_open && ui_audio_control_available(audio_ui) &&
        point_in(sdl_rect(ui_audio_control_geometry(audio_gfx, audio_ui).button), x, y))
      return true;
    if (settings_open) {
      SettingsPanelLayout a = ui_settings_panel_layout(audio_gfx);
      if (point_in(a.music_track, x, y) || point_in(a.fx_track, x, y)) return true;
    }
    PointerTarget target = ui_pointer_resolve(x, y);
    if (pointer_capture_begin(&ui_pointer_capture, target, x, y)) {
      ui_pointer_origin_x = x;
      ui_pointer_origin_y = y;
      ui_pointer_mouse_captured =
          !ui_touch_replayed && SDL_CaptureMouse(SDL_TRUE) == 0;
      return false;
    }
    /* Board selection/note entry remains immediate, including right click. */
    if (!settings_open && !audio_control_popup_open &&
        audio_ui->screen == SCR_PLAY && app_play_interactions_allowed(audio_ui) &&
        !hint_panel_blocks_point(audio_gfx, audio_ui, x, y)) {
      Layout a = compute_layout(audio_gfx->width, audio_gfx->height, audio_ui->mode);
      return point_in(a.board, x, y);
    }
    return false;
  }
  if (ui_scroll_gesture.active) {
    bool dragged = ui_scroll_gesture.dragging;
    ui_scroll_cancel();
    if (dragged) {
      ui_pointer_cancel();
      return false;
    }
  }
  if (audio_control_pressed || settings_slider_drag != SETTINGS_DRAG_NONE)
    return true;
  bool execute = pointer_capture_release(&ui_pointer_capture,
                                          ui_pointer_resolve(x, y), x, y);
  ui_pointer_cancel();
  if (execute) ui_feedback_pointer_press(audio_ui, x, y);
  return execute;
}


/* More is a modal interaction surface, not an underlying gameplay screen. */
static bool ui_handle_more_panel_event(SDL_Event *event) {
  if (!ui_more_open || !audio_ui || !audio_gfx || !event) return false;
  if (event->type == SDL_KEYDOWN) {
    if (event->key.repeat) return true;
    SDL_Keycode key=event->key.keysym.sym;
    if (key==SDLK_ESCAPE) {
      ui_more_close(audio_ui);
    } else if (key==SDLK_TAB || key==SDLK_UP || key==SDLK_DOWN) {
      int direction=key==SDLK_UP ||
          (key==SDLK_TAB && (event->key.keysym.mod & KMOD_SHIFT)) ? -1:1;
      ui_more_focus=(ui_more_focus+direction+GEOMETRY_MORE_COUNT)%
                     GEOMETRY_MORE_COUNT;
    } else if (input_activation_key(key)) {
      (void)ui_more_activate_action(audio_persistence,audio_game,audio_ui,
                                    audio_gfx,ui_more_focus);
    }
    return true;
  }
  if (event->type == SDL_MOUSEBUTTONUP &&
      event->button.button == SDL_BUTTON_LEFT) {
    AppGeometry geometry;
    if (get_geometry(audio_gfx,audio_ui,&geometry)) {
      for (int i=0;i<GEOMETRY_MORE_COUNT;++i) {
        if (point_in(sdl_rect(geometry.more_items[i]),
                     event->button.x,event->button.y)) {
          ui_more_focus=i;
          (void)ui_more_activate_action(audio_persistence,audio_game,audio_ui,
                                         audio_gfx,i);
          return true;
        }
      }
    }
    ui_more_close(audio_ui);
    return true;
  }
  return event->type == SDL_MOUSEBUTTONDOWN ||
         event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEWHEEL ||
         event->type == SDL_MOUSEMOTION;
}

static bool ui_prepare_event(SDL_Event *event) {
  if(!event)return false;
  if(audio_gfx)audio_gfx->dirty=true;
  if (!ui_touch_prepare(event)) return false;
  if (!ui_pointer_prepare(event)) return false;
  if (ui_handle_more_panel_event(event)) return false;
  if(event->type==SDL_WINDOWEVENT)ui_audio_control_cancel_interaction();

  if (event->type == SDL_AUDIODEVICEREMOVED && !event->adevice.iscapture) {
    audio_notify_device_removed();
    return false;
  }
  if (event->type == SDL_AUDIODEVICEADDED && !event->adevice.iscapture) {
    audio_notify_device_added();
    return false;
  }

  if (ui_handle_global_audio_event(event)) return false;
  if (ui_handle_settings_panel_event(event)) return false;

  if (event->type == SDL_KEYDOWN) {
    SDL_Keycode key = event->key.keysym.sym;
    if (event->key.repeat &&
        !input_key_repeat_allowed(key, event->key.keysym.scancode))
      return false;
    int digit = input_digit_value(key, event->key.keysym.scancode);

    if (audio_ui && audio_ui->screen == SCR_ABOUT && digit == 4) {
      ui_open_url(ui_support_url(audio_ui->language));
      return false;
    }

    if (digit >= 0 && ui_keypad_scancode(event->key.keysym.scancode))
      event->key.keysym.sym = (SDL_Keycode)(SDLK_0 + digit);
    return true;
  }

  if (event->type == SDL_MOUSEBUTTONUP && audio_gfx && audio_ui &&
      audio_ui->screen == SCR_ABOUT) {
    int x = event->button.x, y = event->button.y;
    AppGeometry geometry;
    if (get_geometry(audio_gfx, audio_ui, &geometry)) {
      if (point_in(sdl_rect(geometry.about_study_link), x, y)) {
        if (event->button.button == SDL_BUTTON_LEFT) {
          info_keyboard_focus = INFO_FOCUS_STUDY;
          ui_open_url("https://doi.org/10.1002/gps.5085");
        }
        return false;
      }
      if (point_in(sdl_rect(geometry.about_links[3]), x, y)) {
        if (event->button.button == SDL_BUTTON_LEFT) {
          info_keyboard_focus = INFO_FOCUS_LINK_3;
          ui_open_url(ui_support_url(audio_ui->language));
        }
        return false;
      }
    }
  }

  return true;
}

static int ui_poll_event(SDL_Event *event) {
  while (SDL_PollEvent(event))
    if (ui_prepare_event(event)) return 1;
  return 0;
}

static int ui_wait_event_timeout(SDL_Event *event,int timeout_ms) {
  if(!event)return 0;
  if(timeout_ms<0)timeout_ms=0;
  if(!SDL_WaitEventTimeout(event,timeout_ms))return 0;
  do{
    if(ui_prepare_event(event))return 1;
  }while(SDL_PollEvent(event));
  return 0;
}
