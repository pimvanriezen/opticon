#include "opticon-collector.h"
#include "ping.h"
#include <libopticondb/db_local.h>
#include <libopticon/auth.h>
#include <libopticon/ioport.h>
#include <libopticon/pktwrap.h>
#include <libopticon/util.h>
#include <libopticon/var.h>
#include <libopticon/var_parse.h>
#include <libopticon/react.h>
#include <libopticon/daemon.h>
#include <libopticon/log.h>
#include <libopticon/cliopt.h>
#include <libopticon/transport_udp.h>
#include <libopticon/defaultmeters.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>

/*/ ======================================================================= /*/
/** Look up a session by netid and sessionid. If it's a valid session,
  * return its current AES session key.
  * \param netid The host's netid (32 bit ip-derived).
  * \param sid The session-id.
  * \param serial Serial-number of packet being handled.
  * \param blob Pointer for giving back the session.
  * \return The key, or NULL if resolving failed, or session was invalid. */
/*/ ======================================================================= /*/
aeskey *resolve_sessionkey (uint32_t netid, uint32_t sid, uint32_t serial,
                            void **blob) {
    session *S = session_find (netid, sid);
    if (! S) {
        log_warn ("Session <%08x-%08x> not found", sid, netid);
        return NULL;
    }
    if ((S->lastserial >= serial) &&
        ((S->lastserial < 0xfffffff8) || (serial>8))) {
        log_warn ("Rejecting old serial <%i> for session <%08x-%08x> which"
                  " is currently <%i> at the session-level",
                  serial, sid, netid, S->lastserial);
        return NULL;
    }
    if ((S->host->lastserial >= serial) &&
        ((S->host->lastserial < 0xfffffff8) || (serial>8))) {
        log_warn ("Rejecting old serial <%i> for session <%08x-%08x> which"
                  " is currently <%i> at the host-level",
                  serial, sid, netid, S->host->lastserial);
        return NULL;
    }
    S->host->lastserial = serial;
    *blob = (void *)S;
    return &S->key;
}

/*/ ======================================================================= /*/
/** Add a meterwatch to a (tenant's) watchlist, with a var object
  * as configuration; helper for watchlist_populate().
  * \param w The watchlist to add to.
  * \param id The meterid to match
  * \param mtp The metertype to match
  * \param v Metadata, dict with: cmp, weight, val.
  * \param vweight The unweighted 'badness' (depending on alert/warning). */
/*/ ======================================================================= /*/
void make_watcher (watchlist *w, meterid_t id, metertype_t mtp,
                   var *v, double vweight, watchtrigger tr) {
    const char *cmp = var_get_str_forkey (v, "cmp");
    double weight = var_get_double_forkey (v, "weight");
    uint64_t ival = var_get_int_forkey (v, "val");
    double dval = var_get_double_forkey (v, "val");
    const char *sval = var_get_str_forkey (v, "val");
    
    if (weight < 0.01) weight = 1.0;
    weight = weight * vweight;
    
    if (strcmp (cmp, "count") == 0) {
        watchlist_add_uint (w, id, WATCH_COUNT, ival, weight, tr);
    }
    else if (mtp == MTYPE_INT) {
        if (strcmp (cmp, "lt") == 0) {
            watchlist_add_uint (w, id, WATCH_UINT_LT, ival, weight, tr);
        }
        else if (strcmp (cmp, "gt") == 0) {
            watchlist_add_uint (w, id, WATCH_UINT_GT, ival, weight, tr);
        }
    }
    else if (mtp == MTYPE_FRAC) {
        if (strcmp (cmp, "lt") == 0) {
            watchlist_add_frac (w, id, WATCH_FRAC_LT, dval, weight, tr);
        }
        else if (strcmp (cmp, "gt") == 0) {
            watchlist_add_frac (w, id, WATCH_FRAC_GT, dval, weight, tr);
        }
    }
    else if (mtp == MTYPE_STR) {
        if (strcmp (cmp, "eq") == 0) {
            watchlist_add_str (w, id, WATCH_STR_MATCH, sval, weight, tr);
        }
    }
}

/*/ ======================================================================= /*/
/** Populate a watchlist out of a meter definition stored in some
  * variables.
  * \param w The watchlist to populate.
  * \param v_meters The meter definitions. */
/*/ ======================================================================= /*/
void watchlist_populate (watchlist *w, var *v_meters) {
    pthread_mutex_lock (&w->mutex);
    watchlist_clear (w);
    if (v_meters) {
        var *mdef = v_meters->value.arr.first;
        while (mdef) {
            var *v_warn = var_get_dict_forkey (mdef, "warning");
            var *v_alert = var_get_dict_forkey (mdef, "alert");
            var *v_crit = var_get_dict_forkey (mdef, "critical");
            const char *type = var_get_str_forkey (mdef, "type");
            if (! type) {
                var *v_fallback = var_get_dict_forkey (APP.watchdef, mdef->id);
                type = var_get_str_forkey (v_fallback, "type");
            }
            if (type && (v_warn || v_alert || v_crit)) {
                metertype_t tp;
                if (memcmp (type, "int", 3) == 0) tp = MTYPE_INT;
                else if (strcmp (type, "frac") == 0) tp = MTYPE_FRAC;
                else if (memcmp (type, "str", 3) == 0) tp = MTYPE_STR;
                else {
                    /* Skip table, complain about others */
                    if (strcmp (type, "table") != 0) {
                        log_error ("Meter configured with unrecognized "
                                   "typeid %s", type);
                    }
                    mdef = mdef->next;
                    continue;
                }
                
                meterid_t id = makeid (mdef->id, tp, 0);
                if (var_get_count (v_warn)) {
                     make_watcher (w, id, tp, v_warn, 5.0, WATCH_WARN);
                }
                if (var_get_count (v_alert)) {
                     make_watcher (w, id, tp, v_alert, 8.0, WATCH_ALERT);
                }
                if (var_get_count (v_crit)) {
                    make_watcher (w, id, tp, v_crit, 12.0, WATCH_CRIT);
                }
            }
            mdef = mdef->next;
        }
    }
    pthread_mutex_unlock (&w->mutex);
}

/*/ ======================================================================= /*/
/** Populate summaryinfo. Format:
  * [ { id: cpu, meter: pcpu, type: frac, func: avg } ] */
/*/ ======================================================================= /*/
void summaryinfo_populate (summaryinfo *into, var *v_summary) {
    pthread_mutex_lock (&into->mutex);
    summaryinfo_clear (into);
    var *crsr = v_summary->value.arr.first;
    while (crsr) {
        const char *s_id = crsr->id;
        const char *s_meterid = var_get_str_forkey (crsr, "meter");
        if (! s_meterid) break;
        const char *s_type = var_get_str_forkey (crsr, "type");
        if (! s_type) break;
        const char *s_func = var_get_str_forkey (crsr, "func");
        if (! s_func) break;
        const char *s_match = var_get_str_forkey (crsr, "match");
        
        meterid_t mid;
        metertype_t mtype;
        if (strncmp (s_type, "int", 3) == 0) mtype = MTYPE_INT;
        else if (strncmp (s_type, "frac", 4) == 0) mtype = MTYPE_FRAC;
        else if (strncmp (s_type, "str", 3) == 0) mtype = MTYPE_STR;
        else break;
        
        mid = makeid (s_meterid, mtype, 0);
        
        if (strcmp (s_func, "avg") == 0) {
            summaryinfo_add_summary_avg (into, s_id, mid);
        }
        else if (strcmp (s_func, "total") == 0) {
            summaryinfo_add_summary_total (into, s_id, mid);
        }
        else if (strcmp (s_func, "count") == 0) {
            if (! s_match) break;
            summaryinfo_add_summary_count (into, s_id, mid, s_match);
        }
        
        crsr = crsr->next;
    }
    pthread_mutex_unlock (&into->mutex);
}

/*/ ======================================================================= /*/
/** Look up a tenant in memory and in the database, do the necessary
  * bookkeeping, then returns the AES key for that tenant.
  * \param tenantid The tenant UUID.
  * \param serial Serial number of the auth packet we're dealing with.
  * \return Pointer to the key, or NULL if resolving failed. */
/*/ ======================================================================= /*/
aeskey *resolve_tenantkey (uuid tenantid, uint32_t serial) {
    tenant *T = NULL;
    
    var *meta;
    const char *b64;
    aeskey *res = NULL;
    
    /* Discard tenants we can't find in the database */
    if (! db_open (APP.db, tenantid, NULL)) {
        // FIXME remove tenant
        log_error ("(resolve_tenantkey) Packet for unknown tenantid");
        return NULL;
    }
    
    /* Discard tenants with no metadata */
    if (! (meta = db_get_metadata (APP.db))) {
        log_error ("(resolve_tenantkey) Error reading metadata");
        db_close (APP.db);
        return NULL;
    }
    
    T = tenant_find (tenantid, TENANT_LOCK_WRITE);
    
    /* Find the 'key' metavalue */
    if ((b64 = var_get_str_forkey (meta, "key"))) {
        if (T) {
            /* Update the existing tenant structure with information
               from the database */
            T->key = aeskey_from_base64 (b64);
            res = &T->key;
        }
        else {
            /* Create a new in-memory tenant */
            T = tenant_create (tenantid, aeskey_from_base64 (b64));
            if (T) {
                res = &T->key;
            }
        }
    }

    /* If there's meter data defined in the tenant metadata,
       put it in the tenant's watchlist */    
    var *v_meters = var_get_dict_forkey (meta, "meter");
    if (v_meters) watchlist_populate (&T->watch, v_meters);
    
    var *v_summary = var_get_dict_forkey (meta, "summary");
    if (! var_get_count (v_summary)) {
        var_copy (v_summary, var_get_dict_forkey (APP.conf, "summary"));
    }
    else {
        var *conf = var_get_dict_forkey (APP.conf, "summary");
        var *crsr = conf->value.arr.first;
        while (crsr) {
            var *summc = var_get_dict_forkey (v_summary, crsr->id);
            if (! var_get_count (summc)) var_copy (summc, crsr);
            crsr = crsr->next;
        }
    }
    summaryinfo_populate (&T->summ, v_summary);
    var_free (v_summary);
    db_close (APP.db);
    var_free (meta);
    tenant_done (T);
    return res;
}

/*/ ======================================================================= /*/
/** Handler for an auth packet */
/*/ ======================================================================= /*/
void handle_auth_packet (ioport *pktbuf, uint32_t netid,
                         struct sockaddr_storage *remote) {
    authinfo *auth = ioport_unwrap_authdata (pktbuf, resolve_tenantkey);
    
    char addrbuf[INET6_ADDRSTRLEN];
    ip2str (remote, addrbuf);
    
    /* No auth means discarded by the crypto/decoding layer */
    if (! auth) {
        log_warn ("Authentication failed on packet from <%s>", addrbuf);
        return;
    }
    
    char s_hostid[40];
    char s_tenantid[40];
    
    uuid2str (auth->hostid, s_hostid);
    uuid2str (auth->tenantid, s_tenantid);
    
    if (! uuidvalid (auth->hostid)) {
        log_warn ("Rejecting session <%08x-%08x> from <%s> for invalid "
                  "host <%s>", auth->sessionid, netid, addrbuf, s_hostid);
        free (auth);
        return;
    }
    
    time_t tnow = time (NULL);
    
    /* Figure out if we're renewing an existing session */
    session *S = session_find (netid, auth->sessionid);
    if (S) {
        /* Discard replays */
        if (S->lastserial < auth->serial) {
            log_debug ("Renewing session <%08x-%08x> from <%s> serial <%i> "
                       "tenant <%s> host <%s>", auth->sessionid, netid,
                       addrbuf, auth->serial, s_tenantid, s_hostid);
            S->key = auth->sessionkey;
            S->lastcycle = tnow;
            S->lastserial = auth->serial;
        }
        else {
            log_debug ("Rejecting Duplicate serial <%i> on auth from <%s>",
                        auth->serial, addrbuf);
            free (auth);
            return;
        }
    }
    else {
        log_info ("New session <%08x-%08x> from <%s> serial <%i> "
                  "tenant <%s> host <%s>", auth->sessionid, netid,
                  addrbuf, auth->serial, s_tenantid, s_hostid);
    
        S = session_register (auth->tenantid, auth->hostid, netid,
                              auth->sessionid, auth->sessionkey,
                              remote);
        if (! S) log_error ("Session is NULL");
    }
    
    if (S) {
        host *h = S->host;
        pthread_rwlock_wrlock (&h->lock);
        h->lastmodified = time (NULL);
        
        char addrbuf[64];
        ip2str (&S->remote, addrbuf);
        meterid_t mid_linkip = makeid ("link/ip", MTYPE_STR, 0);
        meter *m_linkip = host_get_meter (h, mid_linkip);
        meter_setcount (m_linkip, 0);
        meter_set_str (m_linkip, 0, addrbuf);
        pthread_rwlock_unlock (&h->lock);
    }
    
    /* Now's a good time to cut the dead wood */
    log_debug ("Expiring sessions");
    session_expire (tnow - 905);
    var *vlist = sessiondb_save();
    db_set_global (APP.db, "sessions", vlist);
    var_free (vlist);
    log_debug ("Saved sessions");
    free (auth);
}

/*/ ======================================================================= /*/
/** Goes over fresh host metadata to pick up any parts relevant
  * to the operation of opticon-collector. At this moment,
  * this is just the meterwatch adjustments.
  * \param H The host we're dealing with
  * \param meta The fresh metadata */
/*/ ======================================================================= /*/
void handle_host_metadata (host *H, var *meta) {
    /* layout of adjustment data:
       meter {
           pcpu {
               type: frac # int | frac | string
               warning { val: 30.0, weight: 1.0 }
               alert{ val: 50.0, weight: 1.0 }
           }
       }
       graph {
           pcpu {
               graph: cpu
               datum: usage
               title: "CPU Usage"
               unit: "%"
               color: "blue"
               max: 100.0
           }
       }
    */
    
    char *tstr;
    var *v_meter = var_get_dict_forkey (meta, "meter");
    int adjcount = var_get_count (v_meter);
    if (adjcount) {
        var *v_adjust = v_meter->value.arr.first;
        while (v_adjust) {
            const char *levels[3] = {"warning","alert","critical"};
            watchadjusttype atype = WATCHADJUST_NONE;
            const char *stype = var_get_str_forkey (v_adjust, "type");
            if (! stype) break;
            if (memcmp (stype, "int", 3) == 0) atype = WATCHADJUST_UINT;
            else if (strcmp (stype, "frac") == 0) atype = WATCHADJUST_FRAC;
            else if (memcmp (stype, "str", 3) == 0) atype = WATCHADJUST_STR;
            meterid_t mid_adjust = makeid (v_adjust->id, 0, 0);
            watchadjust *adj = adjustlist_get (&H->adjust, mid_adjust);
            adj->type = atype;
            for (int i=0; i<3; ++i) {
                var *v_level = var_find_key (v_adjust, levels[i]);
                if (! v_level) continue;
                if (v_level->type != VAR_DICT) continue;
                
                switch (atype) {
                    case WATCHADJUST_UINT:
                        /* +1 needed to skip WATCHADJUST_NONE */
                        adj->adjust[i+1].data.u64 =
                            var_get_int_forkey (v_level, "val");
                        break;
                    
                    case WATCHADJUST_FRAC:
                        adj->adjust[i+1].data.frac =
                            var_get_double_forkey (v_level, "val");
                        break;
                    
                    case WATCHADJUST_STR:
                        tstr = adj->adjust[i+1].data.str.str;
                        strncpy (tstr,
                                 var_get_str_forkey(v_level, "val"), 127);
                        tstr[127] = 0;
                        break;
                    
                    default:
                        adj->adjust[i+1].weight = 0.0;
                        break;
                }
                if (atype != WATCHADJUST_NONE) {
                    var *v_level = var_get_dict_forkey (v_adjust, levels[i]);
                    double v_weight = var_get_double_forkey (v_level, "weight");
                    if (v_weight < 0.0005) v_weight = 1.0;
                    adj->adjust[i+1].weight = v_weight;
                }
            }
            v_adjust = v_adjust->next;
        }
    }
    
    var *v_graph = var_get_dict_forkey (meta, "graph");
    int graphcount = var_get_count (v_graph);
    if (graphcount) {
        graphlist *oldlist = H->graphlist;
        graphlist *newlist = graphlist_create();
        graphlist_make (newlist, v_graph);
        
        H->graphlist = newlist;
        graphlist_free (oldlist);
    }
}

/*/ ======================================================================= /*/
/** Handler for a meter packet */
/*/ ======================================================================= /*/
void handle_meter_packet (ioport *pktbuf, uint32_t netid) {
    session *S = NULL;
    ioport *unwrap;
    time_t tnow = time (NULL);
    char uuidstr[40];
    
    /* Unwrap the outer packet layer (crypto and compression) */
    unwrap = ioport_unwrap_meterdata (netid, pktbuf,
                                      resolve_sessionkey, (void**) &S);
    if (! unwrap) {
        log_debug ("Error unwrapping packet from <%08x>: %08x", netid,
                   unwrap_errno);
        return;
    }
    if (! S) {
        log_debug ("Error unwrapping session from <%08x>", netid);
        ioport_close (unwrap);
        return;
    }
    
    /* Write the new meterdata into the host */
    host *H = S->host;

    int i = 0;    
    for (i=0; i<4; ++i) {
      if (pthread_rwlock_trywrlock (&H->lock) == 0) break;
      sleep (1);
    }
    if (i>3) {
        uuid2str (H->uuid, uuidstr);
        log_error ("Host deadlock for <%s>", uuidstr);
        ioport_close (unwrap);
        return;
    }
    
    /* Check if we need to sync up metadata */
    if (tnow - H->lastmetasync > 300) {
        if (! db_open (APP.db, S->tenantid, NULL)) {
            log_error ("Could not load tenant for already open session");
        }
        else {
            time_t changed = db_get_hostmeta_changed (APP.db, S->hostid);
            if (changed > H->lastmetasync) {
                log_debug ("Reloading metadata");
                var *meta = db_get_hostmeta (APP.db, S->hostid);
                handle_host_metadata (H, meta);
                var_free (meta);
            }
            db_close (APP.db);
        }
        H->lastmetasync = tnow;
    }
    
    host_begin_update (H, tnow);

    if (H->mcount == 0) {
        db *DB = localdb_create (APP.dbpath);
        if (db_open (DB, S->tenantid, NULL)) {
            if (db_host_exists (DB, H->uuid)) {
                db_get_current (DB, H);
            }
            db_close (DB);
        }
        db_free (DB);
    }

    if (APP.pktdump && APP.pktdump[0]) {
        size_t sz = ioport_read_available (unwrap);
        char path[512];
        sprintf (path, "%s/%08x-%08x.dump", APP.pktdump,
                 S->sessid, S->addr);
        FILE *fout = fopen (path, "w");
        if (fout) {
            fwrite (ioport_get_buffer (unwrap), sz, 1, fout);
            fclose (fout);
        }
    }
    
    if (codec_decode_host (APP.codec, unwrap, H)) {
        log_debug ("Update handled for session <%08x-%08x>",
                    S->sessid, S->addr);
    }
    
    meterid_t mid_rtt = makeid ("link/rtt",MTYPE_FRAC,0);
    meterid_t mid_loss = makeid ("link/loss",MTYPE_FRAC,0);
    meter *m_rtt = host_get_meter (H, mid_rtt);
    meter *m_loss = host_get_meter (H, mid_loss);
    
    host_end_update (H);
    pthread_rwlock_unlock (&H->lock);
    ioport_close (unwrap);
}

/*/ ======================================================================= /*/
/** Dump all tenant-level watcher data to a file for temporary debugging
  * purposes. */
/*/ ======================================================================= /*/
void dump_tenant_debug (void) {
    if (! APP.dumppath) return;
    if (! APP.dumppath[0]) return;
    
    char tmpstr[48];
    FILE *f = fopen (APP.dumppath,"w");
    tenant *T = tenant_first (TENANT_LOCK_READ);
    while (T) {
        uuid2str (T->uuid, tmpstr);
        fprintf (f, "tenant %s { \n", tmpstr);
        
        meterwatch *mw = T->watch.first;
        while (mw) {
            id2str (mw->id, tmpstr);
            fprintf (f, "  watcher '%s' tr:%i tp:%i\n", tmpstr, mw->trigger,
                     mw->tp);
            mw = mw->next;
        }
        
        T = tenant_next (T, TENANT_LOCK_READ);
    }
    fprintf (f, "}\n");
    fclose (f);
}

/*/ ======================================================================= /*/
/** Thread runner for the reaper. This thread goes over the tenant list,
  * figuring out how much storage they have in use. If this is over the
  * set quota, it starts culling days from the database starting at
  * the earliest recorded date, until the tenant is under quota again. */
/*/ ======================================================================= /*/
void reaper_run (thread *self) {
    thread_setname (self, "quota");
    while (1) {
        sleep (10);
        int numtenants = 0;
        uint64_t totalsz = 0;
        uint64_t quota = 0;
        time_t earliest = 0;
        time_t tnow = time (NULL);
        uuid *tenants = db_list_tenants (APP.reaperdb, &numtenants);
        
        // FIXME remove again
        dump_tenant_debug();
        
        log_info ("Starting quota reaper run for %i tenants", numtenants);
        for (int i=0; i<numtenants; ++i) {
            char uuidstr[40];
            totalsz = 0;
            earliest = 0;
            uuid2str (tenants[i], uuidstr);
            if (db_open (APP.reaperdb, tenants[i], NULL)) {
                log_info ("Checking tenant <%s>", uuidstr);
                var *meta = db_get_metadata (APP.reaperdb);
                if(meta) {
                    quota = var_get_int_forkey (meta, "quota");
                    var_free (meta);
                }
                
                if (!quota) quota = 128;
                quota = quota * (1024ULL*1024ULL);
                int numhosts = 0;
                uuid *hosts = db_list_hosts (APP.reaperdb, &numhosts);
                log_info ("Checking %i hosts", numhosts);
                for (int ii=0; ii<numhosts; ++ii) {
                    usage_info info = {0ULL,0,0};
                    db_get_usage (APP.reaperdb, &info, hosts[ii]);
                    
                    if (info.last && (tnow - info.last > 86400)) {
                        char hoststr[40];
                        uuid2str (hosts[i], hoststr);
                        log_info ("Removing stale host %s", hoststr);
                        db_remove_host (APP.reaperdb, hosts[ii]);
                    }
                    else if (info.earliest) {
                        totalsz += info.bytes;
                        if ((! earliest) || info.earliest < earliest) {
                            earliest = info.earliest;
                        }
                    }
                }
                
                if (totalsz > quota) {
                    char uuidstr[40];
                    uuid2str (tenants[i], uuidstr);
                    log_info ("Tenant %s is %" PRIu64 " bytes over quota",
                              uuidstr, (totalsz-quota));
                    
                    if (tnow - earliest > 86399) {
                        log_info ("Deleting records for ts=%i", 
                                  time2date(earliest));
                        for (int j=0; j<numhosts; ++j) {
                            db_delete_host_date (APP.reaperdb, hosts[j],
                                                 earliest);
                        }
                        --i;
                    }
                }
                
                free (hosts);
                db_close (APP.reaperdb);
            }
            else {
                log_error ("Could not open tenant <%s>", uuidstr);
            }
        }
        free (tenants);
        sleep (300);
    }
}

/*/ ======================================================================= /*/
/** Thread runner for handling configuration reload requests.
  * This is done to keep the signal handler path clean. */
/*/ ======================================================================= /*/
void conf_reloader_run (thread *t) {
    thread_setname (t, "reloader");
    conf_reloader *self = (conf_reloader *) t;
    while (1) {
        conditional_wait_fresh (self->cond);
        if (! var_load_json (APP.conf, APP.confpath)) {
            log_error ("Error loading %s: %s\n",
                       APP.confpath, parse_error());
        }
        else {
            opticonf_handle_config (APP.conf);
        }
    }
}

/*/ ======================================================================= /*/
/** Create the configuration reloader thread */
/*/ ======================================================================= /*/
conf_reloader *conf_reloader_create (void) {
    conf_reloader *self = malloc (sizeof (conf_reloader));
    self->cond = conditional_create();
    thread_init (&self->super, conf_reloader_run, NULL);
    return self;
}

/*/ ======================================================================= /*/
/** Signal the configuration reloader thread to do a little jig */
/*/ ======================================================================= /*/
void conf_reloader_reload (conf_reloader *self) {
    conditional_signal (self->cond);
}

/*/ ======================================================================= /*/
/** Signal handler for SIGHUP */
/*/ ======================================================================= /*/
void daemon_sighup_handler (int sig) {
    conf_reloader_reload (APP.reloader);
    signal (SIGHUP, daemon_sighup_handler);
}

/*/ ======================================================================= /*/
/** Load host data on restoring sessions */
/*/ ======================================================================= /*/
void restore_host_current (uuid tenantid, host *h) {
    char hostidstr[40];
    uuid2str (h->uuid, hostidstr);
    
    db *DB = localdb_create (APP.dbpath);
    if (db_open (DB, tenantid, NULL)) {
        if (db_host_exists (DB, h->uuid)) {
            db_get_current (DB, h);
            log_info ("Repopulating host <%s>: %i", hostidstr, h->mcount);
        }
        db_close (DB);
    }
    db_free (DB);
    h->lastmodified = time(NULL);
}

/*/ ======================================================================= /*/
/** Main loop. Waits for a packet, then handles it. */
/*/ ======================================================================= /*/
int daemon_main (int argc, const char *argv[]) {
    if (strcmp (APP.logpath, "@syslog") == 0) {
        log_open_syslog ("opticon-collector", APP.loglevel);
    }
    else {
        log_open_file (APP.logpath, APP.loglevel);
    }

    log_info ("--- Opticon-collector ready for action ---");

    tenant_init();
    ping_start();
    
    var *slist = db_get_global (APP.db, "sessions");
    if (slist) {
        log_info ("Restoring sessions");
        sessiondb_restore (slist, restore_host_current);
        var_free (slist);
        session_expire (time(NULL) - 905);
    }
    
    /* Set up threads */
    APP.queue = packetqueue_create (1024, APP.transport);
    APP.reloader = conf_reloader_create();
    APP.watchthread = thread_create (watchthread_run, NULL);
    APP.overviewthread = thread_create (overviewthread_run, NULL);
    APP.reaperthread = thread_create (reaper_run, NULL);
    
    signal (SIGHUP, daemon_sighup_handler);
    
    while (1) {
        pktbuf *pkt = packetqueue_waitpkt (APP.queue);
        if (pkt) {
            uint32_t netid = gen_networkid (&pkt->addr);
            ioport *pktbufport = ioport_create_buffer (pkt->pkt, 2048);
            /* Pointing it at itself will just move the cursor */
            ioport_write (pktbufport, pkt->pkt, pkt->sz);
            
            log_debug ("Processing packet with size %i", pkt->sz);
            
            /* Check packet version info */
            if (pkt->pkt[0] == 'o' && pkt->pkt[1] == '6') {
                if (pkt->pkt[2] == 'a' && pkt->pkt[3] == '1') {
                    /* AUTH v1 */
                    handle_auth_packet (pktbufport, netid, &pkt->addr);
                }
                else if (pkt->pkt[2] == 'm' && pkt->pkt[3] == '1') {
                    /* METER v1 */
                    handle_meter_packet (pktbufport, netid);
                }
            }
            ioport_close (pktbufport);
            pkt->pkt[0] = 0;
        }
    }
    return 0;
}

/*/ ======================================================================= /*/
/** Set up foreground flag */
/*/ ======================================================================= /*/
int set_foreground (const char *i, const char *v) {
    APP.foreground = 1;
    return 1;
}

/*/ ======================================================================= /*/
/** Set up configuration file path */
/*/ ======================================================================= /*/
int set_confpath (const char *i, const char *v) {
    APP.confpath = v;
    return 1;
}

/*/ ======================================================================= /*/
/** Set up meter configuration file path */
/*/ ======================================================================= /*/
int set_mconfpath (const char *i, const char *v) {
    APP.mconfpath = v;
    return 1;
}

/*/ ======================================================================= /*/
/** Set up pidfile path */
/*/ ======================================================================= /*/
int set_pidfile (const char *i, const char *v) {
    APP.pidfile = v;
    return 1;
}

/*/ ======================================================================= /*/
/** Set the logfile path, @syslog for logging to syslog */
/*/ ======================================================================= /*/
int set_logpath (const char *i, const char *v) {
    APP.logpath = v;
    return 1;
}

/*/ ======================================================================= /*/
/** Handle --loglevel */
/*/ ======================================================================= /*/
int set_loglevel (const char *i, const char *v) {
    if (strcmp (v, "CRIT") == 0) APP.loglevel = LOG_CRIT;
    else if (strcmp (v, "ERR") == 0) APP.loglevel = LOG_ERR;
    else if (strcmp (v, "WARNING") == 0) APP.loglevel = LOG_WARNING;
    else if (strcmp (v, "INFO") == 0) APP.loglevel = LOG_INFO;
    else if (strcmp (v, "DEBUG") == 0) APP.loglevel = LOG_DEBUG;
    else APP.loglevel = LOG_WARNING;
    return 1;
}

/*/ ======================================================================= /*/
/** Set up network configuration */
/*/ ======================================================================= /*/
int conf_network (const char *id, var *v, updatetype tp) {
    switch (tp) {
        case UPDATE_CHANGE:
        case UPDATE_REMOVE:
            /* Do the lame thing, respawn from the watchdog with the
               new settings */
            exit (0);
        case UPDATE_ADD:
            APP.listenport = var_get_int_forkey (v, "port");
            APP.listenaddr = var_get_str_forkey (v, "address");
            log_info ("Listening on %s:%i", APP.listenaddr, APP.listenport);
            if (strcmp (APP.listenaddr, "*") == 0) {
                APP.listenaddr = "::";
            }
            break;
        case UPDATE_NONE:
            break;
    }
    return 1;
}

/*/ ======================================================================= /*/
/** Set up database path from configuration */
/*/ ======================================================================= /*/
int conf_db_path (const char *id, var *v, updatetype tp) {
    if (tp == UPDATE_REMOVE) return 0;
    if (tp == UPDATE_CHANGE) {
        db_free (APP.db);
        db_free (APP.writedb);
        db_free (APP.overviewdb);
        db_free (APP.reaperdb);
    }
    APP.db = localdb_create (var_get_str (v));
    APP.writedb = localdb_create (var_get_str (v));
    APP.overviewdb = localdb_create (var_get_str (v));
    APP.reaperdb = localdb_create (var_get_str (v));
    APP.dbpath = var_get_str (v);
    return 1;
}

/*/ ======================================================================= /*/
/** Set up watchlist from meter definitions */
/*/ ======================================================================= /*/
int conf_meters (const char *id, var *v, updatetype tp) {
    if (tp == UPDATE_REMOVE) return 0;
    watchlist_populate (&APP.watch, v);
    APP.watchdef = var_clone (v);
    return 1;
}

/*/ ======================================================================= /*/
/** Set up global graph configuration */
/*/ ======================================================================= /*/
int conf_graph (const char *id, var *v, updatetype tp) {
    if (tp == UPDATE_REMOVE) return 0;
    APP.graphlist = graphlist_create();
    graphlist_make (APP.graphlist, v);
    return 1;
}

/*/ ======================================================================= /*/
/** Set up debug dump path, if any */
/*/ ======================================================================= /*/
int conf_dump_path (const char *id, var *v, updatetype tp) {
    if (tp == UPDATE_REMOVE) return 0;
    APP.dumppath = strdup (var_get_str (v));
    return 1;
}

/*/ ======================================================================= /*/
/** Set up packet dump path, if any */
/*/ ======================================================================= /*/
int conf_packetdump_path (const char *id, var *v, updatetype tp) {
    if (tp == UPDATE_REMOVE) return 0;
    APP.pktdump = strdup (var_get_str (v));
    return 1;
}

/*/ ======================================================================= /*/
/** Set up path for notification script, if any */
/*/ ======================================================================= /*/
int conf_notify (const char *id, var *v, updatetype tp) {
    if (tp == UPDATE_REMOVE) return 0;
    APP.notifypath = strdup (var_get_str (v));
    return 1;
}

appcontext APP; /**< Global application context */

/*/ ======================================================================= /*/
/** Command line options */
/*/ ======================================================================= /*/
cliopt CLIOPT[] = {
    {"--foreground","-f",OPT_FLAG,NULL,set_foreground},
    {"--pidfile","-p",OPT_VALUE,
        "/var/run/opticon-collector.pid", set_pidfile},
    {"--logfile","-l",OPT_VALUE, "@syslog", set_logpath},
    {"--loglevel","-L",OPT_VALUE, "INFO", set_loglevel},
    {"--config-path","-c",OPT_VALUE,
        "/etc/opticon/opticon-collector.conf", set_confpath},
    {"--meter-config-path","-m",OPT_VALUE,
        "/etc/opticon/opticon-meter.conf", set_mconfpath},
    {NULL,NULL,0,NULL,NULL}
};

/*/ ======================================================================= /*/
/** Application main. Handles configuration and command line, sets up
  * the application context APP and spawns daemon_main(). */
/*/ ======================================================================= /*/
int main (int _argc, const char *_argv[]) {
    int argc = _argc;
    const char **argv = cliopt_dispatch (CLIOPT, _argv, &argc);
    if (! argv) return 1;

    opticonf_add_reaction ("network", conf_network);
    opticonf_add_reaction ("database/path", conf_db_path);
    opticonf_add_reaction ("meter", conf_meters);
    opticonf_add_reaction ("graph", conf_graph);
    opticonf_add_reaction ("debug/dump", conf_dump_path);
    opticonf_add_reaction ("debug/packetdump", conf_packetdump_path);
    opticonf_add_reaction ("notify/call", conf_notify);
    
    APP.transport = intransport_create_udp();
    APP.codec = codec_create_pkt();
    APP.dumppath = NULL;
    APP.pktdump = NULL;
    APP.notifypath = NULL;

    APP.conf = var_alloc();
    
    /* Preload the default meter set */
    var *defmeters = get_default_meterdef();
    var_link_as (defmeters, APP.conf, "meter");
    
    var *defsummary = get_default_summarydef();
    var_link_as (defsummary, APP.conf, "summary");
    
    var *defgraph = get_default_graphs();
    var_link_as (defgraph, APP.conf, "graph");
    
    /* Load other meters from meter.conf */
    if (! var_load_json (defmeters, APP.mconfpath)) {
        log_error ("Error loading %s: %s\n",
                   APP.mconfpath, parse_error());
        return 1;
    }
    
    /* Now load the main config in the same var space */
    if (! var_load_json (APP.conf, APP.confpath)) {
        log_error ("Error loading %s: %s\n",
                   APP.confpath, parse_error());
        return 1;
    }
    
    opticonf_handle_config (APP.conf);
    ping_init();
    log_info ("Configuration loaded");
    
    if (! intransport_setlistenport (APP.transport, APP.listenaddr, 
                                     APP.listenport)) {
        log_error ("Error setting listening port");
    }
    if (! daemonize (APP.pidfile, argc, argv, daemon_main, APP.foreground)) {
        log_error ("Error spawning");
        return 1;
    }
    return 0;
}
