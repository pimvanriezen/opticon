# Allow try/catch on non-terminating errors (making them terminating)
$ErrorActionPreference = 'Stop'

$omreportCommand = "omreport"
# Uncomment For testing:
#$omreportCommand = ".\omreport.example.ps1"


# If script is called with argument "check" it will exit with an error code if the "omreport" command is not found
# (so the agent will disable the probe)
if ($args[0] -eq 'check') {
	Try {
		$ignore = Get-Command $omreportCommand
	}
	Catch {
		exit 1
	}
	
	Return
}


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
$controllerIds = New-Object System.Collections.Generic.List[System.Object]

$controllerId = ''
$controllerStatus = ''
$controllerName = ''
Invoke-Expression ($omreportCommand + ' storage controller') |
	Select-String -CaseSensitive -Pattern '^(\S+)\s+: (\S+)' |
	ForEach-Object -Process {
		$key = $_.Matches[0].Groups[1].Value
		$value = $_.Matches[0].Groups[2].Value
		
		if ($key -eq 'ID') {
			$controllerId = $value
			$controllerIds.Add($controllerId)
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
		}
	}


# Storage Virtual Disks (raid sets)
$raid = New-Object System.Collections.Generic.List[System.Object]

$vdiskController = ''
$vdiskId = ''
$vdiskStatus = ''
$vdiskName = ''
$vdiskLayout = ''
Foreach ($controllerId in $controllerIds) {
	Invoke-Expression ($omreportCommand + ' storage vdisk controller=' + $controllerId) |
	ForEach-Object -Process {
		if ($_.StartsWith('Controller ')) {
			$vdiskController = $_.Substring(11) # 11 is the length of "Controller "
		}
		elseif ($_ -match '^(\S+)\s+: (\S+)') {
			$key = $Matches[1]
			$value = $Matches[2]
			
			if ($key -eq 'ID') {
				$vdiskId = $value
			}
			elseif ($key -eq 'Status') {
				$vdiskStatus = $value
			}
			elseif ($key -eq 'Name') {
				$vdiskName = $value
			}
			elseif ($key -eq 'Layout') {
				$vdiskLayout = $value
				
				$vdiskDriveCount = 0
		        $vdiskFailCount = 0
		        Invoke-Expression ($omreportCommand + ' storage pdisk controller=' + $controllerId + ' vdisk=' + $vdiskId) |
					ForEach-Object -Process {
						if ($_ -match '^(\S+)\s+: (\S+)') {
							$key = $Matches[1]
							$value = $Matches[2]
							
							if ($key -eq 'Status') {
								++$vdiskDriveCount
								if ($value -ne 'Ok') {
									++$vdiskFailCount
								}
							}
						}
					}
		        
				$raid.Add(@{
					dev = $vdiskController
					name = 'vdisk ' + $vdiskId + ': ' + $vdiskName
					level = $vdiskLayout.ToLower().Replace('-', '')
					state = $vdiskStatus.ToUpper()
					count = $vdiskDriveCount
					fail = $vdiskFailCount
				})
			}
		}
	}
}


$result = @{
	health = $health
	raid = $raid
}
$result | ConvertTo-Json -Depth 10