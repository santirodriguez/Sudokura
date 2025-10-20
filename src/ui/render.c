#include "ui/render.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "core/game.h"
#include "platform/fonts.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
  SDL_Window *win;
  SDL_Renderer *ren;
  TTF_Font *font_big;
  TTF_Font *font_small;
  int width;
  int height;
} Gfx;

typedef struct {
  SDL_Color bg;
  SDL_Color board;
  SDL_Color thin;
  SDL_Color thick;
  SDL_Color hover;
  SDL_Color sel;
  SDL_Color sel_outline;
  SDL_Color text_given;
  SDL_Color text_edit;
  SDL_Color text_wrong;
  SDL_Color boxhl;
  SDL_Color samehl;
  SDL_Color shadow;
  SDL_Color btn;
  SDL_Color btnfg;
  SDL_Color dim;
  SDL_Color title;
  SDL_Color palette_bg;
  SDL_Color palette_fg;
  SDL_Color conflict;
} Theme;

typedef enum {
  MODE_CLASSIC = 0,
  MODE_STRIKES = 1,
  MODE_TIME = 2
} Mode;

typedef enum {
  SCR_TITLE = 0,
  SCR_PLAY = 1,
  SCR_END = 2,
  SCR_HELP = 3,
  SCR_ABOUT = 4
} Screen;

typedef enum {
  RES_NONE = 0,
  RES_WIN = 1,
  RES_LOSE = 2
} Result;

typedef struct {
  int sel_r;
  int sel_c;
  bool notes_mode;
  bool strict_mode;
  bool paused;
  bool dark_theme;

  int mistakes;
  int strikes;
  int strikes_max;
  double start_t;
  double pause_t0;
  double paused_accum;
  double time_limit_s;

  char toast[96];
  double toast_t0;
  bool toast_on;

  Screen screen;
  Screen prev_screen;
  Mode mode;
  Result result;
} UI;

static double now_s(void) { return SDL_GetTicks() * 0.001; }

static double elapsed_time(const UI *ui) {
  double now = now_s();
  if (ui->paused) {
    return ui->pause_t0 - ui->start_t - ui->paused_accum;
  }
  return now - ui->start_t - ui->paused_accum;
}

static void show_toast(UI *ui, const char *msg) {
  snprintf(ui->toast, sizeof(ui->toast), "%s", msg);
  ui->toast_t0 = now_s();
  ui->toast_on = true;
}

static void set_mode_params(UI *ui) {
  ui->strikes = 0;
  ui->strikes_max = 3;
  ui->time_limit_s = (ui->mode == MODE_TIME ? 10 * 60 : 0);
}

static Theme theme_dark(void) {
  Theme t = {
      .bg = {16, 18, 26, 255},
      .board = {26, 30, 44, 255},
      .thin = {110, 118, 140, 160},
      .thick = {150, 180, 255, 220},
      .hover = {39, 46, 66, 160},
      .sel = {60, 80, 130, 170},
      .sel_outline = {180, 210, 255, 230},
      .text_given = {245, 248, 252, 255},
      .text_edit = {170, 255, 210, 255},
      .text_wrong = {255, 125, 125, 255},
      .boxhl = {40, 50, 70, 130},
      .samehl = {100, 140, 220, 70},
      .shadow = {0, 0, 0, 110},
      .btn = {34, 38, 56, 255},
      .btnfg = {230, 230, 235, 255},
      .dim = {165, 175, 185, 255},
      .title = {140, 170, 255, 255},
      .palette_bg = {32, 36, 54, 255},
      .palette_fg = {230, 230, 235, 255},
      .conflict = {220, 60, 60, 80}};
  return t;
}

static Theme theme_light(void) {
  Theme t = {
      .bg = {245, 247, 252, 255},
      .board = {236, 240, 248, 255},
      .thin = {150, 160, 175, 200},
      .thick = {60, 80, 120, 255},
      .hover = {210, 220, 245, 200},
      .sel = {180, 200, 250, 160},
      .sel_outline = {35, 55, 120, 230},
      .text_given = {36, 41, 46, 255},
      .text_edit = {0, 110, 85, 255},
      .text_wrong = {200, 40, 40, 255},
      .boxhl = {200, 210, 235, 160},
      .samehl = {60, 100, 200, 50},
      .shadow = {0, 0, 0, 60},
      .btn = {220, 226, 240, 255},
      .btnfg = {36, 41, 46, 255},
      .dim = {90, 100, 120, 255},
      .title = {40, 70, 160, 255},
      .palette_bg = {225, 232, 246, 255},
      .palette_fg = {36, 41, 46, 255},
      .conflict = {200, 60, 60, 80}};
  return t;
}

static bool point_in(SDL_Rect r, int x, int y) {
  return x >= r.x && x < (r.x + r.w) && y >= r.y && y < (r.y + r.h);
}

static void draw_rect(SDL_Renderer *ren, int x, int y, int w, int h,
                      SDL_Color col) {
  SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
  SDL_Rect r = {x, y, w, h};
  SDL_RenderFillRect(ren, &r);
}

static void draw_line(SDL_Renderer *ren, int x1, int y1, int x2, int y2,
                      SDL_Color col) {
  SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
  SDL_RenderDrawLine(ren, x1, y1, x2, y2);
}

}

static SDL_Texture *render_text_wrapped(Gfx *gfx, TTF_Font *font,
                                        const char *text, SDL_Color color,
                                        int wrap, int *w, int *h) {
  SDL_Surface *surf = TTF_RenderUTF8_Blended_Wrapped(font, text, color, wrap);
  if (!surf) {
    return NULL;
  }
  SDL_Texture *tex = SDL_CreateTextureFromSurface(gfx->ren, surf);
  if (!tex) {
    SDL_FreeSurface(surf);
    return NULL;
  }
  if (w) {
    *w = surf->w;
  }
  if (h) {
    *h = surf->h;
  }
  SDL_FreeSurface(surf);
  return tex;
}

static void blit_wrapped(Gfx *gfx, int x, int y, int w, const char *text,
                         SDL_Color col) {
  int tw, th;
  SDL_Texture *tex = render_text_wrapped(gfx, gfx->font_small, text, col, w, &tw,
                                         &th);
  if (tex) {
    SDL_Rect dst = {x, y, tw, th};
    SDL_RenderCopy(gfx->ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
  }
}

static TTF_Font *load_font(const char *path, int pt) {
  if (!path) {
    return NULL;
  }
  return TTF_OpenFont(path, pt);
}

static bool confirm_box(SDL_Window *win, const char *title, const char *msg,
                        const char *confirm) {
  const SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, confirm},
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"}};
  const SDL_MessageBoxData data = {
      SDL_MESSAGEBOX_INFORMATION, win, title, msg, ARRAY_SIZE(buttons), buttons,
      NULL};
  int button = 0;
  if (SDL_ShowMessageBox(&data, &button) < 0) {
    return false;
  }
  return button == 1;
}

static SDL_Texture *render_text_font(Gfx *gfx, TTF_Font *font,
                                     const char *text, SDL_Color color, int *w,
                                     int *h) {
  SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
  if (!surf) {
    return NULL;
  }
  SDL_Texture *tex = SDL_CreateTextureFromSurface(gfx->ren, surf);
  if (tex && w) {
    *w = surf->w;
  }
  if (tex && h) {
    *h = surf->h;
  }
  SDL_FreeSurface(surf);
  return tex;
}

static SDL_Texture *render_text_small(Gfx *gfx, const char *text, SDL_Color col,
                                      int *w, int *h) {
  return render_text_font(gfx, gfx->font_small, text, col, w, h);
}

static SDL_Texture *render_text_big(Gfx *gfx, const char *text, SDL_Color col,
                                    int *w, int *h) {
  return render_text_font(gfx, gfx->font_big, text, col, w, h);
}
typedef struct {
  SDL_Rect board;
  SDL_Rect side;
  bool right;
} Layout;

static Layout compute_layout(int W, int H) {
  Layout L;
  int margin = 40;
  int gap = 24;
  int minSide = 360;
  int sidebarW = 260;
  if (W >= 900) {
    int side = H - 2 * margin;
    if (side < minSide) {
      side = minSide;
    }
    int usableW = W - 2 * margin - sidebarW - gap;
    if (side > usableW) {
      side = usableW;
    }
    if (side < minSide) {
      side = minSide;
    }
    side = (side / 9) * 9;
    L.board = (SDL_Rect){margin, (H - side) / 2, side, side};
    L.side = (SDL_Rect){L.board.x + L.board.w + gap, L.board.y, sidebarW, side};
    L.right = true;
  } else {
    int side = W - 2 * margin;
    if (side < minSide) {
      side = minSide;
    }
    if (side > H - (margin * 3 + 280)) {
      side = H - (margin * 3 + 280);
    }
    if (side < minSide) {
      side = minSide;
    }
    side = (side / 9) * 9;
    L.board = (SDL_Rect){(W - side) / 2, margin, side, side};
    L.side = (SDL_Rect){margin, L.board.y + L.board.h + gap, W - 2 * margin, 260};
    L.right = false;
  }
  return L;
}

typedef struct {
  SDL_Rect btn[9];
  SDL_Rect pal[9];
  int count_btn;
  int count_pal;
  int title_h;
  int info_h;
  int info_lines;
} SidebarRects;

static void compute_sidebar_rects(const Layout *L, const UI *ui, SidebarRects *R) {
  const int sx = L->side.x;
  const int sy = L->side.y;
  const int sw = L->side.w;
  const int title_h = 46;
  const int info_h = 24;
  const int gap_small = 4;
  const int gap_big = 8;
  int info_lines = 2;
  if (ui->mode == MODE_TIME) {
    info_lines += 1;
  }
  if (ui->mode == MODE_STRIKES) {
    info_lines += 1;
  }
  int y = sy + title_h + gap_small + info_lines * (info_h + gap_small) + gap_big;
  int bw = sw;
  const int bh = 34;
  for (int i = 0; i < 9; ++i) {
    R->btn[i] = (SDL_Rect){sx, y, bw, bh};
    y += bh + gap_big;
  }
  y += 4 + info_h;
  for (int n = 0; n < 9; ++n) {
    R->pal[n] = (SDL_Rect){sx, y, bw, bh};
    y += bh + 6;
  }
  R->count_btn = 9;
  R->count_pal = 9;
  R->title_h = title_h;
  R->info_h = info_h;
  R->info_lines = info_lines;
}

static SDL_Rect back_rect(const Gfx *g) {
  int bw = 140;
  int bh = 40;
  int x = 40;
  int y = g->height - bh - 40;
  SDL_Rect r = {x, y, bw, bh};
  return r;
}

static void draw_back_button(Gfx *g, Theme th) {
  SDL_Rect r = back_rect(g);
  draw_rect(g->ren, r.x, r.y, r.w, r.h, th.btn);
  int tw, thh;
  SDL_Texture *t = render_text_small(g, "Back", th.btnfg, &tw, &thh);
  if (t) {
    SDL_Rect d = {r.x + 12, r.y + (r.h - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, t, NULL, &d);
    SDL_DestroyTexture(t);
  }
}
static void render_board_and_sidebar(Gfx *g, const Game *game, UI *ui) {
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r, th.bg.g, th.bg.b, 255);
  SDL_RenderClear(g->ren);

  Layout layout = compute_layout(g->width, g->height);
  int gx = layout.board.x;
  int gy = layout.board.y;
  int side = layout.board.w;
  int cell = side / 9;

  draw_rect(g->ren, gx - 6, gy - 6, side + 12, side + 12, th.shadow);
  draw_rect(g->ren, gx, gy, side, side, th.board);

  draw_rect(g->ren, gx, gy + ui->sel_r * cell, side, cell, th.boxhl);
  draw_rect(g->ren, gx + ui->sel_c * cell, gy, cell, side, th.boxhl);
  int br = (ui->sel_r / 3) * 3;
  int bc = (ui->sel_c / 3) * 3;
  draw_rect(g->ren, gx + bc * cell, gy + br * cell, cell * 3, cell * 3,
            (SDL_Color){th.boxhl.r, th.boxhl.g, th.boxhl.b,
                         (Uint8)(th.boxhl.a / 2)});

  for (int i = 0; i <= 9; ++i) {
    int x = gx + (side * i) / 9;
    int y = gy + (side * i) / 9;
    SDL_Color col = (i % 3 == 0) ? th.thick : th.thin;
    draw_line(g->ren, x, gy, x, gy + side, col);
    draw_line(g->ren, gx, y, gx + side, y, col);
  }

  int mx, my;
  SDL_GetMouseState(&mx, &my);
  int curV = game->puzzle[GAME_IDX(ui->sel_r, ui->sel_c)];
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 9; ++c) {
      int x = gx + c * cell;
      int y = gy + r * cell;
      bool sel = (ui->sel_r == r && ui->sel_c == c);
      bool hover = (mx >= x && mx < x + cell && my >= y && my < y + cell);
      int v = game->puzzle[GAME_IDX(r, c)];

      if (curV && v == curV && !sel) {
        draw_rect(g->ren, x + 2, y + 2, cell - 4, cell - 4, th.samehl);
      }
      if (sel) {
        double t = now_s();
        double p = 0.65 + 0.35 * (0.5 * (1.0 + sin(t * 4.0)));
        draw_rect(g->ren, x + 2, y + 2, cell - 4, cell - 4,
                  (SDL_Color){(Uint8)(th.sel.r * p), (Uint8)(th.sel.g * p),
                               (Uint8)(th.sel.b * p), 190});
        SDL_Color o = th.sel_outline;
        draw_line(g->ren, x + 1, y + 1, x + cell - 2, y + 1, o);
        draw_line(g->ren, x + cell - 2, y + 1, x + cell - 2, y + cell - 2, o);
        draw_line(g->ren, x + cell - 2, y + cell - 2, x + 1, y + cell - 2, o);
        draw_line(g->ren, x + 1, y + cell - 2, x + 1, y + 1, o);
      } else if (hover) {
        draw_rect(g->ren, x + 2, y + 2, cell - 4, cell - 4, th.hover);
      }

      if (v && game_has_conflict(game, r, c, v)) {
        draw_rect(g->ren, x + 2, y + 2, cell - 4, cell - 4, th.conflict);
      }

      if (sel || ui->notes_mode) {
        SDL_Color sgrid = (SDL_Color){th.thin.r, th.thin.g, th.thin.b, (Uint8)120};
        int sub = cell / 3;
        for (int k = 1; k < 3; ++k) {
          draw_line(g->ren, x + k * sub, y + 2, x + k * sub, y + cell - 2, sgrid);
          draw_line(g->ren, x + 2, y + k * sub, x + cell - 2, y + k * sub, sgrid);
        }
      }

      if (v) {
        SDL_Color col = game->fixed[GAME_IDX(r, c)] ? th.text_given : th.text_edit;
        if (!game->fixed[GAME_IDX(r, c)] && v != game->solution[GAME_IDX(r, c)]) {
          col = th.text_wrong;
        }
        char buf[2] = {(char)('0' + v), 0};
        int tw, thh;
        SDL_Texture *t = render_text_big(g, buf, col, &tw, &thh);
        if (t) {
          SDL_Rect d = {x + (cell - tw) / 2, y + (cell - thh) / 2, tw, thh};
          SDL_RenderCopy(g->ren, t, NULL, &d);
          SDL_DestroyTexture(t);
        }
      } else if (game->notes[GAME_IDX(r, c)]) {
        for (int vv = 1; vv <= 9; ++vv) {
          if (game->notes[GAME_IDX(r, c)] & (1u << vv)) {
            char buf[2] = {(char)('0' + vv), 0};
            SDL_Color ncol = th.dim;
            int tw, thh;
            SDL_Texture *t = render_text_small(g, buf, ncol, &tw, &thh);
            if (t) {
              int sub = cell / 3;
              int q = (vv - 1) / 3;
              int qq = (vv - 1) % 3;
              int nx = x + qq * sub + (sub - tw) / 2;
              int ny = y + q * sub + (sub - thh) / 2;
              SDL_Rect d = {nx, ny, tw, thh};
              SDL_RenderCopy(g->ren, t, NULL, &d);
              SDL_DestroyTexture(t);
            }
          }
        }
      }
    }
  }

  SidebarRects rects;
  compute_sidebar_rects(&layout, ui, &rects);

  int sx = layout.side.x;
  int sy = layout.side.y;
  int sw = layout.side.w;
  int tw, thh;

  SDL_Texture *title = render_text_big(g, "Sudokura v1.0", th.title, &tw, &thh);
  if (title) {
    SDL_Rect d = {sx, sy, tw, thh};
    SDL_RenderCopy(g->ren, title, NULL, &d);
    SDL_DestroyTexture(title);
  }

  int y = sy + rects.title_h;
  char buf[64];
  const char *mode_name =
      (ui->mode == MODE_CLASSIC
           ? "Classic"
           : (ui->mode == MODE_STRIKES ? "Strikes" : "Time Attack"));
  snprintf(buf, sizeof(buf), "Mode: %s", mode_name);
  SDL_Texture *t1 = render_text_small(g, buf, th.dim, &tw, &thh);
  if (t1) {
    SDL_Rect d = {sx, y + (rects.info_h - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, t1, NULL, &d);
    SDL_DestroyTexture(t1);
  }
  y += rects.info_h + 4;

  double el = elapsed_time(ui);
  snprintf(buf, sizeof(buf), "Time: %02d:%02d", (int)(el / 60), (int)fmod(el, 60));
  SDL_Texture *t2 = render_text_small(g, buf, th.dim, &tw, &thh);
  if (t2) {
    SDL_Rect d = {sx, y + (rects.info_h - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, t2, NULL, &d);
    SDL_DestroyTexture(t2);
  }
  y += rects.info_h + 4;

  if (ui->mode == MODE_TIME) {
    int rem = (int)(ui->time_limit_s - el);
    if (rem < 0) {
      rem = 0;
    }
    snprintf(buf, sizeof(buf), "Target: %02d:%02d", rem / 60, rem % 60);
    SDL_Texture *t = render_text_small(g, buf, th.dim, &tw, &thh);
    if (t) {
      SDL_Rect d = {sx, y + (rects.info_h - thh) / 2, tw, thh};
      SDL_RenderCopy(g->ren, t, NULL, &d);
      SDL_DestroyTexture(t);
    }
    y += rects.info_h + 4;
  }
  if (ui->mode == MODE_STRIKES) {
    int left = ui->strikes_max - ui->strikes;
    if (left < 0) {
      left = 0;
    }
    snprintf(buf, sizeof(buf), "Strikes left: %d/%d", left, ui->strikes_max);
    SDL_Texture *t = render_text_small(g, buf, th.dim, &tw, &thh);
    if (t) {
      SDL_Rect d = {sx, y + (rects.info_h - thh) / 2, tw, thh};
      SDL_RenderCopy(g->ren, t, NULL, &d);
      SDL_DestroyTexture(t);
    }
  }

  const char *labels[] = {"New",         "Mode",      "Hint",
                          "Notes (N/Shift)", "Verify",    "Theme",
                          "Help",        "About",     "Menu"};
  for (int i = 0; i < rects.count_btn; ++i) {
    SDL_Rect rc = rects.btn[i];
    draw_rect(g->ren, rc.x, rc.y, rc.w, rc.h, th.btn);
    SDL_Texture *t = render_text_small(g, labels[i], th.btnfg, &tw, &thh);
    if (t) {
      SDL_Rect d = {rc.x + 10, rc.y + (rc.h - thh) / 2, tw, thh};
      SDL_RenderCopy(g->ren, t, NULL, &d);
      SDL_DestroyTexture(t);
    }
  }

  SDL_Texture *tp = render_text_small(g, "Palette", th.dim, &tw, &thh);
  if (tp) {
    SDL_Rect d = {sx, rects.pal[0].y - (thh + 6), tw, thh};
    SDL_RenderCopy(g->ren, tp, NULL, &d);
    SDL_DestroyTexture(tp);
  }
  for (int n = 1; n <= 9; ++n) {
    SDL_Rect rc = rects.pal[n - 1];
    draw_rect(g->ren, rc.x, rc.y, rc.w, rc.h, th.palette_bg);
    char nb[2] = {(char)('0' + n), 0};
    SDL_Texture *t = render_text_small(g, nb, th.palette_fg, &tw, &thh);
    if (t) {
      SDL_Rect d = {rc.x + 12, rc.y + (rc.h - thh) / 2, tw, thh};
      SDL_RenderCopy(g->ren, t, NULL, &d);
      SDL_DestroyTexture(t);
    }
  }

  int filled = 0;
  for (int i = 0; i < SUDOKU_CELLS; ++i) {
    if (game->puzzle[i]) {
      ++filled;
    }
  }
  snprintf(buf, sizeof(buf), "Progress %d%%   Errors %d", (filled * 100) / 81,
           ui->mistakes);
  SDL_Texture *ts =
      render_text_wrapped(g, g->font_small, buf, th.dim, sw, &tw, &thh);
  if (ts) {
    SDL_Rect d = {sx, rects.pal[8].y + rects.pal[8].h + 6, tw, thh};
    SDL_RenderCopy(g->ren, ts, NULL, &d);
    SDL_DestroyTexture(ts);
  }

  if (ui->toast_on) {
    double dt = now_s() - ui->toast_t0;
    if (dt > 2.2) {
      ui->toast_on = false;
    }
    if (ui->toast_on) {
      SDL_Texture *tt2 = render_text_small(
          g, ui->toast, (SDL_Color){255, 255, 255, 230}, &tw, &thh);
      if (tt2) {
        SDL_Rect d = {gx + (side - tw) / 2, gy - (thh + 8), tw, thh};
        draw_rect(g->ren, d.x - 8, d.y - 6, tw + 16, thh + 12,
                  (SDL_Color){0, 0, 0, 110});
        SDL_RenderCopy(g->ren, tt2, NULL, &d);
        SDL_DestroyTexture(tt2);
      }
    }
  }
}
static void render_title(Gfx *g, UI *ui) {
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r, th.bg.g, th.bg.b, 255);
  SDL_RenderClear(g->ren);

  int tw, thh;
  SDL_Texture *title = render_text_big(g, "Sudokura v1.0", th.title, &tw, &thh);
  if (title) {
    SDL_Rect d = {(g->width - tw) / 2, g->height / 2 - 160, tw, thh};
    SDL_RenderCopy(g->ren, title, NULL, &d);
    SDL_DestroyTexture(title);
  }

  int bx = g->width / 2 - 180;
  int by = g->height / 2 - 60;
  int bw = 360;
  int bh = 42;
  int gap = 12;
  SDL_Rect r_mode = {bx, by, bw, bh};
  by += bh + gap;
  SDL_Rect r_start = {bx, by, bw, bh};
  by += bh + gap;
  SDL_Rect r_help = {bx, by, bw, bh};
  by += bh + gap;
  SDL_Rect r_about = {bx, by, bw, bh};
  by += bh + gap;
  SDL_Rect r_quit = {bx, by, bw, bh};

  draw_rect(g->ren, r_mode.x, r_mode.y, bw, bh, th.btn);
  const char *modeName =
      (ui->mode == MODE_CLASSIC
           ? "Classic"
           : (ui->mode == MODE_STRIKES ? "Strikes" : "Time Attack"));
  char mline[64];
  snprintf(mline, sizeof(mline), "Mode: %s", modeName);
  SDL_Texture *mode = render_text_small(g, mline, th.btnfg, &tw, &thh);
  if (mode) {
    SDL_Rect d = {r_mode.x + 12, r_mode.y + (bh - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, mode, NULL, &d);
    SDL_DestroyTexture(mode);
  }

  SDL_Color start_col = {(
      Uint8)(th.btn.r + 10), (Uint8)(th.btn.g + 10), (Uint8)(th.btn.b + 10),
      th.btn.a};
  draw_rect(g->ren, r_start.x, r_start.y, bw, bh, start_col);
  SDL_Texture *start = render_text_small(g, "Start", th.btnfg, &tw, &thh);
  if (start) {
    SDL_Rect d = {r_start.x + 12, r_start.y + (bh - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, start, NULL, &d);
    SDL_DestroyTexture(start);
  }

  draw_rect(g->ren, r_help.x, r_help.y, bw, bh, th.btn);
  SDL_Texture *help = render_text_small(g, "Help (F1)", th.btnfg, &tw, &thh);
  if (help) {
    SDL_Rect d = {r_help.x + 12, r_help.y + (bh - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, help, NULL, &d);
    SDL_DestroyTexture(help);
  }

  draw_rect(g->ren, r_about.x, r_about.y, bw, bh, th.btn);
  SDL_Texture *about = render_text_small(g, "About (F2)", th.btnfg, &tw, &thh);
  if (about) {
    SDL_Rect d = {r_about.x + 12, r_about.y + (bh - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, about, NULL, &d);
    SDL_DestroyTexture(about);
  }

  draw_rect(g->ren, r_quit.x, r_quit.y, bw, bh, th.btn);
  SDL_Texture *quit = render_text_small(g, "Quit", th.btnfg, &tw, &thh);
  if (quit) {
    SDL_Rect d = {r_quit.x + 12, r_quit.y + (bh - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, quit, NULL, &d);
    SDL_DestroyTexture(quit);
  }
}

static void render_help(Gfx *g, UI *ui) {
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r, th.bg.g, th.bg.b, 255);
  SDL_RenderClear(g->ren);
  int tw, thh;
  SDL_Texture *title = render_text_big(g, "Help", th.title, &tw, &thh);
  if (title) {
    SDL_Rect d = {60, 40, tw, thh};
    SDL_RenderCopy(g->ren, title, NULL, &d);
    SDL_DestroyTexture(title);
  }
  int x = 60;
  int y = 40 + thh + 12;
  int w = g->width - 120;
  const char *text =
      "- Goal: fill the 9x9 grid so each row, column and 3x3 box contains 1..9 with no repeats.\n"
      "- Modes: Classic (no limits). Strikes (3 wrong moves = lose). Time Attack (solve under 10:00).\n"
      "- Select a cell with mouse or arrows (WASD). Place with keys 1..9 or the palette.\n"
      "- Notes: press N to toggle Notes mode or hold Shift while typing numbers.\n"
      "         You can also note with the mouse: click a sub-cell (the 3x3 mini-grid inside the cell).\n"
      "- Hint: fills the selected cell with the correct answer.\n"
      "- Verify: checks conflicts against Sudoku rules (rows/cols/boxes). It does not reveal the solution.\n"
      "- Strict mode (M): blocks illegal placements. Free mode allows them (they still count as mistakes).\n"
      "- Theme (T) toggles dark/light. Pause (P) pauses the timer. ESC or Back returns.";
  blit_wrapped(g, x, y, w, text, th.dim);
  draw_back_button(g, th);
}

static void render_about(Gfx *g, UI *ui) {
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r, th.bg.g, th.bg.b, 255);
  SDL_RenderClear(g->ren);
  int tw, thh;
  SDL_Texture *title = render_text_big(g, "About", th.title, &tw, &thh);
  if (title) {
    SDL_Rect d = {60, 40, tw, thh};
    SDL_RenderCopy(g->ren, title, NULL, &d);
    SDL_DestroyTexture(title);
  }
  int x = 60;
  int y = 40 + thh + 12;
  int w = g->width - 120;
  const char *text =
      "Sudokura v1.0 — Modern Sudoku in C + SDL2\n"
      "Author: santirodriguez — https://santiagorodriguez.com\n"
      "License: GPLv3\n\n"
      "This project features a responsive layout, note-taking, hints and multiple modes (Classic, Strikes, Time Attack).\n"
      "It also includes robust font discovery for Linux, macOS and Windows.";
  blit_wrapped(g, x, y, w, text, th.dim);
  draw_back_button(g, th);
}

static void render_end(Gfx *g, UI *ui) {
  Theme th = ui->dark_theme ? theme_dark() : theme_light();
  SDL_SetRenderDrawColor(g->ren, th.bg.r, th.bg.g, th.bg.b, 255);
  SDL_RenderClear(g->ren);
  int tw, thh;
  const char *title = (ui->result == RES_WIN) ? "You Win!" : "Game Over";
  SDL_Texture *t = render_text_big(g, title, th.title, &tw, &thh);
  if (t) {
    SDL_Rect d = {(g->width - tw) / 2, 160, tw, thh};
    SDL_RenderCopy(g->ren, t, NULL, &d);
    SDL_DestroyTexture(t);
  }
  const char *msg =
      (ui->result == RES_WIN)
          ? "Great job! You solved the puzzle."
          : "Try again soon — practice makes perfect.";
  blit_wrapped(g, (g->width - 400) / 2, 220, 400, msg, th.dim);

  int bx = g->width / 2 - 160;
  int by = 260;
  int bw = 320;
  int bh = 40;
  int gap = 12;
  SDL_Rect b1 = {bx, by, bw, bh};
  by += bh + gap;
  SDL_Rect b2 = {bx, by, bw, bh};

  draw_rect(g->ren, b1.x, b1.y, bw, bh, th.btn);
  SDL_Texture *again = render_text_small(g, "Play Again", th.btnfg, &tw, &thh);
  if (again) {
    SDL_Rect d = {b1.x + 12, b1.y + (bh - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, again, NULL, &d);
    SDL_DestroyTexture(again);
  }

  draw_rect(g->ren, b2.x, b2.y, bw, bh, th.btn);
  SDL_Texture *menu = render_text_small(g, "Main Menu", th.btnfg, &tw, &thh);
  if (menu) {
    SDL_Rect d = {b2.x + 12, b2.y + (bh - thh) / 2, tw, thh};
    SDL_RenderCopy(g->ren, menu, NULL, &d);
    SDL_DestroyTexture(menu);
  }
}
int sudokura_run(int argc, char **argv) {
  const char *font_cli = NULL;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--font") && i + 1 < argc) {
      font_cli = argv[++i];
    }
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  if (TTF_Init() != 0) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    SDL_Quit();
    return 1;
  }

  Gfx g = {0};
  g.width = 1024;
  g.height = 720;
  g.win = SDL_CreateWindow("Sudokura v1.0", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, g.width, g.height,
                           SDL_WINDOW_RESIZABLE);
  g.ren = SDL_CreateRenderer(g.win, -1,
                             SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!g.win || !g.ren) {
    fprintf(stderr, "SDL window/renderer failed\n");
    if (g.ren) {
      SDL_DestroyRenderer(g.ren);
    }
    if (g.win) {
      SDL_DestroyWindow(g.win);
    }
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  char font_path[PATH_MAX] = {0};
  const char *fpath = find_font_path_dynamic(font_path, font_cli);
  if (!fpath) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sudokura v1.0",
                             "Could not find a usable TTF/OTF font.\n"
                             "Install any TrueType/OpenType font and try again,\n"
                             "or run: ./sudokura --font /path/to/font.ttf",
                             g.win);
    SDL_DestroyRenderer(g.ren);
    SDL_DestroyWindow(g.win);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  g.font_big = load_font(fpath, 44);
  g.font_small = load_font(fpath, 20);
  if (!g.font_big || !g.font_small) {
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR, "Sudokura v1.0",
        "TTF_OpenFont failed with the chosen font.", g.win);
    if (g.font_big) {
      TTF_CloseFont(g.font_big);
    }
    if (g.font_small) {
      TTF_CloseFont(g.font_small);
    }
    SDL_DestroyRenderer(g.ren);
    SDL_DestroyWindow(g.win);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  Game game;
  game_new(&game, (unsigned)time(NULL));

  UI ui;
  memset(&ui, 0, sizeof(ui));
  ui.sel_r = 4;
  ui.sel_c = 4;
  ui.dark_theme = true;
  ui.mode = MODE_CLASSIC;
  ui.screen = SCR_TITLE;
  ui.prev_screen = SCR_TITLE;
  set_mode_params(&ui);

  bool running = true;
  SDL_Event e;
  while (running) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_WINDOWEVENT &&
                 e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        g.width = e.window.data1;
        g.height = e.window.data2;
      } else if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        if (ui.screen == SCR_TITLE) {
          if (k == SDLK_ESCAPE) {
            running = false;
          } else if (k == SDLK_RETURN) {
            ui.screen = SCR_PLAY;
            ui.start_t = now_s();
            ui.paused = false;
            ui.paused_accum = 0;
            ui.mistakes = 0;
            ui.strikes = 0;
          } else if (k == SDLK_t) {
            ui.dark_theme = !ui.dark_theme;
          } else if (k == SDLK_F1) {
            ui.prev_screen = SCR_TITLE;
            ui.screen = SCR_HELP;
          } else if (k == SDLK_F2) {
            ui.prev_screen = SCR_TITLE;
            ui.screen = SCR_ABOUT;
          } else if (k == SDLK_m) {
            ui.mode = (ui.mode + 1) % 3;
            set_mode_params(&ui);
          }
        } else if (ui.screen == SCR_HELP || ui.screen == SCR_ABOUT) {
          if (k == SDLK_ESCAPE) {
            ui.screen = ui.prev_screen;
          }
        } else if (ui.screen == SCR_END) {
          if (k == SDLK_ESCAPE) {
            ui.screen = SCR_TITLE;
          } else if (k == SDLK_RETURN) {
            game_new(&game, (unsigned)time(NULL));
            ui.screen = SCR_PLAY;
            ui.start_t = now_s();
            ui.paused = false;
            ui.paused_accum = 0;
            ui.mistakes = 0;
            ui.strikes = 0;
          }
        } else {
          bool shifted = (SDL_GetModState() & KMOD_SHIFT) != 0;
          int r = ui.sel_r;
          int c = ui.sel_c;
          int idx = GAME_IDX(r, c);
          if (k == SDLK_ESCAPE) {
            ui.screen = SCR_TITLE;
          } else if (k == SDLK_UP || k == SDLK_w) {
            ui.sel_r = (ui.sel_r + 8) % 9;
          } else if (k == SDLK_DOWN || k == SDLK_s) {
            ui.sel_r = (ui.sel_r + 1) % 9;
          } else if (k == SDLK_LEFT || k == SDLK_a) {
            ui.sel_c = (ui.sel_c + 8) % 9;
          } else if (k == SDLK_RIGHT || k == SDLK_d) {
            ui.sel_c = (ui.sel_c + 1) % 9;
          } else if (k == SDLK_p) {
            if (!ui.paused) {
              ui.paused = true;
              ui.pause_t0 = now_s();
            } else {
              ui.paused = false;
              ui.paused_accum += now_s() - ui.pause_t0;
            }
          } else if (k == SDLK_t) {
            ui.dark_theme = !ui.dark_theme;
          } else if (k == SDLK_n) {
            ui.notes_mode = !ui.notes_mode;
            show_toast(&ui, ui.notes_mode ? "Notes ON" : "Notes OFF");
          } else if (k == SDLK_m) {
            ui.strict_mode = !ui.strict_mode;
            show_toast(&ui, ui.strict_mode ? "Strict" : "Free");
          } else if (k == SDLK_h) {
            if (game_give_hint(&game, r, c)) {
              show_toast(&ui, "Hint used");
            }
          } else if (k == SDLK_DELETE || k == SDLK_BACKSPACE || k == SDLK_0 ||
                     k == SDLK_KP_0) {
            if (!game.fixed[idx]) {
              game.puzzle[idx] = 0;
              game.notes[idx] = 0;
            }
          } else {
            int v = 0;
            if (k >= SDLK_1 && k <= SDLK_9) {
              v = (k - SDLK_0);
            } else if (k >= SDLK_KP_1 && k <= SDLK_KP_9) {
              v = (k - SDLK_KP_0);
            }
            if (v >= 1 && v <= 9) {
              if (ui.notes_mode || shifted) {
                if (!game.fixed[idx] && game.puzzle[idx] == 0) {
                  game.notes[idx] ^= (1u << v);
                }
              } else {
                if (game_place(&game, r, c, v, ui.strict_mode)) {
                  if (v != game.solution[idx]) {
                    ++ui.mistakes;
                    if (ui.mode == MODE_STRIKES) {
                      ++ui.strikes;
                    }
                    show_toast(&ui, "Wrong");
                  }
                } else {
                  if (v != game.solution[idx]) {
                    ++ui.mistakes;
                    if (ui.mode == MODE_STRIKES) {
                      ++ui.strikes;
                    }
                    show_toast(&ui, "Illegal");
                  }
                }
              }
            }
          }
        }
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        int x = e.button.x;
        int y = e.button.y;
        bool right = (e.button.button == SDL_BUTTON_RIGHT);

        if (ui.screen == SCR_TITLE) {
          int bx = g.width / 2 - 180;
          int by = g.height / 2 - 60;
          int bw = 360;
          int bh = 42;
          int gap = 12;
          SDL_Rect r_mode = {bx, by, bw, bh};
          by += bh + gap;
          SDL_Rect r_start = {bx, by, bw, bh};
          by += bh + gap;
          SDL_Rect r_help = {bx, by, bw, bh};
          by += bh + gap;
          SDL_Rect r_about = {bx, by, bw, bh};
          by += bh + gap;
          SDL_Rect r_quit = {bx, by, bw, bh};
          if (point_in(r_mode, x, y)) {
            ui.mode = (ui.mode + 1) % 3;
            set_mode_params(&ui);
          } else if (point_in(r_start, x, y)) {
            ui.screen = SCR_PLAY;
            ui.start_t = now_s();
            ui.paused = false;
            ui.paused_accum = 0;
            ui.mistakes = 0;
            ui.strikes = 0;
          } else if (point_in(r_help, x, y)) {
            ui.prev_screen = SCR_TITLE;
            ui.screen = SCR_HELP;
          } else if (point_in(r_about, x, y)) {
            ui.prev_screen = SCR_TITLE;
            ui.screen = SCR_ABOUT;
          } else if (point_in(r_quit, x, y)) {
            running = false;
          }
        } else if (ui.screen == SCR_END) {
          int bx = g.width / 2 - 160;
          int by = 260;
          int bw = 320;
          int bh = 40;
          int gap = 12;
          SDL_Rect b1 = {bx, by, bw, bh};
          by += bh + gap;
          SDL_Rect b2 = {bx, by, bw, bh};
          if (point_in(b1, x, y)) {
            game_new(&game, (unsigned)time(NULL));
            ui.screen = SCR_PLAY;
            ui.start_t = now_s();
            ui.paused = false;
            ui.paused_accum = 0;
            ui.mistakes = 0;
            ui.strikes = 0;
          } else if (point_in(b2, x, y)) {
            ui.screen = SCR_TITLE;
          }
        } else if (ui.screen == SCR_HELP || ui.screen == SCR_ABOUT) {
          SDL_Rect r = back_rect(&g);
          if (point_in(r, x, y)) {
            ui.screen = ui.prev_screen;
          }
        } else {
          Layout L = compute_layout(g.width, g.height);
          int gx = L.board.x;
          int gy = L.board.y;
          int side = L.board.w;
          int cs = side / 9;
          if (x >= gx && x < gx + side && y >= gy && y < gy + side) {
            int c = (x - gx) / cs;
            int r = (y - gy) / cs;
            ui.sel_r = r;
            ui.sel_c = c;
            int idx = GAME_IDX(r, c);
            if (!game.fixed[idx]) {
              int lx = x - (gx + c * cs);
              int ly = y - (gy + r * cs);
              int sub = cs / 3;
              int qq = lx / sub;
              int q = ly / sub;
              if (qq < 0) {
                qq = 0;
              }
              if (q < 0) {
                q = 0;
              }
              if (qq > 2) {
                qq = 2;
              }
              if (q > 2) {
                q = 2;
              }
              int vv = q * 3 + qq + 1;
              if (right || ui.notes_mode) {
                if (game.puzzle[idx] == 0) {
                  game.notes[idx] ^= (1u << vv);
                }
              }
            }
          } else {
            SidebarRects rects;
            compute_sidebar_rects(&L, &ui, &rects);
            if (point_in(rects.btn[0], x, y)) {
              if (confirm_box(g.win, "New game",
                              "Start a new game? Current progress will be lost.",
                              "New")) {
                game_new(&game, (unsigned)time(NULL));
                ui.start_t = now_s();
                ui.paused = false;
                ui.paused_accum = 0;
                ui.mistakes = 0;
                ui.strikes = 0;
              }
            } else if (point_in(rects.btn[1], x, y)) {
              const char *next =
                  (ui.mode == MODE_CLASSIC
                       ? "Strikes"
                       : (ui.mode == MODE_STRIKES ? "Time Attack" : "Classic"));
              char msg[128];
              snprintf(msg, sizeof(msg),
                       "Change mode to %s?\nThis will start a new game.", next);
              if (confirm_box(g.win, "Change mode", msg, "Change")) {
                ui.mode = (ui.mode + 1) % 3;
                set_mode_params(&ui);
                game_new(&game, (unsigned)time(NULL));
                ui.start_t = now_s();
                ui.paused = false;
                ui.paused_accum = 0;
                ui.mistakes = 0;
                ui.strikes = 0;
              }
            } else if (point_in(rects.btn[2], x, y)) {
              if (game_give_hint(&game, ui.sel_r, ui.sel_c)) {
                show_toast(&ui, "Hint used");
              }
            } else if (point_in(rects.btn[3], x, y)) {
              ui.notes_mode = !ui.notes_mode;
              show_toast(&ui, ui.notes_mode ? "Notes ON" : "Notes OFF");
            } else if (point_in(rects.btn[4], x, y)) {
              int conf = game_count_conflicts(&game);
              if (conf == 0) {
                show_toast(&ui, "No conflicts");
              } else {
                char m[32];
                snprintf(m, sizeof(m), "Conflicts: %d", conf);
                show_toast(&ui, m);
              }
            } else if (point_in(rects.btn[5], x, y)) {
              ui.dark_theme = !ui.dark_theme;
            } else if (point_in(rects.btn[6], x, y)) {
              ui.prev_screen = SCR_PLAY;
              ui.screen = SCR_HELP;
            } else if (point_in(rects.btn[7], x, y)) {
              ui.prev_screen = SCR_PLAY;
              ui.screen = SCR_ABOUT;
            } else if (point_in(rects.btn[8], x, y)) {
              ui.screen = SCR_TITLE;
            } else {
              for (int n = 1; n <= 9; ++n) {
                if (point_in(rects.pal[n - 1], x, y)) {
                  int idx = GAME_IDX(ui.sel_r, ui.sel_c);
                  if (!game.fixed[idx]) {
                    if (ui.notes_mode) {
                      if (game.puzzle[idx] == 0) {
                        game.notes[idx] ^= (1u << n);
                      }
                    } else {
                      if (game_place(&game, ui.sel_r, ui.sel_c, n,
                                      ui.strict_mode)) {
                        if (n != game.solution[idx]) {
                          ++ui.mistakes;
                          if (ui.mode == MODE_STRIKES) {
                            ++ui.strikes;
                          }
                          show_toast(&ui, "Wrong");
                        }
                      } else {
                        if (n != game.solution[idx]) {
                          ++ui.mistakes;
                          if (ui.mode == MODE_STRIKES) {
                            ++ui.strikes;
                          }
                          show_toast(&ui, "Illegal");
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    if (ui.screen == SCR_PLAY && !ui.paused) {
      bool lose = false;
      if (ui.mode == MODE_TIME && ui.time_limit_s > 0 &&
          elapsed_time(&ui) > ui.time_limit_s) {
        lose = true;
      }
      if (ui.mode == MODE_STRIKES && ui.strikes >= ui.strikes_max) {
        lose = true;
      }
      if (game_is_solved(&game)) {
        ui.result = RES_WIN;
        ui.screen = SCR_END;
      } else if (lose) {
        ui.result = RES_LOSE;
        ui.screen = SCR_END;
      }
    }

    if (ui.screen == SCR_TITLE) {
      render_title(&g, &ui);
    } else if (ui.screen == SCR_HELP) {
      render_help(&g, &ui);
    } else if (ui.screen == SCR_ABOUT) {
      render_about(&g, &ui);
    } else if (ui.screen == SCR_END) {
      render_end(&g, &ui);
    } else {
      render_board_and_sidebar(&g, &game, &ui);
    }

    SDL_RenderPresent(g.ren);
  }

  if (g.font_big) {
    TTF_CloseFont(g.font_big);
  }
  if (g.font_small) {
    TTF_CloseFont(g.font_small);
  }
  SDL_DestroyRenderer(g.ren);
  SDL_DestroyWindow(g.win);
  TTF_Quit();
  SDL_Quit();
  return 0;
}
