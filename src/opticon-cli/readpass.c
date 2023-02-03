#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct termios NewInAttribs;
struct termios OldInAttribs;

void readpass (char *buf, int size) {
    int fd = 1;
    if(tcgetattr(fd,&OldInAttribs)<0)
        exit(1);
   
    memcpy (&NewInAttribs, &OldInAttribs, sizeof (NewInAttribs));
    NewInAttribs.c_lflag = OldInAttribs.c_lflag & ~(ECHO | ICANON | ISIG);
    NewInAttribs.c_cc[VMIN] = 1;
    NewInAttribs.c_cc[VTIME] = 0; 
       
    tcsetattr(fd,TCSAFLUSH,&NewInAttribs);
    cfmakeraw(&NewInAttribs);
    
	int tmp;
	int pos;
	
	pos = 0;
	
	while (((pos+1) < size) && ((tmp = getchar()) != '\n'))
	{
		if (tmp == 21) pos = 0;
		else if ((tmp == 8) && pos) --pos;
		else buf[pos++] = tmp;
	}
	buf[pos] = 0;
	
	printf ("\n");

    tcsetattr(fd,TCSAFLUSH,&OldInAttribs);
}
