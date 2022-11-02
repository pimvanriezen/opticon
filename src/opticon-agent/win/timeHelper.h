#pragma once

// 1 tick is 100 nanoseconds
#define WIN_TICKS_PER_SECOND 10000000

// Some windows api functions return LARGE_INTEGER instead of FILETIME, so we need to be able to convert the QuadPart of the LARGE_INTEGER (or ULARGE_INTEGER) representing the ticks
uint64_t ticksToUnixEpochSeconds(uint64_t ticks);
uint64_t unixEpochSecondsToTicks(uint64_t unixSeconds);
uint64_t winFileTimeToTicks(FILETIME time);
FILETIME ticksToWinFileTime(uint64_t ticks);
uint64_t winFileTimeToUnixSeconds(FILETIME time);
FILETIME unixSecondsToWinFileTime(uint64_t unixSeconds);