#ifndef _OPTIONS_H
#define _OPTIONS_H 1

typedef struct apioptions_s {
    const char  *dbpath;
    int          loglevel;
    const char  *logpath;
    const char  *confpath;
    const char  *mconfpath;
    const char  *gconfpath;
    const char  *pidfile;
    const char  *keystone_url;
    const char  *unithost_url;
    int          foreground;
    int          port;
    uuid         admintoken;
    const char  *adminhost;
    var         *conf;
    var         *mconf;
    var         *gconf;
} apioptions;

extern apioptions OPTIONS;

#endif
