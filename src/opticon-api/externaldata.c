#include "externaldata.h"
#include <libopticondb/db.h>
#include <libopticon/popen.h>
#include <libopticon/hash.h>
#include <libopticon/var_parse.h>
#include <libopticon/log.h>
#include <time.h>
#include <pthread.h>
#include "options.h"

typedef struct extdata_s {
    struct extdata_s    *prev;
    struct extdata_s    *next;
    uint32_t             hash;
    uuid                 hostid;
    uuid                 tenantid;
    time_t               lastrefresh;
    var                 *data;
} extdata;

typedef struct extdatalist_s {
    extdata         *first;
    extdata         *last;
    pthread_rwlock_t lock;
} extdatalist;

static extdatalist dlist;

/** Initialize the linked list an lock */
void extdata_init (void) {
    dlist.first = dlist.last = NULL;
    pthread_rwlock_init (&dlist.lock, NULL);
}

/** Find a cache entry */
extdata *extdata_find (uuid tenantid, uuid hostid) {
    uint32_t hash = hash_uuid (hostid);
    extdata *crsr = dlist.first;
    while (crsr) {
        if (crsr->hash == hash) {
            if (uuidcmp (tenantid, crsr->tenantid) &&
                uuidcmp (hostid, crsr->hostid)) break;
        }
        crsr = crsr->next;
    }
    return crsr;
}

/** Create and link a new cache entry */
extdata *extdata_new (uuid tenantid, uuid hostid) {
    extdata *res = malloc (sizeof (extdata));
    res->next = NULL;
    res->prev = dlist.last;
    if (dlist.last) {
        dlist.last->next = res;
        dlist.last = res;
    }
    else {
        dlist.first = dlist.last = res;
    }
    res->hash = hash_uuid (hostid);
    res->tenantid = tenantid;
    res->hostid = hostid;
    res->data = NULL;
    return res;
}

/** Load ext data either from cache or through its command. */
var *extdata_get (uuid tenantid, uuid hostid, var *env) {
    const char *tool = OPTIONS.external_querytool;
    if (! tool) return NULL;

    char tenantstr[40];
    char hoststr[40];
    uuid2str (tenantid, tenantstr);
    uuid2str (hostid, hoststr);

    log_info ("[extdata] Getting external data for host <%s>", hoststr);
    
    pthread_rwlock_rdlock (&dlist.lock);

    extdata *crsr = extdata_find (tenantid, hostid);

    if (crsr) {
        time_t now = time (NULL);
        if (now - crsr->lastrefresh < 600) {
            var *res = var_clone (crsr->data);
            pthread_rwlock_unlock (&dlist.lock);
            log_info ("[extdata] Returning cached result");
            return res;
        }
    }
    
    pthread_rwlock_unlock (&dlist.lock);

    char cmd[1024];
    
    sprintf (cmd, "%s %s %s", tool, tenantstr, hoststr);
    var *vout = var_alloc();
    FILE *f = popen_safe_env (cmd, "r", env);
    if (f) {
        var_read_json (vout, f);
        pclose_safe (f);
    }
    
    log_info ("[extdata] Ran %s", tool);
    
    pthread_rwlock_wrlock (&dlist.lock);
    crsr = extdata_find (tenantid, hostid);
    if (crsr) {
        var_free (crsr->data);
        crsr->data = NULL;
    }
    else {
        crsr = extdata_new (tenantid, hostid);
    }
    crsr->data = vout;
    crsr->lastrefresh = time (NULL);
    var *res = var_clone (vout);
    pthread_rwlock_unlock (&dlist.lock);
    return res;
}
