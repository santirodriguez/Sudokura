/* Presentation geometry layered on top of the validated base geometry. */
static void ui_shift_rect_y(GeoRect *rect, int delta) {
  if (rect) rect->y += delta;
}

static int ui_rect_bottom(GeoRect rect) { return rect.y + rect.h; }

static void ui_grid(GeoRect *items, int start, int count, int x, int y,
                    int width, int height, int columns, int gap) {
  int cell_w = (width - (columns - 1) * gap) / columns;
  for (int i = 0; i < count; ++i) {
    items[start + i] =
        (GeoRect){x + (i % columns) * (cell_w + gap),
                  y + (i / columns) * (height + gap), cell_w, height};
  }
}

static int ui_place_action_rows(AppGeometry *geometry, int x, int y, int width,
                                int menu_h, int row_h, int gap) {
  geometry->actions[PLAY_ACTION_MENU] = (GeoRect){x, y, width, menu_h};
  y += menu_h + gap;
  ui_grid(geometry->actions, PLAY_ACTION_PAUSE, 2, x, y, width, row_h, 2, gap);
  y += row_h + gap;
  ui_grid(geometry->actions, PLAY_ACTION_HINT, 3, x, y, width, row_h, 3, gap);
  y += row_h + gap;
  ui_grid(geometry->actions, PLAY_ACTION_AUDIO, 2, x, y, width, row_h, 2, gap);
  return y + row_h;
}

static bool ui_play_rects_in_bounds(const AppGeometry *geometry, int width,
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

static bool ui_layout_portrait_play(int width, int height,
                                    AppGeometry *geometry) {
  int margin = 8;
  int shell_w = width - margin * 2;
  bool compact = height < 760;
  int section_gap = compact ? 4 : 6;
  int action_gap = compact ? 2 : 3;
  int action_h = compact ? 28 : 32;
  int menu_h = compact ? 28 : 34;
  int about_h = compact ? 28 : 32;
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
  int board_y = ui_rect_bottom(geometry->play_title) + 4;
  int reserve = section_gap + hud_h + section_gap + actions_h + section_gap +
                palette_label_h + 2 + palette_h + section_gap + progress_h +
                section_gap + about_h + margin;
  int board = height - board_y - reserve;
  if (board > shell_w) board = shell_w;
  board -= board % 9;
  if (board < 225) return false;
  geometry->board = (GeoRect){(width - board) / 2, board_y, board, board};

  int y = ui_rect_bottom(geometry->board) + section_gap;
  ui_grid(geometry->hud, 0, geometry->hud_count, margin, y, shell_w,
          hud_item_h, 3, hud_gap);
  y += hud_h + section_gap;
  y = ui_place_action_rows(geometry, margin, y, shell_w, menu_h, action_h,
                           action_gap);
  y += section_gap;
  geometry->palette_label = (GeoRect){margin, y, shell_w, palette_label_h};
  y += palette_label_h + 2;
  ui_grid(geometry->palette, 0, GEOMETRY_PALETTE_COUNT, margin, y, shell_w,
          palette_h, GEOMETRY_PALETTE_COUNT, 2);
  y += palette_h + section_gap;
  geometry->progress = (GeoRect){margin, y, shell_w, progress_h};
  int about_y = height - margin - about_h;
  if (ui_rect_bottom(geometry->progress) + section_gap > about_y) return false;
  geometry->actions[PLAY_ACTION_ABOUT] =
      (GeoRect){margin, about_y, shell_w, about_h};
  int sidebar_y = ui_rect_bottom(geometry->board) + 2;
  geometry->sidebar = (GeoRect){margin, sidebar_y, shell_w,
                                height - margin - sidebar_y};
  return ui_play_rects_in_bounds(geometry, width, height);
}

static bool ui_layout_desktop_play(int width, int height,
                                   AppGeometry *geometry) {
  int hud_bottom = ui_rect_bottom(geometry->play_title);
  for (int i = 0; i < geometry->hud_count; ++i)
    if (ui_rect_bottom(geometry->hud[i]) > hud_bottom)
      hud_bottom = ui_rect_bottom(geometry->hud[i]);

  int top = hud_bottom + 4;
  int bottom = ui_rect_bottom(geometry->sidebar);
  int remaining = bottom - top;
  bool xl = width >= 1600 && height >= 900;
  int section_gap, action_gap, menu_h, row_h, palette_label_h;
  int palette_item_h, palette_gap, progress_h, about_h;

  if (xl && remaining >= 540) {
    section_gap = 7;
    action_gap = 6;
    menu_h = 50;
    row_h = 48;
    palette_label_h = 22;
    palette_item_h = 44;
    palette_gap = 6;
    progress_h = 30;
    about_h = 42;
  } else if (remaining >= 390) {
    section_gap = 5;
    action_gap = 4;
    menu_h = 38;
    row_h = 36;
    palette_label_h = 18;
    palette_item_h = 30;
    palette_gap = 4;
    progress_h = 24;
    about_h = 34;
  } else {
    section_gap = 2;
    action_gap = 2;
    menu_h = 30;
    row_h = 28;
    palette_label_h = 12;
    palette_item_h = 22;
    palette_gap = 2;
    progress_h = 18;
    about_h = 26;
  }

  int x = geometry->sidebar.x;
  int w = geometry->sidebar.w;
  int y = ui_place_action_rows(geometry, x, top, w, menu_h, row_h, action_gap);
  y += section_gap;
  geometry->palette_label = (GeoRect){x, y, w, palette_label_h};
  y += palette_label_h + (section_gap > 2 ? 3 : 2);
  ui_grid(geometry->palette, 0, GEOMETRY_PALETTE_COUNT, x, y, w,
          palette_item_h, 3, palette_gap);
  int palette_h = palette_item_h * 3 + palette_gap * 2;
  y += palette_h + section_gap;
  geometry->progress = (GeoRect){x, y, w, progress_h};

  int about_y = bottom - about_h;
  if (ui_rect_bottom(geometry->progress) + section_gap > about_y) return false;
  geometry->actions[PLAY_ACTION_ABOUT] = (GeoRect){x, about_y, w, about_h};
  return ui_play_rects_in_bounds(geometry, width, height);
}

static bool ui_geometry_compute(int width, int height, GeometryMode mode,
                                AppGeometry *geometry) {
  if (!geometry_compute(width, height, mode, geometry)) return false;
  _Static_assert(PLAY_ACTION_COUNT == GEOMETRY_ACTION_COUNT,
                 "play-action geometry mismatch");

  if (width < 640) {
    if (!ui_layout_portrait_play(width, height, geometry)) return false;
  } else if (!ui_layout_desktop_play(width, height, geometry)) {
    return false;
  }

  if (width < 640) return true;

  int top = geometry->home_logo.y;
  int bottom = ui_rect_bottom(geometry->home_logo);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    if (ui_rect_bottom(geometry->home_mode[i]) > bottom)
      bottom = ui_rect_bottom(geometry->home_mode[i]);
    if (ui_rect_bottom(geometry->home_difficulty[i]) > bottom)
      bottom = ui_rect_bottom(geometry->home_difficulty[i]);
  }
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i)
    if (ui_rect_bottom(geometry->home_primary[i]) > bottom)
      bottom = ui_rect_bottom(geometry->home_primary[i]);
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    if (ui_rect_bottom(geometry->home_secondary[i]) > bottom)
      bottom = ui_rect_bottom(geometry->home_secondary[i]);

  int block_height = bottom - top;
  int available_top =
      geometry->screen_language.y + geometry->screen_language.h + 12;
  int available_bottom = height - 24;
  int spare = available_bottom - available_top - block_height;
  if (spare <= 0) return true;

  int target_top = available_top + spare * 2 / 5;
  int delta = target_top - top;
  if (delta <= 0) return true;

  ui_shift_rect_y(&geometry->home_logo, delta);
  ui_shift_rect_y(&geometry->home_mode_label, delta);
  ui_shift_rect_y(&geometry->home_difficulty_label, delta);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    ui_shift_rect_y(&geometry->home_mode[i], delta);
    ui_shift_rect_y(&geometry->home_difficulty[i], delta);
  }
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i)
    ui_shift_rect_y(&geometry->home_primary[i], delta);
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i)
    ui_shift_rect_y(&geometry->home_secondary[i], delta);

  return true;
}
