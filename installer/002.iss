; 002 by SPKR — Windows installer (Inno Setup 6)
; Build:  installer\build_installer.cmd
; Output: dist\002-by-SPKR-<version>-Windows-x64-Setup.exe

#define AppName        "002"
#define AppFullName    "002 by SPKR"
#define AppVersion     "0.4.6"
#define Publisher      "SPKR"
#define AppURL         "https://spkr.shop"
#define VstName        "002 by SPKR.vst3"
#define ClapName       "002 by SPKR.clap"
#define StandaloneName "002 by SPKR.exe"
#define BuildDir       "..\build\Geminus_artefacts\Release"

[Setup]
; A stable AppId means upgrades replace the old install instead of stacking up.
AppId={{CAEF571A-10D5-4FC4-BD0E-2A1D47F7C5E0}
AppName={#AppFullName}
AppVersion={#AppVersion}
AppVerName={#AppFullName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany={#Publisher}
VersionInfoProductName={#AppFullName}
VersionInfoDescription={#AppFullName} installer
VersionInfoCopyright=© {#Publisher}

DefaultDirName={autopf}\{#Publisher}\{#AppFullName}
DefaultGroupName={#Publisher}
DisableProgramGroupPage=yes
DisableDirPage=no
LicenseFile=LICENSE.txt
InfoAfterFile=README.txt
OutputDir=..\dist
OutputBaseFilename=002-by-SPKR-{#AppVersion}-Windows-x64-Setup
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\{#StandaloneName}
UninstallDisplayName={#AppFullName}

WizardStyle=modern
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard-small.bmp
WizardImageStretch=no
ShowLanguageDialog=no
SolidCompression=yes
Compression=lzma2/max

; 64-bit only, and the VST3 folder lives under Common Files, so this needs admin.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UsePreviousAppDir=yes
UsePreviousTasks=yes
CloseApplications=yes
RestartApplications=no
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Everything (recommended)"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plug-in (for your DAW)"; Types: full custom; Flags: checkablealone
Name: "clap";       Description: "CLAP plug-in (Bitwig, Reaper, FL Studio 2024+)"; Types: full custom
Name: "standalone"; Description: "Standalone app (play without a DAW)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; GroupDescription: "Shortcuts:"; Components: standalone; Flags: unchecked

[Files]
; The VST3 is a bundle (a folder), so it is installed recursively.
Source: "{#BuildDir}\VST3\{#VstName}\*"; DestDir: "{commoncf64}\VST3\002 By SPKR\{#VstName}"; \
    Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs uninsremovereadonly
Source: "{#BuildDir}\CLAP\{#ClapName}"; DestDir: "{commoncf64}\CLAP"; \
    Components: clap; Flags: ignoreversion
Source: "{#BuildDir}\Standalone\{#StandaloneName}"; DestDir: "{app}"; \
    Components: standalone; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppFullName}"; Filename: "{app}\{#StandaloneName}"; Components: standalone
Name: "{group}\Uninstall {#AppFullName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppFullName}"; Filename: "{app}\{#StandaloneName}"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\{#StandaloneName}"; Description: "Open the standalone app"; \
    Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\002 By SPKR"

[Code]
// The WebView2 runtime ships with Windows 11 and current Windows 10, but an old machine can
// be missing it — without it the plug-in window would come up blank, so say so up front.
function WebView2Present(): Boolean;
var
  S: String;
begin
  Result :=
    RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', S) or
    RegQueryStringValue(HKLM, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', S) or
    RegQueryStringValue(HKCU, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', S);
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not WebView2Present() then
    if MsgBox('002''s window needs the Microsoft Edge WebView2 runtime, which does not seem to be installed.' + #13#10#13#10 +
              'You can install 002 now and add WebView2 afterwards from:' + #13#10 +
              'https://developer.microsoft.com/microsoft-edge/webview2/' + #13#10#13#10 +
              'Carry on with the installation?', mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
end;
