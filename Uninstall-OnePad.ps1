# Stops OnePad, removes the logon task, restores the controller firmware settings, HID device nodes and HidHide state.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $root 'bin\onepad.exe'
if (Test-Path $exe) { & $exe --uninstall } else { schtasks /delete /tn OnePad /f 2>$null | Out-Null }
Get-Process onepad -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
# make sure the HID collections are enabled again even if the controller is absent right now
Get-PnpDevice | Where-Object { $_.InstanceId -match 'VID_2993&PID_0012&MI_01&COL0[235]' } | ForEach-Object { try { Enable-PnpDevice -InstanceId $_.InstanceId -Confirm:$false -ErrorAction Stop } catch {} }
Write-Host 'OnePad removed. Config/logs remain in %LOCALAPPDATA%\OnePad'
