#ifndef _OPTICON_PING_H
#define _OPTICON_PING_H 1

#include <libopticon/thread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>

struct pingtargetlist_s;

#define  PKTSIZE    64 
#define  HDRLEN     ICMP_MINLEN
#define  DATALEN    (PKTSIZE-HDRLEN)

typedef struct pingtarget_s {
    struct pingtarget_s     *next;
    struct pingtarget_s     *prev;
    struct pingtargetlist_s *parent;
    uint32_t                 id;
    uint32_t                 wpos;
    uint32_t                 users; /** Counter to prevent untimely
                                        deallocation */
    uint32_t                 sequence; /** Is 0 if no outstanding pings */
    time_t                   lastseen;
    double                   data[16]; /** Aim at 1 ping every 20s */
    struct timeval           tsent;
    struct sockaddr_storage  remote;
} pingtarget;

typedef struct pingtargetlist_s {
    pthread_mutex_t          mutex;
    uint32_t                 count;
    pingtarget              *first;
    pingtarget              *last;
} pingtargetlist;

typedef struct pingstate_s {
    pthread_mutex_t          mutex;
    int                      icmp;
    int                      icmp6;
    uint32_t                 sequence;
    pingtargetlist           v4;
    pingtargetlist           v6;
} pingstate;

extern pingstate PINGSTATE;

void ping_init (void);
void ping_start (void);
void ping_run_sender_v4 (thread *self);
void ping_run_sender_v6 (thread *self);
void ping_run_receiver_v4 (thread *self);
void ping_run_receiver_v6 (thread *self);

void pingtargetlist_init (pingtargetlist *);
pingtarget *pingtargetlist_get (pingtargetlist *, struct sockaddr_storage *);
struct sockaddr_storage *pingtargetlist_all (pingtargetlist *, uint32_t *);
void pingtargetlist_release (pingtargetlist *, pingtarget *);

uint32_t pingtarget_makeid (struct sockaddr_storage *addr);
pingtarget *pingtarget_open (struct sockaddr_storage *addr);
pingtarget *pingtarget_create (struct sockaddr_storage *addr);
double pingtarget_get_rtt (pingtarget *self);
double pingtarget_get_loss (pingtarget *self);
void pingtarget_write (pingtarget *self, double rtt);
void pingtarget_write_loss (pingtarget *self);
void pingtarget_close (pingtarget *);

#endif
