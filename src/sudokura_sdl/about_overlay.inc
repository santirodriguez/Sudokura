/* Desktop About cards redraw their copy with measured wrapping so the minimum
   640x480 layout keeps full context instead of falling back to cryptic text. */
static bool about_draw_single_line_left(Gfx *gfx, const char *text,
                                        SDL_Color color, SDL_Rect rect,
                                        int preferred, int minimum) {
  if (!gfx || !text || !text[0] || rect.w <= 2 || rect.h <= 2) return false;
  for (int i = FONT_CACHE_COUNT - 1; i >= 0; --i) {
    int size = font_cache_sizes[i];
    if (size > preferred || size < minimum) continue;
    int width = 0, height = 0;
    if (TTF_SizeUTF8(gfx->fonts[i], text, &width, &height) != 0 ||
        width > rect.w || height > rect.h)
      continue;
    SDL_Texture *texture =
        render_text(gfx, gfx->fonts[i], text, color, &width, &height);
    if (!texture) continue;
    SDL_Rect destination = {rect.x, rect.y + (rect.h - height) / 2, width, height};
    SDL_RenderCopy(gfx->ren, texture, NULL, &destination);
    SDL_DestroyTexture(texture);
    return true;
  }
  return false;
}

static void about_redraw_desktop_cards(Gfx *gfx, UI *ui) {
  if (!gfx || !ui || ui->screen != SCR_ABOUT || gfx->width < 640) return;

  AppGeometry geometry;
  if (!get_geometry(gfx, ui, &geometry)) return;
  GeometryFonts fonts = geometry_font_sizes(&geometry, gfx->width, gfx->height);
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();

  SDL_Rect fact = sdl_rect(geometry.about_fact);
  SDL_Color fact_bg = theme.title;
  fact_bg.a = ui->dark_theme ? 38 : 26;
  draw_round_rect(gfx->ren, fact, 10, theme.palette_bg);
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
  SDL_Rect fact_copy = {fact_text.x, fact_text.y + fact_title_h,
                        fact_text.w, fact_text.h - fact_title_h};
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_FACT_TITLE), theme.title,
                         fact_title, fonts.help, 10);
  (void)polish_draw_wrapped_fit(gfx, tr(ui->language, T_ABOUT_FACT),
                                theme.btnfg, fact_copy, fonts.body, 10, true);

  SDL_Rect study = sdl_rect(geometry.about_study);
  SDL_Rect study_link = sdl_rect(geometry.about_study_link);
  draw_round_rect(gfx->ren, study, 10, theme.btn);

  int study_title_h = study.h / 4;
  if (study_title_h < 12) study_title_h = 12;
  SDL_Rect study_title = {study.x + 9, study.y + 2, study.w - 18,
                          study_title_h};
  SDL_Rect study_copy = {study.x + 9, study.y + 2 + study_title_h,
                         study.w - 18,
                         study_link.y - (study.y + 2 + study_title_h) - 2};
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_STUDY_TITLE), theme.title,
                         study_title, fonts.help, 10);
  if (study_copy.h > 3) {
    bool rendered = polish_draw_wrapped_fit(
        gfx, tr(ui->language, T_ABOUT_STUDY), theme.dim, study_copy, fonts.help,
        10, false);
    if (!rendered)
      (void)about_draw_single_line_left(
          gfx, tr(ui->language, T_ABOUT_STUDY), theme.dim, study_copy,
          fonts.help, 10);
  }

  int mx = 0, my = 0;
  Uint32 mouse = SDL_GetMouseState(&mx, &my);
  bool study_hover = point_in(study_link, mx, my);
  bool study_pressed = study_hover && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT));
  SDL_Rect study_face = polish_button_face(
      gfx, theme, study_link, study_hover ? theme.hover : theme.palette_bg,
      study_pressed, false);
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_STUDY_LINK), theme.btnfg,
                         study_face, fonts.help, 10);
}

static void app_render(Gfx *gfx, const Game *game, UI *ui) {
  app_render_polish_base(gfx, game, ui);
  about_redraw_desktop_cards(gfx, ui);
}
