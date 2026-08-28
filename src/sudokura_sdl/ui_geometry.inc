/* Presentation-only adjustments layered on top of the validated geometry. */
static void ui_shift_rect_y(GeoRect *rect, int delta) {
  if (rect) rect->y += delta;
}

static int ui_rect_bottom(GeoRect rect) { return rect.y + rect.h; }

static bool ui_geometry_compute(int width, int height, GeometryMode mode,
                                AppGeometry *geometry) {
  if (!geometry_compute(width, height, mode, geometry)) return false;
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
