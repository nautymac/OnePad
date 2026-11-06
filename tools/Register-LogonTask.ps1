# Registers (or replaces) a logon task for the current user: highest privileges, no execution time limit, small delay.
# Used by the installer; schtasks /SC ONLOGON cannot clear the 72 h time limit and schtasks /XML needs UTF-16, so this is simpler.
param(
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string]$Command,
    [string]$Arguments = "",
    [int]$DelaySeconds = 5,
    [string]$Description = ""
)
$ErrorActionPreference = "Stop"
$user = "$env:USERDOMAIN\$env:USERNAME"
$action = if ($Arguments) { New-ScheduledTaskAction -Execute $Command -Argument $Arguments } else { New-ScheduledTaskAction -Execute $Command }
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$trigger.Delay = "PT${DelaySeconds}S"
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -MultipleInstances IgnoreNew -StartWhenAvailable
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Highest
Register-ScheduledTask -TaskName $Name -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Description $Description -Force | Out-Null
"registered '$Name' -> $Command $Arguments (logon +${DelaySeconds}s, highest, no time limit)"
