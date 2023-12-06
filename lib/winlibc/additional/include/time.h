#pragma once
#pragma GCC system_header

// @note We're extending this system header
#include_next <time.h>

time_t timegm(const struct tm *time);

char *strptime(const char *s, const char *format, struct tm *tm);
struct tm *gmtime_r(const time_t *timep, struct tm *result);

// @note if _POSIX_THREAD_SAFE_FUNCTIONS is defined, MinGW defines localtime_r() as a forward to localtime_s() which is a forward to _localtime64_s()
#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
	struct tm *localtime_r(const time_t *seconds, struct tm *result);
#endif