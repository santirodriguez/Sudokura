#ifndef SUDOKURA_UI_ACTION_ICONS_H
#define SUDOKURA_UI_ACTION_ICONS_H

#include "sudokura_sdl/ui_design_draw.h"

/* Gameplay/library actions use the same 24-unit, 1.7-unit stroke family. */
typedef enum {
  UI_ACTION_NONE, UI_ACTION_SAVE, UI_ACTION_UNDO, UI_ACTION_REDO,
  UI_ACTION_CLEAR, UI_ACTION_NOTES, UI_ACTION_PAUSE, UI_ACTION_RESTART,
  UI_ACTION_HINT, UI_ACTION_VERIFY, UI_ACTION_MORE, UI_ACTION_TRASH,
  UI_ACTION_EYE, UI_ACTION_CLOSE, UI_ACTION_COUNT
} UiActionIcon;

static inline UiDesignLabel ui_action_label_layout(int width, int height,
                                                    bool with_icon) {
  UiDesignLabel boxes = ui_design_label_layout(width, height, with_icon);
  if (width <= 8 || height <= 6)
    return (UiDesignLabel){{0,0,0,0},{0,0,0,0}};
  if (!boxes.icon.w) {
    /* Small actions retain their full translated label, not a tiny column. */
    boxes.text = (UiDesignRect){0,1,width,height-2};
  } else {
    boxes.text.y = 3;
    boxes.text.h = height-6;
  }
  return boxes;
}

static inline void ui_action_icon(SDL_Renderer *r, SDL_Rect b,
                                   UiActionIcon icon, UiDesignColor c) {
  if (!r || b.w < 8 || b.h < 8 || icon <= UI_ACTION_NONE ||
      icon >= UI_ACTION_COUNT) return;
#define ACTION_LINE(x1,y1,x2,y2) ui_design_icon_line(r,b,x1,y1,x2,y2,c)
#define ACTION_PATH(p,closed) ui_design_icon_path(r,b,p,(int)(sizeof(p)/sizeof((p)[0])),closed,c)
  switch (icon) {
    case UI_ACTION_SAVE: {
      const SDL_FPoint outer[]={{4,3},{18,3},{21,6},{21,21},{3,21},{3,4}};
      const SDL_FPoint slot[]={{8,3},{8,9},{17,9},{17,3}};
      const SDL_FPoint label[]={{7,21},{7,14},{17,14},{17,21}};
      ACTION_PATH(outer,true); ACTION_PATH(slot,false); ACTION_PATH(label,false);
      break;
    }
    case UI_ACTION_UNDO:
    case UI_ACTION_REDO: {
      SDL_FPoint curve[9]; int n=0;
      ui_design_cubic(curve,&n,(SDL_FPoint){4,9},(SDL_FPoint){19,2},
          (SDL_FPoint){25,21},(SDL_FPoint){10,20});
      curve[n++]=(SDL_FPoint){10,20};
      SDL_FPoint arrow[]={{4,4},{4,9},{9,10}};
      if (icon == UI_ACTION_REDO) {
        for (int i=0;i<n;++i) curve[i].x=24-curve[i].x;
        for (int i=0;i<3;++i) arrow[i].x=24-arrow[i].x;
      }
      ui_design_icon_path(r,b,curve,n,false,c); ACTION_PATH(arrow,false);
      break;
    }
    case UI_ACTION_CLEAR: {
      const SDL_FPoint p[]={{3,15},{13,3},{21,10},{12,21},{8,21}};
      ACTION_PATH(p,true); ACTION_LINE(7,10,16,17); ACTION_LINE(12,21,22,21);
      break;
    }
    case UI_ACTION_NOTES: {
      const SDL_FPoint p[]={{4,15},{16,3},{21,8},{9,20},{3,21}};
      ACTION_PATH(p,true); ACTION_LINE(13,6,18,11); ACTION_LINE(4,15,9,20);
      break;
    }
    case UI_ACTION_PAUSE: {
      const SDL_FPoint a[]={{5,4},{9,4},{9,20},{5,20}};
      const SDL_FPoint d[]={{15,4},{19,4},{19,20},{15,20}};
      ACTION_PATH(a,true); ACTION_PATH(d,true); break;
    }
    case UI_ACTION_RESTART: {
      SDL_FPoint p[33];
      for (int i=0;i<33;++i) {
        float a=-2.35f+(float)i*5.5f/32.0f;
        p[i]=(SDL_FPoint){12+8*cosf(a),12+8*sinf(a)};
      }
      ACTION_PATH(p,false);
      const SDL_FPoint arrow[]={{3,4},{6.4f,6.4f},{3,9}};
      ACTION_PATH(arrow,false); break;
    }
    case UI_ACTION_HINT: {
      const SDL_FPoint p[]={{8,17},{8,14},{5,10},{5,7},{8,3},{16,3},
                            {19,7},{19,10},{16,14},{16,17}};
      ACTION_PATH(p,false); ACTION_LINE(8,18,16,18); ACTION_LINE(9,21,15,21);
      ACTION_LINE(10,8,12,11); ACTION_LINE(14,8,12,11); ACTION_LINE(12,11,12,17);
      break;
    }
    case UI_ACTION_VERIFY: {
      const SDL_FPoint p[]={{4,12},{9,18},{21,5}};
      ACTION_PATH(p,false); break;
    }
    case UI_ACTION_MORE:
      for (int i=0;i<3;++i)
        ui_design_icon_ellipse(r,b,5+(float)i*7,12,1.4f,1.4f,c);
      break;
    case UI_ACTION_TRASH: {
      const SDL_FPoint p[]={{6,7},{7,21},{17,21},{18,7}};
      ACTION_PATH(p,false); ACTION_LINE(4,6,20,6); ACTION_LINE(9,3,15,3);
      ACTION_LINE(10,10,10,17); ACTION_LINE(14,10,14,17); break;
    }
    case UI_ACTION_EYE: {
      SDL_FPoint p[32]; int n=0;
      ui_design_cubic(p,&n,(SDL_FPoint){2,12},(SDL_FPoint){7,2},
          (SDL_FPoint){17,2},(SDL_FPoint){22,12});
      ui_design_cubic(p,&n,(SDL_FPoint){22,12},(SDL_FPoint){17,22},
          (SDL_FPoint){7,22},(SDL_FPoint){2,12});
      ui_design_icon_path(r,b,p,n,true,c);
      ui_design_icon_ellipse(r,b,12,12,3,3,c); break;
    }
    case UI_ACTION_CLOSE:
      ACTION_LINE(6,6,18,18); ACTION_LINE(6,18,18,6); break;
    default: break;
  }
#undef ACTION_PATH
#undef ACTION_LINE
}
#endif
