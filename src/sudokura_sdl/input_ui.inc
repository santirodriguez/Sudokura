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

static bool ui_handle_audio_panel_event(SDL_Event *event) {
  if (!audio_settings_open || !audio_gfx || !audio_ui) return false;

  if (event->type == SDL_KEYDOWN) {
    if (event->key.keysym.sym == SDLK_v) {
      audio_toggle(audio_persistence);
      return true;
    }
    if (event->key.keysym.sym == SDLK_ESCAPE) {
      ui_audio_close(audio_persistence, NULL, audio_ui);
      return true;
    }
    return true;
  }

  if (event->type == SDL_MOUSEBUTTONDOWN) {
    if (event->button.button != SDL_BUTTON_LEFT) return true;
    AudioPanelLayout layout = ui_audio_panel_layout(audio_gfx);
    int x = event->button.x, y = event->button.y;
    if (point_in(layout.toggle, x, y)) {
      audio_toggle(audio_persistence);
    } else if (point_in(layout.music_minus, x, y)) {
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_audio_set_music_level(audio_music_volume() - 5);
    } else if (point_in(layout.music_plus, x, y)) {
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_audio_set_music_level(audio_music_volume() + 5);
    } else if (point_in(layout.music_track, x, y)) {
      ui_audio_set_music_level(ui_slider_value(layout.music_track, x));
    } else if (point_in(layout.fx_minus, x, y)) {
      ui_audio_set_fx_level(audio_fx_volume() - 5);
      audio_play_effect(AUDIO_EFFECT_POSITIVE);
    } else if (point_in(layout.fx_plus, x, y)) {
      ui_audio_set_fx_level(audio_fx_volume() + 5);
      audio_play_effect(AUDIO_EFFECT_POSITIVE);
    } else if (point_in(layout.fx_track, x, y)) {
      ui_audio_set_fx_level(ui_slider_value(layout.fx_track, x));
      audio_play_effect(AUDIO_EFFECT_POSITIVE);
    } else if (point_in(layout.back, x, y)) {
      audio_play_effect(AUDIO_EFFECT_CLICK);
      ui_audio_close(audio_persistence, NULL, audio_ui);
    }
    return true;
  }

  if (event->type == SDL_QUIT || event->type == SDL_WINDOWEVENT) return false;
  return true;
}

static int ui_poll_event(SDL_Event *event) {
  while (SDL_PollEvent(event)) {
    if (ui_handle_audio_panel_event(event)) continue;

    if (event->type == SDL_KEYDOWN) {
      SDL_Keycode key = event->key.keysym.sym;
      int digit = input_digit_value(key, event->key.keysym.scancode);

      if (key == SDLK_v) {
        audio_toggle(audio_persistence);
        continue;
      }

      if (audio_ui && audio_ui->screen == SCR_ABOUT && digit == 4) {
        ui_open_url(ui_support_url(audio_ui->language));
        continue;
      }

      if (digit >= 0 && ui_keypad_scancode(event->key.keysym.scancode))
        event->key.keysym.sym = (SDL_Keycode)(SDLK_0 + digit);
      return 1;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && audio_gfx && audio_ui &&
        audio_ui->screen == SCR_ABOUT) {
      int x = event->button.x, y = event->button.y;
      AppGeometry geometry;
      if (get_geometry(audio_gfx, audio_ui, &geometry)) {
        if (point_in(sdl_rect(geometry.about_study_link), x, y)) {
          if (event->button.button == SDL_BUTTON_LEFT)
            ui_open_url("https://doi.org/10.1002/gps.5085");
          continue;
        }
        if (point_in(sdl_rect(geometry.about_links[3]), x, y)) {
          if (event->button.button == SDL_BUTTON_LEFT)
            ui_open_url(ui_support_url(audio_ui->language));
          continue;
        }
      }
    }

    return 1;
  }
  return 0;
}
