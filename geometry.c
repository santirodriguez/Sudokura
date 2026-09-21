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

  int home_w = portrait ? width - margin * 2
                        : min_i(width - margin * 2, xl ? 700 : 560);
  int home_x = (width - home_w) / 2;
  int logo_h = portrait ? 64 : (height < 600 ? 68 : (xl ? 112 : 96));
  int y = g->screen_language.y + g->screen_language.h + 8;
  g->home_logo = (GeoRect){home_x, y, home_w, logo_h};
  y += logo_h + 8;

  int label_h = xl ? 18 : 16;
  g->home_mode_label = (GeoRect){home_x, y, home_w, label_h};
  y += label_h + 2;
  int segment_h = portrait ? 36 : (xl ? 42 : 34);
  grid_gap((GeoRect){home_x, y, home_w, segment_h}, 3, 6, g->home_mode,
           GEOMETRY_HOME_SEGMENT_COUNT);
  y += segment_h + 6;
  g->home_difficulty_label = (GeoRect){home_x, y, home_w, label_h};
  y += label_h + 2;
  grid_gap((GeoRect){home_x, y, home_w, segment_h}, 3, 6,
           g->home_difficulty, GEOMETRY_HOME_SEGMENT_COUNT);
  y += segment_h + 12;

  if (portrait) {
    int primary_h = 46 * GEOMETRY_HOME_PRIMARY_COUNT +
                    8 * (GEOMETRY_HOME_PRIMARY_COUNT - 1);
    grid_gap((GeoRect){home_x, y, home_w, primary_h}, 1, 8, g->home_primary,
             GEOMETRY_HOME_PRIMARY_COUNT);
    y += primary_h + 12;
    int secondary_h = 40 * 2 + 8;
    grid_gap((GeoRect){home_x, y, home_w, secondary_h}, 2, 8,
             g->home_secondary, GEOMETRY_HOME_SECONDARY_COUNT);
  } else {
    int primary_h = xl ? 54 : 48;
    int secondary_h = xl ? 44 : 38;
    grid_gap((GeoRect){home_x, y, home_w, primary_h}, 3, 8, g->home_primary,
             GEOMETRY_HOME_PRIMARY_COUNT);
    y += primary_h + 10;
    grid_gap((GeoRect){home_x, y, home_w, secondary_h}, 4, 8,
             g->home_secondary, GEOMETRY_HOME_SECONDARY_COUNT);
  }

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

    bool compact = g->sidebar.h < 600;
    int sx = g->sidebar.x, sw = g->sidebar.w;
    int language_h = compact ? 34 : (xl ? 48 : 40);
    int title_h = compact ? 52 : (xl ? 120 : 76);
    int hud_item_h = compact ? 22 : (xl ? 34 : 26);
    int hud_rows = (g->hud_count + 1) / 2;
    int hud_gap = compact ? 4 : (xl ? 8 : 5);
    int hud_h = hud_rows * hud_item_h + (hud_rows - 1) * hud_gap;
    int action_item_h = compact ? 40 : (xl ? 56 : 44);
    int action_gap = compact ? 4 : (xl ? 8 : 6);
    int actions_h = action_item_h * 3 + action_gap * 2;
    int palette_item_h = compact ? 30 : (xl ? 48 : 38);
    int palette_gap = compact ? 4 : (xl ? 8 : 6);
    int palette_h = palette_item_h * 3 + palette_gap * 2;
    int palette_label_h = compact ? 16 : (xl ? 24 : 20);
    int progress_h = compact ? 22 : (xl ? 34 : 26);

    int fixed_h = language_h + title_h + hud_h + actions_h + palette_label_h +
                  palette_h + progress_h;
    int available_gap = g->sidebar.h - fixed_h;
    int gap_count = 6;
    int preferred_gap = compact ? 4 : (xl ? 24 : 20);
    int vertical_gap = available_gap > 0 ? available_gap / gap_count : 0;
    if (vertical_gap > preferred_gap) vertical_gap = preferred_gap;
    if (vertical_gap < (compact ? 3 : 5)) vertical_gap = compact ? 3 : 5;
    int used_h = fixed_h + vertical_gap * gap_count;
    int edge_space = g->sidebar.h - used_h;
    if (edge_space < 0) edge_space = 0;
    int top_pad = edge_space / 2;
    if (top_pad > (xl ? 36 : 24)) top_pad = xl ? 36 : 24;
    int sy = g->sidebar.y + top_pad;

    g->play_language = (GeoRect){sx, sy, sw, language_h};
    sy += language_h + vertical_gap;
    g->play_title = (GeoRect){sx, sy, sw, title_h};
    sy += title_h + vertical_gap;
    grid_gap((GeoRect){sx, sy, sw, hud_h}, 2, hud_gap, g->hud,
             g->hud_count);
    sy += hud_h + vertical_gap;
    grid_gap((GeoRect){sx, sy, sw, actions_h}, 3, action_gap, g->actions,
             GEOMETRY_ACTION_COUNT);
    sy += actions_h + vertical_gap;
    g->palette_label = (GeoRect){sx, sy, sw, palette_label_h};
    sy += palette_label_h + vertical_gap;
    grid_gap((GeoRect){sx, sy, sw, palette_h}, 3, palette_gap, g->palette,
             GEOMETRY_PALETTE_COUNT);
    sy += palette_h + vertical_gap;
    g->progress = (GeoRect){sx, sy, sw, progress_h};
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

  if (!geometry_rect_in_bounds(g->screen_language, width, height) ||
      !geometry_rect_in_bounds(g->home_logo, width, height) ||
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

static int geometry_final_place_action_rows(AppGeometry *geometry, int x, int y, int width,
                                int menu_h, int row_h, int gap) {
  geometry->actions[PLAY_ACTION_MENU] = (GeoRect){x, y, width, menu_h};
  y += menu_h + gap;
  geometry_final_grid(geometry->actions, PLAY_ACTION_PAUSE, 2, x, y, width, row_h, 2, gap);
  y += row_h + gap;
  geometry_final_grid(geometry->actions, PLAY_ACTION_HINT, 3, x, y, width, row_h, 3, gap);
  y += row_h + gap;
  geometry_final_grid(geometry->actions, PLAY_ACTION_AUDIO, 2, x, y, width, row_h, 2, gap);
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

static bool geometry_final_layout_portrait_play(int width, int height,
                                    AppGeometry *geometry) {
  int margin = 8;
  int shell_w = width - margin * 2;
  bool compact = height < 760;
  int section_gap = compact ? 4 : 6;
  int action_gap = compact ? 2 : 3;
  int action_h = compact ? 28 : 32;
  int menu_h = compact ? 28 : 34;
  int status_h = compact ? 28 : 32;
  int hud_item_h = compact ? 20 : 22;
  int hud_rows = (geometry->hud_count + 2) / 3;
  int hud_gap = 3;
  int hud_h = hud_rows * hud_item_h + (hud_rows - 1) * hud_gap;
  int palette_label_h = compact ? 14 : 16;
  int palette_h = compact ? 30 : 34;
  int progress_h = compact ? 18 : 22;
  int actions_h = menu_h + action_h * 3 + action_gap * 3;

  geometry->play_language = (GeoRect){margin, margin, shell_w, 38};
  geometry->play_title = (GeoRect){margin, margin + 42, shell_w, 24};
  int board_y = geometry_final_rect_bottom(geometry->play_title) + 4;
  int reserve = section_gap + hud_h + section_gap + actions_h + section_gap +
                palette_label_h + 2 + palette_h + section_gap + progress_h +
                section_gap + status_h + margin;
  int board = height - board_y - reserve;
  if (board > shell_w) board = shell_w;
  board -= board % 9;
  if (board < 225) return false;
  geometry->board = (GeoRect){(width - board) / 2, board_y, board, board};

  int y = geometry_final_rect_bottom(geometry->board) + section_gap;
  geometry_final_grid(geometry->hud, 0, geometry->hud_count, margin, y, shell_w,
          hud_item_h, 3, hud_gap);
  y += hud_h + section_gap;
  y = geometry_final_place_action_rows(geometry, margin, y, shell_w, menu_h, action_h,
                           action_gap);
  y += section_gap;
  geometry->palette_label = (GeoRect){margin, y, shell_w, palette_label_h};
  y += palette_label_h + 2;
  geometry_final_grid(geometry->palette, 0, GEOMETRY_PALETTE_COUNT, margin, y, shell_w,
          palette_h, GEOMETRY_PALETTE_COUNT, 2);
  y += palette_h + section_gap;
  geometry->progress = (GeoRect){margin, y, shell_w, progress_h};
  int status_y = height - margin - status_h;
  if (geometry_final_rect_bottom(geometry->progress) + section_gap > status_y)
    return false;
  geometry->status = (GeoRect){margin, status_y, shell_w, status_h};
  geometry->actions[PLAY_ACTION_ABOUT] = geometry->status;
  int sidebar_y = geometry_final_rect_bottom(geometry->board) + 2;
  geometry->sidebar = (GeoRect){margin, sidebar_y, shell_w,
                                height - margin - sidebar_y};
  return geometry_final_play_rects_in_bounds(geometry, width, height);
}

static bool geometry_final_layout_desktop_play(int width, int height,
                                   AppGeometry *geometry) {
  int hud_bottom = geometry_final_rect_bottom(geometry->play_title);
  for (int i = 0; i < geometry->hud_count; ++i)
    if (geometry_final_rect_bottom(geometry->hud[i]) > hud_bottom)
      hud_bottom = geometry_final_rect_bottom(geometry->hud[i]);

  int top = hud_bottom + 4;
  int bottom = geometry_final_rect_bottom(geometry->sidebar);
  int remaining = bottom - top;
  bool xl = width >= 1600 && height >= 900;
  int section_gap, action_gap, menu_h, row_h, palette_label_h;
  int palette_item_h, palette_gap, progress_h, status_h;

  if (xl && remaining >= 540) {
    section_gap = 7;
    action_gap = 6;
    menu_h = 50;
    row_h = 48;
    palette_label_h = 22;
    palette_item_h = 44;
    palette_gap = 6;
    progress_h = 30;
    status_h = 42;
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
    menu_h = 30;
    row_h = 28;
    palette_label_h = 12;
    palette_item_h = 22;
    palette_gap = 2;
    progress_h = 18;
    status_h = 26;
  }

  int x = geometry->sidebar.x;
  int w = geometry->sidebar.w;
  int y = geometry_final_place_action_rows(geometry, x, top, w, menu_h, row_h, action_gap);
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

bool geometry_compute(int width, int height, GeometryMode mode,
                      AppGeometry *geometry) {
  if (!geometry_compute_base(width, height, mode, geometry)) return false;
  _Static_assert(PLAY_ACTION_COUNT == GEOMETRY_ACTION_COUNT,
                 "play-action geometry mismatch");

  if (width < 640) {
    if (!geometry_final_layout_portrait_play(width, height, geometry)) return false;
  } else if (!geometry_final_layout_desktop_play(width, height, geometry)) {
    return false;
  }

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



AudioControlGeometry geometry_audio_control(int width, int height,
                                            const AppGeometry *app,
                                            bool play_surface) {
  AudioControlGeometry out;
  memset(&out, 0, sizeof(out));
  if (width <= 0 || height <= 0) return out;

  if (play_surface && app) {
    out.button = app->actions[PLAY_ACTION_AUDIO];
  } else {
    int size = width < 420 ? 36 : 40;
    int margin = width < 420 ? 8 : 12;
    out.button = (GeoRect){width - margin - size, margin, size, size};
  }

  int popup_w = width < 260 ? width - 16 : 220;
  if (popup_w < 140) popup_w = 140;
  int popup_h = 92;
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
