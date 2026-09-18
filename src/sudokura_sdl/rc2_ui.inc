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
    else persistence_stage_preferences(audio_persistence);
  }
}

static void ui_audio_set_fx_level(int value) {
  int level=ui_clamp_percent(value);
  audio_set_fx_volume(level);
  if(audio_persistence){
    audio_persistence->preferences.fx_volume=(uint8_t)level;
    if(audio_ui)(void)save_preferences(audio_persistence,audio_ui);
    else persistence_stage_preferences(audio_persistence);
  }
}

static void ui_audio_open(Persistence *persistence, const Game *game, UI *ui) {
  if (!ui || audio_settings_open || ui->screen != SCR_PLAY) return;
  clock_pause(ui, PAUSE_MODAL);
  if (persistence && game) (void)checkpoint_current_session(persistence, game, ui);
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
  if (persistence && game) (void)checkpoint_current_session(persistence, game, ui);
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

typedef struct {
  SDL_Rect card;
  SDL_Rect title;
  SDL_Rect body;
  SDL_Rect primary;
  SDL_Rect verify;
  SDL_Rect reveal;
  SDL_Rect cancel;
} HintPanelLayout;

static TextKey hint_technique_key(HumanTechnique technique) {
  switch (technique) {
    case HUMAN_TECHNIQUE_NAKED_SINGLE: return T_TECH_NAKED_SINGLE;
    case HUMAN_TECHNIQUE_HIDDEN_SINGLE: return T_TECH_HIDDEN_SINGLE;
    case HUMAN_TECHNIQUE_LOCKED_CANDIDATE: return T_TECH_LOCKED_CANDIDATE;
    case HUMAN_TECHNIQUE_NAKED_PAIR: return T_TECH_NAKED_PAIR;
    case HUMAN_TECHNIQUE_NAKED_TRIPLE: return T_TECH_NAKED_TRIPLE;
    case HUMAN_TECHNIQUE_X_WING: return T_TECH_X_WING;
    case HUMAN_TECHNIQUE_NONE:
    default:
      return T_HINT_PREVIEW;
  }
}

static HintPanelLayout hint_panel_layout(const Gfx *gfx, const UI *ui) {
  HintPanelLayout panel;
  memset(&panel, 0, sizeof(panel));
  if (!gfx || !ui) return panel;
  Layout layout = compute_layout(gfx->width, gfx->height, ui->mode);
  SDL_Rect side = layout.side;
  int margin = side.w < 240 ? 7 : 10;
  panel.card = (SDL_Rect){side.x + margin, side.y + margin,
                          side.w - margin * 2, side.h - margin * 2};
  int x = panel.card.x + 10;
  int width = panel.card.w - 20;
  int y = panel.card.y + 10;
  panel.title = (SDL_Rect){x, y, width, 32};
  int button_h = panel.card.h < 430 ? 31 : 36;
  int gap = 6;
  panel.cancel = (SDL_Rect){x, panel.card.y + panel.card.h - button_h - 10,
                            width, button_h};
  panel.reveal = (SDL_Rect){x, panel.cancel.y - button_h - gap,
                            width, button_h};
  panel.verify = (SDL_Rect){x, panel.reveal.y - button_h - gap,
                            width, button_h};
  panel.primary = (SDL_Rect){x, panel.verify.y - button_h - gap,
                             width, button_h};
  panel.body = (SDL_Rect){x, panel.title.y + panel.title.h + 4, width,
                          panel.primary.y -
                              (panel.title.y + panel.title.h + 8)};
  return panel;
}

static bool hint_panel_hit(const Gfx *gfx, const UI *ui, SDL_Rect rect,
                           int x, int y) {
  (void)gfx;
  return ui && ui->hint_preview_active && rect.w > 0 && rect.h > 0 &&
         point_in(rect, x, y);
}

static bool hint_panel_blocks_point(const Gfx *gfx, const UI *ui, int x, int y) {
  HintPanelLayout panel = hint_panel_layout(gfx, ui);
  return ui && ui->hint_preview_active && point_in(panel.card, x, y);
}

static bool hint_apply_hit(const Gfx *gfx, const UI *ui, int x, int y) {
  HintPanelLayout panel = hint_panel_layout(gfx, ui);
  return ui && ui->hint_preview.status == HUMAN_HINT_LOGICAL &&
         hint_panel_hit(gfx, ui, panel.primary, x, y);
}

static bool hint_verify_hit(const Gfx *gfx, const UI *ui, int x, int y) {
  HintPanelLayout panel = hint_panel_layout(gfx, ui);
  return ui &&
         (ui->hint_preview.status == HUMAN_HINT_INVALID ||
          ui->hint_preview.status == HUMAN_HINT_STALLED) &&
         hint_panel_hit(gfx, ui, panel.verify, x, y);
}

static bool hint_reveal_hit(const Gfx *gfx, const UI *ui, int x, int y) {
  HintPanelLayout panel = hint_panel_layout(gfx, ui);
  return ui && ui->hint_preview.status != HUMAN_HINT_INVALID &&
         hint_panel_hit(gfx, ui, panel.reveal, x, y);
}

static bool hint_cancel_hit(const Gfx *gfx, const UI *ui, int x, int y) {
  HintPanelLayout panel = hint_panel_layout(gfx, ui);
  return hint_panel_hit(gfx, ui, panel.cancel, x, y);
}

static void hint_preview_text(const UI *ui, char *buffer, size_t size) {
  if (!ui || !buffer || size == 0) return;
  buffer[0] = '\0';
  if (ui->hint_preview.status == HUMAN_HINT_INVALID) {
    snprintf(buffer, size, "%s", tr(ui->language, T_HINT_CONTRADICTION));
    return;
  }
  if (ui->hint_preview.status == HUMAN_HINT_STALLED) {
    snprintf(buffer, size, "%s",
             tr(ui->language, ui->hint_verify_required
                                  ? T_HINT_NEEDS_VERIFY
                                  : T_HINT_STALLED));
    return;
  }
  if (ui->hint_preview.status != HUMAN_HINT_LOGICAL) return;

  const HumanStep *reasoning = &ui->hint_preview.reasoning;
  const HumanStep *placement = &ui->hint_preview.placement;
  const char *technique =
      tr(ui->language, hint_technique_key(reasoning->technique));
  TextKey format = ui->hint_preview.reasoning_steps <= 1
                       ? T_HINT_DIRECT_FMT
                       : T_HINT_CHAIN_FMT;
  char deduction[220];
  snprintf(deduction, sizeof(deduction), tr(ui->language, format), technique,
           placement->placement_value, placement->placement_cell / 9 + 1,
           placement->placement_cell % 9 + 1);
  snprintf(buffer, size, "%s\n%s", deduction,
           tr(ui->language, T_HINT_MARKER_LEGEND));
}

static void hint_panel_button(Gfx *gfx, Theme theme, SDL_Rect rect,
                              const char *label, bool primary) {
  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool hover = point_in(rect, mx, my);
  bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Color background =
      primary ? theme.title : (hover ? theme.hover : theme.btn);
  SDL_Color foreground = primary ? theme.bg : theme.btnfg;
  SDL_Rect face =
      polish_button_face(gfx, theme, rect, background, pressed, false);
  (void)draw_fitted_text(gfx, label, foreground, face, 16, 9);
}

static void rc2_render_hint_panel(Gfx *gfx, UI *ui) {
  if (!gfx || !ui || !ui->hint_preview_active || ui->screen != SCR_PLAY ||
      app_pause_hides_play(ui))
    return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  HintPanelLayout panel = hint_panel_layout(gfx, ui);
  draw_round_rect(gfx->ren, panel.card, 10, theme.palette_bg);
  SDL_SetRenderDrawColor(gfx->ren, theme.sel_outline.r, theme.sel_outline.g,
                         theme.sel_outline.b, 255);
  SDL_RenderDrawRect(gfx->ren, &panel.card);

  (void)draw_fitted_text(gfx, tr(ui->language, T_HINT_PREVIEW), theme.title,
                         panel.title, 20, 10);
  char message[320];
  hint_preview_text(ui, message, sizeof(message));
  if (panel.body.h > 8)
    (void)polish_draw_wrapped_fit(gfx, message, theme.btnfg, panel.body, 15, 9,
                                  false);

  if (ui->hint_preview.status == HUMAN_HINT_LOGICAL)
    hint_panel_button(gfx, theme, panel.primary, tr(ui->language, T_APPLY),
                      true);
  if (ui->hint_preview.status == HUMAN_HINT_INVALID ||
      ui->hint_preview.status == HUMAN_HINT_STALLED)
    hint_panel_button(gfx, theme, panel.verify, tr(ui->language, T_VERIFY),
                      true);
  if (ui->hint_preview.status != HUMAN_HINT_INVALID)
    hint_panel_button(gfx, theme, panel.reveal,
                      tr(ui->language, T_REVEAL_CELL), false);
  hint_panel_button(gfx, theme, panel.cancel, tr(ui->language, T_CANCEL),
                    false);
}

static void split_control_rects(SDL_Rect combined, SDL_Rect *left,
                                SDL_Rect *right) {
  int gap = combined.w >= 180 ? 6 : 4;
  int left_w = (combined.w - gap) / 2;
  if (left)
    *left = (SDL_Rect){combined.x, combined.y, left_w, combined.h};
  if (right)
    *right = (SDL_Rect){combined.x + left_w + gap, combined.y,
                        combined.w - left_w - gap, combined.h};
}

static void menu_pause_rects(SDL_Rect combined, SDL_Rect *menu,
                             SDL_Rect *pause) {
  split_control_rects(combined, menu, pause);
}

static void undo_redo_rects(SDL_Rect combined, SDL_Rect *undo,
                            SDL_Rect *redo) {
  split_control_rects(combined, undo, redo);
}

static void restart_clear_rects(SDL_Rect combined, SDL_Rect *restart,
                                SDL_Rect *clear) {
  split_control_rects(combined, restart, clear);
}

static bool clear_selected_available(const Game *game, const UI *ui) {
  if (!game || !ui || !ui->session_open || ui->result != RES_NONE ||
      game_cell_locked(game, ui->sel_r, ui->sel_c))
    return false;
  int index = IDX(ui->sel_r, ui->sel_c);
  return game->puzzle[index] != 0 || game->notes[index] != 0;
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
    if (action == PLAY_ACTION_MENU) {
      SDL_Rect menu_rect, pause_rect;
      menu_pause_rects(rect, &menu_rect, &pause_rect);
      bool menu_hover = point_in(menu_rect, mx, my);
      bool pause_hover = point_in(pause_rect, mx, my);
      SDL_Rect menu_face = polish_button_face(
          gfx, theme, menu_rect, theme.title,
          menu_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT)), false);
      SDL_Rect pause_face = polish_button_face(
          gfx, theme, pause_rect,
          pause_hover ? theme.hover : theme.palette_bg,
          pause_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT)), false);
      (void)draw_fitted_text(gfx, tr(ui->language, T_MENU), theme.bg,
                             menu_face, fonts.control, 9);
      (void)draw_fitted_text(gfx, tr(ui->language, T_PAUSE), theme.btnfg,
                             pause_face, fonts.control, 9);
      continue;
    }

    if (action == PLAY_ACTION_PAUSE) {
      SDL_Rect undo_rect, redo_rect;
      undo_redo_rects(rect, &undo_rect, &redo_rect);
      bool enabled[2] = {ui->undo_count > 0, ui->redo_count > 0};
      SDL_Rect parts[2] = {undo_rect, redo_rect};
      TextKey labels[2] = {T_UNDO, T_REDO};
      for (int i = 0; i < 2; ++i) {
        bool hover = enabled[i] && point_in(parts[i], mx, my);
        SDL_Rect face = polish_button_face(
            gfx, theme, parts[i],
            hover ? theme.hover : theme.palette_bg,
            hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT)), !enabled[i]);
        (void)draw_fitted_text(gfx, tr(ui->language, labels[i]),
                               enabled[i] ? theme.btnfg : theme.dim,
                               face, fonts.control, 9);
      }
      continue;
    }

    if (action == PLAY_ACTION_RESTART) {
      SDL_Rect restart_rect, clear_rect;
      restart_clear_rects(rect, &restart_rect, &clear_rect);
      bool restart_hover = point_in(restart_rect, mx, my);
      bool restart_pressed =
          restart_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
      SDL_Rect restart_face = polish_button_face(
          gfx, theme, restart_rect,
          restart_hover ? theme.hover : theme.palette_bg, restart_pressed,
          false);
      (void)draw_fitted_text(gfx, tr(ui->language, T_RESTART), theme.btnfg,
                             restart_face, fonts.control, 9);

      bool clear_enabled = clear_selected_available(game, ui);
      bool clear_hover = clear_enabled && point_in(clear_rect, mx, my);
      bool clear_pressed =
          clear_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
      SDL_Rect clear_face = polish_button_face(
          gfx, theme, clear_rect,
          clear_hover ? theme.hover : theme.palette_bg, clear_pressed,
          !clear_enabled);
      (void)draw_fitted_text(gfx, tr(ui->language, T_CLEAR),
                             clear_enabled ? theme.btnfg : theme.dim,
                             clear_face, fonts.control, 9);
      continue;
    }

    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    bool active = (action == PLAY_ACTION_NOTES && ui->notes_mode) ||
                  (action == PLAY_ACTION_AUDIO && audio_is_enabled());
    SDL_Color background =
        active ? theme.title : (hover ? theme.hover : theme.palette_bg);
    SDL_Color foreground = active ? theme.bg : theme.btnfg;
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

static void rc2_render_generation_overlay(Gfx *gfx, UI *ui) {
  if (!gfx || !ui || !ui->generating) return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  SDL_Color veil = theme.bg;
  veil.a = 225;
  draw_rect(gfx->ren, 0, 0, gfx->width, gfx->height, veil);

  int width = gfx->width < 640 ? gfx->width - 32 : 460;
  if (width > gfx->width - 32) width = gfx->width - 32;
  int height = gfx->width < 640 ? 132 : 150;
  SDL_Rect card = {(gfx->width - width) / 2, (gfx->height - height) / 2,
                   width, height};
  SDL_Rect shadow = card;
  shadow.y += 5;
  draw_round_rect(gfx->ren, shadow, 14, theme.shadow);
  draw_round_rect(gfx->ren, card, 14, theme.palette_bg);

  char message[128];
  if (ui->generation_max_attempts > 0) {
    snprintf(message, sizeof message, "%s  %u/%u",
             tr(ui->language, T_GENERATING), ui->generation_attempt,
             ui->generation_max_attempts);
  } else {
    snprintf(message, sizeof message, "%s",
             tr(ui->language, T_GENERATING));
  }
  SDL_Rect title = {card.x + 18, card.y + 24, card.w - 36, 48};
  SDL_Rect cancel = {card.x + 18, card.y + 82, card.w - 36, 34};
  (void)draw_fitted_text(gfx, message, theme.title, title, 24, 12);
  (void)draw_fitted_text(gfx, tr(ui->language, T_GENERATING_CANCEL),
                         theme.dim, cancel, 16, 10);
}

static void app_render_ui(Gfx *gfx, const Game *game, UI *ui) {
  app_render_polish(gfx, game, ui);
  rc2_render_play_controls(gfx, game, ui);
  rc2_render_hint_panel(gfx, ui);
  rc2_render_audio_panel(gfx, ui);
  rc2_render_storage_banner(gfx, ui);
  rc2_render_generation_overlay(gfx, ui);
}

static void go_home(Persistence *persistence, const Game *game, UI *ui) {
  bool entering_home = ui && ui->screen != SCR_TITLE;
  audio_settings_open = false;
  go_home_polish(persistence, game, ui);
  if (persistence && ui) refresh_resume_info(persistence, ui);
  if (entering_home) ui_home_cue_requested = true;
}
