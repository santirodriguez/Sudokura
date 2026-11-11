# Good puzzles. Good documentation.

Here for a quick download, a puzzle tip or the deeper technical details? You're in the right place. If you're ready to play, start at [Sudokura's main page](../README.md); we'll keep the build instructions out of your way.

[Official website](https://sudokura.cajapersonal.org/) · [Support development](https://cajapersonal.org/donate/)

## Start with the game

- **[What's new in v1.4.1](RELEASE_NOTES_1.4.1.md)** — safer Saved Games and Android updates that retain local progress.
- **[See the real screenshots](images/README.md)** — the familiar desktop board and historical galleries.
- **[Something not working?](REPORTING_ISSUES.md)** — report a bug, protect your saves or recover a game.
- **[Read the full changelog](../CHANGELOG.md)** — the release-by-release history.

## Playing on Android

Sudokura arrived on Android in 1.4.0. Version 1.4.1 keeps the same production application ID and original signing certificate, enabling a direct update that preserves locally saved games; the maintainer verified this over-install on a real phone. Private debug prototypes have a different identity and are not migrated automatically.

- [Android builds and known limitations](../android/README.md)
- [Installing, signing and updating](../android/RELEASING.md)
- [Android roadmap and distribution plans](ANDROID_RELEASE_PLAN.md)
- [v1.4.0 implementation and acceptance record](V1.4.0_BLUEPRINT.md)
- [1.4.1 changes and compatibility](RELEASE_NOTES_1.4.1.md)

**Availability:** [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases) shows what's actually published. Google Play and F-Droid are independent channels.

## Building Sudokura

- [Development guidelines](DEVELOPMENT.md) — code, tests, design constraints and release checks.

## Find your way around the repository

- [`src/`](../src/) — Sudoku engine and SDL app.
- [`android/`](../android/) — Android host, native build and packaging.
- [`assets/`](../assets/) — artwork, language flags, sounds and music.
- [`tests/`](../tests/) — game, saves, input, layout and tooling tests.
- [`scripts/`](../scripts/) — asset generation and validation tools.
- [`packaging/`](../packaging/) — desktop distribution.
- [`.github/workflows/`](../.github/workflows/) — CI and release automation.

## Previous releases

- [v1.4.0](RELEASE_NOTES_1.4.0.md) · [v1.3.9](RELEASE_NOTES_1.3.9.md) · [v1.3.8](RELEASE_NOTES_1.3.8.md) · [v1.3.7](RELEASE_NOTES_1.3.7.md) · [v1.3.6](RELEASE_NOTES_1.3.6.md) · [v1.3.5](RELEASE_NOTES_1.3.5.md)
- [v1.3.0](RELEASE_NOTES_1.3.0.md) · [v1.2.0](RELEASE_NOTES_1.2.0.md) · [v1.1.0](RELEASE_NOTES_1.1.0.md)
- [All published releases](https://github.com/santirodriguez/Sudokura/releases)

## Historical technical records

These documents preserve implementation and compatibility history. They aren't competing current specifications.

- [v1.3.9 blueprint](V1.3.9_BLUEPRINT.md)
- [v1.3.8 blueprint](V1.3.8_BLUEPRINT.md)
- [v1.3.0 implementation](V1.3.0_IMPLEMENTATION.md)
- [v1.2.0 implementation](V1.2.0_IMPLEMENTATION.md)
- [v1.1.0 implementation](V1.1.0_IMPLEMENTATION.md)
