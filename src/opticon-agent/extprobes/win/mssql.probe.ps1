$ErrorActionPreference = 'Stop'

function getMssqlCounters() {
	# Find counters/instances starting with "MSSQL$" or "SQLServer:"
	Get-Counter -ListSet * | Where-Object -FilterScript { $_.CounterSetName -match '^(MSSQL\$)|(SQLServer:)' }
}


# If script is called with argument "check" it will exit with an error code if no mssql counter is found
# (so the agent will disable the probe)
if ($args[0] -eq 'check') {
	$counterSetsCount = (getMssqlCounters | Measure-Object).Count
	if ($counterSetsCount -eq 0) {
		exit 1
	}
	
	Return
}


# The counter postfixes and the mapping to the result name
$counterMap = @{
	bsp = 'SQL Statistics\Batch Requests/sec'
	erps = 'SQL Errors(_Total)\Errors/sec'
	uc = 'General Statistics\User Connections'
}

# Consolidate all mssql counters per instance
$instanceNames = @{}
$counterSets = getMssqlCounters
foreach ($counterSet in $counterSets) {
	$instanceName = 'SQLServer'
	if ($counterSet.CounterSetName.StartsWith('MSSQL$')) {
		$pos = $counterSet.CounterSetName.IndexOf(':')
		$instanceName = $counterSet.CounterSetName.Substring(0, $pos)
	}
	$instanceNames[$instanceName] = $instanceName
}

$counterResults = @{}
foreach ($instanceName in $instanceNames.Keys) {
	foreach ($resultName in $counterMap.Keys) {
		$counterPath = '\' + $instanceName + ':' + $counterMap[$resultName]
		$counter = Get-Counter -Counter $counterPath
		
		if (-Not $counterResults.ContainsKey($resultName)) {
			$counterResults[$resultName] = @{
				total = 0
				count = 0
			}
		}
		$counterResults[$resultName]['total'] += $counter.CounterSamples.CookedValue
		$counterResults[$resultName]['count'] += 1
	}
}

$mssql = @{}
foreach ($resultName in $counterResults.Keys) {
	$mssql[$resultName] = $counterResults[$resultName]['total'] / $counterResults[$resultName]['count']
}

$result = @{}
if ($mssql.Count -gt 0) {
	$result['mssql'] = $mssql
}
$result | ConvertTo-Json -Depth 10




