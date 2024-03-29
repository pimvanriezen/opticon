// Needed for localtime_r declaration in time.h
#define _POSIX_THREAD_SAFE_FUNCTIONS

#include <libopticon/util.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h> // For struct sockaddr_in6

/* 5 bit character mapping for ids */
const char *IDTABLE = " abcdefghijklmnopqrstuvwxyz.-_/@";
uint64_t ASCIITABLE[] = {
    // 0x00
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x10
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x20
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 27, 30,
    // 0x30
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x40
    31, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    // 0x50
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 0, 0, 0, 29,
    // 0x60
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    // 0x70
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 0, 0, 0, 0
};

/*/ ======================================================================= /*/
/** Construct a meterid.
  * \param label The meter's label (max 11 characters).
  * \param type The meter's type (MTYPE_INT, etc).
  * \return A meterid_t value. */
/*/ ======================================================================= /*/
meterid_t makeid (const char *label, metertype_t type, int pos) {
    meterid_t res = (meterid_t) type;
    int bshift = 57;
    const char *crsr = label;
    char c;
    while ((c = *crsr) && bshift > 2) {
        if (c>0) {
            res |= (ASCIITABLE[(int)c] << bshift);
            bshift -= 5;
        }
        crsr++;
    }
    return res;
}

/*/ ======================================================================= /*/
/** Convert an id to a bitmask representing the non-0 caracters */
/*/ ======================================================================= /*/
meterid_t id2mask (meterid_t id) {
    meterid_t res = 0;
    int bshift = 57;
    while (bshift > 2) {
        uint64_t msk = 31ULL << bshift;
        if (! (id & msk)) return res;
        res |= msk;
        bshift -= 5;
    }
    return res;
}

/*/ ======================================================================= /*/
/** Determine whether one meterid is prefix for another one. The mask of the
  * prefix part of prefixfor should be provided pre-calculated.
  * Example: isidprefix ('top','top/pcpu', '***') would be true. With meterids
  * instead of strings.
  * \param potential The potential prefix node
  * \param prefixfor The potential 'child' node
  * \param mask Mask for the prefix part of the potential child.
  * \return 1 if matches, 0 if not. */
/*/ ======================================================================= /*/
int idisprefix (meterid_t potential, meterid_t prefixfor, meterid_t mask) {
    if ((prefixfor&mask) != (potential&mask)) return 0;
    int bshift = 57;
    while (bshift > 2) {
        uint64_t msk = 31ULL << bshift;
        if (! (mask & msk)) {
            if (((prefixfor & msk) >> bshift) == ASCIITABLE['/']) return 1;
            return 0;
        }
        bshift -= 5;
    }
    return 0;
}

/*/ ======================================================================= /*/
/** Get the prefix part of a meterid that has two levels, e.g. "top/pcpu".
  * \param id The potentially compound meterid.
  * \return The prefix, or 0 if the meter was not compound. */
/*/ ======================================================================= /*/
meterid_t idgetprefix (meterid_t id) {
    if (! (id & MMASK_NAME)) return 0ULL;
    meterid_t res = 0ULL;
    int bshift = 57;
    while (bshift > 2) {
        uint64_t msk = 31ULL << bshift;
        uint64_t ch = (id&msk) >> bshift;
        if (IDTABLE[ch] == '/') return res;
        res |= (id & msk);
        bshift -= 5;
    }
    return 0ULL;
}

/*/ ======================================================================= /*/
/** Extract an ASCII name from a meterid_t value.
  * \param id The meterid to parse.
  * \param into String to copy the ASCII data into (minimum size of 12). */
/*/ ======================================================================= /*/
void id2str (meterid_t id, char *into) {
    char *out = into;
    char *end = into;
    char t;
    uint64_t tmp;
    int bshift = 57;
    *out = 0;
    while (bshift > 2) {
        tmp = ((id & MMASK_NAME) >> bshift) & 0x1f;
        t = IDTABLE[tmp];
        *out = t;
        if (t != ' ') end = out;
        out++;
        bshift -= 5;
    }
    *(end+1) = 0;
}

/*/ ======================================================================= /*/
/** Extract that 'object' name from a meterid with a path
  * separator in it. Note we only support one level of
  * hierarchy. This is used for json encoding.
  * \param id The meterid, e.g., <<foo/quux>>
  * \param into The string to copy the id into. */
/*/ ======================================================================= /*/
void nodeid2str (meterid_t id, char *into) {
    char *out = into;
    char *end = into;
    char t;
    uint64_t tmp;
    int bshift = 57;
    int dowrite = 0;
    *out = 0;
    while (bshift > 2) {
        tmp = ((id & MMASK_NAME) >> bshift) & 0x1f;
        t = IDTABLE[tmp];
        if (t == '/') dowrite = 1;
        else if (dowrite)
        {
            *out = t;
            if (t != ' ') end = out;
            out++;
        }
        bshift -= 5;
    }
    *(end+1) = 0;
}

/*/ ======================================================================= /*/
/** returns the offset of a path separator, if any */
/*/ ======================================================================= /*/
uint64_t idhaspath (meterid_t id) {
    int res = 0;
    char t;
    uint64_t tmp;
    int bshift = 57;
    while (bshift > 2) {
        tmp = ((id & MMASK_NAME) >> bshift) & 0x1f;
        t = IDTABLE[tmp];
        if (t == '/') return idmask(res);
        res++;
        bshift -= 5;
    }
    return 0;
}

/*/ ======================================================================= /*/
/** generate a bitmask for sz characters into a meterid */
/*/ ======================================================================= /*/
uint64_t idmask (int sz) {
    uint64_t mask = 0;
    int bshift = 57;
    for (int i=0; i<sz; ++i) {
        mask |= ((0x1fULL) << bshift);
        bshift -= 5;
    }
    return mask;
}

#define uuidbyte(xx,yy) (unsigned int)((xx >> (56-(8*yy))) & 0xff)

/*/ ======================================================================= /*/
/** Write out a UUID to a string in ASCII 
  * \param u The UUID
  * \param into String buffer for the result, should fit 36 characters plus
  *             nul-terminator. */
/*/ ======================================================================= /*/
void uuid2str (uuid u, char *into) {
    sprintf (into, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                   "%02x%02x%02x%02x%02x%02x",
                   uuidbyte (u.msb, 0),
                   uuidbyte (u.msb, 1),
                   uuidbyte (u.msb, 2),
                   uuidbyte (u.msb, 3),
                   uuidbyte (u.msb, 4),
                   uuidbyte (u.msb, 5),
                   uuidbyte (u.msb, 6),
                   uuidbyte (u.msb, 7),
                   uuidbyte (u.lsb, 0),
                   uuidbyte (u.lsb, 1),
                   uuidbyte (u.lsb, 2),
                   uuidbyte (u.lsb, 3),
                   uuidbyte (u.lsb, 4),
                   uuidbyte (u.lsb, 5),
                   uuidbyte (u.lsb, 6),
                   uuidbyte (u.lsb, 7));
}

/*/ ======================================================================= /*/
/** Convert 16 bytes to uuid struct
  * \param bytes Array of 16 bytes */
/*/ ======================================================================= /*/
uuid bytes2uuid (uint8_t *bytes) {
    uuid u;
    u.msb = (uint64_t)bytes[0] << 56 |
        (uint64_t)bytes[1] << 48 |
        (uint64_t)bytes[2] << 40 |
        (uint64_t)bytes[3] << 32 |
        (uint64_t)bytes[4] << 24 |
        (uint64_t)bytes[5] << 16 |
        (uint64_t)bytes[6] << 8 |
        (uint64_t)bytes[7]
    ;
    u.lsb = (uint64_t)bytes[8] << 56 |
        (uint64_t)bytes[9] << 48 |
        (uint64_t)bytes[10] << 40 |
        (uint64_t)bytes[11] << 32 |
        (uint64_t)bytes[12] << 24 |
        (uint64_t)bytes[13] << 16 |
        (uint64_t)bytes[14] << 8 |
        (uint64_t)bytes[15]
    ;
    
    return u;
}

/*/ ======================================================================= /*/
/** Load a file as an allocated string. */
/*/ ======================================================================= /*/
char *load_file (const char *fn) {
    struct stat st;
    size_t sz;
    FILE *F;
    char *res;
    
    if (stat (fn, &st) != 0) return NULL;
    res = malloc (st.st_size+1);
    if (! res) return NULL;
    if (! (F = fopen (fn, "r"))) {
        free (res);
        return NULL;
    }
    sz = fread (res, st.st_size, 1, F);
    fclose (F);
    if (sz) {
        res[st.st_size] = 0;
        return res;
    }
    free (res);
    return NULL;
}

/*/ ======================================================================= /*/
/** Convert an address to a string */
/*/ ======================================================================= /*/
void ip2str (struct sockaddr_storage *remote, char *addrbuf) {
    if (remote->ss_family == AF_INET) {
        struct sockaddr_in *in = (struct sockaddr_in *) remote;
        inet_ntop (AF_INET, &in->sin_addr, addrbuf, 64);
    }
    else {
        struct sockaddr_in6 *in = (struct sockaddr_in6 *) remote;
        inet_ntop (AF_INET6, &in->sin6_addr, addrbuf, 64);
    }
}

/*/ ======================================================================= /*/
//* Convert a string to a parsed ipaddress */
/*/ ======================================================================= /*/
void str2ip (const char *addrbuf, struct sockaddr_storage *remote) {
    if (strchr (addrbuf, ':')) { /*v6*/
        remote->ss_family = AF_INET6;
        struct sockaddr_in6 *in = (struct sockaddr_in6 *) remote;
        inet_pton (AF_INET6, addrbuf, &in->sin6_addr);
    }
    else {
        remote->ss_family = AF_INET;
        struct sockaddr_in *in = (struct sockaddr_in *) remote;
        inet_pton (AF_INET, addrbuf, &in->sin_addr);
    }
}

/*/ ======================================================================= /*/
/** Format a tm structure */
/*/ ======================================================================= /*/
static char *tm2str (struct tm *tm, int zulu) {
    /* 1234-67-90T23:56:89Z */
    char *res = malloc ((size_t) 24);
    sprintf (res, "%i-%02i-%02iT%02i:%02i:%02i%s",
             tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec,
             zulu ? "Z":"");
    return res;
}

/*/ ======================================================================= /*/
/** Format a timestamp */
/*/ ======================================================================= /*/
char *time2str (time_t ti) {
    struct tm tm;
    localtime_r (&ti, &tm);
    return tm2str (&tm, 0);
}

/*/ ======================================================================= /*/
/** Format a UTC timestamp */
/*/ ======================================================================= /*/
char *time2utcstr (time_t ti) {
    struct tm tm;
    gmtime_r (&ti, &tm);
    return tm2str (&tm, 1);
}

/*/ ======================================================================= /*/
/** Parse a time string */
/*/ ======================================================================= /*/
static void parsetime (const char *str, struct tm *into) {
    if (strlen (str)<18) return;
    into->tm_year = atoi (str) - 1900;
    into->tm_mon = atoi (str+5) - 1;
    into->tm_mday = atoi (str+8);
    into->tm_hour = atoi (str+11);
    into->tm_min = atoi (str+14);
    into->tm_sec = atoi (str+17);
}

/*/ ======================================================================= /*/
/** Parse a time string into a time_t */
/*/ ======================================================================= /*/
time_t str2time (const char *tstr) {
    struct tm tm;
    parsetime (tstr, &tm);
    return mktime (&tm);
}

/*/ ======================================================================= /*/
/** Parse a UTC time string into a time_t */
/*/ ======================================================================= /*/
time_t utcstr2time (const char *tstr) {
    struct tm tm;
    parsetime (tstr, &tm);
    return timegm (&tm);
}
