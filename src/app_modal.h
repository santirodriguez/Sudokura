#ifndef SUDOKURA_APP_MODAL_H
#define SUDOKURA_APP_MODAL_H

#include "app_clock.h"

/* A single native-owner modal, not an event loop. The owner must outlive the
   modal; cancel it before resetting/replacing that AppState. Lifecycle handling
   runs first and establishes its pause reason before cancelling the modal. */
typedef enum {
  APP_MODAL_NONE = 0,
  APP_MODAL_MESSAGE,
  APP_MODAL_CONFIRM,
  APP_MODAL_RECOVERY
} AppModalKind;

typedef enum {
  APP_MODAL_ACTION_NONE = 0,
  APP_MODAL_RETRY_SAVE,
  APP_MODAL_NEW_GAME,
  APP_MODAL_RESTART,
  APP_MODAL_HOME,
  APP_MODAL_EXIT,
  APP_MODAL_LOAD,
  APP_MODAL_REPLAY,
  APP_MODAL_REPLAY_CONFLICT,
  APP_MODAL_REPLAY_CHECKPOINT,
  APP_MODAL_REPLAY_KEEP,
  APP_MODAL_REPLAY_RETAIN_RETRY,
  APP_MODAL_REPLAY_ACTIVATE_RETRY,
  APP_MODAL_DELETE,
  APP_MODAL_RECOVER_RECORD,
  APP_MODAL_RECOVER_PROFILE,
  APP_MODAL_DAILY,
  APP_MODAL_CHANGE_MODE,
  APP_MODAL_RESTART_SAVE,
  APP_MODAL_RESTART_DISCARD,
  APP_MODAL_NEW_SAVE,
  APP_MODAL_NEW_KEEP,
  APP_MODAL_NEW_KEEP_RETRY,
  APP_MODAL_NEW_GENERATE_RETRY,
  APP_MODAL_DAILY_CHECKPOINT,
  APP_MODAL_DAILY_RECOVER_PREVIOUS,
  APP_MODAL_DAILY_ROLLOVER_RETRY,
  APP_MODAL_DAILY_ACTIVATE_RETRY,
  APP_MODAL_LOAD_CHECKPOINT,
  APP_MODAL_LOAD_KEEP,
  APP_MODAL_LOAD_RETAIN_RETRY,
  APP_MODAL_ACTION_COUNT
} AppModalAction;

/* Use the same stable attempt keys as pointer targets, never a catalogue row.
   The UI owner advances generation on navigation, rescan and layout changes. */
typedef struct {
  AppScreen screen;
  uint64_t generation, key_hi, key_lo, revision;
} AppModalBinding;

typedef struct {
  AppModalKind kind;
  AppModalAction action;
  AppModalBinding binding;
  TextKey title, body, buttons[3];
  unsigned button_count;
  unsigned destructive_buttons;
} AppModalRequest;

typedef struct {
  bool active, owns_pause;
  unsigned focus;
  uint64_t serial;
  AppState *owner;
  AppModalRequest request;
} AppModal;

typedef enum {
  APP_MODAL_NO_RESULT = 0,
  APP_MODAL_CHOSEN,
  APP_MODAL_CANCELLED,
  APP_MODAL_STALE
} AppModalResolution;

typedef struct {
  AppModalResolution resolution;
  AppModalAction action;
  unsigned button;
  AppModalBinding binding;
} AppModalResult;

static inline bool app_modal_binding_equal(AppModalBinding a,
                                            AppModalBinding b) {
  return a.screen == b.screen && a.generation == b.generation &&
         a.key_hi == b.key_hi && a.key_lo == b.key_lo &&
         a.revision == b.revision;
}

static inline bool app_modal_request_valid(const AppModalRequest *request) {
  if (!request || request->kind <= APP_MODAL_NONE ||
      request->kind > APP_MODAL_RECOVERY ||
      request->action < APP_MODAL_ACTION_NONE ||
      request->action >= APP_MODAL_ACTION_COUNT ||
      request->binding.screen < APP_SCREEN_HOME ||
      request->binding.screen > APP_SCREEN_GAMES ||
      !request->binding.generation || request->title < 0 ||
      request->title >= T_COUNT || request->body < 0 ||
      request->body >= T_COUNT || !request->button_count ||
      request->button_count > 3)
    return false;
  unsigned allowed = (1u << request->button_count) - 1u;
  /* Button zero is always safe Back/Cancel, including initial Enter focus. */
  if ((request->destructive_buttons & ~allowed) != 0 ||
      (request->destructive_buttons & 1u) != 0)
    return false;
  for (unsigned i = 0; i < request->button_count; ++i)
    if (request->buttons[i] < 0 || request->buttons[i] >= T_COUNT)
      return false;
  if (request->kind == APP_MODAL_MESSAGE)
    return request->button_count == 1 &&
           request->action == APP_MODAL_ACTION_NONE;
  if (request->button_count < 2 || request->action == APP_MODAL_ACTION_NONE)
    return false;
  if (request->action == APP_MODAL_LOAD ||
      request->action == APP_MODAL_REPLAY ||
      request->action == APP_MODAL_REPLAY_CONFLICT ||
      request->action == APP_MODAL_REPLAY_CHECKPOINT ||
      request->action == APP_MODAL_REPLAY_KEEP ||
      request->action == APP_MODAL_REPLAY_RETAIN_RETRY ||
      request->action == APP_MODAL_REPLAY_ACTIVATE_RETRY ||
      request->action == APP_MODAL_DELETE ||
      request->action == APP_MODAL_RECOVER_RECORD) {
    if (!(request->binding.key_hi | request->binding.key_lo)) return false;
  }
  if (request->action == APP_MODAL_LOAD_CHECKPOINT ||
      request->action == APP_MODAL_LOAD_KEEP ||
      request->action == APP_MODAL_LOAD_RETAIN_RETRY) {
    if (request->kind != APP_MODAL_RECOVERY ||
        request->binding.screen != APP_SCREEN_GAMES ||
        !(request->binding.key_hi | request->binding.key_lo)) return false;
    if (request->action == APP_MODAL_LOAD_KEEP)
      return request->button_count == 3 && request->destructive_buttons == 4u;
    if (request->action == APP_MODAL_LOAD_RETAIN_RETRY)
      return request->button_count == 2 && request->destructive_buttons == 0;
    return (request->button_count == 2 && request->destructive_buttons == 0) ||
           (request->button_count == 3 && request->destructive_buttons == 4u);
  }
  if ((request->action == APP_MODAL_DELETE ||
       request->action == APP_MODAL_REPLAY_CONFLICT) &&
      (request->button_count != 2 || request->destructive_buttons != 2u))
    return false;
  if (request->action == APP_MODAL_REPLAY_CHECKPOINT &&
      !((request->button_count == 2 && request->destructive_buttons == 0) ||
        (request->button_count == 3 && request->destructive_buttons == 4u)))
    return false;
  if (request->action == APP_MODAL_REPLAY_KEEP &&
      (request->button_count != 3 || request->destructive_buttons != 4u))
    return false;
  if (request->action == APP_MODAL_RESTART_SAVE &&
      (request->kind != APP_MODAL_RECOVERY ||
       !((request->button_count == 2 && request->destructive_buttons == 0) ||
         (request->button_count == 3 && request->destructive_buttons == 4u))))
    return false;
  if ((request->action == APP_MODAL_RESTART ||
       request->action == APP_MODAL_RESTART_DISCARD) &&
      (request->kind != APP_MODAL_CONFIRM ||
       request->button_count != 2 || request->destructive_buttons != 2u))
    return false;
  if ((request->action == APP_MODAL_REPLAY_RETAIN_RETRY ||
       request->action == APP_MODAL_REPLAY_ACTIVATE_RETRY) &&
      (request->button_count != 2 || request->destructive_buttons != 0))
    return false;
  if (request->action == APP_MODAL_DAILY &&
      (request->kind != APP_MODAL_CONFIRM ||
       request->button_count != 3 ||
       request->destructive_buttons != 0u ||
       !(request->binding.key_hi | request->binding.key_lo)))
    return false;
  if (request->action == APP_MODAL_DAILY_CHECKPOINT &&
      (request->kind != APP_MODAL_RECOVERY ||
       request->button_count != 2 ||
       request->destructive_buttons != 0u))
    return false;
  if (request->action == APP_MODAL_DAILY_RECOVER_PREVIOUS &&
      (request->kind != APP_MODAL_CONFIRM ||
       request->binding.screen != APP_SCREEN_HOME ||
       !(request->binding.key_hi | request->binding.key_lo) ||
       request->button_count != 2 ||
       request->destructive_buttons != 2u))
    return false;
  if ((request->action == APP_MODAL_DAILY_ROLLOVER_RETRY ||
       request->action == APP_MODAL_DAILY_ACTIVATE_RETRY) &&
      (request->kind != APP_MODAL_RECOVERY ||
       request->binding.screen != APP_SCREEN_HOME ||
       request->button_count != 2 ||
       request->destructive_buttons != 0u))
    return false;
  if ((request->action == APP_MODAL_NEW_GAME ||
       request->action == APP_MODAL_NEW_SAVE ||
       request->action == APP_MODAL_NEW_KEEP ||
       request->action == APP_MODAL_NEW_KEEP_RETRY ||
       request->action == APP_MODAL_NEW_GENERATE_RETRY)) {
    switch (request->action) {
      case APP_MODAL_NEW_GAME:
        return request->kind == APP_MODAL_CONFIRM &&
               request->button_count == 2 &&
               request->destructive_buttons == 2u;
      case APP_MODAL_NEW_SAVE:
        return request->kind == APP_MODAL_RECOVERY &&
               ((request->button_count == 2 &&
                 request->destructive_buttons == 0) ||
                (request->button_count == 3 &&
                 request->destructive_buttons == 4u));
      case APP_MODAL_NEW_KEEP:
        return request->kind == APP_MODAL_CONFIRM &&
               request->button_count == 3 &&
               request->destructive_buttons == 4u;
      case APP_MODAL_NEW_KEEP_RETRY:
        return request->kind == APP_MODAL_RECOVERY &&
               request->button_count == 2 &&
               request->destructive_buttons == 0;
      case APP_MODAL_NEW_GENERATE_RETRY:
        return request->kind == APP_MODAL_CONFIRM &&
               request->button_count == 2 &&
               request->destructive_buttons == 0;
      default: break;
    }
  }
  return true;
}

/* Initialize to zero once. Keep serial across closes so a queued result from
   an earlier modal cannot activate a new modal with otherwise identical data. */
static inline bool app_modal_begin(AppModal *modal, AppState *owner,
                                   const AppModalRequest *request,
                                   uint64_t now_ms) {
  if (!modal || !owner || modal->active || modal->serial == UINT64_MAX ||
      !app_modal_request_valid(request) ||
      owner->screen != request->binding.screen)
    return false;
  uint64_t serial = modal->serial + 1u;
  AppModalRequest copy = *request;
  bool owns_pause = app_clock_pause(owner, APP_PAUSE_MODAL, now_ms);
  *modal = (AppModal){.active = true, .owns_pause = owns_pause,
      .serial = serial, .owner = owner, .request = copy};
  return true;
}

static inline bool app_modal_focus(AppModal *modal, uint64_t serial,
                                   unsigned button) {
  if (!modal || !modal->active || modal->serial != serial ||
      button >= modal->request.button_count)
    return false;
  modal->focus = button;
  return true;
}

/* Clear state before returning to the caller's continuation. No callback,
   board mutation or storage write is performed by this contract. */
static inline void app_modal_close(AppModal *modal, uint64_t now_ms) {
  if (!modal || !modal->active) return;
  AppState *owner = modal->owner;
  bool owns_pause = modal->owns_pause;
  uint64_t serial = modal->serial;
  *modal = (AppModal){.serial = serial};
  if (owns_pause) (void)app_clock_resume(owner, APP_PAUSE_MODAL, now_ms);
}

/* Back/Escape/lifecycle cancellation never returns a pending action. */
static inline bool app_modal_cancel(AppModal *modal, uint64_t serial,
                                    uint64_t now_ms) {
  if (!modal || !modal->active || modal->serial != serial) return false;
  app_modal_close(modal, now_ms);
  return true;
}

/* Resolve current identity on the native owner immediately before this call.
   True means the modal closed, not that an action was accepted: dispatch only
   CHOSEN, through the existing checked gameplay/persistence helpers. */
static inline bool app_modal_choose(AppModal *modal, uint64_t serial,
                                    const AppModalBinding *current,
                                    unsigned button, uint64_t now_ms,
                                    AppModalResult *result) {
  if (result) *result = (AppModalResult){0};
  if (!modal || !result || !modal->active || modal->serial != serial ||
      button >= modal->request.button_count)
    return false;
  AppModalResult resolved = {.resolution = APP_MODAL_CANCELLED};
  if (!current || modal->owner->screen != current->screen ||
      !app_modal_binding_equal(modal->request.binding, *current)) {
    resolved.resolution = APP_MODAL_STALE;
  } else if (button != 0) {
    resolved.resolution = APP_MODAL_CHOSEN;
    resolved.action = modal->request.action;
    resolved.button = button;
    resolved.binding = modal->request.binding;
  }
  app_modal_close(modal, now_ms);
  *result = resolved;
  return true;
}

#endif
