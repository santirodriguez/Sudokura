# Documentation images

Repository screenshots used by release notes and documentation belong in this directory.

## Historical interface — v1.1.0

<p align="center"><img src="sudokura-v1.1.0.png" alt="Sudokura v1.1.0 game interface" width="900"></p>

- File: `sudokura-v1.1.0.png`
- Dimensions: 1112×944
- Status: historical v1.1.0 interface; it is not the canonical v1.2 screenshot.

## Historical interface — v1

<p align="center"><img src="sudokura-v1.png" alt="Legacy Sudokura v1 game interface" width="900"></p>

- File: `sudokura-v1.png`
- Dimensions: 1713×1383
- Status: retained as the visual record of the original v1 interface.

The legacy file is byte-identical to the former root-level `screenshot.png`. The duplicate root file was removed after this archival copy was confirmed.

## v1.2 screenshot gate

No canonical v1.2 screenshot is versioned in the release-candidate stage. The final public screenshot must be supplied after a real v1.2 package has been installed and manually tested. Automated `--render-screenshots` output is diagnostic CI material only and must not be committed or presented as the official release image.

When the approved screenshot is supplied, use the stable canonical name `sudokura-v1.2.0.png` unless the final documentation explicitly requires more than one view. README and release-note references are added only after the source image exists in this directory.

## Naming policy

Use `sudokura-v<version>.png` for canonical release screenshots. Images should show only the application at a readable size, without unrelated windows or personal information. PNG is preferred so text and board lines remain sharp.
