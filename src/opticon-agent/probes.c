#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <libopticon/log.h>
#include <libopticon/var.h>
#include "probes.h"
#include "wordlist.h"

/** Get the hostname. POSIX compliant */
var *runprobe_hostname (probe *self) {
    char out[256];
    gethostname (out, 255);
    var *res = var_alloc();
    var_set_str_forkey (res, "hostname", out);
    return res;
}

/** Get kernel version information. POSIX compliant. */
var *runprobe_uname (probe *self) {
    var *res = var_alloc();
    var *v_os = var_get_dict_forkey (res, "os");
    struct utsname uts;
    uname (&uts);
    var_set_str_forkey (v_os, "kernel", uts.sysname);
    var_set_str_forkey (v_os, "version", uts.release);
    var_set_str_forkey (v_os, "arch", uts.machine);

    FILE *f;
    char pname[32];
    char vendor[32];
    char platform[96];
    pname[0] = 0;
    vendor[0] = 0;
    platform[0] = 0;
    
    if (strcmp (uts.sysname, "Darwin") == 0) {
        char buf[256];

        f = popen_safe ("system_profiler SPHardwareDataType","r");
        if (f) {
            while (! feof (f)) {
                fgets (buf, 255, f);
                buf[255] = 0;
                if (strstr (buf, "Model Identifier")) {
                    char *c = strchr (buf, ':');
                    if (c) {
                        c[strlen(c)-1] = 0;
                        strncpy (pname, c, 31);
                        pname[31] = 0;
                        sprintf (platform, "Apple %s", pname);
                        var_set_str_forkey (v_os, "hw", platform);
                        pclose (f);
                        return res;
                    }
                }
            }
            pclose_safe (f);
        }
    }
    
    f = fopen ("/sys/devices/virtual/dmi/id/product_name","r");
    if (f) {
        fgets (pname, 31, f);
        pname[31] = 0;
        if (*pname) pname[strlen(pname)-1] = 0;
        fclose (f);
    }
        
    f = fopen ("/sys/devices/virtual/dmi/id/sys_vendor","r");
    if (f) {
        fgets (vendor, 31, f);
        vendor[31] = 0;
        if (*vendor) vendor[strlen(vendor)-1] = 0;
        fclose (f);
    }
    
    if (strcmp (vendor, "Microsoft Corporation") == 0) {
        strcpy (vendor, "Microsoft");
        if (strcmp (pname, "Virtual Machine") == 0) {
            strcpy (pname, "Hyper-V");
        }
    }
    
    if (*vendor && *pname) {
        sprintf (platform, "%s %s", vendor, pname);
        var_set_str_forkey (v_os, "hw", platform);
        return res;
    }
    
    struct stat st;
    if (stat ("/sbin/getcfg", &st) == 0) {
        f = popen_safe ("/sbin/getcfg System Model","r");
        fgets (pname, 31, f);
        pname[31] = 0;
        if (*pname) pname[strlen(pname)-1] = 0;
        pclose_safe (f);
        sprintf (platform, "QNAP %s", pname);
        var_set_str_forkey (v_os, "hw", platform);
        return res;
    }
    
    if (stat ("/usr/sbin/ubnt-hal", &st) == 0) {
        f = popen_safe ("opticon-helper /usr/sbin/ubnt-hal show-version","r");
        if (f) {
            char buf[256];
            buf[0] = 0;
            
            while (! feof (f)) {
                fgets (buf, 255, f);
                buf[255] = 0;
                if (memcmp (buf, "HW model:", 9) == 0) {
                    buf[strlen(buf)-1] = 0;
                    char *c = buf+9;
                    while (isspace (*c)) c++;
                    strncpy (pname, buf, 31);
                    pname[31] = 0;
                    sprintf (platform, "Ubiquiti %s", pname);
                    var_set_str_forkey (v_os, "hw", platform);
                    return res;
                }
            }
            pclose_safe (f);
        }
    }
    
    return res;
}

/** Get logged on users from unix who */
var *runprobe_who (probe *self) {
    char buf[1024];
    var *res = var_alloc();
    var *res_who = var_get_array_forkey (res, "who");
    char *cremote;
    char *c;
    wordlist *args;
    FILE *f = popen_safe ("/usr/bin/who","r");
    int fd = fileno (f);
    fd_set fds;
    struct timeval tv;
    
    if (! f) return res;
    while (! feof (f)) {
        *buf = 0;
        tv.tv_sec = 30; tv.tv_usec = 0;
        FD_ZERO (&fds);
        FD_SET (fd, &fds);
        if (select (fd+1, &fds, NULL, NULL, &tv) > 0) {
            fgets (buf, 1023, f);
            buf[1023] = 0;
            if (*buf == 0) continue;
            if (! strchr (buf, '(')) continue;
        }
        else {
            log_error ("probe_who timeout");
            break;
        }
        
        args = wordlist_make (buf);
        if (args->argc > 4) {
            var *rec = var_add_dict (res_who);
            var_set_str_forkey (rec, "user", args->argv[0]);
            var_set_str_forkey (rec, "tty", args->argv[1]);
            cremote = strchr (buf, '(');
            cremote++;
            c = strchr (cremote, ')');
            if (c) *c = 0;
            var_set_str_forkey (rec, "remote", cremote);
        }
        wordlist_free (args);
    }
    pclose_safe (f);
    return res;
}

var *runprobe_localip (probe *self) {
    char buf[1024];
    char addr[1024];
    int count = 0;
    fd_set fds;
    struct timeval tv;
    
    var *res = var_alloc();
    var *res_link = var_get_dict_forkey (res, "link");
#if defined(OS_LINUX)
    const char *cmd = "dev=$(ip route show | grep ^default | head -1 | "
                      "sed -e 's/.*dev //' | cut -f1 -d' '); "
                      "ip addr list dev $dev";
#else
    const char *cmd = "ifconfig | grep -v 127.0.0.1";
#endif
    
    FILE *f = popen_safe (cmd, "r");
    if (! f) return res;

    int fd = fileno (f);

    while (! feof (f)) {
         *buf = 0;
        tv.tv_sec = 30; tv.tv_usec = 0;
        FD_ZERO (&fds);
        FD_SET (fd, &fds);
        if (select (fd+1, &fds, NULL, NULL, &tv) > 0) {
            fgets (buf, 1023, f);
            buf[1023] = 0;
            if (*buf == 0) continue;
        }
        else {
            log_error ("probe_localip timeout");
            break;
        }
        
        char *m_inet = strstr (buf, "inet ");
        if (m_inet) {
            m_inet += 5;
            char *c = m_inet;
            while ((*c == '.')||(*c >= '0' && *c <= '9')) {
                ++c;
            }
            if (*c) *c = 0;
            var_set_str_forkey (res_link, "ip", m_inet);
            break;
        }
    }
    pclose_safe (f);
    return res;
}

#define GLOBAL_BUILTINS \
    {"probe_hostname", runprobe_hostname}, \
    {"probe_uname", runprobe_uname}, \
    {"probe_who", runprobe_who}, \
    {"probe_localip", runprobe_localip}

#if defined(OS_DARWIN)
  #include "probes_darwin.c"
#elif defined (OS_LINUX)
  #include "probes_linux.c"
#else
  #error Undefined Operating System
#endif

