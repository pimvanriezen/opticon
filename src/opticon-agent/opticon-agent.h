#ifndef _OPTICON_AGENT_H
#define _OPTICON_AGENT_H 1

#include <libopticon/datatypes.h>
#include <libopticon/codec.h>
#include <libopticon/auth.h>
#include <libopticon/pktwrap.h>
#include <libopticon/var.h>
#include <libopticon/transport.h>
#include <libopticon/thread.h>
#include <libopticon/packetqueue.h>

#include <pthread.h>

/* =============================== TYPES =============================== */

/** Types of metering probes */
typedef enum probetype_e {
    PROBE_NONE, /**< Uninitialized */
    PROBE_BUILTIN, /**< One of the compiled-in probes */
    PROBE_EXEC, /**< An external process */
    PROBE_NAGIOS /**< A nagios plugin script */
} probetype;

struct probe_s; /**< Forward declaration */

typedef var *(*probefunc_f)(struct probe_s *);

/** Represents a probe */
typedef struct probe_s {
    thread           thr; /**< Underlying thread */
    conditional      pulse; /**< Trigger to ask probe to run once */
    probetype        type; /**< The type */
    const char      *call; /**< String representation of probe call */
    const char      *id; /**< The probe name/id string */
    probefunc_f      func; /**< Function that performs one probe */
    struct probe_s  *prev; /**< Link neighbour */
    struct probe_s  *next; /**< Link neighbour */
    volatile var    *vcurrent; /**< Current value */
    volatile var    *vold; /**< Previous value */
    pthread_mutex_t  vlock; /**< Lock on the value */
    time_t           lastpulse; /**< Last trigger time */
    volatile time_t  lastreply; /**< Last update time */
    time_t           lastdispatch; /**< Last time it was dispatched */
    int              interval; /**< Configured time interval */
    var             *options; /**< Probe-specific options, if any */
} probe;

/** List header for a collection of probe objects */
typedef struct probelist_s {
    probe           *first;
    probe           *last;
} probelist;

/** Static mapping of built-in probe names to their functions */
typedef struct builtinfunc_s {
    const char      *name;
    probefunc_f      func;
} builtinfunc;

/** Thread used for resending a random subset of authentication packets */
typedef struct authresender_s {
    thread           super;
    conditional     *cond;
    outtransport    *trans;
    pktbuf           buf;
} authresender;

typedef struct collector_s {
    struct collector_s  *prev;
    struct collector_s  *next;
    int                  port;
    const char          *addr;
    authinfo             auth;
    time_t               lastkeyrotate;
    outtransport        *transport;
    authresender        *resender;
} collector;

typedef struct collectorlist_s {
    collector           *first;
    collector           *last;
} collectorlist;

/** Useful access to application parts and configuration */
typedef struct appcontext_s {
    codec           *codec;
    probelist        probes;
    var             *conf;
    int              loglevel;
    const char      *logpath;
    const char      *dumppath;
    const char      *confpath;
    const char      *defaultspath;
    const char      *pidfile;
    int              foreground;
    uuid             hostid;
    collectorlist    collectors;
    int              showversionflag;
    struct win_s {
        const char      *servicecommand;
        const char      *updatecommand;
        const char      *updateurl;
        const char      *updatechannel;
        bool             updateenabled;
    } win;
} appcontext;

/* ============================== GLOBALS ============================== */

extern builtinfunc BUILTINS[]; /**< NULL-terminated map of built-in probes */
extern appcontext APP; /**< The keep-it-all-together blob */

/* ============================= FUNCTIONS ============================= */

probe           *probe_alloc (void);
void             probelist_init (probelist *);
int              probelist_add (probelist *, probetype, const char *,
                                const char *, int, var *);
void             probelist_start (probelist *);
void             probelist_cancel (probelist *);
authresender    *authresender_create (outtransport *);
void             authresender_schedule (authresender *, const void *, size_t);

bool             matchlist (const char *, var *);

collector       *collector_new (collectorlist *);
void             collectorlist_init (collectorlist *);
void             collectorlist_add_host (collectorlist *, var *);
void             collectorlist_start (collectorlist *);

#endif
