; BackReverse Windows installer (Inno Setup 6). Built by CI: ISCC /DBuildDir=<artefacts\Release> /DOutDir=<dist> BackReverse.iss
#ifndef BuildDir
  #define BuildDir "..\build\src\plugin\BackReverse_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\dist"
#endif

[Setup]
AppId={{6E3B8C0B-6D0F-4C55-9E1B-5B7A0D2C1F42}
AppName=BackReverse
AppVersion=0.0.2
AppVerName=BackReverse 0.0.2 beta
AppPublisher=Circuit Drift Labs
AppPublisherURL=https://github.com/djshellshoxxx/backreverse
DefaultDirName={autopf}\Circuit Drift Labs\BackReverse
DefaultGroupName=BackReverse
OutputDir={#OutDir}
OutputBaseFilename=BackReverse-v0.0.2-beta-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
UninstallDisplayIcon={app}\BackReverse.exe

[Types]
Name: "full"; Description: "Standalone + VST3 + CLAP"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full custom
Name: "vst3"; Description: "VST3 plug-in (Common Files\VST3)"; Types: full custom
Name: "clap"; Description: "CLAP plug-in (Common Files\CLAP)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone; Flags: unchecked

[Files]
Source: "{#BuildDir}\Standalone\BackReverse.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#BuildDir}\VST3\BackReverse.vst3\*"; DestDir: "{commoncf64}\VST3\BackReverse.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\CLAP\BackReverse.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\COPYRIGHT-TRADEMARK.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\BackReverse"; Filename: "{app}\BackReverse.exe"; Components: standalone
Name: "{group}\Uninstall BackReverse"; Filename: "{uninstallexe}"
Name: "{autodesktop}\BackReverse"; Filename: "{app}\BackReverse.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\BackReverse.exe"; Description: "Launch BackReverse"; Flags: nowait postinstall skipifsilent; Components: standalone
