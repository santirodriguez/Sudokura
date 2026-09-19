Sudokura v1.3.0 - Windows package notes
=========================================

ZIP and installer
-----------------
The ZIP is portable. The installer is a per-user installation and does not
require administrator privileges. Both contain the same Sudokura executable,
runtime DLL closure, font, audio resources, notices and data-storage policy.

User data and upgrades
----------------------
Sudokura stores saves and settings through SDL_GetPrefPath for organization
"santirodriguez" and application "Sudokura". On normal Windows profiles this
is under:

  %APPDATA%\santirodriguez\Sudokura\

Updating either the ZIP contents or an installed copy does not move or delete
that profile. Uninstalling the installer removes application files and
shortcuts but deliberately preserves the profile by default. To remove all
Sudokura data after uninstalling, delete that profile directory manually.

Windows reputation warnings
---------------------------
The v1.3.0 candidate is not commercially code-signed. Windows may therefore
show a reputation/SmartScreen warning. Do not disable Windows protections.
Verify the published SHA-256 checksum and decide whether to run the package.
