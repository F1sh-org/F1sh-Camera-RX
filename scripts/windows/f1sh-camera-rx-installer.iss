#ifndef AppVersion
#define AppVersion "0.0.0"
#endif

#ifndef SourceDir
#define SourceDir "..\\..\\dist\\F1sh-Camera-RX"
#endif

#ifndef OutputDir
#define OutputDir "..\\..\\dist\\installer"
#endif

[Setup]
AppId={{E1F9B226-83F3-474F-A52E-DF4F3583D73A}}
AppName=F1sh Camera RX
AppVersion={#AppVersion}
AppPublisher=F1sh Camera Project
AppPublisherURL=https://github.com/F1sh-org/F1sh-Camera-RX
DefaultDirName={autopf}\F1sh Camera RX
DefaultGroupName=F1sh Camera RX
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=F1sh-Camera-RX-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\bin\f1sh-camera-rx.exe
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\F1sh Camera RX"; Filename: "{app}\run-portable.cmd"; WorkingDir: "{app}"
Name: "{commondesktop}\F1sh Camera RX"; Filename: "{app}\run-portable.cmd"; Tasks: desktopicon; WorkingDir: "{app}"

[Run]
Filename: "{app}\run-portable.cmd"; Description: "Launch F1sh Camera RX"; Flags: nowait postinstall skipifsilent
