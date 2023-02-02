#include "authsession.h"

authsessionlist SESSIONS;

void authsession_init (void) {
    SESSIONS.first = SESSIONS.last = NULL;
    pthread_mutex_init (&SESSIONS.lock, NULL);
}

uuid authsession_create (uuid tenant, auth_level userlevel) {
    uuid resid = uuidgen();
    authsession *s = malloc (sizeof (authsession));
    s->next = NULL;
    s->id = resid;
    s->hash = hash_uuid (resid);
    s->tenant = tenant;
    s->userlevel = userlevel;
    s->heartbeat = time (NULL);
    
    pthread_mutex_lock (&SESSIONS.lock);
    
    if (SESSIONS.first) {
        s->prev = SESSIONS.last;
        s->prev->next = s;
        SESSIONS.last = s;
    }
    else {
        s->prev = NULL;
        SESSIONS.first = SESSIONS.last = s;
    }
    
    pthread_mutex_unlock (&SESSIONS.lock);
    return resid;
}

authsession *authsession_find (uuid id) {
    uint32_t hash = hash_uuid (id);
    time_t now = time (NULL);
    
    pthread_mutex_lock (&SESSIONS.lock);
    authsession *crsr = SESSIONS.first;
    while (crsr) {
        if (crsr->hash == hash) {
            if (uuidcmp (crsr->id, id)) {
                if (now - crsr->heartbeat < SESSION_IDLE_TIMEOUT) {
                    crsr->heartbeat = now;
                    pthread_mutex_unlock (&SESSIONS.lock);
                    return crsr;
                }
                if (now - crsr->heartbeat > (2*SESSION_IDLE_TIMEOUT)) {
                    if (crsr->prev) {
                        crsr->prev->next = crsr->next;
                        if (crsr->next) {
                            crsr->next->prev = crsr->prev;
                        }
                        else {
                            SESSIONS.last = crsr->prev;
                        }
                    }
                    else {
                        SESSIONS.first = crsr->next;
                        if (crsr->next) {
                            crsr->next->prev = NULL;
                        }
                        else {
                            SESSIONS.last = NULL;
                        }
                    }
                    free (crsr);
                }
                
                break;
            }
        }
        crsr = crsr->next;
    }
    
    pthread_mutex_unlock (&SESSIONS.lock);
    return NULL;
}
