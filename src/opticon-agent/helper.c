#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <grp.h>

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
    
    setregid (0, 0);
    setreuid (0, 0);
    return execvp (argv[1], argv+1);
}
