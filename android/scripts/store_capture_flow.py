"""Small, emulator-free predicates for the Android screenshot replay/save gates.

Only actual Android accessibility trees authorize navigation. Failure or
recovery screens must never be mistaken for a playable Sudoku board.
"""

import re


CELL = re.compile(r"^Row ([1-9]), column ([1-9])\. (Given|Entered|Empty)(?: ([1-9]))?")


def labels(tree):
    return [node.get("content-desc", "") for node in tree.iter("node")]


def has_button(tree, title):
    return any(
        node.get("content-desc") == title
        and node.get("class") == "android.widget.Button"
        and node.get("enabled") == "true"
        and node.get("clickable") == "true"
        for node in tree.iter("node")
    )


def replay_confirmation_ready(tree):
    return has_button(tree, "Replay") and has_button(tree, "Cancel")


def keep_current_ready(tree):
    return (
        "Keep your current game?" in labels(tree)
        and has_button(tree, "Save and leave")
        and has_button(tree, "Cancel")
    )


def storage_failure(tree):
    names = labels(tree)
    if "Storage error" not in names:
        return None
    return next((name for name in names if "Saving was not completed" in name),
                "Storage error")


def board_ready(tree):
    positions = set()
    for name in labels(tree):
        cell = CELL.match(name)
        if cell:
            positions.add((int(cell.group(1)), int(cell.group(2))))
    return len(positions) == 81
