#ifndef SUDOKURA_GEOMETRY_H
#define SUDOKURA_GEOMETRY_H

#include <stdbool.h>

#define GEOMETRY_ACTION_COUNT 9
#define GEOMETRY_PALETTE_COUNT 9
#define GEOMETRY_HUD_COUNT 6
#define GEOMETRY_HOME_SEGMENT_COUNT 3
#define GEOMETRY_HOME_PRIMARY_COUNT 3
#define GEOMETRY_HOME_SECONDARY_COUNT 4
#define GEOMETRY_TITLE_BUTTON_COUNT 7
#define GEOMETRY_END_BUTTON_COUNT 2
#define GEOMETRY_PAUSE_BUTTON_COUNT 2
#define GEOMETRY_ABOUT_LINK_COUNT 4

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
  GeoRect play_language;

  GeoRect screen_language;
  GeoRect home_logo;
  GeoRect home_mode_label;
  GeoRect home_mode[GEOMETRY_HOME_SEGMENT_COUNT];
  GeoRect home_difficulty_label;
  GeoRect home_difficulty[GEOMETRY_HOME_SEGMENT_COUNT];
  GeoRect home_primary[GEOMETRY_HOME_PRIMARY_COUNT];
  GeoRect home_secondary[GEOMETRY_HOME_SECONDARY_COUNT];

  GeoRect info_heading;
  GeoRect info_body;
  GeoRect back_button;

  GeoRect about_logo;
  GeoRect about_body;
  GeoRect about_meta;
  GeoRect about_fact;
  GeoRect about_study;
  GeoRect about_study_link;
  GeoRect about_credits;
  GeoRect about_links[GEOMETRY_ABOUT_LINK_COUNT];

  GeoRect end_logo;
  GeoRect end_heading;
  GeoRect end_summary;
  GeoRect end_buttons[GEOMETRY_END_BUTTON_COUNT];

  GeoRect pause_logo;
  GeoRect pause_heading;
  GeoRect pause_buttons[GEOMETRY_PAUSE_BUTTON_COUNT];

  /* Compatibility aliases retained through the staged v1.2 upgrade. */
  GeoRect language;
  GeoRect title_heading;
  GeoRect title_buttons[GEOMETRY_TITLE_BUTTON_COUNT];
  GeoRect pause_button;
} AppGeometry;

typedef struct {
  int note, help, body, control, hud, cell, heading;
} GeometryFonts;

bool geometry_compute(int width, int height, GeometryMode mode, AppGeometry *out);
bool geometry_window_size_supported(int width, int height);
bool geometry_normalize_window_size(int requested_width, int requested_height,
                                    int *normalized_width, int *normalized_height);
bool geometry_contains(GeoRect rect, int x, int y);
bool geometry_rect_in_bounds(GeoRect rect, int width, int height);
bool geometry_play_valid(const AppGeometry *geometry, int width, int height);
GeometryFonts geometry_font_sizes(const AppGeometry *geometry, int width, int height);

#endif
