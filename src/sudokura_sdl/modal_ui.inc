/* One SDL-owner modal surface; no separate event pump, storage or thread. */
static void ui_pointer_cancel(void);
static void ui_touch_cancel(void);
static bool games_browser_modal_binding(Persistence *p, UI *ui,
                                        AppModalBinding *out);
static AppModalBinding home_daily_recovery_modal_binding(
    const Persistence *p, const UI *ui);
static AppModal ui_modal;
static AppModalResult ui_modal_pending;
static uint64_t ui_modal_generation = 1;
static int ui_modal_pressed = -1;
static bool ui_modal_mouse_active;
static bool ui_modal_touch_active;
static SDL_TouchID ui_modal_touch_id;
static SDL_FingerID ui_modal_finger_id;
static int ui_modal_scroll_px, ui_modal_scroll_max_px;

typedef struct {
  SDL_Rect panel, title, body, buttons[3];
} UiModalLayout;

static AppModalBinding ui_modal_current_binding(const Persistence *p,
                                                 const UI *ui) {
  AppModalBinding binding = {0};
  if (!ui) return binding;
  binding.screen = ui->screen;
  binding.generation = ui_modal_generation;
  if (p) {
    binding.key_hi = p->active_attempt_id.high;
    binding.key_lo = p->active_attempt_id.low;
    const ProfileSlot *slot = NULL;
    if (p->profile.normal.present &&
        game_attempt_id_equal(p->profile.normal.attempt_id,
                              p->active_attempt_id))
      slot = &p->profile.normal;
    if (p->profile.daily.present &&
        game_attempt_id_equal(p->profile.daily.attempt_id,
                              p->active_attempt_id))
      slot = &p->profile.daily;
    if (slot) binding.revision = slot->revision;
  }
  return binding;
}

static bool ui_modal_has_pending(void) {
  return ui_modal_pending.resolution != APP_MODAL_NO_RESULT;
}

static bool ui_modal_take_result(AppModalResult *result) {
  if (!result || !ui_modal_has_pending()) return false;
  *result = ui_modal_pending;
  ui_modal_pending = (AppModalResult){0};
  return true;
}

static bool ui_modal_open_request(Gfx *gfx, UI *ui,
                                   const AppModalRequest *request) {
  if (!gfx || !ui || ui_modal_has_pending() ||
      !app_modal_begin(&ui_modal, ui, request, now_ms()))
    return false;
  ui_pointer_cancel();
  ui_touch_cancel();
  ui_modal_pressed = -1;
  ui_modal_mouse_active = false;
  ui_modal_touch_active = false;
  ui_modal_scroll_px = ui_modal_scroll_max_px = 0;
  ui_audio_control_cancel_interaction();
  gfx->dirty = true;
  return true;
}

/* Informational errors use the same event-driven surface without a
   continuation or a native blocking message box. */
static bool ui_modal_message(Gfx *gfx, UI *ui, TextKey title, TextKey body) {
  if (!gfx || !ui) return false;
  AppModalRequest request = {
      .kind = APP_MODAL_MESSAGE, .action = APP_MODAL_ACTION_NONE,
      .binding = ui_modal_current_binding(audio_persistence, ui),
      .title = title, .body = body,
      .buttons = {T_BACK}, .button_count = 1};
  return ui_modal_open_request(gfx, ui, &request);
}

static void ui_modal_cancel_current(void) {
  if (!ui_modal.active) return;
  (void)app_modal_cancel(&ui_modal, ui_modal.serial, now_ms());
  ui_modal_pressed = -1;
  ui_modal_mouse_active = false;
  ui_modal_touch_active = false;
  if (audio_gfx) audio_gfx->dirty = true;
}

/* A rotation, restore, screen transition or lifecycle interruption cancels
   outstanding intent; an already queued choice cannot outlive its binding. */
static void ui_modal_invalidate(void) {
  ui_modal_generation = ui_modal_generation == UINT64_MAX ? 1 :
                        ui_modal_generation + 1;
  ui_modal_pending = (AppModalResult){0};
  ui_modal_cancel_current();
}

static UiModalLayout ui_modal_layout(const Gfx *gfx, unsigned count) {
  UiModalLayout layout = {0};
  if (!gfx || count == 0 || count > 3) return layout;
  int width = gfx->width, height = gfx->height;
  int panel_w = width - 24;
  if (panel_w > 540) panel_w = 540;
  int panel_h = height - 24;
  if (panel_h > 302) panel_h = 302;
  if (panel_w < 240 || panel_h < 180) return layout;
  layout.panel = (SDL_Rect){(width-panel_w)/2,(height-panel_h)/2,
                             panel_w,panel_h};
  layout.title = (SDL_Rect){layout.panel.x+16,layout.panel.y+12,
                             panel_w-32,36};
  int buttons_y = layout.panel.y + panel_h - 65;
  layout.body = (SDL_Rect){layout.panel.x+18,layout.panel.y+54,
                            panel_w-36,buttons_y-layout.panel.y-66};
  int gap = 6, button_w = (panel_w-32-(int)(count-1)*gap)/(int)count;
  for (unsigned i = 0; i < count; ++i)
    layout.buttons[i] = (SDL_Rect){
        layout.panel.x+16+(int)i*(button_w+gap),buttons_y,button_w,50};
  return layout;
}

static void ui_modal_render(Gfx *gfx, UI *ui) {
  if (!ui_modal.active || !gfx || !gfx->ren || !ui) return;
  UiModalLayout layout = ui_modal_layout(gfx,ui_modal.request.button_count);
  if (!layout.panel.w) return;
  Theme theme = ui->dark_theme ? theme_dark() : theme_light();
  SDL_BlendMode previous_blend = SDL_BLENDMODE_NONE;
  (void)SDL_GetRenderDrawBlendMode(gfx->ren,&previous_blend);
  (void)SDL_SetRenderDrawBlendMode(gfx->ren,SDL_BLENDMODE_BLEND);
  draw_rect(gfx->ren,0,0,gfx->width,gfx->height,
            (SDL_Color){0,0,0,170});
  (void)SDL_SetRenderDrawBlendMode(gfx->ren,previous_blend);
  draw_round_rect(gfx->ren,layout.panel,12,theme.bg);
  draw_round_rect(gfx->ren,layout.title,5,theme.btn);
  (void)draw_fitted_text(gfx,
      tr(ui->language,ui_modal.request.title),theme.title,layout.title,25,14);

  const char *body_text=tr(ui->language,ui_modal.request.body);
  char dated_body[512];
  if (ui_modal.request.action==APP_MODAL_DAILY && audio_persistence) {
    const SessionState *older=
        profile_slot_session(&audio_persistence->profile.daily);
    if (older && older->is_daily) {
      (void)snprintf(dated_body,sizeof(dated_body),"%04d-%02d-%02d\n%s",
                     older->daily_year,older->daily_month,older->daily_day,
                     body_text);
      body_text=dated_body;
    }
  }
  int text_w=0,text_h=0;
  SDL_Texture *body=render_text_wrapped(
      gfx,gfx->font_body,body_text,
      theme.btnfg,layout.body.w,&text_w,&text_h);
  ui_modal_scroll_max_px = text_h > layout.body.h ?
                           text_h-layout.body.h : 0;
  if (ui_modal_scroll_px > ui_modal_scroll_max_px)
    ui_modal_scroll_px = ui_modal_scroll_max_px;
  SDL_Rect old_clip={0};
  SDL_RenderGetClipRect(gfx->ren,&old_clip);
  SDL_RenderSetClipRect(gfx->ren,&layout.body);
  if (body) {
    SDL_Rect dest={layout.body.x,layout.body.y-ui_modal_scroll_px,
                   text_w,text_h};
    (void)SDL_RenderCopy(gfx->ren,body,NULL,&dest);
  }
  SDL_RenderSetClipRect(gfx->ren,
      old_clip.w || old_clip.h ? &old_clip : NULL);
  for (unsigned i=0;i<ui_modal.request.button_count;++i) {
    SDL_Rect rect=layout.buttons[i];
    bool focused=ui_modal.focus==i;
    draw_round_rect(gfx->ren,rect,7,focused?theme.title:theme.btn);
    (void)draw_fitted_text(gfx,tr(ui->language,ui_modal.request.buttons[i]),
        focused?theme.bg:theme.btnfg,rect,19,12);
    if (focused) draw_focus_ring(gfx->ren,rect,theme.sel_outline);
  }
}

static int ui_modal_button_at(const Gfx *gfx, int x, int y) {
  UiModalLayout layout=ui_modal_layout(gfx,ui_modal.request.button_count);
  for(unsigned i=0;i<ui_modal.request.button_count;++i)
    if (point_in(layout.buttons[i],x,y)) return (int)i;
  return -1;
}

/* New is requested from Home or the result screen: neither guarantees
   active_attempt_id names the departing normal hot slot. Bind to that slot. */
static AppModalBinding ui_modal_new_game_binding(const Persistence *p,
                                                  const UI *ui) {
  AppModalBinding binding = ui_modal_current_binding(p, ui);
  binding.key_hi = binding.key_lo = binding.revision = 0;
  if (p && p->profile.normal.present) {
    const ProfileSlot *slot = &p->profile.normal;
    binding.key_hi = slot->attempt_id.high;
    binding.key_lo = slot->attempt_id.low;
    binding.revision = slot->revision;
  }
  return binding;
}

/* A Home Daily decision binds the departing Daily slot, not the active game. */
static AppModalBinding ui_modal_daily_binding(const Persistence *p,
                                               const UI *ui) {
  AppModalBinding binding=ui_modal_current_binding(p,ui);
  binding.key_hi=binding.key_lo=binding.revision=0;
  if (p && p->profile.daily.present) {
    const ProfileSlot *slot=&p->profile.daily;
    binding.key_hi=slot->attempt_id.high;
    binding.key_lo=slot->attempt_id.low;
    binding.revision=slot->revision;
  }
  return binding;
}

static AppModalBinding ui_modal_dispatch_binding(Persistence *p, UI *ui,
                                                   AppModalAction action) {
  if (action == APP_MODAL_DAILY_RECOVER_PREVIOUS)
    return home_daily_recovery_modal_binding(p, ui);
  if (action==APP_MODAL_DAILY || action==APP_MODAL_DAILY_CHECKPOINT)
    return ui_modal_daily_binding(p,ui);
  if (action == APP_MODAL_NEW_GAME ||
      action == APP_MODAL_NEW_SAVE ||
      action == APP_MODAL_NEW_KEEP ||
      action == APP_MODAL_NEW_KEEP_RETRY ||
      action == APP_MODAL_NEW_GENERATE_RETRY)
    return ui_modal_new_game_binding(p, ui);
  if (action == APP_MODAL_DELETE || action == APP_MODAL_REPLAY ||
      action == APP_MODAL_REPLAY_CONFLICT ||
      action == APP_MODAL_REPLAY_CHECKPOINT ||
      action == APP_MODAL_REPLAY_KEEP ||
      action == APP_MODAL_REPLAY_RETAIN_RETRY ||
      action == APP_MODAL_REPLAY_ACTIVATE_RETRY ||
      action == APP_MODAL_RECOVER_RECORD) {
    AppModalBinding selected = {0};
    (void)games_browser_modal_binding(p, ui, &selected);
    return selected;
  }
  return ui_modal_current_binding(p, ui);
}

static void ui_modal_select(unsigned button) {
  if (!ui_modal.active) return;
  AppModalBinding current = ui_modal_dispatch_binding(
      audio_persistence, audio_ui, ui_modal.request.action);
  AppModalResult result={0};
  uint64_t serial=ui_modal.serial;
  if (app_modal_choose(&ui_modal,serial,&current,button,now_ms(),&result) &&
      result.resolution==APP_MODAL_CHOSEN)
    ui_modal_pending=result;
  ui_modal_pressed=-1;
  ui_modal_mouse_active=false;
  ui_modal_touch_active=false;
  if (audio_gfx) audio_gfx->dirty=true;
}

/* Called after Android lifecycle normalization and touch translation, before
   any underlying pointer/focus/audio/board handler can consume the event. */
static bool ui_modal_handle_event(const SDL_Event *event) {
  if (!ui_modal.active || !event) return false;
  if (event->type==SDL_WINDOWEVENT) return false;
  if (event->type==SDL_QUIT) {
    ui_modal_cancel_current();
    return true;
  }
  if (event->type==SDL_KEYDOWN) {
    if (event->key.repeat) return true;
    SDL_Keycode key=event->key.keysym.sym;
    SDL_Keymod mod=(SDL_Keymod)event->key.keysym.mod;
    if (key==SDLK_ESCAPE) ui_modal_cancel_current();
    else if (key==SDLK_TAB || key==SDLK_LEFT || key==SDLK_RIGHT) {
      int direction=key==SDLK_LEFT ||
          (key==SDLK_TAB && (mod & KMOD_SHIFT)) ? -1:1;
      int count=(int)ui_modal.request.button_count;
      unsigned next=(unsigned)(((int)ui_modal.focus+direction+count)%count);
      (void)app_modal_focus(&ui_modal,ui_modal.serial,next);
    } else if (key==SDLK_PAGEUP || key==SDLK_PAGEDOWN ||
               key==SDLK_UP || key==SDLK_DOWN) {
      int delta=(key==SDLK_PAGEUP||key==SDLK_UP)?-60:60;
      int next=ui_modal_scroll_px+delta;
      if (next<0) next=0;
      if (next>ui_modal_scroll_max_px) next=ui_modal_scroll_max_px;
      ui_modal_scroll_px=next;
    } else if (input_activation_key(key)) {
      ui_modal_select(ui_modal.focus);
    }
    if (audio_gfx) audio_gfx->dirty=true;
    return true;
  }
  if (event->type==SDL_FINGERDOWN || event->type==SDL_FINGERMOTION ||
      event->type==SDL_FINGERUP) {
    if (!audio_gfx || audio_gfx->width<=0 || audio_gfx->height<=0)
      return true;
    const SDL_TouchFingerEvent *finger=&event->tfinger;
    int x=(int)(finger->x*audio_gfx->width);
    int y=(int)(finger->y*audio_gfx->height);
    int hit=(finger->x>=0.0f && finger->x<=1.0f &&
             finger->y>=0.0f && finger->y<=1.0f) ?
        ui_modal_button_at(audio_gfx,x,y):-1;
    if (event->type==SDL_FINGERDOWN) {
      if (ui_modal_touch_active || ui_modal_mouse_active) {
        ui_modal_pressed=-1; /* Multiple fingers cannot confirm. */
      } else {
        ui_modal_touch_active=true;
        ui_modal_touch_id=finger->touchId;
        ui_modal_finger_id=finger->fingerId;
        ui_modal_pressed=hit;
        if (hit>=0)
          (void)app_modal_focus(&ui_modal,ui_modal.serial,(unsigned)hit);
      }
    } else if (ui_modal_touch_active &&
               finger->touchId==ui_modal_touch_id &&
               finger->fingerId==ui_modal_finger_id) {
      if (event->type==SDL_FINGERMOTION && hit!=ui_modal_pressed)
        ui_modal_pressed=-1;
      if (event->type==SDL_FINGERUP) {
        if (!ui_modal_mouse_active && ui_modal_pressed>=0 &&
            hit==ui_modal_pressed)
          ui_modal_select((unsigned)hit);
        ui_modal_touch_active=false;
        ui_modal_pressed=-1;
      }
    }
    if (audio_gfx) audio_gfx->dirty=true;
    return true;
  }
  if (event->type==SDL_MOUSEBUTTONDOWN) {
    ui_modal_pressed=-1;
    ui_modal_mouse_active=event->button.button==SDL_BUTTON_LEFT &&
                          !ui_modal_touch_active;
    if (ui_modal_mouse_active) {
      int hit=ui_modal_button_at(audio_gfx,event->button.x,event->button.y);
      if (hit>=0) {
        ui_modal_pressed=hit;
        (void)app_modal_focus(&ui_modal,ui_modal.serial,(unsigned)hit);
      }
    }
    if (audio_gfx) audio_gfx->dirty=true;
    return true;
  }
  if (event->type==SDL_MOUSEBUTTONUP) {
    int hit=event->button.button==SDL_BUTTON_LEFT ?
        ui_modal_button_at(audio_gfx,event->button.x,event->button.y):-1;
    if (ui_modal_mouse_active && ui_modal_pressed>=0 &&
        hit==ui_modal_pressed)
      ui_modal_select((unsigned)hit);
    ui_modal_mouse_active=false;
    ui_modal_pressed=-1;
    return true;
  }
  if (event->type==SDL_MOUSEWHEEL) {
    int dy=event->wheel.y;
    if (event->wheel.direction==SDL_MOUSEWHEEL_FLIPPED) dy=-dy;
    int next=ui_modal_scroll_px-dy*40;
    if (next<0) next=0;
    if (next>ui_modal_scroll_max_px) next=ui_modal_scroll_max_px;
    ui_modal_scroll_px=next;
    if (audio_gfx) audio_gfx->dirty=true;
    return true;
  }
  return event->type==SDL_KEYUP || event->type==SDL_MOUSEMOTION ||
         event->type==SDL_FINGERDOWN || event->type==SDL_FINGERMOTION ||
         event->type==SDL_FINGERUP;
}
