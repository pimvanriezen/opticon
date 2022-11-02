#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#include <windows.h>

#include "timeHelper.h"

// The FILETIME struct represents the number of ticks since the windows epoch (1601-01-01 UTC)

// The unix epoch is lots of ticks later than the windows epoch
#define UNIX_EPOCH_DIFF_IN_SECONDS 0x019db1ded53e8000

uint64_t ticksToUnixEpochSeconds(uint64_t ticks) {
	return (ticks - UNIX_EPOCH_DIFF_IN_SECONDS) / WIN_TICKS_PER_SECOND;
}

uint64_t unixEpochSecondsToTicks(uint64_t unixSeconds) {
	return (unixSeconds * WIN_TICKS_PER_SECOND) + UNIX_EPOCH_DIFF_IN_SECONDS;
}

uint64_t winFileTimeToTicks(FILETIME time) {
	// Copy the low and high parts of FILETIME into a ULARGE_INTEGER to access the 64-bits as an uint64_t
	ULARGE_INTEGER li;
	li.LowPart = time.dwLowDateTime;
	li.HighPart = time.dwHighDateTime;
	// li.QuadPart now contains the number of ticks as uint64_t
	
	// @note Alternative to ULARGE_INTEGER is bit shifts, but this seems to yield very different values
	//return time.dwHighDateTime << 32 | time.dwLowDateTime;
	
	return li.QuadPart;
}

FILETIME ticksToWinFileTime(uint64_t ticks) {
	ULARGE_INTEGER li;
	li.QuadPart = ticks;
	
	// Copy the low and high parts of ULARGE_INTEGER into the FILETIME
	FILETIME time;
	time.dwLowDateTime = li.LowPart;
	time.dwHighDateTime = li.HighPart;
	
	return time;
}

uint64_t winFileTimeToUnixSeconds(FILETIME time) {
	return ticksToUnixEpochSeconds(winFileTimeToTicks(time));
}

FILETIME unixSecondsToWinFileTime(uint64_t unixSeconds) {
	return ticksToWinFileTime(unixEpochSecondsToTicks(unixSeconds));
}