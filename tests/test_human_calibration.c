#include "human.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALIBRATION_MAX_RECORDS 512

typedef struct {
  double dto[CALIBRATION_MAX_RECORDS];
  double dtr[CALIBRATION_MAX_RECORDS];
  int count;
} RatingMetrics;

static int compare_double(const void *left, const void *right) {
  double a = *(const double *)left, b = *(const double *)right;
  return a < b ? -1 : a > b ? 1 : 0;
}

static void parse_puzzle(const char *text, int board[81]) {
  assert(text && strlen(text) == 81);
  for (int i = 0; i < 81; ++i) {
    char ch = text[i];
    assert(ch == '.' || (ch >= '1' && ch <= '9'));
    board[i] = ch == '.' ? 0 : ch - '0';
  }
}

static const char *rating_name(int rating) {
  switch ((HumanRating)rating) {
    case HUMAN_RATING_EASY: return "easy";
    case HUMAN_RATING_MEDIUM: return "medium";
    case HUMAN_RATING_HARD: return "hard";
    case HUMAN_RATING_UNSUPPORTED:
    default:
      return "unsupported";
  }
}

static double median(double *values, int count) {
  assert(values && count > 0);
  qsort(values, (size_t)count, sizeof(values[0]), compare_double);
  if (count % 2) return values[count / 2];
  return (values[count / 2 - 1] + values[count / 2]) / 2.0;
}

int main(void) {
  FILE *file = fopen("tests/fixtures/human_difficulty_20240415.csv", "rb");
  assert(file);

  char line[512];
  assert(fgets(line, sizeof(line), file));
  RatingMetrics metrics[HUMAN_RATING_HARD + 1];
  memset(metrics, 0, sizeof(metrics));
  int rows = 0, unsupported = 0;

  while (fgets(line, sizeof(line), file)) {
    int game_no = 0;
    char puzzle[82] = {0};
    double dto = 0.0, dtr = 0.0;
    int parsed = sscanf(line, "%d,%81[^,],%lf,%lf", &game_no, puzzle,
                        &dto, &dtr);
    assert(parsed == 4);
    (void)game_no;

    int board[81];
    parse_puzzle(puzzle, board);
    HumanEvaluation evaluation;
    assert(human_evaluate(board, &evaluation));
    assert(evaluation.valid);
    ++rows;
    if (!evaluation.solved ||
        evaluation.rating == HUMAN_RATING_UNSUPPORTED) {
      ++unsupported;
      continue;
    }

    assert(evaluation.rating >= HUMAN_RATING_EASY &&
           evaluation.rating <= HUMAN_RATING_HARD);
    RatingMetrics *group = &metrics[evaluation.rating];
    assert(group->count < CALIBRATION_MAX_RECORDS);
    group->dto[group->count] = dto;
    group->dtr[group->count] = dtr;
    ++group->count;
  }
  assert(fclose(file) == 0);
  assert(rows >= 100);

  printf("human-calibration rows=%d unsupported=%d\n", rows, unsupported);
  for (int rating = HUMAN_RATING_EASY; rating <= HUMAN_RATING_HARD;
       ++rating) {
    RatingMetrics *group = &metrics[rating];
    if (group->count == 0) {
      printf("human-calibration rating=%s count=0\n", rating_name(rating));
      continue;
    }
    double dto_median = median(group->dto, group->count);
    double dtr_median = median(group->dtr, group->count);
    printf("human-calibration rating=%s count=%d dto_median=%.4f"
           " dtr_median=%.4f\n",
           rating_name(rating), group->count, dto_median, dtr_median);
  }

  puts("human-player calibration dataset evaluated");
  return 0;
}
