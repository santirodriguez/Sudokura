#include "geometry.h"

#include <string.h>

enum { MIN_WIDTH = 640, MIN_HEIGHT = 480, MARGIN = 12, GAP = 6 };

static bool overlaps(GeoRect a, GeoRect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

bool geometry_contains(GeoRect r, int x, int y) {
  return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y &&
         x < r.x + r.w && y < r.y + r.h;
}

bool geometry_rect_in_bounds(GeoRect r, int width, int height) {
  return r.w > 0 && r.h > 0 && r.x >= 0 && r.y >= 0 &&
         r.x <= width - r.w && r.y <= height - r.h;
}

static void grid(GeoRect area, int columns, int rows, GeoRect *items, int count) {
  int cell_w = (area.w - (columns - 1) * GAP) / columns;
  int cell_h = (area.h - (rows - 1) * GAP) / rows;
  for (int i = 0; i < count; ++i) {
    items[i] = (GeoRect){area.x + (i % columns) * (cell_w + GAP),
                         area.y + (i / columns) * (cell_h + GAP),
                         cell_w, cell_h};
  }
}

bool geometry_compute(int width, int height, GeometryMode mode, AppGeometry *g) {
  if (g == NULL || width < MIN_WIDTH || height < MIN_HEIGHT ||
      mode < GEOMETRY_MODE_CLASSIC || mode > GEOMETRY_MODE_TIME) {
    return false;
  }
  memset(g, 0, sizeof(*g));

  int sidebar_w = width >= 1024 ? 270 : 250;
  int board_side = height - 2 * MARGIN;
  int board_limit = width - sidebar_w - 3 * MARGIN;
  if (board_side > board_limit) board_side = board_limit;
  board_side -= board_side % 9;
  if (board_side < 9 * 32) return false;

  g->board = (GeoRect){MARGIN, (height - board_side) / 2, board_side, board_side};
  g->sidebar = (GeoRect){g->board.x + board_side + MARGIN, MARGIN,
                         width - (g->board.x + board_side + 2 * MARGIN),
                         height - 2 * MARGIN};
  g->language = (GeoRect){width - 190, 8, 182, 28};

  int x = g->sidebar.x;
  int y = g->language.y + g->language.h + 6;
  int w = g->sidebar.w;
  g->play_title = (GeoRect){x, y, w, 28};
  y += 32;
  g->hud_count = mode == GEOMETRY_MODE_CLASSIC ? 2 : 3;
  for (int i = 0; i < g->hud_count; ++i) {
    g->hud[i] = (GeoRect){x, y, w, 18};
    y += 20;
  }
  y += 4;
  GeoRect action_area = {x, y, w, 154};
  grid(action_area, 2, 5, g->actions, GEOMETRY_ACTION_COUNT);
  y += action_area.h + 6;
  g->palette_label = (GeoRect){x, y, w, 18};
  y += 22;
  int progress_h = 18;
  int palette_h = g->sidebar.y + g->sidebar.h - y - progress_h - 6;
  if (palette_h > 96) palette_h = 96;
  if (palette_h < 84) return false;
  GeoRect palette_area = {x, y, w, palette_h};
  grid(palette_area, 3, 3, g->palette, GEOMETRY_PALETTE_COUNT);
  g->progress = (GeoRect){x, y + palette_h + 6, w, progress_h};

  int menu_w = width < 800 ? 360 : 420;
  if (menu_w > width - 2 * MARGIN) menu_w = width - 2 * MARGIN;
  g->title_heading = (GeoRect){(width - menu_w) / 2, 42, menu_w, 54};
  GeoRect menu_area = {(width - menu_w) / 2, 125, menu_w, 5 * 42 + 4 * 10};
  grid(menu_area, 1, 5, g->title_buttons, GEOMETRY_TITLE_BUTTON_COUNT);

  g->info_heading = (GeoRect){MARGIN * 2, 42, width - 4 * MARGIN, 54};
  g->back_button = (GeoRect){MARGIN * 2, height - 52, 160, 36};
  g->info_body = (GeoRect){MARGIN * 2, 106, width - 4 * MARGIN,
                           g->back_button.y - 118};

  g->end_heading = (GeoRect){MARGIN, 80, width - 2 * MARGIN, 54};
  g->end_summary = (GeoRect){MARGIN, 150, width - 2 * MARGIN, 28};
  GeoRect end_area = {(width - 320) / 2, 220, 320, 92};
  grid(end_area, 1, 2, g->end_buttons, GEOMETRY_END_BUTTON_COUNT);
  return geometry_play_valid(g, width, height);
}

bool geometry_play_valid(const AppGeometry *g, int width, int height) {
  if (g == NULL || !geometry_rect_in_bounds(g->board, width, height) ||
      !geometry_rect_in_bounds(g->sidebar, width, height) ||
      !geometry_rect_in_bounds(g->play_title, width, height) ||
      !geometry_rect_in_bounds(g->palette_label, width, height) ||
      !geometry_rect_in_bounds(g->progress, width, height) ||
      !geometry_rect_in_bounds(g->language, width, height)) return false;
  if (overlaps(g->board, g->sidebar)) return false;
  for (int i = 0; i < g->hud_count; ++i)
    if (!geometry_rect_in_bounds(g->hud[i], width, height)) return false;
  for (int i = 0; i < GEOMETRY_ACTION_COUNT; ++i) {
    if (!geometry_rect_in_bounds(g->actions[i], width, height) ||
        g->actions[i].w < 110 || g->actions[i].h < 26) return false;
    for (int j = i + 1; j < GEOMETRY_ACTION_COUNT; ++j)
      if (overlaps(g->actions[i], g->actions[j])) return false;
  }
  for (int i = 0; i < GEOMETRY_PALETTE_COUNT; ++i) {
    if (!geometry_rect_in_bounds(g->palette[i], width, height) ||
        g->palette[i].w < 70 || g->palette[i].h < 24) return false;
    for (int j = i + 1; j < GEOMETRY_PALETTE_COUNT; ++j)
      if (overlaps(g->palette[i], g->palette[j])) return false;
  }
  if (!geometry_rect_in_bounds(g->title_heading,width,height) ||
      !geometry_rect_in_bounds(g->info_heading,width,height) ||
      !geometry_rect_in_bounds(g->info_body,width,height) ||
      !geometry_rect_in_bounds(g->back_button,width,height) ||
      !geometry_rect_in_bounds(g->end_heading,width,height) ||
      !geometry_rect_in_bounds(g->end_summary,width,height)) return false;
  for (int i=0;i<GEOMETRY_TITLE_BUTTON_COUNT;++i)
    if (!geometry_rect_in_bounds(g->title_buttons[i],width,height)) return false;
  for (int i=0;i<GEOMETRY_END_BUTTON_COUNT;++i)
    if (!geometry_rect_in_bounds(g->end_buttons[i],width,height)) return false;
  return true;
}
