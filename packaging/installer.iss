; Inno Setup 6 script for the SwiftPlace installer. Run through package.bat:
;   iscc /DBuildDir=<cmake build dir> packaging\installer.iss
; BuildDir holds installer-config.iss (version, paths) and stage\ (cmake --install output).
; Saved as UTF-8 with BOM so Inno Setup reads the Chinese names correctly.

#ifndef BuildDir
  #error Pass /DBuildDir=<cmake build directory>
#endif
#include BuildDir + "\installer-config.iss"

#define AppName "迅贴 SwiftPlace"

[Setup]
; AppId identifies the installed product for upgrades and uninstall. Never change it.
AppId={{EDC3D93B-F6B5-4315-96A3-06D4F78F8691}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=SwiftPlace
DefaultDirName={autopf}\SwiftPlace
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\LICENSE
OutputDir={#SourceDir}\dist
OutputBaseFilename=SwiftPlace-{#AppVersion}-setup
SetupIconFile={#SourceDir}\packaging\icon\swiftplace.ico
UninstallDisplayIcon={app}\{#AppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequiredOverridesAllowed=dialog
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Languages]
; Inno Setup 6 ships Chinese only as an unofficial translation, so a copy lives
; here (from jrsoftware/issrc tag is-6_7_4, Files/Languages/Unofficial).
Name: "chs"; MessagesFile: "{#SourceDir}\packaging\ChineseSimplified.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
