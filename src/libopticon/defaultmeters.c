#include <libopticon/var.h>
#include <libopticon/var_parse.h>
#include <libopticon/log.h>
#include <assert.h>

/*/ ======================================================================= /*/
/** JSON text of default summary data */
/*/ ======================================================================= /*/
static const char *DEFSUMMARY =
"   cpu { meter: pcpu, type: frac, func: avg }"
"   warning { meter: status, type: string, func: count, match: WARN }"
"   alert { meter: status, type: string, func: count, match: ALERT }"
"   critical { meter: status, type: string, func: count, match: CRITICAL }"
"   stale { meter: status, type: string, func: count, match: STALE }"
"   netin { meter: net/in_kbs, type: int, func: total }"
"   netout { meter: net/out_kbs, type: int, func: total }";

/*/ ======================================================================= /*/
/** JSON text of default meter data */
/*/ ======================================================================= /*/
static const char *DEFMETERS = 
"    agent/ip {"
"        type: string"
"        description: \"Remote IP Address\""
"    }"
"    link/rtt {"
"        type: frac"
"        description: \"RTT\""
"        unit: ms"
"        warning { cmp: gt, val: 20.0, weight: 1.0 }"
"        alert { cmp: gt, val: 100.0, weight: 1.0 }"
"        critical { cmp: gt, val: 300.0, weight: 1.0 }"
"    }"
"    link/loss {"
"        type: frac"
"        description: \"Packet loss\""
"        unit: %"
"        warning { cmp: gt, val: 5.0, weight: 1.0 }"
"        alert { cmp: gt, val: 15.0, weight: 1.0 }"
"        critical { cmp: gt, val: 20.0, weight: 1.0 }"
"    }"
"    link/ip {"
"        type: string"
"        description: \"Primary IP Address\""
"    }"
"    os/kernel {"
"        type: string"
"        description: \"Operating System\""
"    }"
"    os/kernel {"
"        type: string"
"        description: \"Version\""
"    }"
"    os/arch {"
"        type: string"
"        description: \"CPU Architecture\""
"    }"
"    os/distro {"
"        type: string"
"        description: Distribution"
"    }"
"    df {"
"        type: table"
"        description: \"Storage\""
"    }"
"    df/device {"
"        type: string"
"        description: \"Device\""
"    }"
"    df/size {"
"        type: integer"
"        description: \"Size\""
"        unit: KB"
"    }"
"    df/pused {"
"        type: frac"
"        description: \"In Use\""
"        unit: %"
"        warning { cmp: gt, val: 90.0, weight: 1.0 }"
"        alert { cmp: gt, val: 95.0, weight: 1.0 }"
"        critical { cmp: gt, val: 99.0, weight: 1.0 }"
"    }"
"    uptime {"
"        type: integer"
"        description: \"Uptime\""
"        unit: s"
"    }"
"    top {"
"        type: table"
"        description: \"Process List\""
"    }"
"    top/pid {"
"        type: integer"
"        description: \"PID\""
"    }"
"    top/name {"
"        type: string"
"        description: \"Name\""
"    }"
"    top/pcpu {"
"        type: frac"
"        description: \"CPU Usage\""
"        unit: %"
"    }"
"    top/pmem {"
"        type: frac"
"        description: \"RAM Usage\""
"        unit: %"
"    }"
"    pcpu {"
"        type: frac"
"        description: \"CPU Usage\""
"        unit: %"
"        warning { cmp: gt, val: 90.0, weight: 1.0 }"
"    }"
"    loadavg {"
"        type: frac"
"        description: \"Load Average\""
"        warning { cmp: gt, val: 8.0, weight: 1.0 }"
"        alert { cmp: gt, val: 20.0, weight: 1.0 }"
"        critical { cmp: gt, val: 50.0, weight: 1.0 }"
"    }"
"    proc/total {"
"        type: integer"
"        description: \"Total processes\""
"    }"
"    proc/run {"
"        type: integer"
"        description: \"Running processes\""
"    }"
"    proc/stuck {"
"        type: integer"
"        description: \"Stuck processes\""
"        warning { cmp: gt, val: 15, weight: 0.5 }"
"        alert { cmp: gt, val: 25, weight: 1.0 }"
"        critical { cmp: gt, val: 50, weight: 1.0 }"
"    }"
"    net/in_kbs {"
"        type: integer"
"        description: \"Network data in\""
"        unit: Kb/s"
"        warning { cmp: gt, val: 40000, weight: 0.5 }"
"    }"
"    net/in_pps {"
"        type: integer"
"        description: \"Network packets in\""
"        unit: pps"
"        warning { cmp: gt, val: 10000, weight: 1.0 }"
"        alert { cmp: gt, val: 50000, weight: 1.0 }"
"        critical { cmp: gt, val: 100000, weight: 1.0 }"
"    }"
"    net/out_kbs {"
"        type: integer"
"        description: \"Network data out\""
"        unit: Kb/s"
"    }"
"    net/out_pps {"
"        type: integer"
"        description: \"Network packets out\""
"        unit: pps"
"        warning { cmp: gt, val: 10000, weight: 1.0 }"
"        alert { cmp: gt, val: 50000, weight: 1.0 }"
"        critical { cmp: gt, val: 100000, weight: 1.0 }"
"    }"
"    io/rdops {"
"        type: integer"
"        description: \"Disk I/O (read)\""
"        unit: iops"
"    }"
"    io/wrops {"
"        type: integer"
"        description: \"Disk I/O (write)\""
"        unit: iops"
"    }"
"    mem/total {"
"        type: integer"
"        description: \"Total RAM\""
"        unit: KB"
"    }"
"    mem/free {"
"        type: integer"
"        description: \"Free RAM\""
"        unit: KB"
"        warning { cmp: lt, val: 65536, weight: 0.5 }"
"        alert { cmp: lt, val: 32768, weight: 1.0 }"
"        critical { cmp: lt, val: 4096, weight: 1.0 }"
"    }"
"    chkwarn {"
"        type: string"
"        description: \"Selfcheck warnings\""
"        warning { cmp: count, val: 1, weight: 0.5 }"
"    }"
"    chkalert {"
"        type: string"
"        description: \"Selfcheck alerts\""
"        alert { cmp: count, val: 1, weight: 1.0 }"
"    }"
"    who {"
"        type: table"
"        description: \"Remote Users\""
"    }"
"    who/user {"
"        type: string"
"        description: User"
"    }"
"    who/tty {"
"        type: string"
"        description: TTY"
"    }"
"    who/remote {"
"        type: string"
"        description: \"Remote IP\""
"    }";

/*/ ======================================================================= /*/
/** JSON text of default graph data */
/*/ ======================================================================= /*/
const char *DEFGRAPHS =
"   pcpu {"
"       graph: cpu"
"       datum: usage"
"       title: \"CPU Usage\""
"       unit: \"%\""
"       color: \"blue\""
"       max: 100.0"
"   }"
"   link/rtt {"
"       graph: link"
"       datum: rtt"
"       title: \"RTT\""
"       unit: \"ms\""
"       color: \"blue\""
"       max: 1.0"
"   }"
"   net/in_kbs {"
"       graph: net"
"       datum: input"
"       title: \"Bandwidth In\""
"       unit: \"Kb/s\""
"       color: \"cyan\""
"       max: 500.0"
"   }"
"   net/out_kbs {"
"       graph: net"
"       datum: output"
"       title: \"Bandwidth Out\""
"       unit: \"Kb/s\""
"       color: \"blue\""
"       max: 500.0"
"   }"
"   io/rdops {"
"       graph: io"
"       datum: read"
"       title: \"Read Operations\""
"       unit: \"iops\""
"       color: \"green\""
"       max: 100.0"
"   }"
"   io/wrops {"
"       graph: io"
"       datum: write"
"       title: \"Write Operations\""
"       unit: \"iops\""
"       color: \"red\""
"       max: 100.0"
"   }";

static var *PARSED_DEFMETERS = NULL;

/*/ ======================================================================= /*/
/** Get default meter data as var object */
/*/ ======================================================================= /*/
var *get_default_meterdef (void) {
    if (! PARSED_DEFMETERS) {
        PARSED_DEFMETERS = var_alloc();
        if (! var_parse_json (PARSED_DEFMETERS, DEFMETERS)) {
            log_error ("Parse error: %s", parse_error());
        }
    }
    return PARSED_DEFMETERS;
}

static var *PARSED_DEFSUMMARY = NULL;

/*/ ======================================================================= /*/
/** Get default summary data as var object */
/*/ ======================================================================= /*/
var *get_default_summarydef (void) {
    if (! PARSED_DEFSUMMARY) {
        PARSED_DEFSUMMARY = var_alloc();
        if (! var_parse_json (PARSED_DEFSUMMARY, DEFSUMMARY)) {
            log_error ("Parse error (defsum): %s", parse_error());
        }
    }
    return PARSED_DEFSUMMARY;
}

static var *PARSED_DEFGRAPHS = NULL;

/*/ ======================================================================= /*/
/** Get default graph data as var object */
/*/ ======================================================================= /*/
var *get_default_graphs (void) {
    if (! PARSED_DEFGRAPHS) {
        PARSED_DEFGRAPHS = var_alloc();
        if (! var_parse_json (PARSED_DEFGRAPHS, DEFGRAPHS)) {
            log_error ("Parse error (defgraphs): %s", parse_error());
        }
    }
    return PARSED_DEFGRAPHS;
}
