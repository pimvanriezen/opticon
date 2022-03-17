#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

struct termios NewInAttribs;
struct termios OldInAttribs;

// ============================================================================
// FUNCTION setupterm (filno)                                               
// 
// Sets up termios 'our rules' mode, saving the old state.                  
// ============================================================================
void setupterm(int const fd)
{
    if(tcgetattr(fd,&OldInAttribs)<0)
        exit(1);
   
    memcpy (&NewInAttribs, &OldInAttribs, sizeof (NewInAttribs));
    NewInAttribs.c_lflag = OldInAttribs.c_lflag & ~(ECHO | ICANON | ISIG);
    NewInAttribs.c_cc[VMIN] = 1;
    NewInAttribs.c_cc[VTIME] = 0; 
       
    tcsetattr(fd,TCSAFLUSH,&NewInAttribs);
    cfmakeraw(&NewInAttribs);
}

// ============================================================================
// FUNCTION restoreterm (filno)                                             
// 
// Sets up termios to the old state.                                        
// ============================================================================
void restoreterm(int const fd)
{
    tcsetattr(fd,TCSAFLUSH,&OldInAttribs);
}

int is_iterm (void) {
    fd_set fds;
    char buf[1024];
    int bufpos = 0;
    struct timeval tv;
    int oldflags = fcntl (0, F_GETFL);
    
    buf[0] = 0;
    setupterm (0);
    write (2, "\033]4;-2;?\007", 9);
    
    FD_ZERO (&fds);
    FD_SET (0, &fds);
    
    tv.tv_sec = 0;
    tv.tv_usec = 30000;
    
    fcntl (0, F_SETFL, oldflags | O_NONBLOCK);
    if (select (1, &fds, NULL, NULL, &tv) > 0) {
        bufpos = read (0, buf, 1023);
        buf[bufpos] = 0;
    }
    
    fcntl (0, F_SETFL, oldflags);
    restoreterm (0);
    
    if (*buf) {
        if (memcmp (buf, "\033]4;-2;rgb:", 11) == 0) return 1;
    }
    return 0;
}
