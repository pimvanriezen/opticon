#include "opticon-agent.h"
#include <libopticon/ioport.h>
#include <libopticon/codec_pkt.h>
#include <libopticon/pktwrap.h>
#include <libopticon/util.h>
#include <libopticon/var.h>
#include <libopticon/var_parse.h>
#include <libopticon/react.h>
#include <libopticon/daemon.h>
#include <libopticon/log.h>
#include <libopticon/cliopt.h>
#include <libopticon/transport_udp.h>
#include <libopticon/host_import.h>
#include <libopticon/ioport_file.h>
#include <libopticon/codec_json.h>
#include <libopticon/popen.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>

appcontext APP;

void __breakme (void) {}

/** Daemon main run loop. */
int daemon_main (int argc, const char *argv[]) {
    const char *buf;
    size_t sz;

    if (strcmp (APP.logpath, "@syslog") == 0) {
        log_open_syslog ("opticon-agent", APP.loglevel);
    }
    else {
        log_open_file (APP.logpath, APP.loglevel);
    }
    
    popen_init();
    probelist_start (&APP.probes);
    collectorlist_start (&APP.collectors);
    
    time_t tlast = time (NULL);
    time_t nextslow = tlast + 5;
    time_t nextsend = tlast + 10;

    int slowround = 0;

    log_info ("Daemonized");
    
    while (1) {
        time_t tnow = tlast = time (NULL);
        
        slowround = 0;
        
        /* If a slow round is due at this time, use the excuse to send an
           authentication packet */
        if (nextslow <= tnow) {
            slowround = 1;
            collector *c = APP.collectors.first;
            while (c) {
                
                uint32_t sid = c->auth.sessionid;
                if (! sid) sid = gen_sessionid();
                log_debug ("Authenticating session <%08x>", sid);
                c->auth.sessionid = sid;
                c->auth.serial = 0;
            
                /* Only rotate the AES key every half hour */
                if (tnow - c->lastkeyrotate > 1800) {
                    c->auth.sessionkey = aeskey_create();
                    c->lastkeyrotate = tnow;
                }
            
                /* Dispatch */
                ioport *io_authpkt = ioport_wrap_authdata (&c->auth,
                                                           gen_serial());
            
                sz = ioport_read_available (io_authpkt);
                buf = ioport_get_buffer (io_authpkt);
                outtransport_send (c->transport, (void*) buf, sz);
                authresender_schedule (c->resender, buf, sz);
                ioport_close (io_authpkt);
                
                c = c->next;
            }
            
            /* Schedule next slow round */
            nextslow = nextslow + 300;
        }
        
        log_debug ("Poking probes");

        probe *p = APP.probes.first;
        time_t wakenext = tnow + 300;

        /* Go over the probes to figure out whether we should kick them */        
        while (p) {
            time_t firewhen = p->lastpulse + p->interval;
            if (firewhen <= tnow) {
                conditional_signal (&p->pulse);
                p->lastpulse = tnow;
            }
            
            /* Figure out whether the next event for this probe is sooner
               than the next wake-up time we determined so far */
            if (p->lastpulse + p->interval < wakenext) {
                wakenext = p->lastpulse + p->interval;
            }
            
            p = p->next;
        }
        
        int collected = 0;
        int ncollected = 0;
        host *h = host_alloc();
        var *vnagios = var_alloc();
        var *vchkwarn = var_get_array_forkey (vnagios, "chkwarn");
        var *vchkalert = var_get_array_forkey (vnagios, "chkalert");
        
        /* If we're in a slow round, we already know we're scheduled. Otherwise,
           see if the next scheduled moment for sending a (fast lane) packet
           has passed. */
        if (slowround || (tnow >= nextsend)) {
            h->uuid = APP.hostid;
            host_begin_update (h, time (NULL));

            if (! slowround) while (nextsend <= tnow) nextsend += 60;
            log_debug ("Collecting probes");
        
            /* Go over the probes again, picking up the ones relevant to the
               current round being performed */
            p = APP.probes.first;
            while (p) {
                pthread_mutex_lock (&p->vlock);
                volatile var *v = p->vcurrent;
                /* See if data for this probe has been collected since the last kick */
                if (v && (p->lastdispatch <= p->lastreply)) {
                    /* Filter probes for the current lane */
                    if ((slowround && p->interval>60) ||
                        ((!slowround) && p->interval<61)) {
                        log_debug ("Collecting <%s>", p->call);
                        
                        if (p->type == PROBE_NAGIOS) {
                            /* Check for alert/warning state and
                             * summarize before adding to host struct */
                             int pstatus = var_get_int_forkey ((var*)v, "status");
                             if (pstatus) {
                                 var *arr;
                                 switch (pstatus) {
                                    case 1:
                                        var_add_str (vchkwarn, p->id);
                                        break;
                                    
                                    default:
                                        var_add_str (vchkalert, p->id);
                                        break;
                                }
                                collected++;
                            }
                            ncollected++;
                        }
                        else {
                            host_import (h, (var *) v);
                            collected++;
                        }
                        p->lastdispatch = tnow;
                    }
                }
                else {
                    if (tnow - p->lastreply > (2*(p->interval))) {
                        log_warn ("Probe <%s> seems stuck after %i seconds",
                                  p->call, tnow - p->lastreply);
                    }
                }
                pthread_mutex_unlock (&p->vlock);
                p = p->next;
            }
        }
        
        /* Add the chk tree with nagios self-checks to the data */
        if (ncollected) host_import (h, vnagios);
         
        /* If any data was collected, encode it */
        if (collected) {
            log_debug ("Encoding probes");
        
            ioport *encoded = ioport_create_buffer (NULL, 4096);
            if (! encoded) {
                log_warn ("Error creating ioport");
                ioport_close (encoded);
                host_delete (h);
                continue;
            }
        
            if (! codec_encode_host (APP.codec, encoded, h)) {
                log_warn ("Error encoding host");
                ioport_close (encoded);
                host_delete (h);
                continue;
            }

            if (APP.dumppath) {
                FILE *pktf = fopen (APP.dumppath,"a");
                fprintf (pktf, "\n--- %s ---\n\n", slowround?"Slow":"Fast");
                ioport *dump = ioport_create_filewriter (pktf);
                log_debug ("dump %llx", dump);
                codec *jsonc = codec_create_json();
                log_debug ("jsonc %llx", jsonc);
                codec_encode_host (jsonc, dump, h);
                codec_release (jsonc);
                ioport_close (dump);
                fclose (pktf);
            }
        
            log_debug ("Encoded %i bytes", ioport_read_available (encoded));

            collector *c = APP.collectors.first;
            while (c) {
                ioport *wrapped = ioport_wrap_meterdata (c->auth.sessionid,
                                                         gen_serial(),
                                                         c->auth.sessionkey,
                                                         encoded);
        
        
                if (! wrapped) {
                    log_error ("Error wrapping");
                }
                else {
                    sz = ioport_read_available (wrapped);
                    buf = ioport_get_buffer (wrapped);
        
                    /* Send it off into space */
                    outtransport_send (c->transport, (void*) buf, sz);
                    log_info ("%s lane packet sent: %i bytes", 
                              slowround ? "Slow":"Fast", sz);
                }
                ioport_close (wrapped);
                
                c = c->next;
            }
            ioport_close (encoded);
        }
        
        /* Done with the host object */
        host_delete (h);
        var_free (vnagios);

        /* Figure out what the next scheduled wake-up time is */
        tnow = time (NULL);
        if (nextsend < wakenext) wakenext = nextsend;
        if (nextslow < wakenext) wakenext = nextslow;
        if (wakenext > tnow) {
            log_debug ("Sleeping for %i seconds", (wakenext-tnow));
            sleep (wakenext-tnow);
        }
    }
    return 666;
}

/** Parse /collector */
int conf_collector (const char *id, var *v, updatetype tp) {
    if (tp == UPDATE_REMOVE) exit (0);
    if (v->type == VAR_DICT) {
        collectorlist_add_host (&APP.collectors, v);
    }
    else if (v->type == VAR_ARRAY) {
        var *crsr = var_first(v);
        while (crsr) {
            log_info ("Adding collector host <%s>",
                      var_get_str_forkey (crsr, "address"));
            collectorlist_add_host (&APP.collectors, crsr);
            crsr = crsr->next;
        }
    }
    else {
        log_error ("Invalid collector specification");
        return 0;
    }
    return 1;
}

/** Parse /meter into probes */
int conf_probe (const char *id, var *v, updatetype tp) {
    if (tp != UPDATE_ADD) exit (0);
    const char *vtp = var_get_str_forkey (v, "type");
    const char *call = var_get_str_forkey (v, "call");
    var *opt = var_get_dict_forkey (v, "options");
    int interval = var_get_int_forkey (v, "interval");
    if (interval == 0) {
        const char *frequency = var_get_str_forkey (v, "frequency");
        if (frequency) {
            if (strcmp (frequency, "high") == 0) interval = 60;
            else if (strcmp (frequency, "low") == 0) interval = 300;
            else {
                log_error ("Invalid frequency specification for "
                           "probe '%s'", id);
                return 0;
            }
        }
        else {
            log_error ("No frequency defined for probe '%s'", id);
            return 0;
        }
    }
    probetype t = PROBE_BUILTIN;
    
    if (vtp && call && interval) {
        if (strcmp (vtp, "exec") == 0) t = PROBE_EXEC;
        else if (strcmp (vtp, "nagios") == 0) t = PROBE_NAGIOS;
        if (probelist_add (&APP.probes, t, call, id, interval, opt)) {
            return 1;
        }
        else {
            log_error ("Error adding probe-call <%s>", call);
        }
    }
    return 0;
}

/** Handle --foreground */
int set_foreground (const char *i, const char *v) {
    APP.foreground = 1;
    return 1;
}

/** Handle --config-path */
int set_confpath (const char *i, const char *v) {
    APP.confpath = v;
    return 1;
}

int set_defaultspath (const char *i, const char *v) {
    APP.defaultspath = v;
    return 1;
}

/** Handle --pidfile */
int set_pidfile (const char *i, const char *v) {
    APP.pidfile = v;
    return 1;
}

/** Handle --logfile */
int set_logpath (const char *i, const char *v) {
    APP.logpath = v;
    return 1;
}

/** Handle --dump-packets */
int set_dumppath (const char *i, const char *v) {
    if (*v) APP.dumppath = v;
    return 1;
}

/** Handle --loglevel */
int set_loglevel (const char *i, const char *v) {
    if (strcmp (v, "CRIT") == 0) APP.loglevel = LOG_CRIT;
    else if (strcmp (v, "ERR") == 0) APP.loglevel = LOG_ERR;
    else if (strcmp (v, "WARNING") == 0) APP.loglevel = LOG_WARNING;
    else if (strcmp (v, "INFO") == 0) APP.loglevel = LOG_INFO;
    else if (strcmp (v, "DEBUG") == 0) APP.loglevel = LOG_DEBUG;
    else APP.loglevel = LOG_WARNING;
    return 1;
}

/** Command line options */
cliopt CLIOPT[] = {
    {
        "--foreground",
        "-f",
        OPT_FLAG,
        NULL,
        set_foreground
    },
    {
        "--pidfile",
        "-p",
        OPT_VALUE,
        "/var/run/opticon-agent.pid",
        set_pidfile
    },
    {
        "--logfile",
        "-l",
        OPT_VALUE,
        "@syslog",
        set_logpath
    },
    {
        "--loglevel",
        "-L",
        OPT_VALUE,
        "INFO",
        set_loglevel
    },
    {
        "--dump-packets",
        "-D",
        OPT_VALUE,
        "",
        set_dumppath
    },
    {
        "--config-path",
        "-c",
        OPT_VALUE,
        "/etc/opticon/opticon-agent.conf",
        set_confpath
    },
    {
        "--defaults-path",
        "-d",
        OPT_VALUE,
        "/etc/opticon/opticon-defaultprobes.conf",
        set_defaultspath
    },
    {NULL,NULL,0,NULL,NULL}
};

/** Application main. Handles configuration and command line flags,
  * then daemonizes.
  */
int main (int _argc, const char *_argv[]) {
    int argc = _argc;
    const char **argv = cliopt_dispatch (CLIOPT, _argv, &argc);
    if (! argv) return 1;
    
#if defined (OS_LINUX)    
    char buffer[1024];
    FILE *F = fopen ("/sys/class/dmi/id/product_uuid","r");
    if (F) {
        fgets (buffer, 1023, F);
        APP.hostid = mkuuid (buffer);
        fclose (F);
    }
    else {
        log_warn ("Error loading product_uuid: %s", strerror (errno));
    }
#elif defined (OS_DARWIN)
    char buffer[1024];
    FILE *F = popen_safe ("ioreg -d2 -c IOPlatformExpertDevice | "
                          "awk -F\\\" '/IOPlatformUUID/{print $(NF-1)}'","r");
    if (F) {
        fgets (buffer, 1023, F);
        APP.hostid = mkuuid (buffer);
        pclose_safe (F);
    }
#endif
    
    opticonf_add_reaction ("collector", conf_collector);
    opticonf_add_reaction ("probes/*", conf_probe);
    
    APP.codec = codec_create_pkt();
    APP.conf = var_alloc();

    probelist_init (&APP.probes);
    collectorlist_init (&APP.collectors);
    
    /* For now, we just create one instance */
    (void) collector_new (&APP.collectors);

    if (! var_load_json (APP.conf, APP.confpath)) {
        log_error ("Error loading %s: %s\n", APP.confpath, parse_error());
        return 1;
    }
    
    /** Override default probes where needed */
    var *default_probes = var_alloc();
    var *conf_probes = var_get_dict_forkey (APP.conf, "probes");
    if (var_load_json (default_probes, APP.defaultspath)) {
        var *crsr = var_first (default_probes);
        while (crsr) {
            var *existing = var_find_key (conf_probes, crsr->id);
            if (! existing) {
                var *copy = var_get_dict_forkey (conf_probes, crsr->id);
                var_copy (copy, crsr);
            }
            else {
                var *cc = var_first (crsr);
                while (cc) {
                    var *ee = var_find_key (existing, cc->id);
                    if (! ee) {
                        if (cc->type == VAR_INT) {
                            var_set_int_forkey (existing, cc->id, var_get_int(cc));
                        }
                        else if (cc->type == VAR_STR) {
                            var_set_str_forkey (existing, cc->id, var_get_str(cc));
                        }
                        else if (cc->type == VAR_DICT) {
                            ee = var_get_dict_forkey (existing, cc->id);
                            var_copy (ee, cc);
                        }
                    }
                    cc = cc->next;
                }
            }
            crsr = crsr->next;
        }
    }
    
    opticonf_handle_config (APP.conf);

    if (! uuidvalid (APP.hostid)) {
        log_error ("No hostid configured and could not auto-recognise");
        return 1;
    }
    
    if (! daemonize (APP.pidfile, argc, argv, daemon_main, APP.foreground)) {
        log_error ("Error spawning");
        return 1;
    }
    
    return 0;
}
