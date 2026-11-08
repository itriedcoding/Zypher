; Zypher Windows installer (Inno Setup 6.3+).
;
; Build with:
;   ISCC.exe /DMyVersion=0.1.2 /DStageDir=C:\path\to\stage /DOutputDir=C:\path\to\dist zypher.iss
;
; The staged tree (bin/zypher_editor.exe with its deployed Qt runtime,
; bin/zypher_runtime.exe, lib/*.lib, include/, share/zypher/) is installed
; verbatim. Zypher.cmd at its root also opens the editor.

#ifndef MyVersion
  #define MyVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "stage"
#endif
#ifndef OutputDir
  #define OutputDir "dist"
#endif

[Setup]
AppId={{7C4E1D2A-9B3F-4C58-8A6E-2D1F5B7C9E0A}
AppName=Zypher
AppVersion={#MyVersion}
AppVerName=Zypher {#MyVersion}
VersionInfoVersion={#MyVersion}
AppPublisher=Zypher
AppPublisherURL=https://github.com/itriedcoding/Zypher
AppSupportURL=https://github.com/itriedcoding/Zypher/issues
AppUpdatesURL=https://github.com/itriedcoding/Zypher/releases
DefaultDirName={autopf}\Zypher
DefaultGroupName=Zypher
DisableProgramGroupPage=yes
DisableDirPage=auto
LicenseFile=..\..\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=zypher-{#MyVersion}-windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
UninstallDisplayName=Zypher {#MyVersion}
UninstallDisplayIcon={app}\bin\zypher_editor.exe
; Install for the current user by default (no UAC prompt); the wizard offers an
; all-users install for administrators.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Zypher Editor"; Filename: "{app}\bin\zypher_editor.exe"; WorkingDir: "{app}"; Comment: "Open the Zypher editor"
Name: "{group}\Zypher console"; Filename: "{app}\bin\zypher_runtime.exe"; Parameters: "--help"; WorkingDir: "{app}"; Comment: "Runtime command line options"
Name: "{group}\{cm:UninstallProgram,Zypher}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Zypher Editor"; Filename: "{app}\bin\zypher_editor.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\zypher_editor.exe"; Description: "Launch Zypher Editor"; Flags: postinstall nowait skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
