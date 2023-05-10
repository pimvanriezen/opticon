# Allow try/catch on non-terminating errors (making them terminating)
$ErrorActionPreference = 'Stop'

$useExampleCommand = $false

$omreportCommand = "omreport"
if ($useExampleCommand) {
	$omreportCommand = ".\omreport.probe.ps1 example"
}

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
elseif ($args[0] -eq 'example') {
	if ($args[1] -eq 'chassis') {
		# Example output of "omreport chassis"
		'Health

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

' -split "`r`n"
	}
	elseif ($args[1] -eq 'storage') {
		if ($args[2] -eq 'controller') {
			# Example output of "omreport storage controller"
			' Controller  PERC H730P Mini(Embedded)

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

Controller
ID                                            : 1
Status                                        : Ok
Name                                          : PCIe SSD Subsystem
Slot ID                                       : Not Applicable
State                                         : Ready
Firmware Version                              : Not Available

Controller
ID                                            : 2
Status                                        : Ok
Name                                          : BOSS-S1
Slot ID                                       : Embedded
State                                         : Ready
Firmware Version                              : 2.5.13.3024
Minimum Required Firmware Version             : Not Applicable
' -split "`r`n"
		}
		elseif ($args[2] -eq 'vdisk') {
			# Example output of "omreport storage vdisk"
			'List of Virtual Disks in the System

Controller PERC H730P Mini (Embedded)
ID                                : 0
Status                            : Non-Critical
Name                              : STORAGE
State                             : Degraded
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
Status                            : Non-Critical
Name                              : OS
State                             : Degraded
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
' -split "`r`n"
		}
	}
	
	Return
}


# If you want to use a multiline string in a pipeline:
#$example -split "`r`n" |


# If a pipeline has no result to return, it returns null which evaluates to false in an if-statement


$health = New-Object System.Collections.Generic.List[System.Object]


# Chassis components health
Set-Variable FINDING_COMPONENTS -Option Constant -Value 0
Set-Variable PROCESSING_COMPONENTS -Option Constant -Value 1
$state = $FINDING_COMPONENTS

Invoke-Expression ($omreportCommand + ' chassis') |
	Select-String -CaseSensitive -Pattern '^(\S+)\s+: (\S+)' |
	ForEach-Object -Process {
		$status = $_.Matches[0].Groups[1].Value
		$component = $_.Matches[0].Groups[2].Value
		
		if ($state -eq $FINDING_COMPONENTS) {
			if ($status -eq 'SEVERITY' -And $component -eq 'COMPONENT') {
				$state = $PROCESSING_COMPONENTS
			}
		}
		elseif ($state -eq $PROCESSING_COMPONENTS) {
			$health.Add(@{
				id = $component
				s = $status.ToUpper()
			})
		}
	}


# Storage Controller health
Set-Variable FINDING_CONTROLLER -Option Constant -Value 0
Set-Variable PROCESSING_CONTROLLER -Option Constant -Value 1
$state = $FINDING_CONTROLLER

$controllerId = ''
$controllerStatus = ''
$controllerName = ''
Invoke-Expression ($omreportCommand + ' storage controller') |
	ForEach-Object -Process {
		if ($state -eq $FINDING_CONTROLLER) {
			if ($_.StartsWith('Controller')) {
				$state = $PROCESSING_CONTROLLER
			}
		}
		elseif ($state -eq $PROCESSING_CONTROLLER) {
			if ($_ -match '^(\S+)\s+: (\S+)') {
				$key = $Matches[1]
				$value = $Matches[2]
				
				if ($key -eq 'ID') {
					$controllerId = $value
				}
				elseif ($key -eq 'Status') {
					$controllerStatus = $value
				}
				elseif ($key -eq 'Name') {
					$controllerName = $value
					
					$health.Add(@{
						id = 'Storage Controller ' + $controllerId + ' ' + $controllerName
						s = $controllerStatus.ToUpper()
					})
					$state = $FINDING_CONTROLLER
				}
			}
		}
	}


# Storage Virtual Disks (raid sets, include their own health status)
$raid = New-Object System.Collections.Generic.List[System.Object]

Set-Variable FINDING_VDISKCONTROLLER -Option Constant -Value 0
Set-Variable FINDING_VDISK -Option Constant -Value 1
Set-Variable PROCESSING_VDISK -Option Constant -Value 2
$state = $FINDING_VDISK_CONTROLLER

$vdiskController = ''
$vdiskId = ''
$vdiskStatus = ''
$vdiskName = ''
$vdiskLayout = ''
Invoke-Expression ($omreportCommand + ' storage vdisk') |
	ForEach-Object -Process {
		if ($state -eq $FINDING_VDISK_CONTROLLER) {
			if ($_.StartsWith('Controller ')) {
				$vdiskController = $_.Substring(11) # 11 is the length of "Controller "
				$state = $FINDING_VDISK
			}
		}
		elseif ($state -eq $FINDING_VDISK) {
			if ($_ -match '^(\S+)\s+: (\S+)') {
				$key = $Matches[1]
				$value = $Matches[2]
				
				if ($key -eq 'ID') {
					$vdiskId = $value
	      			$state = $PROCESSING_VDISK
				}
			}
		}
		elseif ($state -eq $PROCESSING_VDISK) {
			if ($_ -match '^(\S+)\s+: (\S+)') {
				$key = $Matches[1]
				$value = $Matches[2]
				
				if ($key -eq 'Status') {
					$vdiskStatus = $value
				}
				elseif ($key -eq 'Name') {
					$vdiskName = $value
				}
				elseif ($key -eq 'Layout') {
					$vdiskLayout = $value
					
					$raid.Add(@{
						dev = $vdiskController
						name = 'Vdisk ' + $vdiskId + ': ' + $vdiskName
						level = $vdiskLayout.ToLower().Replace('-', '')
						state = $vdiskStatus.ToUpper()
					})
	      			$state = $FINDING_VDISK
				}
			}
			elseif ($_ -eq '') {
	      		$state = $FINDING_VDISK_CONTROLLER
			}
		}
	}


$result = @{
	health = $health
	raid = $raid
}
$result | ConvertTo-Json -Depth 10