/* Semantic values are built and actions are dispatched only by SDL's owner.
   Java owns accessibility focus, never game selection or persistence. */
#include "accessibility_labels.h"

enum {
  A11Y_HOME=100, A11Y_MODE=120, A11Y_DIFFICULTY=130, A11Y_LANGUAGE=140,
  A11Y_PLAY=200, A11Y_CELL=300, A11Y_MORE=400, A11Y_MORE_CLOSE=410,
  A11Y_PAUSE=420, A11Y_RESULT=430, A11Y_INFO=440, A11Y_INFO_BODY=1000,
  A11Y_INFO_UP=480, A11Y_INFO_DOWN=481, A11Y_MODAL=500,
  A11Y_MODAL_BODY=1100, A11Y_SETTINGS=520, A11Y_MUSIC_MINUS=540,
  A11Y_MUSIC_PLUS=541, A11Y_FX_MINUS=542, A11Y_FX_PLUS=543,
  A11Y_HINT=550, A11Y_LIBRARY=570, A11Y_ROW=600, A11Y_STATUS=900, A11Y_HEADING=901,
  A11Y_PREVIEW_CELL=700, A11Y_AUDIO=800, A11Y_AUDIO_MUSIC=801, A11Y_AUDIO_FX=802
};
static AccessibilitySnapshot ui_a11y_live;
static SDL_SpinLock ui_a11y_lock;
static AccessibilityCommand ui_a11y_command;
static bool ui_a11y_pending;
static uint64_t ui_a11y_context[256];
static bool ui_a11y_publish_pending;
static int ui_a11y_width,ui_a11y_height;
static char ui_a11y_notice[96];

static bool ui_a11y_submit(AccessibilityCommand command) {
  bool accepted=false;
  SDL_AtomicLock(&ui_a11y_lock);
  if (!ui_a11y_pending && command.binding.generation) {
    ui_a11y_command=command;ui_a11y_pending=true;accepted=true;
  }
  SDL_AtomicUnlock(&ui_a11y_lock);
  if (accepted) {
    SDL_Event wake;SDL_zero(wake);wake.type=SDL_USEREVENT;
    (void)SDL_PushEvent(&wake); /* No pointer payload, even on queue failure. */
  }
  return accepted;
}
static bool ui_a11y_take(AccessibilityCommand *command) {
  SDL_AtomicLock(&ui_a11y_lock);
  bool pending=ui_a11y_pending;
  if (pending) {*command=ui_a11y_command;ui_a11y_pending=false;}
  SDL_AtomicUnlock(&ui_a11y_lock);
  return pending;
}
#if defined(__ANDROID__)
JNIEXPORT jboolean JNICALL
Java_org_sudokura_prototype_PrototypeActivity_nativeAccessibilityCommand(
    JNIEnv *env,jclass klass,jlong generation,jlong high,jlong low,
    jint screen,jint node,jint action) {
  (void)env;(void)klass;
  AccessibilityCommand command={{(uint64_t)generation,(uint64_t)high,
      (uint64_t)low,screen},node,(unsigned)action};
  return ui_a11y_submit(command) ? JNI_TRUE : JNI_FALSE;
}
#endif

/* Clip semantic bounds to the same usable logical surface as rendering.
   UTF-8 labels are split at codepoint boundaries, never solution-derived. */
static void ui_a11y_add(AccessibilitySnapshot *s,const Gfx *g,int id,
    SDL_Rect rect,const char *label,unsigned actions,bool enabled,bool selected) {
  SDL_Rect bounds={0,0,g->width,g->height},clipped;
  if (!label || !*label || !SDL_IntersectRect(&rect,&bounds,&clipped)) return;
  AccessibilityNode node={0};
  node.id=id;node.x=clipped.x;node.y=clipped.y;node.w=clipped.w;node.h=clipped.h;
  node.actions=actions;node.enabled=enabled;node.selected=selected;
  size_t bytes=strlen(label);
  if (bytes>=sizeof(node.label)) {
    bytes=sizeof(node.label)-1;
    while (bytes && (((unsigned char)label[bytes]&0xc0u)==0x80u)) --bytes;
  }
  memcpy(node.label,label,bytes);node.label[bytes]=0;
  (void)accessibility_add(s,&node);
}
static void ui_a11y_text(AccessibilitySnapshot *s,const Gfx *g,int id,
                         SDL_Rect rect,const char *text,unsigned actions) {
  ui_a11y_add(s,g,id,rect,text,actions,true,false);
}
static void ui_a11y_audio(AccessibilitySnapshot *s,const UI *ui,const Gfx *g) {
  AudioControlGeometry geo=ui_audio_control_geometry(g,ui);
  char label[128];
  snprintf(label,sizeof(label),"%s: %s",tr(ui->language,T_SOUND),
      tr(ui->language,audio_is_enabled()?T_ON:T_OFF));
  ui_a11y_add(s,g,A11Y_AUDIO,sdl_rect(geo.button),label,
      ACCESSIBILITY_ACTIVATE|ACCESSIBILITY_LONG_ACTIVATE,true,audio_is_enabled());
  if (!audio_control_popup_open) return;
  const SDL_Rect rects[]={sdl_rect(geo.music),sdl_rect(geo.fx)};
  for (int i=0;i<2;++i) {
    bool muted=i?audio_fx_muted():audio_music_muted();
    snprintf(label,sizeof(label),"%s: %s",tr(ui->language,i?T_FX:T_MUSIC),
        tr(ui->language,muted?T_MUTED:T_ENABLED));
    ui_a11y_add(s,g,A11Y_AUDIO_MUSIC+i,rects[i],label,ACCESSIBILITY_ACTIVATE,true,!muted);
  }
}
static void ui_a11y_saved_preview(AccessibilitySnapshot *s,const UI *ui,const Gfx *g) {
  if (!games_browser.preview_valid || !games_browser_semantic_board.valid ||
      games_browser.scanning || games_browser.selected>=games_browser.count ||
      games_browser_semantic_board.width!=g->width || games_browser_semantic_board.height!=g->height ||
      games_browser_semantic_board.detail!=games_browser.detail_open) return;
  const GameLibraryEntry *entry=&games_browser.rows[games_browser.selected].entry;
  if (!game_attempt_id_equal(entry->id,games_browser_semantic_board.id) ||
      entry->revision!=games_browser_semantic_board.revision) return;
  SDL_Rect board=games_browser_semantic_board.rect;
  int cell=board.w/9;
  const Game *view=&games_browser.preview.game;
  for (int i=0;i<81;++i) {
    char label[ACCESSIBILITY_LABEL_LIMIT];
    accessibility_cell_label(label,ui->language,i/9,i%9,view->puzzle[i],view->fixed[i]!=0,
        cell>=18?view->notes[i]:0,false,false);
    ui_a11y_add(s,g,A11Y_PREVIEW_CELL+i,
        (SDL_Rect){board.x+i%9*cell,board.y+i/9*cell,cell,cell},label,0,true,false);
  }
}
static int ui_a11y_screen(const UI *ui) {
  if (ui_modal.active) return 100+(int)ui->screen;
  if (settings_open) return 200+(int)ui->screen;
  if (ui_more_open) return 300+(int)ui->screen;
  if (audio_control_popup_open && ui_audio_control_available(ui)) return 600+(int)ui->screen;
  if (hint_panel_interactive(ui)) return 400+(int)ui->screen;
  if (ui->screen==SCR_PLAY && ui->pause_reasons) return 500+(int)ui->screen;
  return (int)ui->screen;
}
static void ui_a11y_build_body(AccessibilitySnapshot *s,const Persistence *p,
                         const Game *game,const UI *ui,const Gfx *g) {
  AccessibilityBinding binding={ui_a11y_live.binding.generation
      ? ui_a11y_live.binding.generation : 1,
      p->active_attempt_id.high,p->active_attempt_id.low,ui_a11y_screen(ui)};
  accessibility_begin(s,binding);
  if (!g->mobile_layout || app_suspended || !g->renderer_ready || ui->generating ||
      startup_loading_active || startup_recovery.routing || home_daily_scan.active ||
      ui_modal_has_pending()) return;
#define ADD(id,rect,key,enabled,selected) \
  ui_a11y_add(s,g,id,rect,tr(ui->language,key),ACCESSIBILITY_ACTIVATE,enabled,selected)
  if (ui_modal.active) {
    UiModalLayout m=ui_modal_layout(g,ui_modal.request.button_count);
    ui_a11y_add(s,g,A11Y_HEADING,m.title,tr(ui->language,ui_modal.request.title),0,true,false);
    ui_a11y_text(s,g,A11Y_MODAL_BODY,m.body,tr(ui->language,ui_modal.request.body),
        ACCESSIBILITY_SCROLL_FORWARD|ACCESSIBILITY_SCROLL_BACKWARD);
    for (unsigned i=0;i<ui_modal.request.button_count;++i)
      ADD(A11Y_MODAL+(int)i,m.buttons[i],ui_modal.request.buttons[i],true,false);
    return;
  }
  AppGeometry geo;
  if (!get_geometry(g,ui,&geo)) return;
  if (settings_open) {
    SettingsPanelLayout layout=ui_settings_panel_layout(g);
    ui_a11y_add(s,g,A11Y_HEADING,layout.title,tr(ui->language,T_SETTINGS),0,true,false);
    const TextKey keys[]={T_THEME,T_LANGUAGE,T_SOUND,T_MUSIC,T_FX,
                          T_REDUCED_MOTION,T_AUTO_NOTES,T_BACK};
    for (int i=0;i<SETTINGS_FOCUS_COUNT;++i) {
      if (i==SETTINGS_FOCUS_MUSIC || i==SETTINGS_FOCUS_FX) continue;
      bool state=i==SETTINGS_FOCUS_THEME ? ui->dark_theme :
                 i==SETTINGS_FOCUS_SOUND ? audio_is_enabled() :
                 i==SETTINGS_FOCUS_REDUCED_MOTION ? ui->reduced_motion :
                 i==SETTINGS_FOCUS_AUTO_NOTES ? ui->auto_remove_peer_notes : false;
      char label[256];
      snprintf(label,sizeof(label),"%s: %s",tr(ui->language,keys[i]),
          i==SETTINGS_FOCUS_LANGUAGE ? language_name(ui->language) :
          i==SETTINGS_FOCUS_BACK ? tr(ui->language,T_BACK) :
          i==SETTINGS_FOCUS_THEME ? tr(ui->language,state?T_DARK:T_LIGHT) :
          tr(ui->language,state?T_ON:T_OFF));
      ui_a11y_add(s,g,A11Y_SETTINGS+i,ui_settings_focus_rect(layout,(SettingsKeyboardFocus)i),
          label,ACCESSIBILITY_ACTIVATE,true,state);
    }
    SDL_Rect rects[]={layout.music_minus,layout.music_plus,layout.fx_minus,layout.fx_plus};
    for (int i=0;i<4;++i) {
      char label[128];snprintf(label,sizeof(label),"%s %s, %d%%",
          tr(ui->language,i<2?T_MUSIC:T_FX),i%2?"+":"−",
          i<2?audio_music_volume():audio_fx_volume());
      ui_a11y_add(s,g,A11Y_MUSIC_MINUS+i,rects[i],label,ACCESSIBILITY_ACTIVATE,
          audio_is_enabled(),false);
    }
    return;
  }
  if (ui_more_open) {
    SDL_Rect heading_panel=sdl_rect(geo.more_panel);
    ui_a11y_add(s,g,A11Y_HEADING,(SDL_Rect){heading_panel.x+16,heading_panel.y+12,
        heading_panel.w-80,34},tr(ui->language,T_MORE),0,true,false);
    const TextKey keys[]={T_RESTART,T_HINT,T_VERIFY,T_SETTINGS,T_HELP,T_ABOUT};
    for (int i=0;i<GEOMETRY_MORE_COUNT;++i)
      ADD(A11Y_MORE+i,sdl_rect(geo.more_items[i]),keys[i],true,false);
    SDL_Rect panel=sdl_rect(geo.more_panel);
    ADD(A11Y_MORE_CLOSE,((SDL_Rect){panel.x+panel.w-60,panel.y+4,48,48}),T_BACK,true,false);
    return;
  }
  if (audio_control_popup_open && ui_audio_control_available(ui)) {
    ui_a11y_audio(s,ui,g);return;
  }
  if (ui->screen==SCR_TITLE) {
    ui_a11y_add(s,g,A11Y_HEADING,sdl_rect(geo.home_logo),tr(ui->language,T_HOME),0,true,false);
    const TextKey keys[]={T_START_NEW_GAME,T_CONTINUE_NORMAL,T_DAILY,T_GAMES,T_HELP,T_ABOUT,T_SETTINGS,T_QUIT};
    const int focuses[]={HOME_FOCUS_NEW,HOME_FOCUS_CONTINUE,HOME_FOCUS_DAILY,
        HOME_FOCUS_GAMES,HOME_FOCUS_HELP,HOME_FOCUS_ABOUT,HOME_FOCUS_SETTINGS,HOME_FOCUS_QUIT};
    for (int i=0;i<8;++i)
      ADD(A11Y_HOME+focuses[i],i<3?sdl_rect(geo.home_primary[i]):i==3?home_collection_rect(&geo,false):sdl_rect(geo.home_secondary[i-3]),
          keys[i],i!=1||ui->normal_resume.can_continue,false);
    ADD(A11Y_HOME+HOME_FOCUS_JOURNEY,home_collection_rect(&geo,true),T_JOURNEY,true,false);
    for (int i=0;i<3;++i) {
      ADD(A11Y_MODE+i,sdl_rect(geo.home_mode[i]),mode_key((Mode)i),true,ui->mode==(Mode)i);
      ui_a11y_add(s,g,A11Y_DIFFICULTY+i,sdl_rect(geo.home_difficulty[i]),
          difficulty_name(ui->language,(GameDifficulty)i),ACCESSIBILITY_ACTIVATE,true,
          home_difficulty==(GameDifficulty)i);
    }
  } else if (ui->screen==SCR_HELP || ui->screen==SCR_ABOUT) {
    GeometryInfo mobile_info;
    if (ui_mobile_info_layout(g,ui,&mobile_info))
      ui_a11y_add(s,g,A11Y_HEADING,sdl_rect(mobile_info.heading),
          tr(ui->language,ui->screen==SCR_HELP?T_HELP:T_ABOUT),0,true,false);
    ADD(A11Y_INFO,back_rect(g,ui),T_BACK,true,false);
    unsigned scroll=ACCESSIBILITY_SCROLL_FORWARD|ACCESSIBILITY_SCROLL_BACKWARD;
    if (ui_info_semantic_screen==ui->screen &&
        ui_info_semantic_language==ui->language &&
        ui_info_semantic_width==g->width && ui_info_semantic_height==g->height &&
        ui_info_semantic_scroll==help_scroll_px)
      for (unsigned i=0;i<ui_info_semantic_count;++i) {
        const UiInfoSemanticSpan *span=&ui_info_semantic_spans[i];
        ui_a11y_text(s,g,A11Y_INFO_BODY+span->id,span->rect,span->text,scroll);
      }
    if (ui->screen==SCR_ABOUT) {
      const TextKey keys[]={T_REPOSITORY,T_WEBSITE,T_SUPPORT,T_PRIVACY_POLICY};
      for (int i=0;i<GEOMETRY_ABOUT_LINK_COUNT;++i)
        ADD(A11Y_INFO+1+i,ui_info_link_rect(g,ui,i),keys[i],true,false);
    }
    if (help_scroll_max_px>0) {
      ADD(A11Y_INFO_UP,ui_info_scroll_rect(g,ui,false),T_A11Y_SCROLL_UP,help_scroll_px>0,false);
      ADD(A11Y_INFO_DOWN,ui_info_scroll_rect(g,ui,true),T_A11Y_SCROLL_DOWN,
          help_scroll_px<help_scroll_max_px,false);
    }
    return;
  } else if (ui->screen==SCR_END) {
    ui_a11y_add(s,g,A11Y_HEADING,sdl_rect(geo.end_heading),
        tr(ui->language,ui->result==RES_WIN?T_YOU_WIN:T_YOU_LOSE),0,true,false);
    ADD(A11Y_RESULT,sdl_rect(geo.end_buttons[0]),ui->result==RES_WIN?T_ANOTHER_GAME:T_RETRY_SAME,true,false);
    ADD(A11Y_RESULT+1,sdl_rect(geo.end_buttons[1]),T_HOME,true,false);
    return;
  } else if (ui->screen==APP_SCREEN_GAMES) {
    GamesBrowserLayout lay=games_browser_layout(g);
    ui_a11y_add(s,g,A11Y_HEADING,lay.title,tr(ui->language,games_browser.view==GAMES_SAVED?T_GAMES_LIBRARY_TITLE:T_JOURNEY),0,true,false);
    const TextKey tabs[]={T_GAMES,T_JOURNEY_WON,T_JOURNEY_RETRY};
    for(int i=0;i<3;++i)ADD(A11Y_LIBRARY+7+i,lay.tabs[i],tabs[i],true,games_browser.view==(GamesView)i);
    const int focuses[]={2,1,3,4,5,6};
    const TextKey keys[]={T_BACK,T_GAMES_LOAD,T_GAMES_REPLAY,T_GAMES_DELETE,
                          T_GAMES_PREVIOUS_PAGE,T_GAMES_NEXT_PAGE};
    SDL_Rect rects[]={lay.back,lay.primary,lay.replay,lay.remove,lay.page_previous,lay.page_next};
    for (int i=0;i<6;++i) {
      if (games_browser.detail_open && (i==1 || i>=4)) continue;
      TextKey key=keys[i];
      if(i==2)key=T_JOURNEY_RETRY_ACTION;
      if(i==3 && games_browser.view!=GAMES_SAVED)key=T_JOURNEY_ATTEMPTS;
      if (i==1 && games_browser.selected<games_browser.count) {
        const GamesRow *row=&games_browser.rows[games_browser.selected];
        key=game_library_entry_recoverable(&row->entry)?T_RECOVER:
            row->entry.result.status==SESSION_ACTIVE?T_GAMES_LOAD:T_GAMES_VIEW;
      }
      ADD(A11Y_LIBRARY+focuses[i],rects[i],key,games_browser_focus_enabled(focuses[i]),false);
    }
    if (!games_browser.detail_open && !games_browser.scanning) {
      for (int row=0;row<lay.visible && games_browser.scroll+(size_t)row<games_browser.count;++row) {
        size_t index=games_browser.scroll+(size_t)row;
        const GameLibraryEntry *entry=&games_browser.rows[index].entry;
        char label[256];snprintf(label,sizeof(label),"%s, %s, %s",
            tr(ui->language,mode_key(entry->result.mode)),
            difficulty_name(ui->language,entry->result.difficulty),
            tr(ui->language,entry->status!=STORE_OK?T_GAMES_UNAVAILABLE:
              entry->result.status==SESSION_ACTIVE?T_GAMES_IN_PROGRESS:
              entry->result.status==SESSION_WON?T_YOU_WIN:entry->result.status==SESSION_INTERRUPTED?T_JOURNEY_INTERRUPTED:T_JOURNEY_RETRY));
        ui_a11y_add(s,g,A11Y_ROW+row,games_browser_row_rect(lay,row),label,
            ACCESSIBILITY_ACTIVATE,true,index==games_browser.selected);
      }
    }
    if(games_browser.detail_open && games_browser.selected<games_browser.count) {
      char summary[640];games_browser_result_description(&games_browser.rows[games_browser.selected],ui->language,summary,sizeof(summary));
      ui_a11y_add(s,g,A11Y_STATUS,lay.subtitle,summary,0,true,false);
    }
    ui_a11y_saved_preview(s,ui,g);
    return;
  } else if (ui->screen==SCR_PLAY) {
    if (ui->pause_reasons) {
      ui_a11y_add(s,g,A11Y_HEADING,sdl_rect(geo.pause_heading),tr(ui->language,T_PAUSED),0,true,false);
      if (ui->pause_reasons & PAUSE_USER_VISIBLE) {
        ADD(A11Y_PAUSE,sdl_rect(geo.pause_buttons[0]),T_RESUME,true,false);
        ADD(A11Y_PAUSE+1,sdl_rect(geo.pause_buttons[1]),T_MENU,true,false);
      }
      return; /* Concealed cells never enter a snapshot. */
    }
    if (hint_panel_interactive(ui)) {
      HintPanelLayout panel=hint_panel_layout(g,ui);
      ui_a11y_add(s,g,A11Y_HEADING,panel.title,tr(ui->language,T_HINT_PREVIEW),0,true,false);
      char message[320];hint_preview_text(ui,message,sizeof(message));
      ui_a11y_add(s,g,A11Y_STATUS,panel.body,message,0,true,false);
      for (int i=0;i<HINT_FOCUS_COUNT;++i)
        ADD(A11Y_HINT+i,hint_focus_rect(g,ui,(HintKeyboardFocus)i),
            i==HINT_FOCUS_CANCEL?T_CANCEL:i==HINT_FOCUS_REVEAL?T_REVEAL_CELL:
            ui->hint_preview.status==HUMAN_HINT_LOGICAL?T_APPLY:T_VERIFY,
            hint_focus_enabled(ui,(HintKeyboardFocus)i),false);
      return;
    }
    if (!app_play_interactions_allowed(ui)) return;
    ui_a11y_add(s,g,A11Y_HEADING,sdl_rect(geo.play_title),tr(ui->language,mode_key(ui->mode)),0,true,false);
    const TextKey keys[]={T_MENU,T_PAUSE,T_SAVE,T_UNDO,T_REDO,T_CLEAR,T_RESTART,
        T_HINT,T_NOTES,T_VERIFY,T_SETTINGS,T_HELP,T_MORE,T_STRICT,T_RETRY_SAVE};
    for (int i=0;i<PLAY_FOCUS_COUNT;++i) {
      char number[4];snprintf(number,sizeof(number),"%d",i-PLAY_FOCUS_NUMBER_1+1);
      const char *label=i>=PLAY_FOCUS_NUMBER_1?number:
          tr(ui->language,i==PLAY_FOCUS_STRICT&&!ui->strict_mode?T_FREE:keys[i]);
      ui_a11y_add(s,g,A11Y_PLAY+i,play_focus_rect(g,ui,i),label,ACCESSIBILITY_ACTIVATE,
          play_focus_enabled(game,ui,i),i==PLAY_FOCUS_NOTES&&ui->notes_mode);
    }
    int cell=geo.board.w/9;
    for (int i=0;i<81;++i) {
      char label[ACCESSIBILITY_LABEL_LIMIT];
      accessibility_cell_label(label,ui->language,i/9,i%9,game->puzzle[i],game->fixed[i]!=0,
          game->notes[i],ui->sel_r==i/9&&ui->sel_c==i%9,game_has_conflict(game,i/9,i%9));
      ui_a11y_add(s,g,A11Y_CELL+i,(SDL_Rect){geo.board.x+i%9*cell,geo.board.y+i/9*cell,cell,cell},
          label,ACCESSIBILITY_ACTIVATE,true,ui->sel_r==i/9&&ui->sel_c==i%9);
    }
  }
  if (ui->screen==SCR_TITLE || ui->screen==SCR_PLAY)
    for (int i=0;i<LANG_COUNT;++i)
      ui_a11y_add(s,g,A11Y_LANGUAGE+i,language_segment(g,ui,i),language_name((Language)i),
          ACCESSIBILITY_ACTIVATE,true,ui->language==(Language)i);
#undef ADD
}

static void ui_a11y_build(AccessibilitySnapshot *s,const Persistence *p,
                         const Game *game,const UI *ui,const Gfx *g) {
  ui_a11y_build_body(s,p,game,ui,g);
  if (s->valid && s->count && !ui_modal.active && !ui_more_open &&
      !audio_control_popup_open && ui_audio_control_available(ui))
    ui_a11y_audio(s,ui,g);
}

static bool ui_a11y_publish(const AccessibilitySnapshot *s,const Gfx *g) {
#if defined(__ANDROID__)
  JNIEnv *env=(JNIEnv *)SDL_AndroidGetJNIEnv();
  if (!env) return false;
  jobject activity=(jobject)SDL_AndroidGetActivity();
  if (!activity) return false;
  jclass klass=(*env)->GetObjectClass(env,activity);
  jclass string_class=(*env)->FindClass(env,"java/lang/String");
  jmethodID method=klass ? (*env)->GetMethodID(env,klass,"publishSudokuraAccessibility",
      "(JJJIII[I[Ljava/lang/String;Ljava/lang/String;)V") : NULL;
  size_t count=s->valid?s->count:0;
  jintArray nodes=(*env)->NewIntArray(env,(jsize)(count*8));
  jobjectArray labels=string_class?(*env)->NewObjectArray(env,(jsize)count,string_class,NULL):NULL;
  bool sent=false;
  if (method && nodes && labels && !(*env)->ExceptionCheck(env)) {
    for (size_t i=0;i<count && !(*env)->ExceptionCheck(env);++i) {
      const AccessibilityNode *n=&s->nodes[i];
      jint fields[]={n->id,n->x,n->y,n->w,n->h,(jint)n->actions,n->enabled,n->selected};
      (*env)->SetIntArrayRegion(env,nodes,(jsize)(i*8),8,fields);
      jstring label=(*env)->NewStringUTF(env,n->label);
      if (label) {(*env)->SetObjectArrayElement(env,labels,(jsize)i,label);(*env)->DeleteLocalRef(env,label);}
    }
    jstring status=(*env)->NewStringUTF(env,ui_a11y_notice);
    if (status && !(*env)->ExceptionCheck(env)) {
      (*env)->CallVoidMethod(env,activity,method,(jlong)s->binding.generation,
          (jlong)s->binding.attempt_hi,(jlong)s->binding.attempt_lo,s->binding.screen,
          g->width,g->height,nodes,labels,status);
      sent=!(*env)->ExceptionCheck(env);
    }
    if (status) (*env)->DeleteLocalRef(env,status);
  }
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  if (nodes) (*env)->DeleteLocalRef(env,nodes);
  if (labels) (*env)->DeleteLocalRef(env,labels);
  if (string_class) (*env)->DeleteLocalRef(env,string_class);
  if (klass) (*env)->DeleteLocalRef(env,klass);
  (*env)->DeleteLocalRef(env,activity);
  return sent;
#else
  (void)s;(void)g;return true;
#endif
}
static void ui_a11y_current_context(uint64_t out[256],const Persistence *p,const UI *ui) {
  /* Member values, not a hash: identical labels cannot rebind a row to a
     different record, and entering the same modal twice invalidates consent. */
  memset(out,0,256*sizeof(*out));
  size_t n=0;
  out[n++]=ui_modal_generation;out[n++]=ui->pause_reasons;
  out[n++]=ui_modal.serial;out[n++]=audio_control_popup_open;
  if (ui_modal.active) {
    const AppModalBinding *binding=&ui_modal.request.binding;
    out[n++]=ui_modal.request.kind;out[n++]=ui_modal.request.action;
    out[n++]=binding->screen;out[n++]=binding->generation;
    out[n++]=binding->key_hi;out[n++]=binding->key_lo;out[n++]=binding->revision;
  }
  out[n++]=(uint64_t)ui->prev_screen;out[n++]=(uint64_t)ui->session_open;
  out[n++]=ui->is_daily;out[n++]=ui->sel_r;out[n++]=ui->sel_c;
  const ProfileSlot *slot=ui->is_daily?&p->profile.daily:&p->profile.normal;
  out[n++]=slot->revision;
  if (ui->screen==APP_SCREEN_GAMES) {
    out[n++]=games_browser.scroll;out[n++]=games_browser.selected;
    out[n++]=(uint64_t)games_browser.detail_open;
    out[n++]=games_browser.count;out[n++]=(uint64_t)games_browser.scanning;
    size_t start=games_browser.scroll;
    for (size_t i=start;i<games_browser.count && i-start<60;++i) {
      const GameLibraryEntry *entry=&games_browser.rows[i].entry;
      out[n++]=entry->id.high;out[n++]=entry->id.low;out[n++]=entry->revision;
    }
  }
}
static void ui_a11y_refresh(Persistence *p,const Game *game,const UI *ui,const Gfx *g) {
  if (!g->mobile_layout) return;
  AccessibilitySnapshot next;
  ui_a11y_build(&next,p,game,ui,g);
  uint64_t context[256];ui_a11y_current_context(context,p,ui);
  const char *notice=persistence_has_save_error(p)?tr(ui->language,T_SAVE_FAILED):
      ui->toast_on?ui->toast:"";
  bool changed=strcmp(notice,ui_a11y_notice) || !accessibility_snapshot_equal(&next,&ui_a11y_live) ||
      memcmp(context,ui_a11y_context,sizeof(context)) ||
      g->width!=ui_a11y_width || g->height!=ui_a11y_height;
  if (changed) {
    next.binding.generation=ui_a11y_live.binding.generation+1u;
    if (!next.binding.generation) next.binding.generation=1;
    ui_a11y_live=next;memcpy(ui_a11y_context,context,sizeof(context));
    ui_a11y_width=g->width;ui_a11y_height=g->height;
    snprintf(ui_a11y_notice,sizeof(ui_a11y_notice),"%s",notice);
    ui_a11y_publish_pending=true;
  }
  /* Rejection uses current native state even when Java publication fails. */
  if (ui_a11y_publish_pending && ui_a11y_publish(&ui_a11y_live,g))
    ui_a11y_publish_pending=false;
}
static bool ui_a11y_dispatch(Persistence *p,Game *game,UI *ui,Gfx *g,bool *running) {
  AccessibilityCommand command;
  if (!ui_a11y_take(&command)) return false;
  ui_a11y_refresh(p,game,ui,g);
  AccessibilitySnapshot actual;
  ui_a11y_build(&actual,p,game,ui,g);
  if (!accessibility_resolve(&actual,command)) return false;
  int id=command.node_id;
  if (ui_modal.active) {
    if (id>=A11Y_MODAL && id<A11Y_MODAL+(int)ui_modal.request.button_count)
      ui_modal_select((unsigned)(id-A11Y_MODAL));
    else if (id>=A11Y_MODAL_BODY && id<A11Y_MODAL_BODY+12)
      ui_modal_scroll_by(command.action==ACCESSIBILITY_SCROLL_FORWARD?180:-180);
  } else if (id>=A11Y_AUDIO && id<=A11Y_AUDIO_FX) {
    if (id==A11Y_AUDIO) {
      if (command.action==ACCESSIBILITY_LONG_ACTIVATE)
        audio_control_popup_open=!audio_control_popup_open;
      else {audio_control_popup_open=false;audio_toggle(p);}
    } else if (id==A11Y_AUDIO_MUSIC) audio_toggle_music_mute(p);
    else audio_toggle_fx_mute(p);
  } else if (id>=A11Y_CELL && id<A11Y_CELL+81 && app_play_interactions_allowed(ui)) {
    int cell=id-A11Y_CELL;ui->sel_r=cell/9;ui->sel_c=cell%9;
    play_keyboard_focus=-1; /* Focus alone never reaches this action. */
  } else if (id>=A11Y_PLAY && id<A11Y_PLAY+PLAY_FOCUS_COUNT) {
    play_keyboard_focus=id-A11Y_PLAY;(void)activate_play_focus(p,game,ui,g);
  } else if (id>=A11Y_HOME && id<A11Y_HOME+HOME_FOCUS_COUNT) {
    home_focus=id-A11Y_HOME;home_activate(p,game,ui,g,running);
  } else if (id>=A11Y_MODE && id<A11Y_MODE+3) home_set_mode(p,ui,(Mode)(id-A11Y_MODE));
  else if (id>=A11Y_DIFFICULTY && id<A11Y_DIFFICULTY+3)
    home_set_difficulty(p,ui,(GameDifficulty)(id-A11Y_DIFFICULTY));
  else if (id>=A11Y_LANGUAGE && id<A11Y_LANGUAGE+3) home_set_language(p,ui,(Language)(id-A11Y_LANGUAGE));
  else if (id>=A11Y_MORE && id<A11Y_MORE+GEOMETRY_MORE_COUNT)
    (void)ui_more_activate_action(p,game,ui,g,id-A11Y_MORE);
  else if (id==A11Y_MORE_CLOSE) ui_more_close(ui);
  else if (id>=A11Y_PAUSE && id<A11Y_PAUSE+2) {
    pause_keyboard_focus=id-A11Y_PAUSE;activate_pause_focus(p,game,ui,g);
  } else if (id>=A11Y_RESULT && id<A11Y_RESULT+2) {
    result_keyboard_focus=id-A11Y_RESULT;activate_result_focus(p,game,ui,g);
  } else if (id>=A11Y_INFO && id<A11Y_INFO+GEOMETRY_ABOUT_LINK_COUNT+1) {
    info_keyboard_focus=id==A11Y_INFO?INFO_FOCUS_BACK:
        INFO_FOCUS_LINK_0+id-A11Y_INFO-1;
    (void)activate_info_focus(p,game,ui,g);
  } else if ((id>=A11Y_INFO_BODY && id<A11Y_INFO_BODY+60) ||
             id==A11Y_INFO_UP || id==A11Y_INFO_DOWN)
    help_scroll_by(id==A11Y_INFO_UP || command.action==ACCESSIBILITY_SCROLL_BACKWARD?-180:180);
  else if (id>=A11Y_SETTINGS && id<A11Y_SETTINGS+SETTINGS_FOCUS_COUNT) {
    switch (id-A11Y_SETTINGS) {
      case SETTINGS_FOCUS_THEME:ui_settings_toggle_theme();break;
      case SETTINGS_FOCUS_LANGUAGE:ui_settings_cycle_language(1);break;
      case SETTINGS_FOCUS_SOUND:audio_toggle(p);break;
      case SETTINGS_FOCUS_REDUCED_MOTION:ui_settings_toggle_reduced_motion();break;
      case SETTINGS_FOCUS_AUTO_NOTES:ui_settings_toggle_auto_notes();break;
      case SETTINGS_FOCUS_BACK:ui_settings_close(p,game,ui);break;
      default:break;
    }
  } else if (id>=A11Y_MUSIC_MINUS && id<=A11Y_FX_PLUS) {
    int delta=id%2?5:-5;
    if (id<A11Y_FX_MINUS) ui_audio_set_music_level(audio_music_volume()+delta);
    else ui_audio_set_fx_level(audio_fx_volume()+delta);
  } else if (id>=A11Y_HINT && id<A11Y_HINT+HINT_FOCUS_COUNT) {
    hint_keyboard_focus=(HintKeyboardFocus)(id-A11Y_HINT);(void)activate_hint_focus(p,game,ui);
  } else if (id>=A11Y_LIBRARY && id<A11Y_LIBRARY+10) {
    games_browser.focus=id-A11Y_LIBRARY;(void)games_browser_activate_focus(p,game,ui,g);
  } else if (id>=A11Y_ROW && id<A11Y_ROW+60) {
    games_browser_select(p,g,games_browser.scroll+(size_t)(id-A11Y_ROW));
  }
  g->dirty=true;return true;
}

static bool ui_a11y_extended_smoke(Persistence *p,Game *game,UI *ui,Gfx *g) {
  UI saved_ui=*ui;
  GamesBrowser saved_browser=games_browser;
  Persistence sandbox=*p;sandbox.enabled=false; /* No fixture disk writes. */
  bool saved_popup=audio_control_popup_open;
  bool saved_settings=settings_open,saved_more=ui_more_open;
  bool enabled=audio_is_enabled(),music=audio_music_muted(),fx=audio_fx_muted();
  bool running=true,ok=false;
  settings_open=false;ui_more_open=false;audio_control_popup_open=false;
  ui->screen=SCR_TITLE;ui->pause_reasons=0;ui->hint_preview_active=false;
  ui_a11y_refresh(&sandbox,game,ui,g);
  AccessibilityCommand command={ui_a11y_live.binding,A11Y_AUDIO,ACCESSIBILITY_LONG_ACTIVATE};
  if (!ui_a11y_submit(command) || !ui_a11y_dispatch(&sandbox,game,ui,g,&running) ||
      !audio_control_popup_open) goto done;
  ui_a11y_refresh(&sandbox,game,ui,g);
  if (!ui_a11y_live.valid || ui_a11y_live.count!=3) goto done;
  command=(AccessibilityCommand){ui_a11y_live.binding,A11Y_AUDIO_MUSIC,ACCESSIBILITY_ACTIVATE};
  if (!ui_a11y_submit(command) || !ui_a11y_dispatch(&sandbox,game,ui,g,&running) ||
      audio_music_muted()==music) goto done;
  ui_a11y_refresh(&sandbox,game,ui,g);
  command=(AccessibilityCommand){ui_a11y_live.binding,A11Y_AUDIO_FX,ACCESSIBILITY_ACTIVATE};
  if (!ui_a11y_submit(command) || !ui_a11y_dispatch(&sandbox,game,ui,g,&running) ||
      audio_fx_muted()==fx) goto done;
  ui_a11y_refresh(&sandbox,game,ui,g);
  command=(AccessibilityCommand){ui_a11y_live.binding,A11Y_AUDIO,ACCESSIBILITY_ACTIVATE};
  audio_control_popup_open=false; /* Dismissed before the queued action. */
  if (!ui_a11y_submit(command) || ui_a11y_dispatch(&sandbox,game,ui,g,&running) ||
      audio_is_enabled()!=enabled) goto done;

  GamesRow fixture={0};
  fixture.entry.id=(GameAttemptId){501,502};fixture.entry.revision=7;
  fixture.entry.status=STORE_OK;fixture.entry.kind=GAME_RECORD_EXACT;
  fixture.entry.result.status=SESSION_WON;
  fixture.entry.result.mode=MODE_CLASSIC;
  fixture.entry.result.difficulty=DIFFICULTY_MEDIUM;
  memset(&games_browser,0,sizeof(games_browser));
  games_browser.rows=&fixture;games_browser.count=games_browser.capacity=1;
  games_browser.preview.game=*game;games_browser.preview_valid=true;
  ui->screen=APP_SCREEN_GAMES;
  for (int detail=0;detail<2;++detail) {
    games_browser.detail_open=detail!=0;
    app_render_ui(g,game,ui);ui_a11y_refresh(&sandbox,game,ui,g);
    if (!ui_a11y_live.valid) goto done;
    unsigned cells=0;
    for (size_t i=0;i<ui_a11y_live.count;++i) {
      const AccessibilityNode *node=&ui_a11y_live.nodes[i];
      if (node->id>=A11Y_PREVIEW_CELL && node->id<A11Y_PREVIEW_CELL+81) {
        ++cells;if (node->actions || node->selected) goto done;
      }
    }
    if (games_browser_semantic_board.valid && cells!=81) goto done;
    if (detail && cells!=81) goto done;
    AccessibilitySnapshot before=ui_a11y_live,poisoned;
    for (int i=0;i<81;++i)
      games_browser.preview.game.solution[i]=(games_browser.preview.game.solution[i]%9)+1;
    ui_a11y_build(&poisoned,&sandbox,game,ui,g);
    if (!accessibility_snapshot_equal(&before,&poisoned)) goto done;
    command=(AccessibilityCommand){ui_a11y_live.binding,A11Y_PREVIEW_CELL,ACCESSIBILITY_ACTIVATE};
    if (!ui_a11y_submit(command) || ui_a11y_dispatch(&sandbox,game,ui,g,&running) ||
        games_browser.selected!=0 || games_browser.detail_open!=(detail!=0)) goto done;
    /* Changed record metadata cannot reuse the old painted preview bounds. */
    ++fixture.entry.revision;ui_a11y_refresh(&sandbox,game,ui,g);
    for (size_t i=0;i<ui_a11y_live.count;++i)
      if (ui_a11y_live.nodes[i].id>=A11Y_PREVIEW_CELL &&
          ui_a11y_live.nodes[i].id<A11Y_PREVIEW_CELL+81) goto done;
  }
  ok=true;
done:
  audio_set_enabled(enabled);audio_set_music_muted(music);audio_set_fx_muted(fx);
  audio_control_popup_open=saved_popup;settings_open=saved_settings;ui_more_open=saved_more;
  games_browser=saved_browser;games_browser_semantic_board.valid=false;*ui=saved_ui;
  if (!ok) fprintf(stderr,"semantic audio/readonly-preview regression at %dx%d\n",g->width,g->height);
  return ok;
}

static bool ui_a11y_smoke(Persistence *p,Game *game,UI *ui,Gfx *g) {
  if (!ui_a11y_extended_smoke(p,game,ui,g)) return false;
  UI saved=*ui;
  Game original=*game;
  bool saved_settings=settings_open,saved_more=ui_more_open;
  bool running=true,ok=false;
  settings_open=false;ui_more_open=false;
  ui->screen=SCR_PLAY;ui->session_open=true;ui->result=RES_NONE;ui->mode=MODE_CLASSIC;
  ui->pause_reasons=0;ui->hint_preview_active=false;
  ui_a11y_refresh(p,game,ui,g);
  if (!ui_a11y_live.valid) goto done;
  for (int i=0;i<81;++i) {
    AccessibilityCommand test={ui_a11y_live.binding,A11Y_CELL+i,ACCESSIBILITY_ACTIVATE};
    if (!accessibility_resolve(&ui_a11y_live,test)) goto done;
  }
  /* Changing every hidden answer cannot change Classic/Daily semantics. */
  AccessibilitySnapshot before=ui_a11y_live,poisoned;
  for (int i=0;i<81;++i) game->solution[i]=(game->solution[i]%9)+1;
  ui_a11y_build(&poisoned,p,game,ui,g);
  *game=original;
  if (!accessibility_snapshot_equal(&before,&poisoned)) goto done;
  AccessibilityCommand command={ui_a11y_live.binding,A11Y_CELL+80,ACCESSIBILITY_ACTIVATE};
  if (!ui_a11y_submit(command) || ui_a11y_submit(command) ||
      !ui_a11y_dispatch(p,game,ui,g,&running) ||
      ui->sel_r!=8 || ui->sel_c!=8 || memcmp(game,&original,sizeof(original))) goto done;
  ui_a11y_refresh(p,game,ui,g);
  command=(AccessibilityCommand){ui_a11y_live.binding,A11Y_CELL,ACCESSIBILITY_ACTIVATE};
  ui->pause_reasons=PAUSE_MANUAL;
  if (!ui_a11y_submit(command) || ui_a11y_dispatch(p,game,ui,g,&running)) goto done;
  for (size_t i=0;i<ui_a11y_live.count;++i)
    if (ui_a11y_live.nodes[i].id>=A11Y_CELL && ui_a11y_live.nodes[i].id<A11Y_CELL+81) goto done;
  ui->pause_reasons=0;
  for (int language=0;language<LANG_COUNT;++language) {
    ui->language=(Language)language;
    const int screens[]={SCR_TITLE,SCR_HELP,SCR_ABOUT,APP_SCREEN_SETTINGS,APP_SCREEN_GAMES,SCR_END};
    for (unsigned i=0;i<sizeof(screens)/sizeof(screens[0]);++i) {
      ui->screen=screens[i];settings_open=ui->screen==APP_SCREEN_SETTINGS;
      app_render_ui(g,game,ui);
      ui_a11y_refresh(p,game,ui,g);
      if (!ui_a11y_live.valid || !ui_a11y_live.count) goto done;
      if (ui->screen==SCR_ABOUT) {
        bool found=false;
        for (size_t a=0;a<ui_a11y_live.count;++a) {
          const AccessibilityNode *node=&ui_a11y_live.nodes[a];
          if (node->id==A11Y_INFO+GEOMETRY_ABOUT_LINK_COUNT &&
              strcmp(node->label,tr(ui->language,T_PRIVACY_POLICY))==0 &&
              (node->actions&ACCESSIBILITY_ACTIVATE)) found=true;
        }
        if (!found) goto done;
      }
      for (size_t a=0;a<ui_a11y_live.count;++a) {
        const AccessibilityNode *x=&ui_a11y_live.nodes[a];
        if (x->id<A11Y_INFO_BODY || x->id>=A11Y_INFO_BODY+16) continue;
        for (size_t b=a+1;b<ui_a11y_live.count;++b) {
          const AccessibilityNode *y=&ui_a11y_live.nodes[b];
          if (y->id<A11Y_INFO_BODY || y->id>=A11Y_INFO_BODY+16) continue;
          SDL_Rect rx={x->x,x->y,x->w,x->h},ry={y->x,y->y,y->w,y->h};
          if (SDL_HasIntersection(&rx,&ry)) goto done;
        }
      }
    }
  }
  settings_open=false;ui->screen=SCR_PLAY;ui->result=RES_NONE;
  ui->pause_reasons=0;
  ui_a11y_refresh(p,game,ui,g);
  command=(AccessibilityCommand){ui_a11y_live.binding,A11Y_CELL,ACCESSIBILITY_ACTIVATE};
  ui_more_open_panel(ui);ui_a11y_refresh(p,game,ui,g);
  if (!ui_a11y_live.valid || ui_a11y_live.count!=GEOMETRY_MORE_COUNT+2 ||
      !ui_a11y_submit(command) || ui_a11y_dispatch(p,game,ui,g,&running)) goto done;
  for (size_t i=0;i<ui_a11y_live.count;++i)
    if (ui_a11y_live.nodes[i].id>=A11Y_CELL && ui_a11y_live.nodes[i].id<A11Y_CELL+81) goto done;
  ui_more_close(ui);
  if (!ui_modal_message(g,ui,T_ABOUT,T_ABOUT_BODY)) goto done;
  ui_a11y_refresh(p,game,ui,g);
  command=(AccessibilityCommand){ui_a11y_live.binding,A11Y_MODAL,ACCESSIBILITY_ACTIVATE};
  ui_modal_cancel_current();
  if (!ui_modal_message(g,ui,T_ABOUT,T_ABOUT_BODY) || !ui_a11y_submit(command) ||
      ui_a11y_dispatch(p,game,ui,g,&running) || !ui_modal.active) goto done;
  ui_modal_cancel_current();
  app_suspended=true;ui_a11y_refresh(p,game,ui,g);
  if (ui_a11y_live.count) goto done;
  app_suspended=false;
  ok=true;
done:
  app_suspended=false;
  ui_modal_cancel_current();ui_more_close(ui);*ui=saved;*game=original;
  settings_open=saved_settings;ui_more_open=saved_more;
  if (!ok) fprintf(stderr,"semantic owner projection/dispatch regression at %dx%d\n",g->width,g->height);
  return ok;
}
