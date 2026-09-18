#ifndef SUDOKURA_HUMAN_H
#define SUDOKURA_HUMAN_H

#include <stdbool.h>
#include <stdint.h>

#define HUMAN_CELL_COUNT 81
#define HUMAN_TECHNIQUE_COUNT 7
#define HUMAN_STEP_SOURCE_MAX 4
#define HUMAN_STEP_AFFECTED_MAX 24
#define HUMAN_EXPLANATION_CAPACITY 192

typedef enum {
  HUMAN_TECHNIQUE_NONE = 0,
  HUMAN_TECHNIQUE_NAKED_SINGLE,
  HUMAN_TECHNIQUE_HIDDEN_SINGLE,
  HUMAN_TECHNIQUE_LOCKED_CANDIDATE,
  HUMAN_TECHNIQUE_NAKED_PAIR,
  HUMAN_TECHNIQUE_NAKED_TRIPLE,
  HUMAN_TECHNIQUE_X_WING
} HumanTechnique;

typedef enum {
  HUMAN_UNIT_NONE = 0,
  HUMAN_UNIT_ROW,
  HUMAN_UNIT_COLUMN,
  HUMAN_UNIT_BOX
} HumanUnitType;

typedef enum {
  HUMAN_RATING_UNSUPPORTED = 0,
  HUMAN_RATING_EASY,
  HUMAN_RATING_MEDIUM,
  HUMAN_RATING_HARD
} HumanRating;

typedef struct {
  HumanTechnique technique;
  HumanUnitType unit_type;
  int unit_index;
  int placement_cell;
  int placement_value;
  uint16_t value_mask;
  int source_count;
  int source_cells[HUMAN_STEP_SOURCE_MAX];
  int affected_count;
  int affected_cells[HUMAN_STEP_AFFECTED_MAX];
  char explanation[HUMAN_EXPLANATION_CAPACITY];
} HumanStep;

typedef struct {
  bool valid;
  bool solved;
  bool stalled;
  HumanTechnique max_technique;
  HumanRating rating;
  int total_steps;
  int placements;
  int elimination_steps;
  int longest_elimination_run;
  int technique_steps[HUMAN_TECHNIQUE_COUNT];
} HumanEvaluation;

typedef enum {
  HUMAN_HINT_INVALID = 0,
  HUMAN_HINT_SOLVED,
  HUMAN_HINT_LOGICAL,
  HUMAN_HINT_STALLED
} HumanHintStatus;

typedef struct {
  HumanHintStatus status;
  HumanStep reasoning;
  HumanStep placement;
  int reasoning_steps;
  int contradiction_cell;
} HumanHint;

const char *human_technique_name(HumanTechnique technique);
bool human_evaluate(const int puzzle[HUMAN_CELL_COUNT], HumanEvaluation *out);
bool human_hint_analyze(const int puzzle[HUMAN_CELL_COUNT], HumanHint *out);
bool human_evaluate_with_last_step(const int puzzle[HUMAN_CELL_COUNT],
                                   HumanEvaluation *out,
                                   HumanStep *last_step);

#endif
