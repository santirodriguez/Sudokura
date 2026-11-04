# Sudokura documentation

The [main README](../README.md) introduces the game and links to the latest published downloads. This index keeps technical documentation and older release history available without crowding the front page.

[Official website](https://sudokura.cajapersonal.org/) · [Support development](https://cajapersonal.org/donate/)

## Start here

- [v1.4.0 release notes](RELEASE_NOTES_1.4.0.md)
- [Changelog](../CHANGELOG.md)
- [Screenshot gallery](images/README.md)
- [Issue reporting, saved games, and recovery](REPORTING_ISSUES.md)
- [Development and packaging](DEVELOPMENT.md)

## Android development

- [Android release roadmap and distribution plans](ANDROID_RELEASE_PLAN.md)
- [Android builds and runtime limitations](../android/README.md)
- [v1.4.0 implementation blueprint and acceptance decisions](V1.4.0_BLUEPRINT.md)
- [Android packaging, signing and publication](../android/RELEASING.md)

Version 1.4.0 prepares the first signed Android release. Private prototypes remain separate installations. See [Android release preparation](../android/RELEASING.md) for the direct APK, signing and store-channel details.

## Repository layout

- [`src/`](../src/) — shared Sudoku engine and SDL application code.
- [`android/`](../android/) — Android host, native build, packaging and dependency audits.
- [`assets/`](../assets/) — artwork, flags, music and generated UI resources.
- [`tests/`](../tests/) — gameplay, persistence, input, layout and tooling tests.
- [`scripts/`](../scripts/) — asset generation, validation and release helpers.
- [`packaging/`](../packaging/) — platform-specific distribution and packaging support.
- [`.github/workflows/`](../.github/workflows/) — continuous integration, package previews and release automation.

The existing paths are intentional: avoid moving source, assets or historical releases solely to shorten the directory tree.

## Previous releases

- [v1.3.9](RELEASE_NOTES_1.3.9.md) · [v1.3.8](RELEASE_NOTES_1.3.8.md) · [v1.3.7](RELEASE_NOTES_1.3.7.md) · [v1.3.6](RELEASE_NOTES_1.3.6.md) · [v1.3.5](RELEASE_NOTES_1.3.5.md)
- [v1.3.0](RELEASE_NOTES_1.3.0.md) · [v1.2.0](RELEASE_NOTES_1.2.0.md) · [v1.1.0](RELEASE_NOTES_1.1.0.md)
- [All published GitHub releases](https://github.com/santirodriguez/Sudokura/releases)

## Historical technical records

These documents are retained for implementation and compatibility history; they are not a second current specification.

- [v1.3.9 blueprint](V1.3.9_BLUEPRINT.md)
- [v1.3.8 blueprint](V1.3.8_BLUEPRINT.md)
- [v1.3.0 implementation](V1.3.0_IMPLEMENTATION.md)
- [v1.2.0 implementation](V1.2.0_IMPLEMENTATION.md)
- [v1.1.0 implementation](V1.1.0_IMPLEMENTATION.md)
