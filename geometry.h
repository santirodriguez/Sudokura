#ifndef SUDOKURA_GEOMETRY_H
#define SUDOKURA_GEOMETRY_H

#include <stdbool.h>

#define GEOMETRY_ACTION_COUNT 9
#define GEOMETRY_PALETTE_COUNT 9
#define GEOMETRY_HUD_COUNT 4
#define GEOMETRY_TITLE_BUTTON_COUNT 5
#define GEOMETRY_END_BUTTON_COUNT 2

typedef struct {
  int x, y, w, h;
} GeoRect;

typedef enum {
  GEOMETRY_MODE_CLASSIC = 0,
  GEOMETRY_MODE_STRIKES = 1,
  GEOMETRY_MODE_TIME = 2
} GeometryMode;

typedef struct {
  GeoRect board;
  GeoRect sidebar;
  GeoRect play_title;
  GeoRect hud[GEOMETRY_HUD_COUNT];
  int hud_count;
  GeoRect actions[GEOMETRY_ACTION_COUNT];
  GeoRect palette_label;
  GeoRect palette[GEOMETRY_PALETTE_COUNT];
  GeoRect progress;
  GeoRect language;

  GeoRect title_heading;
  GeoRect title_buttons[GEOMETRY_TITLE_BUTTON_COUNT];
  GeoRect info_heading;
  GeoRect info_body;
  GeoRect back_button;
  GeoRect end_heading;
  GeoRect end_summary;
  GeoRect end_buttons[GEOMETRY_END_BUTTON_COUNT];
} AppGeometry;

bool geometry_compute(int width, int height, GeometryMode mode, AppGeometry *out);
bool geometry_contains(GeoRect rect, int x, int y);
bool geometry_rect_in_bounds(GeoRect rect, int width, int height);
bool geometry_play_valid(const AppGeometry *geometry, int width, int height);

#endif
