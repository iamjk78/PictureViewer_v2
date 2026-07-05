; Inno Setup skript pro PictureViewer.
; Kompilace v CI:  iscc /DAppVersion=0.16 /DSourceDir=..\dist\PictureViewer setup.iss
;
; Uživatelská data (config.ini, profily, cache miniatur) žijí v AppData —
; instalátor ani odinstalátor se jich nedotýká, aktualizace je tedy zachová.

#ifndef AppVersion
  #define AppVersion "0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\PictureViewer"
#endif

[Setup]
AppId={{8F4B1E5A-9C2D-4E7B-A1F3-6D8E0B2C5A91}
AppName=PictureViewer
AppVersion={#AppVersion}
AppPublisher=Jiri Krejci
AppPublisherURL=https://github.com/iamjk78/PictureViewer_v2
DefaultDirName={autopf}\PictureViewer
DefaultGroupName=PictureViewer
UninstallDisplayIcon={app}\PictureViewer.exe
OutputBaseFilename=PictureViewer-Setup
OutputDir=output
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
; Výchozí instalace bez oprávnění správce (do profilu uživatele) — díky tomu
; proběhne i tichá aktualizace (/VERYSILENT) bez UAC dialogu. Uživatel může
; v dialogu zvolit instalaci pro všechny uživatele (vyžaduje správce).
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; Zavřít běžící aplikaci před instalací (relevantní pro ruční spuštění).
CloseApplications=yes

[Languages]
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\PictureViewer"; Filename: "{app}\PictureViewer.exe"
Name: "{autodesktop}\PictureViewer"; Filename: "{app}\PictureViewer.exe"; \
    Tasks: desktopicon

[Run]
; Po ruční instalaci nabídnout spuštění; při tiché aktualizaci se přeskočí.
Filename: "{app}\PictureViewer.exe"; \
    Description: "{cm:LaunchProgram,PictureViewer}"; \
    Flags: nowait postinstall skipifsilent
