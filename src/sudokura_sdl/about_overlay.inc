/* Desktop About cards use single-line fitted copy so the minimum 640x480
   layout never drops body text when SDL_ttf wrapped metrics are tight. */
static void about_redraw_desktop_cards(Gfx *gfx, UI *ui) {
  if (!gfx || !ui || ui->screen != SCR_ABOUT || gfx->width < 640) return;

  AppGeometry geometry;
  if (!get_geometry(gfx, ui, &geometry)) return;
  GeometryFonts fonts = geometry_font_sizes(&geometry, gfx->width, gfx->height);
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();

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
  SDL_Rect fact_copy = {fact_text.x, fact_text.y + fact_title_h,
                        fact_text.w, fact_text.h - fact_title_h};
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_FACT_TITLE), theme.title,
                         fact_title, fonts.help, 10);
  (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_FACT), theme.btnfg,
                         fact_copy, fonts.body, 10);

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
  if (study_copy.h > 3)
    (void)draw_fitted_text(gfx, tr(ui->language, T_ABOUT_STUDY), theme.dim,
                           study_copy, fonts.help, 10);

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
