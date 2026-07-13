[Setup]
AppId={{480062A0-2F26-47C3-A851-D9C5CB0BBD7E}
AppName=Danger Guitar Amps
AppVerName=Danger Guitar Amps 1.0.0 RC2
AppVersion=1.0.0 RC2
AppPublisher=Danger Audio
AppPublisherURL=https://github.com/DangerGuitarAmps/DangerGuitarAmps
AppSupportURL=https://github.com/DangerGuitarAmps/DangerGuitarAmps/issues
AppUpdatesURL=https://github.com/DangerGuitarAmps/DangerGuitarAmps/releases
AppCopyright=Copyright (C) 2026 Danger Audio
DefaultDirName={autopf}\Danger Audio\Danger Guitar Amps
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
OutputDir=..\..\release-artifacts
OutputBaseFilename=DangerGuitarAmps-Windows-x64-Installer-v1.0-rc2
VersionInfoVersion=1.0.0.2
VersionInfoCompany=Danger Audio
VersionInfoDescription=Danger Guitar Amps VST3 installer - built on the Neural Amp Modeler open-source project
VersionInfoProductName=Danger Guitar Amps
VersionInfoProductVersion=1.0.0.2
LicenseFile=..\..\LICENSE
SetupLogging=yes
Uninstallable=yes
UninstallDisplayName=Danger Guitar Amps
UninstallDisplayIcon={commoncf64}\VST3\DangerGuitarAmps.vst3\PlugIn.ico
CloseApplications=no
RestartApplications=no

[Files]
; Package the complete validated VST3 bundle, including its hidden metadata,
; icon, architecture directory and resources. The source bundle is validated
; before compiling this installer and must not contain build/debug artifacts.
Source: "..\build-win\DangerGuitarAmps.vst3\*"; Excludes: "desktop.ini,PlugIn.ico"; DestDir: "{commoncf64}\VST3\DangerGuitarAmps.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build-win\DangerGuitarAmps.vst3\desktop.ini"; DestDir: "{commoncf64}\VST3\DangerGuitarAmps.vst3"; Flags: ignoreversion; Attribs: hidden system
Source: "..\build-win\DangerGuitarAmps.vst3\PlugIn.ico"; DestDir: "{commoncf64}\VST3\DangerGuitarAmps.vst3"; Flags: ignoreversion

; Keep the applicable licence and third-party notices with both the uninstaller
; support files and the installed plug-in bundle.
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "ThirdPartyNotices.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{commoncf64}\VST3\DangerGuitarAmps.vst3\Contents\Resources"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "ThirdPartyNotices.txt"; DestDir: "{commoncf64}\VST3\DangerGuitarAmps.vst3\Contents\Resources"; Flags: ignoreversion

[UninstallDelete]
; This identity and path are Danger-specific. Never remove the official NAM
; bundle or user-created model/IR files stored elsewhere.
Type: filesandordirs; Name: "{commoncf64}\VST3\DangerGuitarAmps.vst3"
