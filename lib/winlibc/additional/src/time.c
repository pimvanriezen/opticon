#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#include <time.h>
#include <windows.h>

// @note if _POSIX_THREAD_SAFE_FUNCTIONS is defined, MinGW defines localtime_r() as a forward to localtime_s() which is a forward to _localtime64_s()
#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
	struct tm *localtime_r(const time_t *seconds, struct tm *result) {
		return _localtime64_s(result, seconds) ? NULL : result;
	}
#endif

// Algorithm: http://howardhinnant.github.io/date_algorithms.html
static int getDaysSinceEpoch(int year, int month, int day) {
	year -= month <= 2;
	int era = year / 400;
	int yearOfEra = year - era * 400;												// [0, 399]
	int dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;		// [0, 365]
	int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;	// [0, 146096]
	return era * 146097 + dayOfEra - 719468;
}

// Convert time struct into unix epoch seconds
time_t timegm(const struct tm *time) {
	int year = time->tm_year + 1900;
	int month = time->tm_mon; // 0-11
	if (month > 11) {
		year += month / 12;
		month %= 12;
	}
	else if (month < 0) {
		int yearsDiff = (11 - month) / 12;
		year -= yearsDiff;
		month += 12 * yearsDiff;
	}
	int daysSinceEpoch = getDaysSinceEpoch(year, month + 1, time->tm_mday);

	return 60 * (60 * (24L * daysSinceEpoch + time->tm_hour) + time->tm_min) + time->tm_sec;
}