#include "opticon-collector.h"
#include "ping.h"
#include <pthread.h>
#include <libopticondb/db_local.h>
#include <libopticon/auth.h>
#include <libopticon/ioport.h>
#include <libopticon/pktwrap.h>
#include <libopticon/util.h>
#include <libopticon/var.h>
#include <libopticon/var_parse.h>
#include <libopticon/var_dump.h>
#include <libopticon/react.h>
#include <libopticon/daemon.h>
#include <libopticon/log.h>
#include <libopticon/cliopt.h>
#include <libopticon/popen.h>
#include <libopticon/transport_udp.h>
#include <libopticon/summary.h>
#include <libopticon/timer.h>

watchlist WATCHERS;

/*/ ======================================================================= /*/
/** Writes host data to an overview dict
    \param h The host to write out
    \param ovdict The var object to write into, keyed by host-uuid. */
/*/ ======================================================================= /*/
void host_to_overview (host *h, var *ovdict) {
    char mid[16];
    char uuidstr[40];
    uuid2str (h->uuid, uuidstr);
    var *res = var_get_dict_forkey (ovdict, uuidstr);
    meter *m = h->first;
    while (m) {
        /* We only want singular values */
        if (m->count > 0) {
        
            /* Except for loadavg, he's special */
            id2str (m->id, mid);
            if (strcmp (mid, "loadavg") != 0) {
                m = m->next;
                continue;
            }
        }

        id2str (m->id, mid);
        
        switch (m->id & MMASK_TYPE) {
            case MTYPE_INT:
                var_set_int_forkey (res, mid, meter_get_uint (m, 0));
                break;
            
            case MTYPE_FRAC:
                var_set_double_forkey (res, mid, meter_get_frac (m, 0));
                break;
            
            case MTYPE_STR:
                var_set_str_forkey (res, mid, m->d.str[0].str);
                break;
            
            default:
                break;
        }
        m = m->next;
    }
}

/*/ ======================================================================= /*/
/** Returns a dictionary of overview data for the tenant, keyed by the
    uuid of each host.
    \param t The tenant object,
    \return Allocated var object (caller should free). */
/*/ ======================================================================= /*/
var *tenant_overview (tenant *t) {
    var *res = var_alloc();
    var *ov = var_get_dict_forkey (res, "overview");
    host *h = t->first;
    while (h) {
        pthread_rwlock_rdlock (&h->lock);
        host_to_overview (h, ov);
        pthread_rwlock_unlock (&h->lock);
        h = h->next;
    }
    return res;
}

/*/ ======================================================================= /*/
/** Handles graphdata for a host.
    \param host The host object.
    \param list The graphlist to go through */
/*/ ======================================================================= /*/
void watchthread_handle_graph (host *host, graphlist *list) {
    graphtarget *tgt = graphlist_begin (list);
    while (tgt) {
        double val = 0.0;
        meter *m = host_find_meter_name (host, tgt->id);
        if (m && (m->count < SZ_EMPTY_VAL)) {
            switch (m->id & MMASK_TYPE) {
                case MTYPE_INT:
                    val = (double) *(m->d.u64);
                    break;
                    
                case MTYPE_FRAC:
                    val = m->d.frac[0];
                    break;
            }
        }
        if (db_open (APP.writedb, host->tenant->uuid, NULL)) {
            db_set_graph (APP.writedb, host->uuid, tgt->graph_id,
                          tgt->datum_id, val);
            db_close (APP.writedb);
        }
        
        tgt = graphlist_next (list, tgt);
    }
}

/*/ ======================================================================= /*/
/** Inspects a host's metering data to determine its current status
  * and problems, then write it to disk.
  * \param host The host to inspect. */
/*/ ======================================================================= /*/
void watchthread_handle_host (host *host) {
    int problemcount = 0;
    double totalbadness = 0.0;
    time_t tnow = time (NULL);
    meter *m = host->first;
    meterwatch *w;
    watchadjust *adj = NULL;
    graphtarget *gt = NULL;
    watchtrigger maxtrigger = WATCH_NONE;
    char label[16];
    char uuidstr[40];
    struct sockaddr_storage addr;
    
    pthread_rwlock_wrlock (&host->lock);
    
    meterid_t mid_ip = makeid ("link/ip",MTYPE_STR,0);
    meter *m_ip = host_find_meter (host, mid_ip);
    if (m_ip) {
        fstring ipstr = meter_get_str (m_ip, 0);
        if (ipstr.str[0]) {
            str2ip (ipstr.str, &addr);
            double rtt = ping_get_rtt (&addr);
            double loss = ping_get_loss (&addr);
            meterid_t mid_rtt = makeid ("link/rtt",MTYPE_FRAC,0);
            meterid_t mid_loss = makeid ("link/loss",MTYPE_FRAC,0);
            meter *m_rtt = host_get_meter (host, mid_rtt);
            meter *m_loss = host_get_meter (host, mid_loss);
            meter_setcount (m_rtt, 0);
            meter_set_frac (m_rtt,0,rtt);
            meter_setcount (m_loss, 0);
            meter_set_frac (m_loss,0,loss);
        }
    }

    /* We'll store the status information as a meter itself */
    meterid_t mid_status = makeid ("status",MTYPE_STR,0);
    meter *m_status = host_get_meter (host, mid_status);
    fstring ostatus = meter_get_str (m_status, 0);
    meter_setcount (m_status, 0);
    if (ostatus.str[0] == 0) strcpy (ostatus.str, "UNSET");
    
    /* If the data is stale, don't add to badness, just set the status. */
    if ((tnow - host->lastmodified) > 80) {
        if (strcmp (ostatus.str, "STALE") != 0) {
            uuid2str (host->uuid, uuidstr);
            log_info ("Status change host <%s> %s -> STALE",
                      uuidstr, ostatus.str);
            db_open (APP.writedb, host->tenant->uuid, NULL);
            db_write_log (APP.writedb, host->uuid, "watcher", "Host went STALE");
            db_close (APP.writedb);
            tenant_set_notification (host->tenant, true, "STALE", host->uuid);
        }
        meter_set_str (m_status, 0, "STALE");
        if (db_open (APP.writedb, host->tenant->uuid, NULL)) {
            if (! db_host_exists (APP.writedb, host->uuid)) {
                uuid2str (host->uuid, uuidstr);
                log_info ("Removing STALE host <%s> that no longer exists in "
                          "the database.", uuidstr);
                pthread_rwlock_unlock (&host->lock);
                tenant *T = host->tenant;
                
                tenant_relock (T, TENANT_LOCK_WRITE);
                host_delete (host);
                tenant_relock (T, TENANT_LOCK_READ);
                
                db_close (APP.writedb);
                return;
            }
            db_close (APP.writedb);
        }
    }
    else {

        /* Figure out badness for each meter, and add to host */
        while (m) {
            m->badness = 0.0;
            int handled = 0;
            
            /* Get host-level adjustments in place */
            adj = adjustlist_find (&host->adjust, m->id);
            
            /* First go over the tenant-defined watchers */
            pthread_mutex_lock (&host->tenant->watch.mutex);
            w = host->tenant->watch.first;
            while (w) {
                if ((w->id & MMASK_NAME) == (m->id & MMASK_NAME)) {
                    m->badness += calculate_badness (m, w, adj, &maxtrigger);
                    handled = 1;
                }
                w = w->next;
            }
            pthread_mutex_unlock (&host->tenant->watch.mutex);
       
            /* If the tenant didn't have anything suitable, go over the
               global watchlist */
            if (! handled) {
                pthread_mutex_lock (&APP.watch.mutex);
                w = APP.watch.first;

                while (w) {
                    if ((w->id & MMASK_NAME) == (m->id & MMASK_NAME)) {
                        m->badness += calculate_badness (m, w, adj, 
                                                         &maxtrigger);
                        handled = 1;
                    }
                    w = w->next;
                }
                pthread_mutex_unlock (&APP.watch.mutex);
            }
      
            if (m->badness) problemcount++;
            totalbadness += m->badness;
            
            handled = 0;
            
            
            m = m->next;
        }
    
        /* Don't raise a CRIT alert on a WARN condition */
        switch (maxtrigger) {
            case WATCH_NONE:
                totalbadness = 0.0;
                break;
        
            case WATCH_WARN:
                if (host->badness > 50.0) totalbadness = 0.0;
                break;
        
            case WATCH_ALERT:
                if (host->badness > 90.0) totalbadness = 0.0;
                break;
        
            case WATCH_CRIT:
                if (host->badness > 150.0) totalbadness = 0.0;
                break;
        }
    
        host->badness += totalbadness;
    
        /* Put up the problems as a meter as well */
        meterid_t mid_problems = makeid ("problems",MTYPE_STR,0);
        meter *m_problems = host_get_meter (host, mid_problems);
    
        /* While we're looking at it, consider the current badness, if there
           are no problems, it should be going down. */
        if (! problemcount) {
            if (host->badness > 100.0) host->badness = host->badness/2.0;
            else if (host->badness > 50.0) host->badness = host->badness *0.75;
            else if (host->badness > 25.0) host->badness = host->badness - 12.50;
            else host->badness = 0.0;
            meter_set_empty_array (m_problems);
        }
        else {
            /* If we reached the top, current level may still be out of
               its league, so allow it to decay slowly */
            if (totalbadness == 0.0) host->badness *= 0.9;
        
            /* Fill in the problem array */
            int i=0;
            meter_setcount (m_problems, problemcount);
            m = host->first;
        
            while (m && (i<16)) {
                if (m->badness > 0.00) {
                    id2str (m->id, label);
                    meter_set_str (m_problems, i++, label);
                }
                m = m->next;
            }
        }
    
        meterid_t mid_badness = makeid ("badness",MTYPE_FRAC,0);
        meter *m_badness = host_get_meter (host, mid_badness);
        meter_setcount (m_badness, 0);
        meter_set_frac (m_badness, 0, host->badness);
    
        const char *nstatus = "UNSET";
    
        /* Convert badness to a status text */
        if (host->badness < 30.0) nstatus = "OK";
        else if (host->badness < 80.0) nstatus = "WARN";
        else if (host->badness < 120.0) nstatus = "ALERT";
        else nstatus = "CRIT";
    
        if (strcmp (nstatus, ostatus.str) != 0) {
            uuid2str (host->uuid, uuidstr);
            log_info ("Status change host <%s> %s -> %s", uuidstr,
                      ostatus.str, nstatus);
                      
            db_open (APP.writedb, host->tenant->uuid, NULL);
            db_write_log (APP.writedb, host->uuid, "watcher",
                          "Status change %s -> %s", ostatus.str, nstatus);
            db_close (APP.writedb);
            
            bool isproblem = (host->badness>= 80.0);
            tenant_set_notification (host->tenant, isproblem, nstatus,
                                     host->uuid);
        }
    
        meter_set_str (m_status, 0, nstatus);
    }
    
    /* Write to db */
    if (db_open (APP.writedb, host->tenant->uuid, NULL)) {
        db_save_record (APP.writedb, tnow, host);
        db_close (APP.writedb);
    }
    
    if (host->graphlist) {
        watchthread_handle_graph (host, host->graphlist);
    }
    
    if (APP.graphlist) {
        watchthread_handle_graph (host, APP.graphlist);
    }
    
    pthread_rwlock_unlock (&host->lock);
    
    /* for tallying the summaries we only need read access */

    if (! host->tenant) return;
    
    pthread_rwlock_rdlock (&host->lock);
    m = host->first;
    while (m) {
        summaryinfo_add_meterdata (&host->tenant->summ, m->id, &m->d);
        m = m->next;
    }
    pthread_rwlock_unlock (&host->lock);
}

/*/ ======================================================================= /*/
/** This thread oes over all tenants, once every minute, to gather their
    overview data. */
/*/ ======================================================================= /*/
void overviewthread_run (thread *self) {
    thread_setname (self, "overview");
    char uuidstr[40];
    tenant *tcrsr;
    timer ti;
    time_t t_now = time (NULL);
    time_t t_next = (t_now+60)-((t_now+60)%60)+2;
    log_debug ("Overviewthread started");
    sleep (t_next - t_now);
    t_next += 60;
    
    while (1) {
        timer_start (&ti);
        tcrsr = tenant_first (TENANT_LOCK_READ);
        while (tcrsr) {
            var *overv = tenant_overview (tcrsr);
            if (db_open (APP.overviewdb, tcrsr->uuid, NULL)) {
                db_set_overview (APP.overviewdb, overv);
                db_close (APP.overviewdb);
            }
            
            var *n = tenant_check_notification (tcrsr);
            if (n) {
                uuid2str (tcrsr->uuid, uuidstr);
                log_info ("Notification triggered for tenant <%s>: %i",
                          uuidstr, var_get_count (n));
                
                var *vissues = var_get_dict_forkey (n, "issues");
                var *crsr = var_first (vissues);
                while (crsr) {
                    var *ovdata = var_find_key (overv, crsr->id);
                    if (ovdata) {
                        var_merge (crsr, ovdata);
                    }
                    crsr = crsr->next;
                }
                
                char cmd[1024];
                sprintf (cmd, "%s %s", APP.notifypath, uuidstr);
                
                if (APP.notifypath) {
                    FILE *fcmd = popen_safe (cmd, "w");
                    if (fcmd) {
                        var_dump (n, fcmd);
                        pclose_safe (fcmd);
                    }
                }
                
                // n now contains the notifications plus extracted
                // overview data for the hosts involved.
                
                // FIXME do something
                var_free (n);
            }
            
            var_free (overv);
            tcrsr = tenant_next (tcrsr, TENANT_LOCK_READ);
        }
        
        timer_end (&ti);
        t_now = time (NULL);
        log_debug ("Overview took %.1f ms", ti.diff);
        if (t_next <= t_now) t_next += 60;
        if (t_now < t_next) {
            sleep (t_next-t_now);
        }
        else {
            log_error ("Overview round cannot keep up: %is overdue",t_now-t_next);
        }
        t_next += 60;
        while (t_next < t_now) t_next += 60;
        while ((t_next - t_now) > 70) t_next -= 60;
    }
}

/*/ ======================================================================= /*/
/** Main loop for the watchthread */
/*/ ======================================================================= /*/
void watchthread_run (thread *self) {
    thread_setname (self, "watcher");
    tenant *tcrsr;
    host *hcrsr;
    host *ncrsr;
    timer ti;
    time_t t_now = time (NULL);
    time_t t_next = (t_now+60)-((t_now+60)%60);
    log_debug ("Watchthread started");
    sleep (t_next - t_now);
    t_next += 60;
    
    while (1) {
        timer_start (&ti);
        tcrsr = tenant_first (TENANT_LOCK_READ);
        while (tcrsr) {
            summaryinfo_start_round (&tcrsr->summ);
            hcrsr = tcrsr->first;
            while (hcrsr) {
                ncrsr = hcrsr->next;
                watchthread_handle_host (hcrsr);
                hcrsr = ncrsr;
            }
            
            var *tally = summaryinfo_tally_round (&tcrsr->summ);
            if (db_open (APP.writedb, tcrsr->uuid, NULL)) {
                db_set_summary (APP.writedb, tally);
                db_close (APP.writedb);
            }
            
            var_free (tally);
            tcrsr = tenant_next (tcrsr, TENANT_LOCK_READ);
        }
        
        timer_end (&ti);
        t_now = time (NULL);
        if (t_now < t_next) {
            log_debug ("Watchthread took %.1f ms", ti.diff);
            sleep (t_next-t_now);
        }
        else {
            log_error ("Watchthread round cannot keep up");
        }
        t_next += 60;
        while (t_next < t_now) t_next += 60;
    }
}
