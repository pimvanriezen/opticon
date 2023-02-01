#ifndef _POPEN_H
#define _POPEN_H 1

#include <stdio.h>
#include <libopticon/var.h>

void popen_init (void);
FILE *popen_safe_env (const char *, const char *, var *);
FILE *popen_safe (const char *, const char *);
int pclose_safe (FILE *);

#endif
