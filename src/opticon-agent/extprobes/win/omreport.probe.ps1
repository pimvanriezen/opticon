# Allow try/catch on non-terminating errors (making them terminating)
$ErrorActionPreference = 'Stop'

# If script is called with argument "check" it will exit with an error code if the "omreport" command is not found
# (so the agent will disable the probe)
if ($args[0] -eq 'check') {
	Try {
		Get-Command omreport
	}
	Catch {
		exit 1
	}
	
	Return
}


# @note If a pipeline has no result to return, it returns null which evaluates to false in an if-statement


$health = New-Object System.Collections.Generic.List[System.Object]

# Chassis Health
#$example = 'Health
#
#SEVERITY : COMPONENT
#Ok       : Main System Chassis
#
#For further help, type the command followed by -?
#'
##$hasProblem = $example -split "`r`n" |
#$hasProblem = omreport system |
#	Select-Object -Skip 3 |
#	Select-String -CaseSensitive -Pattern '^([^\s:]+)\s*: ' |
#	Where-Object { $_.Matches[0].Groups[1].Value -ne 'Ok' } |
#	ForEach-Object -Process { $true }
#	Select-Object -First 1


# Chassis components health
$example = 'Health

Main System Chassis

SEVERITY : COMPONENT
Ok       : Fans
Ok       : Intrusion
Ok       : Memory
Ok       : Power Supplies
Ok       : Power Management
Ok       : Processors
Ok       : Temperatures
Ok       : Voltages
Ok       : Hardware Log
Ok       : Batteries

For further help, type the command followed by -?

'
$example -split "`r`n" |
#omreport chassis |
	Select-Object -Skip 5 |
	Select-String -CaseSensitive -Pattern '^([a-zA-Z]+)\s+: ([a-zA-Z]+)' |
	ForEach-Object -Process {
		$health.Add(@{
			id = $_.Matches[0].Groups[2].Value
			s = $_.Matches[0].Groups[1].Value
		})
	}


# Storage health
$example = ' Controller  PERC H730P Mini(Embedded)

Controller
ID                                            : 0
Status                                        : Ok
Name                                          : PERC H730P Mini
Slot ID                                       : Embedded
State                                         : Ready
Firmware Version                              : 25.5.9.0001
Minimum Required Firmware Version             : Not Applicable
Driver Version                                : 6.604.06.00
Minimum Required Driver Version               : Not Applicable
Storport Driver Version                       : 10.0.20348.740
Minimum Required Storport Driver Version      : Not Applicable
Number of Connectors                          : 2
Rebuild Rate                                  : 30%
BGI Rate                                      : 30%
Check Consistency Rate                        : 30%
Reconstruct Rate                              : 30%
Alarm State                                   : Not Applicable
Cluster Mode                                  : Not Applicable
SCSI Initiator ID                             : Not Applicable
Cache Memory Size                             : 2048 MB
Patrol Read Mode                              : Auto
Patrol Read State                             : Stopped
Patrol Read Rate                              : 30%
Patrol Read Iterations                        : 74
Abort Check Consistency on Error              : Disabled
Allow Revertible Hot Spare and Replace Member : Enabled
Load Balance                                  : Not Applicable
Auto Replace Member on Predictive Failure     : Disabled
Redundant Path view                           : Not Applicable
CacheCade Capable                             : Not Applicable
Persistent Hot Spare                          : Disabled
Encryption Capable                            : Yes
Encryption Key Present                        : No
Encryption Mode                               : None
Preserved Cache                               : Not Applicable
Spin Down Unconfigured Drives                 : Disabled
Spin Down Hot Spares                          : Disabled
Spin Down Configured Drives                   : Disabled
Automatic Disk Power Saving (Idle C)          : Disabled
Start Time (HH:MM)                            : Not Applicable
Time Interval for Spin Up (in Hours)          : Not Applicable
T10 Protection Information Capable            : No
Non-RAID HDD Disk Cache Policy                : Unchanged
Current Controller Mode                       : RAID
'
$example -split "`r`n" |
#omreport storage controller |
	Select-String -CaseSensitive -Pattern '^Status\s+: ([a-zA-Z]+)' |
	ForEach-Object -Process {
		$health.Add(@{
			id = 'Storage'
			s = $_.Matches[0].Groups[1].Value
		})
	} |
	Select-Object -First 1


# Storage Virtual Disk Health
$example = 'List of Virtual Disks in the System

Controller PERC H730P Mini (Embedded)
ID                                : 0
Status                            : Ok
Name                              : STORAGE
State                             : Ready
Hot Spare Policy violated         : Not Assigned
Encrypted                         : No
Layout                            : RAID-5
Size                              : 1,197.06 GB (1285329059840 bytes)
T10 Protection Information Status : No
Associated Fluid Cache State      : Not Applicable
Device Name                       : Windows Disk 0
Bus Protocol                      : SATA
Media                             : SSD
Read Policy                       : Read Ahead
Write Policy                      : Write Back
Cache Policy                      : Not Applicable
Stripe Element Size               : 64 KB
Disk Cache Policy                 : Unchanged

ID                                : 1
Status                            : Ok
Name                              : OS
State                             : Ready
Hot Spare Policy violated         : Not Assigned
Encrypted                         : No
Layout                            : RAID-5
Size                              : 103.19 GB (110803746816 bytes)
T10 Protection Information Status : No
Associated Fluid Cache State      : Not Applicable
Device Name                       : Windows Disk 1
Bus Protocol                      : SATA
Media                             : SSD
Read Policy                       : Read Ahead
Write Policy                      : Write Back
Cache Policy                      : Not Applicable
Stripe Element Size               : 64 KB
Disk Cache Policy                 : Unchanged
'
#$example -split "`r`n" |
#omreport storage vdisk |


$result = @{
	health = $health
}
$result | ConvertTo-Json -Depth 10