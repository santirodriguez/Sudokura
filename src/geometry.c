#include "geometry.h"

#include <string.h>

enum { GAP = 6, DESKTOP_MIN_W = 640, DESKTOP_MIN_H = 480 };

bool geometry_window_size_supported(int width, int height) {
  return (width >= DESKTOP_MIN_W && height >= DESKTOP_MIN_H) ||
         (width >= 360 && height >= 640);
}

bool geometry_normalize_window_size(int requested_width, int requested_height,
                                    int *normalized_width,
                                    int *normalized_height) {
  if (!normalized_width || !normalized_height) return false;
  int width = requested_width < 360 ? 360 : requested_width;
  int height = requested_height < DESKTOP_MIN_H ? DESKTOP_MIN_H : requested_height;
  if (!geometry_window_size_supported(width, height)) width = DESKTOP_MIN_W;
  *normalized_width = width;
  *normalized_height = height;
  return width != requested_width || height != requested_height;
}

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

static void grid_gap(GeoRect area, int columns, int gap, GeoRect *items,
                     int count) {
  int rows = (count + columns - 1) / columns;
  int cell_w = (area.w - (columns - 1) * gap) / columns;
  int cell_h = (area.h - (rows - 1) * gap) / rows;
  for (int i = 0; i < count; ++i) {
    items[i] = (GeoRect){area.x + i % columns * (cell_w + gap),
                         area.y + i / columns * (cell_h + gap), cell_w,
                         cell_h};
  }
}

static int min_i(int a, int b) { return a < b ? a : b; }
static int max_i(int a, int b) { return a > b ? a : b; }

static void common_screens(int width, int height, bool portrait, AppGeometry *g) {
  bool xl = !portrait && width >= 1600 && height >= 900;
  bool short_desktop = !portrait && height < 600;
  int margin = portrait ? 8 : (xl ? 18 : 12);
  int language_w = portrait ? width - margin * 2
                            : min_i(width - margin * 2, xl ? 680 : 540);
  int language_h = portrait ? 40 : (xl ? 44 : 38);
  g->screen_language =
      (GeoRect){(width - language_w) / 2, margin, language_w, language_h};

  /* Home is composed from semantic groups, not a five-button footer.
     All pointer and focus rectangles are shared with the renderer. */
  int home_w = portrait ? width - margin * 2
                        : min_i(width - margin * 2 - 36, xl ? 700 : 640);
  int home_x = (width - home_w) / 2;
  int logo_h = portrait ? 62 : (short_desktop ? 46 : (xl ? 108 : 88));
  int y = g->screen_language.y + g->screen_language.h +
          (short_desktop ? 5 : 8);
  g->home_logo = (GeoRect){home_x, y, home_w, logo_h};
  y += logo_h + (short_desktop ? 4 : 8);

  int group_label_h = 20;
  int label_h = 20;
  int segment_h = portrait ? 34 : (short_desktop ? 30 : (xl ? 42 : 36));
  g->home_new_label = (GeoRect){home_x, y, home_w, group_label_h};
  y += group_label_h + (short_desktop ? 1 : 3);
  g->home_mode_label = (GeoRect){home_x, y, home_w, label_h};
  y += label_h + 2;
  grid_gap((GeoRect){home_x, y, home_w, segment_h}, 3, 6, g->home_mode,
           GEOMETRY_HOME_SEGMENT_COUNT);
  y += segment_h + (short_desktop ? 4 : 7);
  g->home_difficulty_label = (GeoRect){home_x, y, home_w, label_h};
  y += label_h + 2;
  grid_gap((GeoRect){home_x, y, home_w, segment_h}, 3, 6,
           g->home_difficulty, GEOMETRY_HOME_SEGMENT_COUNT);
  y += segment_h + (short_desktop ? 6 : 9);

  int start_h = portrait ? 43 : (short_desktop ? 38 : (xl ? 54 : 47));
  g->home_primary[0] = (GeoRect){home_x, y, home_w, start_h};
  y += start_h + (short_desktop ? 5 : 9);

  g->home_continue_label = (GeoRect){home_x, y, home_w, group_label_h};
  y += group_label_h + (short_desktop ? 2 : 4);
  int continue_h = portrait ? 46 : (short_desktop ? 44 : (xl ? 61 : 55));
  int gap = portrait ? 5 : 8;
  if (portrait) {
    g->home_primary[1] = (GeoRect){home_x, y, home_w, continue_h};
    g->home_primary[2] = (GeoRect){home_x, y + continue_h + gap,
                                   home_w, continue_h};
    y += continue_h * 2 + gap;
  } else {
    int left_w = (home_w - gap) / 2;
    g->home_primary[1] = (GeoRect){home_x, y, left_w, continue_h};
    g->home_primary[2] = (GeoRect){home_x + left_w + gap, y,
                                   home_w - left_w - gap, continue_h};
    y += continue_h;
  }

  y += short_desktop ? 7 : 10;
  int library_h = portrait ? 41 : (short_desktop ? 37 : (xl ? 49 : 44));
  g->home_secondary[0] = (GeoRect){home_x, y, home_w, library_h};
  y += library_h + (short_desktop ? 7 : 11);

  int footer_h = portrait ? 33 : (short_desktop ? 31 : (xl ? 43 : 37));
  int footer_gap = portrait ? 6 : 8;
  int columns = portrait ? 2 : 4;
  GeoRect footer[GEOMETRY_HOME_SECONDARY_COUNT - 1];
  grid_gap((GeoRect){home_x,y,home_w,footer_h * (portrait ? 2 : 1) +
                         footer_gap * (portrait ? 1 : 0)},
           columns,footer_gap,footer,GEOMETRY_HOME_SECONDARY_COUNT - 1);
  for (int i = 1; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    g->home_secondary[i] = footer[i - 1];

  int info_margin = portrait ? 16 : (short_desktop ? 20 : 24);
  int info_top = g->screen_language.y + g->screen_language.h + 16;
  int info_w = portrait ? width - info_margin * 2
                        : min_i(width - info_margin * 2, xl ? 820 : 760);
  int info_x = (width - info_w) / 2;
  int heading_h = short_desktop ? 42 : (xl ? 60 : 48);
  int back_h = short_desktop ? 40 : (xl ? 48 : 44);
  g->info_heading = (GeoRect){info_x, info_top, info_w, heading_h};
  g->back_button = (GeoRect){info_x, height - info_margin - back_h,
                             portrait ? info_w : (xl ? 200 : 180), back_h};
  g->info_body = (GeoRect){info_x, g->info_heading.y + heading_h + 10, info_w,
                           g->back_button.y - g->info_heading.y - heading_h - 20};

  int about_w = portrait ? info_w : min_i(info_w, xl ? 760 : 680);
  int about_x = (width - about_w) / 2;
  int about_available_total = g->back_button.y - info_top - 10;
  int about_stack_h = about_available_total;
  if (!portrait) {
    int cap = xl ? 700 : 560;
    if (about_stack_h > cap) about_stack_h = cap;
  }
  int about_top = info_top;
  if (about_available_total > about_stack_h)
    about_top += (about_available_total - about_stack_h) / 3;

  int about_logo_h = portrait ? 64 : (short_desktop ? 52 : (xl ? 96 : 80));
  g->about_logo = (GeoRect){about_x, about_top, about_w, about_logo_h};

  int link_h = portrait ? 36 : (short_desktop ? 34 : (xl ? 46 : 42));
  int link_gap = portrait ? 6 : 8;
  int link_columns = portrait ? 1 : 2;
  int link_rows = (GEOMETRY_ABOUT_LINK_COUNT + link_columns - 1) / link_columns;
  int links_h = link_h * link_rows + link_gap * (link_rows - 1);
  int links_y = about_top + about_stack_h - links_h;
  grid_gap((GeoRect){about_x, links_y, about_w, links_h}, link_columns, link_gap,
           g->about_links, GEOMETRY_ABOUT_LINK_COUNT);

  int content_top = g->about_logo.y + g->about_logo.h + 8;
  int content_bottom = links_y - 8;
  int content_h = content_bottom - content_top;
  int content_gap = short_desktop ? 3 : 6;
  int intro_h = portrait ? 56 : (short_desktop ? 40 : (xl ? 76 : 64));
  int credits_h = portrait ? 40 : (short_desktop ? 30 : (xl ? 48 : 40));
  int cards_h = content_h - intro_h - credits_h - content_gap * 3;
  if (cards_h < 96) {
    int deficit = 96 - cards_h;
    int intro_reduction = min_i(deficit, max_i(0, intro_h - 36));
    intro_h -= intro_reduction;
    deficit -= intro_reduction;
    int credits_reduction = min_i(deficit, max_i(0, credits_h - 28));
    credits_h -= credits_reduction;
    cards_h = content_h - intro_h - credits_h - content_gap * 3;
  }
  int fact_h = cards_h * 44 / 100;
  int study_h = cards_h - fact_h;

  g->about_body = (GeoRect){about_x, content_top, about_w, intro_h};
  int content_y = content_top + intro_h + content_gap;
  g->about_fact = (GeoRect){about_x, content_y, about_w, fact_h};
  content_y += fact_h + content_gap;
  g->about_study = (GeoRect){about_x, content_y, about_w, study_h};
  int study_link_h = min_i(28, max_i(22, study_h / 4));
  int study_link_w = min_i(about_w / 2, portrait ? 190 : 220);
  g->about_study_link =
      (GeoRect){about_x + about_w - study_link_w - 10,
                content_y + study_h - study_link_h - 4,
                study_link_w, study_link_h};
  content_y += study_h + content_gap;
  g->about_credits = (GeoRect){about_x, content_y, about_w, credits_h};
  g->about_meta = (GeoRect){about_x, g->about_fact.y, about_w,
                            g->about_credits.y + g->about_credits.h -
                                g->about_fact.y};

  int panel_w = portrait ? width - margin * 4
                         : min_i(width - margin * 4, xl ? 560 : 460);
  int panel_x = (width - panel_w) / 2;
  int end_y = g->screen_language.y + g->screen_language.h + 10;
  g->end_logo = (GeoRect){panel_x, end_y, panel_w, portrait ? 70 : (xl ? 90 : 72)};
  end_y += g->end_logo.h + 8;
  g->end_heading = (GeoRect){panel_x, end_y, panel_w, xl ? 60 : 46};
  end_y += g->end_heading.h + 8;
  g->end_summary = (GeoRect){panel_x, end_y, panel_w,
                             portrait ? 128 : (xl ? 140 : 118)};
  end_y += g->end_summary.h + 14;
  grid_gap((GeoRect){panel_x, end_y, panel_w, xl ? 110 : 94}, 1, 6,
           g->end_buttons, GEOMETRY_END_BUTTON_COUNT);

  int pause_y = g->screen_language.y + g->screen_language.h + 18;
  g->pause_logo = (GeoRect){panel_x, pause_y, panel_w,
                            portrait ? 72 : (xl ? 96 : 76)};
  pause_y += g->pause_logo.h + 12;
  g->pause_heading = (GeoRect){panel_x, pause_y, panel_w, xl ? 60 : 44};
  pause_y += g->pause_heading.h + 14;
  grid_gap((GeoRect){panel_x, pause_y, panel_w, xl ? 116 : 102}, 1, 6,
           g->pause_buttons, GEOMETRY_PAUSE_BUTTON_COUNT);
}

static void set_compatibility_aliases(AppGeometry *g) {
  g->language = g->play_language;
  g->title_heading = g->home_logo;
  g->title_buttons[0] = g->home_primary[0];
  g->title_buttons[1] = g->home_primary[1];
  g->title_buttons[2] = g->home_primary[2];
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    g->title_buttons[3 + i] = g->home_secondary[i];
  g->pause_button = g->pause_buttons[0];
}

static bool geometry_compute_base(int width, int height, GeometryMode mode, AppGeometry *g) {
  bool portrait = width >= 360 && height >= 640 && width < 640;
  if (!g || !geometry_window_size_supported(width, height) ||
      mode < GEOMETRY_MODE_CLASSIC || mode > GEOMETRY_MODE_TIME)
    return false;
  memset(g, 0, sizeof(*g));
  g->hud_count = mode == GEOMETRY_MODE_CLASSIC ? 5 : 6;

  if (portrait) {
    int margin = 8;
    int shell_w = width - margin * 2;
    int compact = height < 760;
    g->play_language = (GeoRect){margin, margin, shell_w, 38};
    g->play_title = (GeoRect){margin, margin + 42, shell_w, 24};

    int hud_item_h = compact ? 22 : 24;
    int hud_rows = (g->hud_count + 2) / 3;
    int hud_h = hud_rows * hud_item_h + (hud_rows - 1) * 4;
    int palette_label_h = compact ? 16 : 18;
    int palette_h = compact ? 36 : 40;
    int action_item_h = compact ? 40 : 44;
    int actions_h = action_item_h * 3 + 3 * 2;
    int progress_h = compact ? 22 : 24;
    int board_y = g->play_title.y + g->play_title.h + 4;
    int reserve = 6 + hud_h + 4 + palette_label_h + 4 + palette_h + 6 +
                  actions_h + 6 + progress_h + margin;
    int board = height - board_y - reserve;
    if (board > shell_w) board = shell_w;
    board -= board % 9;
    if (board < 225) return false;
    g->board = (GeoRect){(width - board) / 2, board_y, board, board};

    int y = g->board.y + board + 6;
    grid_gap((GeoRect){margin, y, shell_w, hud_h}, 3, 4, g->hud,
             g->hud_count);
    y += hud_h + 4;
    g->palette_label = (GeoRect){margin, y, shell_w, palette_label_h};
    y += palette_label_h + 4;
    grid_gap((GeoRect){margin, y, shell_w, palette_h}, 9, 3, g->palette,
             GEOMETRY_PALETTE_COUNT);
    y += palette_h + 6;
    grid_gap((GeoRect){margin, y, shell_w, actions_h}, 3, 3, g->actions,
             GEOMETRY_ACTION_COUNT);
    y += actions_h + 6;
    g->progress = (GeoRect){margin, y, shell_w, progress_h};
    g->sidebar = (GeoRect){margin, g->board.y + board + 4, shell_w,
                           g->progress.y + g->progress.h -
                               (g->board.y + board + 4)};
  } else {
    bool xl = width >= 1600 && height >= 900;
    int margin = xl ? 24 : 16;
    int max_shell_w = xl ? 1640 : 1180;
    int max_shell_h = xl ? 1200 : 900;
    int shell_w = width - margin * 2;
    if (shell_w > max_shell_w) shell_w = max_shell_w;
    int shell_h = height - margin * 2;
    if (shell_h > max_shell_h) shell_h = max_shell_h;
    int shell_x = (width - shell_w) / 2;
    int shell_y = (height - shell_h) / 2;
    int gap = xl ? 20 : 16;
    int sidebar_w = xl ? (width >= 2400 ? 400 : 360)
                       : (shell_w < 900 ? 264 : 300);
    int board = shell_h - 8;
    int limit = shell_w - sidebar_w - gap;
    if (board > limit) board = limit;
    int board_cap = xl ? (height >= 1300 ? 1080 : 900) : 720;
    if (board > board_cap) board = board_cap;
    board -= board % 9;
    if (board < 288) return false;
    int composition_w = board + gap + sidebar_w;
    int x = shell_x + (shell_w - composition_w) / 2;
    int board_y = shell_y + (shell_h - board) / 2;
    g->board = (GeoRect){x, board_y, board, board};
    g->sidebar = xl ? (GeoRect){x + board + gap, board_y, sidebar_w, board}
                    : (GeoRect){x + board + gap, shell_y, sidebar_w, shell_h};

    /* An unboxed, centered full wordmark has its own band above the HUD.
       Preserve the controls' current size tier as vertical room varies. */
    int side_h = g->sidebar.h;
    bool compact = side_h < 600;
    int sx = g->sidebar.x, sw = g->sidebar.w;
    int item_h = compact ? 24 : (xl ? 34 : 30);
    int row_gap = compact ? 5 : (xl ? 9 : 8);
    int rows = (g->hud_count + 1) / 2;
    int hud_h = rows * item_h + (rows - 1) * row_gap;
    int original_offset = compact ? 12 : (xl ? 28 : 22);
    int desired_offset = side_h < 480 ? 58 :
                         side_h < 600 ? 68 :
                         side_h < 700 ? 76 :
                         side_h < 900 ? 100 : 126;
    int original_remaining = side_h - (original_offset + hud_h +
                                       (side_h >= 600 ? 18 : 8));
    int tier_floor = xl && original_remaining >= 540 ? 540 :
                     original_remaining >= 495 ? 495 :
                     original_remaining >= 390 ? 390 : 0;
    int available_shift = original_remaining - tier_floor;
    if (available_shift < 0) available_shift = 0;
    int shift = desired_offset - original_offset;
    if (shift > available_shift) shift = available_shift;
    if (shift < 0) shift = 0;
    int hud_offset = original_offset + shift;
    int logo_top = side_h < 480 ? 4 :
                   side_h < 600 ? 6 :
                   side_h < 700 ? 8 :
                   side_h < 900 ? 12 : 18;
    int logo_gap = side_h < 600 ? 8 : 12;
    /* At size-tier boundaries, retaining a taller button tier could
       otherwise reduce the wordmark band to only a handful of pixels.
       Prefer a readable brand and then validate action bounds below. */
    int min_logo_height = side_h < 600 ? 38 : 48;
    if (hud_offset - logo_top - logo_gap < min_logo_height)
      hud_offset = desired_offset;
    int logo_width = side_h < 480 ? 176 :
                     side_h < 600 ? 195 :
                     side_h < 700 ? 210 :
                     side_h < 900 ? 240 : 280;
    if (logo_width > sw - 24) logo_width = sw - 24;
    int logo_height = hud_offset - logo_top - logo_gap;
    g->play_language = (GeoRect){sx, g->sidebar.y + 2, sw, 1};
    g->play_title = (GeoRect){
        sx + (sw - logo_width + 1) / 2, g->sidebar.y + logo_top,
        logo_width, logo_height};
    grid_gap((GeoRect){sx, g->sidebar.y + hud_offset, sw, hud_h},
             2, row_gap, g->hud, g->hud_count);
  }

  common_screens(width, height, portrait, g);
  set_compatibility_aliases(g);
  return true;
}

GeometryFonts geometry_font_sizes(const AppGeometry *g, int width, int height) {
  GeometryFonts f = {12, 14, 16, 16, 14, 28, 36};
  if (!g) return f;
  int cell = g->board.w / 9;
  f.cell = cell * 3 / 5;
  if (f.cell < 16) f.cell = 16;
  if (f.cell > 72) f.cell = 72;
  f.note = f.cell / 3;
  if (f.note < 10) f.note = 10;
  if (f.note > 20) f.note = 20;
  int scale = width < 640 ? 0
                          : (g->board.w >= 810 ? 3
                                              : g->board.w >= 630 ? 2
                                                                  : g->board.w >= 450 ? 1 : 0);
  f.control = scale == 3 ? 24 : scale == 2 ? 20 : scale == 1 ? 18 : 15;
  f.hud = scale == 3 ? 20 : scale == 2 ? 18 : scale == 1 ? 16 : 13;
  f.body = scale == 3 ? 22 : scale == 2 ? 20 : scale == 1 ? 18 : 15;
  f.help = scale == 3 ? 20 : scale == 2 ? 18 : scale == 1 ? 16 : 14;
  f.heading = scale == 3 ? 52 : scale == 2 ? 44 : scale == 1 ? 38 : 30;
  if (height < 600 && f.heading > 32) f.heading = 32;
  return f;
}

GeometryStyle geometry_style(const AppGeometry *g, int width, int height) {
  (void)height;
  GeometryStyle style = {4, 6, 10, 16, 7, 14, 26, 1, 3};
  if (!g) return style;
  bool portrait = width < 640;
  bool xl = width >= 1600 && g->board.w >= 810;
  if (portrait) {
    style.space_xs = 3;
    style.space_sm = 4;
    style.space_md = 8;
    style.space_lg = 12;
    style.radius_control = 6;
    style.radius_panel = 10;
    style.min_control_h = 28;
    style.grid_major_px = 3;
  } else if (xl) {
    style.space_xs = 6;
    style.space_sm = 8;
    style.space_md = 14;
    style.space_lg = 22;
    style.radius_control = 9;
    style.radius_panel = 16;
    style.grid_major_px = 4;
  }
  return style;
}

bool geometry_play_valid(const AppGeometry *g, int width, int height) {
  if (!g || !geometry_rect_in_bounds(g->board, width, height) ||
      !geometry_rect_in_bounds(g->sidebar, width, height) ||
      !geometry_rect_in_bounds(g->play_language, width, height) ||
      !geometry_rect_in_bounds(g->play_title, width, height) ||
      !geometry_rect_in_bounds(g->palette_label, width, height) ||
      !geometry_rect_in_bounds(g->progress, width, height) ||
      !geometry_rect_in_bounds(g->status, width, height))
    return false;

  bool portrait = width < 640;
  if (!portrait && overlaps(g->board, g->sidebar)) return false;
  if (overlaps(g->play_language, g->play_title)) return false;
  for (int i = 0; i < g->hud_count; ++i)
    if (!geometry_rect_in_bounds(g->hud[i], width, height)) return false;
  for (int i = 0; i < GEOMETRY_ACTION_COUNT; ++i) {
    if (!geometry_rect_in_bounds(g->actions[i], width, height) ||
        g->actions[i].h < 26)
      return false;
  }
  for (int i = 0; i < GEOMETRY_PALETTE_COUNT; ++i) {
    if (!geometry_rect_in_bounds(g->palette[i], width, height) ||
        g->palette[i].h < 22 || g->palette[i].w < (portrait ? 32 : 70))
      return false;
  }

  for (int i = 0; i < PLAY_CONTROL_COUNT; ++i) {
    if (!g->play_compact && (i == PLAY_CONTROL_MORE ||
                              i == PLAY_CONTROL_ABOUT)) continue;
    if (!geometry_rect_in_bounds(g->play_controls[i], width, height))
      return false;
  }
  if (g->play_compact) {
    if (!geometry_rect_in_bounds(g->more_panel, width, height)) return false;
    for (int i = 0; i < GEOMETRY_MORE_COUNT; ++i) {
      if (!geometry_rect_in_bounds(g->more_items[i], width, height))
        return false;
      GeoRect r = g->more_items[i], panel = g->more_panel;
      if (r.x < panel.x || r.y < panel.y ||
          r.x + r.w > panel.x + panel.w ||
          r.y + r.h > panel.y + panel.h) return false;
    }
  }

  if (!geometry_rect_in_bounds(g->screen_language, width, height) ||
      !geometry_rect_in_bounds(g->home_logo, width, height) ||
      !geometry_rect_in_bounds(g->home_new_label, width, height) ||
      !geometry_rect_in_bounds(g->home_continue_label, width, height) ||
      !geometry_rect_in_bounds(g->home_mode_label, width, height) ||
      !geometry_rect_in_bounds(g->home_difficulty_label, width, height) ||
      !geometry_rect_in_bounds(g->info_heading, width, height) ||
      !geometry_rect_in_bounds(g->info_body, width, height) ||
      !geometry_rect_in_bounds(g->back_button, width, height) ||
      !geometry_rect_in_bounds(g->about_logo, width, height) ||
      !geometry_rect_in_bounds(g->about_body, width, height) ||
      !geometry_rect_in_bounds(g->about_meta, width, height) ||
      !geometry_rect_in_bounds(g->about_fact, width, height) ||
      !geometry_rect_in_bounds(g->about_study, width, height) ||
      !geometry_rect_in_bounds(g->about_study_link, width, height) ||
      !geometry_rect_in_bounds(g->about_credits, width, height) ||
      !geometry_rect_in_bounds(g->end_logo, width, height) ||
      !geometry_rect_in_bounds(g->end_heading, width, height) ||
      !geometry_rect_in_bounds(g->end_summary, width, height) ||
      !geometry_rect_in_bounds(g->pause_logo, width, height) ||
      !geometry_rect_in_bounds(g->pause_heading, width, height))
    return false;

  if (!geometry_contains(g->about_study,
                         g->about_study_link.x,
                         g->about_study_link.y) ||
      !geometry_contains(g->about_study,
                         g->about_study_link.x + g->about_study_link.w - 1,
                         g->about_study_link.y + g->about_study_link.h - 1))
    return false;

  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i)
    if (!geometry_rect_in_bounds(g->home_mode[i], width, height) ||
        !geometry_rect_in_bounds(g->home_difficulty[i], width, height))
      return false;
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i)
    if (!geometry_rect_in_bounds(g->home_primary[i], width, height)) return false;
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    if (!geometry_rect_in_bounds(g->home_secondary[i], width, height)) return false;
  for (int i = 0; i < GEOMETRY_ABOUT_LINK_COUNT; ++i)
    if (!geometry_rect_in_bounds(g->about_links[i], width, height)) return false;
  for (int i = 0; i < GEOMETRY_END_BUTTON_COUNT; ++i)
    if (!geometry_rect_in_bounds(g->end_buttons[i], width, height)) return false;
  for (int i = 0; i < GEOMETRY_PAUSE_BUTTON_COUNT; ++i)
    if (!geometry_rect_in_bounds(g->pause_buttons[i], width, height)) return false;

  if (overlaps(g->screen_language, g->home_logo)) return false;
  if (overlaps(g->about_body, g->about_fact) ||
      overlaps(g->about_fact, g->about_study) ||
      overlaps(g->about_study, g->about_credits))
    return false;
  return true;
}


/* Final v1.3 presentation geometry. This is the sole exported layout authority. */
/* Final responsive adjustments over the internal base calculation. */
static void geometry_final_shift_rect_y(GeoRect *rect, int delta) {
  if (rect) rect->y += delta;
}

static int geometry_final_rect_bottom(GeoRect rect) { return rect.y + rect.h; }

static void geometry_final_grid(GeoRect *items, int start, int count, int x, int y,
                    int width, int height, int columns, int gap) {
  int cell_w = (width - (columns - 1) * gap) / columns;
  for (int i = 0; i < count; ++i) {
    items[start + i] =
        (GeoRect){x + (i % columns) * (cell_w + gap),
                  y + (i / columns) * (height + gap), cell_w, height};
  }
}

static int geometry_final_place_action_rows(AppGeometry *geometry, int x,
                         int y, int width, int menu_h, int row_h,
                         int group_gap, int row_gap) {
  /* Five explicit functional rows. Each editable pair owns a full-width row
     so neither label is sacrificed to a four-button strip. */
  geometry->actions[PLAY_ACTION_MENU] = (GeoRect){x, y, width, menu_h};
  y += menu_h + group_gap;
  geometry->actions[PLAY_ACTION_PAUSE] = (GeoRect){x, y, width, row_h};
  y += row_h + row_gap;
  geometry->actions[PLAY_ACTION_RESTART] = (GeoRect){x, y, width, row_h};
  y += row_h + group_gap;
  geometry_final_grid(geometry->actions, PLAY_ACTION_HINT, 3, x, y,
                      width, row_h, 3, row_gap);
  y += row_h + row_gap;
  int third = (width - 2 * row_gap) / 3;
  geometry->actions[PLAY_ACTION_AUDIO] =
      (GeoRect){x, y, third * 2 + row_gap, row_h};
  geometry->actions[PLAY_ACTION_HELP] =
      (GeoRect){x + (third + row_gap) * 2, y,
                width - (third + row_gap) * 2, row_h};
  return y + row_h;
}

static bool geometry_final_play_rects_in_bounds(const AppGeometry *geometry, int width,
                                    int height) {
  for (int i = 0; i < GEOMETRY_ACTION_COUNT; ++i)
    if (!geometry_rect_in_bounds(geometry->actions[i], width, height))
      return false;
  for (int i = 0; i < GEOMETRY_PALETTE_COUNT; ++i)
    if (!geometry_rect_in_bounds(geometry->palette[i], width, height))
      return false;
  return geometry_rect_in_bounds(geometry->board, width, height) &&
         geometry_rect_in_bounds(geometry->palette_label, width, height) &&
         geometry_rect_in_bounds(geometry->progress, width, height);
}

static void geometry_final_split(GeoRect whole, int count, int gap,
                                 GeoRect *out) {
  int cell_w = (whole.w - (count - 1) * gap) / count;
  for (int i = 0; i < count; ++i)
    out[i] = (GeoRect){whole.x + i * (cell_w + gap), whole.y,
                       i == count - 1 ? whole.w - i * (cell_w + gap)
                                      : cell_w, whole.h};
}

static bool geometry_final_layout_portrait_play(int width, int height,
                                               AppGeometry *geometry) {
  const int margin = 8, gap = 4;
  int shell_w = width - margin * 2;
  int hud_item_h = 19, hud_gap = 3;
  int hud_rows = (geometry->hud_count + 2) / 3;
  int hud_h = hud_rows * hud_item_h + (hud_rows - 1) * hud_gap;
  int toolbar_y = margin + 43, toolbar_h = 36;
  int hud_y = toolbar_y + toolbar_h + gap;
  int board_y = hud_y + hud_h + 5;
  int reserve_below = 4 + 36 + 4 + 40 + 4 + 18 + 4 + 28 + margin;
  int board = height - board_y - reserve_below;
  if (board > shell_w) board = shell_w;
  board -= board % 9;
  if (board < 288) return false;

  geometry->play_compact = true;
  geometry->play_language = (GeoRect){margin, margin, 1, 1};
  geometry->play_title = (GeoRect){margin, margin + 3, shell_w, 38};
  GeoRect toolbar = {margin, toolbar_y, shell_w, toolbar_h};
  GeoRect top[4], edit[4];
  geometry_final_split(toolbar, 4, gap, top);
  geometry->play_controls[PLAY_CONTROL_MENU] = top[0];
  geometry->play_controls[PLAY_CONTROL_PAUSE] = top[1];
  geometry->play_controls[PLAY_CONTROL_SAVE] = top[2];
  geometry->play_controls[PLAY_CONTROL_MORE] = top[3];

  geometry_final_grid(geometry->hud, 0, geometry->hud_count, margin,
                      hud_y, shell_w, hud_item_h, 3, hud_gap);
  geometry->board = (GeoRect){(width - board) / 2, board_y, board, board};

  int edit_y = board_y + board + 4;
  GeoRect edit_row = {margin, edit_y, shell_w, 36};
  geometry_final_split(edit_row, 4, gap, edit);
  geometry->play_controls[PLAY_CONTROL_UNDO] = edit[0];
  geometry->play_controls[PLAY_CONTROL_REDO] = edit[1];
  geometry->play_controls[PLAY_CONTROL_CLEAR] = edit[2];
  geometry->play_controls[PLAY_CONTROL_NOTES] = edit[3];

  int palette_y = edit_y + 36 + gap;
  geometry->palette_label = (GeoRect){margin, palette_y - 2, shell_w, 1};
  geometry_final_grid(geometry->palette, 0, GEOMETRY_PALETTE_COUNT,
                      margin, palette_y, shell_w, 40,
                      GEOMETRY_PALETTE_COUNT, 2);
  geometry->progress = (GeoRect){margin, palette_y + 44, shell_w, 18};
  geometry->status = (GeoRect){margin, height - margin - 28, shell_w, 28};
  if (geometry_final_rect_bottom(geometry->progress) + 4 >
      geometry->status.y) return false;

  /* The panel geometry is calculated even when hidden, but never shares a
     hit target with gameplay until the transient panel is open. */
  int panel_w = shell_w - 20;
  int panel_h = 252;
  int panel_x = (width - panel_w) / 2;
  int panel_y = (height - panel_h) / 2;
  geometry->more_panel = (GeoRect){panel_x, panel_y, panel_w, panel_h};
  int item_w = (panel_w - 24 - gap) / 2;
  for (int i = 0; i < GEOMETRY_MORE_COUNT; ++i) {
    geometry->more_items[i] =
        (GeoRect){panel_x + 12 + (i % 2) * (item_w + gap),
                  panel_y + 64 + (i / 2) * 57,
                  item_w, 46};
  }
  geometry->play_controls[PLAY_CONTROL_RESTART] =
      geometry->more_items[MORE_RESTART];
  geometry->play_controls[PLAY_CONTROL_HINT] =
      geometry->more_items[MORE_HINT];
  geometry->play_controls[PLAY_CONTROL_VERIFY] =
      geometry->more_items[MORE_VERIFY];
  geometry->play_controls[PLAY_CONTROL_SETTINGS] =
      geometry->more_items[MORE_SETTINGS];
  geometry->play_controls[PLAY_CONTROL_HELP] =
      geometry->more_items[MORE_HELP];
  geometry->play_controls[PLAY_CONTROL_ABOUT] =
      geometry->more_items[MORE_ABOUT];

  /* Legacy action slots stay valid for geometry consumers, but compact
     interaction uses play_controls and never these overlapping aliases. */
  geometry->actions[PLAY_ACTION_MENU] = toolbar;
  geometry->actions[PLAY_ACTION_PAUSE] = edit_row;
  geometry->actions[PLAY_ACTION_RESTART] = edit_row;
  geometry->actions[PLAY_ACTION_HINT] = geometry->more_items[MORE_HINT];
  geometry->actions[PLAY_ACTION_NOTES] = edit[3];
  geometry->actions[PLAY_ACTION_VERIFY] = geometry->more_items[MORE_VERIFY];
  geometry->actions[PLAY_ACTION_AUDIO] = (GeoRect){margin, margin + 5, 76, 34};
  geometry->actions[PLAY_ACTION_HELP] = geometry->more_items[MORE_HELP];
  geometry->actions[PLAY_ACTION_ABOUT] = geometry->status;
  int sidebar_y = board_y + board + 2;
  geometry->sidebar = (GeoRect){margin, sidebar_y, shell_w,
                               height - margin - sidebar_y};
  return geometry_final_play_rects_in_bounds(geometry, width, height);
}

static void geometry_final_play_controls_regular(AppGeometry *geometry,
                                                 int width, int height) {
  (void)height;
  const int gap = geometry->actions[PLAY_ACTION_MENU].w >= 180 ? 6 : 4;
  GeoRect buttons[3];
  geometry_final_split(geometry->actions[PLAY_ACTION_MENU], 3, gap, buttons);
  geometry->play_controls[PLAY_CONTROL_MENU] = buttons[0];
  geometry->play_controls[PLAY_CONTROL_PAUSE] = buttons[1];
  geometry->play_controls[PLAY_CONTROL_SAVE] = buttons[2];
  geometry_final_split(geometry->actions[PLAY_ACTION_PAUSE], 2, gap, buttons);
  geometry->play_controls[PLAY_CONTROL_UNDO] = buttons[0];
  geometry->play_controls[PLAY_CONTROL_REDO] = buttons[1];
  geometry_final_split(geometry->actions[PLAY_ACTION_RESTART], 2, gap, buttons);
  geometry->play_controls[PLAY_CONTROL_CLEAR] = buttons[0];
  geometry->play_controls[PLAY_CONTROL_RESTART] = buttons[1];
  geometry->play_controls[PLAY_CONTROL_NOTES] =
      geometry->actions[PLAY_ACTION_NOTES];
  geometry->play_controls[PLAY_CONTROL_HINT] =
      geometry->actions[PLAY_ACTION_HINT];
  geometry->play_controls[PLAY_CONTROL_VERIFY] =
      geometry->actions[PLAY_ACTION_VERIFY];
  int speaker_w = width < 640 ? 28 :
      geometry->actions[PLAY_ACTION_AUDIO].h > 40 ? 40 :
      geometry->actions[PLAY_ACTION_AUDIO].h;
  GeoRect settings = geometry->actions[PLAY_ACTION_AUDIO];
  settings.w -= speaker_w + 4;
  geometry->play_controls[PLAY_CONTROL_SETTINGS] = settings;
  geometry->play_controls[PLAY_CONTROL_HELP] =
      geometry->actions[PLAY_ACTION_HELP];
}

static bool geometry_final_layout_desktop_play(int width, int height,
                                   AppGeometry *geometry) {
  int hud_bottom = geometry_final_rect_bottom(geometry->play_title);
  for (int i = 0; i < geometry->hud_count; ++i)
    if (geometry_final_rect_bottom(geometry->hud[i]) > hud_bottom)
      hud_bottom = geometry_final_rect_bottom(geometry->hud[i]);

  int top = hud_bottom + (geometry->sidebar.h >= 600 ? 18 : 8);
  int bottom = geometry_final_rect_bottom(geometry->sidebar);
  int remaining = bottom - top;
  bool xl = width >= 1600 && height >= 900;
  int section_gap, action_gap, menu_h, row_h, palette_label_h;
  int palette_item_h, palette_gap, progress_h, status_h;

  if (xl && remaining >= 540) {
    section_gap = 7;
    action_gap = 6;
    menu_h = 48;
    row_h = 45;
    palette_label_h = 22;
    palette_item_h = 44;
    palette_gap = 6;
    progress_h = 30;
    status_h = 42;
  } else if (remaining >= 495) {
    section_gap = 10;
    action_gap = 7;
    menu_h = 42;
    row_h = 40;
    palette_label_h = 18;
    palette_item_h = 36;
    palette_gap = 7;
    progress_h = 26;
    status_h = 34;
  } else if (remaining >= 390) {
    section_gap = 5;
    action_gap = 4;
    menu_h = 38;
    row_h = 36;
    palette_label_h = 18;
    palette_item_h = 30;
    palette_gap = 4;
    progress_h = 24;
    status_h = 34;
  } else {
    section_gap = 2;
    action_gap = 2;
    menu_h = 28;
    row_h = 28;
    palette_label_h = 12;
    palette_item_h = 22;
    palette_gap = 2;
    progress_h = 18;
    status_h = 26;
  }

  int x = geometry->sidebar.x;
  int w = geometry->sidebar.w;
  int action_group_gap = xl && remaining >= 540 ? 12 :
                         remaining >= 495 ? 16 :
                         remaining >= 390 ? 8 : 2;
  int y = geometry_final_place_action_rows(
      geometry, x, top, w, menu_h, row_h, action_group_gap, action_gap);
  y += section_gap;
  geometry->palette_label = (GeoRect){x, y, w, palette_label_h};
  y += palette_label_h + (section_gap > 2 ? 3 : 2);
  geometry_final_grid(geometry->palette, 0, GEOMETRY_PALETTE_COUNT, x, y, w,
          palette_item_h, 3, palette_gap);
  int palette_h = palette_item_h * 3 + palette_gap * 2;
  y += palette_h + section_gap;
  geometry->progress = (GeoRect){x, y, w, progress_h};

  int status_y = bottom - status_h;
  if (geometry_final_rect_bottom(geometry->progress) + section_gap > status_y)
    return false;
  geometry->status = (GeoRect){x, status_y, w, status_h};
  geometry->actions[PLAY_ACTION_ABOUT] = geometry->status;
  return geometry_final_play_rects_in_bounds(geometry, width, height);
}

static int geometry_final_audio_button_size(int width, int height) {
  int size = width < 640 ? 28 : height;
  if (size > 40) size = 40;
  if (size < 28) size = 28;
  return size;
}

static void geometry_final_reserve_audio(GeoRect *language, int width) {
  if (!language || language->w <= 0 || language->h <= 0) return;
  int size = geometry_final_audio_button_size(width, language->h);
  int gap = 4;
  if (language->w > size + gap + 96) language->w -= size + gap;
}

bool geometry_compute(int width, int height, GeometryMode mode,
                      AppGeometry *geometry) {
  if (!geometry_compute_base(width, height, mode, geometry)) return false;
  _Static_assert(PLAY_ACTION_COUNT == GEOMETRY_ACTION_COUNT,
                 "play-action geometry mismatch");

  if (width < 640) {
    if (!geometry_final_layout_portrait_play(width, height, geometry)) return false;
  } else {
    int min_hud_offset = geometry->sidebar.h < 600 ? 12 :
                         width >= 1600 && height >= 900 ? 28 : 22;
    int min_hud_y = geometry->sidebar.y + min_hud_offset;
    bool fitted = false;
    /* The narrowest heights may require a smaller branding band. Keep
       every control's existing minimum dimensions and usable hit areas. */
    for (;;) {
      if (geometry_final_layout_desktop_play(width, height, geometry)) {
        fitted = true;
        break;
      }
      if (geometry->hud[0].y <= min_hud_y) break;
      for (int i = 0; i < geometry->hud_count; ++i)
        --geometry->hud[i].y;
      if (geometry->play_title.h > 1) --geometry->play_title.h;
    }
    if (!fitted) return false;
    geometry_final_play_controls_regular(geometry, width, height);
  }

  geometry_final_reserve_audio(&geometry->screen_language, width);

  if (width < 640) return geometry_play_valid(geometry, width, height);

  int top = geometry->home_logo.y;
  int bottom = geometry_final_rect_bottom(geometry->home_logo);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    if (geometry_final_rect_bottom(geometry->home_mode[i]) > bottom)
      bottom = geometry_final_rect_bottom(geometry->home_mode[i]);
    if (geometry_final_rect_bottom(geometry->home_difficulty[i]) > bottom)
      bottom = geometry_final_rect_bottom(geometry->home_difficulty[i]);
  }
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i)
    if (geometry_final_rect_bottom(geometry->home_primary[i]) > bottom)
      bottom = geometry_final_rect_bottom(geometry->home_primary[i]);
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    if (geometry_final_rect_bottom(geometry->home_secondary[i]) > bottom)
      bottom = geometry_final_rect_bottom(geometry->home_secondary[i]);

  int block_height = bottom - top;
  int available_top =
      geometry->screen_language.y + geometry->screen_language.h + 12;
  int available_bottom = height - 24;
  int spare = available_bottom - available_top - block_height;
  if (spare <= 0) return geometry_play_valid(geometry, width, height);

  int target_top = available_top + spare * 2 / 5;
  int delta = target_top - top;
  if (delta <= 0) return geometry_play_valid(geometry, width, height);

  geometry_final_shift_rect_y(&geometry->home_logo, delta);
  geometry_final_shift_rect_y(&geometry->home_new_label, delta);
  geometry_final_shift_rect_y(&geometry->home_continue_label, delta);
  geometry_final_shift_rect_y(&geometry->home_mode_label, delta);
  geometry_final_shift_rect_y(&geometry->home_difficulty_label, delta);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    geometry_final_shift_rect_y(&geometry->home_mode[i], delta);
    geometry_final_shift_rect_y(&geometry->home_difficulty[i], delta);
  }
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i)
    geometry_final_shift_rect_y(&geometry->home_primary[i], delta);
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    geometry_final_shift_rect_y(&geometry->home_secondary[i], delta);

  return geometry_play_valid(geometry, width, height);
}



/* The complete canonical Home artwork is reused during gameplay.
   Desktop geometry provides a separate band; portrait reuses the existing
   unused space above the board without shrinking Sudoku cells. */
GeoRect geometry_play_wordmark_rect(const AppGeometry *geometry, int width) {
  GeoRect empty = {0, 0, 0, 0};
  if (!geometry || width <= 0) return empty;
  if (width >= 640) return geometry->play_title;

#if defined(__ANDROID__)
  int logo_width = geometry->play_title.w - 24;
  if (logo_width > 220) logo_width = 220;
  return (GeoRect){geometry->play_title.x +
                       (geometry->play_title.w - logo_width) / 2,
                   geometry->play_title.y, logo_width,
                   geometry->play_title.h};
#else
  int logo_width = geometry->play_title.w - 100;
  if (logo_width > 200) logo_width = 200;
  if (logo_width < 150) logo_width = 150;
  return (GeoRect){geometry->play_title.x + geometry->play_title.w -
                       logo_width - 8,
                   geometry->play_title.y, logo_width,
                   geometry->play_title.h};
#endif
}

GeoRect geometry_play_heading_rect(const AppGeometry *geometry, int width) {
  GeoRect empty = {0, 0, 0, 0};
  if (!geometry || width < 640) return empty;
  int left = geometry->hud[0].x + 6;
  int right = geometry->hud[1].x + geometry->hud[1].w - 6;
  if (right <= left) return empty;
  return (GeoRect){left, geometry->hud[0].y,
                   right - left, geometry->hud[0].h};
}

AudioControlGeometry geometry_audio_control(int width, int height,
                                            const AppGeometry *app,
                                            bool play_surface) {
  AudioControlGeometry out;
  memset(&out, 0, sizeof(out));
  if (width <= 0 || height <= 0) return out;

  if (app && play_surface) {
    GeoRect settings = app->actions[PLAY_ACTION_AUDIO];
    int size = geometry_final_audio_button_size(width, settings.h);
    bool phone_toolbar = app->play_compact &&
                         settings.h >= 48 && settings.w >= 48;
    if (phone_toolbar) size = 48;
    else if (size > settings.w / 2) size = settings.w / 2;
    out.button =
        (GeoRect){phone_toolbar ? settings.x + (settings.w - size) / 2
                               : settings.x + settings.w - size,
                  settings.y + (settings.h - size) / 2, size, size};
  } else if (app) {
    GeoRect language = app->screen_language;
    int size = (app->play_compact && language.h >= 48) ? 48 :
               geometry_final_audio_button_size(width, language.h);
    out.button =
        (GeoRect){language.x + language.w + 4,
                  language.y + (language.h - size) / 2, size, size};
  } else {
    int size = width < 420 ? 36 : 40;
    int margin = width < 420 ? 8 : 12;
    out.button = (GeoRect){width - margin - size, margin, size, size};
  }

  int popup_w = width < 260 ? width - 16 : 220;
  if (popup_w < 140) popup_w = 140;
  int popup_h = app && app->play_compact ? 128 : 92;
  int popup_x = out.button.x + out.button.w - popup_w;
  if (popup_x < 8) popup_x = 8;
  if (popup_x + popup_w > width - 8) popup_x = width - 8 - popup_w;
  int popup_y = out.button.y + out.button.h + 6;
  if (popup_y + popup_h > height - 8)
    popup_y = out.button.y - popup_h - 6;
  if (popup_y < 8) popup_y = 8;
  out.popup = (GeoRect){popup_x, popup_y, popup_w, popup_h};

  int inset = 8;
  int gap = 6;
  int row_h = (popup_h - inset * 2 - gap) / 2;
  out.music = (GeoRect){popup_x + inset, popup_y + inset,
                        popup_w - inset * 2, row_h};
  out.fx = (GeoRect){popup_x + inset, popup_y + inset + row_h + gap,
                     popup_w - inset * 2, row_h};
  return out;
}


/* A narrow screen cannot fit the hint's text/actions into the bottom sidebar.
   Reuse the same full-screen overlay geometry for drawing and hit-testing. */
bool geometry_hint_panel(const AppGeometry *geometry, int width, int height,
                         GeometryHintPanel *out) {
  if (!geometry || !out || width <= 0 || height <= 0) return false;
  memset(out, 0, sizeof(*out));
  GeoRect card;
  if (geometry->play_compact) {
    int card_h = height - 80;
    if (card_h > 430) card_h = 430;
    card = (GeoRect){16, (height - card_h) / 2, width - 32, card_h};
  } else {
    GeoRect side = geometry->sidebar;
    int margin = side.w < 240 ? 7 : 10;
    card = (GeoRect){side.x + margin, side.y + margin,
                     side.w - margin * 2, side.h - margin * 2};
  }
  if (!geometry_rect_in_bounds(card, width, height)) return false;
  out->card = card;
  int x = card.x + 10, item_w = card.w - 20, y = card.y + 10;
  out->title = (GeoRect){x, y, item_w, 32};
  int button_h = card.h < 430 ? 31 : 36, gap = 6;
  out->cancel = (GeoRect){x, card.y + card.h - button_h - 10,
                           item_w, button_h};
  out->reveal = (GeoRect){x, out->cancel.y - button_h - gap,
                           item_w, button_h};
  out->verify = (GeoRect){x, out->reveal.y - button_h - gap,
                           item_w, button_h};
  out->primary = (GeoRect){x, out->verify.y - button_h - gap,
                            item_w, button_h};
  out->body = (GeoRect){x, out->title.y + out->title.h + 4,
                         item_w,
                         out->primary.y - (out->title.y + out->title.h + 8)};
  return out->body.h > 8 && out->body.w > 8 &&
         geometry_rect_in_bounds(out->title, width, height) &&
         geometry_rect_in_bounds(out->body, width, height) &&
         geometry_rect_in_bounds(out->primary, width, height) &&
         geometry_rect_in_bounds(out->verify, width, height) &&
         geometry_rect_in_bounds(out->reveal, width, height) &&
         geometry_rect_in_bounds(out->cancel, width, height);
}

/* Mobile Home: use the unused vertical room for real 48dp touch targets,
   not desktop's 33dp footer. Suppress redundant group captions on the
   minimum supported height; shared geometry remains the hit-test owner. */
static void geometry_mobile_home(int width, int height, AppGeometry *g) {
  const int margin = 6, gap = 6, w = width - 2 * margin;
  const bool roomy = height >= 700;
  const int caption_h = 1; /* Mobile action labels already name each group. */
  g->screen_language = (GeoRect){margin, 6, w - 52, 48};

  int y = 60;
  int logo_h = height >= 760 ? 80 : height >= 680 ? 64 : 40;
  g->home_logo = (GeoRect){margin, y, w, logo_h};
  y += logo_h + 4;
  g->home_new_label = (GeoRect){margin, y, w, caption_h};
  y += caption_h + (roomy ? 2 : 0);
  g->home_mode_label = (GeoRect){margin, y, w, 18};
  y += 20;
  grid_gap((GeoRect){margin, y, w, 48}, 3, gap,
           g->home_mode, GEOMETRY_HOME_SEGMENT_COUNT);
  y += 48 + 5;
  g->home_difficulty_label = (GeoRect){margin, y, w, 18};
  y += 20;
  grid_gap((GeoRect){margin, y, w, 48}, 3, gap,
           g->home_difficulty, GEOMETRY_HOME_SEGMENT_COUNT);
  y += 48 + 7;
  g->home_primary[0] = (GeoRect){margin, y, w, 48};
  y += 48 + 6;
  g->home_continue_label = (GeoRect){margin, y, w, caption_h};
  y += caption_h + (roomy ? 2 : 0);
  g->home_primary[1] = (GeoRect){margin, y, w, 48};
  y += 48 + 6;
  g->home_primary[2] = (GeoRect){margin, y, w, 48};
  y += 48 + 8;
  g->home_secondary[0] = (GeoRect){margin, y, w, 48};
  y += 48 + 8;

  GeoRect secondary[GEOMETRY_HOME_SECONDARY_COUNT - 1];
  grid_gap((GeoRect){margin, y, w, 102}, 2, gap, secondary,
           GEOMETRY_HOME_SECONDARY_COUNT - 1);
  for (int i = 1; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    g->home_secondary[i] = secondary[i - 1];
}

/* Android's physical surface has already had system insets removed by Java.
   Compose in dp here, keeping desktop's 360x640 minimum untouched. */
bool geometry_compute_mobile(int width, int height, GeometryMode mode,
                             AppGeometry *g) {
  if (!g || width < 360 || height < 600) return false;
  if (width >= 640) return geometry_compute(width, height, mode, g);

  const int seed_height = height < 640 ? 640 : height;
  if (!geometry_compute(width, seed_height, mode, g)) return false;
  /* Recompose the other screens at current inset-adjusted usable height. */
  common_screens(width, height, true, g);
  geometry_mobile_home(width, height, g);

  const int margin = 6, gap = 6, shell_w = width - 2 * margin;
  const int logo_h = height >= 720 ? 36 : height >= 660 ? 28 : 20;
  const int toolbar_y = logo_h + 8;
  const int hud_y = toolbar_y + 48 + 6;
  const int board_y = hud_y + 46 + 4;
  const bool separate_status = height >= 680;
  const int reserve_after_board = separate_status ? 228 : 198;

  int board = shell_w;
  if (board > height - board_y - reserve_after_board)
    board = height - board_y - reserve_after_board;
  board -= board % 9;
  if (board < 270) return false;

  g->play_compact = true;
  g->play_language = (GeoRect){margin, 1, 1, 1};
  g->play_title = (GeoRect){margin, 4, shell_w, logo_h};
  GeoRect top[5], edit[4];
  geometry_final_split((GeoRect){margin, toolbar_y, shell_w, 48},
                       5, gap, top);
  g->play_controls[PLAY_CONTROL_MENU] = top[0];
  g->play_controls[PLAY_CONTROL_PAUSE] = top[1];
  g->play_controls[PLAY_CONTROL_SAVE] = top[2];
  g->play_controls[PLAY_CONTROL_MORE] = top[3];

  geometry_final_grid(g->hud, 0, g->hud_count, margin, hud_y,
                      shell_w, 22, 3, 2);
  g->board = (GeoRect){(width - board) / 2, board_y, board, board};
  /* Spend tall-phone headroom on real touch surfaces and separation. */
  const int baseline_bottom = board_y + board + 6 + 54 + 108 + 24 +
                              (separate_status ? 30 : 0) + 24;
  int spare = height - baseline_bottom - 12;
  if (spare < 0) spare = 0;
  const int board_gap = 6 + (spare / 8 < 14 ? spare / 8 : 14);
  const int edit_h = 48 + (spare / 5 < 12 ? spare / 5 : 12);
  const int key_h = 48 + (spare / 4 < 16 ? spare / 4 : 16);
  const int row_gap = 6 + (spare / 20 < 6 ? spare / 20 : 6);
  const int progress_gap = 6 + (spare / 20 < 6 ? spare / 20 : 6);
  const int edit_y = board_y + board + board_gap;
  geometry_final_split((GeoRect){margin, edit_y, shell_w, edit_h},
                       4, gap, edit);
  g->play_controls[PLAY_CONTROL_UNDO] = edit[0];
  g->play_controls[PLAY_CONTROL_REDO] = edit[1];
  g->play_controls[PLAY_CONTROL_CLEAR] = edit[2];
  g->play_controls[PLAY_CONTROL_NOTES] = edit[3];

  g->palette_label = (GeoRect){margin, edit_y + edit_h, shell_w, 1};
  const int palette_y = edit_y + edit_h + 6;
  const int key_w = (shell_w - 4 * gap) / 5;
  for (int i = 0; i < 5; ++i)
    g->palette[i] = (GeoRect){margin + i * (key_w + gap),
                             palette_y, key_w, key_h};
  const int second_w = 4 * key_w + 3 * gap;
  const int second_x = (width - second_w) / 2;
  const int second_y = palette_y + key_h + row_gap;
  for (int i = 5; i < GEOMETRY_PALETTE_COUNT; ++i)
    g->palette[i] = (GeoRect){second_x + (i - 5) * (key_w + gap),
                             second_y, key_w, key_h};
  g->progress = (GeoRect){margin, second_y + key_h + progress_gap,
                          shell_w, 24};
  /* At 360x600 the transient toast replaces the progress row rather than
     erasing it with a second, always-empty panel. */
  g->status = (GeoRect){margin, g->progress.y + (separate_status ? 30 : 0),
                        shell_w, 24};
  int bottom = g->status.y + g->status.h;
  g->sidebar = (GeoRect){margin, edit_y, shell_w, bottom - edit_y};

  const int panel_w = shell_w - 20, panel_h = 252;
  const int panel_x = (width - panel_w) / 2;
  const int panel_y = height - panel_h - (height >= 700 ? 24 : 10);
  g->more_panel = (GeoRect){panel_x, panel_y, panel_w, panel_h};
  const int item_w = (panel_w - 24 - gap) / 2;
  for (int i = 0; i < GEOMETRY_MORE_COUNT; ++i)
    g->more_items[i] = (GeoRect){
        panel_x + 12 + (i % 2) * (item_w + gap),
        panel_y + 56 + (i / 2) * 56, item_w, 48};
  g->play_controls[PLAY_CONTROL_RESTART] = g->more_items[MORE_RESTART];
  g->play_controls[PLAY_CONTROL_HINT] = g->more_items[MORE_HINT];
  g->play_controls[PLAY_CONTROL_VERIFY] = g->more_items[MORE_VERIFY];
  g->play_controls[PLAY_CONTROL_SETTINGS] = g->more_items[MORE_SETTINGS];
  g->play_controls[PLAY_CONTROL_HELP] = g->more_items[MORE_HELP];
  g->play_controls[PLAY_CONTROL_ABOUT] = g->more_items[MORE_ABOUT];

  g->actions[PLAY_ACTION_MENU] = top[0];
  g->actions[PLAY_ACTION_PAUSE] = top[1];
  g->actions[PLAY_ACTION_RESTART] = g->more_items[MORE_RESTART];
  g->actions[PLAY_ACTION_HINT] = g->more_items[MORE_HINT];
  g->actions[PLAY_ACTION_NOTES] = edit[3];
  g->actions[PLAY_ACTION_VERIFY] = g->more_items[MORE_VERIFY];
  g->actions[PLAY_ACTION_AUDIO] = top[4];
  g->actions[PLAY_ACTION_HELP] = g->more_items[MORE_HELP];
  g->actions[PLAY_ACTION_ABOUT] = g->more_items[MORE_ABOUT];
  set_compatibility_aliases(g);
  return geometry_play_valid(g, width, height);
}
