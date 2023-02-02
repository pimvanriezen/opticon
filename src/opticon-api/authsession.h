#ifndef _AUTHSESSION_H
#define _AUTHSESSION_H 1

#include <libopticon/uuid.h>
#include <libopticon/hash.h>
#include <pthread.h>
#include "req_context.h"

typedef struct authsession_s {
    struct authsession_s    *next;
    struct authsession_s    *prev;
    uint32_t                 hash;
    uuid                     id;
    uuid                     tenant;
    auth_level               userlevel;
    time_t                   heartbeat;
} authsession;

typedef struct authsessionlist_s {
    authsession         *first;
    authsession         *last;
    pthread_mutex_t      lock;
} authsessionlist;

extern authsessionlist SESSIONS;

#define SESSION_IDLE_TIMEOUT 600

void authsession_init (void);
uuid authsession_create (uuid tenant, auth_level level);
authsession *authsession_find (uuid id);

#endif
