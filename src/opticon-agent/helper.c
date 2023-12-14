#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <grp.h>
#include <stdlib.h>

// We can't trust the PATH to be sensible in an environment where we're
// spawned from a service. Assume default system paths, anything outside
// will need an explicit path specified. This is undesirable for, e.g.,
// /usr/sbin, because some tools are not consistent in their location between
// different distros.
const char *ALLOWED_DEFAULTPATHS[] = {
    "/bin",
    "/sbin",
    "/usr/bin",
    "/usr/sbin",
    NULL
};

int main (int argc, char *const *argv) {
    if (geteuid()) {
        fprintf (stderr, "%% Helper not suid\n");
        return 1;
    }
    
    if (argc < 2) {
        return 0;
    }
    
    gid_t curgroupid = getgid();
    struct group *curgroup = getgrgid (curgroupid);
    if (strcmp (curgroup->gr_name, "opticon")) {
        fprintf (stderr, "%% Wrong group\n");
        return 1;
    }
    
    int allow = 0;
    char buf[1024];
    FILE *f = fopen ("/etc/opticon/helpers.conf","r");
    if (!f) {
        fprintf (stderr, "%% Could not open helpers config\n");
        return 1;
    }
    
    while (! feof (f)) {
        fgets (buf, 1023, f);
        buf[1023] = 0;
        if (*buf) buf[strlen(buf)-1] = 0;
        char *crsr = strchr (buf, ' ');
        if (crsr) {
            *crsr = 0;
            crsr++;
        }

        if (strcmp (buf, argv[1]) == 0) {
            int apos = 2;
            char *next = NULL;
            
            if (! crsr) {
                allow = 1;
                break;
            }
            
            allow = 1;
            while (crsr) {
                if (apos >= argc) {
                    allow = 0;
                    break;
                }
                next = strchr (crsr, ' ');
                if (next) *next = 0;
                if (strcmp (argv[apos],crsr)) {
                    allow = 0;
                    break;
                }
                crsr = next ? next+1 : NULL;
                apos++;
            }
            if (allow) break;
        }
    }
    fclose (f);
    
    if (! allow) {
        fprintf (stderr, "%% Not allowed\n");
        return 1;
    }
    
    char *execpath = (char *) malloc ((size_t) strlen(argv[1])+128);
    if (argv[1][0] == '/') {
        strcpy (execpath, argv[1]);
    }
    else {
        struct stat st;
        int i = 0;
        const char *c = ALLOWED_DEFAULTPATHS[i];
        while (ALLOWED_DEFAULTPATHS[i]) {
            sprintf (execpath, "%s/%s", ALLOWED_DEFAULTPATHS[i], argv[1]);
            if (stat (execpath, &st) == 0) break;
            execpath[0] = 0;
            i++;
        }
        if (execpath[0] == 0) {
            fprintf (stderr, "%% Command '%s' not found in default paths",
                     argv[1]);
            return 1;   
        }
    }
    
    setregid (0, 0);
    setreuid (0, 0);
    return execvp (execpath, argv+1);
}
