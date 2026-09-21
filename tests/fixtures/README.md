# Human difficulty calibration fixture

`human_difficulty_20240415.csv` is an exact copy of upstream blob
`61518b820af3a488d5359f790d3c6b2455313acd` from
`synnwang/sudoku_dataset_difficulty`.

The upstream project describes the data as Sudoku puzzles with difficulty
metrics experienced by human players:

- `D_TO`: playing-time based metric.
- `D_TR`: playing time plus successful-completion ratio.

Source publication: Sheng-Wei Wang, "A Dataset of Sudoku Puzzles With
Difficulty Metrics Experienced by Human Players," IEEE Access 12 (2024),
104254-104262, DOI 10.1109/ACCESS.2024.3434632.

The upstream repository applies **CC0 1.0 Universal** to the dataset. This
fixture is kept unchanged so calibration results are reproducible. It is
external calibration evidence, not a Sudokura-generated corpus and not part of
the generator identity contract.
