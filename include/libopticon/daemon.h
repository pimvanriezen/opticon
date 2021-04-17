#ifndef _DAEMON_H
#define _DAEMON_H 1

#include <unistd.h>
#include <stdbool.h>

/* =============================== TYPES =============================== */

typedef int (*main_f)(int, const char *[]);

/* ============================= FUNCTIONS ============================= */

bool daemonize (const char *pidf, int argc, const char *argv[],
                main_f call, int keep_foreground);

#endif
