/* Shared presentation for the remaining screens. All actions, enabled states,
   modal bindings and hit rectangles still belong to the existing owners. */
#include "sudokura_sdl/ui_action_icons.h"

static void rollout_button(Gfx *gfx, UiDesignTheme theme, SDL_Rect rect,
    const char *label, UiDesignIcon icon, UiActionIcon action, UiDesignRole role,
    bool focused, bool disabled, int preferred) {
  if (rect.w <= 0 || rect.h <= 0) return;
  UiDesignButton style;
  SDL_Rect face = design_button_face(gfx, theme, rect, role,
                                     focused, disabled, &style);
  bool has_icon = icon != UI_DESIGN_ICON_NONE || action != UI_ACTION_NONE;
  UiDesignLabel boxes = ui_action_label_layout(face.w, face.h, has_icon);
  if (!label && has_icon) {
    int side = face.h-12;
    if (side > 26) side=26;
    boxes.icon=(UiDesignRect){(face.w-side)/2,(face.h-side)/2,side,side};
  }
  if (boxes.icon.w > 0) {
    SDL_Rect box={face.x+boxes.icon.x,face.y+boxes.icon.y,
                   boxes.icon.w,boxes.icon.h};
    if (action != UI_ACTION_NONE) ui_action_icon(gfx->ren,box,action,style.text);
    else ui_design_icon(gfx->ren,box,icon,style.text);
  }
  if (label) {
    SDL_Rect text={face.x+boxes.text.x,face.y+boxes.text.y,
                   boxes.text.w,boxes.text.h};
    int minimum=gfx->mobile_layout ? 13 : 10;
    if (!draw_fitted_text(gfx,label,ui_design_sdl_color(style.text),
                          text,preferred,minimum))
      (void)design_fit(gfx,label,style.text,text,preferred,minimum,true);
  }
}

/* Refresh the old board painter's control surfaces before applying the new
   faces. This avoids leaving the previous corner pixels outside a new radius. */
static void rollout_clear_play_control(Gfx *gfx, const AppGeometry *geo,
                                        UiDesignTheme theme, SDL_Rect rect) {
  const GeoRect side=geo->sidebar;
  bool inside=rect.x>=side.x && rect.y>=side.y &&
      rect.x+rect.w<=side.x+side.w && rect.y+rect.h<=side.y+side.h;
  UiDesignColor color=inside ? theme.surface_top : theme.background_bottom;
  draw_rect(gfx->ren,rect.x,rect.y,rect.w,rect.h,ui_design_sdl_color(color));
}

static void rollout_play(Gfx *gfx, const Game *game, UI *ui) {
  if (ui->screen != SCR_PLAY || app_pause_hides_play(ui)) return;
  AppGeometry geo;
  if (!get_geometry(gfx,ui,&geo)) return;
  UiDesignTheme theme=ui_design_theme(ui->dark_theme);
  GeometryFonts fonts=geometry_font_sizes(&geo,gfx->width,gfx->height);
  GeoRect brand=geometry_play_wordmark_rect(&geo,gfx->width);
  if (brand.w>0 && brand.h>0) draw_wordmark(gfx,sdl_rect(brand));
  SDL_Rect frame={geo.board.x-5,geo.board.y-5,geo.board.w+10,geo.board.h+10};
  draw_focus_ring(gfx->ren,frame,ui_design_sdl_color(theme.border));
  for (int number=1;number<=GEOMETRY_PALETTE_COUNT;++number) {
    SDL_Rect rect=sdl_rect(geo.palette[number-1]);
    bool complete=palette_number_complete(game,ui->mode,number);
    bool selected=game->puzzle[IDX(ui->sel_r,ui->sel_c)]==number;
    char label[2]={(char)('0'+number),0};
    rollout_clear_play_control(gfx,&geo,theme,rect);
    rollout_button(gfx,theme,rect,label,UI_DESIGN_ICON_NONE,UI_ACTION_NONE,
        selected&&!complete ? UI_DESIGN_PRIMARY : UI_DESIGN_SECONDARY,
        design_focus_visible(gfx)&&play_keyboard_focus==PLAY_FOCUS_NUMBER_1+number-1,
        complete,fonts.control+4);
    if (complete) ui_action_icon(gfx->ren,
        (SDL_Rect){rect.x+rect.w-16,rect.y+rect.h-15,12,12},
        UI_ACTION_VERIFY,theme.disabled);
  }
  static const PlayControl controls[]={
    PLAY_CONTROL_MENU,PLAY_CONTROL_PAUSE,PLAY_CONTROL_SAVE,PLAY_CONTROL_MORE,
    PLAY_CONTROL_UNDO,PLAY_CONTROL_REDO,PLAY_CONTROL_CLEAR,PLAY_CONTROL_NOTES,
    PLAY_CONTROL_RESTART,PLAY_CONTROL_HINT,PLAY_CONTROL_VERIFY,
    PLAY_CONTROL_SETTINGS,PLAY_CONTROL_HELP};
  static const int focus[]={
    PLAY_FOCUS_MENU,PLAY_FOCUS_PAUSE,PLAY_FOCUS_SAVE,PLAY_FOCUS_MORE,
    PLAY_FOCUS_UNDO,PLAY_FOCUS_REDO,PLAY_FOCUS_CLEAR,PLAY_FOCUS_NOTES,
    PLAY_FOCUS_RESTART,PLAY_FOCUS_HINT,PLAY_FOCUS_VERIFY,
    PLAY_FOCUS_SETTINGS,PLAY_FOCUS_HELP};
  static const TextKey labels[]={T_MENU,T_PAUSE,T_SAVE,T_MORE,T_UNDO,T_REDO,
      T_CLEAR,T_NOTES,T_RESTART,T_HINT,T_VERIFY,T_SETTINGS,T_HELP};
  static const UiActionIcon actions[]={UI_ACTION_NONE,UI_ACTION_PAUSE,
      UI_ACTION_SAVE,UI_ACTION_MORE,UI_ACTION_UNDO,UI_ACTION_REDO,
      UI_ACTION_CLEAR,UI_ACTION_NOTES,UI_ACTION_RESTART,UI_ACTION_HINT,
      UI_ACTION_VERIFY,UI_ACTION_NONE,UI_ACTION_NONE};
  _Static_assert(sizeof(controls)/sizeof(controls[0])==sizeof(focus)/sizeof(focus[0]),
                 "Every painted play control needs the canonical focus key");
  for (unsigned i=0;i<sizeof(controls)/sizeof(controls[0]);++i) {
    if ((geo.play_compact&&i>=8) || (!geo.play_compact&&controls[i]==PLAY_CONTROL_MORE))
      continue;
    SDL_Rect rect=sdl_rect(geo.play_controls[controls[i]]);
    if (rect.w<=0 || rect.h<=0) continue;
    bool enabled=play_focus_enabled(game,ui,focus[i]);
    bool active=controls[i]==PLAY_CONTROL_NOTES&&ui->notes_mode;
    UiDesignRole role=active||controls[i]==PLAY_CONTROL_SAVE
        ? UI_DESIGN_PRIMARY : UI_DESIGN_SECONDARY;
    UiDesignIcon icon=controls[i]==PLAY_CONTROL_MENU ? UI_DESIGN_ICON_BACK :
        controls[i]==PLAY_CONTROL_SETTINGS ? UI_DESIGN_ICON_SETTINGS :
        controls[i]==PLAY_CONTROL_HELP ? UI_DESIGN_ICON_HELP : UI_DESIGN_ICON_NONE;
    const char *label=tr(ui->language,gfx->mobile_layout&&i==0 ? T_HOME_COMPACT : labels[i]);
    /* The existing short-wide toolbar already uses direction symbols here.
       Keep its accessible labels and replace those symbols with vector icons. */
    if (gfx->mobile_layout&&gfx->height<600&&
        (controls[i]==PLAY_CONTROL_UNDO||controls[i]==PLAY_CONTROL_REDO)) label=NULL;
    rollout_clear_play_control(gfx,&geo,theme,rect);
    rollout_button(gfx,theme,rect,label,icon,actions[i],role,
        design_focus_visible(gfx)&&play_keyboard_focus==focus[i],!enabled,fonts.control);
  }
  if (design_focus_visible(gfx)&&play_keyboard_focus==PLAY_FOCUS_STRICT&&
      play_focus_enabled(game,ui,PLAY_FOCUS_STRICT))
    draw_focus_ring(gfx->ren,play_focus_rect(gfx,ui,PLAY_FOCUS_STRICT),
                          ui_design_sdl_color(theme.focus));
}

static void rollout_slider(Gfx *gfx, UI *ui, UiDesignTheme theme,
    SDL_Rect label_rect, SDL_Rect minus, SDL_Rect track, SDL_Rect plus,
    TextKey key, int value, bool available, bool focused, int preferred) {
  char text[128];
  snprintf(text,sizeof(text),"%s  %d%%%s%s",tr(ui->language,key),value,
      available ? "" : " / ",available ? "" : tr(ui->language,T_UNAVAILABLE));
  (void)draw_fitted_text(gfx,text,ui_design_sdl_color(theme.text),label_rect,
                         preferred,gfx->mobile_layout ? 13 : 10);
  rollout_button(gfx,theme,minus,"-",UI_DESIGN_ICON_NONE,UI_ACTION_NONE,
                  UI_DESIGN_QUIET,false,false,preferred+2);
  rollout_button(gfx,theme,plus,"+",UI_DESIGN_ICON_NONE,UI_ACTION_NONE,
                  UI_DESIGN_QUIET,false,false,preferred+2);
  if (value<0) value=0;
  if (value>100) value=100;
  SDL_Rect rail={track.x+6,track.y+track.h/2-3,track.w-12,6};
  ui_design_round(gfx->ren,(SDL_FRect){(float)rail.x,(float)rail.y,
      (float)rail.w,(float)rail.h},3,theme.border,theme.border);
  int filled=rail.w*value/100;
  if (filled>0) ui_design_round(gfx->ren,(SDL_FRect){(float)rail.x,(float)rail.y,
      (float)filled,(float)rail.h},3,theme.primary_top,theme.primary);
  SDL_FRect knob={(float)(rail.x+filled-6),(float)(track.y+track.h/2-10),12,20};
  ui_design_round(gfx->ren,knob,6,theme.accent,theme.accent);
  if (focused) {
    SDL_Rect group={minus.x,minus.y,plus.x+plus.w-minus.x,minus.h};
    draw_focus_ring(gfx->ren,group,ui_design_sdl_color(theme.focus));
  }
}

static void rollout_settings(Gfx *gfx, UI *ui) {
  if (!settings_open) return;
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  SDL_Color veil=ui_design_sdl_color(t.background_bottom);veil.a=240;
  draw_rect(gfx->ren,0,0,gfx->width,gfx->height,veil);
  SettingsPanelLayout l=ui_settings_panel_layout(gfx);
  design_card(gfx,t,l.card);
  bool focus=design_focus_visible(gfx);
  AppGeometry geometry;
  int preferred=18;
  if (get_geometry(gfx,ui,&geometry))
    preferred=geometry_font_sizes(&geometry,gfx->width,gfx->height).control;
  (void)draw_fitted_text(gfx,tr(ui->language,T_SETTINGS),ui_design_sdl_color(t.text),
                         l.title,preferred+7,13);
  char text[128];
  snprintf(text,sizeof(text),"%s: %s",tr(ui->language,T_THEME),
           tr(ui->language,ui->dark_theme ? T_DARK : T_LIGHT));
  rollout_button(gfx,t,l.theme,text,UI_DESIGN_ICON_NONE,UI_ACTION_NONE,
      UI_DESIGN_SECONDARY,focus&&settings_keyboard_focus==SETTINGS_FOCUS_THEME,false,preferred);
  snprintf(text,sizeof(text),"%s: %s",tr(ui->language,T_LANGUAGE),language_name(ui->language));
  rollout_button(gfx,t,l.language,text,UI_DESIGN_ICON_GLOBE,UI_ACTION_NONE,
      UI_DESIGN_SECONDARY,focus&&settings_keyboard_focus==SETTINGS_FOCUS_LANGUAGE,false,preferred);
  bool enabled=audio_is_enabled(),available=audio_is_available();
  snprintf(text,sizeof(text),"%s: %s%s%s",tr(ui->language,T_SOUND),
      tr(ui->language,enabled ? T_ENABLED : T_MUTED),
      enabled&&!available ? " / " : "",
      enabled&&!available ? tr(ui->language,T_UNAVAILABLE) : "");
  rollout_button(gfx,t,l.sound,text,UI_DESIGN_ICON_SOUND,UI_ACTION_NONE,
      enabled&&available ? UI_DESIGN_PRIMARY : UI_DESIGN_SECONDARY,
      focus&&settings_keyboard_focus==SETTINGS_FOCUS_SOUND,false,preferred);
  rollout_slider(gfx,ui,t,l.music_label,l.music_minus,l.music_track,l.music_plus,
      T_MUSIC,audio_music_volume(),audio_music_available(),
      focus&&settings_keyboard_focus==SETTINGS_FOCUS_MUSIC,preferred);
  rollout_slider(gfx,ui,t,l.fx_label,l.fx_minus,l.fx_track,l.fx_plus,
      T_FX,audio_fx_volume(),audio_fx_available(),
      focus&&settings_keyboard_focus==SETTINGS_FOCUS_FX,preferred);
  snprintf(text,sizeof(text),"%s: %s",tr(ui->language,T_REDUCED_MOTION),
      tr(ui->language,ui->reduced_motion ? T_ENABLED : T_DISABLED));
  rollout_button(gfx,t,l.reduced_motion,text,UI_DESIGN_ICON_NONE,UI_ACTION_NONE,
      ui->reduced_motion ? UI_DESIGN_PRIMARY : UI_DESIGN_SECONDARY,
      focus&&settings_keyboard_focus==SETTINGS_FOCUS_REDUCED_MOTION,false,preferred);
  snprintf(text,sizeof(text),"%s: %s",tr(ui->language,T_AUTO_NOTES),
      tr(ui->language,ui->auto_remove_peer_notes ? T_ENABLED : T_DISABLED));
  rollout_button(gfx,t,l.auto_notes,text,UI_DESIGN_ICON_NONE,UI_ACTION_NOTES,
      ui->auto_remove_peer_notes ? UI_DESIGN_PRIMARY : UI_DESIGN_SECONDARY,
      focus&&settings_keyboard_focus==SETTINGS_FOCUS_AUTO_NOTES,false,preferred);
  rollout_button(gfx,t,l.back,tr(ui->language,T_BACK),UI_DESIGN_ICON_BACK,UI_ACTION_NONE,
      UI_DESIGN_PRIMARY,focus&&settings_keyboard_focus==SETTINGS_FOCUS_BACK,false,preferred);
}

static void rollout_hint(Gfx *gfx, UI *ui) {
  if (!hint_panel_interactive(ui)) return;
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  HintPanelLayout l=hint_panel_layout(gfx,ui);
  design_card(gfx,t,l.card);
  (void)draw_fitted_text(gfx,tr(ui->language,T_HINT_PREVIEW),ui_design_sdl_color(t.accent),
                         l.title,20,gfx->mobile_layout ? 16 : 10);
  char message[320];hint_preview_text(ui,message,sizeof(message));
  if (l.body.h>8) (void)design_fit(gfx,message,t.text,l.body,15,
                                   gfx->mobile_layout ? 13 : 10,false);
  bool focus=design_focus_visible(gfx);
  if (ui->hint_preview.status==HUMAN_HINT_LOGICAL)
    rollout_button(gfx,t,l.primary,tr(ui->language,T_APPLY),UI_DESIGN_ICON_NONE,UI_ACTION_VERIFY,
        UI_DESIGN_PRIMARY,focus&&hint_keyboard_focus==HINT_FOCUS_PRIMARY,false,16);
  if (ui->hint_preview.status==HUMAN_HINT_INVALID||ui->hint_preview.status==HUMAN_HINT_STALLED)
    rollout_button(gfx,t,l.verify,tr(ui->language,T_VERIFY),UI_DESIGN_ICON_NONE,UI_ACTION_VERIFY,
        UI_DESIGN_PRIMARY,focus&&hint_keyboard_focus==HINT_FOCUS_PRIMARY,false,16);
  if (ui->hint_preview.status!=HUMAN_HINT_INVALID)
    rollout_button(gfx,t,l.reveal,tr(ui->language,T_REVEAL_CELL),UI_DESIGN_ICON_NONE,UI_ACTION_EYE,
        UI_DESIGN_SECONDARY,focus&&hint_keyboard_focus==HINT_FOCUS_REVEAL,false,16);
  rollout_button(gfx,t,l.cancel,tr(ui->language,T_CANCEL),UI_DESIGN_ICON_BACK,UI_ACTION_NONE,
      UI_DESIGN_QUIET,focus&&hint_keyboard_focus==HINT_FOCUS_CANCEL,false,16);
}

static void rollout_more(Gfx *gfx, UI *ui) {
  if (!ui_more_open || ui->screen!=SCR_PLAY) return;
  AppGeometry geo;
  if (!get_geometry(gfx,ui,&geo)||!geo.play_compact) return;
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  /* An opaque background is required: paused board contents stay concealed. */
  design_background(gfx,t);
  SDL_Rect panel=sdl_rect(geo.more_panel);
  design_card(gfx,t,panel);
  (void)draw_fitted_text(gfx,tr(ui->language,T_MORE),ui_design_sdl_color(t.text),
      (SDL_Rect){panel.x+16,panel.y+12,panel.w-80,34},24,13);
  SDL_Rect close={panel.x+panel.w-48,panel.y+16,24,24};
  ui_action_icon(gfx->ren,close,UI_ACTION_CLOSE,t.muted);
  const TextKey labels[]={T_RESTART,T_HINT,T_VERIFY,T_SETTINGS,T_HELP,T_ABOUT};
  const UiActionIcon action[]={UI_ACTION_RESTART,UI_ACTION_HINT,UI_ACTION_VERIFY,
                                UI_ACTION_NONE,UI_ACTION_NONE,UI_ACTION_NONE};
  const UiDesignIcon icons[]={UI_DESIGN_ICON_NONE,UI_DESIGN_ICON_NONE,UI_DESIGN_ICON_NONE,
      UI_DESIGN_ICON_SETTINGS,UI_DESIGN_ICON_HELP,UI_DESIGN_ICON_INFO};
  for (int i=0;i<GEOMETRY_MORE_COUNT;++i)
    rollout_button(gfx,t,sdl_rect(geo.more_items[i]),tr(ui->language,labels[i]),icons[i],action[i],
        UI_DESIGN_SECONDARY,design_focus_visible(gfx)&&ui_more_focus==i,false,17);
}

static void rollout_navigation(Gfx *gfx, UI *ui) {
  AppGeometry geo;
  if (!get_geometry(gfx,ui,&geo)) return;
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  GeometryFonts fonts=geometry_font_sizes(&geo,gfx->width,gfx->height);
  bool focus=design_focus_visible(gfx);
  if (ui->screen==SCR_HELP) {
    SDL_Rect back=back_rect(gfx,ui);
    draw_rect(gfx->ren,back.x,back.y,back.w,back.h,ui_design_sdl_color(t.background_bottom));
    rollout_button(gfx,t,back,tr(ui->language,T_BACK),UI_DESIGN_ICON_BACK,UI_ACTION_NONE,
        UI_DESIGN_PRIMARY,focus&&info_keyboard_focus==INFO_FOCUS_BACK,false,fonts.control);
    if (gfx->mobile_layout&&help_scroll_max_px>0) {
      for (int i=0;i<2;++i) {
        bool enabled=i ? help_scroll_px<help_scroll_max_px : help_scroll_px>0;
        rollout_button(gfx,t,ui_info_scroll_rect(gfx,ui,i!=0),NULL,
            i ? UI_DESIGN_ICON_DOWN : UI_DESIGN_ICON_UP,UI_ACTION_NONE,
            UI_DESIGN_QUIET,false,!enabled,18);
      }
    }
  } else if (ui->screen==SCR_PLAY&&app_pause_hides_play(ui)) {
    rollout_button(gfx,t,sdl_rect(geo.pause_buttons[0]),tr(ui->language,T_RESUME),
        UI_DESIGN_ICON_PLAY,UI_ACTION_NONE,UI_DESIGN_PRIMARY,
        focus&&pause_keyboard_focus==0,false,fonts.control+2);
    rollout_button(gfx,t,sdl_rect(geo.pause_buttons[1]),tr(ui->language,T_MENU),
        UI_DESIGN_ICON_BACK,UI_ACTION_NONE,UI_DESIGN_SECONDARY,
        focus&&pause_keyboard_focus==1,false,fonts.control);
  } else if (ui->screen==SCR_END) {
    SDL_Rect heading=sdl_rect(geo.end_heading);
    draw_rect(gfx->ren,heading.x,heading.y,heading.w,heading.h,
               ui_design_sdl_color(t.background_bottom));
    (void)draw_fitted_text(gfx,tr(ui->language,ui->result==RES_WIN ? T_YOU_WIN : T_YOU_LOSE),
        ui_design_sdl_color(ui->result==RES_WIN ? t.celebration : t.danger),
        heading,fonts.heading,16);
    TextKey primary=ui->result==RES_WIN
        ? (ui->is_daily ? T_ANOTHER_NORMAL : T_ANOTHER_GAME) : T_RETRY_SAME;
    rollout_button(gfx,t,sdl_rect(geo.end_buttons[0]),tr(ui->language,primary),
        UI_DESIGN_ICON_PLAY,UI_ACTION_NONE,UI_DESIGN_PRIMARY,
        focus&&result_keyboard_focus==0,false,fonts.control+2);
    rollout_button(gfx,t,sdl_rect(geo.end_buttons[1]),tr(ui->language,T_HOME),
        UI_DESIGN_ICON_BACK,UI_ACTION_NONE,UI_DESIGN_SECONDARY,
        focus&&result_keyboard_focus==1,false,fonts.control);
  }
}

static void rollout_modal_controls(Gfx *gfx, UI *ui) {
  if (!ui_modal.active) return;
  UiModalLayout l=ui_modal_layout(gfx,ui_modal.request.button_count);
  if (!l.panel.w) return;
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  for (unsigned i=0;i<ui_modal.request.button_count;++i) {
    bool destructive=(ui_modal.request.destructive_buttons&(1u<<i))!=0;
    rollout_button(gfx,t,l.buttons[i],tr(ui->language,ui_modal.request.buttons[i]),
        UI_DESIGN_ICON_NONE,UI_ACTION_NONE,
        destructive ? UI_DESIGN_DESTRUCTIVE : i==1 ? UI_DESIGN_PRIMARY : UI_DESIGN_SECONDARY,
        design_focus_visible(gfx)&&ui_modal.focus==i,false,18);
  }
  if (ui_modal_scroll_max_px>0) for (int i=0;i<2;++i) {
    bool enabled=i ? ui_modal_scroll_px<ui_modal_scroll_max_px : ui_modal_scroll_px>0;
    rollout_button(gfx,t,i ? l.scroll_down : l.scroll_up,NULL,
        i ? UI_DESIGN_ICON_DOWN : UI_DESIGN_ICON_UP,UI_ACTION_NONE,
        UI_DESIGN_QUIET,false,!enabled,18);
  }
}

/* Library rows/details retain their existing data and layout owners. Only the
   shared glyph and label painters change, including empty/error states. */
static void ui_draw_glyph(SDL_Renderer *renderer, SDL_Rect box,
                                   UiGlyph glyph, SDL_Color color) {
  UiDesignColor c=ui_design_color(color.r,color.g,color.b,color.a);
  UiDesignIcon system=UI_DESIGN_ICON_NONE;
  UiActionIcon action=UI_ACTION_NONE;
  switch (glyph) {
    case UI_GLYPH_FOLDER: system=UI_DESIGN_ICON_FOLDER;break;
    case UI_GLYPH_SAVE: action=UI_ACTION_SAVE;break;
    case UI_GLYPH_PLAY: system=UI_DESIGN_ICON_PLAY;break;
    case UI_GLYPH_BACK: system=UI_DESIGN_ICON_BACK;break;
    case UI_GLYPH_REPLAY: action=UI_ACTION_RESTART;break;
    case UI_GLYPH_TRASH: action=UI_ACTION_TRASH;break;
    case UI_GLYPH_EYE: action=UI_ACTION_EYE;break;
    case UI_GLYPH_TROPHY: system=UI_DESIGN_ICON_TROPHY;break;
    case UI_GLYPH_REPO: system=UI_DESIGN_ICON_REPOSITORY;break;
    case UI_GLYPH_GLOBE: system=UI_DESIGN_ICON_GLOBE;break;
    case UI_GLYPH_HEART: system=UI_DESIGN_ICON_HEART;break;
    case UI_GLYPH_SHIELD: system=UI_DESIGN_ICON_SHIELD;break;
  }
  if (action!=UI_ACTION_NONE) ui_action_icon(renderer,box,action,c);
  else ui_design_icon(renderer,box,system,c);
}

static void ui_draw_icon_label(Gfx *gfx, SDL_Rect face, const char *text,
                                   UiGlyph glyph, SDL_Color color, int preferred) {
  UiDesignLabel boxes=ui_action_label_layout(face.w,face.h,true);
  if (boxes.icon.w>0)
    ui_draw_glyph(gfx->ren,(SDL_Rect){face.x+boxes.icon.x,face.y+boxes.icon.y,
        boxes.icon.w,boxes.icon.h},glyph,color);
  SDL_Rect label={face.x+boxes.text.x,face.y+boxes.text.y,boxes.text.w,boxes.text.h};
  int minimum=gfx->mobile_layout ? 13 : 10;
  if (!draw_fitted_text(gfx,text,color,label,preferred,minimum))
    (void)polish_draw_wrapped_fit(gfx,text,color,label,preferred,minimum,true);
}

/* One external render entry point, preserving overlay ordering. The accepted
   Home/About painters are reused, not duplicated or re-laid out. */
static void app_render_ui(Gfx *gfx, const Game *game, UI *ui) {
  if (!gfx || !game || !ui) return;
  if (ui->screen==SCR_TITLE || ui->screen==SCR_ABOUT) {
    app_render_home_about(gfx,game,ui);
    rollout_modal_controls(gfx,ui);
    return;
  }
  if (ui->screen!=SCR_PLAY && ui->screen!=SCR_HELP && ui->screen!=SCR_END &&
      ui->screen!=APP_SCREEN_GAMES && ui->screen!=APP_SCREEN_SETTINGS) {
    app_render_home_about(gfx,game,ui);
    return;
  }
  ui_feedback_render_screen=(int)ui->screen;
  text_cache_sync(gfx,ui);
  app_render_base(gfx,game,ui);
  if (ui->screen==SCR_PLAY) rollout_play(gfx,game,ui);
  else if (ui->screen==APP_SCREEN_GAMES) games_browser_render(gfx,game,ui);
  rollout_navigation(gfx,ui);
  rollout_hint(gfx,ui);
  rollout_settings(gfx,ui);
  rc2_render_storage_banner(gfx,ui);
  rc2_render_generation_overlay(gfx,ui);
  design_audio(gfx,ui);
  rollout_more(gfx,ui);
  ui_modal_render(gfx,ui);
  rollout_modal_controls(gfx,ui);
}
