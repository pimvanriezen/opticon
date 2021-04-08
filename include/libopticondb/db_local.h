#ifndef _DB_LOCAL_H
#define _DB_LOCAL_H 1

#include <libopticondb/db.h>
#include <libopticon/codec_pkt.h>
#include <libopticon/datatypes.h>

/* =============================== TYPES =============================== */

/** Class structure for localdb */
typedef struct localdb_s {
    db               db; /** Super */
    char            *path; /** Access path for opened handle */
    char            *pathprefix; /** Global prefix for databases */
    codec           *codec; /** Codec reference (for reuse) */
    int              graphfd; /** Filedescriptor for graph */
} localdb;

#define GRAPHDATASZ (12*24*30)

typedef struct graphdata_s {
    uint32_t         writepos;
    uint32_t         count;
    double           accumulator;
    double           data[GRAPHDATASZ];
} graphdata;

typedef struct hostlogrecord_s {
    time_t           when;
    char             subsystem[12];
    char             message[240];
} hostlogrecord;

typedef struct hostlogdata_s {
    uint32_t         writepos;
    uint32_t         padding[63];
    hostlogrecord    rec[63];
} hostlogdata;

typedef struct hostloghandle_s {
    int              fd;
    hostlogdata     *data;
} hostloghandle;

/* ============================= FUNCTIONS ============================= */

datestamp    time2date (time_t in);
db          *localdb_create (const char *prefix);

#endif
