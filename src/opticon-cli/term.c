#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include "term.h"

static struct termios NewInAttribs;
static struct termios OldInAttribs;
terminfo TERMINFO;

// ============================================================================
// FUNCTION setupterm (filno)                                               
// 
// Sets up termios 'our rules' mode, saving the old state.                  
// ============================================================================
void setuptermios(int const fd)
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
void restoretermios(int const fd)
{
    tcsetattr(fd,TCSAFLUSH,&OldInAttribs);
}

void term_init (void) {
    term_getwinsz();
    TERMINFO.name = getenv("TERM");
    TERMINFO.x = 0;
    TERMINFO.y = 0;
    TERMINFO.curcolumn = -1;
    TERMINFO.columnstart = 0;
    TERMINFO.columnend = 0;
    TERMINFO.columnwidth = TERMINFO.cols;
    
    TERMINFO.attr = 0;
    
    if (strcmp (TERMINFO.name, "xterm-256color") == 0) {
        TERMINFO.attr = TERMATTR_RGBCOLOR | TERMATTR_UNICODE;
    }
    else {
        TERMINFO.attr = 0;
    }
    
    if (term_is_iterm()) TERMINFO.attr |= TERMATTR_INLINEGFX;
}

const char *term_write_escape (const char *txt) {
    const char *crsr = txt;
    fputc (*crsr, stdout);
    crsr++;
    if (*crsr == '[') {
        fputc (*crsr, stdout);
        crsr++;
        while (*crsr) {
            fputc (*crsr, stdout);
            if (isalpha (*crsr)) return crsr+1;
            crsr++;
        }
    }
    else if (*crsr == ']') {
        fputc (*crsr, stdout);
        crsr++;
        while (*crsr) {
            fputc (*crsr, stdout);
            if (*crsr == 7) return crsr+1;
            crsr++;
        }
    }
    
    return txt+1;
}

void term_print_tag (char tag) {
    switch (tag) {
        case 'b':
            printf ("\033[1m");
            break;
        
        case '/':
            printf ("\033[0m");
            break;
        
        case 'y':
            printf ("\033[38;5;185m");
            break;
        
        case 'g':
            printf ("\033[38;5;28m");
            break;
            
        case 'c':
            printf ("\033[38;5;45m");
            break;
            
        case 'l':
            printf ("\033[2m");
            break;
        
        case 'B':
            printf ("\033[48;2;48;52;61m");
            break;
            
        default:
            break;
    }
}

void term_write (const char *txt) {
    const char *crsr = txt;
    while (*crsr) {
        unsigned char c = (unsigned char) *crsr;
        
        switch (c) {
            case '\033':
                crsr = term_write_escape (crsr);
                break;
            
            case '<':
                if (crsr[1] == '<') {
                    fputc (c, stdout);
                    crsr += 2;
                }
                else if (crsr[1] && (crsr[2] == '>')) {
                    term_print_tag (crsr[1]);
                    crsr += 3;
                }
                else {
                    if (TERMINFO.x < TERMINFO.columnwidth) {
                        fputc (c, stdout);
                        TERMINFO.x++;
                    }
                    crsr++;
                }
                break;
                
            case '\n':
                fputc (c, stdout);
                term_crsr_setx (0);
                term_advance (1);
                crsr++;
                break;
            
            default:
                if (TERMINFO.x < TERMINFO.columnwidth) {
                    fputc (c, stdout);
                    if (c & 0x80) {
                        if (c & 0x40) TERMINFO.x++;
                    }
                    else TERMINFO.x++;
                }
                crsr++;
                break;
        }
    }
    fflush (stdout);
}

void term_printf (const char *fmt, ...) {
    char str[4096];
    va_list arglist;
    va_start (arglist, fmt);
    vsnprintf (str, 4095, fmt, arglist);
    va_end (arglist);
    str[4095] = 0;
    term_write (str);
}

void term_fill_line (char c) {
    while (TERMINFO.x < TERMINFO.columnwidth) {
        fputc (c, stdout);
        TERMINFO.x++;
    }
}

void term_new_column (void) {
    if (TERMINFO.cols < 160) return;
    TERMINFO.curcolumn++;
    TERMINFO.columnwidth = 80;
    if (TERMINFO.curcolumn == 0) {
        TERMINFO.columnstart = TERMINFO.columnend = TERMINFO.y;
    }
    else {
        if ((80 * (TERMINFO.curcolumn+1)) > TERMINFO.cols) {
            TERMINFO.curcolumn = 0;
            if (TERMINFO.y < TERMINFO.columnend) {
                while (TERMINFO.y < TERMINFO.columnend) {
                    printf ("\n");
                    TERMINFO.y++;
                }
            }
            TERMINFO.columnstart = TERMINFO.columnend = TERMINFO.y;
        }
        else {
            if (TERMINFO.y > TERMINFO.columnstart) {
                printf ("\033[%iA", TERMINFO.y - TERMINFO.columnstart);
                TERMINFO.y = TERMINFO.columnstart;
            }
        }
        
        term_crsr_setx (0);
    }
    int widthleft = TERMINFO.cols - (TERMINFO.curcolumn) * 80;
    if (widthleft < 160) TERMINFO.columnwidth = widthleft;
}

void term_end_column (void) {
    TERMINFO.curcolumn = -1;
    if (TERMINFO.y < TERMINFO.columnend) {
        while (TERMINFO.y < TERMINFO.columnend) {
            printf ("\n");
            TERMINFO.y++;
        }
    }
    TERMINFO.columnwidth = TERMINFO.cols;
    term_crsr_setx (0);
}

void term_advance (int rows) {
    TERMINFO.y += rows;
    if (TERMINFO.curcolumn >= 0) {
        if (TERMINFO.y > TERMINFO.columnend) {
            TERMINFO.columnend = TERMINFO.y;
        }
    }
}

void term_crsr_setx (int inx) {
    int x = inx;
    if (TERMINFO.curcolumn > 0) x += (TERMINFO.curcolumn * 80);
    printf ("\033[%iG", x+1);
    TERMINFO.x = inx;
}

void term_crsr_up (int rows) {
    TERMINFO.y -= rows;
    printf ("\033[%iA", rows);
}

void term_getwinsz (void) {
    struct winsize winsz;
    int c;
    ioctl (0, TIOCGWINSZ, (char *) &winsz);
    TERMINFO.cols = winsz.ws_col ? winsz.ws_col : 80;
    TERMINFO.rows = winsz.ws_row ? winsz.ws_row : 24;
}

void term_print_icns (int height, resource *icns) {
    if (TERMINFO.attr & TERMATTR_INLINEGFX) {
        int output_length = icns->sz / 4 * 3;
        if (icns->sz > 2) {
            if (icns->data[icns->sz-1] == '=') output_length--;
            if (icns->data[icns->sz-2] == '=') output_length--;
        }
        
        printf ("\033]1337;File=name=aWNvbi5wbmcK;height=%i;size=%i;"
                "inline=1:", height, output_length);
        fwrite (icns->data, 1, icns->sz, stdout);
        printf ("\007\n");
        term_advance (height);
        term_crsr_setx (0);
    }
}

void term_print_hdr (const char *title, resource *rs) {
    if (TERMINFO.attr & TERMATTR_INLINEGFX) {
        term_printf ("\n\n<B>           <b>%s", title);
        if (TERMINFO.curcolumn == 0) {
            int tmp = TERMINFO.columnwidth;
            TERMINFO.columnwidth = TERMINFO.cols;
            term_fill_line (' ');
            TERMINFO.columnwidth = tmp;
        }
        else {
            term_fill_line (' ');
        }
        term_printf ("</>\n\n");
        term_crsr_up (3);
        term_crsr_setx (2);
        term_print_icns (3, rs);
        term_printf ("\n");
    }
    else {
        char *utitle = strdup (title);
        for (int i=0; utitle[i]; ++i) {
            utitle[i] = toupper (utitle[i]);
        }
        term_printf ("<b>---( </><c>%s</><b> )", utitle);
        term_fill_line ('-');
        term_write ("</>\n");
        free (utitle);
    }
}

int term_is_iterm (void) {
    fd_set fds;
    char buf[1024];
    int bufpos = 0;
    struct timeval tv;
    int oldflags = fcntl (0, F_GETFL);
    
    buf[0] = 0;
    setuptermios (0);
    write (2, "\033]4;-2;?\007", 9);
    
    FD_ZERO (&fds);
    FD_SET (0, &fds);
    
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    
    fcntl (0, F_SETFL, oldflags | O_NONBLOCK);
    if (select (1, &fds, NULL, NULL, &tv) > 0) {
        bufpos = read (0, buf, 1023);
        buf[bufpos] = 0;
    }
    
    fcntl (0, F_SETFL, oldflags);
    restoretermios (0);
    
    if (*buf) {
        if (memcmp (buf, "\033]4;-2;rgb:", 11) == 0) return 1;
    }
    return 0;
}

/*
int main (void) {
    term_init();
    term_new_column();
    term_print_hdr ("Test", &rsrc.icns.computer);
    term_write ("Hello, <y>world</> how are <b>you</> doing?\n"
                "This is a test of the column system, "
                "I hope it goes well.\n\n");

    term_new_column();
    term_print_hdr ("Another Column", &rsrc.icns.gauge);
    term_write ("This is the <g>second</> column. It should work fine.\n");

    term_end_column();
    term_print_hdr ("And a Full Column", &rsrc.icns.procs);
    term_write ("Hurray! A <b>third</> one appears!\n");
    return 0;
}
*/