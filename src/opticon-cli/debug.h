#ifndef _DEBUG_H
#define _DEBUG_H 1

#ifdef DEBUG
  #define dprintf(x, ...) printf(x,##__VA_ARGS__)
#else
  #define dprintf(x, ...) /**/
#endif

#endif
