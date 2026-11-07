; OnePad installer (Inno Setup 6.x)
; Build:  ISCC.exe installer\OnePad-Setup.iss                 -> dist\OnePad-Setup-<ver>.exe  (public build: downloads 3rd-party parts)
;         ISCC.exe /DPERSONAL=D:\OneDrive\AI\ClaudeCode\OnePad\personal installer\OnePad-Setup.iss
;                                                             -> also restores this PC's Azahar config/saves (never publish that build)
#define MyAppName "OnePad"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "nautymac"
#define MyAppURL "https://github.com/nautymac/OnePad"
#define Root ".."
#define HidHideUrl "https://github.com/nefarius/HidHide/releases/download/v1.5.230.0/HidHide_1.5.230_x64.exe"
#define UsbipUrl   "https://github.com/vadimgrn/usbip-win2/releases/download/v.0.9.8.1/USBip-0.9.8.1-x64.exe"
#define AhkUrl     "https://github.com/AutoHotkey/AutoHotkey/releases/download/v2.0.26/AutoHotkey_2.0.26_setup.exe"
#define AzaharUrl  "https://github.com/azahar-emu/azahar/releases/download/2126.1/azahar-windows-msys2-2126.1.zip"

[Setup]
AppId={{7C3E2B1A-5D4F-4C8B-9E6A-3D0E1A2B4C6D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion} (3D One -> Steam Deck controller)
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\OnePad
DefaultGroupName=OnePad
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Root}\dist
#ifdef PERSONAL
OutputBaseFilename=OnePad-Setup-{#MyAppVersion}-personal
#else
OutputBaseFilename=OnePad-Setup-{#MyAppVersion}
#endif
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#Root}\src\onepad.ico
UninstallDisplayIcon={app}\bin\onepad.exe
CloseApplications=no
AlwaysRestart=no

[Languages]
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
ko.CompOnePad=OnePad 본체 (가상 Steam Deck 컨트롤러 + 터치패드/백버튼 변환, 로그온 자동 시작)
ko.CompHidHide=HidHide 드라이버 (실제 Xbox 패드를 Steam/게임에서 숨김 — 필수, 재부팅 필요)
ko.CompUsbip=usbip-win2 드라이버 (가상 Steam Deck 장치 전송 — 필수)
ko.CompAzahar=Azahar 2126.1 (3DS 에뮬레이터) + 교착 우회 실행기 + 바로가기
ko.CompSteamShortcut=Azahar를 Steam 라이브러리에 비Steam 게임으로 등록 (Steam이 잠시 종료됨)
ko.CompAhk=AutoHotkey v2 + 3D One 단축키 (geo-11 등 3D 픽스 단축키와 겹쳐 AstraDepth3D가 꺼지는 Ctrl+F4 차단, 오른쪽 Alt = 입력 언어 전환)
ko.CompAstra=AstraDepth3D 자동 시작 작업 (AstraDepth3D가 설치된 경우)
ko.CompPersonal=이 PC의 Azahar 설정·세이브 복원 (개인용 빌드 전용)
ko.AzaharDirTitle=Azahar 설치 폴더
ko.AzaharDirDesc=Azahar 3DS 에뮬레이터를 어디에 설치할까요?
ko.AzaharDirSub=다음 폴더에 Azahar를 설치합니다. 기존 설치가 있으면 그 폴더를 고르세요.
ko.Downloading=필요한 구성 요소를 내려받는 중...
ko.FinishNote=재부팅 후 Steam → 설정 → 컨트롤러에 'Steam Deck 컨트롤러'만 보이면 정상입니다. Azahar는 Steam 라이브러리에서 실행하세요.
en.CompOnePad=OnePad core (virtual Steam Deck controller + trackpad/grip translation, starts at logon)
en.CompHidHide=HidHide driver (hides the real Xbox pad from Steam/games — required, needs reboot)
en.CompUsbip=usbip-win2 driver (transport for the virtual Steam Deck — required)
en.CompAzahar=Azahar 2126.1 (3DS emulator) + deadlock-free launcher + shortcuts
en.CompSteamShortcut=Add Azahar to the Steam library as a non-Steam game (Steam is closed briefly)
en.CompAhk=AutoHotkey v2 + 3D One hotkeys (block Ctrl+F4 that kills AstraDepth3D when 3D-fix tools like geo-11 share the hotkey; Right Alt = input language toggle)
en.CompAstra=AstraDepth3D autostart task (only if AstraDepth3D is installed)
en.CompPersonal=Restore this PC's Azahar config and saves (personal build only)
en.AzaharDirTitle=Azahar folder
en.AzaharDirDesc=Where should Azahar be installed?
en.AzaharDirSub=Azahar will be installed into this folder. Pick your existing folder if you already have it.
en.Downloading=Downloading required components...
en.FinishNote=After the reboot only 'Steam Deck controller' should appear in Steam → Settings → Controller. Launch Azahar from the Steam library.

[Types]
Name: "full"; Description: "Full"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "onepad"; Description: "{cm:CompOnePad}"; Types: full custom; Flags: fixed
Name: "hidhide"; Description: "{cm:CompHidHide}"; Types: full custom; Flags: fixed
Name: "usbip"; Description: "{cm:CompUsbip}"; Types: full custom; Flags: fixed
Name: "azahar"; Description: "{cm:CompAzahar}"; Types: full
Name: "azahar\steam"; Description: "{cm:CompSteamShortcut}"; Types: full
Name: "ahk"; Description: "{cm:CompAhk}"; Types: full
Name: "astra"; Description: "{cm:CompAstra}"; Types: full; Check: AstraInstalled
#ifdef PERSONAL
Name: "personal"; Description: "{cm:CompPersonal}"; Types: full
#endif

[Files]
Source: "{#Root}\bin\onepad.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#Root}\bin\probe.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#Root}\bin\azahar-launch.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#Root}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs
Source: "{#Root}\tools\Add-SteamShortcut.ps1"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "{#Root}\tools\Register-LogonTask.ps1"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "{#Root}\config\onepad.ini"; DestDir: "{localappdata}\OnePad"; Flags: onlyifdoesntexist uninsneveruninstall
Source: "{#Root}\ahk\3D-One-Hotkeys.ahk"; DestDir: "{localappdata}\3D-One-Hotkeys"; Components: ahk; Flags: ignoreversion uninsneveruninstall
#ifdef PERSONAL
Source: "{#PERSONAL}\azahar-config\*"; DestDir: "{userappdata}\Azahar\config"; Components: personal; Flags: recursesubdirs onlyifdoesntexist uninsneveruninstall
Source: "{#PERSONAL}\azahar-userdata\*"; DestDir: "{userappdata}\Azahar"; Components: personal; Flags: recursesubdirs onlyifdoesntexist uninsneveruninstall
#endif

[Icons]
Name: "{autoprograms}\Azahar"; Filename: "{code:AzaharDir}\azahar-launch.exe"; WorkingDir: "{code:AzaharDir}"; IconFilename: "{code:AzaharDir}\azahar.exe"; Components: azahar
Name: "{autodesktop}\Azahar"; Filename: "{code:AzaharDir}\azahar-launch.exe"; WorkingDir: "{code:AzaharDir}"; IconFilename: "{code:AzaharDir}\azahar.exe"; Components: azahar
Name: "{group}\OnePad"; Filename: "{app}\bin\onepad.exe"
Name: "{group}\OnePad 문서 (docs)"; Filename: "{app}\docs"

[Run]
; --- drivers (downloaded to {tmp} in NextButtonClick) ---
Filename: "{tmp}\HidHide.exe"; Parameters: "/exenoui /qn /norestart"; StatusMsg: "HidHide..."; Check: NeedHidHide; Flags: waituntilterminated
Filename: "{tmp}\USBip.exe"; Parameters: "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART"; StatusMsg: "usbip-win2..."; Check: NeedUsbip; Flags: waituntilterminated
Filename: "{tmp}\AutoHotkey.exe"; Parameters: "/silent"; StatusMsg: "AutoHotkey v2..."; Components: ahk; Check: NeedAhk; Flags: waituntilterminated
; --- Azahar: unzip with the built-in tar, drop the launcher next to it ---
Filename: "{sys}\tar.exe"; Parameters: "-xf ""{tmp}\azahar.zip"" -C ""{code:AzaharDir}"" --strip-components 1"; StatusMsg: "Azahar..."; Components: azahar; Check: NeedAzahar; Flags: runhidden waituntilterminated
Filename: "{cmd}"; Parameters: "/c copy /y ""{app}\bin\azahar-launch.exe"" ""{code:AzaharDir}\azahar-launch.exe"""; Components: azahar; Flags: runhidden waituntilterminated
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\tools\Add-SteamShortcut.ps1"" -Name Azahar -Exe ""{code:AzaharDir}\azahar-launch.exe"" -Icon ""{code:AzaharDir}\azahar.exe"" -SteamDir ""{code:SteamDir}"" -RestartSteam"; StatusMsg: "Steam..."; Components: azahar\steam; Check: SteamFound; Flags: runhidden waituntilterminated
; --- logon tasks ---
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\tools\Register-LogonTask.ps1"" -Name ""3D-One Hotkeys"" -Command ""{code:AhkExe}"" -Arguments ""\""{localappdata}\3D-One-Hotkeys\3D-One-Hotkeys.ahk\"""" -DelaySeconds 5 -Description ""3D-One hotkeys (Ctrl+F4 guard for AstraDepth3D + Right Alt = Korean/English)"""; Components: ahk; Flags: runhidden waituntilterminated
Filename: "{code:AhkExe}"; Parameters: """{localappdata}\3D-One-Hotkeys\3D-One-Hotkeys.ahk"""; Components: ahk; Flags: nowait
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\tools\Register-LogonTask.ps1"" -Name ""AstraDepth3D Autostart"" -Command ""C:\Program Files (x86)\AstraDepth3D\AstraDepth3D.exe"" -DelaySeconds 10 -Description ""AstraDepth3D autostart"""; Components: astra; Flags: runhidden waituntilterminated
; --- OnePad: stop an older instance (any path), then start the new one; it registers its own logon task ---
Filename: "{app}\bin\onepad.exe"; Parameters: "--stop"; Flags: runhidden waituntilterminated; AfterInstall: WaitOldInstance
; drop any logon task left by a previous install path so the new onepad.exe re-registers itself with {app}
Filename: "schtasks.exe"; Parameters: "/Delete /TN OnePad /F"; Flags: runhidden waituntilterminated
Filename: "{app}\bin\onepad.exe"; WorkingDir: "{app}\bin"; Flags: nowait postinstall; Description: "OnePad 시작 / Start OnePad"

[UninstallRun]
Filename: "{app}\bin\onepad.exe"; Parameters: "--uninstall"; RunOnceId: "onepad-uninstall"; Flags: runhidden waituntilterminated
Filename: "schtasks.exe"; Parameters: "/Delete /TN ""3D-One Hotkeys"" /F"; RunOnceId: "ahk-task"; Flags: runhidden waituntilterminated

[Code]
var
  AzaharPage: TInputDirWizardPage;
  DlPage: TDownloadWizardPage;
  HidHideInstalledNow: Boolean;

const
  HidHideCli = 'C:\Program Files\Nefarius Software Solutions\HidHide\x64\HidHideCLI.exe';
  UsbipExe   = 'C:\Program Files\USBip\usbip.exe';
  AhkExe64   = 'C:\Program Files\AutoHotkey\v2\AutoHotkey64.exe';
  AstraExe   = 'C:\Program Files (x86)\AstraDepth3D\AstraDepth3D.exe';

function NeedHidHide: Boolean; begin Result := not FileExists(HidHideCli); end;
function NeedUsbip: Boolean;   begin Result := not FileExists(UsbipExe); end;
function NeedAhk: Boolean;     begin Result := not FileExists(AhkExe64); end;
function AstraInstalled: Boolean; begin Result := FileExists(AstraExe); end;
function AhkExe(Param: String): String; begin Result := AhkExe64; end;
function AzaharDir(Param: String): String; begin Result := AzaharPage.Values[0]; end;
function NeedAzahar: Boolean;  begin Result := not FileExists(AddBackslash(AzaharDir('')) + 'azahar.exe'); end;

function SteamDir(Param: String): String;
var s: String;
begin
  Result := '';
  if RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', s) then
  begin
    StringChangeEx(s, '/', '\', True);
    if FileExists(AddBackslash(s) + 'steam.exe') then Result := s;
  end;
end;
function SteamFound: Boolean; begin Result := SteamDir('') <> ''; end;

procedure KSleep(ms: Integer); external 'Sleep@kernel32.dll stdcall';
procedure WaitOldInstance;
begin
  KSleep(8000);   // let a previous OnePad (any path) finish detaching the virtual Deck before the new one starts
end;

function DefaultAzaharDir: String;
begin
  if DirExists('D:\') then Result := 'D:\Azahar' else Result := ExpandConstant('{sd}\Azahar');
end;

procedure InitializeWizard;
begin
  AzaharPage := CreateInputDirPage(wpSelectComponents, CustomMessage('AzaharDirTitle'), CustomMessage('AzaharDirDesc'), CustomMessage('AzaharDirSub'), False, '');
  AzaharPage.Add('');
  AzaharPage.Values[0] := DefaultAzaharDir;
  DlPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), CustomMessage('Downloading'), nil);
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (AzaharPage <> nil) and (PageID = AzaharPage.ID) then Result := not WizardIsComponentSelected('azahar');
end;

// schtasks /XML only accepts UTF-16 (UTF-8 with or without BOM is rejected as "malformed"), and Inno's
// SaveStringToFile writes bytes as-is, so the ASCII XML is widened by hand: BOM + each char followed by a zero byte.
function ToUtf16(const S: String): AnsiString;
var i: Integer;
begin
  Result := #$FF#$FE;
  for i := 1 to Length(S) do Result := Result + AnsiString(Chr(Ord(S[i]) and $FF)) + #0;
end;

// Logon task XML: no execution time limit (schtasks /SC ONLOGON would impose 72 h), highest privileges, small delay.
function WriteLogonTaskXml(const Path, Desc, Cmd, Args, Delay: String): Boolean;
var user, xml: String;
begin
  user := ExpandConstant('{computername}\{username}');
  xml :=
    '<?xml version="1.0" encoding="UTF-16"?>' + #13#10 +
    '<Task version="1.4" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">' + #13#10 +
    '  <RegistrationInfo><Description>' + Desc + '</Description></RegistrationInfo>' + #13#10 +
    '  <Triggers><LogonTrigger><Enabled>true</Enabled><UserId>' + user + '</UserId><Delay>' + Delay + '</Delay></LogonTrigger></Triggers>' + #13#10 +
    '  <Principals><Principal id="Author"><UserId>' + user + '</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>' + #13#10 +
    '  <Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>' +
    '<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><AllowHardTerminate>true</AllowHardTerminate><StartWhenAvailable>true</StartWhenAvailable>' +
    '<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Priority>7</Priority></Settings>' + #13#10 +
    '  <Actions Context="Author"><Exec><Command>' + Cmd + '</Command><Arguments>' + Args + '</Arguments></Exec></Actions>' + #13#10 +
    '</Task>' + #13#10;
  Result := SaveStringToFile(Path, ToUtf16(xml), False);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpReady then
  begin
    HidHideInstalledNow := NeedHidHide;
    DlPage.Clear;
    if NeedHidHide then DlPage.Add('{#HidHideUrl}', 'HidHide.exe', '');
    if NeedUsbip then DlPage.Add('{#UsbipUrl}', 'USBip.exe', '');
    if WizardIsComponentSelected('ahk') and NeedAhk then DlPage.Add('{#AhkUrl}', 'AutoHotkey.exe', '');
    if WizardIsComponentSelected('azahar') and NeedAzahar then DlPage.Add('{#AzaharUrl}', 'azahar.zip', '');
    DlPage.Show;
    try
      try
        DlPage.Download;
      except
        if DlPage.AbortedByUser then Log('Download aborted by user.') else SuppressibleMsgBox(AddPeriod(GetExceptionMessage), mbCriticalError, MB_OK, IDOK);
        Result := False;
      end;
    finally
      DlPage.Hide;
    end;
  end;
end;

// Stop a running OnePad before files are copied (otherwise bin\onepad.exe is in use and Setup aborts).
function PrepareToInstall(var NeedsRestart: Boolean): String;
var exe: String; rc: Integer;
begin
  Result := '';
  exe := ExpandConstant('{app}\bin\onepad.exe');
  if FileExists(exe) then
  begin
    Exec(exe, '--stop', '', SW_HIDE, ewWaitUntilTerminated, rc);
    KSleep(8000);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if WizardIsComponentSelected('azahar') then ForceDirectories(AzaharDir(''));
  end;
end;

function NeedRestart: Boolean;
begin
  Result := HidHideInstalledNow;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.FinishedLabel.Caption := WizardForm.FinishedLabel.Caption + #13#10#13#10 + CustomMessage('FinishNote');
end;
