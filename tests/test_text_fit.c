#include "geometry.h"
#include "i18n.h"
#include "src/sudokura_sdl/ui_geometry.inc"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL_ttf.h>
#ifdef main
#undef main
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int test_font_sizes[] = {
    10, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28, 30,
    32, 36, 38, 42, 44, 48, 52, 56, 60, 64, 72,
};

static int fits(const char *font_path, const char *text, int preferred,
                int minimum, int width, int height) {
  for (int i = (int)(sizeof(test_font_sizes) / sizeof(test_font_sizes[0])) - 1;
       i >= 0; --i) {
    int size = test_font_sizes[i];
    if (size > preferred || size < minimum) continue;
    TTF_Font *font = TTF_OpenFont(font_path, size);
    assert(font);
    int w = 0, h = 0;
    int ok = TTF_SizeUTF8(font, text, &w, &h) == 0 && w <= width - 8 &&
             h <= height - 2;
    TTF_CloseFont(font);
    if (ok) return size;
  }
  return 0;
}

static int wrapped_fits(const char *font_path, const char *text, int preferred,
                        int minimum, int width, int height) {
  if (width <= 8 || height <= 2) return 0;
  SDL_Color color = {255, 255, 255, 255};
  for (int i = (int)(sizeof(test_font_sizes) / sizeof(test_font_sizes[0])) - 1;
       i >= 0; --i) {
    int size = test_font_sizes[i];
    if (size > preferred || size < minimum) continue;
    TTF_Font *font = TTF_OpenFont(font_path, size);
    assert(font);
    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(
        font, text, color, (Uint32)(width - 8));
    assert(surface);
    int ok = surface->w <= width - 4 && surface->h <= height - 2;
    SDL_FreeSurface(surface);
    TTF_CloseFont(font);
    if (ok) return size;
  }
  return 0;
}

static void assert_rect(GeoRect r, int width, int height) {
  assert(geometry_rect_in_bounds(r, width, height));
}

int main(void) {
  const char *font = getenv("SUDOKURA_TEST_FONT");
  if (!font) font = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  assert(TTF_Init() == 0);

  const int sizes[][2] = {
      {640, 480}, {800, 600}, {1024, 720}, {1366, 768}, {1920, 1080},
      {2560, 1440}, {3440, 1440}, {360, 640}, {390, 844}, {412, 915},
  };
  const TextKey actions[PLAY_ACTION_COUNT] = {
      [PLAY_ACTION_MENU] = T_MENU,
      [PLAY_ACTION_PAUSE] = T_PAUSE,
      [PLAY_ACTION_RESTART] = T_RESTART,
      [PLAY_ACTION_HINT] = T_HINT,
      [PLAY_ACTION_NOTES] = T_NOTES,
      [PLAY_ACTION_VERIFY] = T_VERIFY,
      [PLAY_ACTION_AUDIO] = T_SOUND,
      [PLAY_ACTION_HELP] = T_HELP,
      [PLAY_ACTION_ABOUT] = T_ABOUT,
  };
  const TextKey modes[] = {T_CLASSIC, T_STRIKES, T_TIME_ATTACK};
  const TextKey difficulties[] = {T_EASY, T_MEDIUM, T_HARD};
  const TextKey about_links[] = {T_GITHUB, T_REPOSITORY, T_WEBSITE, T_SUPPORT};

  for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
    for (int mode = 0; mode < 3; ++mode) {
      int width = sizes[s][0], height = sizes[s][1];
      AppGeometry g;
      assert(geometry_compute(width, height, (GeometryMode)mode, &g));
      GeometryFonts tier = geometry_font_sizes(&g, width, height);

      assert_rect(g.play_language, width, height);
      assert_rect(g.play_title, width, height);
      assert_rect(g.about_logo, width, height);
      assert_rect(g.about_body, width, height);
      assert_rect(g.about_fact, width, height);
      assert_rect(g.about_study, width, height);
      assert_rect(g.about_study_link, width, height);
      assert_rect(g.about_credits, width, height);
      for (int i = 0; i < GEOMETRY_ABOUT_LINK_COUNT; ++i)
        assert_rect(g.about_links[i], width, height);

      for (int language = 0; language < LANG_COUNT; ++language) {
        int play_segment = g.play_language.w / 3 - 40;
        int screen_segment = g.screen_language.w / 3 - 40;
        assert(fits(font, language_name((Language)language), tier.help, 10,
                    play_segment, g.play_language.h));
        assert(fits(font, language_name((Language)language), tier.help, 10,
                    screen_segment, g.screen_language.h));
        assert(fits(font, tr((Language)language, T_MODE), tier.help, 10,
                    g.home_mode_label.w, g.home_mode_label.h));
        assert(fits(font, tr((Language)language, T_DIFFICULTY), tier.help, 10,
                    g.home_difficulty_label.w, g.home_difficulty_label.h));

        for (int i = 0; i < 3; ++i) {
          assert(fits(font, tr((Language)language, modes[i]), tier.control, 10,
                      g.home_mode[i].w, g.home_mode[i].h));
          assert(fits(font, tr((Language)language, difficulties[i]), tier.control,
                      10, g.home_difficulty[i].w, g.home_difficulty[i].h));
        }
        for (int i = 0; i < PLAY_ACTION_COUNT; ++i)
          assert(fits(font, tr((Language)language, actions[i]), tier.control, 10,
                      g.actions[i].w, g.actions[i].h));

        int split_gap = g.actions[PLAY_ACTION_MENU].w >= 180 ? 6 : 4;
        int split_w = (g.actions[PLAY_ACTION_MENU].w - split_gap) / 2;
        assert(fits(font, tr((Language)language, T_MENU), tier.control, 9,
                    split_w, g.actions[PLAY_ACTION_MENU].h));
        assert(fits(font, tr((Language)language, T_PAUSE), tier.control, 9,
                    split_w, g.actions[PLAY_ACTION_MENU].h));
        assert(fits(font, tr((Language)language, T_UNDO), tier.control, 9,
                    split_w, g.actions[PLAY_ACTION_PAUSE].h));
        assert(fits(font, tr((Language)language, T_REDO), tier.control, 9,
                    split_w, g.actions[PLAY_ACTION_PAUSE].h));
        assert(fits(font, tr((Language)language, T_RESTART), tier.control, 9,
                    split_w, g.actions[PLAY_ACTION_RESTART].h));
        assert(fits(font, tr((Language)language, T_CLEAR), tier.control, 9,
                    split_w, g.actions[PLAY_ACTION_RESTART].h));

        int home_main_h = g.home_primary[1].h * 3 / 5;
        int home_sub_h = g.home_primary[1].h - home_main_h + 1;
        assert(fits(font, tr((Language)language, T_CONTINUE_NORMAL),
                    tier.control, 9, g.home_primary[1].w - 4, home_main_h));
        assert(fits(font, tr((Language)language, T_CONTINUE_DAILY),
                    tier.control, 9, g.home_primary[2].w - 4, home_main_h));
        assert(fits(font, tr((Language)language, T_DAILY_COMPLETE),
                    tier.control, 9, g.home_primary[2].w - 4, home_main_h));
        char normal_resume[160];
        snprintf(normal_resume, sizeof normal_resume, "%s · %s · 99:59",
                 tr((Language)language, T_TIME_ATTACK),
                 tr((Language)language, T_HARD));
        assert(fits(font, normal_resume,
                    tier.control > 13 ? tier.control - 4 : 10, 9,
                    g.home_primary[1].w - 4, home_sub_h));
        char daily_resume[160];
        snprintf(daily_resume, sizeof daily_resume,
                 "2026-09-18 · %s · 99:59",
                 tr((Language)language, T_MEDIUM));
        assert(fits(font, daily_resume,
                    tier.control > 13 ? tier.control - 4 : 10, 9,
                    g.home_primary[2].w - 4, home_sub_h));

        assert(fits(font, tr((Language)language, T_ANOTHER_NORMAL),
                    tier.control + 2, 9, g.end_buttons[0].w,
                    g.end_buttons[0].h));
        assert(fits(font, tr((Language)language, T_RETRY_SAME),
                    tier.control + 2, 9, g.end_buttons[0].w,
                    g.end_buttons[0].h));
        assert(fits(font, tr((Language)language, T_HOME),
                    tier.control, 9, g.end_buttons[1].w,
                    g.end_buttons[1].h));

        char sample[128];
        snprintf(sample, sizeof sample, "%s: %s", tr((Language)language, T_MODE),
                 tr((Language)language, T_TIME_ATTACK));
        assert(fits(font, sample, tier.hud, 10, g.hud[0].w, g.hud[0].h));
        snprintf(sample, sizeof sample, "%s: %s",
                 tr((Language)language, T_DIFFICULTY),
                 tr((Language)language, T_MEDIUM));
        assert(fits(font, sample, tier.hud, 10, g.hud[1].w, g.hud[1].h));
        snprintf(sample, sizeof sample, "%s: 99:59",
                 tr((Language)language, T_TIME));
        assert(fits(font, sample, tier.hud, 10, g.hud[2].w, g.hud[2].h));
        snprintf(sample, sizeof sample, "%s: 99",
                 tr((Language)language, T_ERRORS));
        assert(fits(font, sample, tier.hud, 10, g.hud[3].w, g.hud[3].h));
        snprintf(sample, sizeof sample, "%s: 99",
                 tr((Language)language, T_HINTS));
        assert(fits(font, sample, tier.hud, 10, g.hud[3].w, g.hud[3].h));

        char help_text[2048];
        snprintf(help_text, sizeof help_text,
                 tr((Language)language, T_HELP_BODY),
                 "Ctrl", "Ctrl", "Ctrl", "Ctrl");
        assert(wrapped_fits(font, help_text, tier.body, 10,
                            g.info_body.w, g.info_body.h));

        int intro_version_h = g.about_body.h / 2;
        if (intro_version_h < 20) intro_version_h = g.about_body.h;
        int intro_copy_h = g.about_body.h - intro_version_h;
        if (intro_copy_h > 4)
          assert(wrapped_fits(font, tr((Language)language, T_ABOUT_BODY),
                              tier.body, 10, g.about_body.w - 16,
                              intro_copy_h));

        int fact_icon_w = g.about_fact.h;
        if (fact_icon_w > 62) fact_icon_w = 62;
        int fact_text_w = g.about_fact.w - fact_icon_w - 10;
        int fact_text_h = g.about_fact.h - 10;
        int fact_title_h = fact_text_h / 3;
        if (fact_title_h < 16) fact_title_h = 16;
        assert(fits(font, tr((Language)language, T_ABOUT_FACT_TITLE), tier.help,
                    10, fact_text_w, fact_title_h));
        assert(wrapped_fits(font, tr((Language)language, T_ABOUT_FACT),
                            tier.body, 10, fact_text_w,
                            fact_text_h - fact_title_h));

        int study_title_h = g.about_study.h / 4;
        if (study_title_h < 12) study_title_h = 12;
        int study_copy_h = g.about_study_link.y -
                           (g.about_study.y + 2 + study_title_h) - 2;
        assert(fits(font, tr((Language)language, T_ABOUT_STUDY_TITLE), tier.help,
                    10, g.about_study.w - 18, study_title_h));
        assert(study_copy_h > 2);
        assert(wrapped_fits(font, tr((Language)language, T_ABOUT_STUDY),
                            tier.help, 10, g.about_study.w - 18,
                            study_copy_h));
        assert(fits(font, tr((Language)language, T_ABOUT_STUDY_LINK), tier.help,
                    10, g.about_study_link.w, g.about_study_link.h - 3));

        int credit_h = g.about_credits.h / 2;
        assert(fits(font, tr((Language)language, T_ABOUT_SEEDS), tier.help, 10,
                    g.about_credits.w, credit_h));
        assert(fits(font, tr((Language)language, T_ABOUT_STACK), tier.help, 10,
                    g.about_credits.w, g.about_credits.h - credit_h));

        for (int i = 0; i < GEOMETRY_ABOUT_LINK_COUNT; ++i) {
          char label[128];
          snprintf(label, sizeof label, "%d · %s", i + 1,
                   tr((Language)language, about_links[i]));
          assert(fits(font, label, tier.control, 10, g.about_links[i].w,
                      g.about_links[i].h - 3));
        }

        int hint_margin = g.sidebar.w < 240 ? 7 : 10;
        int hint_card_h = g.sidebar.h - hint_margin * 2;
        int hint_button_h = hint_card_h < 430 ? 31 : 36;
        int hint_body_w = g.sidebar.w - hint_margin * 2 - 20;
        int hint_body_h = hint_card_h - hint_button_h * 4 - 78;
        assert(hint_body_w > 8 && hint_body_h > 8);
        assert(fits(font, tr((Language)language, T_HINT_PREVIEW), 20, 10,
                    hint_body_w, 32));
        assert(fits(font, tr((Language)language, T_APPLY), 16, 9,
                    hint_body_w, hint_button_h));
        assert(fits(font, tr((Language)language, T_VERIFY), 16, 9,
                    hint_body_w, hint_button_h));
        assert(fits(font, tr((Language)language, T_REVEAL_CELL), 16, 9,
                    hint_body_w, hint_button_h));
        assert(fits(font, tr((Language)language, T_CANCEL), 16, 9,
                    hint_body_w, hint_button_h));

        char hint_message[320];
        snprintf(hint_message, sizeof hint_message,
                 tr((Language)language, T_HINT_CHAIN_FMT),
                 tr((Language)language, T_TECH_LOCKED_CANDIDATE), 9, 9, 9);
        size_t hint_length = strlen(hint_message);
        snprintf(hint_message + hint_length,
                 sizeof hint_message - hint_length, "\n%s",
                 tr((Language)language, T_HINT_MARKER_LEGEND));
        assert(wrapped_fits(font, hint_message, 15, 9,
                            hint_body_w, hint_body_h));
        assert(wrapped_fits(font, tr((Language)language, T_HINT_CONTRADICTION),
                            15, 9, hint_body_w, hint_body_h));
        assert(wrapped_fits(font, tr((Language)language, T_HINT_STALLED),
                            15, 9, hint_body_w, hint_body_h));
        assert(wrapped_fits(font, tr((Language)language, T_HINT_NEEDS_VERIFY),
                            15, 9, hint_body_w, hint_body_h));

        assert(fits(font, tr((Language)language, T_AUDIO), tier.control + 8, 10,
                    width < 640 ? width - 60 : 420, 42));
        assert(fits(font, tr((Language)language, T_MUSIC), tier.control, 10,
                    width < 640 ? width - 60 : 420, 24));
        assert(fits(font, tr((Language)language, T_FX), tier.control, 10,
                    width < 640 ? width - 60 : 420, 24));
      }
    }
  }

  TTF_Quit();
  puts("SDL_ttf text-fit tests passed for hierarchical gameplay, readable About copy, Audio labels and all responsive tiers");
  return 0;
}
