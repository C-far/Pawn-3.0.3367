/*  Pawn debugger
 *
 *  Minimal debugger with a console interface.
 *
 *  Copyright (c) ITB CompuPhase, 1998-2005
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *
 *  Version: $Id: pawndbg.c 3353 2005-07-17 20:02:31Z thiadmer $
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#if defined READLINE
  #define __READLINE_IMPORT__   1
  #define __P(protos) protos
  #include <readline/readline.h>
  #include <readline/history.h>
#endif
#include "osdefs.h"     /* for _MAX_PATH */
#include "amx.h"
#include "amxdbg.h"

#if defined __WIN32 || defined _WIN32 || defined __WIN32__ || defined __MSDOS__ || defined __WATCOMC__
  #include <conio.h>
  #if defined __WIN32 || defined _WIN32 || defined __WIN32__ || defined __WATCOMC__
    #if !defined __WIN32__
      #define __WIN32__ 1
    #endif
    #include <windows.h>
    #if !defined amx_Init && !defined NO_WIN32_CONSOLE && !defined AMX_TERMINAL
      #define WIN32_CONSOLE
    #endif
  #endif
#elif !defined macintosh
  #include "../linux/getch.h"
  #include <fcntl.h>
  #include <termios.h>
  #include <unistd.h>
#endif


#define MAXSTACKTRACE   128
#define MAXLINELENGTH   128
#define MAX_DIMS        2       /* number of array dimensions */
#define TABSIZE         8
#define EATLINE         5       /* the number of characters for the line number */
#define STD_COLUMNS     80      /* number of characters that fit on a line */
#if defined amx_Init
  #define STD_LINES     29      /* number of lines that fit on a screen */
#else
  #define STD_LINES     24      /* number of lines that fit on a screen */
#endif
#define WATCHLINES      4       /* default number of watches displayed */
#define LISTLINES       (STD_LINES - WATCHLINES - 10) /* default code lines */

#if defined AMX_TERMINAL || defined amx_Init
  /* required functions are implemented elsewhere */
  int amx_printf(char *,...);
  int amx_putchar(int);
  int amx_fflush(void);
  int amx_getch(void);
  char *amx_gets(char *,int);
  int amx_termctl(int,int);
  void amx_clrscr(void);
  void amx_clreol(void);
  void amx_gotoxy(int x,int y);
  void amx_wherexy(int *x,int *y);
  unsigned int amx_setattr(int foregr,int backgr,int highlight);
  void amx_console(int columns, int lines);
  #if defined amx_Init
    #define STR_PROMPT  "dbg\xbb "
    #define CHR_HLINE   '\x97'
  #else
    #define STR_PROMPT  "dbg> "
    #define CHR_HLINE   '-'
  #endif
  #define CHR_VLINE     '|'
#elif defined VT100 || defined LINUX || defined ANSITERM
  /* ANSI/VT100 terminal, or shell emulating "xterm" */
  #if !defined VT100 && !defined ANSITERM && defined LINUX
    #define VT100
  #endif
  #define amx_printf      printf
  #define amx_putchar(c)  putchar(c)
  #define amx_fflush()    fflush(stdout)
  #define amx_getch()     getch()
  #define amx_gets(s,n)   fgets(s,n,stdin)
  int amx_termctl(int,int);
  void amx_clrscr(void);
  void amx_clreol(void);
  void amx_gotoxy(int x,int y);
  void amx_wherexy(int *x,int *y);
  unsigned int amx_setattr(int foregr,int backgr,int highlight);
  void amx_console(int columns, int lines);
  #define STR_PROMPT    "dbg> "
  #define CHR_HLINE     '-'
  #define CHR_VLINE     '|'
#elif defined WIN32_CONSOLE
  /* Win32 console */
  #define amx_printf      printf
  #define amx_putchar(c)  putchar(c)
  #define amx_fflush()    fflush(stdout)
  #define amx_getch()     getch()
  #define amx_gets(s,n)   fgets(s,n,stdin)
  int amx_termctl(int,int);
  void amx_clrscr(void);
  void amx_clreol(void);
  void amx_gotoxy(int x,int y);
  void amx_wherexy(int *x,int *y);
  unsigned int amx_setattr(int foregr,int backgr,int highlight);
  void amx_console(int columns, int lines);
  #define STR_PROMPT    "dbg\xaf "
  #define CHR_HLINE     '\xc4'
  #define CHR_VLINE     '\xb3'
#elif defined(USE_CURSES)
  /* Use the "curses" library to implement the console */
  const int _False = 0;     /* to avoid compiler warnings */
  #define amx_printf        printw
  #define amx_putchar(c)    addch(c)
  #define amx_fflush()      (0)
  #define amx_getch()       getch()
  #define amx_gets(s,n)     getnstr(s,n)
  #define amx_clrscr()      (void)(0)
  #define amx_clreol()      (void)(0)
  #define amx_gotoxy(x,y)   (void)(0)
  #define amx_wherexy(x,y)  (*(x)=*(y)=0)
  #define amx_setattr(c,b,h) (_False)
  #define amx_termctl(c,v)  (_False)
  #define amx_console(c,l)  (void)(0)
  #define STR_PROMPT        "dbg> "
  #define CHR_HLINE         '-'
  #define CHR_VLINE         '|'
#else
  /* assume a streaming terminal; limited features (no colour, no cursor
   * control)
   */
  const int _False = 0;     /* to avoid compiler warnings */
  #define amx_printf        printf
  #define amx_putchar(c)    putchar(c)
  #define amx_fflush()      fflush(stdout)
  #define amx_getch()       getch()
  #define amx_gets(s,n)     fgets(s,n,stdin)
  #define amx_clrscr()      (void)(0)
  #define amx_clreol()      (void)(0)
  #define amx_gotoxy(x,y)   (void)(0)
  #define amx_wherexy(x,y)  (*(x)=*(y)=0)
  #define amx_setattr(c,b,h) (_False)
  #define amx_termctl(c,v)  (_False)
  #define amx_console(c,l)  (void)(0)
  #define STR_PROMPT        "dbg> "
  #define CHR_HLINE         '-'
  #define CHR_VLINE         '|'
#endif
#define CHR_CURLINE         '*'
#if defined VT100
  #define CHR_HLINE_VT100   'q' // in alternate font
  #define CHR_VLINE_VT100   'x'
#endif

enum {
  BP_NONE,
  BP_CODE,
  BP_DATA,    /* ??? not implemented */
  /* --- */
  BP_TYPES
};

enum {
  DISP_DEFAULT = 0x10,
  DISP_STRING  = 0x20,
  DISP_BIN     = 0x30,   /* ??? not implemented */
  DISP_HEX     = 0x40,
  DISP_BOOL    = 0x50,
  DISP_FIXED   = 0x60,
  DISP_FLOAT   = 0x70,
};
#define DISP_MASK 0x0f

typedef struct tagBREAKPOINT {
  struct tagBREAKPOINT *next;
  int type;             /* one of the BP_xxx types */
  ucell addr;           /* address (in code or data segment) */
  const char *name;     /* name of the symbol (function) */
  int number;           /* sequential breakpoint number (to refer to the breakpoint) */
} BREAKPOINT;

typedef struct tagNAMELIST {
  struct tagNAMELIST *next;
  char *name;
  int number;
} NAMELIST;

enum {
  REMOTE_NONE,  /* this means "not remote" */
  REMOTE_RS232,
  REMOTE_UDP,   /* ??? not implemented */
  /* --- */
  REMOTE_TYPES
};

enum {
  STEPPING, /* step into functions */
  STEPOVER, /* step over functions */
  RUNNING,  /* just run */
};
static int runmode;     /* running or tracing */

static const char *curfilename; /* pointer to the name of the "active" file */
static char **cursource;
static int autolist=1;
static int screencolumns=STD_COLUMNS;
static int screenlines=STD_LINES;
static int listlines=LISTLINES;
static int watchlines=WATCHLINES;
static BREAKPOINT breakpoints={ NULL };
static NAMELIST watches={ NULL };
static int curtopline;  /* current line that is on top in the list */
static int recentline=-1;
static int terminal;
static int remote=REMOTE_NONE;
static char chr_hline=CHR_HLINE;
static char chr_vline=CHR_VLINE;
#if defined __WIN32__
  HANDLE hCom=INVALID_HANDLE_VALUE;
#elif !defined __MSDOS__
  int fdCom;
  struct termios oldtio, newtio;
#endif
static char remote_pendingbuf[30];
static int remote_pendingsize=0;


static void draw_hline(int forceinit)
{
static char hline_str[256] = "";  /* number of columns in a window should
                                   * never exceed 255 */

  /* initialize the string, if not yet done */
  if (forceinit || strlen(hline_str)==0) {
    memset(hline_str,chr_hline,sizeof hline_str);
    hline_str[screencolumns]='\0';
  } /* if */

  #if defined VT100
    if (terminal)
      amx_printf("\016");          /* SO code to select the graphic set */
  #endif

  amx_printf(hline_str);

  #if defined VT100
    if (terminal)
      amx_printf("\017");          /* SI code to select the standard set */
  #endif

  amx_fflush();
}

#if defined WIN32_CONSOLE
void win32_getscreensize(int *width,int *height)
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  assert(width!=NULL && height!=NULL);
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&csbi);
  *width=(int)csbi.dwSize.X;
  *height=(int)(csbi.srWindow.Bottom-csbi.srWindow.Top+1);
}
#endif

static int csrsave_x, csrsave_y;
#if !defined NDEBUG
  static int csrsave_flag=0;
#endif

static void term_csrsave(void)
{
  assert(terminal);
  #if !defined NDEBUG
    assert(csrsave_flag==0);
    csrsave_flag++;
  #endif
  amx_wherexy(&csrsave_x,&csrsave_y);
}

static void term_csrrestore(void)
{
  assert(terminal);
  #if !defined NDEBUG
    assert(csrsave_flag==1);
    csrsave_flag--;
  #endif
  amx_gotoxy(csrsave_x,csrsave_y);
}

static void term_refresh(int recalc)
{
  assert(terminal);

  if (recalc) {
    listlines=(screenlines-watchlines)/2;
    if (listlines<LISTLINES)
      listlines=LISTLINES;
  } /* if */

  amx_clrscr();
  amx_gotoxy(1,watchlines+1);
  draw_hline(1);
  amx_gotoxy(1,watchlines+listlines+2);
  draw_hline(0);
  amx_gotoxy(1,watchlines+listlines+3);
}

static void term_statusbar(int show)
{
  #if defined __WIN32__
    amx_gotoxy(1,screenlines+1);
    if (show) {
      amx_setattr(0,7,0);
      amx_printf(" F1 Help ");
      amx_setattr(7,0,0);
      amx_printf("      ");
      amx_setattr(0,7,0);
      amx_printf(" F3 Quit ");
      amx_setattr(7,0,0);
      amx_printf(" ");
      #if !defined __WIN32__
        amx_setattr(0,7,0);
        amx_printf("F4 Output");
        amx_setattr(7,0,0);
        amx_printf(" ");
      #else
        amx_printf("      ");
      #endif
      amx_setattr(0,7,0);
      amx_printf(" F5 Go  ");
      amx_setattr(7,0,0);
      amx_printf(" ");
      amx_setattr(0,7,0);
      amx_printf(" F6 Show ");
      amx_setattr(7,0,0);
      amx_printf("      ");
      amx_setattr(0,7,0);
      amx_printf(" F8 Step ");
      amx_setattr(7,0,0);
      amx_printf("      ");
      amx_setattr(0,7,0);
      amx_printf(" F10 Next");
      amx_setattr(7,0,0);
    } else {
      amx_clreol();
    } /* if */
    amx_gotoxy(6,screenlines);
  #endif
}

static void term_open(int columns,int lines)
{
  terminal=1;
  amx_clrscr();
  amx_termctl(1,0);             /* disable "auto-wrap" */
  amx_console(columns,lines);   /* try to fix the requested console size */
  #if defined VT100
    chr_hline=CHR_HLINE_VT100;
    chr_vline=CHR_VLINE_VT100;
    /* a trick to get the size of the terminal is to position the cursor far
     * away and then read it back
     */
    amx_gotoxy(999,999);
    amx_wherexy(&screencolumns,&screenlines);
    screenlines--;      /* keep last line empty */
    amx_printf("\033[%d;%dr",watchlines+listlines+3,STD_LINES-1); /* set window */
    amx_printf("\033)0");                  /* select graphics codes for set G1 */
  #endif
  #if defined WIN32_CONSOLE
    #define ENABLE_INSERT_MODE      0x0020
    #define ENABLE_QUICK_EDIT_MODE  0x0040
    #define ENABLE_EXTENDED_FLAGS   0x0080
    #define ENABLE_AUTO_POSITION    0x0100
    win32_getscreensize(&screencolumns,&screenlines);
    screenlines--;      /* keep last line empty */
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),
                   ENABLE_ECHO_INPUT | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT
                   | ENABLE_MOUSE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE
                   | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
  #elif defined READLINE
    rl_get_screen_size(&screencolumns,&screenlines);
    screenlines--;      /* keep last line empty */
  #endif
  term_refresh(1);
}

static void term_close(void)
{
  assert(terminal);
  #if defined VT100
    amx_printf("\033[r");                  /* scrolling region is full screen */
    screencolumns=STD_COLUMNS;
    chr_hline=CHR_HLINE;
    chr_vline=CHR_VLINE;
  #endif
  amx_termctl(1,1);                        /* re-enable "auto-wrap" */
  amx_clrscr();
  screencolumns=STD_COLUMNS;
  terminal=0;
}

static void term_scroll(int top, int bottom, int lines)
{
  #if defined WIN32_CONSOLE
    if (terminal) {
      SMALL_RECT ScrollRectangle;
      COORD Destination;
      CHAR_INFO Fill;
      int screenlines,screencolumns;

      win32_getscreensize(&screencolumns,&screenlines);
      ScrollRectangle.Left=0;
      ScrollRectangle.Top=(short)(top-1);
      ScrollRectangle.Right=(short)(screencolumns-1);
      ScrollRectangle.Bottom=(short)(bottom-1);
      Destination.X=0;
      Destination.Y=(short)(top-1-lines);
      Fill.Char.UnicodeChar=' ';
      Fill.Attributes=0x07;
      ScrollConsoleScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE),
                                &ScrollRectangle,&ScrollRectangle,
                                Destination,&Fill);
    } /* if */
  #else
    (void)top;
    (void)bottom;
    (void)lines;
  #endif
}

static int term_switch(int number)
{
  if (terminal)
    return amx_termctl(2,number);
  return 0;
}

static void source_free(char **source)
{
  int i;

  assert(source!=NULL);
  for (i=0; source[i]!=NULL; i++)
    free(source[i]);
  free(source);
}

static char **source_load(const char *filename)
{
  char **source;
  FILE *fp;
  char line[256];
  int lines,i;

  /* open the file, number of characters */
  assert(filename!=NULL);
  if ((fp=fopen(filename,"rt"))==NULL)
    return NULL;
  lines=0;
  while (fgets(line,sizeof(line),fp)!=NULL)
    lines++;

  /* allocate memory, reload the file */
  if ((source=(char **)malloc((lines+1)*sizeof(char *)))==NULL) {
    fclose(fp);
    return NULL;
  } /* if */
  for (i=0; i<=lines; i++)      /* initialize, so that source_free() works */
    source[i]=NULL;
  rewind(fp);
  i=0;
  while (fgets(line,sizeof(line),fp)!=NULL) {
    assert(i<lines);
    source[i]=strdup(line);
    if (source[i]==NULL) {
      fclose(fp);
      source_free(source);      /* free everything allocated so far */
      return NULL;
    } /* if */
    i++;
  } /* if */

  fclose(fp);
  return source;
}

static void source_list(AMX_DBG *amxdbg,int startline,int numlines,int curline)
{
  /* cursource and curline should already have been set */
  int result,lastline;
  ucell address;

  if (terminal) {
    term_csrsave();
    amx_gotoxy(1,watchlines+2);
    numlines=listlines;  /* override user setting */
  } /* if */

  if (startline<0)
    startline=0;
  lastline=startline+numlines;
  curtopline=startline; /* save line that is currently displayed at the top */

  /* seek to line number from the start (to avoid displaying something
   * beyond the file)
   */
  for (result=0; cursource[result]!=NULL && result<startline; result++)
    /* nothing */;
  if (cursource[result]!=NULL) {
    assert(result==startline);
    while (cursource[startline]!=NULL && startline<lastline) {
      char c1='\0',c2='\0';
      if (terminal)
        amx_clreol();
      if ((int)strlen(cursource[startline])>screencolumns-EATLINE) {
        c1=cursource[startline][screencolumns-EATLINE-1];
        c2=cursource[startline][screencolumns-EATLINE];
        cursource[startline][screencolumns-EATLINE-1] = '\n';
        cursource[startline][screencolumns-EATLINE] = '\0';
      } /* if */
      amx_setattr(-1,-1,startline==curline);
      if (!terminal) {
        if (startline==curline)
          amx_printf("%3d%c %s",startline+1,CHR_CURLINE,cursource[startline]);
        else
          amx_printf("%3d  %s",startline+1,cursource[startline]);
      } else {
        /* see if there is a breakpoint on the line */
        if (amxdbg!=NULL && dbg_GetLineAddress(amxdbg,startline,curfilename,&address)==AMX_ERR_NONE) {
          BREAKPOINT *bp;
          for (bp=breakpoints.next; bp!=NULL; bp=bp->next) {
            assert(bp->type!=BP_NONE);
            if (bp->type==BP_CODE && bp->addr==address)
              amx_setattr(-1,0x1,1);
          } /* for */
        } /* if */
        amx_printf("%4d",startline+1);
        #if defined VT100
          if (terminal)
            amx_printf("\016");    /* SO code to select the graphic set */
        #endif
        amx_printf("%c",chr_vline);
        #if defined VT100
          if (terminal)
            amx_printf("\017");    /* SI code to select the standard set */
        #endif
        amx_setattr(-1,0x0,startline==curline);
        amx_printf("%s",cursource[startline]);
      } /* if */
      if (c1!='\0') {
        cursource[startline][screencolumns-EATLINE-1] = c1;
        cursource[startline][screencolumns-EATLINE] = c2;
      } /* if */
      startline++;
    } /* while */
  } /* if */

  if (terminal) {
    amx_setattr(-1,-1,0);
    while (startline<lastline) {
      amx_clreol();
      amx_printf("\n");
      startline++;
    } /* while */
    term_csrrestore();
  } /* if */
}

static int source_lines(void)
{
  int i;

  assert(cursource!=NULL);
  for (i=0; cursource[i]!=NULL; i++)
    /* nothing */;
  return i;
}

static int gettopline(int line,int topline)
{
  if (!terminal)
    return topline;
  if (line<curtopline || line>=curtopline+listlines)
    return topline;
  return curtopline;
}

static char *skipwhitespace(const char *str)
{
  while (*str==' ' || *str=='\t')
    str++;
  return (char*)str;
}

static char *skipvalue(const char *str)
{
  while (isdigit(*str))
    str++;
  return skipwhitespace(str);
}

static const char *skippath(const char *str)
{
  const char *p1,*p2;

  /* DOS/Windows pathnames */
  if ((p1=strrchr(str,'\\'))!=NULL)
    p1++;
  else
    p1=str;
  /* Unix pathnames */
  if ((p2=strrchr(str,'/'))!=NULL)
    p2++;
  else
    p2=str;
  return p1>p2 ? p1 : p2;
}

static void scroll_cmdwin(int lines)
{
  if (terminal) {
    int csrx,csry;
    amx_wherexy(&csrx,&csry);
    if (csry==screenlines) {
      term_scroll(watchlines+listlines+3,screenlines-1,lines);
      amx_gotoxy(csrx,csry-1);
    } /* if */
  } /* if */
}

#if !defined NO_REMOTE
/* For the remote debugging capabilities, the "device" is the apparatus/PC on
 * which the script runs and the "host" is the PC on which the debugger runs.
 * Both the device and the host load the script. The device typically does not
 * load the debugging info. The host would not really need to load the opcodes,
 * but its taks becomes a lot simpler when it has locally an image of what runs
 * on the device.
 *
 * Handshake
 *
 * The first step, after opening the connection, is the handshake. For that,
 * the host must send a packet with the single character "!". Since the device
 * may not be "on-line" immediately and packets may get lost, the host should
 * repeat sending these characters until the device reponds.
 *
 * When the device starts up and loads a script, it should check whether this
 * script contains debugging information (even if it does not load the debugging
 * information). If debugging information is present, it should check for the
 * reception of packets with the character "!". Upon reception, it replies with
 * a return packed with the character "@". The device should have a time-out on
 * the polling loop for receivng the "!" packets, because the script should run
 * normally when no debugger is attached.
 *
 * Running
 *
 * After entering in debug mode, the device will send a packed with the
 * character "@" followed by the new instruction pointer address (CIP) at every
 * "BREAK" instruction. The device then waits for a response from the host (the
 * script is halted). To continue running, the host simply sends a packet with
 * the single character "!".
 *
 * If, instead of allowing the device to continue running the script, you want
 * to halt execution (e.g. because a breakpoint is reached), the first command
 * that the host should send is the string "?R\n", where \n stands for a
 * newline. Upon reception, the device must reply with a more complete state of
 * the abstract machine: the stack address, the frame pointer and the heap top.
 *
 * With the few registers sent over, the state of the abstract machine (on the
 * device) is now known to the host, but it does not know the contents of the
 * data memory on the device: the values of global and local variables. It was
 * estimated too costly to transfer all of this data memory at every "stop".
 * Instead, the host must query for the range of memory that it wants. It does
 * so by sending a packet with the contents:
 *
 *          ?Maddress,size\n
 *
 * where "address" and "size" are hexadecimal numbers. The size is in cells;
 * the address does not have to be cell-aligned, but it typically is. The
 * device responds to this command by sending a packet with an "@" character
 * followed by one or more hexadecimal values, separated with commas and
 * terminated with a newline.
 *
 * Other (suggested) commands are:
 *
 *          ?U\n      unhook the debugger
 *          ?L\n      send over new script (re-initialize)
 *
 * Notes
 *
 * The rationale between the design is performance, and especially attempting to
 * avoid that the communication link becomes the bottleneck. Therefore, the
 * device and the host send over as little as possible until it is known that
 * the device should be halted. In the case of a serial link, data is sent byte
 * by byte. When the device is halted (on a breakpoint) and
 * waiting for user interaction, performance is no longer as important.
 *
 * There is a special case in the handshaking stage when the device is already
 * running in debug mode (because an earlier session was aborted). In that case,
 * it will respond with "@" and the new instruction pointer (CIP) address.
 */
static int remote_rs232(int port,int baud)
{
  #if defined __WIN32__
    DCB    dcb;
    COMMTIMEOUTS commtimeouts;
  #endif
  unsigned long size,cip;
  char buffer[40];

  /* optionally issue a "close-down" request for the remote host */
  #if defined __WIN32__
  if (baud==0 && hCom!=INVALID_HANDLE_VALUE) {
  #else
  if (baud==0 && fdCom>=0) {
  #endif
    sprintf(buffer,"?U\n");
    #if defined __WIN32__
      WriteFile(hCom,buffer,strlen(buffer),&size,NULL);
    #else
      write(fdCom,buffer,strlen(buffer));
    #endif
    /* do not wait for a reply */
  } /* if */

  /* set up the connection */
  #if defined __WIN32__
    if (hCom!=INVALID_HANDLE_VALUE)
      CloseHandle(hCom);
    if (baud==0)
      return 0;
    sprintf(buffer,"com%d:",port+1);
    hCom=CreateFile(buffer,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (hCom==INVALID_HANDLE_VALUE)
      return 0;
    GetCommState(hCom,&dcb);
    dcb.BaudRate=baud;
    dcb.ByteSize=8;
    dcb.StopBits=ONESTOPBIT;
    dcb.Parity=NOPARITY;
    SetCommState(hCom,&dcb);
    SetCommMask(hCom,EV_RXCHAR|EV_TXEMPTY);
    commtimeouts.ReadIntervalTimeout        =0x7fffffff;
    commtimeouts.ReadTotalTimeoutMultiplier =0;
    commtimeouts.ReadTotalTimeoutConstant   =1;
    commtimeouts.WriteTotalTimeoutMultiplier=0;
    commtimeouts.WriteTotalTimeoutConstant  =0;
    SetCommTimeouts(hCom,&commtimeouts);
  #else
    if (fdCom>=0) {
      tcflush(fdCom,TCOFLUSH);
      tcflush(fdCom,TCIFLUSH);
      tcsetattr(fdCom,TCSANOW,&oldtio);
      close(fdCom);
    } /* if */
    if (baud==0)
      return 0;
    sprintf(buffer,"/dev/ttyS%d",port);
    fdCom = open(buffer,O_RDWR|O_NOCTTY|O_NONBLOCK);
    if (fdCom<0)
      return 0;
  	/* clear input & output buffers, then switch to "blocking mode" */
	  tcflush(fdCom,TCOFLUSH);
	  tcflush(fdCom,TCIFLUSH);
	  fcntl(fdCom,F_SETFL,fcntl(fdCom,F_GETFL) & ~O_NONBLOCK);

    tcgetattr(fdCom,&oldtio); /* save current port settings */
    bzero(&newtio,sizeof newtio);
    newtio.c_cflag=CS8 | CLOCAL | CREAD;

    switch (baud) {
    case 1152000: newtio.c_cflag |= B1152000; break;
    case  576000: newtio.c_cflag |=  B576000; break;
    case  230400: newtio.c_cflag |=  B230400; break;
    case  115200: newtio.c_cflag |=  B115200; break;
    case   57600: newtio.c_cflag |=   B57600; break;
    case   38400: newtio.c_cflag |=   B38400; break;
    case   19200: newtio.c_cflag |=   B19200; break;
    case    9600: newtio.c_cflag |=    B9600; break;
    default:      return 0;
    } /* switch */
    newtio.c_iflag=IGNPAR | IGNBRK | IXON | IXOFF;
    newtio.c_oflag=0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag=0;

    cfmakeraw(&newtio);
    newtio.c_cc[VTIME]=1;   /* inter-character timer used */
    newtio.c_cc[VMIN] =0;   /* blocking read until 0 chars received */
    tcflush(fdCom,TCIFLUSH);
    tcsetattr(fdCom,TCSANOW,&newtio);
  #endif

  /* handshake, send token and wait for a reply */
  do {
    #if defined __WIN32__
      WriteFile(hCom,"!",1,&size,NULL);
      Sleep(100);
      do {
        ReadFile(hCom,buffer,1,&size,NULL);
      } while (size>0 && buffer[0]!='@');
      Sleep(100);
      /* read remaining buffer (if any) */
      ReadFile(hCom,buffer+1,sizeof buffer - 1,&size,NULL);
      size++; /* add size of the handshake character */
    #else
      write(fdCom,"!",1);
      usleep(100*1000);
      do {
        size=read(fdCom,buffer,1);
      } while (size>0 && buffer[0]!='@');
      usleep(100*1000);
      /* read remaining buffer (if any) */
      size=read(fdCom,buffer+1,sizeof buffer - 1);
      size++; /* add size of the handshake character */
    #endif
  } while (size==0 || buffer[0]!='@');
  if (sscanf(buffer,"@%lx",&cip)>0) {
    /* if the script is already running, give a "go" command */
    #if defined __WIN32__
      WriteFile(hCom,"!",1,&size,NULL);
    #else
      write(fdCom,"!",1);
    #endif
  } else if (size>1 && size<sizeof remote_pendingbuf) {
    remote_pendingsize=size-1;
    memcpy(remote_pendingbuf,buffer+1,remote_pendingsize);
  } /* if */

  remote=REMOTE_RS232;
  return 1;
}

static int remote_wait_rs232(AMX *amx)
{
  char buffer[50],*ptr;
  unsigned long size;
  long cip;
  int offs,state;
  enum { SCAN, START, FINISH };

  for ( ;; ) {
    offs=0;
    size=0;
    state=SCAN;
    while (state!=FINISH) {
      offs+=size;
      if (offs>=sizeof buffer - 1) {
        amx_printf("%s",buffer);
        offs=0;
        state=SCAN;
      } /* if */
      /* read a buffer, see if we can find the start condition */
      do {
        if (remote_pendingsize>0) {
          assert(remote_pendingsize<sizeof buffer);
          size=remote_pendingsize;
          memcpy(buffer,remote_pendingbuf,remote_pendingsize);
          remote_pendingsize=0;
        } else {
          #if defined __WIN32__
            ReadFile(hCom,buffer+offs,sizeof buffer - offs - 1,&size,NULL);
          #else
            size=read(fdCom,buffer+offs,sizeof buffer - offs - 1);
          #endif
        } /* if */
      } while (size==0 && remote_pendingsize==0);
      assert(size+offs<sizeof buffer);
      buffer[size+offs]='\0';   /* force zero-termination */
      if (state==SCAN) {
        for (ptr=buffer; (unsigned long)(ptr-buffer)<size && *ptr!='@'; ptr++)
          /* nothing */;
        if ((unsigned long)(ptr-buffer)>=size) {
          amx_printf("%s",buffer);
        } else {
          memmove(buffer,ptr,offs);
          state=START;
        } /* if */
      } /* if */
      if (state==START) {
        for (ptr=buffer; (unsigned long)(ptr-buffer)<(size+offs) && *ptr!='\n'; ptr++)
          /* nothing */;
        if (*ptr=='\n') {
          state=FINISH;
          if (strlen(++ptr)>0)
            amx_printf("%s",ptr);
        } /* if */
      } /* if */
    } /* while */
    amx_fflush();
    /* we found a line starting with '@' and ending '\n'; now check the validity
     * of the line
     */
    if (sscanf(buffer,"@%lx",&cip)==1) {
      amx->cip=(cell)cip;
      return 1;
    } else {
      /* unknown buffer format; print and continue */
      amx_printf("%s",buffer);
    } /* if */
  } /* for */
}

static void remote_resume_rs232(void)
{
  #if defined __WIN32__
    unsigned long size;
    WriteFile(hCom,"!",1,&size,NULL);
  #else
    write(fdCom,"!",1);
  #endif
}

static void remote_sync_rs232(AMX *amx)
{
  char buffer[128];
  unsigned long size;
  long frm,stk,hea;
  int len,retry;

  for ( ;; ) {
    #if defined __WIN32__
      WriteFile(hCom,"?R\n",3,&size,NULL);
    #else
      write(fdCom,"?R\n",3);
    #endif

    len=0;
    retry=10;
    buffer[0]='\0';
    do {
      #if defined __WIN32__
        ReadFile(hCom,buffer+len,sizeof buffer-len,&size,NULL);
      #else
        size=read(fdCom,buffer+len,sizeof buffer-len);
      #endif
      if (len+size<sizeof buffer)
        buffer[len+size]='\0';  /* force zero termination */
      if (size==0) {
        #if defined __WIN32__
          Sleep(50);
        #else
          usleep(50*1000);
        #endif
      } /* if */
    } while (strchr(buffer,'\n')==NULL && --retry>=0);
    if (sscanf(buffer,"@%lx,%lx,%lx",&frm,&stk,&hea)==3) {
      amx->frm=(cell)frm;
      amx->stk=(cell)stk;
      amx->hea=(cell)hea;
      return;
    } /* if */
  } /* for */
}

static void remote_read_rs232(AMX *amx,cell vaddr,int number)
{
  char buffer[128];
  char *ptr;
  unsigned long size;
  int len;
  cell val;
  cell *cptr;

  while (number>0) {
    sprintf(buffer,"?M%lx,%x\n",(long)vaddr,(number>10) ? 10 : number);
    len=strlen(buffer);
    #if defined __WIN32__
      WriteFile(hCom,buffer,len,&size,NULL);
    #else
      write(fdCom,buffer,len);
    #endif

    len=0;
    do {
      #if defined __WIN32__
        ReadFile(hCom,buffer+len,sizeof buffer-len,&size,NULL);
      #else
        size=read(fdCom,buffer+len,sizeof buffer-len);
      #endif
      if (len+size<sizeof buffer)
        buffer[len+size]='\0';  /* force zero termination */
    } while (size==0 || buffer[0]!='@' || strchr(buffer,'\n')==NULL);
    ptr=buffer+1;       /* skip '@' */
    while (number>0 && (ptr-buffer)<sizeof buffer && *ptr!='\0') {
      val=strtol(ptr,&ptr,16);
      if (amx_GetAddr(amx,vaddr,&cptr)==AMX_ERR_NONE)
        *cptr=val;
      number--;
      vaddr+=sizeof(cell);
      while (*ptr!='\0' && *ptr<=' ' || *ptr==',')
        ptr++;          /* skip optional comma (and whitespace) */
    } /* while */
  } /* while */
}
#endif

static void namelist_init(NAMELIST *root)
{
  NAMELIST *next;

  while (root->next!=NULL) {
    next=root->next;
    root->next=next->next;      /* unlink */
    free(next->name);           /* then free */
    free(next);
  } /* while */
}

static NAMELIST *namelist_find(NAMELIST *root,int number)
{
  NAMELIST *name;

  for (name=root->next; name!=NULL && name->number!=number; name=name->next)
    /* nothing */;
  return name;
}

static int namelist_delete(NAMELIST *root,NAMELIST *item)
{
  NAMELIST *cur;

  /* find the item */
  assert(root!=NULL);
  cur=root;
  while (cur->next!=NULL && cur->next!=item)
    cur=cur->next;
  assert(cur!=NULL);
  if (cur->next==NULL)
    return 0;                   /* item not found */

  cur->next=item->next;         /* unlink */
  free(item->name);             /* then free */
  free(item);

  return 1;
}

static NAMELIST *namelist_add(NAMELIST *root,char *name,int number)
{
  NAMELIST *cur, *pred;

  /* allocate a structure */
  cur=malloc(sizeof(NAMELIST));
  if (cur==NULL)
    return NULL;
  cur->name=strdup(name);
  cur->number=number;
  if (cur->name==NULL) {
    free(cur);
    return NULL;
  } /* if */

  /* link it in the list */
  assert(root!=NULL);
  for (pred=root; pred->next!=NULL && pred->next->number<number; pred=pred->next)
    /* nothing */;
  cur->next=pred->next;
  pred->next=cur;

  return cur;
}

static int namelist_count(NAMELIST *root)
{
  int count = 0;
  NAMELIST *name;

  for (name=root->next; name!=NULL; name=name->next)
    count++;
  return count;
}

static int get_symbolvalue(AMX *amx,AMX_DBG_SYMBOL *sym,int index,cell *value)
{
  cell *vptr;
  cell base=sym->address;
  if (sym->vclass & DISP_MASK)
    base+=amx->frm;     /* addresses of local vars are relative to the frame */
  if (sym->ident==iREFERENCE || sym->ident==iREFARRAY) {   /* a reference */
    amx_GetAddr(amx,base,&vptr);
    base=*vptr;
  } /* if */
  #if !defined NO_REMOTE
    if (remote==REMOTE_RS232)
      remote_read_rs232(amx,(cell)(base+index*sizeof(cell)),1);
  #endif
  if (amx_GetAddr(amx,(cell)(base+index*sizeof(cell)),&vptr)!=AMX_ERR_NONE)
    return 0;
  *value=*vptr;
  return 1;
}

static char *get_string(AMX *amx,AMX_DBG_SYMBOL *sym,int maxlength)
{
static char string[MAXLINELENGTH];
  char *ptr;
  cell *addr;
  cell base;
  int length,num;

  assert(sym->ident==iARRAY || sym->ident==iREFARRAY);
  assert(sym->dim==1);
  assert(maxlength<MAXLINELENGTH);
  string[0]='\0';

  /* get the staring address and the length of the string */
  base=sym->address;
  if (sym->vclass)
    base+=amx->frm;     /* addresses of local vars are relative to the frame */
  if (sym->ident==iREFARRAY) {   /* a reference */
    amx_GetAddr(amx,base,&addr);
    base=*addr;
  } /* if */
  #if !defined NO_REMOTE
    if (remote==REMOTE_RS232)
      remote_read_rs232(amx,base,MAXLINELENGTH);
  #endif
  if (amx_GetAddr(amx,base,&addr)==AMX_ERR_NONE) {
    amx_StrLen(addr,&length);

    /* allocate a temporary buffer */
    ptr=malloc(length+1);
    if (ptr!=NULL) {
      amx_GetString(ptr,addr,0,UNLIMITED);
      num=length;
      if (num>=maxlength) {
        num=maxlength-1;
        if (num>3)
          num-=3;         /* make space for the ... terminator */
      } /* if */
      assert(num>=0);
      strncpy(string,ptr,num);
      string[num]='\0';
      if (num<length && num==maxlength-3)
        strcat(string,"...");
      free(ptr);
    } /* if */
  } else {
    strcpy(string,"?");
  } /* if */
  return string;
}

static void printvalue(long value, int disptype)
{
  if (disptype==DISP_FLOAT) {
    amx_printf("%f", amx_ctof(value));
  } else if (disptype==DISP_FIXED) {
    #define MULTIPLIER 1000
    long ipart=value/MULTIPLIER;
    value-=MULTIPLIER*ipart;
    if (value<0)
      value=-value;
    amx_printf("%ld.%03ld", ipart, value);
  } else if (disptype==DISP_HEX) {
    amx_printf("%lx", value);
  } else if (disptype==DISP_BOOL) {
    switch (value) {
    case 0:
      amx_printf("false");
      break;
    case 1:
      amx_printf("true");
      break;
    default:
      amx_printf("%ld (true)",value);
      break;
    } /* switch */
  } else {
    amx_printf("%ld", value);
  } /* if */
}

static void display_variable(AMX *amx,AMX_DBG *amxdbg,AMX_DBG_SYMBOL *sym,int index,int idxlevel)
{
  const AMX_DBG_SYMDIM *symdim;
  cell value;

  /* set default display type for the symbol (if none was set) */
  if ((sym->vclass & ~DISP_MASK)==0) {
    const char *tagname;
    if (dbg_GetTagName(amxdbg,sym->tag,&tagname)==AMX_ERR_NONE) {
      if (stricmp(tagname,"bool")==0)
        sym->vclass |= DISP_BOOL;
      else if (stricmp(tagname,"fixed")==0)
        sym->vclass |= DISP_FIXED;
      else if (stricmp(tagname,"float")==0)
        sym->vclass |= DISP_FLOAT;
    } /* if */
  } /* if */

  if ((sym->ident==iARRAY || sym->ident==iREFARRAY) && idxlevel==0)
    dbg_GetArrayDim(amxdbg, sym, &symdim);

  if ((sym->ident==iARRAY || sym->ident==iREFARRAY) && idxlevel==0) {
    if ((sym->vclass & ~DISP_MASK)==DISP_STRING) {
      amx_printf("\"%s\"", get_string(amx,sym,40));
    } else if (sym->dim==1) {
      ucell len,i;
      len=symdim[0].size;
      if (len>5)
        len=5;
      else if (len==0)
        len=1;  /* unknown array length, assume at least 1 element */
      amx_printf("{");
      for (i=0; i<len; i++) {
        if (i>0)
          amx_printf(",");
        if (get_symbolvalue(amx,sym,(int)i,&value))
          printvalue(value,(sym->vclass & ~DISP_MASK));
        else
          amx_printf("?");
      } /* for */
      if (len<symdim[0].size || symdim[0].size==0)
        amx_printf(",...");
      amx_printf("}");
    } else {
      amx_printf("(multi-dimensional array)");
    } /* if */
  } else if ((sym->ident==iARRAY || sym->ident==iREFARRAY) && symdim[0].size>0
             && (ucell)index>=symdim[0].size)
  {
    amx_printf("(index out of range, size=%d)",symdim[0].size);
  } else if (sym->ident!=iARRAY && sym->ident!=iREFARRAY && idxlevel>0) {
    /* index used on a non-array */
    amx_printf("(invalid index, not an array)");
  } else {
    /* simple variable, or indexed array element */
    if (get_symbolvalue(amx,sym,0,&value))
      printvalue(value,(sym->vclass & ~DISP_MASK));
    else
      amx_printf("?");
  } /* if */
}

static void watch_init(void)
{
  namelist_init(&watches);
}

static void watch_list(AMX *amx,AMX_DBG *amxdbg)
{
  int num,idx;
  char *indexptr;
  char name[sNAMEMAX+20]; /* extra space for an array index */
  NAMELIST *watch;
  const AMX_DBG_SYMBOL *sym;

  if (terminal) {
    term_csrsave();
    amx_gotoxy(1,1);
    printf("\n");       /* clear "tab state" if the terminal driver forgot */
    amx_gotoxy(1,1);    /* and set the cursor position again */
  } /* if */

  num=0;
  for (watch=watches.next; watch!=NULL; watch=watch->next) {
    assert(watch->name!=NULL && strlen(watch->name)>0);
    strcpy(name,watch->name);
    indexptr=strchr(name,'[');
    if (indexptr!=NULL) {
      idx=atoi(indexptr+1);
      *indexptr='\0';
    } else {
      idx=0;
    } /* if */
    amx_printf("%d   %-12s ",num++,watch->name);
    /* find the symbol with the given range with the smallest scope */
    if (dbg_GetVariable(amxdbg,watch->name,amx->cip,&sym)==AMX_ERR_NONE)
      display_variable(amx,amxdbg,(AMX_DBG_SYMBOL *)sym,idx,(indexptr==NULL) ? 0 : 1);
    else
      amx_printf("(unknown symbol)");
    if (terminal)
      amx_clreol();
    amx_printf("\n");
  } /* for */

  if (terminal) {
    if (num==0)
      amx_printf("(no watches)");
    for ( ; num<watchlines; num++) {
      amx_clreol();
      amx_printf("\n");
    } /* for */
    term_csrrestore();
  } else {
    if (num>0)
      draw_hline(0);
  } /* if */
}

static int watch_set(int number, char *name)
{
  NAMELIST *cur;

  /* find a free number */
  if (number<0) {
    int changed;
    number=0;
    do {
      changed=0;
      for (cur=watches.next; cur!=NULL; cur=cur->next) {
        if (cur->number==number) {
          number++;
          changed=1;
        } /* if */
      } /* for */
    } while (changed);
  } /* if */

  /* add the watch */
  return namelist_add(&watches,name,number)!=NULL;
}

static int watch_clear(int number)
{
  NAMELIST *name=namelist_find(&watches,number);
  if (name!=NULL) {
    namelist_delete(&watches,name);
    return 1;
  } /* if */
  return 0;
}

static int watch_count(void)
{
  return namelist_count(&watches);
}

static void break_init(void)
{
  BREAKPOINT *next;

  while (breakpoints.next!=NULL) {
    next=breakpoints.next;
    breakpoints.next=next->next;  /* unlink */
    free(next);                   /* then free */
  } /* while */
}

static int break_clear(int number)
{
  BREAKPOINT *cur;

  /* find the breakpoint */
  cur=&breakpoints;
  while (cur->next!=NULL && cur->next->number!=number)
    cur=cur->next;
  if (cur->next==NULL)
    return 0;                   /* breakpoint not found */

  cur->next=cur->next->next;    /* unlink */
  free(cur->next);              /* then free */
  return 1;
}

static int break_set(AMX_DBG *amxdbg,const char *symaddr,int flags)
{
  BREAKPOINT *cur,*newbp, bp;
  int number,changed,err;

  /* find type */
  memset(&bp,0,sizeof(BREAKPOINT));
  symaddr=(const char*)skipwhitespace(symaddr);
  if (isdigit(*symaddr)) {
    bp.type=BP_CODE;
    if ((flags & 1)!=0)
      bp.number=0;      /* for temporary breakpoint, use number 0 */
    err=dbg_GetLineAddress(amxdbg,strtol(symaddr,NULL,10)-1,curfilename,&bp.addr);
  } else {
    bp.type=BP_CODE;
    err=dbg_GetFunctionAddress(amxdbg,symaddr,curfilename,&bp.addr);
    dbg_LookupFunction(amxdbg, bp.addr, &bp.name);
  } /* if */
  if (err!=AMX_ERR_NONE)
    return -1;

  /* walk through the list, see if the breakpoint already exists */
  for (cur=breakpoints.next; cur!=NULL; cur=cur->next) {
    if (cur->addr==bp.addr) {
      if (flags & 2)
        break_clear(cur->number); /* toggle the breakpoint */
      return AMX_ERR_NONE;
    } /* if */
  } /* for */

  /* find an unused breakpoint number */
  number=1;
  do {
    changed=0;
    for (cur=breakpoints.next; cur!=NULL; cur=cur->next) {
      if (cur->number==number) {
        number++;
        changed=1;
      } /* if */
    } /* for */
  } while (changed);

  /* allocate a new breakpoint, add the entry parsed earlier to the list */
  newbp=malloc(sizeof(BREAKPOINT));
  if (newbp==0)
    return -1;
  memcpy(newbp,&bp,sizeof(BREAKPOINT));
  for (cur=&breakpoints; cur->next!=NULL && cur->next->number<number; cur=cur->next)
    /* nothing */;
  assert(cur!=NULL);
  newbp->next=cur->next;
  cur->next=newbp;
  newbp->number=number;

  return number;
}

static void break_list(AMX_DBG *amxdbg)
{
  BREAKPOINT *cur;

  for (cur=breakpoints.next; cur!=NULL; cur=cur->next) {
    assert(cur->type!=BP_NONE);
    scroll_cmdwin(1);
    amx_printf("%2d  ",cur->number);
    if (cur->type==BP_CODE) {
      long line;
      const char *filename;
      dbg_LookupLine(amxdbg,cur->addr,&line);
      amx_printf("line: %ld",line);
      if (cur->name!=NULL) {
        amx_printf("\tfunc: %s",cur->name);
      } else {
        dbg_LookupFile(amxdbg,cur->addr,&filename);
        if (filename!=NULL)
          amx_printf("\tfile: %s",skippath(filename));
      } /* if */
    } /* if */
    amx_printf("\n");
  } /* for */
}

static int break_check(AMX *amx)
{
  BREAKPOINT *cur;
  ucell cip;

  /* when the "break" statement comes, the instruction pointer is already
   * incremented; we must adjust for this
   */
  cip=(ucell)amx->cip-sizeof(cell);
  for (cur=breakpoints.next; cur!=NULL; cur=cur->next) {
    if (cur->type==BP_CODE) {
      if (cur->addr==cip) {
        int number=cur->number;
        if (number==0)
          break_clear(number);
        return number;
      } /* if */
    } /* if */
  } /* for */
  return -1;
}

#if defined READLINE
/* Read a string, and return a pointer to it. Returns NULL on EOF. */
char *rl_gets(char *str,int length)
{
  char *line_read=readline(STR_PROMPT);
  if (line_read==NULL)
    return NULL;

  /* copy the line (with possible truncation) */
  strncpy(str,line_read,length);
  if ((int)strlen(line_read)>length)
    str[length-1]='\0';
  #if !defined __WIN32__
    /* readline() allocates memory internally, which it asks you to deallocate
     * with free(). This is an acceptable design with a statically linked
     * library, and perhaps even with shared libraries in Unix/Linux, but it
     * is problematic with DLLs under Microsoft Windows. In Microsoft Windows,
     * the standard C library was never part of the OS, so each compiler
     * vendor provided its own. If your DLL allocates memory and your program
     * (that uses the DLL) frees it, chances are that the malloc() and free()
     * implementations assume a different heap lay-out (because they originate
     * from different vendors, or from different versions of the compiler, or
     * even because of different compilation options/flags).
     * Now if malloc() and free() disagree, trouble is never far.
     * To illustruate the problem: I downloaded a readline Win32 port (a DLL),
     * but calling free() on the returned pointer consistently crashes the
     * program. I must have a different compiler than the one used to compile
     * the DLL, or a different version of the compiler, or different compiler
     * options, or a different MSVCRT.DLL.
     * As a temporary solution, I have chosen to "leak" memory (instead of
     * crashing).
     */
    free(line_read);
  #endif

  /* If the line has any text in it, save it on the history. */
  if (strlen(str)>0)
    add_history(line_read);

  return str;
}
#else
char *rl_gets(char *str,int length)
{
  #if defined __WIN32__
    unsigned long num;
    INPUT_RECORD rec;
    CONSOLE_CURSOR_INFO csrinfo;
    HANDLE stdinput=GetStdHandle(STD_INPUT_HANDLE);
    HANDLE stdoutput=GetStdHandle(STD_OUTPUT_HANDLE);
  #endif

  amx_printf("%s",STR_PROMPT);
  amx_fflush();
  #if defined __WIN32__
    if (terminal) {
      /* first wait for a function key */
      GetConsoleCursorInfo(stdoutput,&csrinfo);
      csrinfo.bVisible=FALSE;
      SetConsoleCursorInfo(stdoutput,&csrinfo);
      term_statusbar(1);
      assert(length>5);
      for ( ;; ) {
        while (PeekConsoleInput(stdinput,&rec,1,&num) && num==0)
          /* nothing */;
        if (num==0)
          break;
        /* analyze the event, keep only the keyboard events */
        switch (rec.EventType) {
        case KEY_EVENT:
          if (!rec.Event.KeyEvent.bKeyDown) {
            ReadConsoleInput(stdinput,&rec,1,&num);
            continue;
          } /* if */
          str[0]='\0';
          switch (rec.Event.KeyEvent.wVirtualKeyCode) {
          case VK_F1:
            strcpy(str,"\033[11~"); /* \e[11~ on VT220, \eOP on VT100 */
            break;
          case VK_F2:
            strcpy(str,"\033[12~"); /* \e[12~ on VT220, \eOQ on VT100 */
            break;
          case VK_F3:
            strcpy(str,"\033[13~"); /* \e[13~ on VT220, \eOR on VT100 */
            break;
          case VK_F4:
            strcpy(str,"\033[14~"); /* \e[14~ on VT220, \eOS on VT100 */
            break;
          case VK_F5:
            strcpy(str,"\033[15~");
            break;
          case VK_F6:
            strcpy(str,"\033[17~");
            break;
          case VK_F7:
            strcpy(str,"\033[18~");
            break;
          case VK_F8:
            strcpy(str,"\033[19~");
            break;
          case VK_F9:
            strcpy(str,"\033[20~");
            break;
          case VK_F10:
            strcpy(str,"\033[21~");
            break;
          case VK_F11:
            strcpy(str,"\033[23~");
            break;
          case VK_F12:
            strcpy(str,"\033[24~");
            break;
          case VK_PRIOR:  /* PgUp */
            strcpy(str,"\033[5~");
            break;
          case VK_NEXT:  /* PgDn */
            strcpy(str,"\033[6~");
            break;
#if 0//???
          case VK_UP:   /* Arrow up */
            strcpy(str,"\033[A");
            break;
          case VK_DOWN: /* Arrow down */
            strcpy(str,"\033[B");
            break;
          case VK_LEFT: /* Arrow left */
            strcpy(str,"\033[C");
            break;
          case VK_RIGHT:/* Arrow right */
            strcpy(str,"\033[D");
            break;
#endif
          } /* switch */
          if (str[0]!='\0') {
            ReadConsoleInput(stdinput,&rec,1,&num); /* remove from queue */
            csrinfo.bVisible=TRUE;
            SetConsoleCursorInfo(stdoutput,&csrinfo);
            term_statusbar(0);
            return str;
          } /* if */
          break;
        case MOUSE_EVENT:
          ReadConsoleInput(stdinput,&rec,1,&num);
          if (rec.Event.MouseEvent.dwEventFlags==0) {
            /* mouse click or release */
            int y=rec.Event.MouseEvent.dwMousePosition.Y;
            if (terminal) {
              int line=y-watchlines-1;
              if (line>=0 && line<listlines)
                recentline=line+curtopline;
            } /* if */
          } /* if */
          continue;
        default:
          ReadConsoleInput(stdinput,&rec,1,&num);
          continue;     /* ignore event, check for a next event */
        } /* switch */
        /* there are explicit "continues", default action is to NOT loop */
        break;
      } /* for */
      csrinfo.bVisible=TRUE;
      SetConsoleCursorInfo(stdoutput,&csrinfo);
      term_statusbar(0);
    } /* if */
    ReadConsole(stdinput,str,length,&num,NULL);
  #else
    amx_gets(str,length);
  #endif
  return str;
}
#endif

static void listcommands(char *command,AMX_DBG *amxdbg,int curline)
{
  if (command==NULL)
    command="";
  if (terminal) {
    term_csrsave();
    amx_clrscr();
    if (strlen(command)==0 || strcmp(command,"?")==0)
      amx_printf("At the prompt, you can type debug commands. For example, the word \"step\" is a\n"
                 "command to execute a single line in the source code. The commands that you will\n"
                 "use most frequently may be abbreviated to a single letter: instead of the full\n"
                 "word \"step\", you can also type the letter \"s\" followed by the enter key.\n\n"
                 "Available commands:\n");
    else
      amx_printf("Options for command \"%s\":\n",command);
  } /* if */

  if (stricmp(command,"break")==0) {
    amx_printf("\tBREAK\t\tlist all breakpoints\n"
            "\tBREAK n\t\tset a breakpoint at line \"n\"\n"
            "\tBREAK func\tset a breakpoint at function with name \"func\"\n");
  } else if (stricmp(command,"cbreak")==0) {
    amx_printf("\tCBREAK n\tremove breakpoint number \"n\"\n"
            "\tCBREAK *\tremove all breakpoints\n");
  } else if (stricmp(command,"cw")==0 || stricmp(command,"cwatch")==0) {
    amx_printf("\tCWATCH may be abbreviated to CW\n\n"
            "\tCWATCH n\tremove watch number \"n\"\n"
            "\tCWATCH *\tremove all watches\n");
  } else if (stricmp(command,"d")==0 || stricmp(command,"disp")==0) {
    amx_printf("\tDISP may be abbreviated to D\n\n"
            "\tDISP\t\tdisplay all variables that are currently in scope\n"
            "\tDISP var\tdisplay the value of variable \"var\"\n"
            "\tDISP var[i]\tdisplay the value of array element \"var[i]\"\n");
  } else if (stricmp(command,"file")==0) {
    amx_printf("\tFILE\t\tlist all files that this program is composed off\n"
            "\tFILE name\tset the current file to \"name\"\n");
  } else if (stricmp(command,"g")==0 || stricmp(command,"go")==0) {
    amx_printf("\tGO may be abbreviated to G\n\n"
            "\tGO\t\trun until the next breakpoint or program termination\n"
            "\tGO n\t\trun until line number \"n\"\n");
  } else if (stricmp(command,"l")==0 || stricmp(command,"list")==0) {
    amx_printf("\tLIST may be abbreviated to L\n\n"
            "\tLIST\t\tdisplay source code lines around the current line\n"
            "\tLIST n\t\tdisplay lines, starting from line \"n\"\n"
            "\tLIST n m\tdisplay \"m\" lines, starting from line \"n\" (not\n"
            "\t\t\tsupported in terminal emulation modes)\n"
            "\tLIST UP\tdisplay preceding lines\n"
            "\tLIST DOWN\tdisplay following lines\n"
            "\tLIST FUNCS\tdisplay all functions\n"
            "\tLIST ON\t\tautomatically list source code lines after each step\n"
            "\tLIST OFF\tturn off automatic list (not supported in terminal\n"
            "\t\t\temulation modes)\n"
            "\tLIST STATES\tdisplay all automatons and the state that these are in\n");
  } else if (stricmp(command,"n")==0 || stricmp(command,"next")==0
             || stricmp(command,"quit")==0
             || stricmp(command,"s")==0 || stricmp(command,"step")==0)
  {
    amx_printf("\tno additional information\n");
  } else if (stricmp(command,"term")==0) {
    amx_printf("\tTERM OFF\tdisable VT100/Win32 terminal functions\n"
            "\tTERM ON\t\tuse terminal capabilities, default console size\n"
            "\tTERM x y\tset terminal size (x columns, y lines)\n");
  } else if (stricmp(command,"type")==0) {
    amx_printf("\tTYPE var STRING\tdisplay \"var\" as string\n"
            "\tTYPE var STD\tset default display format (decimal integer)\n"
            "\tTYPE var HEX\tset hexadecimal integer format\n"
            "\tTYPE var FIXED\tset fixed point format (3 decimals)\n"
            "\tTYPE var FLOAT\tset floating point format\n");
  } else if (stricmp(command,"watch")==0 || stricmp(command,"w")==0) {
    amx_printf("\tWATCH may be abbreviated to W\n\n"
            "\tWATCH var\tset a new watch at variable \"var\"\n"
            "\tWATCH n var\tchange watch \"n\" to variable \"var\"\n");
  } else {
    amx_printf("\tBREAK\t\tset breakpoint at line number or variable name\n"
            "\tCBREAK\t\tremove breakpoint\n"
            "\tCW(atch)\tremove a \"watchpoint\"\n"
            "\tD(isp)\t\tdisplay the value of a variable, list variables\n"
            "\tFILE\t\tswitch to a file\n"
            "\tG(o)\t\trun program (until breakpoint)\n"
            "\tL(ist)\t\tdisplay source file, automatons and functions\n"
            "\tN(ext)\t\tRun until next line, step over functions\n"
            "\tOUTPUT\t\tShow program output (if supported by terminal)\n"
            "\tQUIT\t\texit debugger, terminate program\n"
            "\tS(tep)\t\tsingle step, step into functions\n"
            "\tTERM\t\tset terminal type\n"
            "\tTYPE\t\tset the \"display type\" of a symbol\n"
            "\tW(atch)\t\tset a \"watchpoint\" on a variable\n"
            "\n\tUse \"? <command name>\" to view more information on a command\n");
  } /* if */
  amx_fflush();
  if (terminal) {
    amx_printf("\nPress a key to continue...");
    amx_fflush();
    amx_getch();
    term_refresh(0);
    term_csrrestore();
    source_list(amxdbg,gettopline(curline,curline-autolist/2),autolist,curline);
  } /* if */
}

static int docommand(AMX *amx,AMX_DBG *amxdbg,int curline)
{
static char lastcommand[10] = "";
  char line[MAXLINELENGTH], command[32];
  int result,i;
  char *params;

  for ( ;; ) {
    if (terminal) {
      term_csrsave();
      amx_gotoxy(1,screenlines);
      amx_clreol();
    } /* if */
    rl_gets(line,sizeof(line));
    if (terminal)
      term_csrrestore();
    if (line[0]=='\033') {
      assert(terminal);
      if (strcmp(line+1,"[11~")==0 || strcmp(line+1,"OP")==0) {
        strcpy(line,"?");                       /* F1 == Help */
      } else if (strcmp(line+1,"[13~")==0 || strcmp(line+1,"OR")==0) {
        strcpy(line,"QUIT");                    /* F3 == List */
      } else if (strcmp(line+1,"[14~")==0 || strcmp(line+1,"OS")==0) {
        strcpy(line,"Output");                  /* F4 == Output */
      } else if (strcmp(line+1,"[15~")==0) {
        strcpy(line,"G");                       /* F5 == Go */
      } else if (strcmp(line+1,"[17~")==0) {
        strcpy(line,"D");                       /* F6 == Disp */
      } else if (strcmp(line+1,"[19~")==0) {
        strcpy(line,"S");                       /* F8 == Step */
      #if 0 //???
      } else if (strcmp(line+1,"[20~")==0) {
        if (recentline<0)
          recentline=curline;
        sprintf(line,"BREAK %d",recentline+1);  /* F9 == Breakpoint */
        //??? should toggle if a breakpoint is already set
      #endif
      } else if (strcmp(line+1,"[21~")==0) {
        strcpy(line,"N");                       /* F10 == Next */
      } else if (strcmp(line+1,"[5~")==0) {
        strcpy(line,"L UP");                    /* PgUp */
      } else if (strcmp(line+1,"[6~")==0) {
        strcpy(line,"L DOWN");                  /* PgDn */
      } else {
        continue;
      } /* if */
    } /* if */
    if ((params=strchr(line,'\n'))!=NULL)
      *params='\0';     /* strip newline character */
    if ((params=strchr(line,'\r'))!=NULL)
      *params='\0';     /* strip carriage return character too */
    if (strlen(line)==0) {
      if (!terminal) {
        int csrx,csry;
        amx_wherexy(&csrx,&csry);
        amx_gotoxy(1,csry-1);
        amx_clreol();
      } /* if */
      strcpy(line,lastcommand);
    } /* if */
    lastcommand[0]='\0';

    result=sscanf(line,"%8s",command);
    if (result<=0) {
      listcommands(NULL,amxdbg,curline);
      continue;
    } /* if */
    params=strchr(line,' ');
    params=(params!=NULL) ? skipwhitespace(params) : "";

    if (stricmp(command,"?")==0) {
      result=sscanf(line,"%*s %30s",command);
      listcommands(result ? command : NULL,amxdbg,curline);
    } else if (stricmp(command,"quit")==0) {
      if (terminal) {
        amx_clrscr();
        term_close();
      } /* if */
      #if !defined NO_REMOTE
        if (remote==REMOTE_RS232)
          remote_rs232(0,0);
      #endif
      exit(0);
    } else if (stricmp(command,"g")==0 || stricmp(command,"go")==0) {
      if (isdigit(*params))
        break_set(amxdbg,params,0x1);
      recentline=-1;
      return RUNNING;
    } else if (stricmp(command,"s")==0 || stricmp(command,"step")==0) {
      strcpy(lastcommand,"s");
      recentline=-1;
      return STEPPING;
    } else if (stricmp(command,"n")==0 || stricmp(command,"next")==0) {
      strcpy(lastcommand,"n");
      recentline=-1;
      return STEPOVER;
    } else if (stricmp(command,"l")==0 || stricmp(command,"list")==0) {
      /* first check a few hard cases */
      if (stricmp(params,"funcs")==0) {
        for (i=0; i<amxdbg->hdr->symbols; i++) {
          if (amxdbg->symboltbl[i]->ident == iFUNCTN) {
            const char *filename;
            scroll_cmdwin(1);
            amx_printf("%s",amxdbg->symboltbl[i]->name);
            if (dbg_LookupFile(amxdbg,amxdbg->symboltbl[i]->address,&filename)==AMX_ERR_NONE)
              amx_printf("\t(%s)",skippath(filename));
            //??? also say what automaton this function is part of
            amx_printf("\n");
          } /* if */
        } /* for */
      } else if (stricmp(params,"states")==0) {
        const char *statename;
        cell *cptr;
        if (amxdbg->hdr->automatons==0) {
          scroll_cmdwin(1);
          amx_printf("\t(no states)\n");
        } /* if */
        for (i=0; i<amxdbg->hdr->automatons; i++) {
          scroll_cmdwin(1);
          amx_printf("%s",strlen(amxdbg->automatontbl[i]->name)==0 ? "(anonymous)" : amxdbg->automatontbl[i]->name);
          /* read the variable at the address */
          #if !defined NO_REMOTE
            if (remote==REMOTE_RS232)
              remote_read_rs232(amx,amxdbg->automatontbl[i]->address,1);
          #endif
          if (amx_GetAddr(amx,amxdbg->automatontbl[i]->address,&cptr)==AMX_ERR_NONE)
            dbg_GetStateName(amxdbg,(int)*cptr,&statename);
          amx_printf("\t-> %s\n",(statename==NULL) ? "(none)" : statename);
        } /* for */
      } else if (stricmp(params,"on")==0) {
        autolist=listlines;
        watch_list(amx,amxdbg);
        source_list(amxdbg,curline-autolist/2,autolist,curline);
      } else if (stricmp(params,"off")==0) {
        if (!terminal)
          autolist=1;
        else
          amx_printf("\tCommand not supported on terminals\n");
      } else {
        int lnum,numlines;
        lnum=curline-(listlines/2-1);    /* preset */
        numlines=listlines;
        if (stricmp(params,"up")==0) {
          lnum=curtopline-listlines;
          if (lnum<0)
            lnum=0;
        } else if (stricmp(params,"down")==0) {
          lnum=curtopline+listlines;
          if (lnum>source_lines()-3)
            lnum=source_lines()-3;
        } else {
          sscanf(line,"%*s %d %d",&lnum,&numlines);
          lnum--;         /* if user filled in a line number, subtract 1 */
        } /* if */
        #if !defined VT100
          if (terminal) {
            int csrx,csry;
            amx_wherexy(&csrx,&csry);
            if (csry>=screenlines)
              term_refresh(0);
          } /* if */
        #endif
        source_list(amxdbg,lnum,numlines,curline);
      } /* if */
    } else if (stricmp(command,"break")==0) {
      if (*params=='\0') {
        break_list(amxdbg);
      } else {
        result=break_set(amxdbg,params,0x0);
        if (result<0)
          amx_printf("Invalid breakpoint\n");
        else if (terminal)
          source_list(amxdbg,gettopline(curline,curline-autolist/2),autolist,curline);
      } /* if */
    } else if (stricmp(command,"cbreak")==0) {
      if (*params=='*') {
        /* clear all breakpoints */
        break_init();
      } else if (isdigit(*params)) {
        if (!break_clear(atoi(params)))
          amx_printf("\tUnknown breakpoint\n");
      } else {
        amx_printf("\tInvalid command\n");
      } /* if */
      if (terminal)
        source_list(amxdbg,gettopline(curline,curline-autolist/2),autolist,curline);
    } else if (stricmp(command,"disp")==0 || stricmp(command,"d")==0) {
      if (*params=='\0') {
        /* display all variables that are in scope */
        for (i=0; i<amxdbg->hdr->symbols; i++) {
          if (amxdbg->symboltbl[i]->ident!=iFUNCTN
              && amxdbg->symboltbl[i]->codestart<=(ucell)amx->cip
              && amxdbg->symboltbl[i]->codeend>(ucell)amx->cip)
          {
            scroll_cmdwin(1);
            amx_printf("%s\t%s\t",(amxdbg->symboltbl[i]->vclass & DISP_MASK)>0 ? "loc" : "glb",amxdbg->symboltbl[i]->name);
            display_variable(amx,amxdbg,amxdbg->symboltbl[i],0,0);
            amx_printf("\n");
          } /* if */
        } /* for */
      } else {
        const AMX_DBG_SYMBOL *sym;
        char *indexptr=strchr(params,'[');
        if (indexptr!=NULL) {
          i=atoi(indexptr+1);
          *indexptr='\0';
        } else {
          i=0;
        } /* if */
        /* find the symbol with the smallest scope */
        if (dbg_GetVariable(amxdbg,params,amx->cip,&sym)==AMX_ERR_NONE) {
          scroll_cmdwin(1);
          amx_printf("%s\t%s\t",(sym->vclass & DISP_MASK)>0 ? "loc" : "glb",sym->name);
          display_variable(amx,amxdbg,(AMX_DBG_SYMBOL*)sym,i,(indexptr==NULL) ? 0 : 1);
          amx_printf("\n");
        } else {
          amx_printf("\tSymbol not found, or not a variable\n");
        } /* if */
      } /* if */
    } else if (stricmp(command,"file")==0) {
      if (*params=='\0') {
        /* browse through the file table */
        for (i=0; i<amxdbg->hdr->files; i++) {
          scroll_cmdwin(1);
          amx_printf("%s\n",amxdbg->filetbl[i]->name);
        } /* for */
      } else {
        /* find the file */
        for (i=0; i<amxdbg->hdr->files; i++)
          if (stricmp(params,amxdbg->filetbl[i]->name)==0 || stricmp(params,skippath(amxdbg->filetbl[i]->name))==0)
            break;
        if (i<amxdbg->hdr->files) {
          if (curfilename!=amxdbg->filetbl[i]->name) {
            curfilename=amxdbg->filetbl[i]->name;
            curline=0;
            if (cursource!=NULL)
              source_free(cursource);
            assert(amxdbg->filetbl[i]->name!=NULL);
            cursource=source_load(amxdbg->filetbl[i]->name);
            if (cursource==NULL) {
              amx_printf("\tSource file not found (or insufficient memory)\n");
              continue;
            } /* if */
          } /* if */
        } else {
          amx_printf("\tunknown file\n");
        } /* if */
      } /* if */
    } else if (stricmp(command,"term")==0) {
      int new_term=-1;
      int columns=screencolumns;
      int lines=screenlines+1;
      if (stricmp(params,"off")==0)
        new_term=0;
      else if (stricmp(params,"on")==0)
        new_term=1;
      else if (sscanf(params,"%d %d",&columns,&lines)==2)
        new_term=2;
      if (new_term==-1) {
        amx_printf("\tTerminal support is %s\n",terminal ? "ON" : "OFF");
      } else if (terminal!=new_term) {
        curtopline=0;
        if (terminal)
          term_close();
        if (new_term && amx_termctl(0,0)) {
          term_open(columns,lines);
          autolist=listlines;
        } else if (new_term) {
          amx_printf("\tTerminal not supported\n");
        } /* if */
        watch_list(amx,amxdbg);
        source_list(amxdbg,gettopline(curline,curline-autolist/2),autolist,curline);
      } /* if */
    } else if (stricmp(command,"output")==0) {
      if (term_switch(0))       /* switch back to user screen */
        amx_getch();            /* if it worked, wait for a key press */
      term_switch(1);           /* switch to debugger screen */
    } else if (stricmp(command,"type")==0) {
      char symname[sNAMEMAX+1],*ptr;
      int len;
      for (ptr=params; *ptr!='\0' && *ptr!=' ' && *ptr!='\t'; ptr++)
        /* nothing */;
      len=(int)(ptr-params);
      if (len==0 || len>sNAMEMAX) {
        amx_printf("\tInvalid (or missing) symbol name\n");
      } else {
        AMX_DBG_SYMBOL *sym;
        strncpy(symname,params,len);
        symname[len]='\0';
        params=skipwhitespace(ptr);
        /* look up the symbol */
        if (dbg_GetVariable(amxdbg,symname,amx->cip,(const AMX_DBG_SYMBOL**)&sym)==AMX_ERR_NONE) {
          assert(sym!=NULL);
          if (stricmp(params,"std")==0) {
            sym->vclass = (char)((sym->vclass & DISP_MASK) | DISP_DEFAULT);
          } else if (stricmp(params,"string")==0) {
            /* check array with single dimension */
            if (!(sym->ident==iARRAY || sym->ident==iREFARRAY) || sym->dim!=1)
              amx_printf("\t\"string\" display type is only valid for arrays with one dimension\n");
            else
              sym->vclass = (char)((sym->vclass & DISP_MASK) | DISP_STRING);
          } else if (stricmp(params,"bin")==0) {
            sym->vclass = (char)((sym->vclass & DISP_MASK) | DISP_BIN);
          } else if (stricmp(params,"hex")==0) {
            sym->vclass = (char)((sym->vclass & DISP_MASK) | DISP_HEX);
          } else if (stricmp(params,"fixed")==0) {
            sym->vclass = (char)((sym->vclass & DISP_MASK) | DISP_FIXED);
          } else if (stricmp(params,"float")==0) {
            sym->vclass = (char)((sym->vclass & DISP_MASK) | DISP_FLOAT);
          } else {
            amx_printf("\tUnknown (or missing) display type\n");
          } /* if */
          watch_list(amx,amxdbg);
        } else {
          amx_printf("\tUnknown symbol \"%s\"\n",symname);
        } /* if */
      } /* if */
    } else if (stricmp(command,"w")==0 || stricmp(command,"watch")==0) {
      /* see whether this is a new watch, or whether one is replaced */
      if (isdigit(*params)) {
        i=atoi(params);
        params=skipvalue(params);
      } else {
        i = -1;
      } /* if */
      if (strlen(params)==0) {
        amx_printf("Missing variable name\n");
        continue;
      } /* if */
      /* is there space for another watch? (only when using terminal support) */
      if (i<0 && terminal && watch_count()>=WATCHLINES) {
        int w=watch_count()+1;  /* new number of watches */
        int l=listlines;
        int c=screenlines-w-l-3;
        if (c<=1) {
          /* command window cannot shrink, shrink code listing instead */
          c=2;
          l=screenlines-w-c-3;
          if (l<5) {
            /* code listing cannot shrink either, give a message */
            amx_printf("Too many watches, sorry\n");
            continue;
          } /* if */
        } /* if */
        watchlines=w;
        listlines=l;
        term_refresh(0); /* reset */
        source_list(amxdbg,gettopline(curline,curline-autolist/2),autolist,curline);
      } /* if */
      result=watch_set(i,params);
      if (result>=0)
        watch_list(amx,amxdbg);
      else
        amx_printf("Invalid watch\n");
    } else if (stricmp(command,"cw")==0 || stricmp(command,"cwatch")==0) {
      if (*params=='*') {
        /* clear all watches */
        watch_init();
      } else if (isdigit(*params)) {
        watch_clear(atoi(params));
      } else {
        amx_printf("\tInvalid command syntax, use \"? cw\" for details\n");
      } /* if */
      if (terminal && watchlines>WATCHLINES) {
        int w,l,c;
        w=watch_count();        /* new number of watches */
        if (w<WATCHLINES)
          w=WATCHLINES;
        l=LISTLINES;            /* desired number of lines in code listing */
        c=screenlines-w-l-3;
        if (c<=1) {
          /* command window too small, shrink code listing */
          c=2;
          l=screenlines-w-c-3;
        } /* if */
        watchlines=w;
        listlines=l;
        term_refresh(0);        /* reset */
        source_list(amxdbg,gettopline(curline,curline-autolist/2),autolist,curline);
      } /* if */
      watch_list(amx,amxdbg);
    } else {
      amx_printf("\tInvalid command, use \"?\" to view all commands\n");
    } /* if */
  } /* for */
}

void sigabort(int sig)
{
  /* set "trace mode" */
  runmode=STEPPING;
  signal(sig,sigabort); /* re-install the signal handler */
}

int AMXAPI amx_InternalDebugProc(AMX *amx)
{
static cell lastfrm;
static long lastline;
  AMX_DBG *amxdbg;
  const char *filename;
  long line;
  int breaknr,i,err;
  char *msg;

  if (amx == NULL) {
    /* special case: re-initialize the abstract machine */
    if (cursource!=NULL) {
      source_free(cursource);
      cursource=NULL;
    } /* if */
    break_init();               /* frees all existing breakpoints */
    watch_init();
    if (amx_termctl(0,0)) {
      curtopline=0;
      term_switch(1);           /* switch to the debugger console */
      term_open(screencolumns,screenlines+1);
      autolist=listlines;
      term_switch(0);           /* switch back to the program output */
    } /* if */
    lastfrm=0;
    lastline=-1;
    runmode=STEPPING;           /* wait at the entry point */
    return AMX_ERR_NONE;
  } /* if */

  breaknr=-1;
  if (runmode==RUNNING) {
    /* check breakpoint address */
    breaknr=break_check(amx);
    if (breaknr<0) {
      /* just print the address at the bottom line, so one can trace where the
       * the program is
       */
      if (terminal) {
        term_csrsave();
        amx_gotoxy(1,screenlines);
        amx_printf("cip: %08lx",(long)amx->cip);
        term_csrrestore();
      } /* if */
      return AMX_ERR_NONE;
    } /* if */
    runmode=STEPPING;
  } /* for */

  /* get more information from the remote target */
  #if !defined NO_REMOTE
    if (remote!=REMOTE_NONE)
      remote_sync_rs232(amx);
  #endif

  /* get the debug information header */
  err = amx_GetUserData(amx, AMX_USERTAG('P','D','b','g'), (void**)&amxdbg);
  if (err!=AMX_ERR_NONE)
    return AMX_ERR_DEBUG;

  /* try to avoid halting on the same line twice */
  dbg_LookupLine(amxdbg,amx->cip,&line);
  if (line==lastline)
    return AMX_ERR_NONE;
  lastline=line;

  /* check whether we are stepping through a sub-function */
  if (runmode==STEPOVER) {
    assert(lastfrm!=0);
    if (amx->frm<lastfrm)
      return AMX_ERR_NONE;
  } /* if */

  /* find the file at which we are (for loading the appropriate source file) */
  dbg_LookupFile(amxdbg,amx->cip,&filename);

  /* check breakpoints */
  msg=NULL;
  if (breaknr==0) {
    msg="STOPPED at line %d\n";
    i=(int)line+1;
  } else if (breaknr>0) {
    msg="STOPPED at breakpoint %d\n";
    i=breaknr;
  } /* if */
  term_switch(1);             /* switch to the debugger console */
  if (msg!=NULL)
    amx_printf(msg,i);        /* print breakpoint number */
  assert(filename!=NULL);
  if (curfilename!=filename) {
    curfilename=filename;
    if (cursource!=NULL)
      source_free(cursource);
    cursource=source_load(filename);
    if (cursource==NULL) {
      amx_printf("\nCritical error: source file not found (or insufficient memory)\n");
      exit(1);
    } /* if */
  } /* if */
  #if !defined VT100
    if (terminal) {
      int csrx,csry;
      amx_wherexy(&csrx,&csry);
      if (csry>=screenlines)
        term_refresh(0);
    } /* if */
  #endif

  assert(cursource[(int)line]!=NULL);
  watch_list(amx,amxdbg);
  source_list(amxdbg,gettopline((int)line,(int)line-autolist/2),autolist,(int)line);
  runmode=docommand(amx,amxdbg,(int)line);
  term_switch(0);     /* switch back to the program output */
  if (runmode==STEPOVER)
    lastfrm=amx->frm; /* step OVER functions (so save the stack frame) */

  return AMX_ERR_NONE;
}

#if !defined amx_Init

static void *loadprogram(AMX *amx,const char *filename)
{
  FILE *fp;
  AMX_HEADER hdr;
  void *program;

  if ((fp = fopen(filename,"rb")) != NULL) {
    fread(&hdr, sizeof hdr, 1, fp);
    amx_Align32((uint32_t *)&hdr.stp);
	amx_Align32((uint32_t *)&hdr.size);
    if ((program = malloc((int)hdr.stp)) != NULL) {
      rewind(fp);
      fread(program, 1, (int)hdr.size, fp);
      fclose(fp);
      memset(amx, 0, sizeof *amx);
      if (amx_Init(amx,program) == AMX_ERR_NONE)
        return program;
      free(program);
    } /* if */
  } /* if */
  return NULL;
}

static int loaddbginfo(AMX *amx,AMX_DBG *amxdbg,const char *filename)
{
  FILE *fp;
  int err;

  /* load the debug information from the file */
  if ((fp = fopen(filename,"rb")) == NULL)
    return AMX_ERR_DEBUG;
  err = dbg_LoadInfo(amxdbg,fp);
  fclose(fp);
  if (err != AMX_ERR_NONE)
    return err;

  /* attach the debug information structure to the abstract machine */
  err = amx_SetUserData(amx, AMX_USERTAG('P','D','b','g'), amxdbg);
  return err;
}

int main(int argc,char *argv[])
{
extern AMX_NATIVE_INFO core_Natives[];
extern AMX_NATIVE_INFO console_Natives[];

  AMX amx;
  AMX_DBG amxdbg;
  cell ret;
  int err,i;
  void *program;
  unsigned short flags;
  char filename[_MAX_PATH],*ptr;

  amx_printf("Pawn command line debugger\n\n");
  /* get filename */
  if (argc >= 2) {
    strcpy(filename,argv[1]);
  } else {
    amx_printf("File: ");
    amx_gets(filename,sizeof filename);
    if ((ptr=strchr(filename,'\n'))!=NULL)
      *ptr='\0';                /* strip newline characters */
  } /* if */
  if (strlen(filename)==0)
    return 1;
  if ((program = loadprogram(&amx,filename)) == NULL) {
    /* try adding an extension */
    strcat(filename, ".amx");
    if ((program = loadprogram(&amx,filename)) == NULL) {
      amx_printf("Cannot load the program file \"%s\"\n", filename);
      return 1;
    } /* if */
  } /* if */
  amx_Flags(&amx,&flags);
  if ((flags & AMX_FLAG_DEBUG)==0) {
    amx_printf("This program has no debug information\n");
    return 1;
  } /* if */
  /* switch the current directory to that of the debugged script */
  if ((ptr=strrchr(filename,DIRSEP_CHAR))!=NULL) {
    extern int chdir(const char *path);
    char dir[_MAX_PATH];
    int len=(int)(ptr-filename);
    if (len<sizeof dir) {
      strncpy(dir,filename,len);
      dir[len]='\0';
      chdir(dir);
    } /* if */
  } /* if */

  for (i = 2; i < argc; i++) {
    if (strncmp(argv[i],"-rs232",6)==0) {
      #if !defined NO_REMOTE
        int port;
        ptr=strchr(argv[i],'=');
        port=(ptr!=NULL) ? atoi(ptr+1) : 0;
        amx_printf("Waiting for connection...");
        amx_fflush();
        remote_rs232(port,57600);
        printf("\n");
      #else
        printf("Remote debugging is not supported\n");
        return 1;
      #endif
    } else if (strncmp(argv[i],"-term",5)==0) {
      if ((ptr=strchr(argv[i],'='))!=NULL) {
        sscanf(ptr+1,"%d,%d",&screencolumns,&screenlines);
        screenlines--;
      } /* if */
    } /* if */
  } /* for */

  if (loaddbginfo(&amx,&amxdbg,filename)!=AMX_ERR_NONE) {
    amx_printf("Error loading debug information\n");
    return 1;
  } /* if */
  amx_SetDebugHook(&amx, amx_InternalDebugProc);
  signal(SIGINT,sigabort);
  amx_InternalDebugProc(NULL);  /* initialize debug hook */

  if (remote==REMOTE_NONE) {
    /* libraries do not need to be present for remote debugging */
    amx_Register(&amx, core_Natives, -1);
    err = amx_Register(&amx, console_Natives, -1);
    if (err != AMX_ERR_NONE)
      amx_printf("Load error %d\n", err);
  } else {
    err = AMX_ERR_NONE;
  } /* if */

  /* run the script, except... when doing remote debugging */
  if (err == AMX_ERR_NONE) {
    if (remote==REMOTE_NONE) {
      err = amx_Exec(&amx, &ret, AMX_EXEC_MAIN);
      while (err == AMX_ERR_SLEEP) {
        amx_printf("Paused execution on \"sleep\"\n");
        runmode = STEPPING;       /* use the "sleep" as a "coded" break point */
        err = amx_Exec(&amx, &ret, AMX_EXEC_CONT);
      } /* while */
    } else {
      #if !defined NO_REMOTE
        for ( ;; ) {
          /* wait for input from the host */
          remote_wait_rs232(&amx);
          /* call the debug procedure ourselves */
          amx_InternalDebugProc(&amx);
          /* reply, to say that the script may continue to run */
          remote_resume_rs232();
        } /* for */
      #endif
    } /* if */
  } /* if */

  amx_InternalDebugProc(NULL);  /* clean up debug information */
  if (terminal)
    amx_clrscr();
  #if !defined NO_REMOTE
    if (remote==REMOTE_RS232)
      remote_rs232(0,0);
  #endif
  if (err != AMX_ERR_NONE)
    amx_printf("Run time error %d\n", err);
  else
    amx_printf("Normal termination, return value %ld\n", (long)ret);

  dbg_FreeInfo(&amxdbg);
  free(program);

  return 0;
}

#endif /* !defined amx_Init */
