#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <ctype.h>
#include <libopticon/log.h>
#include <libopticon/var.h>
#include <libopticon/thread.h>
#include <libopticon/popen.h>
#include <errno.h>
#include <stdint.h>
#include "tproc.h"
#include "wordlist.h"
#include "opticon-agent.h"

static int KMEMTOTAL = 1024;

/* ======================================================================= */
/* PROBE probe_net                                                         */
/* ======================================================================= */

/** Tallies */
static struct linuxprobe_net_info_s {
    time_t      lastrun;
    uint64_t    net_in_kbits;
    uint64_t    net_out_kbits;
    uint64_t    net_in_packets;
    uint64_t    net_out_packets;
} NETPROBE = {0,0,0,0,0};

/** Probe function */
var *runprobe_net (probe *self) {
    FILE *F;
    char buf[256];
    uint64_t totalin_kbits = 0;
    uint64_t totalin_packets = 0;
    uint64_t totalout_kbits = 0;
    uint64_t totalout_packets = 0;
    uint64_t diffin_kbits;
    uint64_t diffin_packets;
    uint64_t diffout_kbits;
    uint64_t diffout_packets;
    wordlist *args;
    char *colon;
    char *devstart;
    time_t ti;
    var *res = var_alloc();
    var *skipdevices = var_get_array_forkey (self->options, "skip");
    var *matchdevices = var_get_array_forkey (self->options, "match");
    
    F = fopen ("/proc/net/dev", "r");
    if (! F) {
        log_warn ("probe_net: Can't read /proc/net/dev: %s", strerror(errno));
        return res;
    }
    var *res_net = var_get_dict_forkey (res, "net");
    
    fgets (buf, 255, F); /* skip first line */
    while (! feof (F)) {
        *buf = 0;
        fgets (buf, 255, F);
        if (! *buf) continue;
        colon = strchr (buf, ':');
        if (! colon) continue;
        args = wordlist_make (colon);
        *colon = 0;
        devstart = buf;
        while (*devstart == ' ') devstart++;
        
        if (var_get_count (matchdevices)) {
            if (! matchlist (devstart, matchdevices)) {
                wordlist_free (args);
                continue;
            }
        }
        
        if (matchlist (devstart, skipdevices)) {
            wordlist_free (args);
            continue;
        }
        
        if (args->argc < 11) {
            wordlist_free (args);
            continue;
        }
        
        totalin_kbits += (strtoull (args->argv[1],NULL,10) >> 7);
        totalin_packets += (strtoull (args->argv[2],NULL,10));
        totalout_kbits += (strtoull (args->argv[9],NULL,10) >> 7);
        totalout_packets += (strtoull (args->argv[10],NULL,10));
        wordlist_free (args);
    }
    fclose (F);
    log_debug ("probe_net: Data read");
    ti = time (NULL);
    if (ti == NETPROBE.lastrun) NETPROBE.lastrun--;
    
    if (NETPROBE.lastrun) {
        diffin_kbits = totalin_kbits - NETPROBE.net_in_kbits;
        diffin_packets = totalin_packets - NETPROBE.net_in_packets;
        diffout_kbits = totalout_kbits - NETPROBE.net_out_kbits;
        diffout_packets = totalout_packets - NETPROBE.net_out_packets;
        uint64_t tdiff = (ti - NETPROBE.lastrun);
        log_debug ("probe_net: diffs: %" PRIu64 ", %" PRIu64 ", %"
                   PRIu64 ", %" PRIu64,
                   diffin_kbits, diffin_packets,
                   diffout_kbits, diffout_packets);
        var_set_int_forkey (res_net, "in_kbs", diffin_kbits / tdiff);
        var_set_int_forkey (res_net, "in_pps", diffin_packets / tdiff);
        var_set_int_forkey (res_net, "out_kbs", diffout_kbits / tdiff);
        var_set_int_forkey (res_net, "out_pps", diffout_packets / tdiff);
    }
    else {
        log_debug ("probe_net: Skipping because no lastrun");
    }
    NETPROBE.net_in_kbits = totalin_kbits;
    NETPROBE.net_out_kbits = totalout_kbits;
    NETPROBE.net_in_packets = totalin_packets;
    NETPROBE.net_out_packets = totalout_packets;
    NETPROBE.lastrun = ti;
    return res;
}

/* ======================================================================= */
/* PROBE probe_portinfo                                                    */
/* ======================================================================= */

typedef struct portinfo_s {
    struct portinfo_s   *next;
    struct portinfo_s   *prev;
    char                *devname;
    time_t               lastrun;
    uint64_t             in_bytes;
    uint64_t             out_bytes;
    uint32_t             in_kbps;
    uint32_t             out_kbps;
    bool                 up;
} portinfo;

typedef struct portlist_s {
    portinfo    *first;
    portinfo    *last;
} portlist;

static portlist PORTS = {NULL,NULL};

portinfo *portinfo_create (const char *devname) {
    portinfo *res = (portinfo *) malloc (sizeof (portinfo));
    res->next = res->prev = 0;
    res->in_bytes = res->out_bytes = 0;
    res->in_kbps = res->out_kbps = 0;
    res->up = false;
    res->devname = strdup (devname);
    res->lastrun = 0;
    return res;
}

/** Handles data for a specific interface. Updates the counters and
 *  calculates the rate.
 *  \param self The portinfo object to update
 *  \param up True if the associated interface is currently up.
 *  \param in Current input bytes counter for the interface.
 *  \param out Current output bytes counter for the interface.
 */
void portinfo_handle_data (portinfo *self, bool up, uint64_t in, uint64_t out) {
    time_t now = time (NULL);
    self->up = up;

    if ((! self->lastrun) || (self->lastrun >= now)) {
        self->in_bytes = in;
        self->out_bytes = out;
        self->lastrun = now;
        return;
    }
    
    uint64_t diff_in;
    uint64_t diff_out;
    int tdiff = now - self->lastrun;
    
    diff_in = in - self->in_bytes;
    diff_out = out - self->out_bytes;
    
    self->in_kbps = (diff_in/128) / tdiff;
    self->out_kbps = (diff_out/128) / tdiff;
    self->in_bytes = in;
    self->out_bytes = out;
    self->lastrun = now;
}

/** Locates a portinfo object in a portlist, or creates a new one
 *  if none exists with the given name.
 *  \param self The portlist object
 *  \param dev The requested device name.
 */
portinfo *portlist_get_port (portlist *self, const char *dev) {
    portinfo *res = self->first;
    while (res) {
        if (strcmp (res->devname, dev) == 0) return res;
        res = res->next;
    }
    
    res = portinfo_create (dev);
    if (! self->first) {
        self->first = self->last = res;
    }
    else {
        res->prev = self->last;
        self->last->next = res;
        self->last = res;
    }
    
    return res;
}

/** Probe implementation */
var *runprobe_portinfo (probe *self) {
    FILE *fproc;
    FILE *fsys;
    char buf[512];
    char sysbuf[512];
    wordlist *args;
    char *colon;
    char *devstart;
    portinfo *port;
    var *res = var_alloc();
    var *skipdevices = var_get_array_forkey (self->options, "skip");
    var *matchdevices = var_get_array_forkey (self->options, "match");
    
    fproc = fopen ("/proc/net/dev","r");
    if (! fproc) {
        log_warn ("probe_portinfo: Can't read /proc/net/dev: %s",
                  strerror (errno));
        return res;
    }
    
    fgets (buf, 511, fproc);
    while (! feof (fproc)) {
        *buf = 0;
        fgets (buf, 511, fproc);
        if (! *buf) continue;
        if (! colon) continue;
        args = wordlist_make (colon);
        *colon = 0;
        devstart = buf;
        while (*devstart == ' ') devstart++;
        
        if (var_get_count (matchdevices)) {
            if (! matchlist (devstart, matchdevices)) {
                wordlist_free (args);
                continue;
            }
        }
        else if (! var_get_count (skipdevices)) {
            /* If no match/skip devices are given, assume a default
               match on only 'ethX' devices. */
            if (strncmp (devstart, "eth", 3) != 0) {
                wordlist_free (args);
                continue;
            }
        }
        
        if (matchlist (devstart, skipdevices)) {
            wordlist_free (args);
            continue;
        }

        if (args->argc < 11) {
            wordlist_free (args);
            continue;
        }
        
        uint64_t in = strtoull (args->argv[1], NULL, 10);
        uint64_t out = strtoull (args->argv[9], NULL, 10);
        bool up = false;
        sprintf (sysbuf, "/sys/class/net/%s/operstate", devstart);
        fsys = fopen (sysbuf, "r");
        if (fsys) {
            fgets (sysbuf, 511, fsys);
            if (sysbuf[0] == 'u' && sysbuf[1] == 'p') up = true;
            fclose (fsys);
        }

        port = portlist_get_port (&PORTS, devstart);
        if (port) {
            portinfo_handle_data (port, up, in, out);
        }
    }
    fclose (fproc);
    
    var *res_ports = var_get_array_forkey (res, "port");
    
    port = PORTS.first;
    while (port) {
        var *res_port = var_add_dict (res_ports);
        var_set_str_forkey (res_port, "if", port->devname);
        var_set_int_forkey (res_port, "in", port->in_kbps);
        var_set_int_forkey (res_port, "out", port->out_kbps);
        var_set_str_forkey (res_port, "st", port->up ? "up" : "down");
        
        port = port->next;
    }
    
    return res;
}

/* ======================================================================= */
/* PROBE probe_ipmi                                                        */
/* ======================================================================= */

var *runprobe_ipmi (probe *self) {
    FILE *F;
    char buf[1024];
    var *res = var_alloc();
    wordlist *args;
    
    const char *ipmi_interface = "open";
    
    strcpy (buf, "ipmitool -c ");
    var *vipmi_options = var_get_dict_forkey (self->options, "ipmi");
    var *vipmi_if = var_find_key (vipmi_options, "interface");
    if (vipmi_if) ipmi_interface = var_get_str (vipmi_if);
    var *vipmi_values = var_get_dict_forkey (self->options, "values");
    
    sprintf (buf, "opticon-helper ipmitool -c -I %s sdr", ipmi_interface);
    F = popen_safe (buf, "r");
    if (! F) {
        log_warn ("probe_ipmi: Could not open ipmitool: %s", strerror(errno));
        return res;
    }
    
    log_debug ("probe_ipmi: Called '%s'", buf);
    
    while (! feof (F)) {
        *buf = 0;
        fgets (buf, 255, F);
        if (! *buf) continue;
        buf[strlen(buf)-1] = 0;
        args = wordlist_split (buf, ',');
        if (args->argc > 3) {
            var *crsr = var_first (vipmi_values);
            while (crsr) {
                var *cc = var_first (crsr);
                while (cc) {
                    const char *match = var_get_str_forkey (cc, "match");
                    if (strcasecmp (match, args->argv[0]) == 0) {
                        log_debug ("probe_ipmi:   matching '%s' value '%s'",
                                   match, args->argv[1]);
                        const char *key = crsr->id;
                        const char *id = var_get_str_forkey (cc, "id");
                        const char *unit = var_get_str_forkey (cc, "u");
                        var *arr = var_get_array_forkey (res, key);
                        var *nrow = var_add_dict (arr);
                        var_set_str_forkey (nrow, "id", id);
                        
                        if (strchr (args->argv[3], '.')) {
                            var_set_double_forkey (nrow, "v",
                                                   atof(args->argv[1]));
                        }
                        else {
                            var_set_int_forkey (nrow, "v",
                                                atoi(args->argv[1]));
                        }
                        var_set_str_forkey (nrow, "u", unit);
                        cc = cc->parent->value.arr.last;
                        crsr = crsr->parent->value.arr.last;
                    }
                    cc = cc->next;
                }
                crsr = crsr->next;
            }
        }
        wordlist_free (args);
    }
    pclose_safe (F);
    log_debug ("probe_ipmi: run complete");
    return res;
}

/* ======================================================================= */
/* PROBE probe_df                                                          */
/* ======================================================================= */

/** Get the %used of a mounted filesystem. We don't worry about timeouts,
  * if the thread stalls, it stalls.
  * \param fs The mountpoint.
  * \return Usage percentage.
  */
double fs_used (const char *fs) {
    struct statfs sfs;
    double blkuse;
    double blksz;
    double res = 0.0;
    
    if (statfs (fs, &sfs) == 0) {
        log_debug ("(fs_used) %s %i %i", fs, sfs.f_blocks, sfs.f_bfree);
        blkuse = (sfs.f_blocks - sfs.f_bfree);
        blksz = (sfs.f_blocks+1);
        res = 100.0 * blkuse / blksz;
    }
    else {
        log_error ("(fs_used) error from stats %s", fs);
    }
    return res;
}

/** Query the os for the size of a disk device, convert to megabytes.
  * \param devname The name of the device node.
  * \return Number of megabytes, 0 on any kind of failure.
  */
uint64_t diskdevice_size_in_mb (const char *devname) {
    struct stat          st;
    wordlist            *args;
    int                  iminor, imajor;
    FILE                *F;
    char                 buf[256];
    unsigned long long   sizebytes;

    if (stat (devname, &st)) return 0;
    iminor = minor (st.st_rdev);
    imajor = major (st.st_rdev);

    F = fopen ("/proc/partitions","r");
    if (!F) return 0;

    while (! feof (F))
    {
        *buf = 0;
        fgets (buf, 255, F);
        buf[255] = 0;
        if (*buf) buf[strlen(buf)-1] = 0;
        if (! (*buf)) continue;

        args = wordlist_make (buf);
        if (args->argc < 3)
        {
            wordlist_free (args);
            continue;
        }

        if (imajor == atoi (args->argv[0]))
        {
            if (iminor == atoi (args->argv[1]))
            {
                sizebytes = atoll (args->argv[2]);
                wordlist_free (args);
                fclose (F);
                return sizebytes/1024;
            }
        }
        wordlist_free (args);
    }
    fclose (F);
    return 0;
}

/** Probe function */
var *runprobe_df (probe *self) {
    FILE *F;
    char buf[256];
    wordlist *args;
    var *res = var_alloc();
    var *res_df = var_get_array_forkey (res, "df");
    var *skipdevices = var_get_array_forkey (self->options, "skip");
    var *matchdevices = var_get_array_forkey (self->options, "match");

    F = fopen ("/proc/mounts","r");
    if (! F) return res;
    while (! feof (F)) {
        *buf = 0;
        fgets (buf, 255, F);
        if (*buf) {
            args = wordlist_make (buf);
            if (args->argc > 3) {
                const char *dev = args->argv[0];
                const char *mntat = args->argv[1];
                
                if (var_get_count (matchdevices)) {
                    if (! matchlist (dev, matchdevices)) {
                        wordlist_free (args);
                        continue;
                    }
                }
                
                if (matchlist (dev, skipdevices)) {
                    wordlist_free (args);
                    continue;
                }
                
                if ((! strncmp (args->argv[3], "rw", 2)) &&
                    (strcmp (args->argv[2], "rootfs")) &&
                    (strcmp (args->argv[2], "proc")) &&
                    (strcmp (args->argv[2], "usbdevfs")) &&
                    (strcmp (args->argv[2], "devfs")) &&
                    (strcmp (args->argv[2], "tmpfs")) &&
                    (strcmp (args->argv[2], "usbfs")) &&
                    (strcmp (args->argv[2], "autofs")) &&
                    (strncmp (args->argv[2], "nfs", 3)) &&
                    (strncmp (args->argv[2], "sys", 3)) &&
                    (strcmp (args->argv[2], "devpts")) &&
                    (strncmp (args->argv[2], "binfmt_", 7)) &&
                    (strncmp (args->argv[2], "rpc_", 4)) &&
                    (strcmp (args->argv[0], "none")) &&
                    (strncmp (args->argv[1], "/sys", 4)) &&
                    (strncmp (args->argv[1], "/proc", 5))) {
                    uint64_t sz = diskdevice_size_in_mb (dev);
                    if (sz) {
                        var *mnt = var_add_dict (res_df);
                        if (strncmp (dev, "/dev/mapper", 11) == 0) {
                            char *ndev = malloc (strlen(dev) * sizeof(char));
                            ndev[0] = '@';
                            const char *dc = dev+12;
                            int nc = 1;
                            while (*dc) {
                                if (*dc == '-') {
                                    if (dc[1] == '-') {
                                        ndev[nc++] = '-';
                                        dc++;
                                    }
                                    else {
                                        ndev[nc++] = '/';
                                    }
                                }
                                else {
                                    ndev[nc++] = *dc;
                                }
                                dc++;
                            }
                            ndev[nc] = 0;
                            var_set_str_forkey (mnt, "device", ndev);
                            free (ndev);
                        }
                        else {
                            var_set_str_forkey (mnt, "device", dev);
                        }
                        var_set_int_forkey (mnt, "size", sz);
                        var_set_double_forkey (mnt, "pused", fs_used (mntat));
                        var_set_str_forkey (mnt, "mount", mntat);
                        var_set_str_forkey (mnt, "fs", args->argv[2]);
                    }
                }
            }
            wordlist_free (args);
        }
    }
    fclose (F);
    return res;
}

/* ======================================================================= */
/* PROBE probe_loadavg                                                     */
/* ======================================================================= */
var *runprobe_loadavg (probe *self) {
    FILE *F;
    char buf[256];
    wordlist *arg;
    char *slash;
    double td;
    var *res = var_alloc();
    
    F = fopen ("/proc/loadavg", "r");
    if (F) {
        *buf = 0;
        fgets (buf, 255, F);
        fclose (F);
        
        arg = wordlist_make (buf);
        td = atof (arg->argv[0]);
        var *res_load = var_get_array_forkey (res, "loadavg");
        var_add_double (res_load, atof (arg->argv[0]));
        var_add_double (res_load, atof (arg->argv[1]));
        var_add_double (res_load, atof (arg->argv[2]));
        
        var *res_proc = var_get_dict_forkey (res, "proc");
        var_set_int_forkey (res_proc, "run", atoi (arg->argv[3]));
        slash = strchr (arg->argv[3], '/');
        if (slash) {
            var_set_int_forkey (res_proc, "total", atoi (slash+1));
        }
        wordlist_free (arg);
    }
    return res;
}

/* ======================================================================= */
/* PROBE probe_distro                                                      */
/* ======================================================================= */
var *runprobe_distro (probe *self) {
    FILE *F;
    char buf[256];
    char *distro = NULL;
    char *c;
    var *res = var_alloc();
    
    F = fopen ("/etc/lsb-release","r");
    if (F) {
        while (! feof (F)) {
            *buf = 0;
            fgets (buf, 255, F);
            if (strncmp (buf, "DISTRIB_DESCRIPTION=", 20) == 0) {
                distro = buf+21;
                break;
            }
        }
        fclose (F);
    }
    else {
        F = fopen ("/etc/redhat-release","r");
        if (! F) F = fopen ("/etc/version","r");
        
        if (F) {
            *buf = 0;
            fgets (buf, 255, F);
            if (*buf) distro = buf;
            fclose (F);
        }
        else {
            F = fopen ("/etc/version_info","r");
            if (! F) {
                /* Fuck my life */
                F = fopen ("/etc/os-release","r");
                if (! F) {
                    log_debug ("No distro files found");
                    return res;
                }
                while (! feof (F)) {
                    *buf = 0;
                    fgets (buf, 255, F);
                    if (strncmp (buf, "PRETTY_NAME=", 12) == 0) {
                        c = distro = buf+13;
                        while (*c) {
                            if (*c == '\"') *c = 0;
                            else c++;
                        }
                        break;
                    }
                }
                fclose (F);
            }
            else {
                while (! feof (F)) {
                    *buf = 0;
                    fgets (buf, 255, F);
                    buf[255] = 0;
                    if (strncmp (buf, "* ",2) == 0) {
                        c = distro = buf+2;
                        while (*c) {
                            if (*c == '_') *c = ' ';
                            c++;
                        }
                        break;
                    }
                }
                fclose (F);
            }
        }
    }
    
    if (distro && strlen (distro)) {
        int len = strlen (distro);
        if (distro[len-1] == '\n') distro[len-1] = 0;
        len--;
        if (len && distro[len-1] == '\"') distro[len-1] = 0;
        log_debug ("distro: %s", distro);
        var *res_os = var_get_dict_forkey (res, "os");
        var_set_str_forkey (res_os, "distro", distro);
    }
    else {
        log_debug ("Error getting distro");
    }
    
    return res;
}

/* ======================================================================= */
/* PROBE probe_meminfo                                                     */
/* ======================================================================= */
var *runprobe_meminfo (probe *self) {
    FILE *F;
    char buf[256];
    var *res = var_alloc();
    var *res_mem = var_get_dict_forkey (res, "mem");
    uint64_t kmemfree;
    
    F = fopen ("/proc/meminfo", "r");
    if (F) {
        while (! feof (F)) {
            *buf = 0;
            fgets (buf, 255, F);
            if (strncmp (buf, "MemTotal:", 9) == 0) {
                var_set_int_forkey (res_mem, "total", strtoull (buf+9,NULL,10));
            }
            else if (strncmp (buf, "MemFree:", 8) == 0) {
                kmemfree = strtoull (buf+8, NULL, 10);
            }
            else if (strncmp (buf, "Buffers:", 8) == 0) {
                kmemfree += strtoull (buf+8, NULL, 10);
            }
            else if (strncmp (buf, "Cached:", 7) == 0) {
                kmemfree += strtoull (buf+7, NULL, 10);
            }
            else if (strncmp (buf, "SwapFree:", 9) == 0) {
                var_set_int_forkey (res_mem, "swap", strtoull (buf+9,NULL,10));
            }
        }
        fclose (F);
        var_set_int_forkey (res_mem, "free", kmemfree);
    }
    return res;
}

/* ======================================================================= */
/* PROBE probe_uptime                                                      */
/* ======================================================================= */
var *runprobe_uptime (probe *self) {
    char buf[256];
    var *res = var_alloc();
    FILE *F = fopen ("/proc/uptime","r");
    if (F) {
        *buf = 0;
        fgets (buf, 255, F);
        fclose (F);
        var_set_int_forkey (res, "uptime", strtoull (buf, NULL, 10));
    }
    time_t tnow = time (NULL);
    var *res_agent = var_get_dict_forkey (res, "agent");
    var_set_int_forkey (res_agent, "up", (tnow - APP.starttime));
    return res;
}

/* ======================================================================= */
/* PROBE probe_io                                                          */
/* ======================================================================= */

/** Tallies */
static struct linuxprobe_io_info_s {
    time_t      lastrun;
    uint64_t    total_cpu;
    uint64_t    net_in;
    uint64_t    net_out;
    uint64_t    io_blk_r;
    uint64_t    io_blk_w;
    uint64_t    io_wait;
} IOPROBE;

/** Probe function */
var *runprobe_io (probe *self)
{
    wordlist *split;
    FILE *F;
    time_t ti;
    char buf[256];
    char *c;
    uint64_t totalblk_r = 0;
    uint64_t totalblk_w = 0;
    uint64_t delta_r;
    uint64_t delta_w;
    double   delta;
    double   cpudelta;
    uint64_t totalcpu;
    uint64_t totalwait = 0;
    int ncpu = 1;
    var *res = var_alloc();
    var *toplevels = var_alloc(); /* to skip hda1 if we already did hda */
    var *skipdevices = var_get_array_forkey (self->options, "skip");
    var *matchdevices = var_get_array_forkey (self->options, "match");
    
    F = fopen ("/proc/diskstats", "r");
    if (F) {
        while (! feof (F)) {
            buf[0] = 0;
            fgets (buf, 255, F);
            if (strlen (buf) <15) continue;
            
            split = wordlist_make (buf+13);
            if (split->argc < 8) {
                wordlist_free (split);
                continue;
            }
            
            /* make copy of the device name with digits stripped off */
            strncpy (buf, split->argv[0], 255);
            buf[255] = 0;
            
            if (var_get_count (matchdevices)) {
                if (! matchlist (buf, matchdevices)) {
                    wordlist_free (split);
                    continue;
                }
            }
            
            if (matchlist (buf, skipdevices)) {
                wordlist_free (split);
                continue;
            }
            
            c = buf;
            while (isalpha (*c)) c++;
            if (isdigit (*c)) *c = 0;
            
            if (var_find_key (toplevels, buf)) {
                wordlist_free (split);
                continue;
            }
            if (strncmp (split->argv[0], "ram", 3) == 0) {
                wordlist_free (split);
                continue;
            }
            var_set_int_forkey (toplevels, split->argv[0], 1);
            
            totalblk_r += atoll (split->argv[3]);
            totalblk_w += atoll (split->argv[7]);
            
            log_debug ("disk %s r/%" PRIu64 " w/%" PRIu64,
                        split->argv[0], totalblk_r, totalblk_w);
            
            wordlist_free (split);
        }
        
        fclose (F);
    }
    
    var_free (toplevels);

    F = fopen ("/proc/stat","r");
    if (F) {
        fgets (buf, 255, F);
        split = wordlist_make (buf);
        totalwait = atoll (split->argv[5]);
        totalcpu = atoll (split->argv[1]) + atoll (split->argv[2]) +
                   atoll (split->argv[3]);
        
        log_debug ("cpustat: %lli %lli %lli",
                   atoll (split->argv[1]), atoll (split->argv[2]),
                   atoll (split->argv[3]));
        
        wordlist_free (split);
        ncpu = 0;
        
        while (!feof (F)) {
            buf[0] = 0;
            fgets (buf, 255, F);
            if (memcmp (buf, "cpu", 3) == 0) ncpu++;
        }
        fclose (F);
    }
    
    delta_r = totalblk_r - IOPROBE.io_blk_r;
    delta_w = totalblk_w - IOPROBE.io_blk_w;
    
    ti = time (NULL);
    if (ti == IOPROBE.lastrun) IOPROBE.lastrun--;
    if (IOPROBE.io_blk_r || IOPROBE.io_blk_w) {
        var *res_io = var_get_dict_forkey (res, "io");
        if (IOPROBE.io_blk_r) {
            var_set_int_forkey (res_io, "rdops", delta_r/(ti-IOPROBE.lastrun));
        }
        if (IOPROBE.io_blk_w) {
            var_set_int_forkey (res_io, "wrops", delta_w/(ti-IOPROBE.lastrun));
        }
    }

    delta = (totalwait - IOPROBE.io_wait) / (CLK_TCK * 1.0);
    cpudelta = (totalcpu - IOPROBE.total_cpu) / (CLK_TCK * 1.0);

    log_debug ("cpudelta: %.1f, ncpu %i, tdelta: %i tck:%i",
               cpudelta, ncpu, ti - IOPROBE.lastrun, CLK_TCK);

    var_set_double_forkey (res, "pcpu",
                        (100.0 * (cpudelta/ncpu)) / (ti - IOPROBE.lastrun));
    if (IOPROBE.io_wait) {
        var *res_io = var_get_dict_forkey (res, "io");
        var_set_double_forkey (res_io, "pwait",
                        (100.0 * (delta/ncpu)) / (ti - IOPROBE.lastrun));
    }

    IOPROBE.io_blk_r = totalblk_r;
    IOPROBE.io_blk_w = totalblk_w;
    IOPROBE.io_wait = totalwait;
    IOPROBE.total_cpu = totalcpu;
    IOPROBE.lastrun = ti;
    
    return res;
}

/* ======================================================================= */
/* PROBE probe_top                                                         */
/* ======================================================================= */

/** When collecting processes from tproc, they get sorted into an array
  * of these babies, before things get sent to var heaven.
  */
typedef struct topentry_s {
    char                username[16];
    pid_t               pid;
    double              pcpu;
    double              pmem;
    time_t              secrun;
    char                ptitle[48];
} topentry;

#define NR_TPROCS 24 /**< Needless headroom FTW */
#define MAX_NTOP 16 /**< Maximum number of records we'll return */

/** Array carrier for the topentry nodes */
typedef struct topinfo_s {
    short                ntop;
    topentry             tprocs[NR_TPROCS];
} topinfo;

/** Perform a sample round on a procrun */
void sample_tprocs (procrun *run) {
    struct dirent   *de;
    DIR             *D;
    FILE            *F;
    pid_t            pid;
    unsigned long    utime;
    unsigned long    stime;
    uid_t            tuid;
    gid_t            tgid;
    struct stat      st;
    char             buf[256];
    char            *c;
    char            *cc;
    wordlist        *words;
    struct timeval   tv;
    int              pausetimer;
    long long        rss;
    double           tpmem;
    double           pmem;
    
    procrun_initsample (run);
    D = opendir ("/proc");
    if (!D) return;
    pausetimer = 0;
    
    while ( (de = readdir (D)) ) {
        pid = atoi (de->d_name);
        
        /* Don't spike CPU on machines with lots of processes */
        pausetimer++;
        if (pausetimer & 15) {
            tv.tv_sec = 0;
            tv.tv_usec = 64;
            select (0, NULL, NULL, NULL, &tv);
        }
        
        if (pid>0) {
            sprintf (buf, "/proc/%d", pid);
            if (stat (buf, &st) == 0) {
                tuid = st.st_uid;
                tgid = st.st_gid;
            }
            else continue;

            sprintf (buf, "/proc/%d/stat", pid);
            if ( (F = fopen (buf, "r")) ) {
                fgets (buf, 255, F);
                buf[255] = 0;
                
                c = buf;
                while (strchr (c, ')')) {
                    c = strchr (c, ')') + 1;
                }
                    
                /* 11=u 12=s */
                words = wordlist_make (c);
                if (words->argc > 12) {
                    utime = atol (words->argv[11]);
                    stime = atol (words->argv[12]);
                }
                fclose (F);
                wordlist_free (words);

                sprintf (buf, "/proc/%d/statm", pid);
                if ((F = fopen (buf, "r"))) {
                    memset (buf, 0, 255);
                    fgets (buf, 255, F);
                    c = strchr (buf, ' ');
                    if (c) {
                        ++c;
                        rss = 4 * atoll (c);
                        
                        tpmem = rss;
                        tpmem *= 10000.0;
                        tpmem /= KMEMTOTAL;
                        pmem = tpmem;
                    }
                    fclose (F);
                }
                else {
                    rss = 128000;
                }
                
                sprintf (buf, "/proc/%d/cmdline", pid);
                if ((F = fopen (buf, "r"))) {
                    memset (buf, 0, 255);
                    fread (buf, 1, 255, F);
                    fclose (F);
                                        
                    if (*buf == 0) {
                        sprintf (buf, "/proc/%d/status", pid);
                        if ((F = fopen (buf, "r"))) {
                            memset (buf, 0, 255);
                            fgets (buf, 255, F);
                            c = strchr (buf, ':');
                            if (! c) {
                                fclose (F);
                                continue;
                            }
                            ++c;
                            while (*c <= ' ') ++c;
                            if (strlen (c)) {
                                c[strlen(c)-1] = 0;
                            }
                            
                            fclose (F);
                            
                            procrun_setproc (run, pid, utime, stime,
                                             tuid, tgid, c, pmem);
                        }
                    }
                    else {
                        for (int pos=0; pos<255; ++pos) {
                            if (buf[pos] == 0 && buf[pos+1] != 0) {
                                buf[pos] = ' ';
                            }
                            else if (! buf[pos]) break;
                        }
                        
                        /* strip paths */
                        c = buf;
                        cc = c;
                        
                        while (*c && (*c != ' ')) {
                            if (*c == '/') cc = c+1;
                            c++;
                        }
                        
                        procrun_setproc (run, pid, utime, stime,
                                         tuid, tgid, cc, pmem);
                    }
                }
            }
        }
    }
    closedir (D);
}

/** Utility function for inserting a hole in the topinfo array, to make room
  * for a higher ranked process to enter the folds. */
void make_top_hole (topinfo *inf, int pos) {
    int tailsz;
    
    if (pos>(MAX_NTOP-2)) return;
    tailsz = ((MAX_NTOP-1) - pos);
    
    memmove (inf->tprocs + pos + 1,
             inf->tprocs + pos,
             tailsz * sizeof (topentry));
    
    if (inf->ntop < MAX_NTOP) inf->ntop++;
}

/** Function that reaps the procrun data gathered and sorts it into the
  * topinfo array. */
var *gather_tprocs (procrun *procs) {
    int i;
    int j;
    int inserted;
    struct passwd *pwd;
    topinfo inf;
    inf.ntop = 0;
    
    var *res = var_alloc();
    
    sample_tprocs (procs);
    procrun_calc (procs);
    
    //var_set_double_forkey (res, "pcpu",procs->pcpu);
    for (i=0; i<procs->count; ++i) {
        inserted = 0;
        if (! procs->array[i].pid) continue;
        for (j=0; j<inf.ntop;++j) {
            if (((3*inf.tprocs[j].pcpu) + (inf.tprocs[j].pmem/2)) <
                ((3*procs->array[i].pcpu) + (procs->array[i].pmem/2))) {
                if ((procs->array[i].pcpu == 0) &&
                    (procs->array[i].pmem < 501)) continue;
                
                make_top_hole (&inf, j);
                pwd = getpwuid (procs->array[i].uid);
                if (pwd) {
                    strncpy (inf.tprocs[j].username, pwd->pw_name, 15);
                    inf.tprocs[j].username[15] = 0;
                }
                else {
                    sprintf (inf.tprocs[j].username, "#%d",
                             procs->array[i].uid);
                }
                inf.tprocs[j].pid = procs->array[i].pid;
                inf.tprocs[j].pcpu = procs->array[i].pcpu;
                inf.tprocs[j].pmem = procs->array[i].pmem;
                inf.tprocs[j].secrun = 0;
                strncpy (inf.tprocs[j].ptitle, procs->array[i].ptitle, 47);
                inf.tprocs[j].ptitle[47] = 0;
                inserted = 1;
                j = inf.ntop;
            }
        }
        
        if ((! inserted) && (inf.ntop < MAX_NTOP-2) &&
            (procs->array[i].pcpu || (procs->array[i].pmem > 500))) {
            j = inf.ntop;
            inf.ntop++;
            
            pwd = getpwuid (procs->array[i].uid);
            if (pwd) {
                strncpy (inf.tprocs[j].username, pwd->pw_name, 15);
                inf.tprocs[j].username[15] = 0;
            }
            else {
                sprintf (inf.tprocs[j].username, "#%d", procs->array[i].uid);
            }
            
            inf.tprocs[j].pid = procs->array[i].pid;
            inf.tprocs[j].pcpu = procs->array[i].pcpu;
            inf.tprocs[j].pmem = procs->array[i].pmem;
            inf.tprocs[j].secrun = 0;
            strncpy (inf.tprocs[j].ptitle, procs->array[i].ptitle, 47);
            inf.tprocs[j].ptitle[47] = 0;
            inserted = 1;
        }
    }
    
    var *vtop = var_get_array_forkey (res, "top");
    for (i=0; i<inf.ntop; ++i) {
        var *vproc = var_add_dict (vtop);
        var_set_int_forkey (vproc, "pid", inf.tprocs[i].pid);
        var_set_str_forkey (vproc, "user", inf.tprocs[i].username);
        var_set_double_forkey (vproc, "pcpu", inf.tprocs[i].pcpu / 100.0);
        var_set_double_forkey (vproc, "pmem", inf.tprocs[i].pmem / 100.0);
        var_set_str_forkey (vproc, "name", inf.tprocs[i].ptitle);
    }
    
    procrun_newround (procs);
    return res;
}

/** Structure for safely allowing other threads to consume data out of the
  * probe, provided they take less than 30 seconds to copy the var.
  */
static struct topthreadinfo_s {
    var *oldres;
    var *res;
    thread *thread;
} TOPTHREAD = {NULL,NULL, NULL};

/** We oversample the procrun, to allow us to catch information at a higher
  * frequency. The lower the frequency of sampling, the higher the chance that
  * a process has a chance to be created, do something nasty that you'd like
  * to know, and disappear before the next sample is taken, leaving you none
  * the wiser. This function runs the background thread that feeds the actual
  * probe function the same way probe threads feed the main aggregator.
  */
void run_top (thread *self) {
    FILE *F;
    char buf[256];
    thread_setname (self, "top");
    
    procrun *p = (procrun *) malloc (sizeof (procrun));
    int i;
    
    procrun_init (p);

    F = fopen ("/proc/meminfo","r");
    if (F) {
        while (! feof (F)) {
            *buf = 0;
            fgets (buf, 255, F);
            
            if (strncmp (buf, "MemTotal:", 9) == 0) {
                KMEMTOTAL = atoi (buf+9);
            }
        }
        fclose (F);
    }

    F = fopen ("/proc/cpuinfo","r");
    if (F) {
        while (! feof (F)) {
            buf[0] = 0;
            fgets (buf, 255, F);
            if (strncasecmp (buf, "processor", 9) == 0) {
                p->ncpu++;
            }
        }
        fclose (F);
    }
    else {
        p->ncpu = 1;
    }

    while (1) {
        for (i=0; i<3; ++i) {
            sample_tprocs (p);
            sleep (10);
        }
        var *newres = gather_tprocs (p);
        if (TOPTHREAD.oldres) {
            var_free (TOPTHREAD.oldres);
            TOPTHREAD.oldres = NULL;
        }
        if (TOPTHREAD.res) TOPTHREAD.oldres = TOPTHREAD.res;
        TOPTHREAD.res = newres;
    }
}

/** Probe function */
var *runprobe_top (probe *self) {
    if (! TOPTHREAD.thread) {
        TOPTHREAD.thread = thread_create (run_top, NULL);
    }

    if (TOPTHREAD.res) {
        var *res = var_alloc();
        var_copy (res, TOPTHREAD.res);
        return res;
    }    
    return NULL;
}

/* Probe cpu models */
var *runprobe_cpu (probe *self) {
    FILE *f = fopen ("/proc/cpuinfo","r");
    if (!f) return NULL;
    
    var *res = var_alloc();
    var *res_cpu = var_get_array_forkey (res, "cpu");
    var *tally = var_alloc();
    char buf[1024];
    
    while (! feof (f)) {
        *buf = 0;
        fgets (buf, 1023, f);
        if ((strncmp (buf, "model name", 10) == 0) ||
            (strncmp (buf, "cpu model", 9) == 0)) {
            char *c = strchr (buf, ':');
            if (c) {
                c++;
                while (isspace (*c)) c++;
                
                char *nl = strchr (buf, '\n');
                if (nl) *nl=0;
                
                var *k = var_find_key (tally, c);
                if (! k) {
                    var_set_int_forkey (tally, c, 1);
                }
                else {
                    var_set_int (k, var_get_int (k) + 1);
                }
            }
        }
    }
    fclose (f);
    
    var *crsr = var_first (tally);
    while (crsr) {
        var *obj = var_add_dict (res_cpu);
        var_set_str_forkey (obj, "model", crsr->id);
        var_set_int_forkey (obj, "count", var_get_int (crsr));
        crsr = crsr->next;
    }
    
    return res;
}

/* ======================================================================= */
builtinfunc BUILTINS[] = {
    GLOBAL_BUILTINS,
    {"probe_cpu", runprobe_cpu},
    {"probe_top", runprobe_top},
    {"probe_uptime", runprobe_uptime},
    {"probe_io", runprobe_io},
    {"probe_loadavg", runprobe_loadavg},
    {"probe_meminfo", runprobe_meminfo},
    {"probe_df", runprobe_df},
    {"probe_net", runprobe_net},
    {"probe_portinfo", runprobe_portinfo},
    {"probe_ipmi", runprobe_ipmi},
    {"probe_distro", runprobe_distro},
    {"probe_version", runprobe_version},
    {NULL, NULL}
};
