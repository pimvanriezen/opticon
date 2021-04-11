#include <libopticon/log.h>
#include <libopticon/util.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ping.h"

pingstate PINGSTATE;

/*/ ======================================================================= /*/
/** Opens a raw socket for ICMP datagrams */
/*/ ======================================================================= /*/
int ping_open_icmp (void) {
    int fd;
    struct protoent *proto;

    if ((proto = getprotobyname("icmp")) == NULL) {
        log_error ("Could not resolve protocol: icmp");
        exit (1);
    }
    
    fd = socket (AF_INET, SOCK_RAW, proto->p_proto);
    if (fd < 0) {
        log_error ("Could not open icmp raw socket");
        exit (1);
    }
    return fd;
}
/*/ ======================================================================= /*/
/** Opens a raw socket for ICMPv6 datagrams */
/*/ ======================================================================= /*/
int ping_open_icmp6 (void) {
    return -1;
}

/*/ ======================================================================= /*/
/** Generates a new sequence number */
/*/ ======================================================================= /*/
uint32_t ping_sequence (void) {
    uint32_t res;
    pthread_mutex_lock (&PINGSTATE.mutex);
    res = PINGSTATE.sequence++;
    pthread_mutex_unlock (&PINGSTATE.mutex);
    return res;
}

/*/ ======================================================================= /*/
/** Take a micronap */
/*/ ======================================================================= /*/
void ping_msleep (uint32_t msec) {
    struct timeval tv;
    int sec, usec;

    sec = msec / 1000;
    usec = 1000 * (msec - (1000 * sec));

    tv.tv_sec = sec;
    tv.tv_usec = usec;

    select (0, NULL, NULL, NULL, &tv);
}

/*/ ======================================================================= /*/
/** Calculates checksum for an icmp packet */
/*/ ======================================================================= /*/
unsigned short ping_checksum (void *buf, int len) {
    uint32_t sum = 0;
    uint16_t *buf_w = (uint16_t *) buf;
    uint8_t *buf_b = (uint8_t *) buf;
    
    while (len>1) {
        sum += *buf_w++;
        len -= 2;
    }
    
    if (len==1) {
        buf_b = (uint8_t *) buf_w;
        sum += *buf_b;
    }
    
    sum = (sum>>16) + (sum &0xffff);
    sum += (sum>>16);
    return ~sum;
}

/*/ ======================================================================= /*/
/** Compares two addresses */
/*/ ======================================================================= /*/
int ping_compare_addr (struct sockaddr_storage *left,
                       struct sockaddr_storage *right) {
    void *target = NULL;
    size_t sz = 0;
    if (!left || !right) return 0;
    
    struct sockaddr_in6 *left6 = (struct sockaddr_in6 *) left;
    struct sockaddr_in6 *right6 = (struct sockaddr_in6 *) right;
    struct sockaddr_in *left4 = (struct sockaddr_in *) left;
    struct sockaddr_in *right4 = (struct sockaddr_in *) right;
    
    if (left->ss_family != right->ss_family) return 0;
    switch (left->ss_family) {
        case AF_INET:
            if (memcmp (&left4->sin_addr,&right4->sin_addr, 4) == 0) {
                return 1;
            }
            return 0;
        
        case AF_INET6:
            if (memcmp (&left6->sin6_addr,&right6->sin6_addr, 16) == 0) {
                return 1;
            }
            return 0;
        
        default:
            return 0;
    }
}

/*/ ======================================================================= /*/
/** Initializes data structures, and opens any resources that need to
    be accessed as root prior to daemonize() */
/*/ ======================================================================= /*/
void ping_init (void) {
    pthread_mutex_init (&PINGSTATE.mutex, NULL);
    PINGSTATE.icmp = ping_open_icmp();
    PINGSTATE.icmp6 = ping_open_icmp6();
    PINGSTATE.sequence = (uint32_t) time(NULL);
    
    pingtargetlist_init (&PINGSTATE.v4);
    pingtargetlist_init (&PINGSTATE.v6);
}

/*/ ======================================================================= /*/
/** Starts all ping-related threads */
/*/ ======================================================================= /*/
void ping_start (void) {
    thread_create (ping_run_sender_v4, NULL);
    thread_create (ping_run_sender_v6, NULL);
    thread_create (ping_run_receiver_v4, NULL);
    thread_create (ping_run_receiver_v6, NULL);
}

/*/ ======================================================================= /*/
/** Convenience function for resolving an address to an rtt result */
/*/ ======================================================================= /*/
double ping_get_rtt (struct sockaddr_storage *addr) {
    double res = 0.0;
    pingtarget *tgt = pingtarget_open (addr);
    if (tgt) {
        res = pingtarget_get_rtt (tgt);
        pingtarget_close (tgt);
    }
    return res;
}

/*/ ======================================================================= /*/
/** Convenience function for resolving an address to a loss result */
/*/ ======================================================================= /*/
double ping_get_loss (struct sockaddr_storage *addr) {
    double res = 0.0;
    pingtarget *tgt = pingtarget_open (addr);
    if (tgt) {
        res = pingtarget_get_loss (tgt);
        pingtarget_close (tgt);
    }
    return res;
}

/*/ ======================================================================= /*/
/** Thread that sends a ping for every v4 host in the list every 20
    seconds */
/*/ ======================================================================= /*/
void ping_run_sender_v4 (thread *self) {
    struct sockaddr_storage *list;
    char buf[PKTSIZE];
    struct icmp *icp = (struct icmp *) buf;
    const char *fillstr = "o6pingv4";
    uint32_t count;
    uint32_t i;
    uint32_t seq;
    
    for (i=HDRLEN;i<PKTSIZE;++i) {
        buf[i] = fillstr[(i-HDRLEN)&7];
    }
    
    while (1) {
        list = pingtargetlist_all (&PINGSTATE.v4, &count);
        for (i=0; i<count; ++i) {
            pingtarget *tgt = pingtarget_open (list+i);
            if (tgt) {
                seq = ping_sequence();
                if (tgt->sequence) {
                    /* No ping reply for our last sequence has been sent,
                       so we register that as a lost packet */
                    pingtarget_write_loss (tgt);
                    log_debug ("ping: Set loss for %08x\n", tgt->id);
                }
                tgt->sequence = seq;
                gettimeofday (&tgt->tsent, NULL);

                icp->icmp_type = ICMP_ECHO;
                icp->icmp_code = 0;
                icp->icmp_cksum = 0;
                icp->icmp_id = (seq & 0xffff0000) >> 16;
                icp->icmp_seq = (seq & 0x0000ffff);
                icp->icmp_cksum = ping_checksum (icp, PKTSIZE);        
            
                size_t res = sendto (PINGSTATE.icmp, buf, PKTSIZE, 0,
                                     (struct sockaddr *) list+i,
                                     sizeof (struct sockaddr_in));
                                 
                log_debug ("ping: Sent ping to %08x\n", tgt->id);
                pingtarget_close (tgt);
            }
            ping_msleep (20000 / count);
        }
        if (! count) sleep (5);
        free (list);
    }
}

/*/ ======================================================================= /*/
/** Thread that sends a ping for every v6 host in the list every 20
    seconds */
/*/ ======================================================================= /*/
void ping_run_sender_v6 (thread *self) {
    while (1) sleep (60);
}

/*/ ======================================================================= /*/
/** Calculates difference in milliseconds between two timevals. */
/*/ ======================================================================= /*/
double ping_diff (struct timeval *then, struct timeval *now) {
    int secdiff = now->tv_sec - then->tv_sec;
    int usecdiff = now->tv_usec - then->tv_usec;
    if (secdiff < 0) return 0.0;
    if (secdiff == 0 && usecdiff < 0) return 0.0;
    double res = (1000.0 * secdiff);
    res += usecdiff / 1000.0;
    return res;
}

/*/ ======================================================================= /*/
/** Thread that receives and processes ping replies from v4 hosts. */
/*/ ======================================================================= /*/
void ping_run_receiver_v4 (thread *self) {
    uint8_t buf[1500];
    struct icmp *icp;
    struct ip *ip;
    struct timeval tv;
    struct sockaddr_storage faddr;
    uint32_t in_seq;
    int rsz;
    int hlen;
    
    faddr.ss_family = AF_INET;
    
    while (1) {
        socklen_t fromlen = sizeof (struct sockaddr_storage);
        rsz = recvfrom (PINGSTATE.icmp, (char*)buf, 1500, 0,
                        (struct sockaddr *)&faddr, &fromlen);
        gettimeofday (&tv, NULL);
        
        ip = (struct ip *) buf;
        hlen = ip->ip_hl << 2;
        icp = (struct icmp *)(buf + hlen);
        
        pingtarget *tgt = pingtarget_open (&faddr);
        if (tgt) {
            log_debug ("ping: Got reply for %08x", tgt->id);
            in_seq = icp->icmp_seq;
            if ((tgt->sequence & 0xffff) == in_seq) {
                tgt->sequence = 0;
                double delta = ping_diff (&tgt->tsent, &tv);
                pingtarget_write (tgt, delta);
            }
            pingtarget_close (tgt);
        }
    }
}

/*/ ======================================================================= /*/
/** Thread that receives and processes ping replies from v6 hosts. */
/*/ ======================================================================= /*/
void ping_run_receiver_v6 (thread *self) {
    while (1) sleep (60);
}

/*/ ======================================================================= /*/
/** Initializes a preallocated pingtargetlist. */
/*/ ======================================================================= /*/
void pingtargetlist_init (pingtargetlist *self) {
    pthread_mutex_init (&self->mutex, NULL);
    self->count = 0;
    self->first = self->last = NULL;
}

/*/ ======================================================================= /*/
/** Returns an array of all addresses on the list that should be pinged */
/*/ ======================================================================= /*/
struct sockaddr_storage *pingtargetlist_all (pingtargetlist *self,
                                             uint32_t *count) {
    struct sockaddr_storage *res;
    uint32_t i = 0;
    pingtarget *crsr;
    
    pthread_mutex_lock (&self->mutex);
    *count = self->count;
    res = calloc (*count, sizeof (struct sockaddr_storage));
    crsr = self->first;
    while (crsr && i<*count) {
        memcpy (res+i, &crsr->remote, sizeof (struct sockaddr_storage));
        
        char buf[INET6_ADDRSTRLEN];
        ip2str (&crsr->remote, buf);
        log_debug ("ping: add list: %08x %s", crsr->id, buf);
        
        i++;
        crsr = crsr->next;
    }
    pthread_mutex_unlock (&self->mutex);
    return res;
}

/*/ ======================================================================= /*/
/** Unreserves a pingtarget, allowing it potentially to be deallocated */
/*/ ======================================================================= /*/
void pingtargetlist_release (pingtargetlist *self, pingtarget *tgt) {
    pthread_mutex_lock (&self->mutex);
    tgt->users--;
    pthread_mutex_unlock (&self->mutex);
}

/*/ ======================================================================= /*/
/** Gets a target out of a targetlist with a specific address. If no target
    with the address exists, it is created. */
/*/ ======================================================================= /*/
pingtarget *pingtargetlist_get (pingtargetlist *self,
                                struct sockaddr_storage *addr) {
    if (! addr->ss_family) {
        log_debug ("pingtargetlist_get: no family");
        return NULL'
    }
    uint32_t id = pingtarget_makeid (addr);
    pthread_mutex_lock (&self->mutex);
    pingtarget *crsr = self->first;
    while (crsr) {
        if (crsr->id == id) {
            if (ping_compare_addr (&crsr->remote, addr)) {
                crsr->users++;
                pthread_mutex_unlock (&self->mutex);
                return crsr;
            }
        }
        crsr = crsr->next;
    }
    
    crsr = pingtarget_create (addr);
    crsr->parent = self;
    
    if (self->last) {
        crsr->prev = self->last;
        self->last = crsr;
    }
    else {
        self->first = self->last = crsr;
    }
    
    self->count++;
    pthread_mutex_unlock (&self->mutex);
    return crsr;
}

/*/ ======================================================================= /*/
/** Turns an IPv4 or IPv6 address into a 32bit id for lookups */
/*/ ======================================================================= /*/
uint32_t pingtarget_makeid (struct sockaddr_storage *remote) {
    size_t sz = sizeof (struct sockaddr_storage);
    size_t offs;
    uint8_t *addr;
    uint8_t shifts[8] = {0,24,8,16,12,20,4,18};
    uint32_t res = 0;
    
    if (remote->ss_family == AF_INET) {
        sz = 4;
        addr = (uint8_t *) remote;
        addr += 4;
    }
    else if (remote->ss_family == AF_INET6) {
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) remote;
        addr = (uint8_t *) &in6->sin6_addr;
        sz = 16;
    }
    else {
        addr = (uint8_t *) remote;
    }
    
    for (offs=0; offs < sz; ++offs) {
        res ^= (uint32_t) addr[offs] << shifts[offs&7];
    }
    return res;
}

/*/ ======================================================================= /*/
/** Open a pingtarget from the right targetlist depending on
    address type. */
/*/ ======================================================================= /*/
pingtarget *pingtarget_open (struct sockaddr_storage *remote) {
    pingtargetlist *list = NULL;
    if (remote->ss_family == AF_INET) {
        list = &PINGSTATE.v4;
    }
    else if (remote->ss_family == AF_INET6) {
        list = &PINGSTATE.v6;
    }
    
    if (! list) {
        log_error ("Unknown target family: %02x", remote->ss_family);
        return NULL;
    }
    return pingtargetlist_get (list, remote);
}

/*/ ======================================================================= /*/
/** Allocates and initializes a pingtarget */
/*/ ======================================================================= /*/
pingtarget *pingtarget_create (struct sockaddr_storage *remote) {
    pingtarget *self = (pingtarget *) malloc (sizeof (pingtarget));
    self->next = self->prev = NULL;
    self->parent = NULL;
    self->id = pingtarget_makeid (remote);
    self->wpos = 0;
    self->users = 0;
    self->sequence = 0;
    self->lastseen = 0;
    for (int i=0; i<16; ++i) self->data[i] = 0.0;
    memcpy (&self->remote, remote, sizeof(struct sockaddr_storage));
    char addr[INET6_ADDRSTRLEN];
    ip2str (remote, addr);
    log_debug ("ping: Created target %08x (%s)", self->id, addr);
    return self;
}

/*/ ======================================================================= /*/
/** Calculates 5 minute rtt average from gathered data */
/*/ ======================================================================= /*/
double pingtarget_get_rtt (pingtarget *self) {
    uint8_t losscount = 0;
    uint8_t msrcount = 0;
    double total = 0.0;
    
    for (int i=0; i<16; ++i) {
        double c = self->data[i];
        if (c<0) losscount++;
        else {
            msrcount++;
            total += c;
        }
    }
    
    if (! msrcount) return 0.0;
    return (total / msrcount);
}

/*/ ======================================================================= /*/
/** Calculates 5 minute packet loss percentage from gathered data */
/*/ ======================================================================= /*/
double pingtarget_get_loss (pingtarget *self) {
    uint8_t losscount = 0;
    
    for (int i=0; i<16; ++i) {
        if (self->data[i]<0) losscount++;
    }
    
    return (100.0 * (losscount / 16.0));
}

/*/ ======================================================================= /*/
/** Releases an opened pingtarget */
/*/ ======================================================================= /*/
void pingtarget_close (pingtarget *self) {
    pingtargetlist_release (self->parent, self);
}

/*/ ======================================================================= /*/
/** Writes a new rtt time to the target */
/*/ ======================================================================= /*/
void pingtarget_write (pingtarget *self, double rtt) {
    self->data[self->wpos] = rtt;
    self->wpos = (self->wpos+1) & 15;
}

/*/ ======================================================================= /*/
/** Registers a lost reply packet for the target */
/*/ ======================================================================= /*/
void pingtarget_write_loss (pingtarget *self) {
    pingtarget_write (self, -1.0);
}
