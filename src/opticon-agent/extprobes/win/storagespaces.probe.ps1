$ErrorActionPreference = 'Stop'

# If script is called with argument "check" it will exit with an error code if no mssql counter is found
# (so the agent will disable the probe)
if ($args[0] -eq 'check') {
	$virtualDisk = Get-VirtualDisk | Select-Object -First 1
	if ($virtualDisk -eq $null) {
		exit 1
	}
	
	Return
}


# https://learn.microsoft.com/en-us/windows-server/storage/storage-spaces/storage-spaces-states

# Wrapping a command in @() prevents the output from being a single object vs an array when only a single item is returned
#$pools = @(Get-StoragePool -IsPrimordial $false)
#$pool = $pools[0]


$raid = New-Object System.Collections.Generic.List[System.Object]


Get-VirtualDisk | ForEach-Object -Process {
	$driveCount = 0
	$failCount = 0
	Get-PhysicalDisk -VirtualDisk $_ | ForEach-Object -Process {
		++$driveCount
		
		if ($_.HealthStatus -ne "Healthy") {
			++$failCount
		}
	}
	
	$status = $_.OperationalStatus
	if ($status -eq "Detached") {
		$status += '-' + $_.DetachedReason
	}
	
	$raid.Add(@{
		dev = 'Storage Spaces (' + $_.FriendlyName + ')'
		level = $_.ResiliencySettingName.ToLower()
		state = $status.ToUpper()
		count = $driveCount
		fail = $failCount
	})
}


$result = @{
	raid = $raid
}
$result | ConvertTo-Json -Depth 10