#include <libopticon/glob.h>
#include <string.h>

bool globcmp (const char *str, const char *glob) {
    const char *m = glob;
    const char *n = str;
    const char *ma = NULL;
    const char *na = NULL;
    const char *pm = NULL;
    const char *pn = NULL;
    int just = 0;
    int lp_c = 0;
    int count = 0;
    int last_count = 0;
    
    while (1) {
        switch (*m) {
            case '*':
                goto match_star;
            case '%':
                goto match_perc;
            case '?':
                m++;
                if (! *n++) return false;
            case 0:
                if (!*n) return true;
                return false;
            case '\\':
                if ((*m == '\\' && m[1] == '*') || (m[1] == '?')) m++;
            default:
                if (*m != *n) return false;
                count++;
                m++;
                n++;
        }
    }
    
    while (1) {
        switch (*m) {
            case '*':
                match_star:
                ma = ++m;
                na = n;
                just = 1;
                last_count = count;
                break;
            case '%':
                match_perc:
                pm = ++m;
                pn = n;
                lp_c = count;
                if (*n == ' ') pm = NULL;
                break;
            case '?':
                if (!*n || just) return true;
            case '\\':
                if ((*m == '\\' && m[1] == '*') || (m[1] == '?')) m++;
            default:
                just = 0;
                if (*m != *n) {
                    if (!*n) return false;
                    if (pm) {
                        m = pm;
                        n = ++pn;
                        count = lp_c;
                        if (*n == ' ') pm = NULL;
                    }
                    else if (ma) {
                        m = ma;
                        n = ++na;
                        if (!*n) return false;
                        count = last_count;
                    }
                    else return false;
                }
                else {
                    count++;
                    m++;
                    n++;
                }
                break;
        }
    }
}
