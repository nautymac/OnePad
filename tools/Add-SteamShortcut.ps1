<#
.SYNOPSIS
  Steam 비Steam 게임(shortcuts.vdf)에 항목을 추가한다. Steam이 켜져 있으면 먼저 종료한다(켜진 채 편집하면 종료 시 덮어써짐).
.EXAMPLE
  .\Add-SteamShortcut.ps1 -Name Azahar -Exe "D:\Azahar\azahar-launch.exe" -StartDir "D:\Azahar\" -Icon "D:\Azahar\azahar.exe"
.NOTES
  바이너리 VDF: 0x00=오브젝트, 0x01=문자열(NUL 종료), 0x02=UInt32 LE, 0x08=오브젝트 끝. 파일 끝은 08 08.
  appid = crc32('"' + exe + '"' + name) | 0x80000000. 실행 GameID = (appid << 32) | 0x02000000 → steam://rungameid/<GameID>
#>
param(
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string]$Exe,
    [string]$StartDir = "",
    [string]$Icon = "",
    [string]$SteamDir = "D:\Steam",
    [switch]$RestartSteam
)
$ErrorActionPreference = "Stop"
if (-not $StartDir) { $StartDir = (Split-Path $Exe) + "\" }
$users = Get-ChildItem "$SteamDir\userdata" -Directory | Where-Object { Test-Path "$($_.FullName)\config" }
if ($users.Count -ne 1) { throw "userdata 폴더가 하나가 아닙니다: $($users.Name -join ', ') — -SteamDir 또는 수동 지정 필요" }
$vdf = "$($users[0].FullName)\config\shortcuts.vdf"

function Read-Vdf { if (Test-Path $vdf) { [IO.File]::ReadAllBytes($vdf) } else { [byte[]](0) + [Text.Encoding]::UTF8.GetBytes("shortcuts") + [byte[]](0, 8, 8) } }
$bytes = Read-Vdf
if ([Text.Encoding]::UTF8.GetString($bytes) -match "\x01appname\x00$([regex]::Escape($Name))\x00") { "already present: $Name"; exit 0 }

if (Get-Process steam -ErrorAction SilentlyContinue) {
    & "$SteamDir\steam.exe" -shutdown
    $t = 0; while ((Get-Process steam -ErrorAction SilentlyContinue) -and $t -lt 60) { Start-Sleep 1; $t++ }
    if (Get-Process steam -ErrorAction SilentlyContinue) { throw "Steam이 종료되지 않았습니다" }
    $bytes = Read-Vdf   # Steam이 종료하며 다시 쓴 최신본
}
$text = [Text.Encoding]::UTF8.GetString($bytes)
if ($bytes[-1] -ne 8 -or $bytes[-2] -ne 8) { throw "shortcuts.vdf 끝이 08 08이 아닙니다 — 손대지 않음" }
$count = ([regex]::Matches($text, "\x01appname\x00")).Count

$table = 0..255 | ForEach-Object { $c = [uint32]$_; for ($k = 0; $k -lt 8; $k++) { if ($c -band 1) { $c = 0xEDB88320 -bxor ($c -shr 1) } else { $c = $c -shr 1 } }; $c }
$crc = [uint32]4294967295
foreach ($b in [Text.Encoding]::UTF8.GetBytes(('"' + $Exe + '"') + $Name)) { $crc = $table[($crc -bxor $b) -band 0xFF] -bxor ($crc -shr 8) }
$crc = [uint32]($crc -bxor 4294967295)
$appid = [uint32]($crc -bor 0x80000000)

$e = New-Object System.Collections.Generic.List[byte]
function S([string]$k, [string]$v) { $script:e.Add(1); $script:e.AddRange([Text.Encoding]::UTF8.GetBytes($k)); $script:e.Add(0); $script:e.AddRange([Text.Encoding]::UTF8.GetBytes($v)); $script:e.Add(0) }
function I([string]$k, [uint32]$v) { $script:e.Add(2); $script:e.AddRange([Text.Encoding]::UTF8.GetBytes($k)); $script:e.Add(0); $script:e.AddRange([BitConverter]::GetBytes($v)) }
$e.Add(0); $e.AddRange([Text.Encoding]::UTF8.GetBytes("$count")); $e.Add(0)
I "appid" $appid
S "appname" $Name
S "Exe" ('"' + $Exe + '"')
S "StartDir" $StartDir
S "icon" $Icon
S "ShortcutPath" ""
S "LaunchOptions" ""
I "IsHidden" 0
I "AllowDesktopConfig" 1
I "AllowOverlay" 1
I "OpenVR" 0
I "Devkit" 0
S "DevkitGameID" ""
I "DevkitOverrideAppID" 0
I "LastPlayTime" 0
S "FlatpakAppID" ""
$e.Add(0); $e.AddRange([Text.Encoding]::UTF8.GetBytes("tags")); $e.Add(0); $e.Add(8)
$e.Add(8)
$out = New-Object System.Collections.Generic.List[byte]
$out.AddRange([byte[]]$bytes[0..($bytes.Length - 3)]); $out.AddRange($e); $out.Add(8); $out.Add(8)
if (Test-Path $vdf) { Copy-Item $vdf "$vdf.backup-$(Get-Date -Format yyyyMMddHHmmss)" -Force }
[IO.File]::WriteAllBytes($vdf, $out.ToArray())
$gameid = ([uint64]$appid -shl 32) -bor 0x02000000
"added '$Name' (#$count) appid=$appid  run: steam://rungameid/$gameid"
if ($RestartSteam) { explorer.exe "$SteamDir\steam.exe" }
