; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "デカ文字PDF"
#define MyAppVersion "1.2.0"
#define MyAppPublisher "片山博文MZ"
#define MyAppURL "https://katahiromz.web.fc2.com/"
#define MyAppExeName "DekaMoji.exe"
#define MyAppCopyright "(c) katahiromz"
#define MyAppDescription "大きな文字でPDFを生成"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{28720DA4-8036-4104-B79C-62A8C5F9997D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\DekaMojiPDF
DefaultGroupName={#MyAppName}
LicenseFile=LICENSE.txt
OutputDir=.
OutputBaseFilename=DekaMoji-{#MyAppVersion}-setup
SetupIconFile=res\1041_Icon_1.ico
SolidCompression=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright={#MyAppCopyright}
VersionInfoDescription={#MyAppDescription}
VersionInfoProductName={#MyAppName}
VersionInfoProductTextVersion={#MyAppVersion}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoVersion={#MyAppVersion}

[Languages]
;Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "HISTORY.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "pdf2bmp\pdfium.dll"; DestDir: "{app}\pdf2bmp"; Flags: ignoreversion
Source: "pdf2bmp\pdf2bmp.exe"; DestDir: "{app}\pdf2bmp"; Flags: ignoreversion
Source: "pdfplaca\iconv.dll"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion
Source: "pdfplaca\libcairo-2.dll"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion
Source: "pdfplaca\libfontconfig-1.dll"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion
Source: "pdfplaca\libfreetype-6.dll"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion
Source: "pdfplaca\libpng12.dll"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion
Source: "pdfplaca\libxml2.dll"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion
Source: "pdfplaca\pdfplaca.exe"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion
Source: "pdfplaca\zlib1.dll"; DestDir: "{app}\pdfplaca"; Flags: ignoreversion

; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\README.txt"; Filename: "{app}\README.txt"
Name: "{group}\LICENSE.txt"; Filename: "{app}\LICENSE.txt"
Name: "{group}\HISTORY.txt"; Filename: "{app}\HISTORY.txt"
Name: "{group}\アンインストール"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\Katayama Hirofumi MZ\DekaMojiPDF"; Flags: uninsdeletekey
