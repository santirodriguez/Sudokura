#ifndef SUDOKURA_GEOMETRY_H
#define SUDOKURA_GEOMETRY_H

#include <stdbool.h>

#define GEOMETRY_ACTION_COUNT 9
#define GEOMETRY_PALETTE_COUNT 9
#define GEOMETRY_HUD_COUNT 6
#define GEOMETRY_HOME_SEGMENT_COUNT 3
#define GEOMETRY_HOME_PRIMARY_COUNT 3
#define GEOMETRY_HOME_SECONDARY_COUNT 5
#define GEOMETRY_TITLE_BUTTON_COUNT 8
#define GEOMETRY_END_BUTTON_COUNT 2
#define GEOMETRY_PAUSE_BUTTON_COUNT 2
#define GEOMETRY_ABOUT_LINK_COUNT 4
#define GEOMETRY_MORE_COUNT 6

typedef struct {
  int x, y, w, h;
} GeoRect;

typedef enum {
  GEOMETRY_MODE_CLASSIC = 0,
  GEOMETRY_MODE_STRIKES = 1,
  GEOMETRY_MODE_TIME = 2
} GeometryMode;

typedef enum {
  PLAY_ACTION_MENU = 0,
  PLAY_ACTION_PAUSE,
  PLAY_ACTION_RESTART,
  PLAY_ACTION_HINT,
  PLAY_ACTION_NOTES,
  PLAY_ACTION_VERIFY,
  PLAY_ACTION_AUDIO,
  PLAY_ACTION_HELP,
  PLAY_ACTION_ABOUT,
  PLAY_ACTION_COUNT
} PlayAction;

typedef enum {
  PLAY_CONTROL_MENU, PLAY_CONTROL_PAUSE, PLAY_CONTROL_SAVE,
  PLAY_CONTROL_MORE, PLAY_CONTROL_UNDO, PLAY_CONTROL_REDO,
  PLAY_CONTROL_CLEAR, PLAY_CONTROL_NOTES, PLAY_CONTROL_RESTART,
  PLAY_CONTROL_HINT, PLAY_CONTROL_VERIFY, PLAY_CONTROL_SETTINGS,
  PLAY_CONTROL_HELP, PLAY_CONTROL_ABOUT, PLAY_CONTROL_COUNT
} PlayControl;

typedef enum {
  MORE_RESTART, MORE_HINT, MORE_VERIFY, MORE_SETTINGS,
  MORE_HELP, MORE_ABOUT
} MoreControl;

typedef struct {
  GeoRect board;
  GeoRect sidebar;
  GeoRect play_title;
  GeoRect hud[GEOMETRY_HUD_COUNT];
  int hud_count;
  GeoRect actions[GEOMETRY_ACTION_COUNT];
  /* One semantic hit-box map for painting, focus, and pointer capture. */
  GeoRect play_controls[PLAY_CONTROL_COUNT];
  bool play_compact;
  bool mobile_layout;
  GeoRect more_panel;
  GeoRect more_items[GEOMETRY_MORE_COUNT];
  GeoRect palette_label;
  GeoRect palette[GEOMETRY_PALETTE_COUNT];
  GeoRect progress;
  GeoRect status;
  GeoRect play_language;

  GeoRect screen_language;
  GeoRect home_logo;
  GeoRect home_new_label;
  GeoRect home_continue_label;
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
  GeoRect button;
  GeoRect popup;
  GeoRect music;
  GeoRect fx;
} AudioControlGeometry;

typedef struct {
  GeoRect card, title, body, primary, verify, reveal, cancel;
} GeometryHintPanel;

typedef struct {
  GeoRect panel, title, body, buttons[3], scroll_up, scroll_down;
} GeometryModal;

bool geometry_modal(int width, int height, unsigned button_count,
                    GeometryModal *out);

typedef struct {
  GeoRect card, title, theme, language, sound;
  GeoRect music_label, music_minus, music_track, music_plus;
  GeoRect fx_label, fx_minus, fx_track, fx_plus;
  GeoRect reduced_motion, auto_notes, back;
} GeometrySettings;

bool geometry_mobile_settings(int width, int height, GeometrySettings *out);
/* Compose Home fields in-place, preserving gameplay and other screens. */
bool geometry_mobile_home(int width, int height, AppGeometry *out);

typedef struct {
  GeoRect heading, body, back, up, down, links[5];
} GeometryInfo;

bool geometry_mobile_info(int width, int height, bool about, GeometryInfo *out);

typedef struct {
  GeoRect title, subtitle, list, detail, primary, back, replay, remove;
  GeoRect page_previous, page_next, page_label;
  int row_height, visible;
  bool side_by_side;
} GeometryLibrary;

bool geometry_mobile_library(int width, int height, GeometryLibrary *out);

typedef struct {
  int note, help, body, control, hud, cell, heading;
} GeometryFonts;

typedef struct {
  int space_xs, space_sm, space_md, space_lg;
  int radius_control, radius_panel;
  int min_control_h;
  int grid_minor_px, grid_major_px;
} GeometryStyle;

bool geometry_compute(int width, int height, GeometryMode mode, AppGeometry *out);
/* Mobile-only composition; never relaxes desktop window-size validation. */
bool geometry_mobile_size_supported(int width_dp, int height_dp);
bool geometry_compute_mobile(int width_dp, int height_dp, GeometryMode mode,
                             AppGeometry *out);
/* Gameplay-only landscape composer. Secondary-screen fields remain zero;
   compose those screens before routing the complete Android shell here. */
bool geometry_mobile_landscape_play(int width_dp, int height_dp,
                                    GeometryMode mode, AppGeometry *out);
bool geometry_window_size_supported(int width, int height);
bool geometry_normalize_window_size(int requested_width, int requested_height,
                                    int *normalized_width, int *normalized_height);
bool geometry_contains(GeoRect rect, int x, int y);
bool geometry_rect_in_bounds(GeoRect rect, int width, int height);
bool geometry_play_valid(const AppGeometry *geometry, int width, int height);
bool geometry_hint_panel(const AppGeometry *geometry, int width, int height,
                         GeometryHintPanel *out);
/* Full artwork band and separate status headline, shared with render tests. */
GeoRect geometry_play_wordmark_rect(const AppGeometry *geometry, int width);
GeoRect geometry_play_heading_rect(const AppGeometry *geometry, int width);
GeometryFonts geometry_font_sizes(const AppGeometry *geometry, int width, int height);
GeometryStyle geometry_style(const AppGeometry *geometry, int width, int height);
AudioControlGeometry geometry_audio_control(int width, int height,
                                                  const AppGeometry *app,
                                                  bool play_surface);

#endif
