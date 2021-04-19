#include <libopticon/timer.h>
#include <string.h>

void timer_start (timer *self) {
    self->diff = 0.0;
    gettimeofday (&self->tv, NULL);
}

void timer_end (timer *self) {
    struct timeval tvnow;
    gettimeofday (&tvnow, NULL);
    int secdiff = tvnow.tv_sec - self->tv.tv_sec;
    int usecdiff = tvnow.tv_usec - self->tv.tv_usec;
    if (secdiff < 0) {
        self->diff = 0.0;
        return;
    }
    if (secdiff == 0 && usecdiff < 0) {
        self->diff = 0.0;
        return;
    }
    double res = (1000.0 * secdiff);
    res += usecdiff / 1000.0;
    self->diff = res;
}
