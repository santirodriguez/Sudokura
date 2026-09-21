#include "human.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void parse_puzzle(const char *text, int board[81]) {
  assert(text);
  assert(strlen(text) == 81);
  for (int i = 0; i < 81; ++i) {
    char ch = text[i];
    assert(ch == '.' || (ch >= '1' && ch <= '9'));
    board[i] = ch == '.' ? 0 : ch - '0';
  }
}

static void test_visible_reasoning_only(void) {
  static const char *easy =
      "53..7...."
      "6..195..."
      ".98....6."
      "8...6...3"
      "4..8.3..1"
      "7...2...6"
      ".6....28."
      "...419..5"
      "....8..79";
  int board[81];
  parse_puzzle(easy, board);

  HumanEvaluation evaluation;
  HumanStep last;
  assert(human_evaluate_with_last_step(board, &evaluation, &last));
  assert(evaluation.valid);
  assert(evaluation.solved);
  assert(!evaluation.stalled);
  assert(evaluation.rating == HUMAN_RATING_EASY);
  assert(evaluation.max_technique <= HUMAN_TECHNIQUE_HIDDEN_SINGLE);
  assert(evaluation.placements > 0);
  assert(evaluation.total_steps == evaluation.placements);
  assert(last.technique != HUMAN_TECHNIQUE_NONE);
  assert(last.placement_cell >= 0);
  assert(last.placement_value >= 1 && last.placement_value <= 9);
  assert(last.affected_count >= 1);
  assert(last.explanation[0] != '\0');
}

static void test_actionable_hint_states(void) {
  static const char *easy =
      "53..7...."
      "6..195..."
      ".98....6."
      "8...6...3"
      "4..8.3..1"
      "7...2...6"
      ".6....28."
      "...419..5"
      "....8..79";
  int board[81];
  parse_puzzle(easy, board);
  HumanHint hint;
  assert(human_hint_analyze(board, &hint));
  assert(hint.status == HUMAN_HINT_LOGICAL);
  assert(hint.reasoning.technique != HUMAN_TECHNIQUE_NONE);
  assert(hint.placement.placement_cell >= 0);
  assert(hint.placement.placement_value >= 1 &&
         hint.placement.placement_value <= 9);
  assert(hint.reasoning_steps >= 1);

  int contradiction[81] = {0};
  contradiction[0] = 4;
  contradiction[1] = 4;
  assert(human_hint_analyze(contradiction, &hint));
  assert(hint.status == HUMAN_HINT_INVALID);
  assert(hint.contradiction_cell == 1);

  int empty[81] = {0};
  assert(human_hint_analyze(empty, &hint));
  assert(hint.status == HUMAN_HINT_STALLED);

  static const char *solved =
      "534678912"
      "672195348"
      "198342567"
      "859761423"
      "426853791"
      "713924856"
      "961537284"
      "287419635"
      "345286179";
  parse_puzzle(solved, board);
  assert(human_hint_analyze(board, &hint));
  assert(hint.status == HUMAN_HINT_SOLVED);
}

static void test_invalid_and_names(void) {
  int invalid[81] = {0};
  invalid[0] = 1;
  invalid[1] = 1;
  HumanEvaluation evaluation;
  assert(!human_evaluate(invalid, &evaluation));
  assert(!evaluation.valid);

  assert(!strcmp(human_technique_name(HUMAN_TECHNIQUE_NAKED_SINGLE),
                 "naked single"));
  assert(!strcmp(human_technique_name(HUMAN_TECHNIQUE_HIDDEN_SINGLE),
                 "hidden single"));
  assert(!strcmp(human_technique_name(HUMAN_TECHNIQUE_LOCKED_CANDIDATE),
                 "locked candidate"));
  assert(!strcmp(human_technique_name(HUMAN_TECHNIQUE_NAKED_PAIR),
                 "naked pair"));
  assert(!strcmp(human_technique_name(HUMAN_TECHNIQUE_NAKED_TRIPLE),
                 "naked triple"));
  assert(!strcmp(human_technique_name(HUMAN_TECHNIQUE_X_WING), "X-Wing"));
}

int main(void) {
  test_visible_reasoning_only();
  test_actionable_hint_states();
  test_invalid_and_names();
  puts("human evaluator uses visible candidates and emits structured explanations");
  return 0;
}
