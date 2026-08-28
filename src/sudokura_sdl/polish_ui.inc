/* Final v1.2 presentation polish kept separate from game rules. */
static SDL_Texture *app_icon_texture = NULL;

static SDL_Rect polish_button_face(Gfx *gfx, Theme theme, SDL_Rect rect,
                                   SDL_Color background, bool pressed,
                                   bool disabled) {
  SDL_Rect face = rect;
  if (!disabled && !pressed && rect.h > 5) {
    SDL_Rect shadow = rect;
    shadow.y += 3;
    shadow.h -= 1;
    draw_round_rect(gfx->ren, shadow, 7, theme.shadow);
    face.h -= 3;
  } else if (!disabled && pressed && face.h > 4) {
    face.y += 2;
    face.h -= 2;
  }
  draw_round_rect(gfx->ren, face, 7, background);
  if (!disabled && face.w > 18 && face.h > 8) {
    SDL_Color highlight = theme.sel_outline;
    highlight.a = 55;
    draw_line(gfx->ren, face.x + 9, face.y + 1,
              face.x + face.w - 10, face.y + 1, highlight);
  }
  return face;
}

static void polish_draw_icon(Gfx *gfx, SDL_Rect box) {
  if (!app_icon_texture || box.w <= 0 || box.h <= 0) return;
  int size = box.w < box.h ? box.w : box.h;
  size -= 8;
  if (size < 12) return;
  SDL_Rect dst = {box.x + (box.w - size) / 2, box.y + (box.h - size) / 2,
                  size, size};
  SDL_RenderCopy(gfx->ren, app_icon_texture, NULL, &dst);
}

static bool polish_draw_wrapped_fit(Gfx *gfx, const char *text, SDL_Color color,
                                    SDL_Rect rect, int preferred, int minimum,
                                    bool centered) {
  if (!text || !text[0] || rect.w <= 8 || rect.h <= 4) return false;
  for (int i = FONT_CACHE_COUNT - 1; i >= 0; --i) {
    int size = font_cache_sizes[i];
    if (size > preferred || size < minimum) continue;
    int tw = 0, th = 0;
    SDL_Texture *texture = render_text_wrapped(
        gfx, gfx->fonts[i], text, color, rect.w - 8, &tw, &th);
    if (!texture) continue;
    bool fits = tw <= rect.w - 4 && th <= rect.h - 2;
    if (fits) {
      SDL_Rect dst = {rect.x + 4,
                      centered ? rect.y + (rect.h - th) / 2 : rect.y + 1,
                      tw, th};
      SDL_RenderCopy(gfx->ren, texture, NULL, &dst);
      SDL_DestroyTexture(texture);
      return true;
    }
    SDL_DestroyTexture(texture);
  }
  return false;
}

static int polish_home_bottom(const AppGeometry *geometry) {
  int bottom = geometry->home_logo.y + geometry->home_logo.h;
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i) {
    int candidate = geometry->home_secondary[i].y + geometry->home_secondary[i].h;
    if (candidate > bottom) bottom = candidate;
  }
  return bottom;
}

static void polish_home_button(Gfx *gfx, Theme theme, SDL_Rect rect,
                               const char *label, bool primary, bool disabled,
                               bool focused, int preferred) {
  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool hover = !disabled && point_in(rect, mx, my);
  bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Color background =
      primary ? theme.title : (hover ? theme.hover : theme.btn);
  SDL_Color foreground = primary ? theme.bg : (disabled ? theme.dim : theme.btnfg);
  if (disabled) background = theme.palette_bg;
  SDL_Rect face =
      polish_button_face(gfx, theme, rect, background, pressed, disabled);
  (void)draw_fitted_text(gfx, label, foreground, face, preferred, 10);
  if (focused && !disabled) draw_focus_ring(gfx->ren, rect, theme.sel_outline);
}

static void polish_render_home(Gfx *gfx, UI *ui) {
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  AppGeometry geometry;
  if (!get_geometry(gfx, ui, &geometry)) return;
  GeometryFonts fonts = geometry_font_sizes(&geometry, gfx->width, gfx->height);

  if (gfx->width >= 640) {
    SDL_Rect card = {geometry.home_logo.x - 18, geometry.home_logo.y - 16,
                     geometry.home_logo.w + 36,
                     polish_home_bottom(&geometry) - geometry.home_logo.y + 32};
    SDL_Rect shadow = card;
    shadow.y += 5;
    draw_round_rect(gfx->ren, shadow, 14, theme.shadow);
    draw_round_rect(gfx->ren, card, 14, theme.palette_bg);
  }

  draw_wordmark(gfx, sdl_rect(geometry.home_logo));
  (void)draw_fitted_text(gfx, tr(ui->language, T_MODE), theme.dim,
                         sdl_rect(geometry.home_mode_label), fonts.help, 10);

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    SDL_Rect rect = sdl_rect(geometry.home_mode[i]);
    bool active = i == (int)ui->mode;
    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    SDL_Color background = active ? theme.title : (hover ? theme.hover : theme.btn);
    SDL_Color foreground = active ? theme.bg : theme.btnfg;
    SDL_Rect face = polish_button_face(gfx, theme, rect, background, pressed, false);
    (void)draw_fitted_text(gfx, tr(ui->language, mode_key((Mode)i)), foreground,
                           face, fonts.control, 10);
    if (home_focus == HOME_FOCUS_MODE && active)
      draw_focus_ring(gfx->ren, rect, theme.sel_outline);
  }

  (void)draw_fitted_text(gfx, tr(ui->language, T_DIFFICULTY), theme.dim,
                         sdl_rect(geometry.home_difficulty_label), fonts.help, 10);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    SDL_Rect rect = sdl_rect(geometry.home_difficulty[i]);
    bool active = i == (int)home_difficulty;
    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    SDL_Color background = active ? theme.title : (hover ? theme.hover : theme.btn);
    SDL_Color foreground = active ? theme.bg : theme.btnfg;
    SDL_Rect face = polish_button_face(gfx, theme, rect, background, pressed, false);
    (void)draw_fitted_text(gfx, difficulty_name(ui->language, (GameDifficulty)i),
                           foreground, face, fonts.control, 10);
    if (home_focus == HOME_FOCUS_DIFFICULTY && active)
      draw_focus_ring(gfx->ren, rect, theme.sel_outline);
  }

  polish_home_button(gfx, theme, sdl_rect(geometry.home_primary[0]),
                     tr(ui->language, T_NEW_GAME), true, false,
                     home_focus == HOME_FOCUS_NEW, fonts.control);
  polish_home_button(gfx, theme, sdl_rect(geometry.home_primary[1]),
                     tr(ui->language, T_CONTINUE), false, !ui->has_session,
                     home_focus == HOME_FOCUS_CONTINUE, fonts.control);

  SDL_Rect daily = sdl_rect(geometry.home_primary[2]);
  bool daily_hover = point_in(daily, mx, my);
  bool daily_pressed = daily_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Rect daily_face = polish_button_face(
      gfx, theme, daily, daily_hover ? theme.hover : theme.btn, daily_pressed, false);
  SDL_Rect daily_main = {daily_face.x, daily_face.y + 2, daily_face.w,
                         daily_face.h * 3 / 5};
  SDL_Rect daily_sub = {daily_face.x, daily_face.y + daily_face.h * 3 / 5 - 2,
                        daily_face.w, daily_face.h * 2 / 5};
  (void)draw_fitted_text(gfx, tr(ui->language, T_DAILY), theme.btnfg, daily_main,
                         fonts.control, 10);
  char daily_detail[64];
  snprintf(daily_detail, sizeof daily_detail, "%s · %s",
           tr(ui->language, T_CLASSIC), tr(ui->language, T_MEDIUM));
  int sub_preferred = fonts.help > 12 ? fonts.help - 4 : 12;
  (void)draw_fitted_text(gfx, daily_detail, theme.dim, daily_sub, sub_preferred, 10);
  if (home_focus == HOME_FOCUS_DAILY)
    draw_focus_ring(gfx->ren, daily, theme.sel_outline);

  const TextKey secondary_keys[GEOMETRY_HOME_SECONDARY_COUNT] = {
      T_HELP, T_ABOUT, T_THEME, T_QUIT};
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i) {
    char label[64];
    const char *text = tr(ui->language, secondary_keys[i]);
    if (i == 2) {
      snprintf(label, sizeof label, "%s: %s", tr(ui->language, T_THEME),
               tr(ui->language, ui->dark_theme ? T_DARK : T_LIGHT));
      text = label;
    }
    polish_home_button(gfx, theme, sdl_rect(geometry.home_secondary[i]), text,
                       false, false, home_focus == HOME_FOCUS_HELP + i,
                       fonts.control);
  }
}

static void polish_render_play(Gfx *gfx, const Game *game, UI *ui) {
  if (!game || (ui->pause_reasons & PAUSE_MANUAL)) return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  Layout layout = compute_layout(gfx->width, gfx->height, ui->mode);
  SidebarRects controls;
  compute_sidebar_rects(&layout, &controls);
  GeometryFonts fonts = geometry_font_sizes(&layout.model, gfx->width, gfx->height);

  SDL_Rect title = sdl_rect(layout.model.play_title);
  draw_round_rect(gfx->ren, title, 8, theme.palette_bg);
  polish_draw_icon(gfx, title);

  if (ui->mode == MODE_CLASSIC && layout.model.hud_count > 3) {
    SDL_Rect hints = sdl_rect(layout.model.hud[3]);
    draw_round_rect(gfx->ren, hints, hints.h / 2, theme.palette_bg);
    char label[64];
    snprintf(label, sizeof label, "%s: %d", tr(ui->language, T_HINTS),
             game_hint_count(game));
    (void)draw_fitted_text(gfx, label, theme.dim, hints, fonts.hud, 10);
  }

  SDL_Color separator = theme.dim;
  separator.a = 42;
  if (controls.btn[0].y > title.y + title.h)
    draw_line(gfx->ren, layout.side.x + 12, controls.btn[0].y - 7,
              layout.side.x + layout.side.w - 13, controls.btn[0].y - 7,
              separator);
  if (controls.palette_label.y > controls.btn[8].y + controls.btn[8].h)
    draw_line(gfx->ren, layout.side.x + 12, controls.palette_label.y - 6,
              layout.side.x + layout.side.w - 13, controls.palette_label.y - 6,
              separator);

  const TextKey action_keys[GEOMETRY_ACTION_COUNT] = {
      T_RESTART, T_PAUSE, T_HINT, T_NOTES, T_VERIFY,
      T_SOUND,   T_HELP,  T_ABOUT, T_MENU};
  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  for (int i = 0; i < controls.count_btn; ++i) {
    SDL_Rect rect = controls.btn[i];
    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    bool active = (i == 3 && ui->notes_mode) || (i == 5 && audio_is_enabled());
    SDL_Color background =
        active ? theme.title : (hover ? theme.hover : theme.palette_bg);
    SDL_Color foreground = active ? theme.bg : theme.btnfg;
    SDL_Rect face =
        polish_button_face(gfx, theme, rect, background, pressed, false);
    (void)draw_fitted_text(gfx, tr(ui->language, action_keys[i]), foreground,
                           face, fonts.control, 10);
  }

  for (int number = 1; number <= 9; ++number) {
    SDL_Rect rect = controls.pal[number - 1];
    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    bool complete = palette_number_complete(game, ui->mode, number);
    bool selected = game->puzzle[IDX(ui->sel_r, ui->sel_c)] == number;
    SDL_Color background = complete
                               ? theme.btn
                               : (selected ? theme.title
                                           : (hover ? theme.hover : theme.palette_bg));
    SDL_Color foreground = complete ? theme.dim : theme.palette_fg;
    SDL_Rect face = polish_button_face(gfx, theme, rect, background,
                                       pressed && !complete, complete);
    char label[2] = {(char)('0' + number), 0};
    (void)draw_fitted_text(gfx, label, foreground, face, fonts.control + 4, 12);
    if (complete) {
      int x = face.x + face.w - 16;
      int y = face.y + face.h - 11;
      draw_line(gfx->ren, x, y, x + 4, y + 4, theme.dim);
      draw_line(gfx->ren, x + 4, y + 4, x + 10, y - 4, theme.dim);
    }
  }
}

static void polish_draw_smiley(Gfx *gfx, Theme theme, SDL_Rect box) {
  int size = box.w < box.h ? box.w : box.h;
  if (size > 48) size = 48;
  if (size < 24) return;
  SDL_Rect face = {box.x + (box.w - size) / 2, box.y + (box.h - size) / 2,
                   size, size};
  SDL_Color fill = theme.title;
  fill.a = 55;
  draw_round_rect(gfx->ren, face, size / 2, fill);
  int eye_y = face.y + size * 2 / 5;
  int left_x = face.x + size / 3;
  int right_x = face.x + size * 2 / 3;
  draw_rect(gfx->ren, left_x - 1, eye_y - 1, 3, 3, theme.title);
  draw_rect(gfx->ren, right_x - 1, eye_y - 1, 3, 3, theme.title);
  int mouth_y = face.y + size * 3 / 5;
  draw_line(gfx->ren, face.x + size / 3, mouth_y,
            face.x + size * 2 / 5, mouth_y + 3, theme.title);
  draw_line(gfx->ren, face.x + size * 2 / 5, mouth_y + 3,
            face.x + size * 3 / 5, mouth_y + 3, theme.title);
  draw_line(gfx->ren, face.x + size * 3 / 5, mouth_y + 3,
            face.x + size * 2 / 3, mouth_y, theme.title);
}

static void polish_render_about(Gfx *gfx, UI *ui) {
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(gfx->ren, theme.bg.r, theme.bg.g, theme.bg.b, 255);
  SDL_RenderClear(gfx->ren);

  AppGeometry geometry;
  if (!get_geometry(gfx, ui, &geometry)) return;
  GeometryFonts fonts = geometry_font_sizes(&geometry, gfx->width, gfx->height);
  SDL_Rect logo = sdl_rect(geometry.about_logo);
  SDL_Rect last = sdl_rect(geometry.about_links[GEOMETRY_ABOUT_LINK_COUNT - 1]);
  SDL_Rect card = {logo.x - 12, logo.y - 8, logo.w + 24,
                   last.y + last.h - logo.y + 16};
  SDL_Rect shadow = card;
  shadow.y += 5;
  draw_round_rect(gfx->ren, shadow, 14, theme.shadow);
  draw_round_rect(gfx->ren, card, 14, theme.palette_bg);
  draw_wordmark(gfx, logo);

  SDL_Rect intro = sdl_rect(geometry.about_body);
  int version_h = intro.h / 2;
  if (version_h < 20) version_h = intro.h;
  SDL_Rect version = {intro.x, intro.y, intro.w, version_h};
  SDL_Rect intro_copy = {intro.x + 8, intro.y + version_h, intro.w - 16,
                         intro.h - version_h};
  (void)draw_fitted_text(gfx, SUDOKURA_NAME_VERSION, theme.title, version,
                         fonts.body, 10);
  if (intro_copy.h > 4)
    (void)polish_draw_wrapped_fit(gfx, tr(ui->language, T_ABOUT_BODY),
                                  theme.btnfg, intro_copy, fonts.body, 10, true);

  SDL_Rect fact = sdl_rect(geometry.about_fact);
  SDL_Color fact_bg = theme.title;
  fact_bg.a = ui->dark_theme ? 38 : 26;
  draw_round_rect(gfx->ren, fact, 10, fact_bg);
  int icon_w = fact.h;
  if (icon_w > 62) icon_w = 62;
  SDL_Rect smiley = {fact.x + 6, fact.y + 4, icon_w - 8, fact.h - 8};
  polish_draw_smiley(gfx, theme, smiley);
  SDL_Rect fact_text = {fact.x + icon_w + 2, fact.y + 5,
                        fact.w - icon_w - 10, fact.h - 10};
  int fact_title_h = fact_text.h / 3;
  if (fact_title_h < 16) fact_title_h = 16;
  SDL_Rect fact_title = {fact_text.x, fact_text.y, fact_text.w, fact_title_h};
  SDL_Rect fact_copy = {fact_text.x, fact_text.y + fact_title_h, fact_text.w,
                        fact_text.h - fact_title_h};
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_FACT_TITLE), theme.title,
                         fact_title, fonts.help, 10);
  (void)polish_draw_wrapped_fit(gfx, tr(ui->language, T_ABOUT_FACT), theme.btnfg,
                                fact_copy, fonts.body, 10, true);

  SDL_Rect study = sdl_rect(geometry.about_study);
  draw_round_rect(gfx->ren, study, 10, theme.btn);
  SDL_Rect study_link = sdl_rect(geometry.about_study_link);
  int study_title_h = study.h / 4;
  if (study_title_h < 12) study_title_h = 12;
  SDL_Rect study_title = {study.x + 9, study.y + 2, study.w - 18,
                          study_title_h};
  SDL_Rect study_copy = {study.x + 9, study.y + 2 + study_title_h,
                         study.w - 18,
                         study_link.y - (study.y + 2 + study_title_h) - 2};
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_STUDY_TITLE), theme.title,
                         study_title, fonts.help, 10);
  if (study_copy.h > 3)
    (void)polish_draw_wrapped_fit(gfx, tr(ui->language, T_ABOUT_STUDY),
                                  theme.dim, study_copy, fonts.help, 10, false);

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool study_hover = point_in(study_link, mx, my);
  bool study_pressed = study_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Rect study_face = polish_button_face(
      gfx, theme, study_link, study_hover ? theme.hover : theme.palette_bg,
      study_pressed, false);
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_STUDY_LINK), theme.btnfg,
                         study_face, fonts.help, 10);

  SDL_Rect credits = sdl_rect(geometry.about_credits);
  int credit_h = credits.h / 2;
  SDL_Rect author = {credits.x, credits.y, credits.w, credit_h};
  SDL_Rect music = {credits.x, credits.y + credit_h, credits.w,
                    credits.h - credit_h};
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_SEEDS), theme.title,
                         author, fonts.help, 10);
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_STACK), theme.dim,
                         music, fonts.help, 10);

  const TextKey link_keys[GEOMETRY_ABOUT_LINK_COUNT] = {
      T_GITHUB, T_REPOSITORY, T_WEBSITE, T_SUPPORT};
  for (int i = 0; i < GEOMETRY_ABOUT_LINK_COUNT; ++i) {
    SDL_Rect rect = sdl_rect(geometry.about_links[i]);
    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    SDL_Rect face = polish_button_face(
        gfx, theme, rect, hover ? theme.hover : theme.btn, pressed, false);
    char label[96];
    snprintf(label, sizeof label, "%d · %s", i + 1,
             tr(ui->language, link_keys[i]));
    (void)draw_fitted_text(gfx, label, theme.btnfg, face, fonts.control, 10);
  }

  draw_back_button(gfx, ui, theme);
  draw_language_selector(gfx, ui);
}

static void app_render(Gfx *gfx, const Game *game, UI *ui) {
  app_render_base(gfx, game, ui);
  if (ui->screen == SCR_TITLE)
    polish_render_home(gfx, ui);
  else if (ui->screen == SCR_PLAY)
    polish_render_play(gfx, game, ui);
  else if (ui->screen == SCR_ABOUT)
    polish_render_about(gfx, ui);
}

static void start_new_session(Persistence *persistence, Game *game, UI *ui) {
  start_new_session_base(persistence, game, ui);
  audio_play_effect(AUDIO_EFFECT_START);
}

static bool start_daily_session(Persistence *persistence, Game *game, UI *ui,
                                int year, int month, int day) {
  bool started =
      start_daily_session_base(persistence, game, ui, year, month, day);
  if (started) audio_play_effect(AUDIO_EFFECT_START);
  return started;
}

static void go_home(Persistence *persistence, const Game *game, UI *ui) {
  bool leaving_play = ui && ui->screen == SCR_PLAY && ui->result == RES_NONE;
  if (leaving_play) audio_play_effect(AUDIO_EFFECT_LEAVE);
  go_home_base(persistence, game, ui);
}
