#ifndef SUDOKURA_UI_DESIGN_H
#define SUDOKURA_UI_DESIGN_H

#include <stdbool.h>
#include <stdint.h>

/* Presentation-only tokens. Gameplay colors and persistent state are separate. */
typedef struct { uint8_t r, g, b, a; } UiDesignColor;
typedef struct {
  UiDesignColor background_top, background_bottom, surface, surface_top;
  UiDesignColor border, text, muted, accent, primary, primary_top, on_primary;
  UiDesignColor focus, danger, celebration, disabled, shadow;
} UiDesignTheme;
typedef enum {
  UI_DESIGN_SECONDARY, UI_DESIGN_PRIMARY, UI_DESIGN_QUIET,
  UI_DESIGN_DESTRUCTIVE, UI_DESIGN_CELEBRATION
} UiDesignRole;
typedef struct { bool hovered, pressed, focused, disabled; } UiDesignState;
typedef struct {
  UiDesignColor top, bottom, border, text;
  bool focus;
  int displacement;
} UiDesignButton;

typedef struct { int x, y, w, h; } UiDesignRect;
typedef struct { UiDesignRect icon, text; } UiDesignLabel;

static inline UiDesignColor ui_design_color(int r, int g, int b, int a) {
  return (UiDesignColor){(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
}
static inline UiDesignColor ui_design_mix(UiDesignColor a, UiDesignColor b,
                                         unsigned amount) {
  if (amount > 255u) amount = 255u;
  unsigned inverse = 255u - amount;
  return (UiDesignColor){
    (uint8_t)((a.r * inverse + b.r * amount + 127u) / 255u),
    (uint8_t)((a.g * inverse + b.g * amount + 127u) / 255u),
    (uint8_t)((a.b * inverse + b.b * amount + 127u) / 255u),
    (uint8_t)((a.a * inverse + b.a * amount + 127u) / 255u)};
}
static inline UiDesignTheme ui_design_theme(bool dark) {
  if (dark) return (UiDesignTheme){
    {15,21,43,255}, {9,12,24,255}, {23,29,49,255}, {33,42,69,255},
    {73,86,126,255}, {242,245,255,255}, {180,190,212,255},
    {171,184,255,255}, {69,84,169,255}, {87,101,193,255}, {255,255,255,255},
    {190,203,255,255}, {255,153,166,255}, {244,201,124,255},
    {135,148,177,255}, {0,0,0,42}};
  return (UiDesignTheme){
    {239,243,255,255}, {249,250,255,255}, {251,252,255,255}, {238,242,255,255},
    {169,181,212,255}, {29,37,62,255}, {80,94,122,255},
    {57,74,161,255}, {66,82,165,255}, {81,96,181,255}, {255,255,255,255},
    {52,70,157,255}, {168,37,62,255}, {126,80,17,255},
    {104,115,139,255}, {39,54,100,22}};
}
static inline UiDesignButton ui_design_button_style(UiDesignTheme t,
    UiDesignRole role, UiDesignState state) {
  UiDesignButton b = {t.surface_top, t.surface, t.border, t.text, false, 0};
  if (role == UI_DESIGN_PRIMARY) {
    b.top = t.primary_top; b.bottom = t.primary;
    b.border = t.accent; b.text = t.on_primary;
  } else if (role == UI_DESIGN_QUIET) {
    b.top = ui_design_mix(t.surface, t.background_top, 80);
    b.bottom = ui_design_mix(t.surface, t.background_bottom, 80);
  } else if (role == UI_DESIGN_DESTRUCTIVE) {
    b.text = t.danger;
  } else if (role == UI_DESIGN_CELEBRATION) {
    b.text = t.celebration;
    b.border = ui_design_mix(t.border, t.celebration, 70);
  }
  if (state.disabled) {
    b.top = b.bottom = t.surface;
    b.border = ui_design_mix(t.surface, t.border, 70);
    b.text = t.disabled;
    return b;
  }
  if (state.hovered) {
    b.top = role == UI_DESIGN_PRIMARY ? ui_design_mix(b.top, b.bottom, 48)
                                       : ui_design_mix(b.top, t.accent, 20);
    b.border = ui_design_mix(b.border, t.accent, 110);
  }
  if (state.pressed) {
    UiDesignColor shade = role == UI_DESIGN_PRIMARY
                            ? ui_design_color(0,0,0,255) : t.background_bottom;
    b.top = ui_design_mix(b.top, shade, 65);
    b.bottom = ui_design_mix(b.bottom, shade, 45);
    b.displacement = 1;
  }
  b.focus = state.focused;
  return b;
}

/* Icons never steal the translated label's minimum useful width. */
static inline UiDesignLabel ui_design_label_layout(int width, int height,
                                                   bool with_icon) {
  if (width <= 16 || height <= 0) return (UiDesignLabel){{0,0,0,0},{0,0,0,0}};
  UiDesignLabel out = {{0,0,0,0}, {8,0,width > 16 ? width-16 : 0,height}};
  if (!with_icon || width < 116 || height < 32) return out;
  int side = height >= 44 ? 24 : 20;
  if (width < 152) side = 20;
  out.icon = (UiDesignRect){10,(height-side)/2,side,side};
  out.text = (UiDesignRect){side+18,0,width-side-26,height};
  return out;
}

/* Shared theme/layout constants are logical units, never device pixels. */
enum {
  UI_DESIGN_CARD_RADIUS = 16,
  UI_DESIGN_BUTTON_RADIUS = 12,
  UI_DESIGN_CARD_PADDING = 14,
  UI_DESIGN_BODY_FONT = 18,
  UI_DESIGN_CONTROL_FONT = 16,
  UI_DESIGN_ICON_SIZE = 24
};
#endif
