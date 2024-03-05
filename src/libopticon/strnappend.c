#include <libopticon/strnappend.h>
#include <string.h>

char *strnappend (char *s1, const char *s2, size_t maxsz) {
    if (! s1) return NULL;    
    size_t offs = strlen (s1);
    char *res = strncat (s1, s2, maxsz-offs);
    s1[maxsz-1] = 0;
    return res;
}
