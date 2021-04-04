#ifndef _OPTICON_TIMESTR_H
#define _OPTICON_TIMESTR_H 1

#include <time.h>

time_t parse_timestr (const char *tstr);
const char *make_timestr (time_t ti);

#endif
