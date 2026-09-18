#include "human.h"

#include <stdio.h>
#include <string.h>

#define HUMAN_FULL_MASK UINT16_C(0x03fe)
#define HUMAN_MAX_STEPS 4096
#define HUMAN_IDX(r, c) ((r) * 9 + (c))

typedef struct {
  int value[HUMAN_CELL_COUNT];
  uint16_t candidates[HUMAN_CELL_COUNT];
} HumanState;

static int bit_count(uint16_t mask) {
  int count = 0;
  while (mask) {
    mask &= (uint16_t)(mask - 1u);
    ++count;
  }
  return count;
}

static int single_value(uint16_t mask) {
  if (bit_count(mask) != 1) return 0;
  for (int value = 1; value <= 9; ++value)
    if (mask == (uint16_t)(1u << value)) return value;
  return 0;
}

static int box_index(int row, int column) {
  return (row / 3) * 3 + column / 3;
}

static bool peer(int a, int b) {
  if (a == b) return false;
  int ar = a / 9, ac = a % 9, br = b / 9, bc = b % 9;
  return ar == br || ac == bc || box_index(ar, ac) == box_index(br, bc);
}

static uint16_t used_mask(const HumanState *state, int index) {
  uint16_t used = 0;
  for (int other = 0; other < HUMAN_CELL_COUNT; ++other) {
    if (peer(index, other) && state->value[other] >= 1 &&
        state->value[other] <= 9)
      used |= (uint16_t)(1u << state->value[other]);
  }
  return used;
}

static bool state_valid(const HumanState *state) {
  for (int i = 0; i < HUMAN_CELL_COUNT; ++i) {
    if (state->value[i] < 0 || state->value[i] > 9) return false;
    if (state->value[i]) {
      for (int j = i + 1; j < HUMAN_CELL_COUNT; ++j)
        if (peer(i, j) && state->value[i] == state->value[j]) return false;
    } else if ((state->candidates[i] & HUMAN_FULL_MASK) == 0) {
      return false;
    }
  }
  return true;
}

static bool state_init(HumanState *state, const int puzzle[HUMAN_CELL_COUNT]) {
  if (!state || !puzzle) return false;
  memset(state, 0, sizeof(*state));
  for (int i = 0; i < HUMAN_CELL_COUNT; ++i) {
    if (puzzle[i] < 0 || puzzle[i] > 9) return false;
    state->value[i] = puzzle[i];
  }
  for (int i = 0; i < HUMAN_CELL_COUNT; ++i) {
    if (state->value[i]) {
      state->candidates[i] = 0;
    } else {
      state->candidates[i] =
          (uint16_t)(HUMAN_FULL_MASK & (uint16_t)~used_mask(state, i));
    }
  }
  return state_valid(state);
}

static void step_reset(HumanStep *step, HumanTechnique technique) {
  if (!step) return;
  memset(step, 0, sizeof(*step));
  step->technique = technique;
  step->unit_type = HUMAN_UNIT_NONE;
  step->unit_index = -1;
  step->placement_cell = -1;
}

static void step_add_source(HumanStep *step, int cell) {
  if (!step || cell < 0 || cell >= HUMAN_CELL_COUNT) return;
  for (int i = 0; i < step->source_count; ++i)
    if (step->source_cells[i] == cell) return;
  if (step->source_count < HUMAN_STEP_SOURCE_MAX)
    step->source_cells[step->source_count++] = cell;
}

static void step_add_affected(HumanStep *step, int cell) {
  if (!step || cell < 0 || cell >= HUMAN_CELL_COUNT) return;
  for (int i = 0; i < step->affected_count; ++i)
    if (step->affected_cells[i] == cell) return;
  if (step->affected_count < HUMAN_STEP_AFFECTED_MAX)
    step->affected_cells[step->affected_count++] = cell;
}

static bool place_value(HumanState *state, int cell, int value) {
  if (!state || cell < 0 || cell >= HUMAN_CELL_COUNT || value < 1 ||
      value > 9 || state->value[cell] != 0 ||
      (state->candidates[cell] & (uint16_t)(1u << value)) == 0)
    return false;
  state->value[cell] = value;
  state->candidates[cell] = 0;
  uint16_t bit = (uint16_t)(1u << value);
  for (int other = 0; other < HUMAN_CELL_COUNT; ++other)
    if (peer(cell, other) && state->value[other] == 0)
      state->candidates[other] &= (uint16_t)~bit;
  return state_valid(state);
}

static bool eliminate_mask(HumanState *state, HumanStep *step, int cell,
                           uint16_t mask) {
  if (!state || !step || cell < 0 || cell >= HUMAN_CELL_COUNT ||
      state->value[cell] != 0)
    return false;
  uint16_t before = state->candidates[cell];
  uint16_t after = (uint16_t)(before & (uint16_t)~mask);
  if (after == before || after == 0) return false;
  state->candidates[cell] = after;
  step_add_affected(step, cell);
  return true;
}

static void unit_cells(HumanUnitType type, int unit, int cells[9]) {
  if (type == HUMAN_UNIT_ROW) {
    for (int i = 0; i < 9; ++i) cells[i] = HUMAN_IDX(unit, i);
  } else if (type == HUMAN_UNIT_COLUMN) {
    for (int i = 0; i < 9; ++i) cells[i] = HUMAN_IDX(i, unit);
  } else {
    int row = (unit / 3) * 3, column = (unit % 3) * 3;
    for (int i = 0; i < 9; ++i)
      cells[i] = HUMAN_IDX(row + i / 3, column + i % 3);
  }
}

static bool find_naked_single(HumanState *state, HumanStep *step) {
  for (int cell = 0; cell < HUMAN_CELL_COUNT; ++cell) {
    if (state->value[cell]) continue;
    int value = single_value(state->candidates[cell]);
    if (!value) continue;
    step_reset(step, HUMAN_TECHNIQUE_NAKED_SINGLE);
    step->placement_cell = cell;
    step->placement_value = value;
    step->value_mask = (uint16_t)(1u << value);
    step_add_source(step, cell);
    step_add_affected(step, cell);
    snprintf(step->explanation, sizeof(step->explanation),
             "Cell r%dc%d has only candidate %d.", cell / 9 + 1,
             cell % 9 + 1, value);
    return place_value(state, cell, value);
  }
  return false;
}

static bool find_hidden_single(HumanState *state, HumanStep *step) {
  const HumanUnitType types[] = {
      HUMAN_UNIT_ROW, HUMAN_UNIT_COLUMN, HUMAN_UNIT_BOX};
  for (unsigned type_index = 0;
       type_index < sizeof(types) / sizeof(types[0]); ++type_index) {
    for (int unit = 0; unit < 9; ++unit) {
      int cells[9];
      unit_cells(types[type_index], unit, cells);
      for (int value = 1; value <= 9; ++value) {
        uint16_t bit = (uint16_t)(1u << value);
        int target = -1, count = 0;
        for (int i = 0; i < 9; ++i) {
          int cell = cells[i];
          if (!state->value[cell] && (state->candidates[cell] & bit)) {
            target = cell;
            ++count;
          }
        }
        if (count != 1) continue;
        step_reset(step, HUMAN_TECHNIQUE_HIDDEN_SINGLE);
        step->unit_type = types[type_index];
        step->unit_index = unit;
        step->placement_cell = target;
        step->placement_value = value;
        step->value_mask = bit;
        step_add_source(step, target);
        step_add_affected(step, target);
        snprintf(step->explanation, sizeof(step->explanation),
                 "Candidate %d appears in only one cell of this %s.", value,
                 types[type_index] == HUMAN_UNIT_ROW
                     ? "row"
                     : types[type_index] == HUMAN_UNIT_COLUMN ? "column"
                                                              : "box");
        return place_value(state, target, value);
      }
    }
  }
  return false;
}

static bool locked_from_box(HumanState *state, HumanStep *step) {
  for (int box = 0; box < 9; ++box) {
    int cells[9];
    unit_cells(HUMAN_UNIT_BOX, box, cells);
    for (int value = 1; value <= 9; ++value) {
      uint16_t bit = (uint16_t)(1u << value);
      int sources[9], source_count = 0;
      int shared_row = -1, shared_column = -1;
      bool same_row = true, same_column = true;
      for (int i = 0; i < 9; ++i) {
        int cell = cells[i];
        if (state->value[cell] || !(state->candidates[cell] & bit)) continue;
        sources[source_count++] = cell;
        int row = cell / 9, column = cell % 9;
        if (shared_row < 0) shared_row = row;
        else if (shared_row != row) same_row = false;
        if (shared_column < 0) shared_column = column;
        else if (shared_column != column) same_column = false;
      }
      if (source_count < 2) continue;
      step_reset(step, HUMAN_TECHNIQUE_LOCKED_CANDIDATE);
      step->unit_type = HUMAN_UNIT_BOX;
      step->unit_index = box;
      step->value_mask = bit;
      for (int i = 0; i < source_count; ++i) step_add_source(step, sources[i]);
      if (same_row) {
        for (int column = 0; column < 9; ++column) {
          int cell = HUMAN_IDX(shared_row, column);
          if (box_index(shared_row, column) == box) continue;
          (void)eliminate_mask(state, step, cell, bit);
        }
      }
      if (step->affected_count == 0 && same_column) {
        for (int row = 0; row < 9; ++row) {
          int cell = HUMAN_IDX(row, shared_column);
          if (box_index(row, shared_column) == box) continue;
          (void)eliminate_mask(state, step, cell, bit);
        }
      }
      if (step->affected_count > 0) {
        snprintf(step->explanation, sizeof(step->explanation),
                 "Candidate %d is locked inside one line of box %d.", value,
                 box + 1);
        return state_valid(state);
      }
    }
  }
  return false;
}

static bool locked_from_line(HumanState *state, HumanStep *step,
                             HumanUnitType type) {
  for (int unit = 0; unit < 9; ++unit) {
    int cells[9];
    unit_cells(type, unit, cells);
    for (int value = 1; value <= 9; ++value) {
      uint16_t bit = (uint16_t)(1u << value);
      int sources[9], source_count = 0, shared_box = -1;
      bool same_box = true;
      for (int i = 0; i < 9; ++i) {
        int cell = cells[i];
        if (state->value[cell] || !(state->candidates[cell] & bit)) continue;
        sources[source_count++] = cell;
        int box = box_index(cell / 9, cell % 9);
        if (shared_box < 0) shared_box = box;
        else if (shared_box != box) same_box = false;
      }
      if (source_count < 2 || !same_box) continue;
      step_reset(step, HUMAN_TECHNIQUE_LOCKED_CANDIDATE);
      step->unit_type = type;
      step->unit_index = unit;
      step->value_mask = bit;
      for (int i = 0; i < source_count; ++i) step_add_source(step, sources[i]);
      int box_cells[9];
      unit_cells(HUMAN_UNIT_BOX, shared_box, box_cells);
      for (int i = 0; i < 9; ++i) {
        int cell = box_cells[i];
        bool in_line =
            type == HUMAN_UNIT_ROW ? cell / 9 == unit : cell % 9 == unit;
        if (!in_line) (void)eliminate_mask(state, step, cell, bit);
      }
      if (step->affected_count > 0) {
        snprintf(step->explanation, sizeof(step->explanation),
                 "Candidate %d in this %s is confined to box %d.", value,
                 type == HUMAN_UNIT_ROW ? "row" : "column", shared_box + 1);
        return state_valid(state);
      }
    }
  }
  return false;
}

static bool find_locked_candidate(HumanState *state, HumanStep *step) {
  return locked_from_box(state, step) ||
         locked_from_line(state, step, HUMAN_UNIT_ROW) ||
         locked_from_line(state, step, HUMAN_UNIT_COLUMN);
}

static bool find_naked_pair(HumanState *state, HumanStep *step) {
  const HumanUnitType types[] = {
      HUMAN_UNIT_ROW, HUMAN_UNIT_COLUMN, HUMAN_UNIT_BOX};
  for (unsigned type_index = 0;
       type_index < sizeof(types) / sizeof(types[0]); ++type_index) {
    for (int unit = 0; unit < 9; ++unit) {
      int cells[9];
      unit_cells(types[type_index], unit, cells);
      for (int a = 0; a < 8; ++a) {
        int first = cells[a];
        uint16_t mask = state->candidates[first];
        if (state->value[first] || bit_count(mask) != 2) continue;
        for (int b = a + 1; b < 9; ++b) {
          int second = cells[b];
          if (state->value[second] || state->candidates[second] != mask)
            continue;
          step_reset(step, HUMAN_TECHNIQUE_NAKED_PAIR);
          step->unit_type = types[type_index];
          step->unit_index = unit;
          step->value_mask = mask;
          step_add_source(step, first);
          step_add_source(step, second);
          for (int i = 0; i < 9; ++i) {
            int cell = cells[i];
            if (cell != first && cell != second)
              (void)eliminate_mask(state, step, cell, mask);
          }
          if (step->affected_count > 0) {
            snprintf(step->explanation, sizeof(step->explanation),
                     "Two cells in this unit share the same two candidates.");
            return state_valid(state);
          }
        }
      }
    }
  }
  return false;
}

static bool find_naked_triple(HumanState *state, HumanStep *step) {
  const HumanUnitType types[] = {
      HUMAN_UNIT_ROW, HUMAN_UNIT_COLUMN, HUMAN_UNIT_BOX};
  for (unsigned type_index = 0;
       type_index < sizeof(types) / sizeof(types[0]); ++type_index) {
    for (int unit = 0; unit < 9; ++unit) {
      int cells[9];
      unit_cells(types[type_index], unit, cells);
      for (int a = 0; a < 7; ++a) {
        int ca = cells[a];
        int count_a = bit_count(state->candidates[ca]);
        if (state->value[ca] || count_a < 2 || count_a > 3) continue;
        for (int b = a + 1; b < 8; ++b) {
          int cb = cells[b];
          int count_b = bit_count(state->candidates[cb]);
          if (state->value[cb] || count_b < 2 || count_b > 3) continue;
          for (int d = b + 1; d < 9; ++d) {
            int cd = cells[d];
            int count_d = bit_count(state->candidates[cd]);
            if (state->value[cd] || count_d < 2 || count_d > 3) continue;
            uint16_t mask = (uint16_t)(state->candidates[ca] |
                                       state->candidates[cb] |
                                       state->candidates[cd]);
            if (bit_count(mask) != 3) continue;
            step_reset(step, HUMAN_TECHNIQUE_NAKED_TRIPLE);
            step->unit_type = types[type_index];
            step->unit_index = unit;
            step->value_mask = mask;
            step_add_source(step, ca);
            step_add_source(step, cb);
            step_add_source(step, cd);
            for (int i = 0; i < 9; ++i) {
              int cell = cells[i];
              if (cell != ca && cell != cb && cell != cd)
                (void)eliminate_mask(state, step, cell, mask);
            }
            if (step->affected_count > 0) {
              snprintf(step->explanation, sizeof(step->explanation),
                       "Three cells in this unit are restricted to three candidates.");
              return state_valid(state);
            }
          }
        }
      }
    }
  }
  return false;
}

static bool find_x_wing_rows(HumanState *state, HumanStep *step, int value) {
  uint16_t bit = (uint16_t)(1u << value);
  int row_columns[9][2], row_counts[9] = {0};
  for (int row = 0; row < 9; ++row) {
    for (int column = 0; column < 9; ++column) {
      int cell = HUMAN_IDX(row, column);
      if (!state->value[cell] && (state->candidates[cell] & bit) &&
          row_counts[row] < 3) {
        if (row_counts[row] < 2)
          row_columns[row][row_counts[row]] = column;
        ++row_counts[row];
      }
    }
  }
  for (int first = 0; first < 8; ++first) {
    if (row_counts[first] != 2) continue;
    for (int second = first + 1; second < 9; ++second) {
      if (row_counts[second] != 2 ||
          row_columns[first][0] != row_columns[second][0] ||
          row_columns[first][1] != row_columns[second][1])
        continue;
      step_reset(step, HUMAN_TECHNIQUE_X_WING);
      step->unit_type = HUMAN_UNIT_ROW;
      step->unit_index = first;
      step->value_mask = bit;
      step_add_source(step, HUMAN_IDX(first, row_columns[first][0]));
      step_add_source(step, HUMAN_IDX(first, row_columns[first][1]));
      step_add_source(step, HUMAN_IDX(second, row_columns[first][0]));
      step_add_source(step, HUMAN_IDX(second, row_columns[first][1]));
      for (int row = 0; row < 9; ++row) {
        if (row == first || row == second) continue;
        (void)eliminate_mask(state, step,
                             HUMAN_IDX(row, row_columns[first][0]), bit);
        (void)eliminate_mask(state, step,
                             HUMAN_IDX(row, row_columns[first][1]), bit);
      }
      if (step->affected_count > 0) {
        snprintf(step->explanation, sizeof(step->explanation),
                 "Candidate %d forms an X-Wing across rows %d and %d.", value,
                 first + 1, second + 1);
        return state_valid(state);
      }
    }
  }
  return false;
}

static bool find_x_wing_columns(HumanState *state, HumanStep *step, int value) {
  uint16_t bit = (uint16_t)(1u << value);
  int column_rows[9][2], column_counts[9] = {0};
  for (int column = 0; column < 9; ++column) {
    for (int row = 0; row < 9; ++row) {
      int cell = HUMAN_IDX(row, column);
      if (!state->value[cell] && (state->candidates[cell] & bit) &&
          column_counts[column] < 3) {
        if (column_counts[column] < 2)
          column_rows[column][column_counts[column]] = row;
        ++column_counts[column];
      }
    }
  }
  for (int first = 0; first < 8; ++first) {
    if (column_counts[first] != 2) continue;
    for (int second = first + 1; second < 9; ++second) {
      if (column_counts[second] != 2 ||
          column_rows[first][0] != column_rows[second][0] ||
          column_rows[first][1] != column_rows[second][1])
        continue;
      step_reset(step, HUMAN_TECHNIQUE_X_WING);
      step->unit_type = HUMAN_UNIT_COLUMN;
      step->unit_index = first;
      step->value_mask = bit;
      step_add_source(step, HUMAN_IDX(column_rows[first][0], first));
      step_add_source(step, HUMAN_IDX(column_rows[first][1], first));
      step_add_source(step, HUMAN_IDX(column_rows[first][0], second));
      step_add_source(step, HUMAN_IDX(column_rows[first][1], second));
      for (int column = 0; column < 9; ++column) {
        if (column == first || column == second) continue;
        (void)eliminate_mask(state, step,
                             HUMAN_IDX(column_rows[first][0], column), bit);
        (void)eliminate_mask(state, step,
                             HUMAN_IDX(column_rows[first][1], column), bit);
      }
      if (step->affected_count > 0) {
        snprintf(step->explanation, sizeof(step->explanation),
                 "Candidate %d forms an X-Wing across columns %d and %d.",
                 value, first + 1, second + 1);
        return state_valid(state);
      }
    }
  }
  return false;
}

static bool find_x_wing(HumanState *state, HumanStep *step) {
  for (int value = 1; value <= 9; ++value)
    if (find_x_wing_rows(state, step, value) ||
        find_x_wing_columns(state, step, value))
      return true;
  return false;
}

static bool find_next_step(HumanState *state, HumanStep *step) {
  return find_naked_single(state, step) ||
         find_hidden_single(state, step) ||
         find_locked_candidate(state, step) ||
         find_naked_pair(state, step) ||
         find_naked_triple(state, step) ||
         find_x_wing(state, step);
}

static bool state_solved(const HumanState *state) {
  for (int i = 0; i < HUMAN_CELL_COUNT; ++i)
    if (state->value[i] == 0) return false;
  return state_valid(state);
}

static HumanRating classify(const HumanEvaluation *evaluation) {
  if (!evaluation || !evaluation->solved) return HUMAN_RATING_UNSUPPORTED;
  if (evaluation->max_technique <= HUMAN_TECHNIQUE_HIDDEN_SINGLE)
    return HUMAN_RATING_EASY;
  if (evaluation->max_technique == HUMAN_TECHNIQUE_LOCKED_CANDIDATE &&
      evaluation->technique_steps[HUMAN_TECHNIQUE_LOCKED_CANDIDATE] > 0)
    return HUMAN_RATING_MEDIUM;
  if (evaluation->max_technique >= HUMAN_TECHNIQUE_NAKED_PAIR &&
      evaluation->max_technique <= HUMAN_TECHNIQUE_X_WING &&
      (evaluation->technique_steps[HUMAN_TECHNIQUE_NAKED_PAIR] > 0 ||
       evaluation->technique_steps[HUMAN_TECHNIQUE_NAKED_TRIPLE] > 0 ||
       evaluation->technique_steps[HUMAN_TECHNIQUE_X_WING] > 0))
    return HUMAN_RATING_HARD;
  return HUMAN_RATING_UNSUPPORTED;
}

const char *human_technique_name(HumanTechnique technique) {
  switch (technique) {
    case HUMAN_TECHNIQUE_NAKED_SINGLE: return "naked single";
    case HUMAN_TECHNIQUE_HIDDEN_SINGLE: return "hidden single";
    case HUMAN_TECHNIQUE_LOCKED_CANDIDATE: return "locked candidate";
    case HUMAN_TECHNIQUE_NAKED_PAIR: return "naked pair";
    case HUMAN_TECHNIQUE_NAKED_TRIPLE: return "naked triple";
    case HUMAN_TECHNIQUE_X_WING: return "X-Wing";
    case HUMAN_TECHNIQUE_NONE:
    default:
      return "none";
  }
}

bool human_hint_analyze(const int puzzle[HUMAN_CELL_COUNT],
                        HumanHint *out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  step_reset(&out->reasoning, HUMAN_TECHNIQUE_NONE);
  step_reset(&out->placement, HUMAN_TECHNIQUE_NONE);
  out->contradiction_cell = -1;

  HumanState state;
  if (!state_init(&state, puzzle)) {
    out->status = HUMAN_HINT_INVALID;
    for (int cell = 0; cell < HUMAN_CELL_COUNT; ++cell) {
      if (puzzle[cell] < 0 || puzzle[cell] > 9) {
        out->contradiction_cell = cell;
        break;
      }
      if (puzzle[cell]) {
        for (int other = 0; other < cell; ++other)
          if (puzzle[other] == puzzle[cell] && peer(cell, other)) {
            out->contradiction_cell = cell;
            break;
          }
      } else if ((state.candidates[cell] & HUMAN_FULL_MASK) == 0) {
        out->contradiction_cell = cell;
      }
      if (out->contradiction_cell >= 0) break;
    }
    return true;
  }
  if (state_solved(&state)) {
    out->status = HUMAN_HINT_SOLVED;
    return true;
  }

  for (int iteration = 0; iteration < HUMAN_MAX_STEPS; ++iteration) {
    HumanStep step;
    if (!find_next_step(&state, &step)) {
      out->status = HUMAN_HINT_STALLED;
      return true;
    }
    ++out->reasoning_steps;
    if (out->reasoning_steps == 1) out->reasoning = step;
    if (step.placement_cell >= 0) {
      out->placement = step;
      out->status = HUMAN_HINT_LOGICAL;
      return true;
    }
  }

  out->status = HUMAN_HINT_STALLED;
  return true;
}

bool human_evaluate_with_last_step(const int puzzle[HUMAN_CELL_COUNT],
                                   HumanEvaluation *out,
                                   HumanStep *last_step) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (last_step) step_reset(last_step, HUMAN_TECHNIQUE_NONE);

  HumanState state;
  if (!state_init(&state, puzzle)) return false;
  out->valid = true;

  int elimination_run = 0;
  for (int iteration = 0; iteration < HUMAN_MAX_STEPS; ++iteration) {
    if (state_solved(&state)) {
      out->solved = true;
      break;
    }

    HumanStep step;
    bool changed = find_next_step(&state, &step);
    if (!changed) {
      out->stalled = true;
      break;
    }

    ++out->total_steps;
    if (step.technique > out->max_technique)
      out->max_technique = step.technique;
    if (step.technique >= HUMAN_TECHNIQUE_NONE &&
        step.technique < HUMAN_TECHNIQUE_COUNT)
      ++out->technique_steps[step.technique];

    if (step.placement_cell >= 0) {
      ++out->placements;
      elimination_run = 0;
    } else {
      ++out->elimination_steps;
      ++elimination_run;
      if (elimination_run > out->longest_elimination_run)
        out->longest_elimination_run = elimination_run;
    }
    if (last_step) *last_step = step;
  }

  if (!out->solved && !out->stalled) out->stalled = true;
  out->rating = classify(out);
  return out->valid;
}

bool human_evaluate(const int puzzle[HUMAN_CELL_COUNT], HumanEvaluation *out) {
  return human_evaluate_with_last_step(puzzle, out, NULL);
}
