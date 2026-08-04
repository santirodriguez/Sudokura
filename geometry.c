#include "geometry.h"

#include <string.h>

enum { GAP = 6, DESKTOP_MIN_W = 640, DESKTOP_MIN_H = 480 };

bool geometry_window_size_supported(int width,int height){
  return (width>=DESKTOP_MIN_W&&height>=DESKTOP_MIN_H)||
         (width>=360&&height>=640);
}

bool geometry_normalize_window_size(int requested_width,int requested_height,
                                    int *normalized_width,int *normalized_height){
  if(!normalized_width||!normalized_height)return false;
  int width=requested_width<360?360:requested_width;
  int height=requested_height<DESKTOP_MIN_H?DESKTOP_MIN_H:requested_height;
  if(!geometry_window_size_supported(width,height))width=DESKTOP_MIN_W;
  *normalized_width=width;*normalized_height=height;
  return width!=requested_width||height!=requested_height;
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

static void grid(GeoRect area, int columns, GeoRect *items, int count) {
  int rows = (count + columns - 1) / columns;
  int cell_w = (area.w - (columns - 1) * GAP) / columns;
  int cell_h = (area.h - (rows - 1) * GAP) / rows;
  for (int i = 0; i < count; ++i)
    items[i] = (GeoRect){area.x + i % columns * (cell_w + GAP),
                         area.y + i / columns * (cell_h + GAP), cell_w, cell_h};
}

static void common_screens(int width, int height, int margin, AppGeometry *g) {
  int menu_w = width < 520 ? width - margin * 2 : 440;
  int button_h = height < 700 ? 40 : 48;
  int menu_h = 5 * button_h + 4 * 8;
  int top = (height - menu_h - 96) / 2;
  if (top < margin + 52) top = margin + 52;
  g->title_heading = (GeoRect){(width-menu_w)/2, top-64, menu_w, 54};
  grid((GeoRect){(width-menu_w)/2,top,menu_w,menu_h},1,g->title_buttons,5);
  g->info_heading=(GeoRect){margin*2,margin+42,width-margin*4,48};
  g->back_button=(GeoRect){margin*2,height-margin-44,width < 520 ? width-margin*4 : 180,44};
  g->info_body=(GeoRect){margin*2,g->info_heading.y+58,width-margin*4,
                         g->back_button.y-g->info_heading.y-68};
  g->end_heading=(GeoRect){margin*2,height/5,width-margin*4,54};
  g->end_summary=(GeoRect){margin*2,g->end_heading.y+66,width-margin*4,32};
  int end_w=width < 520 ? width-margin*4 : 360;
  grid((GeoRect){(width-end_w)/2,g->end_summary.y+54,end_w,96},1,g->end_buttons,2);
}

bool geometry_compute(int width, int height, GeometryMode mode, AppGeometry *g) {
  bool portrait = width >= 360 && height >= 640 && width < 640;
  if (!g || !geometry_window_size_supported(width,height) ||
      mode < GEOMETRY_MODE_CLASSIC || mode > GEOMETRY_MODE_TIME) return false;
  memset(g,0,sizeof(*g));
  int margin = portrait ? 8 : 16;
  g->hud_count = mode == GEOMETRY_MODE_CLASSIC ? 2 : 3;

  if (portrait) {
    int shell_w=width-margin*2, board=width-margin*2;
    int board_budget=height-415;
    if(board>board_budget)board=board_budget;
    if(board<225)board=225;
    board-=board%9;
    g->language=(GeoRect){margin,margin,shell_w,40};
    g->play_title=(GeoRect){margin,margin+46,shell_w,26};
    g->board=(GeoRect){(width-board)/2,g->play_title.y+32,board,board};
    int y=g->board.y+board+8;
    int hud_h=24;
    grid((GeoRect){margin,y,shell_w,hud_h},g->hud_count,g->hud,g->hud_count);
    y+=hud_h+6;
    g->palette_label=(GeoRect){margin,y,shell_w,18};
    y+=24;
    int palette_h=height >= 800 ? 126 : 96;
    grid((GeoRect){margin,y,shell_w,palette_h},3,g->palette,9);
    y+=palette_h+6;
    int remaining=height-margin-y-24;
    int rows=3, action_h=(remaining-(rows-1)*GAP)/rows;
    if(action_h<40) action_h=40;
    grid((GeoRect){margin,y,shell_w,rows*action_h+(rows-1)*GAP},3,g->actions,9);
    g->progress=(GeoRect){margin,height-margin-18,shell_w,18};
    g->sidebar=(GeoRect){margin,g->board.y+board+4,shell_w,height-margin-(g->board.y+board+4)};
  } else {
    int max_shell_w=1180, max_shell_h=900;
    int shell_w=width-margin*2; if(shell_w>max_shell_w)shell_w=max_shell_w;
    int shell_h=height-margin*2; if(shell_h>max_shell_h)shell_h=max_shell_h;
    int shell_x=(width-shell_w)/2, shell_y=(height-shell_h)/2;
    int sidebar_w=300; if(shell_w<900)sidebar_w=260;
    int board=shell_h-16; int limit=shell_w-sidebar_w-24; if(board>limit)board=limit;
    if(board>720)board=720;
    board-=board%9;
    if(board<288)return false;
    int composition_w=board+16+sidebar_w;
    int x=shell_x+(shell_w-composition_w)/2;
    g->board=(GeoRect){x,shell_y+(shell_h-board)/2,board,board};
    g->sidebar=(GeoRect){x+board+16,shell_y,sidebar_w,shell_h};
    int sx=g->sidebar.x, sy=g->sidebar.y, sw=g->sidebar.w;
    g->language=(GeoRect){sx,sy,sw,40}; sy+=48;
    g->play_title=(GeoRect){sx,sy,sw,34}; sy+=40;
    grid((GeoRect){sx,sy,sw,30},g->hud_count,g->hud,g->hud_count); sy+=38;
    int action_h=132; grid((GeoRect){sx,sy,sw,action_h},3,g->actions,9); sy+=action_h+8;
    g->palette_label=(GeoRect){sx,sy,sw,22}; sy+=26;
    int palette_h=126; grid((GeoRect){sx,sy,sw,palette_h},3,g->palette,9); sy+=palette_h+8;
    g->progress=(GeoRect){sx,sy,sw,22};
  }
  common_screens(width,height,margin,g);
  return geometry_play_valid(g,width,height);
}

GeometryFonts geometry_font_sizes(const AppGeometry *g,int width,int height){
  GeometryFonts f={12,14,16,16,14,28,36};
  if(!g)return f;
  int cell=g->board.w/9;
  f.cell=cell*3/5;
  if(f.cell<16)f.cell=16;
  if(f.cell>48)f.cell=48;
  f.note=f.cell/3;
  if(f.note<10)f.note=10;
  if(f.note>16)f.note=16;
  int scale=width<640?0:(g->board.w>=630?2:g->board.w>=450?1:0);
  f.control=scale==2?20:scale==1?18:15;
  f.hud=scale==2?18:scale==1?16:13;
  f.body=scale==2?20:scale==1?18:15;
  f.help=scale==2?18:scale==1?16:14;
  f.heading=scale==2?44:scale==1?38:30;
  if(height<600&&f.heading>32)f.heading=32;
  return f;
}

bool geometry_play_valid(const AppGeometry *g,int width,int height) {
  if(!g || !geometry_rect_in_bounds(g->board,width,height) ||
     !geometry_rect_in_bounds(g->sidebar,width,height) ||
     !geometry_rect_in_bounds(g->language,width,height) ||
     !geometry_rect_in_bounds(g->play_title,width,height) ||
     !geometry_rect_in_bounds(g->palette_label,width,height) ||
     !geometry_rect_in_bounds(g->progress,width,height)) return false;
  bool portrait=width<640;
  if(!portrait && overlaps(g->board,g->sidebar))return false;
  for(int i=0;i<g->hud_count;i++)if(!geometry_rect_in_bounds(g->hud[i],width,height))return false;
  for(int i=0;i<9;i++) {
    if(!geometry_rect_in_bounds(g->actions[i],width,height) || g->actions[i].h<40)return false;
    if(!geometry_rect_in_bounds(g->palette[i],width,height) || g->palette[i].h<28)return false;
  }
  if(!geometry_rect_in_bounds(g->title_heading,width,height)||!geometry_rect_in_bounds(g->info_heading,width,height)||
     !geometry_rect_in_bounds(g->info_body,width,height)||!geometry_rect_in_bounds(g->back_button,width,height)||
     !geometry_rect_in_bounds(g->end_heading,width,height)||!geometry_rect_in_bounds(g->end_summary,width,height))return false;
  for(int i=0;i<5;i++)if(!geometry_rect_in_bounds(g->title_buttons[i],width,height))return false;
  for(int i=0;i<2;i++)if(!geometry_rect_in_bounds(g->end_buttons[i],width,height))return false;
  return true;
}
