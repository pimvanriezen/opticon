#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <grp.h>

int main (int argc, const char *argv[]) {
    if (geteuid()) {
        fprintf (stderr, "%% Helper not suid\n");
        return 1;
    }
    
    if (argc < 1) {
        return 0;
    }
    
    gid_t curgroupid = getgid();
    struct group *curgroup = getgrgid (curgroupid);
    if (strcmp (curgroup->gr_name, "opticon")) {
        fprintf (stderr, "%% Wrong group\n");
        return 1;
    }
    
    return execvp (argv[1], argv+1);
}
