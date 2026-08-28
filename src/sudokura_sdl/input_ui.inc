/* Normalize physical keypad input and route presentation-only controls before
   the legacy main event loop. Gameplay actions still flow through their normal
   keyboard paths, so pointer and keyboard behavior stay consistent. */
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

static void ui_make_keydown(SDL_Event *event, SDL_Keycode key) {
  memset(event, 0, sizeof(*event));
  event->type = SDL_KEYDOWN;
  event->key.type = SDL_KEYDOWN;
  event->key.state = SDL_PRESSED;
  event->key.repeat = 0;
  event->key.keysym.sym = key;
  event->key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
  event->key.keysym.mod = KMOD_NONE;
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

static int ui_poll_event(SDL_Event *event) {
  while (SDL_PollEvent(event)) {
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

    if (event->type == SDL_MOUSEBUTTONDOWN && audio_gfx && audio_ui) {
      int x = event->button.x, y = event->button.y;

      if (audio_ui->screen == SCR_PLAY && audio_ui->pause_reasons == 0) {
        Layout layout = compute_layout(audio_gfx->width, audio_gfx->height,
                                       audio_ui->mode);
        SidebarRects controls;
        compute_sidebar_rects(&layout, &controls);

        if (point_in(controls.btn[1], x, y)) {
          if (event->button.button == SDL_BUTTON_LEFT) {
            ui_make_keydown(event, SDLK_p);
            return 1;
          }
          continue;
        }
        if (point_in(controls.btn[5], x, y)) {
          if (event->button.button == SDL_BUTTON_LEFT)
            audio_toggle(audio_persistence);
          continue;
        }
      }

      if (audio_ui->screen == SCR_ABOUT) {
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
    }

    return 1;
  }
  return 0;
}
