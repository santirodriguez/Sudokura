#!/usr/bin/env python3
"""Fast replay/save scene contract tests; no Android SDK or image tools."""
import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'android' / 'scripts'))
from store_capture_flow import (board_ready, has_button, keep_current_ready,
                                replay_confirmation_ready, storage_failure)


def tree(*items):
    root = ET.Element('hierarchy')
    for title, kind in items:
        ET.SubElement(root, 'node', {
            'content-desc': title,
            'class': 'android.widget.Button' if kind == 'button' else 'android.widget.TextView',
            'enabled': 'true',
            'clickable': 'true' if kind == 'button' else 'false',
        })
    return root


class StoreCaptureFlowTests(unittest.TestCase):
    def test_replay_requires_a_clickable_button_not_the_duplicate_title(self):
        title_only = tree(('Replay', 'title'), ('Cancel', 'button'))
        self.assertFalse(replay_confirmation_ready(title_only))
        modal = tree(('Replay', 'title'), ('Cancel', 'button'), ('Replay', 'button'))
        self.assertTrue(replay_confirmation_ready(modal))
        self.assertTrue(has_button(modal, 'Replay'))
        self.assertFalse(keep_current_ready(modal))

    def test_save_and_leave_is_explicit_and_never_abandon(self):
        modal = tree(('Keep your current game?', 'title'),
                     ('Save and leave', 'button'), ('Abandon attempt', 'button'),
                     ('Cancel', 'button'))
        self.assertTrue(keep_current_ready(modal))
        self.assertFalse(replay_confirmation_ready(modal))
        self.assertFalse(keep_current_ready(tree(('Keep your current game?', 'title'),
                                                 ('Save and leave', 'title'),
                                                 ('Cancel', 'button'))))

    def test_actual_storage_recovery_is_a_hard_failure_not_a_board(self):
        message = ('Saving was not completed. Retry saving or go back to Games; '
                   'the current game remains in memory.')
        error = tree(('Storage error', 'title'), (message, 'title'),
                     ('Back', 'button'), ('Retry save', 'button'))
        self.assertEqual(storage_failure(error), message)
        self.assertFalse(board_ready(error))
        self.assertFalse(keep_current_ready(error))
        self.assertFalse(replay_confirmation_ready(error))

    def test_board_requires_81_distinct_actual_cell_descriptions(self):
        items = [(f'Row {i // 9 + 1}, column {i % 9 + 1}. Empty', 'title')
                 for i in range(81)]
        full = tree(*items)
        self.assertTrue(board_ready(full))
        self.assertIsNone(storage_failure(full))
        self.assertFalse(board_ready(tree(*items[:-1])))
        self.assertFalse(board_ready(tree(*items[:-1], items[0])))

    def test_screenshot_branch_skips_full_matrix_and_preflights_before_emulator(self):
        ci = (ROOT / '.github' / 'workflows' / 'ci.yml').read_text()
        guard = "!startsWith(github.ref, 'refs/heads/store-screenshots/')"
        for job in ('linux', 'macos', 'windows'):
            section = ci.split('  ' + job + ':\n', 1)[1].split('\n  ', 1)[0]
            self.assertIn(guard, section, job)
        self.assertIn('  store-preflight:\n', ci)
        self.assertIn('  store-screenshots:\n    needs: store-preflight', ci)
        self.assertIn("needs['store-preflight'].result == 'success'", ci)
        self.assertIn('make WERROR=-Werror tests/test_live_save tests/test_save_policy tests/test_app_modal', ci)
        self.assertIn('timeout-minutes: 15', ci)

    def test_terminal_attempt_does_not_request_replay_retention(self):
        code = (ROOT / 'src' / 'sudokura_sdl' / 'games_ui.inc').read_text()
        helper = code.split('static bool games_browser_replay_keep_required(', 1)[1]
        helper = helper.split('static bool games_browser_replay_preflight(', 1)[0]
        self.assertIn('slot->value.session.status == SESSION_ACTIVE', helper)
        self.assertIn('!slot->value.explicitly_saved && slot->value.started', helper)
        self.assertIn('if (games_browser_replay_keep_required(normal))', code)
        self.assertIn('if (!games_browser_replay_keep_required(normal)) return false;', code)
        smoke = (ROOT / 'src' / 'sudokura_sdl' / '05_main.inc').read_text()
        self.assertIn('Replay terminal-win keep regression', smoke)
        self.assertIn('Replay terminal-interrupted keep regression', smoke)

    def test_daily_hud_shows_the_real_session_date(self):
        render = (ROOT / 'src' / 'sudokura_sdl' / '03_board_render.inc').read_text()
        self.assertIn('"%s %04d-%02d-%02d"', render)
        for field in ('ui->daily_year', 'ui->daily_month', 'ui->daily_day'):
            self.assertIn(field, render)
        self.assertIn('ui->is_daily ? daily_title : mode_name', render)

    def test_status_bar_is_cleaned_before_first_original_capture(self):
        script = (ROOT / 'android' / 'scripts' / 'capture_store_screenshots.py').read_text()
        self.assertIn('sysui_demo_allowed', script)
        self.assertIn('self.shell(*action, "notifications", "-e", "visible", "false")', script)
        self.assertLess(script.index('self.clean_emulator_status_bar()'),
                        script.index('self.capture(ROLES[0]'))

    def test_capture_accepts_direct_replay_or_legitimate_keep_and_aborts_errors(self):
        code = (ROOT / 'android' / 'scripts' / 'capture_store_screenshots.py').read_text()
        stages = (
            'self.wait("replay-confirmation", replay_confirmation_ready)',
            'self.press(confirmation, label="Replay", last=True)',
            'self.wait("replay-activation",',
            'if keep_current_ready(activation):',
            'self.press(activation, label="Save and leave")',
            'self.board("replayed-real-puzzle")',
        )
        positions = [code.index(marker) for marker in stages]
        self.assertEqual(positions, sorted(positions))
        self.assertIn('storage_failure(tree)', code)
        self.assertIn('self.evidence_png(name + "-storage-error")', code)
        self.assertNotIn('self.press(activation, label="Abandon attempt")', code)


if __name__ == '__main__':
    unittest.main()
