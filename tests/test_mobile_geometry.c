#include "geometry.h"

#include <assert.h>
#include <stdio.h>

static bool overlap(GeoRect a, GeoRect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

static void check(int w, int h, GeometryMode mode) {
  AppGeometry g;
  assert(geometry_compute_mobile(w, h, mode, &g));
  assert(geometry_play_valid(&g, w, h));
  assert(g.play_compact);
  assert(g.board.w >= 270 && g.board.w == g.board.h);
  assert(g.board.w % 9 == 0);
  for (int i = 0; i < GEOMETRY_PALETTE_COUNT; ++i) {
    assert(g.palette[i].w >= 48 && g.palette[i].h >= 48);
    assert(geometry_rect_in_bounds(g.palette[i], w, h));
    for (int j = 0; j < i; ++j)
      assert(!overlap(g.palette[i], g.palette[j]));
  }
  assert(g.palette[4].y == g.palette[0].y);
  assert(g.palette[5].y >= g.palette[0].y + g.palette[0].h + 6);
  assert(g.progress.y >= g.palette[5].y + g.palette[5].h);
  assert(g.status.y + g.status.h <= h);
  assert(g.board.y + g.board.h < g.play_controls[PLAY_CONTROL_UNDO].y);
  assert(g.more_panel.y + g.more_panel.h <= h - 8);
  assert(g.progress.h >= 24);
  assert(g.hud[0].h >= 22);
  assert(geometry_rect_in_bounds(g.status, w, h));
  assert(g.play_title.h >= 20);
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i) {
    assert(g.home_mode[i].h >= 48 && g.home_difficulty[i].h >= 48);
    assert(geometry_rect_in_bounds(g.home_mode[i], w, h));
    assert(geometry_rect_in_bounds(g.home_difficulty[i], w, h));
  }
  for (int i = 0; i < GEOMETRY_HOME_PRIMARY_COUNT; ++i) {
    assert(g.home_primary[i].h >= 48);
    assert(geometry_rect_in_bounds(g.home_primary[i], w, h));
  }
  for (int i = 0; i < GEOMETRY_HOME_SECONDARY_COUNT; ++i) {
    assert(g.home_secondary[i].h >= 48);
    assert(geometry_rect_in_bounds(g.home_secondary[i], w, h));
  }
  AudioControlGeometry home_speaker = geometry_audio_control(w, h, &g, false);
  assert(home_speaker.button.w >= 48 && home_speaker.button.h >= 48);
  assert(geometry_rect_in_bounds(home_speaker.button, w, h));
  assert(home_speaker.music.h >= 48 && home_speaker.fx.h >= 48);
  assert(geometry_rect_in_bounds(home_speaker.popup, w, h));
  assert(!overlap(home_speaker.button, g.screen_language));
  assert(!overlap(g.home_mode[0], g.home_logo));
  assert(!overlap(g.home_secondary[3], g.home_secondary[4]));
  for (int i = 0; i < GEOMETRY_HOME_SEGMENT_COUNT; ++i)
    for (int j = 0; j < i; ++j) {
      assert(!overlap(g.home_mode[i], g.home_mode[j]));
      assert(!overlap(g.home_difficulty[i], g.home_difficulty[j]));
    }
  AudioControlGeometry speaker = geometry_audio_control(w, h, &g, true);
  assert(speaker.button.w >= 48 && speaker.button.h >= 48);
  assert(geometry_rect_in_bounds(speaker.button, w, h));
  assert(speaker.music.h >= 48 && speaker.fx.h >= 48);
  assert(geometry_rect_in_bounds(speaker.popup, w, h));
  for (int i = PLAY_CONTROL_MENU; i <= PLAY_CONTROL_MORE; ++i)
    assert(!overlap(speaker.button, g.play_controls[i]));
  assert(!overlap(speaker.button, g.board));
  for (int i = PLAY_CONTROL_MENU; i <= PLAY_CONTROL_NOTES; ++i) {
    assert(geometry_rect_in_bounds(g.play_controls[i], w, h));
    assert(g.play_controls[i].w >= 48 && g.play_controls[i].h >= 48);
  }
  for (int i = 0; i < GEOMETRY_MORE_COUNT; ++i) {
    assert(geometry_rect_in_bounds(g.more_items[i], w, h));
    assert(g.more_items[i].w >= 48 && g.more_items[i].h >= 48);
    for (int j = 0; j < i; ++j)
      assert(!overlap(g.more_items[i], g.more_items[j]));
  }
}

static void check_landscape(int w,int h,GeometryMode mode) {
  AppGeometry g;
  assert(geometry_mobile_landscape_play(w,h,mode,&g));
  assert(g.play_compact && g.board.w>=270 && g.board.w%9==0);
  assert(g.board.w==g.board.h && geometry_rect_in_bounds(g.board,w,h));
  assert(!overlap(g.board,g.sidebar));
  assert(g.screen_language.w==0 && g.home_logo.w==0);
  GeoRect targets[GEOMETRY_PALETTE_COUNT+10];
  int count=0;
  for (int i=PLAY_CONTROL_MENU;i<=PLAY_CONTROL_NOTES;++i)
    targets[count++]=g.play_controls[i];
  targets[count++]=g.hud[4];
  targets[count++]=geometry_audio_control(w,h,&g,true).button;
  for (int i=0;i<GEOMETRY_PALETTE_COUNT;++i) targets[count++]=g.palette[i];
  assert(count==19);
  for (int i=0;i<count;++i) {
    assert(targets[i].w>=48 && targets[i].h>=48);
    assert(geometry_rect_in_bounds(targets[i],w,h));
    assert(!overlap(targets[i],g.board));
    for (int j=0;j<i;++j) assert(!overlap(targets[i],targets[j]));
    assert(!overlap(targets[i],g.progress) && !overlap(targets[i],g.status));
  }
  for (int i=0;i<g.hud_count;++i) {
    assert(geometry_rect_in_bounds(g.hud[i],w,h));
    assert(!overlap(g.hud[i],g.board));
    for (int j=0;j<i;++j) assert(!overlap(g.hud[i],g.hud[j]));
    if (i!=4) for (int j=0;j<count;++j)
      assert(!overlap(g.hud[i],targets[j]));
  }
  if (h>=360) assert(!overlap(g.progress,g.status));
  else assert(g.progress.x==g.status.x && g.progress.y==g.status.y);
  assert(geometry_rect_in_bounds(g.progress,w,h));
  assert(geometry_rect_in_bounds(g.status,w,h));
  for (int i=0;i<GEOMETRY_MORE_COUNT;++i) {
    assert(g.more_items[i].w>=48 && g.more_items[i].h>=48);
    assert(geometry_rect_in_bounds(g.more_items[i],w,h));
    for (int j=0;j<i;++j) assert(!overlap(g.more_items[i],g.more_items[j]));
  }
  AppGeometry shell;
  assert(geometry_compute_mobile(w,h,mode,&shell));
  assert(shell.mobile_layout && geometry_play_valid(&shell,w,h));
  GeoRect result[]={shell.screen_language,shell.end_logo,shell.end_heading,
      shell.end_summary,shell.end_buttons[0],shell.end_buttons[1]};
  GeoRect paused[]={shell.screen_language,shell.pause_logo,shell.pause_heading,
      shell.pause_buttons[0],shell.pause_buttons[1]};
  for (unsigned i=0;i<sizeof(result)/sizeof(result[0]);++i) {
    assert(geometry_rect_in_bounds(result[i],w,h));
    for (unsigned j=0;j<i;++j) assert(!overlap(result[i],result[j]));
  }
  for (unsigned i=0;i<sizeof(paused)/sizeof(paused[0]);++i) {
    assert(geometry_rect_in_bounds(paused[i],w,h));
    for (unsigned j=0;j<i;++j) assert(!overlap(paused[i],paused[j]));
  }
  GeometryHintPanel hint;
  assert(geometry_hint_panel(&shell,w,h,&hint));
  GeoRect hint_controls[]={hint.primary,hint.reveal,hint.cancel};
  assert(geometry_rect_in_bounds(hint.body,w,h));
  for (unsigned i=0;i<3;++i) {
    assert(hint_controls[i].w>=48 && hint_controls[i].h>=48);
    assert(geometry_rect_in_bounds(hint_controls[i],w,h));
    assert(!overlap(hint_controls[i],hint.body));
    assert(!overlap(hint_controls[i],hint.title));
    for (unsigned j=0;j<i;++j) assert(!overlap(hint_controls[i],hint_controls[j]));
  }
  AppGeometry play_before=g;
  assert(geometry_mobile_home(w,h,&g));
  assert(g.board.x==play_before.board.x && g.board.y==play_before.board.y &&
         g.board.w==play_before.board.w && g.board.h==play_before.board.h);
  GeoRect home[16];int n=0;
  for (int i=0;i<GEOMETRY_HOME_SEGMENT_COUNT;++i) {
    home[n++]=g.home_mode[i];home[n++]=g.home_difficulty[i];
  }
  for (int i=0;i<GEOMETRY_HOME_PRIMARY_COUNT;++i) home[n++]=g.home_primary[i];
  for (int i=0;i<GEOMETRY_HOME_SECONDARY_COUNT;++i) home[n++]=g.home_secondary[i];
  home[n++]=g.screen_language;
  home[n++]=geometry_audio_control(w,h,&g,false).button;
  assert(n==16);
  for (int i=0;i<n;++i) {
    assert(home[i].w>=48 && home[i].h>=48 && geometry_rect_in_bounds(home[i],w,h));
    assert(!overlap(home[i],g.home_logo));
    for (int j=0;j<i;++j) assert(!overlap(home[i],home[j]));
  }
}

static void check_settings(int w,int h) {
  GeometrySettings g;
  assert(geometry_mobile_settings(w,h,&g));
  assert(geometry_rect_in_bounds(g.card,w,h));
  const GeoRect targets[]={g.theme,g.language,g.sound,g.music_minus,
      g.music_track,g.music_plus,g.fx_minus,g.fx_track,g.fx_plus,
      g.reduced_motion,g.auto_notes,g.back};
  const GeoRect labels[]={g.title,g.music_label,g.fx_label};
  for (unsigned i=0;i<sizeof(targets)/sizeof(targets[0]);++i) {
    GeoRect r=targets[i];
    assert(r.w>=48 && r.h>=48 && geometry_rect_in_bounds(r,w,h));
    assert(r.x>=g.card.x && r.y>=g.card.y &&
           r.x+r.w<=g.card.x+g.card.w && r.y+r.h<=g.card.y+g.card.h);
    for (unsigned j=0;j<i;++j) assert(!overlap(r,targets[j]));
    for (unsigned j=0;j<sizeof(labels)/sizeof(labels[0]);++j)
      assert(!overlap(r,labels[j]));
  }
  for (unsigned i=0;i<sizeof(labels)/sizeof(labels[0]);++i)
    assert(geometry_rect_in_bounds(labels[i],w,h));
}

static void check_modal(int w,int h,unsigned count) {
  GeometryModal g;
  assert(geometry_modal(w,h,count,&g));
  assert(geometry_rect_in_bounds(g.panel,w,h));
  assert(geometry_rect_in_bounds(g.title,w,h));
  assert(geometry_rect_in_bounds(g.body,w,h));
  assert(g.body.w>=150 && g.body.h>=102);
  GeoRect targets[5]={g.scroll_up,g.scroll_down};
  for (unsigned i=0;i<count;++i) targets[i+2]=g.buttons[i];
  for (unsigned i=0;i<count+2;++i) {
    GeoRect r=targets[i];
    assert(r.w>=48 && r.h>=48 && geometry_rect_in_bounds(r,w,h));
    assert(r.x>=g.panel.x && r.y>=g.panel.y &&
           r.x+r.w<=g.panel.x+g.panel.w && r.y+r.h<=g.panel.y+g.panel.h);
    assert(!overlap(r,g.body) && !overlap(r,g.title));
    for (unsigned j=0;j<i;++j) assert(!overlap(r,targets[j]));
  }
}

static void check_auxiliary(int w,int h) {
  for (int about=0;about<2;++about) {
    GeometryInfo g;
    assert(geometry_mobile_info(w,h,about!=0,&g));
    assert(g.body.w>=180 && g.body.h>=(h<360 ? 150 : 190));
    assert(geometry_rect_in_bounds(g.body,w,h));
    GeoRect controls[GEOMETRY_ABOUT_LINK_COUNT+3]={g.back,g.up,g.down};
    int count=3;
    if (about) {
      for (int i=0;i<GEOMETRY_ABOUT_LINK_COUNT;++i)
        controls[count++]=g.links[i];
      assert(g.links[0].y==g.links[1].y);
      assert(g.links[2].y==g.links[3].y);
      assert(g.links[2].y>g.links[0].y);
      assert(g.back.x==g.links[0].x);
      assert(g.back.w==g.links[1].x+g.links[1].w-g.links[0].x);
      assert(g.back.h==g.links[0].h);
      assert(g.back.y>=g.links[3].y+g.links[3].h);
    }
    for (int i=0;i<count;++i) {
      assert(controls[i].w>=48 && controls[i].h>=48);
      if (about && h<600 && i>=3) assert(controls[i].w>=120);
      assert(geometry_rect_in_bounds(controls[i],w,h));
      assert(!overlap(controls[i],g.body) && !overlap(controls[i],g.heading));
      for (int j=0;j<i;++j) assert(!overlap(controls[i],controls[j]));
    }
  }
  GeometryLibrary g;
  assert(geometry_mobile_library(w,h,&g));
  assert(g.visible>=1 && g.list.h==g.visible*g.row_height && g.row_height>=48);
  assert(geometry_rect_in_bounds(g.list,w,h));
  assert(geometry_rect_in_bounds(g.detail,w,h));
  assert(g.detail.h>=120 && !overlap(g.detail,g.list));
  GeoRect controls[]={g.back,g.primary,g.replay,g.remove,g.page_previous,g.page_next};
  for (unsigned i=0;i<sizeof(controls)/sizeof(controls[0]);++i) {
    assert(controls[i].w>=48 && controls[i].h>=48);
    assert(geometry_rect_in_bounds(controls[i],w,h));
    assert(!overlap(controls[i],g.list) && !overlap(controls[i],g.detail));
    for (unsigned j=0;j<i;++j) assert(!overlap(controls[i],controls[j]));
  }
}

int main(void) {
  for (int w=360;w<=1200;w+=29)
    for (int h=600;h<=960;h+=31) check_auxiliary(w,h);
  for (int w=600;w<=1200;w+=29)
    for (int h=320;h<600;h+=23) check_auxiliary(w,h);
  check_auxiliary(360,600);check_auxiliary(600,360);check_auxiliary(800,480);
  GeometryInfo info;
  GeometryLibrary library;
  assert(!geometry_mobile_info(599,599,true,&info));
  assert(!geometry_mobile_info(600,319,false,&info));
  assert(!geometry_mobile_library(359,600,&library));
  assert(!geometry_mobile_library(599,599,&library));
  for (int w=360;w<=800;w+=23)
    for (int h=600;h<=960;h+=31) check_settings(w,h);
  for (int w=600;w<=1200;w+=29)
    for (int h=320;h<600;h+=23) check_settings(w,h);
  check_settings(360,600);check_settings(600,360);check_settings(800,480);
  GeometrySettings settings;
  assert(!geometry_mobile_settings(359,600,&settings));
  assert(!geometry_mobile_settings(599,599,&settings));
  for (int w=264;w<=1000;w+=37)
    for (int h=264;h<=920;h+=41)
      for (unsigned count=1;count<=3;++count) check_modal(w,h,count);
  check_modal(360,600,3);check_modal(600,360,3);check_modal(800,480,2);
  for(int width=360;width<=432;width+=24) {
    GeometryModal phone;
    assert(geometry_modal(width,892,3,&phone));
    for(unsigned i=0;i<3;++i) {
      assert(phone.buttons[i].w>=width-64);
      if(i) assert(phone.buttons[i].y>=phone.buttons[i-1].y+phone.buttons[i-1].h+6);
    }
  }
  check_auxiliary(412,892);
  GeometryModal modal;
  assert(!geometry_modal(263,360,1,&modal));
  assert(!geometry_modal(360,263,1,&modal));
  assert(!geometry_modal(360,600,0,&modal));
  assert(!geometry_modal(360,600,4,&modal));
  for (int w = 360; w <= 639; w += 27) {
    for (int h = 600; h <= 920; h += 40)
      for (int mode = GEOMETRY_MODE_CLASSIC;
           mode <= GEOMETRY_MODE_TIME; ++mode)
        check(w, h, (GeometryMode)mode);
  }
  check(360, 600, GEOMETRY_MODE_CLASSIC);
  check(390, 844, GEOMETRY_MODE_STRIKES);
  check(412, 915, GEOMETRY_MODE_TIME);
  AppGeometry tall;
  assert(geometry_compute_mobile(390, 920, GEOMETRY_MODE_CLASSIC, &tall));
  assert(tall.palette[0].h > 48);
  assert(tall.play_controls[PLAY_CONTROL_UNDO].h > 48);
  AppGeometry g;
  assert(!geometry_compute_mobile(359, 600, GEOMETRY_MODE_CLASSIC, &g));
  assert(!geometry_compute_mobile(360, 599, GEOMETRY_MODE_CLASSIC, &g));
  assert(!geometry_compute_mobile(599,599,GEOMETRY_MODE_CLASSIC,&g));
  assert(!geometry_compute_mobile(600,319,GEOMETRY_MODE_CLASSIC,&g));
  assert(geometry_compute_mobile(780, 430, GEOMETRY_MODE_CLASSIC, &g));
  for (int w=600;w<=1200;w+=25)
    for (int h=320;h<=599 && h<w;h+=19)
      for (int mode=GEOMETRY_MODE_CLASSIC;mode<=GEOMETRY_MODE_TIME;++mode)
        check_landscape(w,h,(GeometryMode)mode);
  check_landscape(600,320,GEOMETRY_MODE_TIME);
  check_landscape(600,360,GEOMETRY_MODE_CLASSIC);
  check_landscape(800,480,GEOMETRY_MODE_TIME);
  assert(!geometry_mobile_landscape_play(599,360,GEOMETRY_MODE_CLASSIC,&g));
  assert(!geometry_mobile_landscape_play(600,319,GEOMETRY_MODE_CLASSIC,&g));
  assert(!geometry_mobile_landscape_play(600,600,GEOMETRY_MODE_CLASSIC,&g));
  assert(!geometry_mobile_landscape_play(600,360,(GeometryMode)-1,&g));
  assert(!geometry_window_size_supported(360, 600));
  assert(!geometry_compute(360, 600, GEOMETRY_MODE_CLASSIC, &g));
  assert(geometry_compute(360, 640, GEOMETRY_MODE_CLASSIC, &g));
  puts("Android portrait and complete landscape shell geometry, 48dp targets and desktop bounds passed");
  return 0;
}
