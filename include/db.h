#ifndef _DB_H
#define _DB_H 1

typedef int (*get_record_f)(struct db_s *, time_t, host *);
typedef uint64_t *(*get_vrangei_f)(struct db_s *, time_t, time_t, int,
                                   const char *, uint8_t);
typedef double *(*get_vrangef_f)(struct db_s *, time_t, time_t, int,
                                 const char *, uint8_t);
typedef int (*save_record_f)(struct db_s *, time_t, host *);

typedef struct db_s {
    get_record_f     get_record;
    get_vrangei_f    get_value_range_int;
    get_vrangef_f    get_value_range_frac;
    save_record_f    save_record;
} db;

typedef struct localdb_s {
    db               db;
    char            *path;
    FILE            *findex;
} localdb;

db          *db_open_local (const char *path);
int          db_get_record (db *d, time_t when, host *into);
uint64_t    *db_get_value_range_int (db *d, time_t from, time_t to,
                                     int numsamples, const char *key,
                                     uint8_t arrayindex);
double      *db_get_value_range_frac (db *d, time_t from, time_t to,
                                      int numsamples, const char *key,
                                      uint8_t arrayindex);
int          db_save_record (db *d, time_t when, host *what);

#endif
