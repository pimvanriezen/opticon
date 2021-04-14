#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include <libopticon/util.h>
#include <libopticon/auth.h>
#include <libopticon/base64.h>
#include <libopticon/log.h>
#include <libopticon/var.h>
#include <arpa/inet.h>

sessiondb SESSIONS;

/*/ ======================================================================= /*/
/** Initialize session-related global storage. */
/*/ ======================================================================= /*/
void sessiondb_init (void) {
    pthread_mutex_init (&SESSIONS.lock, NULL);
    for (int i=0; i<256; ++i) {
        SESSIONS.s[i].first = SESSIONS.s[i].last = NULL;
    }
}

/*/ ======================================================================= /*/
/** Encode the sessionlist ready for JSON export */
/*/ ======================================================================= /*/
var *sessiondb_save (void) {
    var *v_root = var_alloc();
    var *v_session = var_get_array_forkey (v_root, "session");
    session *crsr;
    pthread_mutex_lock (&SESSIONS.lock);
    for (int i=0; i<256; ++i) {
        crsr = SESSIONS.s[i].first;
        while (crsr) {
            if (crsr->expired) {
                crsr = crsr->next;
                continue;
            }
            var *v_sess = var_add_dict (v_session);
            var_set_uuid_forkey (v_sess, "tenantid", crsr->tenantid);
            var_set_uuid_forkey (v_sess, "hostid", crsr->hostid);
            var_set_int_forkey (v_sess, "addr", (uint64_t) crsr->addr);
            var_set_int_forkey (v_sess, "sessid", (uint64_t) crsr->sessid);
            var_set_time_forkey (v_sess, "lastcycle", crsr->lastcycle);
            var_set_int_forkey (v_sess, "lastserial", crsr->lastserial);
            char *strkey = aeskey_to_base64 (crsr->key);
            var_set_str_forkey (v_sess, "key", strkey);
            free (strkey);
            char straddr[INET6_ADDRSTRLEN];
            ip2str (&crsr->remote, straddr);
            var_set_str_forkey (v_sess, "remote", straddr);
            crsr = crsr->next;
        }
    }
    pthread_mutex_unlock (&SESSIONS.lock);
    return v_root;
}

/*/ ======================================================================= /*/
/** Invalidate all sessions for a given tenant id */
/*/ ======================================================================= /*/
void sessiondb_remove_tenant (uuid tenantid) {
    session *crsr;
    pthread_mutex_lock (&SESSIONS.lock);
    for (int i=0; i<256; ++i) {
        crsr = SESSIONS.s[i].first;
        while (crsr) {
            if (uuidcmp (crsr->tenantid, tenantid)) crsr->expired = true;
            crsr = crsr->next;
        }
    }
    pthread_mutex_unlock (&SESSIONS.lock);
}

/*/ ======================================================================= /*/
/** Restore the sessionlist from imported JSON */
/*/ ======================================================================= /*/
void sessiondb_restore (var *list) {
    var *v_session = var_get_array_forkey (list, "session");
    var *crsr = v_session->value.arr.first;
    while (crsr) {
        session *s = session_alloc();
        s->tenantid = var_get_uuid_forkey (crsr, "tenantid");
        s->hostid = var_get_uuid_forkey (crsr, "hostid");
        s->addr = (uint32_t) (var_get_int_forkey (crsr, "addr") & 0xffffffff);
        s->sessid = (uint32_t) (var_get_int_forkey (crsr, "sessid") & 0xffffffff);
        s->lastcycle = var_get_time_forkey (crsr, "lastcycle");
        s->lastserial = (uint32_t) var_get_int_forkey (crsr, "lastserial");
        s->key = aeskey_from_base64 (var_get_str_forkey (crsr, "key"));
        str2ip (var_get_str_forkey (crsr, "remote"), &s->remote);
        host *h = host_find (s->tenantid, s->hostid);
        if (! h) { free(s); continue; }
        s->host = h;
        session_link (s);
        crsr = crsr->next;
    }
}

/*/ ======================================================================= /*/
/** Allocate and initialize session object. */
/*/ ======================================================================= /*/
session *session_alloc (void) {
    session *res = (session *) malloc (sizeof (session));
    res->next = res->prev = NULL;
    res->addr = res->sessid = res->lastserial = 0;
    bzero (res->key.data, 8);
    res->lastcycle = time (NULL);
    res->host = NULL;
    res->expired = false;
    return res;
}

/*/ ======================================================================= /*/
/** Link a new session into the proper bucket. */
/*/ ======================================================================= /*/
void session_link (session *s) {
    uint32_t sid = s->sessid ^ s->addr;
    uint8_t bucket = (sid >> 24) ^
                     ((sid & 0x00ff0000) >> 16) ^
                     ((sid & 0x0000ff00) >> 8) ^
                     (sid & 0xff);
    pthread_mutex_lock (&SESSIONS.lock);
    if (SESSIONS.s[bucket].first) {
        // link in at top.
        s->next = SESSIONS.s[bucket].first;
        SESSIONS.s[bucket].first->prev = s;
        SESSIONS.s[bucket].first = s;
    }
    else {
        SESSIONS.s[bucket].first = SESSIONS.s[bucket].last = s;
    }
    pthread_mutex_unlock (&SESSIONS.lock);
}

/*/ ======================================================================= /*/
/** Register a new session (or update an existing session to
  * stop it from expiring. */
/*/ ======================================================================= /*/
session *session_register (uuid tenantid, uuid hostid,
                           uint32_t addrpart, uint32_t sess_id,
                           aeskey sess_key, struct sockaddr_storage *ss) {
    host *h = host_find (tenantid, hostid);
    session *s;
    if (! h) return NULL;

    if ((s = session_find (addrpart, sess_id))) {
        s->lastcycle = time (NULL);
        s->key = sess_key;
        return s;
    }
        
    s = session_alloc();
    if (! s) return NULL;
    
    s->tenantid = tenantid;
    s->hostid = hostid;
    s->host = h;
    s->addr = addrpart;
    s->sessid = sess_id;
    s->key = sess_key;
    s->lastcycle = time (NULL);
    memcpy (&s->remote, ss, sizeof (struct sockaddr_storage));
    
    session_link (s);
    return s;
}

/*/ ======================================================================= /*/
/** Look up a session by 32 bits worth of IP address data and a 32 bits
    session-id. */
/*/ ======================================================================= /*/
session *session_find (uint32_t addrpart, uint32_t sess_id) {
    uint32_t sid = sess_id ^ addrpart;
    uint8_t bucket = (sid >> 24) ^
                     ((sid & 0x00ff0000) >> 16) ^
                     ((sid & 0x0000ff00) >> 8) ^
                     (sid & 0xff);
    pthread_mutex_lock (&SESSIONS.lock);
    session *s = SESSIONS.s[bucket].first;
    while (s) {
        if (s->addr == addrpart && s->sessid == sess_id) {
            if (! s->expired) {
                pthread_mutex_unlock (&SESSIONS.lock);
                return s;
            }
        }
        s = s->next;
    }
    pthread_mutex_unlock (&SESSIONS.lock);
    return NULL;
}

/*/ ======================================================================= /*/
/** Expire sessions that have been last updated before a given
    cut-off point. */
/*/ ======================================================================= /*/
void session_expire (time_t cutoff) {
    session *s, *ns;
    pthread_mutex_lock (&SESSIONS.lock);
    for (int i=0; i<=255; ++i) {
        s = SESSIONS.s[i].first;
        while (s) {
            ns = s->next;
            if ((s->expired) || (s->lastcycle < cutoff)) {
                if (! s->expired) {
                    s->expired = true;
                    continue;
                }
                log_info ("Expiring session %08x-%08x",s->sessid,s->addr);
                if (s->prev) {
                    if (s->next) {
                        s->prev->next = s->next;
                        s->next->prev = s->prev;
                    }
                    else {
                        s->prev->next = NULL;
                        SESSIONS.s[i].last = s->prev;
                    }
                }
                else {
                    if (s->next) {
                        s->next->prev = NULL;
                        SESSIONS.s[i].first = s->next;
                    }
                    else {
                        SESSIONS.s[i].first = SESSIONS.s[i].last = NULL;
                    }
                }
                
                free (s);
            }
            s = ns;
        }
    }
    pthread_mutex_unlock (&SESSIONS.lock);
}

/*/ ======================================================================= /*/
/** Dump information about a session into a filedescriptor. */
/*/ ======================================================================= /*/
void session_print (session *s, ioport *into) {
    char buf[256];
    char *keystr;
    char stenantid[48], shostid[48];
    uuid2str (s->tenantid, stenantid);
    uuid2str (s->hostid, shostid);
    keystr = base64_encode ((char *) s->key.data, 32, NULL);
    
    sprintf (buf, "Session %08x\n"
                  "Tenant %s\n"
                  "Host %s\n"
                  "Key %s\n",
                  s->sessid,
                  stenantid,
                  shostid,
                  keystr);
    
    free (keystr);
    ioport_write (into, buf, strlen (buf));
}
