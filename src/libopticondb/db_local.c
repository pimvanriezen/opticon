#include <libopticondb/db_local.h>
#include <libopticon/ioport_file.h>
#include <libopticon/codec.h>
#include <libopticon/util.h>
#include <libopticon/var_parse.h>
#include <libopticon/var_dump.h>
#include <libopticon/log.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#define LOCALDB_OFFS_INVALID 0xffffffffffffffffULL
#define LOCALDB_FLAG_NOCREATE 0x0000001

/** Convert epoch time to a GMT date stamp integer, to be used as part of
  * the filename of underlying database files.
  */
datestamp time2date (time_t in) {
    datestamp res = 0;
    struct tm tmin;
    gmtime_r (&in, &tmin);
    res = 10000 * (1900+tmin.tm_year) +
          100 * (1+tmin.tm_mon) + tmin.tm_mday;
    return res;
}

/** Bind a localdb to a specific, existing, tenant.
  * \param d The db handle
  * \param tenant The tenant uuid
  * \param options Driver-specific options (unused).
  * \return 1 on success, 0 on failure.
  */
int localdb_open (db *d, uuid tenant, var *options) {
    localdb *self = (localdb *) d;
    char *insertpos = NULL;
    struct stat st;
    char uuidstr[40];
    int prefixlen = strlen (self->pathprefix);
    uuid2str (tenant, uuidstr);
    if (self->path) free (self->path);
    self->path = (char *) malloc (prefixlen + 64);
    strcpy (self->path, self->pathprefix);
    insertpos = self->path + prefixlen;
    if (*(insertpos-1) != '/') {
        insertpos[0] = '/';
        insertpos++;
    }
    sprintf (insertpos, "%c%c", uuidstr[0], uuidstr[1]);
    insertpos += 2;
    if (stat (self->path, &st) != 0) return 0;
    sprintf (insertpos, "/%c%c", uuidstr[2], uuidstr[3]);
    insertpos += 3;
    if (stat (self->path, &st) != 0) return 0;
    sprintf (insertpos, "/%s", uuidstr);
    if (stat (self->path, &st) != 0) return 0;
    strcat (self->path, "/");
    return 1;
}

/** Open the database file for a specified datestamp */
FILE *localdb_open_dbfile (localdb *ctx, uuid hostid, datestamp dt, int flags) {
    char uuidstr[40];
    struct stat st;
    char *dbpath = (char *) malloc (strlen (ctx->path) + 64);
    if (! dbpath) return NULL;

    uuid2str (hostid, uuidstr);
    sprintf (dbpath, "%s/%s/%u.db", ctx->path, uuidstr, dt);
    if (flags & LOCALDB_FLAG_NOCREATE) {
        if (stat (dbpath, &st) != 0) {
            free (dbpath);
            return NULL;
        }
    }
    FILE *res = fopen (dbpath, "a+");
    if (! res) {
        sprintf (dbpath, "%s/%s", ctx->path, uuidstr);
        if (mkdir (dbpath, 0750) != 0) {
            free (dbpath);
            return NULL;
        }
        sprintf (dbpath, "%s/%s/%u.db", ctx->path, uuidstr, dt);
        res = fopen (dbpath, "a+");
    }
    free (dbpath);
    return res;
}

/** Open the index file for a specified datestamp */
FILE *localdb_open_indexfile (localdb *ctx, uuid hostid, datestamp dt, int flags) {
    char uuidstr[40];
    struct stat st;
    char *dbpath = (char *) malloc (strlen (ctx->path) + 64);
    if (! dbpath) return NULL;

    uuid2str (hostid, uuidstr);
    sprintf (dbpath, "%s/%s/%u.idx", ctx->path, uuidstr, dt);
    if (flags & LOCALDB_FLAG_NOCREATE) {
        if (stat (dbpath, &st) != 0) {
            free (dbpath);
            return NULL;
        }
    }
    FILE *res = fopen (dbpath, "a+");
    if (! res) {
        sprintf (dbpath, "%s/%s", ctx->path, uuidstr);
        if (mkdir (dbpath, 0750) != 0) {
            free (dbpath);
            return NULL;
        }
        sprintf (dbpath, "%s/%s/%u.idx", ctx->path, uuidstr, dt);
        res = fopen (dbpath, "a+");
    }
    free (dbpath);
    return res;
}

FILE *localdb_open_current (localdb *ctx, uuid hostid, int flags) {
    char uuidstr[40];
    struct stat st;
    char *dbpath = (char *) malloc (strlen (ctx->path) + 64);
    if (! dbpath) return NULL;
    
    uuid2str (hostid, uuidstr);
    sprintf (dbpath, "%s/%s/current.db.new", ctx->path, uuidstr);
    if (flags & LOCALDB_FLAG_NOCREATE) {
        if (stat (dbpath, &st) != 0) {
            free (dbpath);
            return NULL;
        }
    }
    
    FILE *res = fopen (dbpath, "w");
    if (! res) {
        sprintf (dbpath, "%s/%s", ctx->path, uuidstr);
        if (mkdir (dbpath, 0750) != 0) {
            free (dbpath);
            return NULL;
        }
        sprintf (dbpath, "%s/%s/current.db.new", ctx->path, uuidstr);
        res = fopen (dbpath, "w");
    }
    free (dbpath);
    return res;
}

void localdb_done_current (localdb *ctx, uuid hostid) {
    char uuidstr[40];
    struct stat st;
    char *newpath = (char *) malloc (strlen (ctx->path) + 64);
    char *oldpath = (char *) malloc (strlen (ctx->path) + 64);
    
    if (! newpath) return;
    if (! oldpath) { free (newpath); return; }
    
    uuid2str (hostid, uuidstr);
    sprintf (oldpath, "%s/%s/current.db.new", ctx->path, uuidstr);
    sprintf (newpath, "%s/%s/current.db", ctx->path, uuidstr);
    rename (oldpath, newpath);
    free (newpath);
    free (oldpath);
}

/** Utility function for reading an encoded 64 bits integer out
  * of a FILE stream. This because setting up an iobuffer for
  * just this task stinks for the index.
  */
uint64_t localdb_read64 (FILE *fix) {
    uint64_t dt, res;
    if (fread (&dt, sizeof (res), 1, fix) == 0) {
        return 0;
    }
    res = ((uint64_t) ntohl (dt & 0xffffffffLLU)) << 32;
    res |= ntohl ((dt & 0xffffffff00000000LLU) >> 32);
    return res;
}

/** Get the offset for the closest matching record to a given timestamp
  * out of an indexfile.
  * \param fix The indexfile
  * \param ts The desired timestamp.
  * \return Offset inside the dbfile of the wanted record, or
  *         LOCALDB_OFFS_INVALID if something went wrong.
  */
uint64_t localdb_find_index (FILE *fix, time_t ts) {
    uint64_t first_when;
    uint64_t last_when;
    if (fseek (fix, 0, SEEK_SET) != 0) {
        return LOCALDB_OFFS_INVALID;
    }
    first_when = localdb_read64 (fix);
    if (fseek (fix, -(2*sizeof(uint64_t)), SEEK_END) != 0) {
        return LOCALDB_OFFS_INVALID;
    }
    uint64_t count = (ftell (fix) / (2*sizeof(uint64_t)))+1;
    if (count < 2) return 0;
    last_when = localdb_read64 (fix);
    if (first_when > ts) return LOCALDB_OFFS_INVALID;
    if (last_when < ts) {
        if (ts-last_when > 90) return LOCALDB_OFFS_INVALID;
        ts = last_when;
    }
    uint64_t range = last_when - first_when;
    uint64_t diff = ts - first_when;
    uint64_t pos = (count * diff) / range;
    if (pos) pos--;
    
    fseek (fix, pos * (2*sizeof(uint64_t)), SEEK_SET);
    
    uint64_t tsatpos = localdb_read64 (fix);
    uint64_t lastmatch;
    uint64_t tmp;
    uint64_t newts;

    if (tsatpos <= ts) {
        while (tsatpos <= ts) {
            tmp = localdb_read64 (fix);
            newts = localdb_read64 (fix);
            if (newts) {
                lastmatch = tmp;
                tsatpos = newts;
            }
            else {
                if (tsatpos == ts) {
                    return tmp;
                }
                return lastmatch;
            }
        }
        return lastmatch;
    }
    lastmatch = localdb_read64 (fix);
    while (tsatpos > ts) {
        if (fseek (fix, -(4*sizeof(uint64_t)), SEEK_CUR) != 0) {
            return lastmatch;
        }
        tsatpos = localdb_read64 (fix);
        lastmatch = localdb_read64 (fix);
    }
    return lastmatch;
}

/** Implementation for db_get_record() */
int localdb_get_record (db *d, time_t when, host *into) {
    localdb *self = (localdb *) d;
    datestamp dt = time2date (when);
    FILE *dbf = localdb_open_dbfile (self, into->uuid, dt,
                                     LOCALDB_FLAG_NOCREATE);
    if (! dbf) return 0;
    flock (fileno (dbf), LOCK_SH);
    FILE *ixf = localdb_open_indexfile (self, into->uuid, dt,
                                        LOCALDB_FLAG_NOCREATE);
    if (! ixf) {
        fclose (dbf);
        flock (fileno (dbf), LOCK_UN);
        return 0;
    }
    
    uint64_t offs = localdb_find_index (ixf, when);
    if (offs == LOCALDB_OFFS_INVALID) {
        flock (fileno (dbf), LOCK_UN);
        fclose (dbf);
        fclose (ixf);
        return 0;
    }
    if (fseek (dbf, offs, SEEK_SET) != 0) {
        flock (fileno (dbf), LOCK_UN);
        fclose (dbf);
        fclose (ixf);
        return 0;
    }
    ioport *dbport = ioport_create_filereader (dbf);
    uint64_t pad = ioport_read_u64 (dbport);
    if (pad != 0) {
        flock (fileno (dbf), LOCK_UN);
        fclose (dbf);
        fclose (ixf);
        ioport_close (dbport);
        return 0;
    }
    (void) ioport_read_u64 (dbport);
    int res = codec_decode_host (self->codec, dbport, into);
    flock (fileno (dbf), LOCK_UN);
    fclose (dbf);
    fclose (ixf);
    ioport_close (dbport);
    return res;
}

/** Implementation for db_get_value_range_int() */
uint64_t *localdb_get_value_range_int (db *d, time_t start, time_t end,
                                       int numsamples, meterid_t key,
                                       uint8_t arrayindex, host *h) {
    if (end <= start) return NULL;
    if (numsamples <2) return NULL;
    if (numsamples > 1024) return NULL;
    uint64_t *res = (uint64_t *) malloc ((numsamples+1) * sizeof (uint64_t));
    uint64_t interval = end-start;
    meter *m = host_get_meter (h, key);
    meter_setcount (m, 0);
    int respos = 0;
    time_t pos = start;
    int skip = interval / (numsamples-1);
    while (pos < end) {
        if (! localdb_get_record (d, pos, h)) {
            res[respos++] = 0;
        }
        else {
            res[respos++] = meter_get_uint (m, arrayindex);
        }
        pos += skip;
    }
    localdb_get_record (d, end, h);
    res[respos++] = meter_get_uint (m, arrayindex);
    return res;
}

/** Implementation for db_get_value_range_frac() */
double *localdb_get_value_range_frac (db *d, time_t start, time_t end,
                                      int numsamples, meterid_t key,
                                      uint8_t arrayindex, host *h) {
    if (end <= start) return NULL;
    if (numsamples < 2) return NULL;
    if (numsamples > 1024) return NULL;
    double *res = (double *) malloc ((numsamples+1) * sizeof (double));
    uint64_t interval = end-start;
    meter *m = host_get_meter (h, key);
    meter_setcount (m, 0);
    int respos = 0;
    time_t pos = start;
    int skip = interval / (numsamples-1);
    while (pos < end) {
        if (! localdb_get_record (d, pos, h)) {
            res[respos++] = 0.0;
        }
        else {
            res[respos++] = meter_get_frac (m, arrayindex);
        }
        pos += skip;
    }
    localdb_get_record (d, end, h);
    res[respos++] = meter_get_frac (m, arrayindex);
    return res;
}

/** Implementation for db_save_record() */
int localdb_save_record (db *dbctx, time_t when, host *h) {
    localdb *self = (localdb *) dbctx;
    datestamp dt = time2date (when);
    off_t dbpos = 0;
    
    FILE *curf = localdb_open_current (self, h->uuid, 0);
    if (! curf) return 0;

    FILE *dbf = localdb_open_dbfile (self, h->uuid, dt, 0);
    if (! dbf) {
        fclose (curf);
        return 0;
    }
    flock (fileno (dbf), LOCK_EX);
    
    FILE *ixf = localdb_open_indexfile (self, h->uuid, dt, 0);
    if (! ixf) {
        flock (fileno (dbf), LOCK_UN);
        fclose (curf);
        fclose (dbf);
        return 0;
    }
    
    ioport *dbport = ioport_create_filewriter (dbf);
    ioport *ixport = ioport_create_dualfilewriter (ixf, curf);
    
    fseek (dbf, 0, SEEK_END);
    fseek (ixf, 0, SEEK_END);
    dbpos = ftell (dbf);
    
    ioport_write_u64 (dbport, 0);
    ioport_write_u64 (dbport, when);
    codec_encode_host (self->codec, dbport, h);
    
    ioport_write_u64 (ixport, when);
    ioport_write_u64 (ixport, dbpos);
    ioport_close (dbport);
    ioport_close (ixport);
    flock (fileno (dbf), LOCK_UN);
    fclose (dbf);
    fclose (ixf);
    fclose (curf);
    localdb_done_current (self, h->uuid);
    return 1;
}

/** Implementation for db_delete_host_date() */
void localdb_delete_host_date (db *dbctx, uuid hostid, time_t ti) {
    datestamp dt = time2date (ti);
    localdb *self = (localdb *) dbctx;
    char uuidstr[40];
    char *dbpath = (char *) malloc (strlen (self->path) + 128);
    if (! dbpath) return;
    
    uuid2str (hostid, uuidstr);
    log_debug ("delete_host_date: %s/%i", uuidstr, dt); 

    sprintf (dbpath, "%s%s/%i.db", self->path, uuidstr, dt);
    unlink (dbpath);
    sprintf (dbpath, "%s%s/%i.idx", self->path, uuidstr, dt);
    unlink (dbpath);
    return;
}

/** Implementation for db_get_usage() */
int localdb_get_usage (db *dbctx, usage_info *into, uuid hostid) {
    localdb *self = (localdb *) dbctx;
    char uuidstr[40];
    struct dirent *dir;
    struct stat st;
    char *filename;
    uint64_t szsum = 0;
    datestamp earliest = 0;
    datestamp latest = 0;
    time_t t_earliest = 0;
    time_t t_latest = 0;
    datestamp dt;
    DIR *D;
    char *dbpath = (char *) malloc (strlen (self->path) + 64);
    if (! dbpath) return 0;

    uuid2str (hostid, uuidstr);
    sprintf (dbpath, "%s%s", self->path, uuidstr);
    
    if (! (D = opendir (dbpath))) { free (dbpath); return 0; }
    
    while ((dir = readdir (D))) {
        int nlen = strlen (dir->d_name);
        if (nlen < 5) continue;
        if (strcmp (dir->d_name + (nlen-3), ".db") == 0) {
            dt = atoi (dir->d_name);
            if (! dt) continue;
            if (! earliest) {
                earliest = latest = dt;
            }
            else {
                if (dt < earliest) earliest = dt;
                if (dt > latest) latest = dt;
            }
            filename = (char *) malloc (strlen (dbpath) + strlen (dir->d_name) + 2);
            sprintf (filename, "%s/%s", dbpath, dir->d_name);
            if (stat (filename, &st) == 0) {
                szsum += st.st_size;
            }
            free (filename);
        }
    }
    closedir (D);
    if (!earliest) {
        free (dbpath);
        return 0;
    }
    
    FILE *F = localdb_open_indexfile(self, hostid, earliest, 0);
    if (F) {
        fseek (F, 0, SEEK_SET);
        uint64_t bs = localdb_read64 (F);
        t_earliest = bs & 0x00000000ffffffff;
        fclose (F);
        F = localdb_open_indexfile(self, hostid, latest, 0);
        if (fseek (F, -(2*sizeof(uint64_t)), SEEK_END) == 0) {
            t_latest = localdb_read64 (F)  & 0x00000000ffffffff;
        }
        fclose (F);
    }
    
    if (t_earliest && t_latest) {
        into->bytes = szsum;
        into->earliest = t_earliest;
        into->last = t_latest;
        free (dbpath);
        return 1;
    }
    free (dbpath);
    return 0;
}

/** Implementation for db_close() */
void localdb_close (db *dbctx) {
    localdb *self = (localdb *) dbctx;
    free (self->path);
    self->path = NULL;
}

/** Implementation for db_free() */
void localdb_free (db *dbctx) {
    localdb *self = (localdb *) dbctx;
    codec_release (self->codec);
}

/** Recurse over a directory structure fishing for uuids */
void recurse_subs (const char *path, int depthleft,
                   int *alloc, int *outsz, uuid **res) {
    char *newpath;
    struct dirent *dir;
    struct stat st;
    DIR *D = opendir (path);
    if (! D) return;
    
    while ((dir = readdir (D))) {
        if (strcmp (dir->d_name, ".") == 0) continue;
        if (strcmp (dir->d_name, "..") == 0) continue;
        newpath = (char *) malloc (strlen(path) + strlen(dir->d_name)+4);
        sprintf (newpath, "%s/%s", path, dir->d_name);
        if (stat (newpath, &st) == 0) {
            if ((st.st_mode & S_IFMT) == S_IFDIR) {
                if (depthleft == 0) {
                    uuid u = mkuuid (dir->d_name);
                    if (u.msb == 0 && u.lsb == 0) continue;
                    if (*outsz == *alloc) {
                        *alloc *= 2;
                        (*res) = (uuid *)
                            realloc (*res, *alloc * sizeof(uuid));
                    }
                    (*res)[*outsz] = u;
                    (*outsz)++;
                }
                else {
                    recurse_subs (newpath, depthleft-1, alloc, outsz, res);
                }
            }
        }
        free (newpath);
    }
    
    closedir (D);
}

/** Implementation for db_list_tenants() */
uuid *localdb_list_tenants (db *d, int *outsz) {
    localdb *self = (localdb *) d;
    int alloc = 4;
    *outsz = 0;
    uuid *res = (uuid *) malloc (4 * sizeof (uuid));
    recurse_subs (self->pathprefix, 2, &alloc, outsz, &res);
    return res;
}


/** Implementation for db_create_tenant(). Creates the
  * necessary directory structures.
  */
int localdb_create_tenant (db *d, uuid tenant, var *meta) {
    localdb *self = (localdb *) d;
    char *insertpos = NULL;
    struct stat st;
    char uuidstr[40];
    int prefixlen = strlen (self->pathprefix);
    uuid2str (tenant, uuidstr);
    char *tmppath = (char *) malloc (prefixlen + 64);
    strcpy (tmppath, self->pathprefix);
    insertpos = tmppath + prefixlen;
    if (*(insertpos-1) != '/') {
        insertpos[0] = '/';
        insertpos++;
    }
    sprintf (insertpos, "%c%c", uuidstr[0], uuidstr[1]);
    insertpos += 2;
    if (stat (tmppath, &st) != 0) {
        if (mkdir (tmppath, 0750) != 0) {
            log_error ("(db_local) Error creating %s: %s", tmppath, strerror(errno));
            return 0;
        }
    }
    sprintf (insertpos, "/%c%c", uuidstr[2], uuidstr[3]);
    insertpos += 3;
    if (stat (tmppath, &st) != 0) {
        if (mkdir (tmppath, 0750) != 0) {
            log_error ("(db_local) Error creating %s", tmppath);
            return 0;
        }
    }
    sprintf (insertpos, "/%s", uuidstr);
    if (stat (tmppath, &st) != 0) {
        if (mkdir (tmppath, 0750) != 0) {
            log_error ("(db_local) Error creating %s", tmppath);
            return 0;
        }
    }
    
    if (! meta) {
        free (tmppath);
        return 1;
    }
    
    strcat (tmppath, "/");

    char *metapath = (char *) malloc (strlen (tmppath) + 17);
    char *mtmppath = (char *) malloc (strlen (tmppath) + 24);
    strcpy (metapath, tmppath);
    strcpy (mtmppath, tmppath);
    strcat (metapath, "tenant.metadata");
    strcat (mtmppath, ".tenant.metadata.new");
    FILE *F = fopen (mtmppath, "w");
    if (F) {
        int res = var_dump (meta, F);
        fclose (F);
        if (res) {
            if (rename (mtmppath, metapath) != 0) res = 0;
        }
        if (! res) unlink (mtmppath);
    }
    free (metapath);
    free (mtmppath);
    free (tmppath);
    return 1;
}

/** Utility function for cleaning out disk directories (rm -rf).
  * \param path The directory to remove recursively.
  * \return 1 on success, 0 on failure.
  */
int localdb_remove_dir (const char *path) {
    char *newpath;
    struct dirent *dir;
    struct stat st;
    
    DIR *D = opendir (path);
    if (! D) return 1;
    
    log_debug ("rmdir: %s", path);
    
    while ((dir = readdir (D))) {
        if (strcmp (dir->d_name, ".") == 0) continue;
        if (strcmp (dir->d_name, "..") == 0) continue;
        newpath = (char *) malloc (strlen(path) + strlen(dir->d_name)+4);
        sprintf (newpath, "%s/%s", path, dir->d_name);
        if (stat (newpath, &st) == 0) {
            if ((st.st_mode & S_IFMT) == S_IFDIR) {
                if (! localdb_remove_dir (newpath)) {
                    closedir (D);
                    free (newpath);
                    return 0;
                }
            }
            else {
                if (unlink (newpath) != 0) {
                    closedir (D);
                    free (newpath);
                    return 0;
                }
            }
        }
        free (newpath);
    }
    
    closedir (D);
    if (rmdir (path) != 0) {
        return 0;
    }
    return 1;
}

/** Implementation for db_remove_tenant() */
int localdb_remove_tenant (db *d, uuid tenantid) {
    localdb *self = (localdb *) d;
    char *insertpos = NULL;
    struct stat st;
    char uuidstr[40];
    int prefixlen = strlen (self->pathprefix);
    uuid2str (tenantid, uuidstr);
    char *tmppath = (char *) malloc (prefixlen + 64);
    strcpy (tmppath, self->pathprefix);
    insertpos = tmppath + prefixlen;
    if (*(insertpos-1) != '/') {
        insertpos[0] = '/';
        insertpos++;
    }
    sprintf (insertpos, "%c%c", uuidstr[0], uuidstr[1]);
    insertpos += 2;
    if (stat (tmppath, &st) != 0) return 1;
    sprintf (insertpos, "/%c%c", uuidstr[2], uuidstr[3]);
    insertpos += 3;
    if (stat (tmppath, &st) != 0) return 1;
    sprintf (insertpos, "/%s", uuidstr);
    if (stat (tmppath, &st) != 0)  return 1;
    if (! localdb_remove_dir (tmppath)) {
        free (tmppath);
        return 0;
    }
    free (tmppath);
    return 1;
}

/** Implementation for db_remove_host() */
int localdb_remove_host (db *d, uuid hostid) {
    localdb *self = (localdb *) d;
    char uuidstr[40];
    char *tmpstr = (char *) malloc (strlen (self->path) + 128);
    strcpy (tmpstr, self->path);
    uuid2str (hostid, uuidstr);
    strcat (tmpstr, uuidstr);
    if (! localdb_remove_dir (tmpstr)) return 0;
    strcat (tmpstr, ".metadata");
    unlink (tmpstr);
    free (tmpstr);
    return 1;
}

/** Implementation for db_list_hosts() */
uuid *localdb_list_hosts (db *d, int *outsz) {
    localdb *self = (localdb *) d;
    uuid *res = (uuid *) malloc (4 * sizeof (uuid));
    int alloc = 4;
    *outsz = 0;
    char *tmpstr;
    uuid u;
    
    DIR *D = opendir (self->path);
    struct dirent *dir;
    struct stat st;
    
    if (! D) return res;

    while ((dir = readdir (D))) {
        tmpstr = (char *) malloc (strlen(self->path)+strlen(dir->d_name)+4);
        strcpy (tmpstr, self->path);
        strcat (tmpstr, dir->d_name);
        if (stat (tmpstr, &st) == 0) {
            if ((st.st_mode & S_IFMT) == S_IFDIR) {
                u = mkuuid (dir->d_name);
                if (u.msb || u.lsb) {
                    res[*outsz] = u;
                    *outsz = (*outsz) +1;
                    if (*outsz == alloc) {
                        alloc *= 2;
                        res = (uuid *) realloc (res, alloc*sizeof(uuid));
                    }
                }
            }
        }
        free (tmpstr);
    }
    closedir (D);
    return res;
}

/** Implementation for db_get_tenantdata */
var *localdb_get_tenantdata (db *d, const char *suffx) {
    localdb *self = (localdb *) d;
    struct stat st;
    FILE *F;
    
    char *metapath = (char *) malloc (strlen (self->path) + 16);
    strcpy (metapath, self->path);
    strcat (metapath, "tenant.");
    strcat (metapath, suffx);
    if (stat (metapath, &st) != 0) {
        free (metapath);
        return NULL;
    }
    F = fopen (metapath, "r");
    if (! F) {
        free (metapath);
        return NULL;
    }
    flock (fileno (F), LOCK_SH);
    char *data = (char *) malloc (st.st_size + 16);
    fread (data, st.st_size, 1, F);
    data[st.st_size] = 0;
    flock (fileno (F), LOCK_UN);
    fclose (F);
    var *res = var_alloc();
    if (! var_parse_json (res, data)) {
        log_error ("Parse error: %s\n", parse_error());
        var_free (res);
        res = NULL;
    }
    free (metapath);
    free (data);
    return res;
}

/** Implementation for db_get_metadata() */
var *localdb_get_metadata (db *d) {
    return localdb_get_tenantdata (d, "metadata");
}

/** Implementation for db_get_summary() */
var *localdb_get_summary (db *d) {
    return localdb_get_tenantdata (d, "summary");
}

/** Implementation for db_get_overview() */
var *localdb_get_overview (db *d) {
    return localdb_get_tenantdata (d, "overview");
}

/** Generic function for storing tenant-related data */
int localdb_store_tenantdata (db *d, var *v, const char *suffx) {
    localdb *self = (localdb *) d;
    int res = 0;
    FILE *F;
    char *metapath = (char *) malloc (strlen (self->path) + 64);
    char *tmppath = (char *) malloc (strlen (self->path) + 96);
    strcpy (metapath, self->path);
    strcpy (tmppath, self->path);
    strcat (metapath, "tenant.");
    strcat (metapath, suffx);
    strcat (tmppath, ".tenant.");
    strcat (tmppath, suffx);
    strcat (tmppath, ".new");
    F = fopen (tmppath, "w");
    if (F) {
        res = var_dump (v, F);
        fclose (F);
        if (res) {
            F = fopen (metapath, "r");
            if (F) flock (fileno (F), LOCK_EX);
            if (rename (tmppath, metapath) != 0) res = 0;
            if (F) {
                flock (fileno (F), LOCK_UN);
                fclose (F);
            }
        }
        if (! res) unlink (tmppath);
    }
    free (metapath);
    free (tmppath);
    return res;
}

/** Implementation for db_set_metadata() */
int localdb_set_metadata (db *d, var *v) {
    return localdb_store_tenantdata (d, v, "metadata");
}

/** Implementation for db_set_summary() */
int localdb_set_summary (db *d, var *v) {
    return localdb_store_tenantdata (d, v, "summary");
}

/** Implementation for db_set_overview() */
int localdb_set_overview (db *d, var *v) {
    return localdb_store_tenantdata (d, v, "overview");
}

/** Implementation for db_get_hostmeta() */
var *localdb_get_hostmeta (db *d, uuid hostid) {
    localdb *self = (localdb *) d;
    char uuidstr[40];
    struct stat st;
    FILE *F;
    
    uuid2str (hostid, uuidstr);
    char *metapath = (char *) malloc (strlen (self->path) + 64);
    sprintf (metapath, "%s%s/metadata.json", self->path, uuidstr);
    if (stat (metapath, &st) != 0) {
        free (metapath);
        return var_alloc();
    }
    F = fopen (metapath, "r");
    if (! F) {
        log_debug ("Could open hostmeta: %s\n", metapath);
        free (metapath);
        return var_alloc();
    }
    flock (fileno (F), LOCK_SH);
    char *data = (char *) malloc (st.st_size + 16);
    fread (data, st.st_size, 1, F);
    data[st.st_size] = 0;
    flock (fileno (F), LOCK_UN);
    fclose (F);
    var *res = var_alloc();
    if (! var_parse_json (res, data)) {
        log_error ("Parse error: %s\n", parse_error());
        var_free (res);
        res = var_alloc();
    }
    free (metapath);
    free (data);
    return res;
}

/** Implementation for db_get_hostmeta_changed() */
time_t localdb_get_hostmeta_changed (db *d, uuid hostid) {
    struct stat st;
    localdb *self = (localdb *) d;
    char uuidstr[40];
    uuid2str (hostid, uuidstr);
    char *metapath = (char *) malloc (strlen (self->path) + 64);
    sprintf (metapath, "%s%s/metadata.json", self->path, uuidstr);
    if (stat (metapath, &st) != 0) return 0;
    free (metapath);
    return st.st_mtime;
}

/** Implementation for db_set_metadata() */
int localdb_set_hostmeta (db *d, uuid hostid, var *v) {
    localdb *self = (localdb *) d;
    int res = 0;
    char uuidstr[40];
    FILE *F;
    uuid2str (hostid, uuidstr);

    char *metapath = (char *) malloc (strlen (self->path) + 64);
    char *tmppath = (char *) malloc (strlen (self->path) + 64);
    sprintf (metapath, "%s%s/metadata.json", self->path, uuidstr);
    sprintf (tmppath, "%s%s/metadata.json.new", self->path, uuidstr);
    F = fopen (tmppath, "w");
    if (F) {
        res = var_dump (v, F);
        fclose (F);
        if (res) {
            F = fopen (metapath, "r");
            if (F) flock (fileno (F), LOCK_EX);
            if (rename (tmppath, metapath) != 0) {
                res = 0;
            }
            if (F) {
                flock (fileno (F), LOCK_UN);
                fclose (F);
            }
        }
        if (! res) unlink (tmppath);
    }
    free (metapath);
    free (tmppath);
    return res;
}

/** Converts a timestamp to an offset in a graph file. A graph file is always
    30 days worth of 5 minute samples. */
uint32_t localdb_graph_offset (time_t ti) {
    uint32_t clipped = (uint32_t) (ti % (30 * 86400));
    return (clipped / 300);
}

/** Calculate next offset in graph data */
uint32_t localdb_offset_next (uint32_t i) {
    return ((i+1) % GRAPHDATASZ);
}

/** Maps a host's logfile into memory, returning a handle */
hostloghandle *localdb_open_log (localdb *self, uuid hostid) {
    struct stat st;
    int initialize=0;
    char uuidstr[40];
    char *logpath = (char *) malloc (strlen(self->path)+64);
    
    hostloghandle *res = malloc (sizeof (hostloghandle));
    res->fd = -1;
    res->data = NULL;
    
    uuid2str (hostid, uuidstr);
    sprintf (logpath, "%s%s/host.log", self->path, uuidstr);
    if (stat (logpath, &st) != 0) initialize=1;
    res->fd = open (logpath, O_RDWR | O_CREAT);
    
    if (res->fd < 0) return res;
    
    if (initialize) {
        fchmod (res->fd, 0660);
        const char *blockdata = "\0\0\0\0\0\0\0\0";
        log_debug ("log: create %s", logpath);
        for (uint32_t i=0; i<sizeof(hostlogdata); i+=8) {
            write (res->fd, blockdata, 8);
        }
    }
    
    free (logpath);

    res->data = mmap (0, sizeof (hostlogdata), PROT_READ|PROT_WRITE,
                      MAP_SHARED, res->fd, 0);
    if (res->data == MAP_FAILED) {
        log_error ("mmap error: %s", strerror(errno));
        res->data = NULL;
        close (res->fd);
        res->fd = -1;
    }
    
    return res;
}

/** Unmaps a previously opened log */
void localdb_close_log (hostloghandle *hdl) {
    if (hdl->data) munmap (hdl->data, sizeof (hostlogdata));
    if (hdl->fd >= 0) close (hdl->fd);
    free (hdl);
}

/** Implementation for db_write_log() */
void localdb_write_log (db *d, uuid hostid, const char *subsystem,
                        const char *msg) {
    localdb *self = (localdb *) d;
    hostloghandle *log;
    char message[240];
    strncpy (message, msg, 240);
    message[239] = 0;
    
    log = localdb_open_log (self, hostid);
    if (log) {
        if (log->data) {
            uint32_t wpos = log->data->writepos;
            log->data->writepos = (wpos+1) % 63;
            log->data->rec[wpos].when = time (NULL);
            memcpy (log->data->rec[wpos].message, message, 240);
            strncpy (log->data->rec[wpos].subsystem, subsystem, 11);
        }
        localdb_close_log (log);
    }
}

/** Implementation for db_get_log() */
var *localdb_get_log (db *d, uuid hostid) {
    localdb *self = (localdb *) d;
    hostloghandle *log = localdb_open_log (self, hostid);
    var *res = var_alloc();
    if (log && log->data) {
        uint32_t rpos = (log->data->writepos-1) % 63;
        while (rpos != log->data->writepos) {        
            hostlogrecord *irec = &log->data->rec[rpos];
            if (irec->when) {
                var *orec = var_add_dict (res);
                var_set_unixtime_forkey (orec, "when", irec->when);
                var_set_str_forkey (orec, "subsystem", irec->subsystem);
                var_set_str_forkey (orec, "message", irec->message);
            }
            rpos = (rpos-1) % 63;
        }
    }
    if (log) localdb_close_log (log);
    return res;
}

/** Utility function that opens and mmaps a graph file. */
graphdata *localdb_open_graph (localdb *self, uuid hostid, const char *id,
                               const char *v) {
    graphdata *res = NULL;
    int initialize=0;
    struct stat st;
    char uuidstr[40];
    uuid2str (hostid, uuidstr);
    size_t keylen = strlen (id) + strlen (v) + 2;
    char *graphpath = (char *) malloc (strlen(self->path)+40+keylen+8);
    
    sprintf (graphpath, "%s%s/graph.%s.%s.dat", self->path, uuidstr, id, v);
    if (stat (graphpath, &st) != 0) initialize=1;
    
    if (initialize) log_debug ("graph: create %s", graphpath);
    
    self->graphfd = open (graphpath, O_RDWR | O_CREAT);
    if (initialize) fchmod (self->graphfd, 0660);
    
    free (graphpath);
    if (self->graphfd < 0) return NULL;
    
    if (initialize) {
        const char *blockdata = "\0\0\0\0\0\0\0\0";
        for (uint32_t i=0; i<sizeof(graphdata); i+=8) {
            write (self->graphfd, blockdata, 8);
        }
    }
    
    res = mmap (0, sizeof (graphdata), PROT_READ|PROT_WRITE,
                MAP_SHARED, self->graphfd, 0);
    if (res == MAP_FAILED) {
        log_error ("mmap error: %s", strerror(errno));
        return NULL;
    }
                
    if (initialize) {
        res->writepos = localdb_graph_offset (time(NULL));
        res->accumulator = 0.0;
        res->count = 0;
        for (int i=0; i<GRAPHDATASZ; ++i) res->data[i] = 0.0;
    }
    
    return res;
}

/** Utility function that closes a previously opened graph file. */
void localdb_close_graph (localdb *self, graphdata *data) {
    munmap (data, sizeof(graphdata));
    close (self->graphfd);
    self->graphfd = -1;
}

/** Implementation for db_set_graph() */
int localdb_set_graph (db *d, uuid hostid, const char *id, const char *key,
                       double val) {
    localdb *self = (localdb *) d;
    graphdata *graph = localdb_open_graph (self, hostid, id, key);
    if (! graph) return 0;
    
    uint32_t offnow = localdb_graph_offset (time(NULL));
    if (offnow != graph->writepos) {
        graph->writepos = offnow;
        graph->accumulator = 0.0;
        graph->count = 0;
    }
    
    graph->count++;
    graph->accumulator += val;
    
    graph->data[graph->writepos] = graph->accumulator / graph->count;
    localdb_close_graph (self, graph);
    return 1;
}

/** Implementation for db_get_graph() */
double *localdb_get_graph (db *d, uuid hostid, const char *id,
                           const char *key, time_t interval,
                           int numsamples) {
    localdb *self = (localdb *) d;
    if (! numsamples) return NULL;
                           
    time_t t_end = time (NULL);
    time_t t_start = t_end - interval;
    uint32_t offs_start = localdb_graph_offset (t_start);
    uint32_t offs_end = localdb_graph_offset (t_end);
    
    double samplesize = ((double)interval)/(numsamples*300.0);
    if (samplesize < 1.0) return NULL;
    
    graphdata *graph = localdb_open_graph (self, hostid, id, key);
    if (! graph) return NULL;

    int pos = 0;
    double *res = (double *) malloc ((numsamples+1) * sizeof (double));
    uint32_t crsr = offs_start;
    double offs_next = offs_start + samplesize;
    
    time_t t_now = t_start;
    
    while (pos < numsamples) {
        double ra = 0.0;
        double rc = 0.0;
        while (localdb_offset_next(crsr) != ((uint32_t)offs_next)%(30*86400)) {
            ra += graph->data[crsr];
            rc += 1.0;
            crsr = localdb_offset_next(crsr);
        }
        crsr = localdb_offset_next(crsr);
        double diff = offs_next - floor(offs_next);
        if (diff > 0.01) {
            ra += graph->data[crsr] * diff;
            rc += diff;
            res[pos] = ra / rc;
            rc = 1.0 - diff;
            ra = graph->data[crsr] * rc;
        }
        else {
            ra += graph->data[crsr];
            rc += 1;
            res[pos] = ra / rc;
            rc = 0.0;
            ra = 0.0;
        }
        offs_next += samplesize;
        pos++;
    }
    
    localdb_close_graph (self, graph);
    return res;
}

/** Implementation for db_set_global() */
int localdb_set_global (db *d, const char *id, var *v) {
    int res = 0;
    FILE *F;
    localdb *self = (localdb *) d;
    size_t sz = strlen (self->pathprefix) + strlen (id);
    char *metapath = (char *) malloc (sz+32);
    char *tmppath = (char *) malloc (sz + 64);
    sprintf (metapath, "%s/%s.json", self->pathprefix, id);
    sprintf (tmppath, "%s/%s.json.new", self->pathprefix, id);
    F = fopen (tmppath, "w");
    if (F) {
        res = var_dump (v, F);
        fclose (F);
        if (res) {
            F = fopen (metapath, "r");
            if (F) flock (fileno (F), LOCK_EX);
            if (rename (tmppath, metapath) != 0) {
                res = 0;
            }
            if (F) {
                flock (fileno (F), LOCK_UN);
                fclose (F);
            }
        }
        if (! res) unlink (tmppath);
    }
    free (metapath);
    free (tmppath);
    return res;
}

/** Implementation for db_get_global() */
var *localdb_get_global (db *d, const char *id) {
    localdb *self = (localdb *) d;
    struct stat st;
    FILE *F;
    
    size_t sz = strlen (self->pathprefix) + strlen (id);
    char *metapath = (char *) malloc (sz+16);
    sprintf (metapath, "%s/%s.json", self->pathprefix, id);
    if (stat (metapath, &st) != 0) {
        free (metapath);
        return NULL;
    }
    F = fopen (metapath, "r");
    if (! F) {
        log_debug ("Could open stat: %s\n", metapath);
        free (metapath);
        return NULL;
    }
    flock (fileno (F), LOCK_SH);
    char *data = (char *) malloc (st.st_size + 16);
    fread (data, st.st_size, 1, F);
    data[st.st_size] = 0;
    flock (fileno (F), LOCK_UN);
    fclose (F);
    var *res = var_alloc();
    if (! var_parse_json (res, data)) {
        log_error ("Parse error: %s\n", parse_error());
        var_free (res);
        res = NULL;
    }
    free (metapath);
    free (data);
    return res;
}

/** Allocate an unbound localdb object.
  * \param prefix The path prefix for local storage.
  * \return A database handle (or NULL).
  */
db *localdb_create (const char *prefix) {
    localdb *self = (localdb *) malloc (sizeof (localdb));
    if (!self) return NULL;
    
    self->db.open = localdb_open;
    self->db.get_record = localdb_get_record;
    self->db.get_value_range_int = localdb_get_value_range_int;
    self->db.get_value_range_frac = localdb_get_value_range_frac;
    self->db.save_record = localdb_save_record;
    self->db.delete_host_date = localdb_delete_host_date;
    self->db.get_hostmeta = localdb_get_hostmeta;
    self->db.get_hostmeta_changed = localdb_get_hostmeta_changed;
    self->db.set_hostmeta = localdb_set_hostmeta;
    self->db.get_usage = localdb_get_usage;
    self->db.list_hosts = localdb_list_hosts;
    self->db.get_metadata = localdb_get_metadata;
    self->db.set_metadata = localdb_set_metadata;
    self->db.get_summary = localdb_get_summary;
    self->db.set_summary = localdb_set_summary;
    self->db.get_overview = localdb_get_overview;
    self->db.set_overview = localdb_set_overview;
    self->db.set_graph = localdb_set_graph;
    self->db.get_graph = localdb_get_graph;
    self->db.write_log = localdb_write_log;
    self->db.get_log = localdb_get_log;
    self->db.remove_host = localdb_remove_host;
    self->db.close = localdb_close;
    self->db.list_tenants = localdb_list_tenants;
    self->db.create_tenant = localdb_create_tenant;
    self->db.remove_tenant = localdb_remove_tenant;
    self->db.get_global = localdb_get_global;
    self->db.set_global = localdb_set_global;
    self->db.free = localdb_free;
    self->db.opened = 0;
    self->db.tenant.lsb = 0;
    self->db.tenant.msb = 0;
    self->pathprefix = strdup (prefix);
    self->path = NULL;
    self->codec = codec_create_pkt();
    return (db *) self;
}
