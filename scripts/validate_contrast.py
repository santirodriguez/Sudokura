#!/usr/bin/env python3
"""Verify Sudokura's documented WCAG-inspired design contrast targets."""
from pathlib import Path
import re

source = Path("src/sudokura_sdl/01_runtime.inc").read_text(encoding="utf-8")


def theme(name):
    match = re.search(
        rf"static Theme theme_{name}\(void\)\{{\s*Theme t=\{{(.*?)\n\s*\}};",
        source,
        re.S,
    )
    assert match, f"theme_{name} not found"
    values = {}
    for key, r, g, b, a in re.findall(
        r"\.([a-z_]+)=\{(\d+),(\d+),(\d+),(\d+)\}", match.group(1)
    ):
        values[key] = tuple(map(int, (r, g, b, a)))
    return values


def channel(value):
    component = value / 255.0
    return (
        component / 12.92
        if component <= 0.04045
        else ((component + 0.055) / 1.055) ** 2.4
    )


def luminance(rgb):
    r, g, b = map(channel, rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def ratio(a, b):
    first, second = luminance(a), luminance(b)
    return (max(first, second) + 0.05) / (min(first, second) + 0.05)


def composite(foreground, background_rgb):
    alpha = foreground[3] / 255.0
    return tuple(
        foreground[index] * alpha + background_rgb[index] * (1.0 - alpha)
        for index in range(3)
    )


def visible_ratio(foreground, background):
    background_rgb = background[:3]
    return ratio(composite(foreground, background_rgb), background_rgb)


def layered_background(overlay, background_rgb, alpha=None):
    rgba = overlay if alpha is None else (*overlay[:3], alpha)
    return composite(rgba, background_rgb)


normal_text = [
    ("btnfg", "btn"),
    ("dim", "bg"),
    ("title", "bg"),
    ("bg", "title"),
    ("palette_fg", "palette_bg"),
    ("text_given", "board"),
    ("text_edit", "board"),
    ("text_wrong", "board"),
]
progress_values = (0, 49, 52, 53, 100)

for name in ("dark", "light"):
    colors = theme(name)

    for foreground, background in normal_text:
        value = visible_ratio(colors[foreground], colors[background])
        assert value >= 4.5, (name, foreground, background, value)

    board = colors["board"][:3]
    one_highlight = layered_background(colors["boxhl"], board)
    two_highlights = layered_background(colors["boxhl"], one_highlight)
    selected_region = layered_background(
        colors["boxhl"], two_highlights, colors["boxhl"][3] // 2
    )
    grid_backgrounds = (
        ("board", board),
        ("row_or_column_highlight", one_highlight),
        ("row_and_column_highlight", two_highlights),
        ("selected_box_intersection", selected_region),
    )
    for grid in ("thin", "thick"):
        for background_name, background_rgb in grid_backgrounds:
            value = ratio(composite(colors[grid], background_rgb), background_rgb)
            assert value >= 3.0, (name, grid, background_name, value)

    selected_surface = layered_background(colors["sel"], board)
    focus_backgrounds = {
        "background": colors["bg"][:3],
        "board": board,
        "selected_cell": selected_surface,
        "button": colors["btn"][:3],
        "palette": colors["palette_bg"][:3],
    }
    for background_name, background_rgb in focus_backgrounds.items():
        value = ratio(
            composite(colors["sel_outline"], background_rgb), background_rgb
        )
        assert value >= 3.0, (name, "sel_outline", background_name, value)

    fill_contrast = visible_ratio(colors["title"], colors["palette_bg"])
    assert fill_contrast >= 3.0, (name, "progress_fill", fill_contrast)

    label_contrast = visible_ratio(colors["btnfg"], colors["btn"])
    for progress in progress_values:
        assert label_contrast >= 4.5, (
            name,
            "progress_label",
            progress,
            label_contrast,
        )

print(
    "dark/light effective contrast passed: text >=4.5:1; "
    "functional grid/focus/progress >=3:1 across alpha-composited states"
)
