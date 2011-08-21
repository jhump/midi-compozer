/* FILE: findnote.c
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains routines that find previous or next notes in the
 * song based on specified criteria (if it has beams not flags or if it has
 * a tie...)...
 */

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <time.h>
#include "ems.h"

extern unsigned long elapsed, ticks, spt, realtime;
extern void VESAQuit(void);
extern void (*FM_Reset)(void);
extern void unloadkeyboardISR(void);
extern void unloadErrorHandler(void);
extern void unloadFMTimerISR(void);

#define NUMVOICES 	10
#define NUMCOL 		75
#define PAGES		(4096/(NUMCOL*NUMVOICES))

extern unsigned long *page, *song;
extern int curpage, hipage, mappedpage, EMShandle[];
extern unsigned pages;

/* returns column number of previous note or -1 if it hits beginning of
   page without a note or -2 if it hits a rest */
int getprevtie(int voice, int column) {
  int i = column; unsigned long j;
  if (i == 0)
    if (curpage == 0) return -2;
    else return -1;
  else
    for (--i; i >= 0; i--) {
      if (((j=page[i*NUMVOICES+voice])&0xE0000000l) == 0x20000000l)
	if (j&0x80) return -2;
	else return i;
    }
  if (curpage == 0) return -2;
  else return -1;
}

/* for redrawing notes: return column of next note if it is tied, otherwise
   return -1 (stops at end of page) */
int getnexttie(int voice, int column) {
  int i = column; unsigned long j;
  if (i == NUMCOL-1)
    return -1;
  else
    for (++i; i < NUMCOL; i++) {
      if (((j=page[i*NUMVOICES+voice])&0xE0000000l) == 0x20000000l)
	if (j&0x80) return -1;
	else if (j&0x4000) return i;
	else return -1;
    }
  return -1;
}

/* if next note is on this page or next note (on any following pages) is not
   tied then returns 0 */
int nextpagetie(int voice, int column) {
  int i = column+1, p = curpage, retval = 0; unsigned long j; char tmp[80];
  for (; p <= hipage; p++) {
    for (; i < NUMCOL; i++) {
      if (((j=page[i*NUMVOICES+voice])&0xE0000000l) == 0x20000000l) {
	if (p != curpage && !(j&0x80) && (j&0x4000l)) {
	  retval = -1;
	  goto _End;
	}
	else goto _End;
      }
    }
    if (p != hipage) {
      if (((p+1)%PAGES) == 0) {
	if (EMSmap (EMShandle[0],0,++mappedpage)) {
	  VESAQuit();
	  FM_Reset();
	  EMSfree (EMShandle[0]);
	  EMSfree (EMShandle[1]);
	  unloadkeyboardISR();
          unloadErrorHandler();
	  fprintf (stderr, "compozer: EMS mapping error\n");
	  _exit (1);
	}
	page = song;
      }
      else
	page += NUMVOICES*NUMCOL;
    }
    i = 0;
  }
_End:
  if (p != curpage) {
    if (EMSmap (EMShandle[0],0,curpage/PAGES)) {
      VESAQuit();
      FM_Reset();
      EMSfree (EMShandle[0]);
      EMSfree (EMShandle[1]);
      unloadkeyboardISR();
      unloadErrorHandler();
      fprintf (stderr, "compozer: EMS mapping error\n");
      _exit (1);
    }
    mappedpage = curpage/PAGES;
    page = song + (curpage%PAGES)*NUMVOICES*NUMCOL;
  }
  return retval;
}

/* trace back for previous note - if it is greater than an 8th note (no flags
   or beams) or if a bar is hit first return -1, otherwise return column */
int getprevbeam(int voice, int column) {
  int i; unsigned long j;
  if ((page[column*NUMVOICES+voice]&0xE0080000l) != 0x20080000l)
    return -1;
  for (i = column-1; i>=0; i--) {
    if (((j=page[i*NUMVOICES+voice])&0xE0000000l) == 0x20000000l) {
      if (((j>>8)&0x7) > 2 && !(j&0x80))
	return i;
      else return -1;
    }
    else if ((page[i*NUMVOICES]&0xE0000000l) == 0x80000000l)
      return -1;
  }
  return -1;
}

/* same as above but searches forward instead */
int getnextbeam(int voice, int column) {
  int i; unsigned long j;
  for (i = column+1; i<NUMCOL; i++) {
    if (((j=page[i*NUMVOICES+voice])&0xE0000000l) == 0x20000000l) {
      if ((j&0x00080000) && ((j>>8)&0x7) > 2 && !(j&0x80))
	return i;
      else return -1;
    }
    else if ((page[i*NUMVOICES]&0xE0000000l) == 0x80000000l)
      return -1;
  }
  return -1;
}

/* searches back and returns _value_ of previous note or accidental; returns
   -1 if no prior notes found; if flag then it will return value of key
   signature accidentals... otherwise it will keep searching back */
unsigned long getprevnote(int voice, int column, int flag) {
  int done, p, v = column, k;
  unsigned long t = 0xFFFFFFFFl;
  for (p = curpage, done = 0; !done && p >= 0; p--, v=NUMCOL-1) {
    if (mappedpage != p/PAGES) {
      if (EMSmap (EMShandle[0],0,p/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = p/PAGES;
    }
    page = song + (p%PAGES)*NUMVOICES*NUMCOL;
    for (;!done && v >= 0; v--) {
      if ((page[k=v*NUMVOICES]&0xE1000800l) == 0x40000800l && flag) {
        t = page[k];
        done = -1;
      }
      else if ((page[k+=voice]&0xE1000800l) == 0x40000000l) {
        t = page[k];
        if (flag)
          done = -1;
      }
      else if ((page[k]&0xE0000080l) == 0x20000000l) {
        if (t != 0xFFFFFFFFl)
          t = (t&0xFFl) | (page[k]&0xFFFFFF00l);
        else
          t = page[k];
        done = -1;
      }
    }
  }
  if (mappedpage != curpage/PAGES) {
    if (EMSmap (EMShandle[0],0,curpage/PAGES)) {
      VESAQuit();
      FM_Reset();
      EMSfree (EMShandle[0]);
      EMSfree (EMShandle[1]);
      unloadkeyboardISR();
      unloadErrorHandler();
      fprintf (stderr, "compozer: EMS mapping error\n");
      _exit (1);
    }
    mappedpage = curpage/PAGES;
  }
  page = song + (curpage%PAGES)*NUMVOICES*NUMCOL;
  return t;
}

/* searches back and returns _value_ of previous rest; returns
   -1 if no prior rests found; */
unsigned long getprevrest(int voice, int column) {
  int done, p, v = column, k;
  unsigned long t = 0xFFFFFFFFl;
  for (p = curpage, done = 0; !done && p >= 0; p--, v=NUMCOL-1) {
    if (mappedpage != p/PAGES) {
      if (EMSmap (EMShandle[0],0,p/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = p/PAGES;
    }
    page = song + (p%PAGES)*NUMVOICES*NUMCOL;
    for (;!done && v >= 0; v--) {
      if ((page[k=v*NUMVOICES+voice]&0xE0000080l) == 0x20000080l) {
        t = page[k];
        done = -1;
      }
    }
  }
  if (mappedpage != curpage/PAGES) {
    if (EMSmap (EMShandle[0],0,curpage/PAGES)) {
      VESAQuit();
      FM_Reset();
      EMSfree (EMShandle[0]);
      EMSfree (EMShandle[1]);
      unloadkeyboardISR();
      unloadErrorHandler();
      fprintf (stderr, "compozer: EMS mapping error\n");
      _exit (1);
    }
    mappedpage = curpage/PAGES;
  }
  page = song + (curpage%PAGES)*NUMVOICES*NUMCOL;
  return t;
}


/* returns column in high word and page in low word of next action */
unsigned long getnextitem(int voice, int column, unsigned pg) {
  int done = 0, j;
  unsigned long i;
  if (pg > hipage)
    return 0xFFFFFFFFl;
  if (curpage != pg) {
    if (mappedpage != pg/PAGES) {
      if (EMSmap (EMShandle[0],0,pg/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        unloadFMTimerISR();
        outportb (0x43, 0x3C);
        outportb (0x40, 0xFF);
        outportb (0x40, 0xFF);
        elapsed += ticks*spt;
        realtime += elapsed >> 16;
        stime (&realtime);
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = pg/PAGES;
    }
    page = song + (pg%PAGES)*NUMVOICES*NUMCOL;
  }
  while (pg <= hipage && !done) {
    while (column < NUMCOL && !done) {
      if ((page[column*NUMVOICES+voice]&0xE0000000l) != 0x0l)
        done = -1;
      else if (((i=page[column*NUMVOICES])&0xE0000000l) == 0x80000000l)
        done = -1;
      else if ((i&0xE0000000l) == 0xA0000000l)
          done = -1;
      else if ((i&0xE0000000l) == 0xC0000000l)
        done = -1;
      else if ((i&0xE0080000l) == 0xE0000000l)
        done = -1;
      else if ((i&0xE1000800l) == 0x40000800l)
        done = -1;
      else if ((i&0xE00E8000l) == 0xE0088000l)
        done = -1;
      else
        column++;
    }
    if (!done) {
      pg++;
      column = 0;
      if (pg <= hipage) {
        if (mappedpage != pg/PAGES) {
          if (EMSmap (EMShandle[0],0,pg/PAGES)) {
            VESAQuit();
            FM_Reset();
            EMSfree (EMShandle[0]);
            EMSfree (EMShandle[1]);
            unloadkeyboardISR();
            unloadErrorHandler();
            unloadFMTimerISR();
            outportb (0x43, 0x3C);
            outportb (0x40, 0xFF);
            outportb (0x40, 0xFF);
            elapsed += ticks*spt;
            realtime += elapsed >> 16;
            stime (&realtime);
            fprintf (stderr, "compozer: EMS mapping error\n");
            _exit (1);
          }
          mappedpage = pg/PAGES;
        }
        page = song + (pg%PAGES)*NUMVOICES*NUMCOL;
      }
    }
  }
  if (curpage != pg) {
    if (mappedpage != curpage/PAGES) {
      if (EMSmap (EMShandle[0],0,curpage/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        unloadFMTimerISR();
        outportb (0x43, 0x3C);
        outportb (0x40, 0xFF);
        outportb (0x40, 0xFF);
        elapsed += ticks*spt;
        realtime += elapsed >> 16;
        stime (&realtime);
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = curpage/PAGES;
    }
    page = song + (curpage%PAGES)*NUMVOICES*NUMCOL;
  }
  if (done)
    return (((unsigned long)column)<<16) + (long)pg;
  else
    return 0xFFFFFFFFl;
}

/* finds ending with ending number and returns column in high word and page
   in low word; returns -1 if no ending found (end of song) - if ending <= 0
   then searches forward for a coda instead */
unsigned long getending(int ending, int column, unsigned pg) {
  int done = 0;
  unsigned long i, j;
  if (pg > hipage)
    return 0xFFFFFFFFl;
  if (curpage != pg) {
    if (mappedpage != pg/PAGES) {
      if (EMSmap (EMShandle[0],0,pg/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        unloadFMTimerISR();
        outportb (0x43, 0x3C);
        outportb (0x40, 0xFF);
        outportb (0x40, 0xFF);
        elapsed += ticks*spt;
        realtime += elapsed >> 16;
        stime (&realtime);
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = pg/PAGES;
    }
    page = song + (pg%PAGES)*NUMVOICES*NUMCOL;
  }
  while (pg <= hipage && !done) {
    while (column < NUMCOL && !done) {
      if (ending <= 0 && (page[column*NUMVOICES]&0xE0000700l) == 0xA0000300l)
        done = -1;
      else if (ending > 0 &&
              ((j=page[column*NUMVOICES])&0xE0000000l) == 0xC0000000l &&
              (j&(0x80l<<ending)))
        done = -1;
      else
        column++;
    }
    if (!done) {
      pg++;
      column = 0;
      if (pg <= hipage) {
        if (mappedpage != pg/PAGES) {
          if (EMSmap (EMShandle[0],0,pg/PAGES)) {
            VESAQuit();
            FM_Reset();
            EMSfree (EMShandle[0]);
            EMSfree (EMShandle[1]);
            unloadkeyboardISR();
            unloadErrorHandler();
            unloadFMTimerISR();
            outportb (0x43, 0x3C);
            outportb (0x40, 0xFF);
            outportb (0x40, 0xFF);
            elapsed += ticks*spt;
            realtime += elapsed >> 16;
            stime (&realtime);
            fprintf (stderr, "compozer: EMS mapping error\n");
            _exit (1);
          }
          mappedpage = pg/PAGES;
        }
        page = song + (pg%PAGES)*NUMVOICES*NUMCOL;
      }
    }
  }
  if (curpage != pg) {
    if (mappedpage != curpage/PAGES) {
      if (EMSmap (EMShandle[0],0,curpage/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        unloadFMTimerISR();
        outportb (0x43, 0x3C);
        outportb (0x40, 0xFF);
        outportb (0x40, 0xFF);
        elapsed += ticks*spt;
        realtime += elapsed >> 16;
        stime (&realtime);
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = curpage/PAGES;
    }
    page = song + (curpage%PAGES)*NUMVOICES*NUMCOL;
  }
  if (done)
    return (((unsigned long)column)<<16) + (long)pg;
  else
    return 0xFFFFFFFFl;
}

/* returns value at colpg (where high word is column and low word is page)
   for voice... if colpg is invalid (beyond end of song) it returns 0 (which
   is a NULL event) */
unsigned long getevent(int voice, unsigned long colpg) {
  unsigned pg = colpg&0xFFFF, col = colpg >> 16;
  unsigned long l, i;
  if (pg > hipage)
    return 0;
  if (curpage != pg) {
    if (mappedpage != pg/PAGES) {
      if (EMSmap (EMShandle[0],0,pg/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        unloadFMTimerISR();
        outportb (0x43, 0x3C);
        outportb (0x40, 0xFF);
        outportb (0x40, 0xFF);
        elapsed += ticks*spt;
        realtime += elapsed >> 16;
        stime (&realtime);
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = pg/PAGES;
    }
    page = song + (pg%PAGES)*NUMVOICES*NUMCOL;
  }
  if (((i=page[col*NUMVOICES])&0xE0000000l) == 0x80000000l)
    l = i;
  else if ((i&0xE0000000l) == 0xA0000000l)
    l = i;
  else if ((i&0xE0000000l) == 0xC0000000l)
    l = i;
  else if ((i&0xE0080000l) == 0xE0000000l)
    l = i;
  else if ((i&0xE1000800l) == 0x40000800l)
    l = i;
  else if ((i&0xE00E8000l) == 0xE0088000l)
    l = i;
  else
    l = page[col*NUMVOICES+voice];
  if (curpage != pg) {
    if (mappedpage != curpage/PAGES) {
      if (EMSmap (EMShandle[0],0,curpage/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        unloadFMTimerISR();
        outportb (0x43, 0x3C);
        outportb (0x40, 0xFF);
        outportb (0x40, 0xFF);
        elapsed += ticks*spt;
        realtime += elapsed >> 16;
        stime (&realtime);
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = curpage/PAGES;
    }
    page = song + (curpage%PAGES)*NUMVOICES*NUMCOL;
  }
  return l;
}
