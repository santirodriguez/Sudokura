/* Adapt the existing screen painters to the shared palette without changing
   gameplay semantics. Included after the runtime types, before screen code. */
#include "sudokura_sdl/ui_design_draw.h"

static Theme ui_shared_theme(bool dark) {
  Theme t = dark ? theme_base_dark() : theme_base_light();
  UiDesignTheme u = ui_design_theme(dark);
  t.bg = ui_design_sdl_color(u.background_bottom);
  t.btn = ui_design_sdl_color(u.surface_top);
  t.btnfg = ui_design_sdl_color(u.text);
  t.palette_bg = ui_design_sdl_color(u.surface);
  t.palette_fg = ui_design_sdl_color(u.text);
  t.dim = ui_design_sdl_color(u.muted);
  t.title = ui_design_sdl_color(u.accent);
  t.sel_outline = ui_design_sdl_color(u.focus);
  t.shadow = ui_design_sdl_color(u.shadow);
  t.hover = ui_design_sdl_color(ui_design_mix(u.surface_top, u.accent, 28));
  /* Board ink, correctness, selection, peer and conflict colors stay owned
     by the existing gameplay theme. No hidden game state is inspected. */
  return t;
}

static Theme theme_dark(void) { return ui_shared_theme(true); }
static Theme theme_light(void) { return ui_shared_theme(false); }

static void draw_round_rect(SDL_Renderer *renderer, SDL_Rect rect,
                                  int radius, SDL_Color color) {
  if (!renderer || rect.w <= 0 || rect.h <= 0) return;
  UiDesignColor base = ui_design_color(color.r, color.g, color.b, color.a);
  SDL_FRect box = {(float)rect.x, (float)rect.y,
                   (float)rect.w, (float)rect.h};
  /* Rails, tiny indicators and translucent masks are not elevated cards. */
  if (radius < 6 || rect.h < 24 || rect.w < 24 || color.a < 255) {
    ui_design_round(renderer, box, (float)radius, base, base);
    return;
  }
  bool dark = (unsigned)color.r + color.g + color.b < 384u;
  UiDesignTheme theme = ui_design_theme(dark);
  UiDesignColor top = ui_design_mix(base, theme.text, dark ? 8u : 0u);
  UiDesignColor rim = ui_design_mix(base, theme.border, 100u);
  ui_design_panel(renderer, rect,
      radius >= 12 ? UI_DESIGN_CARD_RADIUS : UI_DESIGN_BUTTON_RADIUS,
      top, base, rim);
}

static void draw_focus_ring(SDL_Renderer *renderer, SDL_Rect rect,
                                  SDL_Color color) {
  if (!renderer || rect.w < 8 || rect.h < 8) return;
  enum { STEPS = 8, COUNT = 4 * (STEPS + 1) };
  SDL_FPoint points[COUNT];
  float x = (float)rect.x + 2.0f, y = (float)rect.y + 2.0f;
  float w = (float)rect.w - 4.0f, h = (float)rect.h - 4.0f;
  float radius = fminf(10.0f, fminf(w, h) * 0.5f);
  const float pi = 3.14159265358979323846f;
  for (int corner = 0; corner < 4; ++corner) {
    float cx = x + ((corner == 1 || corner == 2) ? w-radius : radius);
    float cy = y + (corner >= 2 ? h-radius : radius);
    for (int j = 0; j <= STEPS; ++j) {
      float a = pi + (float)corner*pi/2 + (float)j*pi/(2*STEPS);
      points[corner*(STEPS+1)+j] =
          (SDL_FPoint){cx + radius*cosf(a), cy + radius*sinf(a)};
    }
  }
  ui_design_stroke(renderer, points, COUNT, true, 1.6f,
      ui_design_color(color.r, color.g, color.b, color.a));
}
