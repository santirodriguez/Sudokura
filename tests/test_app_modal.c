#include "app_modal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AppState playing(void) {
  AppState state = {0};
  state.session_open = state.has_session = true;
  state.screen = APP_SCREEN_PLAY;
  state.running_since_ms = 100;
  return state;
}

static AppModalRequest deletion(void) {
  return (AppModalRequest){
      .kind = APP_MODAL_CONFIRM, .action = APP_MODAL_DELETE,
      .binding = {APP_SCREEN_PLAY, 7, 11, 22, 3},
      .title = T_GAMES, .body = T_GAMES_DELETE_PROMPT,
      .buttons = {T_CANCEL, T_GAMES_DELETE}, .button_count = 2,
      .destructive_buttons = 2u};
}

static void test_default_cancel_and_once(void) {
  AppState state = playing();
  AppModal modal = {0};
  AppModalRequest request = deletion();
  AppModalResult result;
  assert(app_modal_begin(&modal, &state, &request, 200));
  uint64_t first = modal.serial;
  assert(modal.focus == 0 && modal.owns_pause);
  assert(app_clock_elapsed_ms(&state, 900) == 100);
  assert(app_modal_choose(&modal, first, &request.binding,
                          modal.focus, 1000, &result));
  assert(result.resolution == APP_MODAL_CANCELLED);
  assert(result.action == APP_MODAL_ACTION_NONE);
  assert(app_clock_elapsed_ms(&state, 1100) == 200);
  assert(!app_modal_choose(&modal, first, &request.binding, 1, 1100, &result));
  assert(result.resolution == APP_MODAL_NO_RESULT);

  assert(app_modal_begin(&modal, &state, &request, 1200));
  assert(modal.serial != first);
  assert(!app_modal_choose(&modal, first, &request.binding, 1, 1300, &result));
  assert(!app_modal_cancel(&modal, first, 1300));
  assert(!app_modal_focus(&modal, first, 1));
  assert(modal.active && modal.focus == 0);
  assert(app_modal_focus(&modal, modal.serial, 1));
  assert(app_modal_choose(&modal, modal.serial, &request.binding,
                          modal.focus, 1400, &result));
  assert(result.resolution == APP_MODAL_CHOSEN);
  assert(result.action == APP_MODAL_DELETE && result.button == 1);
  assert(app_modal_binding_equal(result.binding, request.binding));
  assert(!modal.active && !modal.owner && !modal.owns_pause);
  assert(!app_modal_cancel(&modal, modal.serial, 1500));
}

static void test_pause_ownership(void) {
  /* Every combination of pre-existing reasons, with focus acquired while the
     dialog is open. The real clock implementation must preserve all owners. */
  for (unsigned reasons = 0; reasons < 32; ++reasons) {
    for (unsigned focus = 0; focus < 2; ++focus) {
      AppState state = playing();
      state.pause_reasons = reasons;
      AppModal modal = {0};
      AppModalRequest request = deletion();
      assert(app_modal_begin(&modal, &state, &request, 200));
      assert(modal.owns_pause == !(reasons & APP_PAUSE_MODAL));
      assert(state.pause_reasons == (reasons | APP_PAUSE_MODAL));
      if (focus) (void)app_clock_pause(&state, APP_PAUSE_FOCUS, 300);
      assert(app_modal_cancel(&modal, modal.serial, 400));
      unsigned expected = reasons | (focus ? APP_PAUSE_FOCUS : 0);
      assert(state.pause_reasons == expected);
      assert(app_clock_running(&state) == (expected == 0));
      uint64_t elapsed = reasons == 0 ? 100 : 0;
      assert(app_clock_elapsed_ms(&state, 500) ==
             elapsed + (expected == 0 ? 100 : 0));
    }
  }
}

static void test_stale_identity(void) {
  for (unsigned field = 0; field < 7; ++field) {
    AppState state = playing();
    AppModal modal = {0};
    AppModalRequest request = deletion();
    if (field & 1u) {
      request.action = APP_MODAL_REPLAY;
      request.buttons[1] = T_GAMES_REPLAY;
      request.destructive_buttons = 0;
    }
    assert(app_modal_begin(&modal, &state, &request, 200));
    AppModalBinding current = request.binding;
    switch (field) {
      case 0: ++current.generation; break;
      case 1: ++current.key_hi; break;
      case 2: ++current.key_lo; break;
      case 3: ++current.revision; break;
      case 4: current.screen = APP_SCREEN_HOME; break;
      case 5: state.screen = APP_SCREEN_HOME; break;
      default: break;
    }
    AppModalResult result;
    assert(app_modal_choose(&modal, modal.serial,
                            field == 6 ? NULL : &current, 1, 300, &result));
    assert(result.resolution == APP_MODAL_STALE);
    assert(result.action == APP_MODAL_ACTION_NONE && result.button == 0);
    assert(!modal.active && state.pause_reasons == 0);
  }
}

static void test_invalid_and_nested(void) {
  AppState state = playing();
  AppModal modal = {0};
  AppModalRequest request = deletion();
  assert(!app_modal_begin(NULL, &state, &request, 200));
  assert(!app_modal_begin(&modal, NULL, &request, 200));
  assert(!app_modal_begin(&modal, &state, NULL, 200));
  for (unsigned field = 0; field < 12; ++field) {
    AppModalRequest bad = request;
    switch (field) {
      case 0: bad.button_count = 0; break;
      case 1: bad.button_count = 4; break;
      case 2: bad.destructive_buttons = 1; break;
      case 3: bad.destructive_buttons = 4; break;
      case 4: bad.binding.generation = 0; break;
      case 5: bad.binding.key_hi = bad.binding.key_lo = 0; break;
      case 6: bad.title = T_COUNT; break;
      case 7: bad.body = (TextKey)-1; break;
      case 8: bad.buttons[1] = T_COUNT; break;
      case 9: bad.action = APP_MODAL_ACTION_COUNT; break;
      case 10: bad.kind = APP_MODAL_NONE; break;
      default: bad.binding.screen = APP_SCREEN_HOME; break;
    }
    assert(!app_modal_begin(&modal, &state, &bad, 200));
    assert(!modal.active && modal.serial == 0 && state.pause_reasons == 0);
  }
  assert(app_modal_begin(&modal, &state, &request, 200));
  AppModal saved = modal;
  assert(!app_modal_begin(&modal, &state, &request, 300));
  assert(memcmp(&modal, &saved, sizeof modal) == 0);
  AppModalResult result;
  assert(!app_modal_focus(&modal, modal.serial, 2));
  assert(!app_modal_choose(&modal, modal.serial, &request.binding, 3, 300, &result));
  assert(!app_modal_choose(&modal, modal.serial, &request.binding, 1, 300, NULL));
  assert(modal.active && state.pause_reasons == APP_PAUSE_MODAL);
  assert(app_modal_cancel(&modal, modal.serial, 300));
  modal.serial = UINT64_MAX - 1;
  assert(app_modal_begin(&modal, &state, &request, 400));
  assert(modal.serial == UINT64_MAX);
  assert(app_modal_cancel(&modal, modal.serial, 500));
  assert(!app_modal_begin(&modal, &state, &request, 600));
  assert(state.pause_reasons == 0);
}

static void test_message_and_recovery(void) {
  AppState state = playing();
  AppModal modal = {0};
  AppModalRequest request = deletion();
  request.kind = APP_MODAL_MESSAGE;
  request.action = APP_MODAL_ACTION_NONE;
  request.title = T_STORAGE_ERROR;
  request.body = T_SAVE_FAILED;
  request.buttons[0] = T_BACK;
  request.button_count = 1;
  request.destructive_buttons = 0;
  AppModalResult result;
  assert(app_modal_begin(&modal, &state, &request, 200));
  assert(app_modal_choose(&modal, modal.serial, &request.binding, 0, 300, &result));
  assert(result.action == APP_MODAL_ACTION_NONE);
  request.kind = APP_MODAL_RECOVERY;
  request.action = APP_MODAL_RETRY_SAVE;
  request.buttons[1] = T_RETRY_SAVE;
  request.buttons[2] = T_DISCARD;
  request.button_count = 3;
  request.destructive_buttons = 4;
  for (unsigned button = 0; button < 3; ++button) {
    assert(app_modal_begin(&modal, &state, &request, 400));
    assert(modal.focus == 0);
    assert(app_modal_choose(&modal, modal.serial, &request.binding,
                            button, 500, &result));
    assert(result.resolution == (button ? APP_MODAL_CHOSEN : APP_MODAL_CANCELLED));
    assert(result.action == (button ? APP_MODAL_RETRY_SAVE : APP_MODAL_ACTION_NONE));
    assert(result.button == button);
  }
}

static void test_home_recovery(void) {
  AppState state = playing();
  AppModal modal = {0};
  AppModalRequest request = deletion();
  request.kind = APP_MODAL_RECOVERY;
  request.action = APP_MODAL_HOME;
  request.buttons[0] = T_BACK;
  request.buttons[1] = T_RETRY_SAVE;
  request.button_count = 2;
  request.destructive_buttons = 0;
  request.binding.key_hi = request.binding.key_lo = 0;
  assert(app_modal_begin(&modal, &state, &request, 200));
  assert(app_modal_focus(&modal, modal.serial, 1));
  AppModalResult result;
  assert(app_modal_choose(&modal, modal.serial,
                          &request.binding, 1, 300, &result));
  assert(result.action == APP_MODAL_HOME && result.button == 1);
  assert(state.pause_reasons == 0);
}

static void test_library_recovery_identity(void) {
  AppState state = playing();
  state.screen = APP_SCREEN_GAMES;
  state.pause_reasons = APP_PAUSE_MODAL;
  AppModalRequest request = deletion();
  request.kind = APP_MODAL_CONFIRM;
  request.action = APP_MODAL_RECOVER_RECORD;
  request.binding.screen = APP_SCREEN_GAMES;
  request.buttons[1] = T_RECOVER;
  AppModal modal = {0};
  AppModalResult result;
  assert(app_modal_begin(&modal, &state, &request, 200));
  assert(!modal.owns_pause && modal.focus == 0);
  AppModalBinding changed = request.binding;
  ++changed.revision;
  assert(app_modal_choose(&modal, modal.serial, &changed, 1, 300, &result));
  assert(result.resolution == APP_MODAL_STALE);
  assert(state.pause_reasons == APP_PAUSE_MODAL);
  assert(app_modal_begin(&modal, &state, &request, 400));
  assert(app_modal_choose(&modal, modal.serial, &request.binding, 0, 500, &result));
  assert(result.resolution == APP_MODAL_CANCELLED);
  assert(app_modal_begin(&modal, &state, &request, 600));
  assert(app_modal_focus(&modal, modal.serial, 1));
  assert(app_modal_choose(&modal, modal.serial, &request.binding, 1, 700, &result));
  assert(result.resolution == APP_MODAL_CHOSEN &&
         result.action == APP_MODAL_RECOVER_RECORD && result.button == 1);
  assert(state.pause_reasons == APP_PAUSE_MODAL);
}

static void test_replay_confirmation_identity(void) {
  AppState state=playing();
  state.screen=APP_SCREEN_GAMES;
  state.pause_reasons=APP_PAUSE_MODAL;
  AppModalRequest request=deletion();
  request.action=APP_MODAL_REPLAY;
  request.binding.screen=APP_SCREEN_GAMES;
  request.title=T_GAMES_REPLAY;
  request.body=T_GAMES_REPLAY_START_PROMPT;
  request.buttons[1]=T_GAMES_REPLAY;
  request.destructive_buttons=0;
  AppModal modal={0};
  AppModalResult result;
  assert(app_modal_request_valid(&request));
  assert(app_modal_begin(&modal,&state,&request,200));
  assert(modal.focus==0 && !modal.owns_pause);
  assert(app_modal_choose(&modal,modal.serial,&request.binding,
                          0,300,&result));
  assert(result.resolution==APP_MODAL_CANCELLED);
  assert(result.action==APP_MODAL_ACTION_NONE);
  assert(state.pause_reasons==APP_PAUSE_MODAL);
  assert(app_modal_begin(&modal,&state,&request,400));
  AppModalBinding stale=request.binding;
  ++stale.key_hi;
  assert(app_modal_choose(&modal,modal.serial,&stale,1,500,&result));
  assert(result.resolution==APP_MODAL_STALE);
  assert(state.pause_reasons==APP_PAUSE_MODAL);
  assert(app_modal_begin(&modal,&state,&request,600));
  assert(app_modal_focus(&modal,modal.serial,1));
  uint64_t serial=modal.serial;
  assert(app_modal_choose(&modal,serial,&request.binding,1,700,&result));
  assert(result.resolution==APP_MODAL_CHOSEN);
  assert(result.action==APP_MODAL_REPLAY && result.button==1);
  assert(app_modal_binding_equal(result.binding,request.binding));
  assert(!app_modal_choose(&modal,serial,&request.binding,1,800,&result));
  assert(state.pause_reasons==APP_PAUSE_MODAL);
}

static void test_pending_replay_consent(void) {
  AppState state=playing();
  state.screen=APP_SCREEN_GAMES;
  state.pause_reasons=APP_PAUSE_MODAL;
  AppModalRequest request=deletion();
  request.action=APP_MODAL_REPLAY_CONFLICT;
  request.binding.screen=APP_SCREEN_GAMES;
  request.title=T_GAMES_REPLAY;
  request.body=T_GAMES_REPLAY_PENDING_CONFLICT;
  request.buttons[1]=T_DISCARD;
  AppModal modal={0};
  AppModalResult result;
  assert(app_modal_request_valid(&request));
  AppModalRequest unsafe=request;
  unsafe.destructive_buttons=0;
  assert(!app_modal_request_valid(&unsafe));
  assert(app_modal_begin(&modal,&state,&request,300));
  assert(modal.focus==0 && !modal.owns_pause);
  assert(app_modal_choose(&modal,modal.serial,&request.binding,0,400,&result));
  assert(result.resolution==APP_MODAL_CANCELLED);
  assert(state.pause_reasons==APP_PAUSE_MODAL);
  assert(app_modal_begin(&modal,&state,&request,500));
  AppModalBinding stale=request.binding;
  ++stale.revision;
  assert(app_modal_choose(&modal,modal.serial,&stale,1,600,&result));
  assert(result.resolution==APP_MODAL_STALE);
  assert(app_modal_begin(&modal,&state,&request,700));
  assert(app_modal_focus(&modal,modal.serial,1));
  uint64_t serial=modal.serial;
  assert(app_modal_choose(&modal,serial,&request.binding,1,800,&result));
  assert(result.resolution==APP_MODAL_CHOSEN);
  assert(result.action==APP_MODAL_REPLAY_CONFLICT && result.button==1);
  assert(app_modal_binding_equal(result.binding,request.binding));
  assert(!app_modal_choose(&modal,serial,&request.binding,1,900,&result));
  assert(state.pause_reasons==APP_PAUSE_MODAL);
}

static void test_replay_save_stages(void) {
  const struct {
    AppModalAction action;
    unsigned buttons, destructive, accept;
  } cases[] = {
      {APP_MODAL_REPLAY_CHECKPOINT,2,0,1},
      {APP_MODAL_REPLAY_CHECKPOINT,3,4u,2},
      {APP_MODAL_REPLAY_KEEP,3,4u,2},
      {APP_MODAL_REPLAY_RETAIN_RETRY,2,0,1},
      {APP_MODAL_REPLAY_ACTIVATE_RETRY,2,0,1}
  };
  for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
    AppState state=playing();
    state.screen=APP_SCREEN_GAMES;
    state.pause_reasons=APP_PAUSE_MODAL;
    AppModalRequest request=deletion();
    request.kind=APP_MODAL_RECOVERY;
    request.action=cases[i].action;
    request.binding.screen=APP_SCREEN_GAMES;
    request.title=T_STORAGE_ERROR;
    request.body=T_GAMES_RETRY_SAVE_PROMPT;
    request.buttons[0]=T_BACK;
    request.buttons[1]=T_RETRY_SAVE;
    request.buttons[2]=T_DISCARD;
    request.button_count=cases[i].buttons;
    request.destructive_buttons=cases[i].destructive;
    assert(app_modal_request_valid(&request));
    AppModalRequest unsafe=request;
    unsafe.destructive_buttons=cases[i].destructive?0:2u;
    assert(!app_modal_request_valid(&unsafe));
    AppModal modal={0};
    AppModalResult result;
    assert(app_modal_begin(&modal,&state,&request,100));
    assert(!modal.owns_pause && modal.focus==0);
    assert(app_modal_choose(&modal,modal.serial,&request.binding,
                            0,200,&result));
    assert(result.resolution==APP_MODAL_CANCELLED);
    assert(state.pause_reasons==APP_PAUSE_MODAL);
    assert(app_modal_begin(&modal,&state,&request,300));
    AppModalBinding stale=request.binding;
    ++stale.revision;
    assert(app_modal_choose(&modal,modal.serial,&stale,
                            cases[i].accept,400,&result));
    assert(result.resolution==APP_MODAL_STALE);
    assert(state.pause_reasons==APP_PAUSE_MODAL);
    assert(app_modal_begin(&modal,&state,&request,500));
    assert(app_modal_focus(&modal,modal.serial,cases[i].accept));
    uint64_t serial=modal.serial;
    assert(app_modal_choose(&modal,serial,&request.binding,
                            cases[i].accept,600,&result));
    assert(result.resolution==APP_MODAL_CHOSEN);
    assert(result.action==cases[i].action);
    assert(result.button==cases[i].accept);
    assert(app_modal_binding_equal(result.binding,request.binding));
    assert(!app_modal_choose(&modal,serial,&request.binding,
                             cases[i].accept,700,&result));
    assert(state.pause_reasons==APP_PAUSE_MODAL);
  }
}

static void test_restart_modal_contract(void) {
  AppState state=playing();
  state.pause_reasons=APP_PAUSE_MANUAL|APP_PAUSE_FOCUS;
  AppModalRequest request={
      .kind=APP_MODAL_RECOVERY,.action=APP_MODAL_RESTART_SAVE,
      .binding={APP_SCREEN_PLAY,7,11,22,3},
      .title=T_UNSAVED_TITLE,.body=T_UNSAVED_REPLACE,
      .buttons={T_BACK,T_RETRY_SAVE,T_DISCARD},
      .button_count=3,.destructive_buttons=4u};
  AppModal invalid_modal={0};
  AppModalRequest invalid=request;
  invalid.destructive_buttons=0;
  assert(!app_modal_request_valid(&invalid));
  invalid=request;
  invalid.kind=APP_MODAL_CONFIRM;
  assert(!app_modal_request_valid(&invalid));
  invalid=request;
  invalid.button_count=2;
  assert(!app_modal_request_valid(&invalid));
  assert(app_modal_request_valid(&request));
  AppModalResult result={0};
  assert(app_modal_begin(&invalid_modal,&state,&request,100));
  assert(invalid_modal.focus==0 && invalid_modal.owns_pause);
  assert(state.pause_reasons==
         (APP_PAUSE_MANUAL|APP_PAUSE_FOCUS|APP_PAUSE_MODAL));
  assert(app_modal_choose(&invalid_modal,invalid_modal.serial,
                          &request.binding,0,200,&result));
  assert(result.resolution==APP_MODAL_CANCELLED);
  assert(state.pause_reasons==(APP_PAUSE_MANUAL|APP_PAUSE_FOCUS));

  assert(app_modal_begin(&invalid_modal,&state,&request,300));
  AppModalBinding stale=request.binding;
  ++stale.revision;
  assert(app_modal_choose(&invalid_modal,invalid_modal.serial,
                          &stale,2,400,&result));
  assert(result.resolution==APP_MODAL_STALE);
  assert(state.pause_reasons==(APP_PAUSE_MANUAL|APP_PAUSE_FOCUS));

  assert(app_modal_begin(&invalid_modal,&state,&request,500));
  assert(app_modal_focus(&invalid_modal,invalid_modal.serial,2));
  uint64_t serial=invalid_modal.serial;
  assert(app_modal_choose(&invalid_modal,serial,
                          &request.binding,2,600,&result));
  assert(result.resolution==APP_MODAL_CHOSEN &&
         result.action==APP_MODAL_RESTART_SAVE && result.button==2);
  assert(!app_modal_choose(&invalid_modal,serial,&request.binding,2,700,&result));
  assert(state.pause_reasons==(APP_PAUSE_MANUAL|APP_PAUSE_FOCUS));

  request.button_count=2;
  request.destructive_buttons=0;
  assert(app_modal_request_valid(&request));
  request.kind=APP_MODAL_CONFIRM;
  request.action=APP_MODAL_RESTART;
  request.title=T_RESTART;request.body=T_RESTART_PROMPT;
  request.buttons[0]=T_CANCEL;request.buttons[1]=T_RESTART;
  request.destructive_buttons=2u;
  assert(app_modal_request_valid(&request));
  assert(app_modal_begin(&invalid_modal,&state,&request,800));
  assert(app_modal_choose(&invalid_modal,invalid_modal.serial,
                          &request.binding,0,900,&result));
  assert(result.resolution==APP_MODAL_CANCELLED);
  request.action=APP_MODAL_RESTART_DISCARD;
  assert(app_modal_request_valid(&request));
  invalid=request; invalid.destructive_buttons=0;
  assert(!app_modal_request_valid(&invalid));
  invalid=request; invalid.button_count=3;
  assert(!app_modal_request_valid(&invalid));
  assert(app_modal_begin(&invalid_modal,&state,&request,1000));
  assert(app_modal_focus(&invalid_modal,invalid_modal.serial,1));
  assert(app_modal_choose(&invalid_modal,invalid_modal.serial,
                          &request.binding,1,1100,&result));
  assert(result.resolution==APP_MODAL_CHOSEN &&
         result.action==APP_MODAL_RESTART_DISCARD && result.button==1);
  assert(state.pause_reasons==(APP_PAUSE_MANUAL|APP_PAUSE_FOCUS));
}

int main(void) {
  test_restart_modal_contract();
  test_default_cancel_and_once();
  test_pause_ownership();
  test_stale_identity();
  test_invalid_and_nested();
  test_message_and_recovery();
  test_home_recovery();
  test_library_recovery_identity();
  test_replay_confirmation_identity();
  test_pending_replay_consent();
  test_replay_save_stages();
  puts("app modal contract: PASS (including 64 pause-ownership combinations)");
  return 0;
}
