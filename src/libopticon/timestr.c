#include <stdlib.h>
#include <stdio.h>
#include "libopticon/timestr.h"

time_t parse_timestr (const char *tstr) {
    struct tm t;
    if (! strptime (tstr, "%Y-%m-%dT%H:%M", &t)) return 0;
    return timegm (&t);
}

const char *make_timestr (time_t ti) {
    struct tm t;
    
    gmtime_r (&ti, &t);
    char *res = malloc ((size_t) 32);
    snprintf (res, 31, "%4i-%02i-%02iT%02i:%02i", t.tm_year + 1900,
              t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min);
    return res;
}
