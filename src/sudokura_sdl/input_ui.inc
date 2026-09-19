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
  if (!url || !audio_gfx || !audio_ui) return;
  audio_play_effect(AUDIO_EFFECT_CLICK);
  if (SDL_OpenURL(url) != 0)
    (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                   tr(audio_ui->language, T_ABOUT),
                                   tr(audio_ui->language, T_LINK_ERROR),
                                   audio_gfx->win);
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
    settings_slider_drag = SETTINGS_DRAG_NONE;
    return true;
  }
  if (event->type == SDL_MOUSEBUTTONDOWN) {
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

static bool ui_prepare_event(SDL_Event *event) {
  if(!event)return false;
  if(audio_gfx)audio_gfx->dirty=true;

  if (event->type == SDL_AUDIODEVICEREMOVED && !event->adevice.iscapture) {
    audio_notify_device_removed();
    return false;
  }
  if (event->type == SDL_AUDIODEVICEADDED && !event->adevice.iscapture) {
    audio_notify_device_added();
    return false;
  }

  if (ui_handle_settings_panel_event(event)) return false;

  if (event->type == SDL_KEYDOWN) {
    SDL_Keycode key = event->key.keysym.sym;
    if (event->key.repeat &&
        !input_key_repeat_allowed(key, event->key.keysym.scancode))
      return false;
    int digit = input_digit_value(key, event->key.keysym.scancode);

    if (key == SDLK_v) {
      audio_toggle(audio_persistence);
      return false;
    }

    if (audio_ui && audio_ui->screen == SCR_ABOUT && digit == 4) {
      ui_open_url(ui_support_url(audio_ui->language));
      return false;
    }

    if (digit >= 0 && ui_keypad_scancode(event->key.keysym.scancode))
      event->key.keysym.sym = (SDL_Keycode)(SDLK_0 + digit);
    return true;
  }

  if (event->type == SDL_MOUSEBUTTONDOWN && audio_gfx && audio_ui &&
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
