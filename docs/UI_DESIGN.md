# Shared UI design

The visual refresh is introduced screen by screen. Home and About are its first
consumers; gameplay, libraries, Settings and other screens retain their existing
presentation until their own migration. No game, save, input or release identity
is changed by adopting the visual layer.

## Owners

- `src/ui_design.h`: semantic dark/light colors, button states and icon/label spacing.
- `src/sudokura_sdl/ui_design_draw.h`: original vector icons and rounded gradient
  surfaces using the existing SDL2 renderer, with an output-pixel coverage fringe.
- `src/sudokura_sdl/ui_design.inc`: Home/About painters, measured mobile About
  cards and the staged rendering entry point.
- `src/geometry.[ch]`: unchanged canonical interactive rectangles. Painting,
  pointer capture, keyboard focus and Android semantic actions use the same map.
- `src/i18n.h`: localized public publisher credit. Existing copy and content
  translations remain in `src/i18n.c`; source/license attribution is unaffected.

The composition file names the previous presentation dispatcher
`app_render_existing_ui`. The single public `app_render_ui` delegates unchanged
screens to it and retains the existing overlay order for migrated screens.
There is no second event loop, new widget framework or persistent rollout flag.
Do not redirect input or persistence through this presentation seam. Consolidate
screen painters as migration proceeds rather than duplicating command logic.

## Visual behavior

Use dark navy/indigo or the independently contrasted light palette. Panels have
restrained rims and depth; buttons distinguish hover, pressed, keyboard focus
and disabled states. Icons share a normalized 24-unit original vector family.
No icon font, third-party artwork, remote fetch or new runtime dependency is used.

Shapes have bounded geometry and do not own SDL textures. They survive renderer
recreation without a retained-resource cache. Text continues to use the existing
bounded font/texture cache. The established dirty/wait loop and reduced-motion
policy remain unchanged; this layer introduces no continuous animation.

Mobile About measures real wrapped text, derives card heights, and scrolls only
its existing body viewport. Navigation, four external links and scroll actions
stay in their original hit rectangles. Its semantic paragraphs use the actual
painted text bounds. Credits show Caja Personal while retaining GPLv3 and the
music/source credits. The actual branding masters are not modified.

## Validation

`make test` covers token contrast, every button-state combination, small/odd label
bounds and localized credits. `make test-ui` also exercises all vector glyphs and
surfaces with SDL's software renderer at 1x/2x/3x and real SDL_ttf fitting for
localized About controls. The normal core/geometry/input/accessibility tests
remain enabled. Use the existing `[ui-review]` CI capture to inspect actual app
pixels in both themes and all supported languages; a concept image is not app
acceptance. Physical Android, complete TalkBack and release approval are distinct.
