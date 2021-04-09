#ifndef _OPTICON_PING_H
#define _OPTICON_PING_H 1

typedef struct pingtarget_s {
    struct pingtarget_s     *next;
    struct pingtarget_s     *prev;
    uint32_t                 id;
    uint32_t                 wpos;
    uint32_t                 readers;
    uint32_t                 sequence; /** Is 0 if no outstanding pings */
    time_t                   lastseen;
    double                   data[16]; /** Aim at 1 ping every 20s */
    struct sockaddr_storage  remote;
} pingtarget;

typedef struct pingtargetlist_s {
    pthread_mutex_t          mutex;
    pingtarget              *first;
    pingtarget              *last;
} pingtargetlist;

extern pingtargetlist TARGETS;

void pingthread_run (thread *self);
pingtarget *pingtarget_open (struct sockaddr_storage &remote);
double pingtarget_get_rtt (pingtarget *self);
double pingtarget_get_loss (pingtarget *self);
void pingtarget_write (pingtarget *self, double rtt);
void pingtarget_write_loss (pingtarget *self);
void pingtarget_close (pingtarget *);

#endif
