#ifndef _LIBOPTICON_TIMER_H
#define _LIBOPTICON_TIMER_H

#include <sys/time.h>

typedef struct timer_s {
    struct timeval tv;
    double diff;
} timer;

void timer_start (timer *);
void timer_end (timer *);

#endif
