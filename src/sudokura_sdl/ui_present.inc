/* Consolidated Phase 5 presentation surface.
   Home/About/play presentation and final controls/settings overlays share one
   explicit render route; the former polish/rc2 layers are no longer composed. */
/* Presentation helpers used by the single explicit SDL render pipeline. */
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
      return true;
    }
  }

  /* The supported geometry matrix must fit above. If a platform font is
     unexpectedly wider, keep copy visible in the validated rectangle rather
     than silently dropping it. */
  TTF_Font *fallback = cached_font(gfx, minimum);
  int tw = 0, th = 0;
  SDL_Texture *texture =
      render_text_wrapped(gfx, fallback, text, color, rect.w - 8, &tw, &th);
  if (!texture) return false;
  int visible_w = tw < rect.w - 4 ? tw : rect.w - 4;
  int visible_h = th < rect.h - 2 ? th : rect.h - 2;
  if (visible_w <= 0 || visible_h <= 0) return false;
  SDL_Rect source = gfx_physical_rect(gfx,0,0,visible_w,visible_h);
  SDL_Rect destination = {
      rect.x + 4,
      centered ? rect.y + (rect.h - visible_h) / 2 : rect.y + 1,
      visible_w, visible_h};
  SDL_RenderCopy(gfx->ren, texture, &source, &destination);
  return true;
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

static void polish_home_context_button(
    Gfx *gfx, Theme theme, SDL_Rect rect, const char *label,
    const char *detail, bool primary, bool disabled, bool focused,
    int preferred) {
  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool hover = !disabled && point_in(rect, mx, my);
  bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Color background =
      disabled ? theme.palette_bg
               : (primary ? theme.title : (hover ? theme.hover : theme.btn));
  SDL_Rect face =
      polish_button_face(gfx, theme, rect, background, pressed, disabled);
  SDL_Color main_color =
      disabled ? theme.dim : (primary ? theme.bg : theme.btnfg);
  SDL_Rect main = {face.x + 2, face.y + 1, face.w - 4,
                   face.h * 3 / 5};
  SDL_Rect sub = {face.x + 2, face.y + face.h * 3 / 5 - 2,
                  face.w - 4, face.h * 2 / 5 + 1};
  (void)draw_fitted_text(gfx, label, main_color, main, preferred, 9);
  if (detail && detail[0]) {
    int sub_preferred = preferred > 13 ? preferred - 4 : 10;
    (void)draw_fitted_text(gfx, detail, theme.dim, sub, sub_preferred, 9);
  }
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

  bool normal_resumable = ui->normal_resume.can_continue;
  polish_home_button(gfx, theme, sdl_rect(geometry.home_primary[0]),
                     tr(ui->language, T_NEW_GAME), !normal_resumable, false,
                     home_focus == HOME_FOCUS_NEW, fonts.control);

  char normal_detail[128] = "";
  if (ui->normal_resume.can_continue) {
    unsigned long long total_s =
        (unsigned long long)(ui->normal_resume.elapsed_ms / 1000u);
    snprintf(normal_detail, sizeof normal_detail, "%s · %s · %02llu:%02llu",
             tr(ui->language, mode_key(ui->normal_resume.mode)),
             difficulty_name(ui->language, ui->normal_resume.difficulty),
             total_s / 60u, total_s % 60u);
  }
  SDL_Rect continue_rect = sdl_rect(geometry.home_primary[1]);
  polish_home_context_button(
      gfx, theme, continue_rect,
      tr(ui->language, T_CONTINUE_NORMAL), normal_detail,
      normal_resumable, !normal_resumable,
      home_focus == HOME_FOCUS_CONTINUE, fonts.control);

  const char *daily_label = tr(ui->language, T_DAILY);
  char daily_detail[128] = "";
  if (ui->daily_resume.can_continue) {
    daily_label = tr(ui->language, T_CONTINUE_DAILY);
    unsigned long long total_s =
        (unsigned long long)(ui->daily_resume.elapsed_ms / 1000u);
    snprintf(daily_detail, sizeof daily_detail,
             "%04d-%02d-%02d · %s · %02llu:%02llu",
             ui->daily_resume.year, ui->daily_resume.month,
             ui->daily_resume.day,
             difficulty_name(ui->language, ui->daily_resume.difficulty),
             total_s / 60u, total_s % 60u);
  } else if (ui->daily_resume.completed && ui->daily_resume.today) {
    daily_label = tr(ui->language, T_DAILY_COMPLETE);
    unsigned long long total_s =
        (unsigned long long)(ui->daily_resume.elapsed_ms / 1000u);
    snprintf(daily_detail, sizeof daily_detail,
             "%04d-%02d-%02d · %02llu:%02llu",
             ui->daily_resume.year, ui->daily_resume.month,
             ui->daily_resume.day, total_s / 60u, total_s % 60u);
  } else {
    snprintf(daily_detail, sizeof daily_detail, "%s · %s",
             tr(ui->language, T_CLASSIC), tr(ui->language, T_MEDIUM));
  }
  polish_home_context_button(
      gfx, theme, sdl_rect(geometry.home_primary[2]), daily_label, daily_detail,
      false, false, home_focus == HOME_FOCUS_DAILY, fonts.control);

  const TextKey secondary_keys[GEOMETRY_HOME_SECONDARY_COUNT] = {
      T_HELP, T_ABOUT, T_SETTINGS, T_QUIT};
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i) {
    const char *text = tr(ui->language, secondary_keys[i]);
    polish_home_button(gfx, theme, sdl_rect(geometry.home_secondary[i]), text,
                       false, false, home_focus == HOME_FOCUS_HELP + i,
                       fonts.control);
  }
}

static void polish_render_play(Gfx *gfx, const Game *game, UI *ui) {
  if (!game || app_pause_hides_play(ui)) return;
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
    char label[96];
    if (ui->assisted)
      snprintf(label, sizeof label, "%s: %d · %s", tr(ui->language, T_HINTS),
               game_hint_count(game), tr(ui->language, T_ASSISTED));
    else
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
  if (controls.palette_label.y > controls.btn[PLAY_ACTION_HELP].y + controls.btn[PLAY_ACTION_HELP].h)
    draw_line(gfx->ren, layout.side.x + 12, controls.palette_label.y - 6,
              layout.side.x + layout.side.w - 13, controls.palette_label.y - 6,
              separator);

  (void)draw_fitted_text(gfx, tr(ui->language, T_PALETTE), theme.dim,
                         controls.palette_label, fonts.help, 10);

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
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
  if(info_keyboard_focus==INFO_FOCUS_STUDY)
    draw_focus_ring(gfx->ren,study_link,theme.sel_outline);

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
    if(info_keyboard_focus==INFO_FOCUS_LINK_0+i)
      draw_focus_ring(gfx->ren,rect,theme.sel_outline);
  }

  draw_back_button(gfx, ui, theme, info_keyboard_focus==INFO_FOCUS_BACK);
}

static GameGenerationResult start_new_session(
    Persistence *persistence, Game *game, UI *ui,
    const GameGenerationControl *control) {
  GameGenerationResult result =
      start_new_session_base(persistence, game, ui, control);
  if (result == GAME_GENERATION_OK) audio_play_effect(AUDIO_EFFECT_START);
  return result;
}

static GameGenerationResult start_daily_session(
    Persistence *persistence, Game *game, UI *ui, int year, int month, int day,
    const GameGenerationControl *control) {
  GameGenerationResult result =
      start_daily_session_base(persistence, game, ui, year, month, day, control);
  if (result == GAME_GENERATION_OK) audio_play_effect(AUDIO_EFFECT_START);
  return result;
}

static void go_home_polish(Persistence *persistence, const Game *game,
                           UI *ui) {
  bool leaving_play = ui && ui->screen == SCR_PLAY && ui->result == RES_NONE;
  if (leaving_play) audio_play_effect(AUDIO_EFFECT_LEAVE);
  go_home_base(persistence, game, ui);
}

/* RC2 interaction/presentation corrections that stay outside Sudoku rules. */
typedef struct {
  SDL_Rect card;
  SDL_Rect title;
  SDL_Rect theme;
  SDL_Rect language;
  SDL_Rect sound;
  SDL_Rect music_label;
  SDL_Rect music_minus;
  SDL_Rect music_track;
  SDL_Rect music_plus;
  SDL_Rect fx_label;
  SDL_Rect fx_minus;
  SDL_Rect fx_track;
  SDL_Rect fx_plus;
  SDL_Rect reduced_motion;
  SDL_Rect back;
} SettingsPanelLayout;

static Persistence *audio_persistence = NULL;
static Gfx *audio_gfx = NULL;
static UI *audio_ui = NULL;
static bool settings_open = false;
typedef enum {
  SETTINGS_FOCUS_THEME = 0,
  SETTINGS_FOCUS_LANGUAGE,
  SETTINGS_FOCUS_SOUND,
  SETTINGS_FOCUS_MUSIC,
  SETTINGS_FOCUS_FX,
  SETTINGS_FOCUS_REDUCED_MOTION,
  SETTINGS_FOCUS_BACK,
  SETTINGS_FOCUS_COUNT
} SettingsKeyboardFocus;
static SettingsKeyboardFocus settings_keyboard_focus = SETTINGS_FOCUS_THEME;
typedef enum{SETTINGS_DRAG_NONE=0,SETTINGS_DRAG_MUSIC,SETTINGS_DRAG_FX}SettingsSliderDrag;
static SettingsSliderDrag settings_slider_drag=SETTINGS_DRAG_NONE;
static bool ui_home_cue_requested = false;
static bool audio_control_popup_open = false;
static bool audio_control_pressed = false;
static uint64_t audio_control_pressed_at_ms = 0;

typedef struct {
  SDL_Rect banner;
  SDL_Rect message;
  SDL_Rect retry;
} StorageBannerLayout;

static StorageBannerLayout storage_banner_layout(const Gfx *gfx,
                                                 const UI *ui) {
  StorageBannerLayout layout;
  memset(&layout, 0, sizeof(layout));
  if (!gfx) return layout;

  AppGeometry geometry;
  bool play_surface = ui && ui->screen == SCR_PLAY;
  if (play_surface && get_geometry(gfx, ui, &geometry)) {
    layout.banner = sdl_rect(geometry.status);
    int inset = layout.banner.h <= 28 ? 2 : 4;
    int retry_w = layout.banner.w < 300 ? 88 : 118;
    layout.retry = (SDL_Rect){
        layout.banner.x + layout.banner.w - retry_w - inset,
        layout.banner.y + inset, retry_w, layout.banner.h - inset * 2};
    layout.message = (SDL_Rect){
        layout.banner.x + inset + 4, layout.banner.y + inset,
        layout.retry.x - layout.banner.x - inset * 2 - 4,
        layout.banner.h - inset * 2};
    return layout;
  }

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

static bool storage_retry_hit(const Gfx *gfx, const UI *ui, int x, int y) {
  if (!audio_persistence || !persistence_has_save_error(audio_persistence))
    return false;
  StorageBannerLayout layout = storage_banner_layout(gfx, ui);
  return point_in(layout.retry, x, y);
}

static int ui_clamp_percent(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

static void ui_audio_set_music_level(int value) {
  int level = ui_clamp_percent(value);
  audio_set_music_volume(level);
  if (audio_persistence) {
    audio_persistence->preferences.music_volume = (uint8_t)level;
    if (audio_ui)
      (void)save_preferences(audio_persistence, audio_ui);
    else
      persistence_stage_preferences(audio_persistence);
  }
}

static void ui_audio_set_fx_level(int value) {
  int level = ui_clamp_percent(value);
  audio_set_fx_volume(level);
  if (audio_persistence) {
    audio_persistence->preferences.fx_volume = (uint8_t)level;
    if (audio_ui)
      (void)save_preferences(audio_persistence, audio_ui);
    else
      persistence_stage_preferences(audio_persistence);
  }
}

static void ui_settings_cycle_language(int delta) {
  if (!audio_ui) return;
  int language = ((int)audio_ui->language + delta) % LANG_COUNT;
  if (language < 0) language += LANG_COUNT;
  audio_ui->language = (Language)language;
  if (audio_persistence) (void)save_preferences(audio_persistence, audio_ui);
}

static void ui_settings_toggle_theme(void) {
  if (!audio_ui) return;
  audio_ui->dark_theme = !audio_ui->dark_theme;
  if (audio_persistence) (void)save_preferences(audio_persistence, audio_ui);
}

static void ui_settings_toggle_reduced_motion(void) {
  if (!audio_ui) return;
  audio_ui->reduced_motion = !audio_ui->reduced_motion;
  if (audio_ui->reduced_motion) victory_animation_reset();
  if (audio_persistence) (void)save_preferences(audio_persistence, audio_ui);
}

static void ui_settings_open(Persistence *persistence, const Game *game, UI *ui) {
  if (!ui || settings_open ||
      (ui->screen != SCR_PLAY && ui->screen != SCR_TITLE))
    return;
  bool from_play = ui->screen == SCR_PLAY;
  settings_keyboard_focus = SETTINGS_FOCUS_THEME;
  settings_slider_drag = SETTINGS_DRAG_NONE;
  if (from_play) {
    clock_pause(ui, PAUSE_MODAL);
    if (persistence && game)
      (void)checkpoint_current_session(persistence, game, ui);
  }
  if (!app_open_aux(ui, APP_SCREEN_SETTINGS)) {
    if (from_play) clock_resume(ui, PAUSE_MODAL);
    return;
  }
  settings_open = true;
}

static void ui_settings_close(Persistence *persistence, const Game *game,
                              UI *ui) {
  if (!ui || !settings_open) return;
  bool returns_to_play = ui->prev_screen == SCR_PLAY;
  settings_open = false;
  settings_slider_drag = SETTINGS_DRAG_NONE;
  (void)app_return_aux(ui);
  if (returns_to_play) {
    clock_resume(ui, PAUSE_MODAL);
    if (persistence && game)
      (void)checkpoint_current_session(persistence, game, ui);
  }
}

static SDL_Rect ui_settings_focus_rect(SettingsPanelLayout layout,
                                       SettingsKeyboardFocus focus) {
  switch (focus) {
    case SETTINGS_FOCUS_THEME:
      return layout.theme;
    case SETTINGS_FOCUS_LANGUAGE:
      return layout.language;
    case SETTINGS_FOCUS_SOUND:
      return layout.sound;
    case SETTINGS_FOCUS_MUSIC:
      return (SDL_Rect){
          layout.music_minus.x, layout.music_minus.y,
          layout.music_plus.x + layout.music_plus.w - layout.music_minus.x,
          layout.music_minus.h};
    case SETTINGS_FOCUS_FX:
      return (SDL_Rect){
          layout.fx_minus.x, layout.fx_minus.y,
          layout.fx_plus.x + layout.fx_plus.w - layout.fx_minus.x,
          layout.fx_minus.h};
    case SETTINGS_FOCUS_REDUCED_MOTION:
      return layout.reduced_motion;
    case SETTINGS_FOCUS_BACK:
      return layout.back;
    case SETTINGS_FOCUS_COUNT:
    default:
      return (SDL_Rect){0, 0, 0, 0};
  }
}

static SettingsPanelLayout ui_settings_panel_layout(const Gfx *gfx) {
  SettingsPanelLayout layout;
  memset(&layout, 0, sizeof(layout));
  int margin = gfx->width < 640 ? 12 : 24;
  int width = gfx->width - margin * 2;
  if (width > 540) width = 540;
  int height = gfx->height - margin * 2;
  if (height > 520) height = 520;
  if (height < 420) height = gfx->height - margin * 2;
  layout.card = (SDL_Rect){(gfx->width - width) / 2,
                           (gfx->height - height) / 2, width, height};

  int x = layout.card.x + 18;
  int w = layout.card.w - 36;
  int y = layout.card.y + 12;
  int row_h = layout.card.h < 470 ? 30 : 34;
  int gap = layout.card.h < 470 ? 4 : 6;
  int label_h = layout.card.h < 470 ? 16 : 18;
  int slider_h = layout.card.h < 470 ? 32 : 34;
  int button_w = layout.card.w < 400 ? 42 : 44;
  int control_gap = 7;

  layout.title = (SDL_Rect){x, y, w, row_h + 4};
  y += layout.title.h + gap;
  layout.theme = (SDL_Rect){x, y, w, row_h};
  y += row_h + gap;
  layout.language = (SDL_Rect){x, y, w, row_h};
  y += row_h + gap;
  layout.sound = (SDL_Rect){x, y, w, row_h};
  y += row_h + gap;

  layout.music_label = (SDL_Rect){x, y, w, label_h};
  y += label_h + 2;
  layout.music_minus = (SDL_Rect){x, y, button_w, slider_h};
  layout.music_plus = (SDL_Rect){x + w - button_w, y, button_w, slider_h};
  layout.music_track =
      (SDL_Rect){x + button_w + control_gap, y,
                 w - button_w * 2 - control_gap * 2, slider_h};
  y += slider_h + gap;

  layout.fx_label = (SDL_Rect){x, y, w, label_h};
  y += label_h + 2;
  layout.fx_minus = (SDL_Rect){x, y, button_w, slider_h};
  layout.fx_plus = (SDL_Rect){x + w - button_w, y, button_w, slider_h};
  layout.fx_track =
      (SDL_Rect){x + button_w + control_gap, y,
                 w - button_w * 2 - control_gap * 2, slider_h};
  y += slider_h + gap;

  layout.reduced_motion = (SDL_Rect){x, y, w, row_h};
  layout.back = (SDL_Rect){x, layout.card.y + layout.card.h - row_h - 12,
                           w, row_h};
  return layout;
}

static void rc2_draw_slider(Gfx *gfx, Theme theme, Language language,
                            SDL_Rect label_rect, SDL_Rect minus, SDL_Rect track,
                            SDL_Rect plus, TextKey label_key, int value,
                            bool available, int preferred) {
  char label[96];
  if (available)
    snprintf(label, sizeof label, "%s  %d%%", tr(language, label_key), value);
  else
    snprintf(label, sizeof label, "%s  %d%% · %s", tr(language, label_key),
             value, tr(language, T_UNAVAILABLE));
  (void)draw_fitted_text(gfx, label, theme.btnfg, label_rect, preferred, 9);

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

static void rc2_settings_button(Gfx *gfx, Theme theme, SDL_Rect rect,
                                const char *label, bool active,
                                int preferred) {
  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool hover = point_in(rect, mx, my);
  bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Color bg = active ? theme.title : (hover ? theme.hover : theme.btn);
  SDL_Color fg = active ? theme.bg : theme.btnfg;
  SDL_Rect face = polish_button_face(gfx, theme, rect, bg, pressed, false);
  (void)draw_fitted_text(gfx, label, fg, face, preferred, 9);
}

static void rc2_render_settings_panel(Gfx *gfx, UI *ui) {
  if (!settings_open || !gfx || !ui) return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  SDL_Color veil = theme.bg;
  veil.a = 224;
  draw_rect(gfx->ren, 0, 0, gfx->width, gfx->height, veil);

  SettingsPanelLayout layout = ui_settings_panel_layout(gfx);
  SDL_Rect shadow = layout.card;
  shadow.y += 5;
  draw_round_rect(gfx->ren, shadow, 14, theme.shadow);
  draw_round_rect(gfx->ren, layout.card, 14, theme.palette_bg);

  AppGeometry geometry;
  int preferred = 18;
  if (get_geometry(gfx, ui, &geometry))
    preferred = geometry_font_sizes(&geometry, gfx->width, gfx->height).control;

  (void)draw_fitted_text(gfx, tr(ui->language, T_SETTINGS), theme.title,
                         layout.title, preferred + 7, 13);

  char label[128];
  snprintf(label, sizeof label, "%s: %s", tr(ui->language, T_THEME),
           tr(ui->language, ui->dark_theme ? T_DARK : T_LIGHT));
  rc2_settings_button(gfx, theme, layout.theme, label, false, preferred);

  snprintf(label, sizeof label, "%s: %s", tr(ui->language, T_LANGUAGE),
           language_name(ui->language));
  rc2_settings_button(gfx, theme, layout.language, label, false, preferred);

  bool enabled = audio_is_enabled();
  bool available = audio_is_available();
  if (enabled && !available)
    snprintf(label, sizeof label, "%s: %s · %s", tr(ui->language, T_SOUND),
             tr(ui->language, T_ON), tr(ui->language, T_UNAVAILABLE));
  else
    snprintf(label, sizeof label, "%s: %s", tr(ui->language, T_SOUND),
             tr(ui->language, enabled ? T_ON : T_OFF));
  rc2_settings_button(gfx, theme, layout.sound, label,
                      enabled && available, preferred);

  rc2_draw_slider(gfx, theme, ui->language, layout.music_label,
                  layout.music_minus, layout.music_track, layout.music_plus,
                  T_MUSIC, audio_music_volume(), audio_music_available(),
                  preferred);
  rc2_draw_slider(gfx, theme, ui->language, layout.fx_label, layout.fx_minus,
                  layout.fx_track, layout.fx_plus, T_FX, audio_fx_volume(),
                  audio_fx_available(), preferred);

  snprintf(label, sizeof label, "%s: %s", tr(ui->language, T_REDUCED_MOTION),
           tr(ui->language, ui->reduced_motion ? T_ON : T_OFF));
  rc2_settings_button(gfx, theme, layout.reduced_motion, label,
                      ui->reduced_motion, preferred);

  rc2_settings_button(gfx, theme, layout.back, tr(ui->language, T_BACK),
                      false, preferred);

  SDL_Rect focus_rect =
      ui_settings_focus_rect(layout, settings_keyboard_focus);
  if (focus_rect.w > 0 && focus_rect.h > 0)
    draw_focus_ring(gfx->ren, focus_rect, theme.sel_outline);
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

typedef enum{HINT_FOCUS_PRIMARY=0,HINT_FOCUS_REVEAL,HINT_FOCUS_CANCEL,HINT_FOCUS_COUNT}HintKeyboardFocus;
static HintKeyboardFocus hint_keyboard_focus=HINT_FOCUS_PRIMARY;
static bool hint_focus_enabled(const UI*ui,HintKeyboardFocus focus){
  if(!ui||!ui->hint_preview_active)return false;
  if(focus==HINT_FOCUS_REVEAL)return ui->hint_preview.status!=HUMAN_HINT_INVALID;
  return focus>=HINT_FOCUS_PRIMARY&&focus<HINT_FOCUS_COUNT;
}
static void hint_focus_reset(const UI*ui){
  hint_keyboard_focus=HINT_FOCUS_PRIMARY;
  if(!hint_focus_enabled(ui,hint_keyboard_focus))hint_keyboard_focus=HINT_FOCUS_CANCEL;
}
static void hint_move_focus(const UI*ui,int direction){
  int next=hint_keyboard_focus;
  for(int i=0;i<HINT_FOCUS_COUNT;i++){
    next=input_focus_step(next,HINT_FOCUS_COUNT,direction);
    if(hint_focus_enabled(ui,(HintKeyboardFocus)next)){hint_keyboard_focus=(HintKeyboardFocus)next;return;}
  }
}
static SDL_Rect hint_focus_rect(const Gfx*gfx,const UI*ui,HintKeyboardFocus focus){
  HintPanelLayout panel=hint_panel_layout(gfx,ui);
  if(focus==HINT_FOCUS_PRIMARY)return ui&&ui->hint_preview.status==HUMAN_HINT_LOGICAL?panel.primary:panel.verify;
  if(focus==HINT_FOCUS_REVEAL)return panel.reveal;
  return panel.cancel;
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
  if(hint_focus_enabled(ui,hint_keyboard_focus))
    draw_focus_ring(gfx->ren,hint_focus_rect(gfx,ui,hint_keyboard_focus),theme.sel_outline);
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

typedef enum{
  PLAY_FOCUS_MENU=0,PLAY_FOCUS_PAUSE,PLAY_FOCUS_UNDO,PLAY_FOCUS_REDO,
  PLAY_FOCUS_RESTART,PLAY_FOCUS_CLEAR,PLAY_FOCUS_HINT,PLAY_FOCUS_NOTES,
  PLAY_FOCUS_VERIFY,PLAY_FOCUS_SETTINGS,PLAY_FOCUS_HELP,PLAY_FOCUS_STRICT,
  PLAY_FOCUS_SAVE_RETRY,PLAY_FOCUS_NUMBER_1,PLAY_FOCUS_NUMBER_2,PLAY_FOCUS_NUMBER_3,
  PLAY_FOCUS_NUMBER_4,PLAY_FOCUS_NUMBER_5,PLAY_FOCUS_NUMBER_6,
  PLAY_FOCUS_NUMBER_7,PLAY_FOCUS_NUMBER_8,PLAY_FOCUS_NUMBER_9,
  PLAY_FOCUS_COUNT
}PlayKeyboardFocus;
static int play_keyboard_focus=-1;
static bool play_focus_enabled(const Game*game,const UI*ui,int focus){
  if(!game||!ui||focus<0||focus>=PLAY_FOCUS_COUNT)return false;
  if(focus==PLAY_FOCUS_UNDO)return ui->undo_count>0;
  if(focus==PLAY_FOCUS_REDO)return ui->redo_count>0;
  if(focus==PLAY_FOCUS_CLEAR)return clear_selected_available(game,ui);
  if(focus==PLAY_FOCUS_SAVE_RETRY)
    return audio_persistence&&persistence_has_save_error(audio_persistence);
  if(focus>=PLAY_FOCUS_NUMBER_1){
    int number=focus-PLAY_FOCUS_NUMBER_1+1;
    return !palette_number_complete(game,ui->mode,number);
  }
  return true;
}
static void play_move_focus(const Game*game,const UI*ui,int direction){
  int next=play_keyboard_focus;
  for(int i=0;i<PLAY_FOCUS_COUNT;i++){
    next=input_focus_step(next,PLAY_FOCUS_COUNT,direction);
    if(play_focus_enabled(game,ui,next)){play_keyboard_focus=next;return;}
  }
  play_keyboard_focus=-1;
}
static SDL_Rect play_focus_rect(const Gfx*gfx,const UI*ui,int focus){
  if(!gfx||!ui||focus<0||focus>=PLAY_FOCUS_COUNT)return(SDL_Rect){0,0,0,0};
  Layout layout=compute_layout(gfx->width,gfx->height,ui->mode);SidebarRects controls;compute_sidebar_rects(&layout,&controls);SDL_Rect left,right;
  if(focus==PLAY_FOCUS_MENU||focus==PLAY_FOCUS_PAUSE){menu_pause_rects(controls.btn[PLAY_ACTION_MENU],&left,&right);return focus==PLAY_FOCUS_MENU?left:right;}
  if(focus==PLAY_FOCUS_UNDO||focus==PLAY_FOCUS_REDO){undo_redo_rects(controls.btn[PLAY_ACTION_PAUSE],&left,&right);return focus==PLAY_FOCUS_UNDO?left:right;}
  if(focus==PLAY_FOCUS_RESTART||focus==PLAY_FOCUS_CLEAR){restart_clear_rects(controls.btn[PLAY_ACTION_RESTART],&left,&right);return focus==PLAY_FOCUS_RESTART?left:right;}
  if(focus==PLAY_FOCUS_HINT)return controls.btn[PLAY_ACTION_HINT];
  if(focus==PLAY_FOCUS_NOTES)return controls.btn[PLAY_ACTION_NOTES];
  if(focus==PLAY_FOCUS_VERIFY)return controls.btn[PLAY_ACTION_VERIFY];
  if(focus==PLAY_FOCUS_SETTINGS){
    SDL_Rect settings=controls.btn[PLAY_ACTION_AUDIO];
    AudioControlGeometry audio=
        geometry_audio_control(gfx->width,gfx->height,&layout.model,true);
    settings.w=audio.button.x-4-settings.x;
    return settings;
  }
  if(focus==PLAY_FOCUS_HELP)return controls.btn[PLAY_ACTION_HELP];
  if(focus==PLAY_FOCUS_STRICT&&layout.model.hud_count>4)return sdl_rect(layout.model.hud[4]);
  if(focus==PLAY_FOCUS_SAVE_RETRY)return storage_banner_layout(gfx,ui).retry;
  if(focus>=PLAY_FOCUS_NUMBER_1)return controls.pal[focus-PLAY_FOCUS_NUMBER_1];
  return(SDL_Rect){0,0,0,0};
}

static void rc2_render_play_controls(Gfx *gfx, const Game *game, UI *ui) {
  bool play_surface = ui && ui->screen == SCR_PLAY;
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
      [PLAY_ACTION_AUDIO] = T_SETTINGS,
      [PLAY_ACTION_HELP] = T_HELP,
      [PLAY_ACTION_ABOUT] = T_ABOUT,
  };

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  for (int action = 0; action < PLAY_ACTION_COUNT; ++action) {
    if (action == PLAY_ACTION_ABOUT) continue;
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

    if (action == PLAY_ACTION_AUDIO) {
      AudioControlGeometry audio =
          geometry_audio_control(gfx->width, gfx->height, &layout.model, true);
      rect.w = audio.button.x - 4 - rect.x;
    }
    bool hover = point_in(rect, mx, my);
    bool pressed = hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
    bool active = action == PLAY_ACTION_NOTES && ui->notes_mode;
    SDL_Color background =
        active ? theme.title : (hover ? theme.hover : theme.palette_bg);
    SDL_Color foreground = active ? theme.bg : theme.btnfg;
    SDL_Rect face =
        polish_button_face(gfx, theme, rect, background, pressed, false);
    (void)draw_fitted_text(gfx, tr(ui->language, keys[action]), foreground,
                           face, fonts.control, 10);
  }

  if(play_focus_enabled(game,ui,play_keyboard_focus)&&
     play_keyboard_focus!=PLAY_FOCUS_SAVE_RETRY){
    SDL_Rect focus_rect=play_focus_rect(gfx,ui,play_keyboard_focus);
    if(focus_rect.w>0&&focus_rect.h>0)draw_focus_ring(gfx->ren,focus_rect,theme.sel_outline);
  }

  SDL_Color separator = theme.dim;
  separator.a = 42;
  if (controls.palette_label.y > controls.btn[PLAY_ACTION_HELP].y)
    draw_line(gfx->ren, layout.side.x + 12, controls.palette_label.y - 4,
              layout.side.x + layout.side.w - 13, controls.palette_label.y - 4,
              separator);
}

static void rc2_render_storage_banner(Gfx *gfx, UI *ui) {
  if (!gfx || !ui || !audio_persistence ||
      !persistence_has_save_error(audio_persistence))
    return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  StorageBannerLayout layout = storage_banner_layout(gfx, ui);
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
  if(play_keyboard_focus==PLAY_FOCUS_SAVE_RETRY)
    draw_focus_ring(gfx->ren,layout.retry,theme.sel_outline);
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


static AudioControlGeometry ui_audio_control_geometry(const Gfx *gfx,
                                                      const UI *ui) {
  AppGeometry app;
  memset(&app, 0, sizeof(app));
  bool has_geometry = gfx && ui && get_geometry(gfx, ui, &app);
  bool play_surface = has_geometry && ui->screen == SCR_PLAY;
  return geometry_audio_control(gfx ? gfx->width : 0, gfx ? gfx->height : 0,
                                has_geometry ? &app : NULL, play_surface);
}

static void rc2_draw_speaker_icon(Gfx *gfx, Theme theme, SDL_Rect rect,
                                  bool muted) {
  if (!gfx || rect.w <= 0 || rect.h <= 0) return;
  int pad = rect.w < rect.h ? rect.w / 5 : rect.h / 5;
  if (pad < 5) pad = 5;
  int cx = rect.x + rect.w / 2;
  int cy = rect.y + rect.h / 2;
  SDL_Color color = muted ? theme.dim : theme.btnfg;
  SDL_Rect box = {rect.x + pad, cy - rect.h / 10,
                  rect.w / 5, rect.h / 5};
  draw_rect(gfx->ren, box.x, box.y, box.w, box.h, color);
  draw_line(gfx->ren, box.x + box.w, box.y,
            cx, cy - rect.h / 4, color);
  draw_line(gfx->ren, cx, cy - rect.h / 4,
            cx, cy + rect.h / 4, color);
  draw_line(gfx->ren, cx, cy + rect.h / 4,
            box.x + box.w, box.y + box.h, color);
  if (!muted) {
    draw_line(gfx->ren, cx + 4, cy - 7, cx + 8, cy - 3, color);
    draw_line(gfx->ren, cx + 8, cy - 3, cx + 8, cy + 3, color);
    draw_line(gfx->ren, cx + 8, cy + 3, cx + 4, cy + 7, color);
  } else {
    draw_line(gfx->ren, rect.x + pad, rect.y + pad,
              rect.x + rect.w - pad, rect.y + rect.h - pad, theme.conflict);
  }
}

static void rc2_render_audio_control(Gfx *gfx, UI *ui) {
  if (!gfx || !ui) return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  AudioControlGeometry geo = ui_audio_control_geometry(gfx, ui);
  SDL_Rect button = sdl_rect(geo.button);
  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool hover = point_in(button, mx, my);
  SDL_Color bg = hover ? theme.hover : theme.btn;
  SDL_Rect face = polish_button_face(gfx, theme, button, bg, false, false);
  rc2_draw_speaker_icon(gfx, theme, face, !audio_is_enabled());

  if (!audio_control_popup_open) return;

  SDL_Rect popup = sdl_rect(geo.popup);
  SDL_Rect shadow = popup;
  shadow.y += 4;
  draw_round_rect(gfx->ren, shadow, 10, theme.shadow);
  draw_round_rect(gfx->ren, popup, 10, theme.palette_bg);

  SDL_Rect music = sdl_rect(geo.music);
  SDL_Rect fx = sdl_rect(geo.fx);
  rc2_settings_button(gfx, theme, music, tr(ui->language, T_MUSIC),
                      !audio_music_muted(), 15);
  rc2_settings_button(gfx, theme, fx, tr(ui->language, T_FX),
                      !audio_fx_muted(), 15);
  if (audio_music_muted())
    draw_line(gfx->ren, music.x + 8, music.y + 7,
              music.x + music.w - 8, music.y + music.h - 7, theme.conflict);
  if (audio_fx_muted())
    draw_line(gfx->ren, fx.x + 8, fx.y + 7,
              fx.x + fx.w - 8, fx.y + fx.h - 7, theme.conflict);
}

static void app_render_ui(Gfx *gfx, const Game *game, UI *ui) {
  text_cache_sync(gfx,ui);
  app_render_base(gfx, game, ui);
  if (ui->screen == SCR_TITLE)
    polish_render_home(gfx, ui);
  else if (ui->screen == SCR_PLAY)
    polish_render_play(gfx, game, ui);
  else if (ui->screen == SCR_ABOUT)
    polish_render_about(gfx, ui);
  rc2_render_play_controls(gfx, game, ui);
  rc2_render_hint_panel(gfx, ui);
  rc2_render_settings_panel(gfx, ui);
  rc2_render_storage_banner(gfx, ui);
  rc2_render_generation_overlay(gfx, ui);
  rc2_render_audio_control(gfx, ui);
}

static void go_home(Persistence *persistence, const Game *game, UI *ui) {
  bool entering_home = ui && ui->screen != SCR_TITLE;
  settings_open = false;
  go_home_polish(persistence, game, ui);
  if (persistence && ui) {
    refresh_resume_info(persistence, ui);
    home_focus = ui->normal_resume.can_continue
                     ? HOME_FOCUS_CONTINUE
                     : HOME_FOCUS_NEW;
  }
  if (entering_home) ui_home_cue_requested = true;
}
