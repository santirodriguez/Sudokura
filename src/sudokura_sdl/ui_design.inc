/* Shared visual vocabulary, adopted first by Home and About. Rendering only:
   the established geometry, input owner, semantics and save gates stay intact. */
#include "ui_design.h"
#include "sudokura_sdl/ui_design_draw.h"

static SDL_Rect design_inset(SDL_Rect r,int amount) {
  if (r.w<=2*amount || r.h<=2*amount) return r;
  return (SDL_Rect){r.x+amount,r.y+amount,r.w-2*amount,r.h-2*amount};
}
static bool design_focus_visible(const Gfx *gfx) {
  return !gfx->mobile_layout || ui_keyboard_focus_visible;
}
static void design_background(Gfx *gfx,UiDesignTheme t) {
  SDL_Vertex v[4]={
    {{0,0},ui_design_sdl_color(t.background_top),{0,0}},
    {{(float)gfx->width,0},ui_design_sdl_color(t.background_bottom),{0,0}},
    {{(float)gfx->width,(float)gfx->height},ui_design_sdl_color(t.background_top),{0,0}},
    {{0,(float)gfx->height},ui_design_sdl_color(t.background_bottom),{0,0}}};
  const int indices[]={0,1,2,0,2,3};
  SDL_SetRenderDrawBlendMode(gfx->ren,SDL_BLENDMODE_NONE);
  if(SDL_RenderGeometry(gfx->ren,NULL,v,4,indices,6)!=0) {
    SDL_Color c=ui_design_sdl_color(t.background_bottom);
    SDL_SetRenderDrawColor(gfx->ren,c.r,c.g,c.b,c.a);
    SDL_RenderClear(gfx->ren);
  }
  SDL_SetRenderDrawBlendMode(gfx->ren,SDL_BLENDMODE_BLEND);
}
static void design_card(Gfx *gfx,UiDesignTheme t,SDL_Rect rect) {
  SDL_Rect shadow=rect;shadow.y+=2;
  ui_design_round(gfx->ren,(SDL_FRect){(float)shadow.x,(float)shadow.y,
      (float)shadow.w,(float)shadow.h},UI_DESIGN_CARD_RADIUS,t.shadow,t.shadow);
  ui_design_panel(gfx->ren,rect,UI_DESIGN_CARD_RADIUS,
      ui_design_mix(t.surface_top,t.surface,110),t.surface,t.border);
}
static SDL_Rect design_button_face(Gfx *gfx,UiDesignTheme t,SDL_Rect rect,
    UiDesignRole role,bool focused,bool disabled,UiDesignButton *out) {
  int x=0,y=0;SDL_GetMouseState(&x,&y);
  bool latched=button_feedback_matches(&ui_button_feedback,
      ui_feedback_render_screen,SDL_GetTicks64(),rect.x,rect.y,rect.w,rect.h);
  UiDesignState state={!gfx->mobile_layout && point_in(rect,x,y),
      ui_pointer_pressed(rect) || latched,focused,disabled};
  UiDesignButton b=ui_design_button_style(t,role,state);
  ui_design_panel(gfx->ren,rect,UI_DESIGN_BUTTON_RADIUS,b.top,b.bottom,b.border);
  if(b.focus) {
    SDL_Rect ring=design_inset(rect,2);
    ui_design_panel(gfx->ren,ring,UI_DESIGN_BUTTON_RADIUS-2,
                    b.top,b.bottom,t.focus);
  }
  if(out)*out=b;
  rect.y+=b.displacement;rect.h-=b.displacement;
  return rect;
}
static bool design_fit(Gfx *gfx,const char *text,UiDesignColor color,
                       SDL_Rect box,int preferred,int minimum,bool centered) {
  return polish_draw_wrapped_fit(gfx,text,ui_design_sdl_color(color),box,
                                 preferred,minimum,centered);
}
static void design_button(Gfx *gfx,UiDesignTheme t,SDL_Rect rect,
    const char *label,const char *detail,UiDesignIcon icon,UiDesignRole role,
    bool focused,bool disabled,int preferred) {
  if(rect.w<=0 || rect.h<=0)return;
  UiDesignButton style;
  SDL_Rect face=design_button_face(gfx,t,rect,role,focused,disabled,&style);
  UiDesignLabel layout=ui_design_label_layout(face.w,face.h,icon!=UI_DESIGN_ICON_NONE);
  SDL_Rect text={face.x+layout.text.x,face.y+3,layout.text.w,face.h-6};
  if(layout.icon.w>0) {
    SDL_Rect glyph={face.x+layout.icon.x,face.y+layout.icon.y,
                    layout.icon.w,layout.icon.h};
    ui_design_icon(gfx->ren,glyph,icon,style.text);
  }
  int minimum=gfx->mobile_layout ? 13 : 10;
  if(detail && detail[0] && text.h>=36) {
    int title_h=text.h*55/100;
    SDL_Rect title={text.x,text.y,text.w,title_h};
    SDL_Rect subtitle={text.x,text.y+title_h,text.w,text.h-title_h};
    (void)design_fit(gfx,label,style.text,title,preferred,minimum,true);
    UiDesignColor subdued=role==UI_DESIGN_PRIMARY ? style.text : t.muted;
    if(disabled)subdued=t.disabled;
    (void)design_fit(gfx,detail,subdued,subtitle,13,10,true);
  } else {
    (void)design_fit(gfx,label,style.text,text,preferred,minimum,true);
  }
}
static void design_back(Gfx *gfx,UI *ui,UiDesignTheme t) {
  SDL_Rect rect=back_rect(gfx,ui);
  UiDesignButton style;
  SDL_Rect face=design_button_face(gfx,t,rect,UI_DESIGN_PRIMARY,
      design_focus_visible(gfx) && info_keyboard_focus==INFO_FOCUS_BACK,false,&style);
  int tw=0,th=0;
  SDL_Texture *text=render_text(gfx,cached_font(gfx,18),tr(ui->language,T_BACK),
                                ui_design_sdl_color(style.text),&tw,&th);
  int side=22,gap=10,width=tw+side+gap;
  if(text && width<=face.w-20 && th<=face.h-4) {
    int x=face.x+(face.w-width)/2;
    ui_design_icon(gfx->ren,(SDL_Rect){x,face.y+(face.h-side)/2,side,side},
                   UI_DESIGN_ICON_BACK,style.text);
    SDL_Rect dst={x+side+gap,face.y+(face.h-th)/2,tw,th};
    SDL_RenderCopy(gfx->ren,text,NULL,&dst);
  } else {
    (void)design_fit(gfx,tr(ui->language,T_BACK),style.text,design_inset(face,4),
                     18,gfx->mobile_layout?13:10,true);
  }
}
static void design_languages(Gfx *gfx,UI *ui,UiDesignTheme t) {
  AppGeometry geo;
  if(!get_geometry(gfx,ui,&geo))return;
  GeometryFonts fonts=geometry_font_sizes(&geo,gfx->width,gfx->height);
  for(int i=0;i<LANG_COUNT;++i) {
    SDL_Rect outer=language_segment(gfx,ui,i),rect=design_inset(outer,2);
    bool active=i==(int)ui->language;
    UiDesignButton style;
    SDL_Rect face=design_button_face(gfx,t,rect,
        active?UI_DESIGN_PRIMARY:UI_DESIGN_QUIET,
        design_focus_visible(gfx)&&home_focus==HOME_FOCUS_LANGUAGE&&active,false,&style);
    int h=face.h-12,cap=face.h>=42?26:20;
    if(h>cap)h=cap;
    if(h<12)h=12;
    SDL_Rect flag={face.x+7,face.y+(face.h-h)/2,h*4/3,h};
    if(language_flags[i])SDL_RenderCopy(gfx->ren,language_flags[i],NULL,&flag);
    SDL_Rect label={flag.x+flag.w+5,face.y,face.x+face.w-flag.x-flag.w-8,face.h};
    if(label.w<24)label=design_inset(face,4);
    (void)draw_fitted_text(gfx,language_name((Language)i),
        ui_design_sdl_color(style.text),label,fonts.help,10);
  }
}
static void design_home(Gfx *gfx,UI *ui) {
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  AppGeometry a;
  if(!get_geometry(gfx,ui,&a))return;
  GeometryFonts f=geometry_font_sizes(&a,gfx->width,gfx->height);
  bool focus=design_focus_visible(gfx);
  design_background(gfx,t);
  SDL_Rect first=sdl_rect(a.home_mode[0]),last=sdl_rect(a.home_primary[0]);
  int left=first.x-8,top=a.home_new_label.y-6;
  if(left<3)left=3;
  if(top<0)top=first.y-6;
  SDL_Rect setup={left,top,gfx->width-2*left,last.y+last.h-top+6};
  int right=a.home_primary[0].x+a.home_primary[0].w+8;
  if(right<gfx->width-3)setup.w=right-setup.x;
  if(setup.h>0)design_card(gfx,t,setup);
  draw_wordmark(gfx,sdl_rect(a.home_logo));
  bool captions=!gfx->mobile_layout || a.home_new_label.h>=18;
  if(captions)(void)draw_fitted_text(gfx,tr(ui->language,T_NEW_GAME),
      ui_design_sdl_color(t.accent),sdl_rect(a.home_new_label),f.help,10);
  (void)draw_fitted_text(gfx,tr(ui->language,T_MODE),ui_design_sdl_color(t.muted),
                         sdl_rect(a.home_mode_label),f.help,10);
  for(int i=0;i<GEOMETRY_HOME_SEGMENT_COUNT;++i) {
    bool selected=i==(int)ui->mode;
    design_button(gfx,t,sdl_rect(a.home_mode[i]),tr(ui->language,mode_key((Mode)i)),
        NULL,UI_DESIGN_ICON_NONE,selected?UI_DESIGN_PRIMARY:UI_DESIGN_SECONDARY,
        focus&&home_focus==HOME_FOCUS_MODE&&selected,false,f.control);
  }
  (void)draw_fitted_text(gfx,tr(ui->language,T_DIFFICULTY),ui_design_sdl_color(t.muted),
                         sdl_rect(a.home_difficulty_label),f.help,10);
  for(int i=0;i<GEOMETRY_HOME_SEGMENT_COUNT;++i) {
    bool selected=i==(int)home_difficulty;
    design_button(gfx,t,sdl_rect(a.home_difficulty[i]),
        difficulty_name(ui->language,(GameDifficulty)i),NULL,UI_DESIGN_ICON_NONE,
        selected?UI_DESIGN_PRIMARY:UI_DESIGN_SECONDARY,
        focus&&home_focus==HOME_FOCUS_DIFFICULTY&&selected,false,f.control);
  }
  design_button(gfx,t,sdl_rect(a.home_primary[0]),tr(ui->language,T_START_NEW_GAME),
      NULL,UI_DESIGN_ICON_PLAY,UI_DESIGN_PRIMARY,focus&&home_focus==HOME_FOCUS_NEW,
      false,f.control);
  if(captions)(void)draw_fitted_text(gfx,tr(ui->language,T_CONTINUE),
      ui_design_sdl_color(t.accent),sdl_rect(a.home_continue_label),f.help,10);
  char detail[128]="";
  if(ui->normal_resume.can_continue) {
    unsigned long long s=(unsigned long long)(ui->normal_resume.elapsed_ms/1000u);
    snprintf(detail,sizeof(detail),"%s \xC2\xB7 %s \xC2\xB7 %02llu:%02llu",
        tr(ui->language,mode_key(ui->normal_resume.mode)),
        difficulty_name(ui->language,ui->normal_resume.difficulty),s/60u,s%60u);
  }
  design_button(gfx,t,sdl_rect(a.home_primary[1]),tr(ui->language,T_CONTINUE_NORMAL),
      detail,UI_DESIGN_ICON_PLAY,UI_DESIGN_SECONDARY,
      focus&&home_focus==HOME_FOCUS_CONTINUE,!ui->normal_resume.can_continue,f.control);
  const char *daily=tr(ui->language,T_DAILY);
  if(ui->daily_resume.can_continue) {
    daily=tr(ui->language,T_CONTINUE_DAILY);
    unsigned long long s=(unsigned long long)(ui->daily_resume.elapsed_ms/1000u);
    snprintf(detail,sizeof(detail),"%04d-%02d-%02d \xC2\xB7 %s \xC2\xB7 %02llu:%02llu",
        ui->daily_resume.year,ui->daily_resume.month,ui->daily_resume.day,
        difficulty_name(ui->language,ui->daily_resume.difficulty),s/60u,s%60u);
  } else if(ui->daily_resume.completed&&ui->daily_resume.today) {
    daily=tr(ui->language,T_DAILY_COMPLETE);
    unsigned long long s=(unsigned long long)(ui->daily_resume.elapsed_ms/1000u);
    snprintf(detail,sizeof(detail),"%04d-%02d-%02d \xC2\xB7 %02llu:%02llu",
        ui->daily_resume.year,ui->daily_resume.month,ui->daily_resume.day,s/60u,s%60u);
  } else {
    snprintf(detail,sizeof(detail),"%s \xC2\xB7 %s",tr(ui->language,T_CLASSIC),
              tr(ui->language,T_MEDIUM));
  }
  if(daily_lookup_scanning) {
    daily=tr(ui->language,T_DAILY_CHECKING);
    snprintf(detail,sizeof(detail),"%s",tr(ui->language,T_DAILY_CHECK_CANCEL));
  }
  design_button(gfx,t,sdl_rect(a.home_primary[2]),daily,detail,UI_DESIGN_ICON_GRID,
      UI_DESIGN_SECONDARY,focus&&home_focus==HOME_FOCUS_DAILY,false,f.control);
  design_button(gfx,t,home_collection_rect(&a,false),tr(ui->language,T_GAMES),NULL,
      UI_DESIGN_ICON_FOLDER,UI_DESIGN_SECONDARY,focus&&home_focus==HOME_FOCUS_GAMES,
      false,f.control);
  detail[0]=0;
  if(journey_home_summary.available) {
    const ProfileResult *latest=&journey_home_summary.latest;
    unsigned long long s=latest->elapsed_ms/1000u;
    snprintf(detail,sizeof(detail),"%02llu:%02llu \xC2\xB7 %u %s",s/60u,s%60u,
              latest->mistakes,tr(ui->language,T_ERRORS));
  }
  design_button(gfx,t,home_collection_rect(&a,true),tr(ui->language,T_JOURNEY),
      detail,UI_DESIGN_ICON_TROPHY,UI_DESIGN_CELEBRATION,
      focus&&home_focus==HOME_FOCUS_JOURNEY,false,f.control);
  const TextKey keys[]={T_HELP,T_ABOUT,T_SETTINGS,T_QUIT};
  const UiDesignIcon icons[]={UI_DESIGN_ICON_HELP,UI_DESIGN_ICON_INFO,
                             UI_DESIGN_ICON_SETTINGS,UI_DESIGN_ICON_EXIT};
  for(int i=1;i<GEOMETRY_HOME_SECONDARY_COUNT;++i)
    design_button(gfx,t,sdl_rect(a.home_secondary[i]),tr(ui->language,keys[i-1]),
        NULL,icons[i-1],UI_DESIGN_QUIET,
        focus&&home_focus==HOME_FOCUS_GAMES+i,false,f.control);
  design_languages(gfx,ui,t);
}

static void design_about_actions(Gfx *gfx,UI *ui,UiDesignTheme t) {
  const TextKey keys[]={T_REPOSITORY,T_WEBSITE,T_SUPPORT,T_PRIVACY_POLICY};
  const UiDesignIcon icons[]={UI_DESIGN_ICON_REPOSITORY,UI_DESIGN_ICON_GLOBE,
                             UI_DESIGN_ICON_HEART,UI_DESIGN_ICON_SHIELD};
  for(int i=0;i<GEOMETRY_ABOUT_LINK_COUNT;++i)
    design_button(gfx,t,ui_info_link_rect(gfx,ui,i),tr(ui->language,keys[i]),
        NULL,icons[i],UI_DESIGN_SECONDARY,
        design_focus_visible(gfx)&&info_keyboard_focus==INFO_FOCUS_LINK_0+i,false,18);
  if(help_scroll_max_px>0 && gfx->mobile_layout) {
    for(int i=0;i<2;++i) {
      SDL_Rect rect=ui_info_scroll_rect(gfx,ui,i!=0);
      bool enabled=i ? help_scroll_px<help_scroll_max_px : help_scroll_px>0;
      UiDesignButton style;
      design_button_face(gfx,t,rect,UI_DESIGN_QUIET,false,!enabled,&style);
      SDL_Rect icon={rect.x+(rect.w-24)/2,rect.y+(rect.h-24)/2,24,24};
      ui_design_icon(gfx->ren,icon,i?UI_DESIGN_ICON_DOWN:UI_DESIGN_ICON_UP,style.text);
    }
  }
  design_back(gfx,ui,t);
}

/* Mobile cards are measured from cached real font textures, not fixed heights. */
static bool design_about_mobile(Gfx *gfx,UI *ui,GeometryInfo layout) {
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  SDL_Rect body=sdl_rect(layout.body);
  const char *copy[]={SUDOKURA_NAME_VERSION,tr(ui->language,T_ABOUT_BODY),
      tr(ui->language,T_ABOUT_FACT_TITLE),tr(ui->language,T_ABOUT_FACT),
      tr(ui->language,T_ABOUT_STUDY_TITLE),tr(ui->language,T_ABOUT_STUDY),
      tr(ui->language,T_ABOUT_STUDY_REFERENCE),tr_publisher_credit(ui->language),
      tr(ui->language,T_ABOUT_STACK)};
  SDL_Texture *textures[9]={0};
  SDL_Rect text[9]={{0}},cards[3]={{0}},icons[3]={{0}};
  int tw[9]={0},th[9]={0};
  int width=body.w-12,total=8;
  if(width<140 || body.h<1)return false;
  for(int i=0;i<2;++i) {
    int font=i ? 18 : 28;
    textures[i]=render_text_wrapped(gfx,cached_font(gfx,font),copy[i],
        ui_design_sdl_color(i?t.muted:t.text),width-16,&tw[i],&th[i]);
    if(!textures[i])return false;
    text[i]=(SDL_Rect){6+(width-tw[i])/2,total,tw[i],th[i]};
    total+=th[i]+(i?18:8);
  }
  const int first[]={2,4,7},last[]={3,6,8};
  bool column=width>=380;
  for(int card=0;card<3;++card) {
    int top=total,y=top+14;
    for(int i=first[card];i<=last[card];++i) {
      bool heading=i==first[card];
      int inset=column?62:(heading?48:14);
      int available=width-inset-14;
      int font=(i==6||i==8)?14:(i==7?16:18);
      UiDesignColor color=(i==2||i==4)?t.accent:(i==6||i==8)?t.muted:t.text;
      textures[i]=render_text_wrapped(gfx,cached_font(gfx,font),copy[i],
          ui_design_sdl_color(color),available,&tw[i],&th[i]);
      if(!textures[i])return false;
      text[i]=(SDL_Rect){6+inset,y,tw[i],th[i]};
      y+=th[i]+(i<last[card]?8:0);
    }
    cards[card]=(SDL_Rect){6,top,width,y-top+14};
    int side=column?32:24;
    icons[card]=(SDL_Rect){20,column?top+(cards[card].h-side)/2:top+14,side,side};
    total=y+14+12;
  }
  help_scroll_max_px=total>body.h?total-body.h:0;
  help_scroll_set(help_scroll_px);
  design_background(gfx,t);
  SDL_Rect logo=sdl_rect(layout.heading);
  logo.y-=8;logo.h+=16;
  draw_wordmark(gfx,logo);
  SDL_Rect old_clip;SDL_RenderGetClipRect(gfx->ren,&old_clip);
  SDL_bool clipped=SDL_RenderIsClipEnabled(gfx->ren);
  SDL_RenderSetClipRect(gfx->ren,&body);
  const UiDesignIcon glyphs[]={UI_DESIGN_ICON_GRID,UI_DESIGN_ICON_BRAIN,UI_DESIGN_ICON_INFO};
  for(int i=0;i<3;++i) {
    SDL_Rect card=cards[i];card.x+=body.x;card.y+=body.y-help_scroll_px;
    if(card.y+card.h<body.y || card.y>body.y+body.h)continue;
    design_card(gfx,t,card);
    SDL_Rect icon=icons[i];icon.x+=body.x;icon.y+=body.y-help_scroll_px;
    ui_design_icon(gfx->ren,icon,glyphs[i],t.accent);
  }
  ui_info_semantic_begin(gfx,ui);
  for(int i=0;i<9;++i) {
    SDL_Rect viewport={body.x+text[i].x,body.y,text[i].w,body.h};
    ui_info_semantic_add(viewport,text[i].y,text[i].h,copy[i],i);
    help_draw_scrolled_texture(gfx,textures[i],viewport,text[i].y,
                               tw[i],th[i],help_scroll_px);
  }
  SDL_RenderSetClipRect(gfx->ren,clipped?&old_clip:NULL);
  design_about_actions(gfx,ui,t);
  return true;
}
static void design_desktop_info_card(Gfx *gfx,UiDesignTheme t,SDL_Rect card,
    const char *title,const char *copy,const char *reference,UiDesignIcon icon) {
  if(card.w<=0 || card.h<=0)return;
  design_card(gfx,t,card);
  bool compact=card.h<90;
  int inset=compact?8:14,side=compact?24:32;
  int left=side+inset*2;
  if(card.w<360)left=inset;
  if(left>inset)ui_design_icon(gfx->ren,
      (SDL_Rect){card.x+inset,card.y+(card.h-side)/2,side,side},icon,t.accent);
  SDL_Rect area={card.x+left,card.y+inset,card.w-left-inset,card.h-2*inset};
  if(area.h<20)area=(SDL_Rect){card.x+left,card.y+3,card.w-left-inset,card.h-6};
  int title_h=compact?16:24,ref_h=reference?(compact?14:18):0;
  SDL_Rect heading={area.x,area.y,area.w,title_h};
  (void)design_fit(gfx,title,t.accent,heading,compact?14:18,10,false);
  SDL_Rect paragraph={area.x,area.y+title_h+2,area.w,area.h-title_h-ref_h-2};
  (void)design_fit(gfx,copy,t.text,paragraph,compact?13:16,10,false);
  if(reference) {
    SDL_Rect ref={area.x,area.y+area.h-ref_h,area.w,ref_h};
    (void)design_fit(gfx,reference,t.muted,ref,14,10,false);
  }
}
static bool design_about(Gfx *gfx,UI *ui) {
  GeometryInfo mobile;
  if(ui_mobile_info_layout(gfx,ui,&mobile))return design_about_mobile(gfx,ui,mobile);
  AppGeometry a;
  if(!get_geometry(gfx,ui,&a))return false;
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  design_background(gfx,t);
  draw_wordmark(gfx,sdl_rect(a.about_logo));
  SDL_Rect intro=sdl_rect(a.about_body);
  int title_h=intro.h/2;
  (void)design_fit(gfx,SUDOKURA_NAME_VERSION,t.text,
      (SDL_Rect){intro.x,intro.y,intro.w,title_h},28,12,true);
  (void)design_fit(gfx,tr(ui->language,T_ABOUT_BODY),t.muted,
      (SDL_Rect){intro.x,intro.y+title_h,intro.w,intro.h-title_h},18,10,true);
  design_desktop_info_card(gfx,t,sdl_rect(a.about_fact),
      tr(ui->language,T_ABOUT_FACT_TITLE),tr(ui->language,T_ABOUT_FACT),NULL,
      UI_DESIGN_ICON_GRID);
  design_desktop_info_card(gfx,t,sdl_rect(a.about_study),
      tr(ui->language,T_ABOUT_STUDY_TITLE),tr(ui->language,T_ABOUT_STUDY),
      tr(ui->language,T_ABOUT_STUDY_REFERENCE),UI_DESIGN_ICON_BRAIN);
  SDL_Rect credits=sdl_rect(a.about_credits);
  design_card(gfx,t,credits);
  int h=credits.h/2;
  (void)design_fit(gfx,tr_publisher_credit(ui->language),t.text,
      (SDL_Rect){credits.x+8,credits.y,credits.w-16,h},16,10,true);
  (void)design_fit(gfx,tr(ui->language,T_ABOUT_STACK),t.muted,
      (SDL_Rect){credits.x+8,credits.y+h,credits.w-16,credits.h-h},14,10,true);
  design_about_actions(gfx,ui,t);
  return true;
}
static void design_audio(Gfx *gfx,UI *ui) {
  if(!ui_audio_control_reconcile(ui))return;
  UiDesignTheme t=ui_design_theme(ui->dark_theme);
  AudioControlGeometry geo=ui_audio_control_geometry(gfx,ui);
  SDL_Rect rect=sdl_rect(geo.button);
  UiDesignButton style;
  design_button_face(gfx,t,rect,UI_DESIGN_SECONDARY,false,false,&style);
  int side=rect.h-16;if(side>28)side=28;
  SDL_Rect icon={rect.x+(rect.w-side)/2,rect.y+(rect.h-side)/2,side,side};
  ui_design_icon(gfx->ren,icon,UI_DESIGN_ICON_SOUND,style.text);
  if(!audio_is_enabled()) {
    SDL_FPoint line[]={{(float)icon.x,(float)icon.y},
                      {(float)(icon.x+icon.w),(float)(icon.y+icon.h)}};
    ui_design_stroke(gfx->ren,line,2,false,1.7f,t.danger);
  }
  if(!audio_control_popup_open)return;
  design_card(gfx,t,sdl_rect(geo.popup));
  const SDL_Rect buttons[]={sdl_rect(geo.music),sdl_rect(geo.fx)};
  for(int i=0;i<2;++i) {
    bool muted=i?audio_fx_muted():audio_music_muted();
    char label[96];
    snprintf(label,sizeof(label),"%s: %s",tr(ui->language,i?T_FX:T_MUSIC),
             tr(ui->language,muted?T_MUTED:T_ENABLED));
    design_button(gfx,t,buttons[i],label,NULL,UI_DESIGN_ICON_NONE,
        muted?UI_DESIGN_QUIET:UI_DESIGN_SECONDARY,false,false,15);
  }
}

/* One public render entry point. Unmigrated screens retain their exact path;
   native overlays keep their existing order and command ownership. */
static void app_render_ui(Gfx *gfx,const Game *game,UI *ui) {
  if(ui->screen!=SCR_TITLE && ui->screen!=SCR_ABOUT) {
    app_render_existing_ui(gfx,game,ui);return;
  }
  ui_feedback_render_screen=(int)ui->screen;
  text_cache_sync(gfx,ui);
  app_render_base(gfx,game,ui);
  if(ui->screen==SCR_TITLE)design_home(gfx,ui);
  else if(!design_about(gfx,ui))polish_render_about(gfx,ui);
  rc2_render_play_controls(gfx,game,ui);
  rc2_render_hint_panel(gfx,ui);
  rc2_render_settings_panel(gfx,ui);
  rc2_render_storage_banner(gfx,ui);
  rc2_render_generation_overlay(gfx,ui);
  design_audio(gfx,ui);
  rc2_render_more_panel(gfx,ui);
  ui_modal_render(gfx,ui);
}
