/* RC2 interaction/presentation corrections that stay outside Sudoku rules. */
typedef struct {
  SDL_Rect card;
  SDL_Rect title;
  SDL_Rect toggle;
  SDL_Rect music_label;
  SDL_Rect music_minus;
  SDL_Rect music_track;
  SDL_Rect music_plus;
  SDL_Rect fx_label;
  SDL_Rect fx_minus;
  SDL_Rect fx_track;
  SDL_Rect fx_plus;
  SDL_Rect back;
} AudioPanelLayout;

static Persistence *audio_persistence = NULL;
static Gfx *audio_gfx = NULL;
static UI *audio_ui = NULL;
static bool audio_settings_open = false;
static bool ui_home_cue_requested = false;

typedef struct {
  SDL_Rect banner;
  SDL_Rect message;
  SDL_Rect retry;
} StorageBannerLayout;

static StorageBannerLayout storage_banner_layout(const Gfx *gfx) {
  StorageBannerLayout layout;
  memset(&layout, 0, sizeof(layout));
  if (!gfx) return layout;
  int margin = gfx->width < 640 ? 8 : 14;
  int height = gfx->width < 640 ? 64 : 52;
  int width = gfx->width - margin * 2;
  if (width > 760) width = 760;
  layout.banner = (SDL_Rect){(gfx->width - width) / 2,
                             gfx->height - height - margin, width, height};
  int retry_w = gfx->width < 640 ? 116 : 150;
  layout.retry = (SDL_Rect){layout.banner.x + layout.banner.w - retry_w - 8,
                            layout.banner.y + 8, retry_w,
                            layout.banner.h - 16};
  layout.message = (SDL_Rect){layout.banner.x + 10, layout.banner.y + 7,
                              layout.retry.x - layout.banner.x - 18,
                              layout.banner.h - 14};
  return layout;
}

static bool storage_retry_hit(const Gfx *gfx, int x, int y) {
  if (!audio_persistence || !persistence_has_save_error(audio_persistence))
    return false;
  StorageBannerLayout layout = storage_banner_layout(gfx);
  return point_in(layout.retry, x, y);
}

static int ui_clamp_percent(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

static void ui_audio_set_music_level(int value) {
  int level=ui_clamp_percent(value);
  audio_set_music_volume(level);
  if(audio_persistence){
    audio_persistence->preferences.music_volume=(uint8_t)level;
    if(audio_ui)(void)save_preferences(audio_persistence,audio_ui);
    else (void)persistence_write_profile(audio_persistence);
  }
}

static void ui_audio_set_fx_level(int value) {
  int level=ui_clamp_percent(value);
  audio_set_fx_volume(level);
  if(audio_persistence){
    audio_persistence->preferences.fx_volume=(uint8_t)level;
    if(audio_ui)(void)save_preferences(audio_persistence,audio_ui);
    else (void)persistence_write_profile(audio_persistence);
  }
}

static void ui_audio_open(Persistence *persistence, const Game *game, UI *ui) {
  if (!ui || audio_settings_open || ui->screen != SCR_PLAY) return;
  clock_pause(ui, PAUSE_MODAL);
  if (persistence && game) (void)save_current_session(persistence, game, ui);
  if (!app_open_aux(ui, APP_SCREEN_SETTINGS)) {
    clock_resume(ui, PAUSE_MODAL);
    return;
  }
  audio_settings_open = true;
}

static void ui_audio_close(Persistence *persistence, const Game *game, UI *ui) {
  if (!ui || !audio_settings_open) return;
  audio_settings_open = false;
  (void)app_return_aux(ui);
  clock_resume(ui, PAUSE_MODAL);
  if (persistence && game) (void)save_current_session(persistence, game, ui);
}

static AudioPanelLayout ui_audio_panel_layout(const Gfx *gfx) {
  AudioPanelLayout layout;
  memset(&layout, 0, sizeof(layout));
  int margin = gfx->width < 640 ? 12 : 24;
  int width = gfx->width - margin * 2;
  if (width > 500) width = 500;
  int height = gfx->width < 640 ? 330 : 340;
  if (height > gfx->height - margin * 2) height = gfx->height - margin * 2;
  if (height < 280) height = 280;
  layout.card = (SDL_Rect){(gfx->width - width) / 2,
                           (gfx->height - height) / 2, width, height};
  int x = layout.card.x + 18;
  int w = layout.card.w - 36;
  int y = layout.card.y + 14;
  layout.title = (SDL_Rect){x, y, w, 42};
  y += 50;
  layout.toggle = (SDL_Rect){x, y, w, 42};
  y += 52;

  int label_h = 24;
  int control_h = 34;
  int button_w = 42;
  int control_gap = 8;
  layout.music_label = (SDL_Rect){x, y, w, label_h};
  y += label_h + 3;
  layout.music_minus = (SDL_Rect){x, y, button_w, control_h};
  layout.music_plus =
      (SDL_Rect){x + w - button_w, y, button_w, control_h};
  layout.music_track =
      (SDL_Rect){x + button_w + control_gap, y,
                 w - button_w * 2 - control_gap * 2, control_h};
  y += control_h + 10;

  layout.fx_label = (SDL_Rect){x, y, w, label_h};
  y += label_h + 3;
  layout.fx_minus = (SDL_Rect){x, y, button_w, control_h};
  layout.fx_plus = (SDL_Rect){x + w - button_w, y, button_w, control_h};
  layout.fx_track =
      (SDL_Rect){x + button_w + control_gap, y,
                 w - button_w * 2 - control_gap * 2, control_h};

  int back_h = 40;
  layout.back = (SDL_Rect){x, layout.card.y + layout.card.h - back_h - 14, w,
                           back_h};
  return layout;
}

static void rc2_draw_slider(Gfx *gfx, Theme theme, Language language,
                            SDL_Rect label_rect, SDL_Rect minus, SDL_Rect track,
                            SDL_Rect plus, TextKey label_key, int value,
                            int preferred) {
  char label[64];
  snprintf(label, sizeof label, "%s  %d%%", tr(language, label_key), value);
  (void)draw_fitted_text(gfx, label, theme.btnfg, label_rect, preferred, 10);

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  SDL_Rect buttons[2] = {minus, plus};
  const char *labels[2] = {"−", "+"};
  for (int i = 0; i < 2; ++i) {
    bool hover = point_in(buttons[i], mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    SDL_Rect face = polish_button_face(
        gfx, theme, buttons[i], hover ? theme.hover : theme.btn, pressed, false);
    (void)draw_fitted_text(gfx, labels[i], theme.btnfg, face, preferred + 2, 12);
  }

  SDL_Rect rail = {track.x + 6, track.y + track.h / 2 - 3, track.w - 12, 6};
  draw_round_rect(gfx->ren, rail, 3, theme.btn);
  int fill_w = rail.w * value / 100;
  if (fill_w > 0)
    draw_round_rect(gfx->ren, (SDL_Rect){rail.x, rail.y, fill_w, rail.h}, 3,
                    theme.title);
  int knob_x = rail.x + rail.w * value / 100;
  SDL_Rect knob = {knob_x - 5, track.y + track.h / 2 - 8, 10, 16};
  draw_round_rect(gfx->ren, knob, 5, theme.sel_outline);
}

static void rc2_render_audio_panel(Gfx *gfx, UI *ui) {
  if (!audio_settings_open || !gfx || !ui) return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  SDL_Color veil = theme.bg;
  veil.a = 220;
  draw_rect(gfx->ren, 0, 0, gfx->width, gfx->height, veil);

  AudioPanelLayout layout = ui_audio_panel_layout(gfx);
  SDL_Rect shadow = layout.card;
  shadow.y += 5;
  draw_round_rect(gfx->ren, shadow, 14, theme.shadow);
  draw_round_rect(gfx->ren, layout.card, 14, theme.palette_bg);
  AppGeometry geometry;
  int preferred = 18;
  if (get_geometry(gfx, ui, &geometry))
    preferred = geometry_font_sizes(&geometry, gfx->width, gfx->height).control;

  (void)draw_fitted_text(gfx, tr(ui->language, T_AUDIO), theme.title,
                         layout.title, preferred + 8, 14);

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool toggle_hover = point_in(layout.toggle, mx, my);
  bool toggle_pressed = toggle_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  bool enabled = audio_is_enabled();
  SDL_Color toggle_bg = enabled ? theme.title : theme.btn;
  if (toggle_hover && !enabled) toggle_bg = theme.hover;
  SDL_Rect toggle_face = polish_button_face(gfx, theme, layout.toggle, toggle_bg,
                                            toggle_pressed, false);
  char toggle_label[96];
  snprintf(toggle_label, sizeof toggle_label, "%s: %s",
           tr(ui->language, T_SOUND),
           tr(ui->language, enabled ? T_ON : T_OFF));
  (void)draw_fitted_text(gfx, toggle_label, enabled ? theme.bg : theme.btnfg,
                         toggle_face, preferred, 10);

  rc2_draw_slider(gfx, theme, ui->language, layout.music_label,
                  layout.music_minus, layout.music_track, layout.music_plus,
                  T_MUSIC, audio_music_volume(), preferred);
  rc2_draw_slider(gfx, theme, ui->language, layout.fx_label, layout.fx_minus,
                  layout.fx_track, layout.fx_plus, T_FX, audio_fx_volume(),
                  preferred);

  bool back_hover = point_in(layout.back, mx, my);
  bool back_pressed = back_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Rect back_face = polish_button_face(
      gfx, theme, layout.back, back_hover ? theme.hover : theme.btn,
      back_pressed, false);
  (void)draw_fitted_text(gfx, tr(ui->language, T_BACK), theme.btnfg, back_face,
                         preferred, 10);
}

static void rc2_render_play_controls(Gfx *gfx, const Game *game, UI *ui) {
  bool play_surface =
      ui && (ui->screen == SCR_PLAY ||
             (ui->screen == APP_SCREEN_SETTINGS && ui->prev_screen == SCR_PLAY));
  if (!gfx || !game || !play_surface || app_pause_hides_play(ui))
    return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  Layout layout = compute_layout(gfx->width, gfx->height, ui->mode);
  SidebarRects controls;
  compute_sidebar_rects(&layout, &controls);
  GeometryFonts fonts = geometry_font_sizes(&layout.model, gfx->width, gfx->height);
  const TextKey keys[PLAY_ACTION_COUNT] = {
      [PLAY_ACTION_MENU] = T_MENU,
      [PLAY_ACTION_PAUSE] = T_PAUSE,
      [PLAY_ACTION_RESTART] = T_RESTART,
      [PLAY_ACTION_HINT] = T_HINT,
      [PLAY_ACTION_NOTES] = T_NOTES,
      [PLAY_ACTION_VERIFY] = T_VERIFY,
      [PLAY_ACTION_AUDIO] = T_SOUND,
      [PLAY_ACTION_HELP] = T_HELP,
      [PLAY_ACTION_ABOUT] = T_ABOUT,
  };

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  for (int action = 0; action < PLAY_ACTION_COUNT; ++action) {
    SDL_Rect rect = controls.btn[action];
    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    bool active = (action == PLAY_ACTION_NOTES && ui->notes_mode) ||
                  (action == PLAY_ACTION_AUDIO && audio_is_enabled());
    bool primary = action == PLAY_ACTION_MENU;
    SDL_Color background = primary
                               ? theme.title
                               : (active ? theme.title
                                         : (hover ? theme.hover : theme.palette_bg));
    SDL_Color foreground = (primary || active) ? theme.bg : theme.btnfg;
    SDL_Rect face =
        polish_button_face(gfx, theme, rect, background, pressed, false);
    (void)draw_fitted_text(gfx, tr(ui->language, keys[action]), foreground,
                           face, fonts.control, 10);
  }

  SDL_Color separator = theme.dim;
  separator.a = 42;
  if (controls.palette_label.y > controls.btn[PLAY_ACTION_HELP].y)
    draw_line(gfx->ren, layout.side.x + 12, controls.palette_label.y - 4,
              layout.side.x + layout.side.w - 13, controls.palette_label.y - 4,
              separator);
  if (controls.btn[PLAY_ACTION_ABOUT].y > controls.progress.y)
    draw_line(gfx->ren, layout.side.x + 12,
              controls.btn[PLAY_ACTION_ABOUT].y - 4,
              layout.side.x + layout.side.w - 13,
              controls.btn[PLAY_ACTION_ABOUT].y - 4, separator);
}

static void rc2_render_storage_banner(Gfx *gfx, UI *ui) {
  if (!gfx || !ui || !audio_persistence ||
      !persistence_has_save_error(audio_persistence))
    return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  StorageBannerLayout layout = storage_banner_layout(gfx);
  SDL_Color background = theme.palette_bg;
  SDL_Color outline = theme.text_wrong;
  draw_round_rect(gfx->ren, layout.banner, 10, background);
  SDL_SetRenderDrawColor(gfx->ren, outline.r, outline.g, outline.b, 255);
  SDL_RenderDrawRect(gfx->ren, &layout.banner);
  (void)draw_fitted_text(gfx, tr(ui->language, T_SAVE_FAILED), theme.btnfg,
                         layout.message, 15, 9);

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool hover = point_in(layout.retry, mx, my);
  bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Rect face = polish_button_face(
      gfx, theme, layout.retry, hover ? theme.hover : theme.title, pressed,
      false);
  (void)draw_fitted_text(gfx, tr(ui->language, T_RETRY_SAVE), theme.bg, face,
                         14, 9);
}

static void app_render_ui(Gfx *gfx, const Game *game, UI *ui) {
  app_render_polish(gfx, game, ui);
  rc2_render_play_controls(gfx, game, ui);
  rc2_render_audio_panel(gfx, ui);
  rc2_render_storage_banner(gfx, ui);
}

static void go_home(Persistence *persistence, const Game *game, UI *ui) {
  bool entering_home = ui && ui->screen != SCR_TITLE;
  audio_settings_open = false;
  go_home_polish(persistence, game, ui);
  if (persistence && ui)
    ui->has_session = persistence->enabled && session_can_continue(&persistence->saved);
  if (entering_home) ui_home_cue_requested = true;
}
