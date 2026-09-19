#define SudokuraVersion GetEnv("SUDOKURA_VERSION")
#define RepoRoot GetEnv("SUDOKURA_ROOT_WIN")
#define DistDir AddBackslash(RepoRoot) + "dist"

[Setup]
AppId={{B06FEA8D-5F9A-4A8F-92BB-5A89B5E5E301}
AppName=Sudokura
AppVersion={#SudokuraVersion}
AppVerName=Sudokura v{#SudokuraVersion}
AppPublisher=Santiago Rodriguez
AppPublisherURL=https://santiagorodriguez.com/
DefaultDirName={localappdata}\Programs\Sudokura
DefaultGroupName=Sudokura
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#RepoRoot}
OutputBaseFilename=Sudokura-v{#SudokuraVersion}-windows-x86_64-setup
SetupIconFile={#RepoRoot}\assets\generated\sudokura.ico
UninstallDisplayIcon={app}\sudokura.exe
LicenseFile={#RepoRoot}\LICENSE
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
VersionInfoVersion={#SudokuraVersion}.0
VersionInfoProductVersion={#SudokuraVersion}
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Sudokura"; Filename: "{app}\sudokura.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Sudokura"; Filename: "{app}\sudokura.exe"; WorkingDir: "{app}"; Tasks: desktopicon
