/* FILE: getfile.c
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains the source for GetFileName() which uses the DOS
 * file system routines find_first() and find_next() to create a list of
 * files for the user to interactively browse to select a filename.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include "gr.h"
#include "keys.h"

extern void VESAQuit(void);
extern void (*FM_Reset)(void);
extern int EMSfree(int handle);

void unloadErrorHandler(void);

extern void Message(char *str);
extern void redraw_screen(void);

struct _node {
  char name[13], dir;
} list[4096];

typedef struct {
  char reserved[21];
  unsigned char attr;
  unsigned int time, date;
  unsigned long size;
  char name[13];
} fileinfo;

extern int EMShandle[];
extern unsigned char menufg, menubg, menufgh, menubgh;

char tmpfn[81];
char *DTA = NULL, *Errors[16] =
{"write-protect error", "unknown unit error", "drive not ready error",
 "unknown command error", "data error (bad CRC)",
 "bad request structure length error", "seek error", "unknown media type error",
 "sector not found error", "printer out of paper error", "write fault error",
 "read fault error", "general failure error", "unknown error", "unknown error",
 "invalid disk change"};
int  Error = 0;
void interrupt (*oldErrorHandler)(void);

extern volatile char shiftflags[3];

int getcurdir (char drv, char *dname) {
  unsigned s = (unsigned long)dname >> 16, o = (unsigned long)dname & 0xFFFF, stat;
  unloadkeyboardISR(); /* we do this in case drive B: in a single floppy
                          system is referenced... this is because DOS will
                          print a message and request a keystroke (which
                          it will never receive while our ISR is installed) */
  asm {
    mov ah, 0x47
    mov dl, drv
    mov bx, s
    mov ds, bx
    mov si, o
    int 0x21
    jc _getcurdirErr
    xor ax, ax
_getcurdirErr:
    mov stat, ax
  }
  installkeyboardISR(); /* re-install the ISR so our program continues to work */
  memset (shiftflags,0,3); /* because our ISR would not have caught the
                              break codes for these since we installed the
                              original above */
  if (stat == 0)
    return 0;
  else {
    Error = stat;
    return -1;
  }
}

char getdrv (void) {
  asm {
    mov ah, 0x19
    int 0x21
  }
  return _AL;
}

char setdrv (char drv) {
  asm {
    mov ah, 0x0E
    mov dl, drv
    int 0x21
  }
  return _AL;
}

char *getDTA (void) {
  unsigned s, o;
  asm {
    mov ah, 0x2F
    int 0x21
    mov o, bx
    mov bx, es
    mov s, bx
  }
  return MK_FP(s,o);
}

int findfirst (char *fn, int attr, fileinfo *buf) {
  unsigned s = (unsigned long)fn >> 16, o = (unsigned long)fn & 0xFFFF, stat;
  if (DTA == NULL)
    DTA = getDTA();
  asm {
    push ds
    mov ah, 0x4e
    mov cx, attr
    mov bx, s
    mov ds, bx
    mov dx, o
    int 0x21
    jc  _findfirstErr
    xor ax, ax
_findfirstErr:
    mov stat, ax
    pop ds
  }
  if (stat == 0) {
    memcpy (buf,DTA,sizeof(fileinfo));
    return 0;
  }
  else {
    Error = stat;
    return -1;
  }
}

int findnext (fileinfo *buf) {
  unsigned stat;
  if (DTA == NULL)
    DTA = getDTA();
  asm {
    mov ah, 0x4f
    int 0x21
    jc  _findnextErr
    xor ax, ax
_findnextErr:
    mov stat, ax
  }
  if (stat == 0) {
    memcpy (buf,DTA,sizeof(fileinfo));
    return 0;
  }
  else {
    Error = stat;
    return -1;
  }
}

void interrupt ErrorHandler (void) {
  char cd, dr; int er, i; unsigned segm, offs;
  asm {
    and di, 0xff
    mov er, di
    mov dr, al
    mov cd, ah
  }
    /* if we can, fail the calling function */
  if (cd & 0x08) cd = 3;
    /* else if we can, retry the operation */
  else if (cd & 0x10) cd = 1;
    /* else if we can, ignore the error */
  else if (cd & 0x20) cd = 0;
    /* else, we can do nothing but terminate the program so we prepare
       for a clean shutdown */
  else {
    VESAQuit();
    FM_Reset();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    unloadkeyboardISR();
    unloadErrorHandler();
    /* write message to screen */
    if (er < 16) {
      sprintf (tmpfn, "compozer: unrecoverable %s (%c:)\n", Errors[er], dr+'A');
      for (i = 0; tmpfn[i]; i++)
        tmpfn[i] = tolower(tmpfn[i]);
    }
    else
      strcpy (tmpfn, "compozer: unrecoverable unknown error\n");
    fprintf (stderr, tmpfn);
    /* instead of setting disposition code to 2 to terminate process we
       will simply exit since returning 2 in AL from the error handler
       (at least in this case) seems to crash the computer... */
    _exit(1);
  }
  asm mov al, cd
  return;
}

int partition (int begin, int end) {
  char *str = list[begin].name; int i = begin-1, j = end+1; struct _node tmp;
  while (-1) {
    do j--;
    while (strcmp (list[j].name, str) > 0);
    do i++;
    while (strcmp (list[i].name, str) < 0);
    if (i < j) {
      memcpy (&tmp, &(list[j]), sizeof(struct _node));
      memcpy (&(list[j]), &(list[i]), sizeof(struct _node));
      memcpy (&(list[i]), &tmp, sizeof(struct _node));
    }
    else
      return j;
  }
}

void quicksort (int begin, int end) {
  int middle;
  if (begin < end) {
    middle = partition (begin, end);
    quicksort (begin, middle);
    quicksort (middle+1, end);
  }
  return;
}

int GetFileList (char drv, char *dir, char *mask) {
  fileinfo tmp; int dirs = 0, files = 0; char path[84], filter[88];
  if (dir[0] == 0)
    sprintf (path, "%c:", drv+'A');
  else
    sprintf (path, "%c:\\%s", drv+'A', dir);
  sprintf (filter, "%s\\*.*", path);
  if (findfirst (filter, 0x12, &tmp) != -1) {
    if ((tmp.attr & 0x10) && strcmp (tmp.name, ".")) {
      memcpy (list[dirs].name, tmp.name, 13);
      list[dirs++].dir = -1;
    }
    while (dirs < 4096 && findnext (&tmp) != -1)
      if ((tmp.attr & 0x10) && strcmp (tmp.name, ".")) {
        memcpy (list[dirs].name, tmp.name, 13);
        list[dirs++].dir = -1;
      }
  }
  sprintf (filter, "%s\\*%s", path, mask);
  if (findfirst (filter, 0x03, &tmp) != -1) {
    *(strchr(tmp.name,'.')) = 0;
    memcpy (list[dirs+files].name, tmp.name, 13);
    list[dirs+(files++)].dir = 0;
    while (dirs+files < 4096 && findnext (&tmp) != -1) {
      *(strchr(tmp.name,'.')) = 0;
      memcpy (list[dirs+files].name, tmp.name, 13);
      list[dirs+(files++)].dir = 0;
    }
  }
  if (dirs+files > 0)
    quicksort (0,dirs+files-1);
  return dirs + files;
}

int GetName (char *str, int numi) {
  int i,j;
  for (i = 0; i<numi && (j=strncmpi(list[i].name,str,strlen(str))) < 0; i++);
  if (i == 0) return 0;
  else if (j == 0) return i;
  else return i-1;
}

char *GetFileName (int autof, char *mask) {
  int i, numi, y, chx = 0, top = 0, _x, _y, curs = 0, olds = 0, draw = 0;
  char ch, dir[81], cur[57] = "", drv, blink = 0, bc = menubg;
  /* draw info box */
  /* get files */
  drv = getdrv ();
  if (getcurdir (0, dir) == -1) {
    Message("Current disk drive is invalid !");
    return NULL;
  }
  numi = GetFileList (drv, dir, mask);
  /* draw the window */
  if (dir[0] == 0)
    sprintf (tmpfn, "%c:\\", drv+'A');
  else
    sprintf (tmpfn, "%c:\\%s\\", drv+'A', dir);
  if (autof) {
    box (52,164,588,316,menubg);
    line (51,163,51,317,menufg);
    line (589,163,589,317,menufg);
    line (51,163,589,163,menufg);
    line (51,317,589,317,menufg);
    y = 180;
  }
  else {
    box (52,156,588,324,menubg);
    line (51,155,51,325,menufg);
    line (589,155,589,325,menufg);
    line (51,155,589,155,menufg);
    line (51,325,589,325,menufg);
    outtextxy (56,164,"FILENAME:", menufg);
    y = 188;
  }
  outtextxy (56,y-8,"PATH:",menufg);
  outtextxy (56,y,tmpfn,menufg);
  line (79,y+15,79,y+96,menufg);
  line (560,y+15,560,y+96,menufg);
  line (79,y+15,560,y+15,menufg);
  line (79,y+96,560,y+96,menufg);
  for (i = top; i < ((top+30>numi)?(numi):(top+30)); i++) {
    outtextxy (_x=(80+160*((i-top)/10)),_y=(y+16+(((i-top)%10)<<3)),list[i].name,menufg);
    if (list[i].dir)
      outtextxy (_x + 112, _y, "[dir]", menufg);
  }
  if (numi > 0) {
    _x=80+160*((curs-top)/10);
    _y=y+16+(((curs-top)%10)<<3);
    box (_x,_y,_x+159,_y+7,menubgh);
    outtextxy (_x,_y,list[curs].name,menufgh);
    if (list[curs].dir)
      outtextxy (_x + 112, _y, "[dir]", menufgh);
  }
  else
    outtextxy (264,y+52,"NO FILES FOUND",menufg);
  outtextxy (87,y+104,"  \030,\031,\033,\032", menufg);
  outtextxy (79,y+112,"PageUp,PageDn - Move Cursor", menufg);
  outtextxy (84,y+120,"  Home,End", menufg);
  outtextxy (425,y+108, " <Enter> - Accept", menufg);
  outtextxy (425,y+116, "<Escape> - Cancel", menufg);
  /* process user input */
  do {
    if (kbhit())
      chx = getxkey();
    else
      chx = 0;
    /* cursor blinking */
    if (!autof && blink == 0) {
      blink = 20;
      if (bc == menufg) bc = menubg;
      else bc = menufg;
      line (136+(strlen(cur)<<3),172,143+(strlen(cur)<<3),172,bc);
    }
    /* take appropriate action */
    switch (chx & 0xff) {
      case 0x47: cur[0] = 0; draw = -1;
                 curs = 0;
                 break;
      case 0x48: cur[0] = 0; draw = -1;
                 if (curs > 0) curs--;
                 break;
      case 0x49: cur[0] = 0; draw = -1;
                 if (curs > 29) curs-=30;
                 else curs = 0;
                 break;
      case 0x4b: cur[0] = 0; draw = -1;
                 if (curs > 9) curs-=10;
                 else curs = 0;
                 break;
      case 0x4d: cur[0] = 0; draw = -1;
                 if (curs < numi-10) curs+=10;
                 else curs = numi-1;
                 break;
      case 0x4f: cur[0] = 0; draw = -1;
                 curs = numi-1;
                 break;
      case 0x50: cur[0] = 0; draw = -1;
                 if (curs < numi-1) curs++;
                 break;
      case 0x51: cur[0] = 0; draw = -1;
                 if (curs < numi-30) curs+=30;
                 else curs = numi-1;
                 break;
      /* change directories */
      case 0x1C: if (autof || (!autof && cur[0] == 0)) {
                   if (numi == 0)
                     chx = 0;
                   else if (list[curs].dir) {
                     chx = 0;
                     if (strcmp (list[curs].name, "..") == 0) {
                       for (i = strlen(dir)-1; i >= 0 && dir[i] != '\\'; i--);
                       if (i < 0) i = 0;
                       dir[i] = 0;
                     }
                     else {
                       if (dir[0])
                         strcat (dir, "\\");
                       strcat (dir,list[curs].name);
                     }
                     box (56,y,56+(strlen(tmpfn)<<3),y+7,menubg);
                     numi = GetFileList (drv,dir,mask);
                     if (dir[0] == 0)
                       sprintf (tmpfn, "%c:\\", drv+'A');
                     else
                       sprintf (tmpfn, "%c:\\%s\\", drv+'A', dir);
                     outtextxy (56,y,tmpfn,menufg);
                     top = curs = olds = 0;
                     box (80,y+16,559,y+95,menubg);
                     for (i = top; i < ((top+30>numi)?(numi):(top+30)); i++) {
                       outtextxy (_x=(80+160*((i-top)/10)),_y=(y+16+(((i-top)%10)<<3)),list[i].name,menufg);
                       if (list[i].dir)
                         outtextxy (_x + 112, _y, "[dir]", menufg);
                     }
                     if (numi > 0) {
                       _x=80+160*((curs-top)/10);
                       _y=y+16+(((curs-top)%10)<<3);
                       box (_x,_y,_x+159,_y+7,menubgh);
                       outtextxy (_x,_y,list[curs].name,menufgh);
                       if (list[curs].dir)
                         outtextxy (_x + 112, _y, "[dir]", menufgh);
                     }
                     else
                       outtextxy (264,y+52,"NO FILES FOUND",menufg);
                     cur[0] = 0; draw = -1;
                   }
                 }
                 break;
      default:
        ch = toupper(ASCIIcode(chx));
        /* change drives */
        if ((chx&0x400) && ch > 0 && ch < 27) {
          if (ch-1 != drv)
            if (getcurdir (ch, dir) == -1)
              Message("Invalid disk drive !");
            else {
              drv = ch-1;
              box (56,y,588,y+7,menubg);
              numi = GetFileList (drv, dir,mask);
              box (80,y+16,559,y+95,menubg);
              if (dir[0] == 0)
                sprintf (tmpfn, "%c:\\", drv+'A');
              else
                sprintf (tmpfn, "%c:\\%s\\", drv+'A', dir);
              outtextxy (56,y,tmpfn,menufg);
              top = curs = olds = 0;
              for (i = top; i < ((top+30>numi)?(numi):(top+30)); i++) {
                outtextxy (_x=(80+160*((i-top)/10)),_y=(y+16+(((i-top)%10)<<3)),list[i].name,menufg);
                if (list[i].dir)
                  outtextxy (_x + 112, _y, "[dir]", menufg);
              }
              if (numi > 0) {
                _x=80+160*((curs-top)/10);
                _y=y+16+(((curs-top)%10)<<3);
                box (_x,_y,_x+159,_y+7,menubgh);
                outtextxy (_x,_y,list[curs].name,menufgh);
                if (list[curs].dir)
                  outtextxy (_x + 112, _y, "[dir]", menufgh);
              }
              else
                outtextxy (264,y+52,"NO FILES FOUND",menufg);
              cur[0] = 0; draw = -1;
            }
        }
        /* if TAB was pressed set cur string to entry in directory */
        else if (ch == 9 && !list[curs].dir) {
          draw = -1;
          strcpy (cur, list[curs].name);
        }
        /* add to cur string */
        else if (ch > 32 && ch != '+' && ch != '=' && ch != '\\' && ch !='|' &&
                 ch != '[' && ch != ']' && ch != ':' && ch != ';' && ch != ',' &&
                 ch != '.' && ch != '?' && ch != '<' && ch !='>' && ch != '/' &&
                 ch != '*' && ch != '"') {
          draw = -1;
          if (strlen(cur) < 8) {
            cur[i=strlen(cur)] = ch;
            cur[i+1] = 0;
          }
          else
            cur[strlen(cur)-1] = ch;
          curs = GetName (cur, numi);
        }
        else if (ch == 8 && cur[0] != 0) {
          draw = -1;
          cur[strlen(cur)-1] = 0;
          curs = GetName (cur, numi);
        }
    }
    if (olds != curs && numi > 0) {
      if (curs >= top+30 || curs < top) {
        top = (curs/30)*30;
        box (80,y+16,559,y+95,menubg);
        for (i = top; i < ((top+30>numi)?(numi):(top+30)); i++) {
          outtextxy (_x=(80+160*((i-top)/10)),_y=(y+16+(((i-top)%10)<<3)),list[i].name,menufg);
          if (list[i].dir)
            outtextxy (_x + 112, _y, "[dir]", menufg);
        }
      }
      else {
        _x=80+160*((olds-top)/10);
        _y=y+16+(((olds-top)%10)<<3);
        box (_x,_y,_x+159,_y+7,menubg);
        outtextxy (_x,_y,list[olds].name,menufg);
        if (list[olds].dir)
          outtextxy (_x + 112, _y, "[dir]", menufg);
      }
      _x=80+160*((curs-top)/10);
      _y=y+16+(((curs-top)%10)<<3);
      box (_x,_y,_x+159,_y+7,menubgh);
      outtextxy (_x,_y,list[curs].name,menufgh);
      if (list[curs].dir)
        outtextxy (_x + 112, _y, "[dir]", menufgh);
      olds = curs;
    }
    if (!autof && draw) {
      box (136,164,589,172,menubg);
      outtextxy (136,164,cur,menufg);
      line (136+(strlen(cur)<<3),172,143+(strlen(cur)<<3),172,bc);
      draw = 0;
    }
    delay (10); /* delay 100th of a second so that the cursor will blink
                   at the proper rate (~5 times a second) */
    blink--;
  } while ((chx&0xfff) != 0x01 && (chx&0xfff) != 0x1C);
  /* done */
  redraw_screen();
  if ((chx&0xfff) == 0x01)
    return NULL;
  else if (autof || cur[0] == 0)
    strcat (tmpfn, list[curs].name);
  else
    strcat (tmpfn, cur);
  strcat (tmpfn, mask);
  return tmpfn;
}

void installErrorHandler (void) {
  oldErrorHandler = getvect (0x24);
  setvect (0x24, ErrorHandler);
  return;
}

void unloadErrorHandler (void) {
  setvect (0x24, oldErrorHandler);
  return;
}
