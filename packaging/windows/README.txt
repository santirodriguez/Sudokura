Sudokura - Windows package notes
=========================================

Public v1.3.5 filenames
-----------------------
  Sudokura-1.3.5-Windows-x64-Setup.exe
  Sudokura-1.3.5-Windows-x64-Portable.exe

The release also publishes SHA256SUMS.txt and
Sudokura-1.3.5-Build-Info.json. The ZIP produced by CI is internal staging
evidence and is not a public portable download.

Portable EXE and installer
--------------------------
The portable download is one self-extracting EXE and does not install Sudokura,
request administrator privileges, register an uninstaller, create shortcuts, or
download runtime files. Each launch extracts the same audited application
payload used by the installer into a private temporary directory, starts the
contained game directly, waits for it to finish, returns its exit status, and
removes that launcher's temporary files on normal exit.

A forced operating-system/process termination can interrupt cleanup and leave
that launch's temporary files behind. This is a normal limitation of
self-extracting packages; Sudokura never deletes unrelated temporary content.

The installer is a per-user installation and does not require administrator
privileges. The portable EXE and installer use the same Sudokura executable,
runtime DLL closure, font, audio resources, notices, and data-storage policy.

User data and upgrades
----------------------
Sudokura stores saves and settings through SDL_GetPrefPath for organization
"santirodriguez" and application "Sudokura". On normal Windows profiles this
is under:

  %APPDATA%\santirodriguez\Sudokura\

Switching between the portable EXE and an installed copy does not move or delete
that profile. Uninstalling the installer removes application files and
shortcuts but deliberately preserves the profile by default. To remove all
Sudokura data after uninstalling, delete that profile directory manually.

Windows reputation warnings
---------------------------
Sudokura is not commercially code-signed. Windows may therefore show a
reputation/SmartScreen warning. Do not disable Windows protections. Verify the
published SHA-256 checksum and decide whether to run the package.
