; Chartchotic Inno Setup Installer
;
; Usage: iscc /DVERSION=1.0.0 windows-installer.iss
; Or:    iscc /DVERSION=1.0.0 /DSOURCE_DIR=path\to\Release windows-installer.iss

#ifndef VERSION
  #error "VERSION must be defined. Use: iscc /DVERSION=x.x.x windows-installer.iss"
#endif

; Release artefacts dir (contains VST3\Chartchotic.vst3). Default matches the _plugins
; hub layout: this .iss lives in <plugin>/.ci and the build lands in
; <plugin>/build/Chartchotic_artefacts/Release.
#ifndef SOURCE_DIR
  #define SOURCE_DIR "..\build\Chartchotic_artefacts\Release"
#endif

#define MyAppName "Chartchotic"
#define MyAppPublisher "Dichotic Studios"
#define MyAppURL "https://github.com/noahbaxter/chartchotic"

[Setup]
AppId={{E8A3B2C1-4D5F-6789-ABCD-EF0123456789}
AppName={#MyAppName}
AppVersion={#VERSION}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={commoncf64}\VST3
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=Chartchotic-{#VERSION}-Windows-x64
OutputDir=.
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DisableDirPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[InstallDelete]
Type: filesandordirs; Name: "{app}\Chartchotic.vst3"

[Files]
Source: "{#SOURCE_DIR}\VST3\Chartchotic.vst3\*"; DestDir: "{app}\Chartchotic.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
SetupWindowTitle=Install {#MyAppName} v{#VERSION}
WelcomeLabel2=This will install {#MyAppName} v{#VERSION} VST3 plugin.%n%nThe plugin installs to:%n  C:\Program Files\Common Files\VST3%n%nClose your DAW before continuing.
