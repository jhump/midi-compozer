/* FILE: main.c
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains the main() routine for the program compozer...
 */

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <time.h>
#include "fm.h"
#include "gr.h"
#include "ems.h"
#include "keys.h"
#include "getfile.h"
#include "function.h"
#include "findnote.h"

/*****************************  MAIN  ****************************************/

int main (void) {
  unsigned ch, i, j;
  int k, n, t;
  unsigned long l;

  if (FM_Detect())
    _sound = 0;
  switch (load_instrum()) {
    case -1 :
      fprintf (stderr, "compozer: could not find General MIDI instrument bank\n");
      return 1;
    case -2 :
      fprintf (stderr, "compozer: General MIDI instrument bank not valid\n");
      return 1;
    case -3 :
      fprintf (stderr, "compozer: could not find Percussion instrument bank\n");
      return 1;
    case -4 :
      fprintf (stderr, "compozer: Percussion instrument bank not valid\n");
      return 1;
  }
  switch (initEMS()) {
    case -1 :
      fprintf (stderr, "compozer: EMS not installed or not active\n");
      return 1;
    case -2 :
      fprintf (stderr, "compozer: not enough EMS pages available\n");
      return 1;
    case -3 :
      fprintf (stderr, "compozer: not enough EMS handles available\n");
      return 1;
    case -4 :
      fprintf (stderr, "compozer: EMS allocation error\n");
      return 1;
    case -5 :
      fprintf (stderr, "compozer: EMS mapping error\n");
      return 1;
  }
  installkeyboardISR();
  installErrorHandler();
  memset (colbuf, bg, 2*HEIGHT);
  if (VESAInit()) {
    fprintf (stderr, "compozer: error initializing VESA mode 0x%x\n", MODE);
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    unloadkeyboardISR();
    unloadErrorHandler();
    return 1;
  }
  memset (song, 0, 16384);
  load_config();
  change_palette();
  display_startup();
  redraw_screen();
  while (!quitflag) {
    ch = getxkey() & 0x0FFF; /* we'll ignore lock states */
    /* Alt + menu shortcut will highlight the menu bar, bring down submenu, and
       allow menu selection */
    if (ch != 0x0200 && (ch >> 8) == 0x02)
      for (i = 0; i < 7; i++)
	if ((ch & 0xff) == ASCIItoScan[mainmenu[i].name[mainmenu[i].shortcut]])	{
	  menucur = i;
	  submenucur = 0;
	  menudown = -1;
	  navigate_menu();
	}
    /* check hotkey list */
    for (i = 0; i < 43; i++)
      if (ch == hkeys[i].key)
	if (hkeys[i].proc)
	  hkeys[i].proc();
	else
	  *(hkeys[i].change) = !*(hkeys[i].change);
    /* check built-in keys list */
    switch (ch) {
      /* <escape> unselects area */
      case 0x0001: if (editmode) {
		     undraw_col();
		     box ((markbegin<<3)+COLOFFS,TOPY,(markend<<3)+COLOFFS+7,LOWY,bg);
		     editmode = 0;
		     redraw_music(markbegin, markend);
		     redraw_info();
		     draw_col();
		   }
                   break;
      /* Ctrl+Alt+Del will close the program (user can press it again if they
         really want to reboot) */
      case 0x0653: VESAQuit();
                   if (_sound) FM_Reset();
		   unloadkeyboardISR();
                   unloadErrorHandler();
		   EMSfree (EMShandle[0]);
		   EMSfree (EMShandle[1]);
		   printf ("To reboot press CTRL+ALT+DEL again.\n");
		   return 2;
      /* F10 or Alt by itself will just highlight the menu bar and allow menu
	 selection */
      case 0x0044:
      case 0x0200: menucur = 0; navigate_menu(); break;
      /* Alt + Number changes the current voice to that number */
      case 0x0202: case 0x0203: case 0x0204: case 0x0205: case 0x0206:
      case 0x0207: case 0x0208: case 0x0209: case 0x020A: case 0x020B:
	j = curstaff; i = curvoice;
	curvoice = (ch & 0xFF) - 2;
	if (curvoice != i) {
	  undraw_col();
	  curstaff = voiceinfo[curvoice];
	  if (editmode) {
	    if (markbegin > 0) {
	      undrawrow_nvoice (curvoice, 0, markbegin-1);
	      drawrow_nvoice (i, 0, markbegin-1);
	    }
	    if (markend < NUMCOL-1) {
	      undrawrow_nvoice (curvoice, markend+1, NUMCOL-1);
	      drawrow_nvoice (i, markend+1, NUMCOL-1);
	    }
	  }
	  else {
	    undrawrow_nvoice (curvoice, 0, NUMCOL-1);
	    drawrow_nvoice (i, 0, NUMCOL-1);
	  }
	  if (curstaff != j) {
	    drawstaff (j, 0, NUMCOL-1);
	    curstaffy = curstaff*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(curstaff<<1)-1];
	  }
	  if (editmode) voice[curvoice] = -1;
	  redraw_staff (curstaff, 0, NUMCOL-1);
	  draw_col();
	}
	redraw_info();
	break;
      /* Shift + Number toggles the current voice */
      case 0x0102: case 0x0103: case 0x0104: case 0x0105: case 0x0106:
      case 0x0107: case 0x0108: case 0x0109: case 0x010A: case 0x010B:
	if (editmode) {
	  undraw_col();
	  i = (ch & 0xFF) - 2;
	  voice[i] = !voice[i];
	  /* just in case they try to have zero selected voices... */
	  voice[curvoice] = -1;
	  if (!voice[i])
	    redraw_staff (voiceinfo[i], markbegin, markend);
	  else
	    drawrow_nvoice (i, markbegin, markend);
	  redraw_info();
	  draw_col();
        }
        else if (ch == 0x0104)
          goto _enterTimeSig;   /* '#' is time sig */
        else if (ch == 0x010A)
          goto _enterFermata;   /* '(' is fermata */
        break;
      /* move the cursor column to the left and unselect area if necessary */
      case 0x004B: exit_editmode();
		   if (curpos > 0) {
		     undraw_col();
		     curpos--;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* move the cursor column to the right and unselect area if necessary */
      case 0x004D: exit_editmode();
		   if (curpos < NUMCOL-1) {
		     undraw_col();
		     curpos++;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* move cursor to the left and add that column to the selected area */
      case 0x014B: if (curpos > 0) {
		     undraw_col();
		     enter_editmode();
		     if (curpos == markbegin) {
		       markbegin--;
		       box ((markbegin<<3)+COLOFFS,TOPY,(markbegin<<3)+COLOFFS+7,LOWY,selbg);
		       redraw_column (markbegin);
		     }
		     else if (curpos == markend) {
		       markend--;
		       box ((markend<<3)+COLOFFS+8,TOPY,(markend<<3)+COLOFFS+15,LOWY,bg);
		       redraw_column (markend+1);
		     }
		     curpos--;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* move cursor to the right and add that column to the selected area */
      case 0x014D: if (curpos < 74) {
		     undraw_col();
		     enter_editmode();
		     if (curpos == markend) {
		       markend++;
		       box ((markend<<3)+COLOFFS,TOPY,(markend<<3)+COLOFFS+7,LOWY,selbg);
		       redraw_column (markend);
		     }
		     else if (curpos == markbegin) {
		       markbegin++;
		       box ((markbegin<<3)+COLOFFS-8,TOPY,(markbegin<<3)+COLOFFS-1,LOWY,bg);
		       redraw_column (markbegin-1);
		     }
		     curpos++;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* move the cursor column all the way left and unselect area if necessary */
      case 0x0047: exit_editmode();
		   if (curpos > 0) {
		     undraw_col();
		     curpos = 0;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* move the cursor column all the way right and unselect area if necessary */
      case 0x004F: exit_editmode();
		   if (curpos < NUMCOL-1) {
		     undraw_col();
		     curpos = NUMCOL-1;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* move cursor all the way left and add the columns to the selected area */
      case 0x0147: if (curpos > 0) {
		     undraw_col();
		     enter_editmode();
		     if (curpos == markbegin) {
		       box (COLOFFS,TOPY,(markbegin<<3)+COLOFFS-1,LOWY,selbg);
		       i = markbegin; markbegin = 0;
		       redraw_music (0, i-1);
		     }
		     else if (curpos == markend) {
		       box (COLOFFS,TOPY,(markbegin<<3)+COLOFFS-1,LOWY,selbg);
		       i = markbegin; markbegin = 0;
		       redraw_music (0, i-1);
		       box ((i<<3)+COLOFFS+8,TOPY,(markend<<3)+COLOFFS+7,LOWY,bg);
		       j = markend; markend = i;
		       redraw_music (i+1, j);
		     }
		     curpos = 0;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* move cursor all the way right and add the columns to the selected area */
      case 0x014F: if (curpos < 74) {
		     undraw_col();
		     enter_editmode();
		     if (curpos == markend) {
		       box ((markend<<3)+COLOFFS+8,TOPY,(NUMCOL<<3)+COLOFFS-1,LOWY,selbg);
		       i = markend; markend = NUMCOL-1;
		       redraw_music (i+1, NUMCOL-1);
		     }
		     else if (curpos == markbegin) {
		       box ((markend<<3)+COLOFFS+8,TOPY,(NUMCOL<<3)+COLOFFS-1,LOWY,selbg);
		       i = markend; markend = NUMCOL-1;
		       redraw_music (i+1, NUMCOL-1);
		       box ((markbegin<<3)+COLOFFS,TOPY,(i<<3)+COLOFFS-1,LOWY,bg);
		       j = markbegin; markbegin = i;
		       redraw_music (j, i-1);
		     }
		     curpos = NUMCOL-1;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* go to the previous page */
      case 0x0049: exit_editmode();
		   if (curpage > 0) {
		     if (page == song) {
		       curpage--;
		       if (EMSmap (EMShandle[0],0,--mappedpage) == -1) {
			 VESAQuit();
                         if (_sound) FM_Reset();
			 EMSfree (EMShandle[0]);
			 EMSfree (EMShandle[1]);
			 unloadkeyboardISR();
                         unloadErrorHandler();
			 fprintf (stderr, "compozer: EMS mapping error\n");
			 return 1;
		       }
		       page = song + NUMVOICES*NUMCOL*(PAGES-1);
		     }
		     else {
		       page -= NUMVOICES*NUMCOL; curpage--;
		     }
		     box (0,TOPY,MAXX,LOWY,bg);
		     redraw_music(0,NUMCOL-1); redraw_info(); draw_col();
		   }
		   break;
      /* go to the next page */
      case 0x0051: exit_editmode();
		   if (curpage == hipage) {
                     getkeysig(NUMCOL);
		     if (!((curpage+1) % PAGES)) {
		       if ((i = EMSrealloc (EMShandle[0], pages+1)) == 0xFFFF || i < pages) {
			 VESAQuit();
                         if (_sound) FM_Reset();
			 EMSfree (EMShandle[0]);
			 EMSfree (EMShandle[1]);
			 unloadkeyboardISR();
                         unloadErrorHandler();
			 fprintf (stderr, "compozer: EMS reallocation error\n");
			 return 1;
		       }
		       else if (i == pages) {
			 Message ("Not enough available EMS memory for another page!");
			 break;
		       }
		       pages = i; hipage++; curpage++;
		       if (EMSmap (EMShandle[0],0,curpage/PAGES) == -1) {
			 VESAQuit();
                         if (_sound) FM_Reset();
			 EMSfree (EMShandle[0]);
			 EMSfree (EMShandle[1]);
			 unloadkeyboardISR();
                         unloadErrorHandler();
			 fprintf (stderr, "compozer: EMS mapping error\n");
			 return 1;
		       }
		       mappedpage = curpage/PAGES;
		       memset (song, 0, 16384);
		       page = song;
		     }
		     else {
		       page += NUMVOICES*NUMCOL; curpage++; hipage++;
		     }
		     writekeysig();
		   }
		   else if (!((curpage+1) % PAGES)) {
		     curpage++;
		     if (EMSmap (EMShandle[0],0,++mappedpage) == -1) {
		       VESAQuit();
                       if (_sound) FM_Reset();
		       EMSfree (EMShandle[0]);
		       EMSfree (EMShandle[1]);
		       unloadkeyboardISR();
                       unloadErrorHandler();
		       fprintf (stderr, "compozer: EMS mapping error\n");
		       return 1;
		     }
		     page = song;
		   }
		   else {
		     page += NUMVOICES*NUMCOL; curpage++;
		   }
		   box (0,TOPY,MAXX,LOWY,bg);
		   redraw_music(0,NUMCOL-1); redraw_info(); draw_col();
		   break;
      /* go to the first column of first page */
      case 0x0447: exit_editmode();
		   if (curpage > 0) {
                     mappedpage = curpage = 0;
                     if (EMSmap (EMShandle[0],0,mappedpage) == -1) {
                       VESAQuit();
                       if (_sound) FM_Reset();
                       EMSfree (EMShandle[0]);
                       EMSfree (EMShandle[1]);
                       unloadkeyboardISR();
                       unloadErrorHandler();
                       fprintf (stderr, "compozer: EMS mapping error\n");
                       return 1;
                     }
                     page = song;
                     curpos = 0;
		     box (0,TOPY,MAXX,LOWY,bg);
		     redraw_music(0,NUMCOL-1); redraw_info(); draw_col();
		   }
                   else if (curpos > 0) {
		     undraw_col();
		     curpos = 0;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* go to the last column of last page */
      case 0x044F: exit_editmode();
                   if (curpage < hipage) {
                     curpage = hipage; mappedpage = hipage/PAGES;
                     if (EMSmap (EMShandle[0],0,mappedpage) == -1) {
		       VESAQuit();
                       if (_sound) FM_Reset();
		       EMSfree (EMShandle[0]);
		       EMSfree (EMShandle[1]);
		       unloadkeyboardISR();
                       unloadErrorHandler();
		       fprintf (stderr, "compozer: EMS mapping error\n");
		       return 1;
		     }
                     page = NUMVOICES*NUMCOL*(hipage%PAGES) + song;
                     curpos = NUMCOL-1;
                     box (0,TOPY,MAXX,LOWY,bg);
                     redraw_music(0,NUMCOL-1); redraw_info(); draw_col();
                   }
                   else if (curpos < NUMCOL-1) {
		     undraw_col();
		     curpos = NUMCOL-1;
		     draw_col();
		     redraw_info();
		   }
		   break;
      /* go to the first page */
      case 0x0449: exit_editmode();
		   if (curpage > 0) {
                     mappedpage = curpage = 0;
                     if (EMSmap (EMShandle[0],0,mappedpage) == -1) {
                       VESAQuit();
                       if (_sound) FM_Reset();
                       EMSfree (EMShandle[0]);
                       EMSfree (EMShandle[1]);
                       unloadkeyboardISR();
                       unloadErrorHandler();
                       fprintf (stderr, "compozer: EMS mapping error\n");
                       return 1;
                     }
                     page = song;
		     box (0,TOPY,MAXX,LOWY,bg);
		     redraw_music(0,NUMCOL-1); redraw_info(); draw_col();
		   }
		   break;
      /* go to the last page */
      case 0x0451: exit_editmode();
                   if (curpage < hipage) {
                     curpage = hipage; mappedpage = hipage/PAGES;
                     if (EMSmap (EMShandle[0],0,mappedpage) == -1) {
		       VESAQuit();
                       if (_sound) FM_Reset();
		       EMSfree (EMShandle[0]);
		       EMSfree (EMShandle[1]);
		       unloadkeyboardISR();
                       unloadErrorHandler();
		       fprintf (stderr, "compozer: EMS mapping error\n");
		       return 1;
		     }
                     page = NUMVOICES*NUMCOL*(hipage%PAGES) + song;
                     box (0,TOPY,MAXX,LOWY,bg);
                     redraw_music(0,NUMCOL-1); redraw_info(); draw_col();
                   }
		   break;
      /* go to next non-empty column */
      case 0x000F: exit_editmode();
                   undraw_col();
                   for (k = curpos+1; k < NUMCOL-1; k++)
                     for (j = 0; j < NUMVOICES; j++)
                       if ((page[k*NUMVOICES+j]&0xE0000000l)!=0l) goto _TabEnd;
_TabEnd:
                   curpos = (k>=NUMCOL)?(NUMCOL-1):(k);
                   draw_col();
                   redraw_info();
                   break;
      /* go to previous non-empty column */
      case 0x040F: exit_editmode();
                   undraw_col();
                   for (k = curpos-1; k > 0; k--)
                     for (j = 0; j < NUMVOICES; j++)
                       if ((page[k*NUMVOICES+j]&0xE0000000l)!=0l) goto _CtrlTabEnd;
_CtrlTabEnd:
                   curpos = (k<0)?(0):(k);
                   draw_col();
                   redraw_info();
                   break;
      /* go to previous non-empty column for current voice */
      case 0x044B: exit_editmode();
                   undraw_col();
                   for (k = curpos-1; k > 0; k--)
                     if (!(isblank(curvoice,k))) goto _CtrlLeftEnd;
_CtrlLeftEnd:
                   curpos = (k<0)?(0):(k);
                   draw_col();
                   redraw_info();
                   break;
      /* go to next non-empty column for current voice */
      case 0x044D: exit_editmode();
                   undraw_col();
                   for (k = curpos+1; k < NUMCOL-1; k++)
                     if (!(isblank(curvoice,k))) goto _CtrlRightEnd;
_CtrlRightEnd:
                   curpos = (k>=NUMCOL)?(NUMCOL-1):(k);
                   draw_col();
                   redraw_info();
                   break;
      /* change display position of screen element or change pitch of a note
         or accidental (move it up or down) */
      case 0x0048: k = -1;
      case 0x0050: if (ch == 0x0050) k = 1;
                   exit_editmode(); changed = -1;
                   if (!(isblank(curvoice,curpos)) &&
                      (page[i=(curpos*NUMVOICES)]&0xE0000000l) != 0x80000000l) {
                     undraw_col();
                     drawcontrol = -1;
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     drawcontrol = 0;
                     if ((l=page[i+curvoice]&0xE0000000l) == 0x20000000l) {
                       if ((t = getnexttie (curvoice, curpos)) != -1)
                         undraw_cvoice (t, curvoice, curstaffy);
                       if ((page[i+curvoice]&0x00000700l) > 0x00000200l) {
                         n = curpos;
                         while ((n=getprevbeam(curvoice,n)) != -1)
                           undraw_cvoice (n, curvoice, curstaffy);
                         n = curpos;
                         while ((n=getnextbeam(curvoice,n)) != -1)
                           if (n != t) undraw_cvoice (n, curvoice, curstaffy);
                         if ((j = (page[i+=curvoice]&0x7F)-k) == 71)
                           j = 0;
                         else if (j > 70)
                           j = 70;
                         page[i] = (page[i]&0xFFFFFF80l) | j;
                         n = curpos;
                         while ((n=getprevbeam(curvoice,n)) != -1)
                           draw_cvoice (n, curvoice, curstaffy, fg);
                         n = curpos;
                         while ((n=getnextbeam(curvoice,n)) != -1) {
                           if (n == t) t = -1;
                           draw_cvoice (n, curvoice, curstaffy, fg);
                         }
                       }
                       else {
                         if ((j = (page[i+=curvoice]&0x7F)-k) == 71)
                           j = 0;
                         else if (j > 70)
                           j = 70;
                         page[i] = (page[i]&0xFFFFFF80l) | j;
                       }
                       if (t != -1)
                         draw_cvoice (t, curvoice, curstaffy, fg);
                       redraw_info();
                     }
                     else if ((page[i]&0xE1000800) == 0x40000800l) {
                       for (j=0;j<NUMVOICES;j++)
                         if (j!=curvoice)
                           undraw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1]);
                       if ((j = (page[i]&0x7F)-k) == 71)
                         j = 0;
                       else if (j > 70)
                         j = 70;
                       page[i] = (page[i]&0xFFFFFF80l) | j;
                       for (j=0;j<NUMVOICES;j++)
                         if (j!=curvoice)
                           draw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1], mg);
                       for (j=1;j<=numstaves;j++)
                         drawstaff (j,curpos,curpos);
                     }
                     else if ((page[i+curvoice]&0xE1000800) == 0x40000000l) {
                       if ((j = (page[i+=curvoice]&0x7F)-k) == 71)
                         j = 0;
                       else if (j > 70)
                         j = 70;
                       page[i] = (page[i]&0xFFFFFF80l) | j;
                     }
                     else if (l != 0l) {
                       i += curvoice;
                       page[i] = (page[i]&0xFFFFFF00l) | ((page[i]+k)&0xFF);
                     }
/*                     else if ((page[i]&0xE00E8000l) == 0xE0088000l) {
                       page[i] = (page[i]&0xFFFFFF00l) | ((page[i]+k)&0xFF);
                     }*/
                     else if ((page[i]&0xE1000800l) == 0x40000800l) {
                       for (j=0;j<NUMVOICES;j++)
                         if (j!=curvoice)
                           undraw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1]);
                       if ((j = (page[i]&0x7F)-k) == 71)
                         j = 0;
                       else if (j > 70)
                         j = 70;
                       page[i] = (page[i]&0xFFFFFF80l) | j;
                       for (j=0;j<NUMVOICES;j++)
                         if (j!=curvoice)
                           draw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1], mg);
                       for (j=1;j<=numstaves;j++)
                         drawstaff (j,curpos,curpos);
                     }
                     else
                       page[i] = (page[i]&0xFFFFFF00l) | ((page[i]+k)&0xFF);
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   break;
      /* enter a note or toggle between note and rest */
      case 0x0039: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     l = getprevnote (curvoice, curpos, 0);
                     if (l == 0xFFFFFFFFl)
                       l = 0x223l;
                     else if ((l&0xE1000000l)==0x40000000l)
                       l = 0x200l | (l&0x7F);
                     l = 0x20000000l | (l&0x4077F);
                     if ((k = getnextbeam(curvoice,curpos)) != -1)
                       l |= 0x80000l;
                     undraw_col();
                     n = (k==-1)?(curpos):(k);
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);
                     page[curpos*NUMVOICES+curvoice] = l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     n = curpos;
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);

                     if (page[i]&0x80) {
                       l = getprevnote (curvoice, curpos, 0);
                       page[i] &= 0xFFFFFF00l;
                       if (l == 0xFFFFFFFFl)
                         page[i] += 0x23l;
                       else
                         page[i] += l&0xFF;
                     }
                     else {
                       l = getprevrest (curvoice, curpos);
                       page[i] &= 0xFFFFFF00l;
                       if (l == 0xFFFFFFFFl)
                         page[i] += 0xA3l;
                       else
                         page[i] += l&0xFF;
                     }

                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   redraw_info();
                   break;
      /* enter a note or toggle between dotted or not */
      case 0x0002: j=0x000; goto _enterNote;
      case 0x0003: j=0x100; goto _enterNote;
      case 0x0004: j=0x500; goto _enterNote;
      case 0x0005: j=0x200; goto _enterNote;
      case 0x0007: j=0x400; goto _enterNote;
      case 0x0008: j=0x600; goto _enterNote;
      case 0x0009: j=0x300;
_enterNote:
        exit_editmode(); changed = -1;
        if (isblank(curvoice,curpos)) {
          l = getprevnote (curvoice, curpos, 0);
          if (l == 0xFFFFFFFFl)
            l = 0x23l | j;
          else
            l = (l&0x4007F) | j;
          l = 0x20000000l + l;
          if ((k = getnextbeam(curvoice,curpos)) != -1)
            l |= 0x80000l;
          undraw_col();
          n = (k==-1)?(curpos):(k);
          while ((n=getprevbeam(curvoice,n)) != -1)
            undraw_cvoice (n, curvoice, curstaffy);
          n = curpos;
          while ((n=getnextbeam(curvoice,n)) != -1)
            undraw_cvoice (n, curvoice, curstaffy);
          page[curpos*NUMVOICES+curvoice] = l;
          draw_cvoice (curpos, curvoice, curstaffy, fg);
          n = curpos;
          while ((n=getprevbeam(curvoice,n)) != -1)
            draw_cvoice (n, curvoice, curstaffy, fg);
          n = curpos;
          while ((n=getnextbeam(curvoice,n)) != -1)
            draw_cvoice (n, curvoice, curstaffy, fg);
          draw_col();
          redraw_info();
        }
        else if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
          undraw_col();
          undraw_cvoice (curpos, curvoice, curstaffy);
          n = curpos;
          while ((n=getprevbeam(curvoice,n)) != -1)
            undraw_cvoice (n, curvoice, curstaffy);
          n = curpos;
          while ((n=getnextbeam(curvoice,n)) != -1)
            undraw_cvoice (n, curvoice, curstaffy);
          if ((page[i]&0x700l) != j) page[i] = (page[i]&0xFFFFF8FFl)|j;
          else if (page[i]&0x800) page[i] &= 0xFFFFF7FFl;
          else page[i] |= 0x800l;
          draw_cvoice (curpos, curvoice, curstaffy, fg);
          n = curpos;
          while ((n=getprevbeam(curvoice,n)) != -1)
            draw_cvoice (n, curvoice, curstaffy, fg);
          n = curpos;
          while ((n=getnextbeam(curvoice,n)) != -1)
            draw_cvoice (n, curvoice, curstaffy, fg);
          draw_col();
        }
        else
          goto _endingNumber;
        break;
      /* toggle triplet flag */
      case 0x0014: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if (page[i]&0x1000) page[i] &= 0xFFFFEFFFl;
                     else page[i] |= 0x1000l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* toggle staccato flag */
      case 0x0034: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if (page[i]&0x8000) page[i] &= 0xFFFF7FFFl;
                     else page[i] |= 0x8000l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* toggle accent flag */
      case 0x0134: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if (page[i]&0x2000) page[i] &= 0xFFFFDFFFl;
                     else page[i] |= 0x2000l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else
                     goto _enterDimm;
                   break;
      /* toggle beam flag */
      case 0x004E:
      case 0x010D: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     if ((page[i]&0x00000700l) > 0x00000200l) {
                       undraw_col();
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       l = page[i];
                       page[i] |= 0x80000l;
                       n = getprevbeam(curvoice,curpos);
                       if (n != -1) {
                         page[i] = l;
                         undraw_cvoice (n, curvoice, curstaffy);
                         while ((n=getprevbeam(curvoice,n)) != -1)
                           undraw_cvoice (n, curvoice, curstaffy);
                       }
                       n = curpos;
                       while ((n=getnextbeam(curvoice,n)) != -1)
                         undraw_cvoice (n, curvoice, curstaffy);
                       if (page[i]&0x80000) page[i] &= 0xFFF7FFFFl;
                       else page[i] |= 0x80000l;
                       draw_cvoice (curpos, curvoice, curstaffy, fg);
                       l = page[i];
                       page[i] |= 0x80000l;
                       n = getprevbeam(curvoice,curpos);
                       if (n != -1) {
                         page[i] = l;
                         draw_cvoice (n, curvoice, curstaffy, fg);
                         while ((n=getprevbeam(curvoice,n)) != -1)
                           draw_cvoice (n, curvoice, curstaffy, fg);
                       }
                       n = curpos;
                       while ((n=getnextbeam(curvoice,n)) != -1)
                         draw_cvoice (n, curvoice, curstaffy, fg);
                       draw_col();
                     }
                   }
                   else
                     goto _numRepeatInc;
                   break;
      /* toggle tie flag */
      case 0x004A:
      case 0x000C: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if (page[i]&0x4000) page[i] &= 0xFFFFBFFFl;
                     else page[i] |= 0x4000l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else
                     goto _numRepeatDec;
                   break;
      /* toggle downstem flag */
      case 0x0015: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     n = curpos;
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);
                     if (page[i]&0x40000) page[i] &= 0xFFFBFFFFl;
                     else page[i] |= 0x40000l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* toggle flagless flags */
      case 0x0016: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     n = curpos;
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       undraw_cvoice (n, curvoice, curstaffy);
                     if (page[i]&0x20000) page[i] &= 0xFFFDFFFFl;
                     else page[i] |= 0x20000l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getprevbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     n = curpos;
                     while ((n=getnextbeam(curvoice,n)) != -1)
                       draw_cvoice (n, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else
                     goto _setPercussion;
                   break;
      /* toggle backwards flag */
      case 0x0116: exit_editmode(); changed = -1;
                   if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if (page[i]&0x10000) page[i] &= 0xFFFEFFFFl;
                     else page[i] |= 0x10000l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else
                     goto _setNoPercussion;
                   break;
      /* enter a fermata */
_enterFermata:
/* case 0x010A: */ exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     gethold();
                     undraw_col();
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a sharp */
      case 0x0023: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     l = getprevnote (curvoice, curpos, -1);
                     if (l == 0xFFFFFFFFl) {
                       l = 10;
                       switch (staffinfo[(curstaff-1)<<1]) {
                         case 0:
                         case 1: l += 35; break;
                         case 2:
                         case 3:
                         case 4:
                         case 5:
                         case 6: l += 28; break;
                         case 7:
                         case 8: l += 21;
                       }
                     }
                     else if ((l&0xE1000800l)==0x40000800l) {
                       if (l&0x200l)
                         l = (l&0x7F)+4;
                       l %= 7;
                       if (l < 5) l += 7;
                       switch (staffinfo[(curstaff-1)<<1]) {
                         case 0:
                         case 1: l += 35; break;
                         case 2:
                         case 3:
                         case 4:
                         case 5:
                         case 6: l += 28; break;
                         case 7:
                         case 8: l += 21;
                       }
                     }
                     l = 0x40000200l | (l&0x7F);
                     undraw_col();
                     page[curpos*NUMVOICES+curvoice] = l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else if ((page[i=curpos*NUMVOICES+curvoice]&0xE1000800l) == 0x40000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if (page[i]&0x200) {
                       if (page[i]&0x100)
                         page[i] &= 0xFFFFFEFFl;
                       else
                         page[i] |= 0x100l;
                     }
                     else page[i] = (page[i]&0xFFFFF8FFl) | 0x200;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a flat */
      case 0x0030: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     l = getprevnote (curvoice, curpos, -1);
                     if (l == 0xFFFFFFFFl) {
                       l = 6;
                       switch (staffinfo[(curstaff-1)<<1]) {
                         case 0:
                         case 1: l += 35; break;
                         case 2:
                         case 3:
                         case 4:
                         case 5:
                         case 6: l += 28; break;
                         case 7:
                         case 8: l += 21;
                       }
                     }
                     else if ((l&0xE1000800l)==0x40000800l) {
                       if (!(l&0x600l))
                         l = (l&0x7F)+3;
                       l %= 7;
                       if (l < 3) l += 7;
                       switch (staffinfo[(curstaff-1)<<1]) {
                         case 0:
                         case 1: l += 35; break;
                         case 2:
                         case 3:
                         case 4:
                         case 5:
                         case 6: l += 28; break;
                         case 7:
                         case 8: l += 21;
                       }
                     }
                     l = 0x40000000l | (l&0x7F);
                     undraw_col();
                     page[curpos*NUMVOICES+curvoice] = l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else if ((page[i=curpos*NUMVOICES+curvoice]&0xE1000800l) == 0x40000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if (!(page[i]&0x600)) {
                       if (page[i]&0x100)
                         page[i] &= 0xFFFFFEFFl;
                       else
                         page[i] |= 0x100l;
                     }
                     else page[i] = page[i]&0xFFFFF8FFl;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else
                     goto _endingNumber;
                   break;
      /* enter a natural */
      case 0x0031: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     l = getprevnote (curvoice, curpos, -1);
                     if (l == 0xFFFFFFFFl) {
                       l = 10;
                       switch (staffinfo[(curstaff-1)<<1]) {
                         case 0:
                         case 1: l += 35; break;
                         case 2:
                         case 3:
                         case 4:
                         case 5:
                         case 6: l += 28; break;
                         case 7:
                         case 8: l += 21;
                       }
                     }
                     else if ((l&0xE1000800l)==0x40000800l) {
                       l = (l&0x7F)%7;
                       if (l < 4) l += 7;
                       switch (staffinfo[(curstaff-1)<<1]) {
                         case 0:
                         case 1: l += 35; break;
                         case 2:
                         case 3:
                         case 4:
                         case 5:
                         case 6: l += 28; break;
                         case 7:
                         case 8: l += 21;
                       }
                     }
                     l = 0x40000400l | (l&0x7F);
                     undraw_col();
                     page[curpos*NUMVOICES+curvoice] = l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else if ((page[i=curpos*NUMVOICES+curvoice]&0xE1000800l) == 0x40000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     page[i] = (page[i]&0xFFFFF8FFl) | 0x400;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* confirm key signature */
      case 0x0025: exit_editmode(); changed = -1;
                   if ((page[(k=curpos*NUMVOICES)+curvoice]&0xE1000800l) == 0x40000000l) {
                     for (i=j=0; i<NUMVOICES; i++)
                       if (i!=curvoice && (page[k+i]&0xE0000000l)!=0x0l)
                         j++;
                     if (j==0) {
                       undraw_col();
                       undraw_cvoice(curpos, curvoice, curstaffy);
                       if (curvoice == 0)
                         page[k] |= 0x800l;
                       else {
                         l = page[k+curvoice];
                         page[k+curvoice] = 0x0l;
                         page[k] = l | 0x800l;
                       }
                       redraw_music (curpos, curpos);
                       draw_col();
                     }
                   }
                   else if ((page[k]&0xE1000800l) == 0x40000800l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     for (i=0; i<NUMVOICES; i++)
                       if (i!=curvoice)
                         undraw_nvoice (curpos, i, voiceinfo[i]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[i]<<1)-1]);
                     l = page[k]&0xFFFFF700l;
                     j = (page[k]&0x7F)%7;
                     page[k] = 0x0l;
                     switch ((l&0x700)>>8) {
                       case 0: case 1:
                         if (j < 3) j += 7; break;
                       case 2: case 3:
                         if (j < 5) j += 7; break;
                       default:if (j < 4) j += 7;
                     }
                     switch (staffinfo[(curstaff-1)<<1]) {
                       case 0:
                       case 1: j += 35; break;
                       case 2:
                       case 3:
                       case 4:
                       case 5:
                       case 6: j += 28; break;
                       case 7:
                       case 8: j += 21;
                     }
                     page[k+curvoice] = l + j;
                     redraw_music (curpos, curpos);
                     draw_col();
                   }
                   break;
      /* enter a forte */
      case 0x0021: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     undraw_col();
                     page[curpos*NUMVOICES+curvoice] = 0x60001600l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l) == 0x60000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if ((l=page[i]&0x0000FF00l) == 0x00001600l ||
                         l == 0x00001A00l)
                       page[i] += 0x400l;
                     else
                       page[i] = 0x60001600l + (page[i]&0xff);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else
                     goto _endingNumber;
                   break;
      /* enter a piano */
      case 0x0019: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     undraw_col();
                     page[curpos*NUMVOICES+curvoice] = 0x60000A00l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l) == 0x60000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if ((l=page[i]&0x0000FF00l) == 0x00000A00l ||
                         l == 0x00000600l)
                       page[i] -= 0x400l;
                     else
                       page[i] = 0x60000A00l + (page[i]&0xff);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a mezzo-forte */
      case 0x0032: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos)) {
                     undraw_col();
                     page[curpos*NUMVOICES+curvoice] = 0x60001200l;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   else if ((page[i=curpos*NUMVOICES+curvoice]&0xE0000000l) == 0x60000000l) {
                     undraw_col();
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     if ((page[i]&0x0000FF00l) == 0x00001200l)
                       page[i] = 0x60000E00l + (page[i]&0xff);
                     else
                       page[i] = 0x60001200l + (page[i]&0xff);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a crescendo */
      case 0x0133: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE0000000l) == 0x60000000l) {
                     undraw_col();
                     if (l==0x60000000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getdelta(0,1);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a decrescendo */
_enterDimm:
/* case 0x0134: */ if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE0000000l) == 0x60000000l) {
                     undraw_col();
                     if (l==0x60000000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getdelta(0,0);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a time signature */
_enterTimeSig:
/* case 0x0104: */ exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j==0) {
                     gettimesig();
                     undraw_col();
                     redraw_music (curpos, curpos);
                     draw_col();
                   }
                   break;
      /* enter a "Percussion" marker */
_setPercussion:
/*      case 0x0016: */ for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                          if ((page[k+i]&0xE0000000l)!=0x0l)
                            j++;
                        if (j==0) {
                          undraw_col();
                          page[curpos*NUMVOICES] = 0xA0000F00l;
                          drawcontrol = -1;
                          draw_cvoice (curpos, curvoice, curstaffy, fg);
                          drawcontrol = 0;
                          draw_col();
                        }
                        break;
_setNoPercussion:
      /* enter a "No Percussion" marker */
/*      case 0x0116: */ for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                          if ((page[k+i]&0xE0000000l)!=0x0l)
                            j++;
                        if (j==0) {
                          undraw_col();
                          page[curpos*NUMVOICES] = 0xA0000700l;
                          drawcontrol = -1;
                          draw_cvoice (curpos, curvoice, curstaffy, fg);
                          drawcontrol = 0;
                          draw_col();
                        }
                        break;
      /* enter a tempo */
      case 0x0018: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 ||
                      (l=page[k]&0xE00E0000l) == 0xE0020000l ||
                      l == 0xE0040000l || l == 0xE0060000l) {
                     undraw_col();
                     if ((l&0xE0000000l)==0xE0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     gettempo();
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   break;
      /* enter an accelerando */
      case 0x001E: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 ||
                      (l=page[k]&0xE00E0000l) == 0xE0020000l ||
                      l == 0xE0040000l || l == 0xE0060000l) {
                     undraw_col();
                     if ((l&0xE0000000l)==0xE0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     getdelta(1,1);
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   else
                     goto _endingNumber;
                   break;
      /* enter a ritardando */
      case 0x0013: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 ||
                      (l=page[k]&0xE00E0000l) == 0xE0020000l ||
                      l == 0xE0040000l || l == 0xE0060000l) {
                     undraw_col();
                     if ((l&0xE0000000l)==0xE0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     getdelta(1,0);
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   break;
      /* enter a bar or change bar type */
      case 0x001C: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j==0) {
                     page[k] = 0x80000000l;
                     undraw_col();
                     redraw_music (curpos,curpos);
                     draw_col();
                   }
                   else if ((page[k]&0xE0000000l)==0x80000000l) {
                     undraw_col();
                     drawcontrol = -1;
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     drawcontrol = 0;
                     drawlines = -1;
                     for (j=0;j<NUMVOICES;j++)
                       if (j!=curvoice) {
                         undraw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1]);
                         drawlines = 0;
                       }
                     i=((page[k]&0x7)+1)%6;
                     page[k]=(page[k]&0xFFFFFFF0) + i;
                     redraw_music (curpos,curpos);
                     draw_col();
                   }
                   break;
      /* add one to number of repeats for end-repeat type bar */
_numRepeatInc:
/* case 0x004E:
   case 0x010D: */ if ((page[curpos*NUMVOICES]&0xE0000007l)==0x80000003l) {
                     undraw_col();
                     drawcontrol = -1;
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     drawcontrol = 0;
                     drawlines = -1;
                     for (j=0;j<NUMVOICES;j++)
                       if (j!=curvoice) {
                         undraw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1]);
                         drawlines = 0;
                       }
                     i=((page[k=curpos*NUMVOICES]>>4)+1)&0xF;
                     page[k]=(page[k]&0xFFFFFF0F) + (i<<4);
                     redraw_music (curpos,curpos);
                     draw_col();
                   }
                   break;
      /* subtract one from number of repeats for end-repeat type bar */
_numRepeatDec:
/* case 0x004A:
   case 0x000C: */ if ((page[curpos*NUMVOICES]&0xE0000007l)==0x80000003l) {
                     undraw_col();
                     drawcontrol = -1;
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     drawcontrol = 0;
                     drawlines = -1;
                     for (j=0;j<NUMVOICES;j++)
                       if (j!=curvoice) {
                         undraw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1]);
                         drawlines = 0;
                       }
                     i=((page[k=curpos*NUMVOICES]>>4)-1)&0xF;
                     page[k]=(page[k]&0xFFFFFF0F) + (i<<4);
                     redraw_music (curpos,curpos);
                     draw_col();
                   }
                   break;
      /* enter program change */
      case 0x0017: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos))
                     getinstrument(-1);
                   else if ((page[k=(curpos*NUMVOICES+curvoice)]&0xE00E0000l)==0xE0080000l)
                     getinstrument((page[k]>>8)&0xff);
                   break;
      /* enter drumset change */
      case 0x0120: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                         j++;
                   if (j==0)
                     getdrumset(0);
                   else if ((page[k]&0xE00E8000l)==0xE0088000l)
                     getdrumset((page[k]>>8)&0x7F);
                   break;
      /* enter a pan level */
      case 0x0119: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE00E0000l) == 0xE00A0000l ||
                      l == 0xE00E0000l) {
                     undraw_col();
                     if (l==0xE00A0000l||l==0xE00E0000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getlev(-1);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a volume level */
      case 0x0126: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE0000000l) == 0x60000000l) {
                     undraw_col();
                     if (l==0x60000000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getlev(0);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a Da Capo or Dal Segno */
      case 0x0020: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 || (l=page[k]&0xE0000000l)==0xA0000000l) {
                     undraw_col();
                     if (l==0xA0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     if ((page[k]&0xE0000F00l) == 0xA0000000l)
                       page[k] = 0xA0000100l;
                     else
                       page[k] = 0xA0000000l;
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   else
                     goto _endingNumber;
                   break;
      /* enter a Segno */
      case 0x001F: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 || (l=page[k]&0xE0000000l)==0xA0000000l) {
                     undraw_col();
                     if (l==0xA0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     page[k] = 0xA0000200l;
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   break;
      /* enter a stereo fade */
      case 0x011F: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE00E0000l) == 0xE00A0000l ||
                      l == 0xE00E0000l) {
                     undraw_col();
                     if (l==0xE00A0000l||l==0xE00E0000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getfade();
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a keyboard pan */
      case 0x0125: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE1000000l) == 0x41000000l) {
                     undraw_col();
                     if (l==0x61000000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getkeypan();
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a Pitch Wheel Position */
      case 0x0011: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE1000000l) == 0x61000000l) {
                     undraw_col();
                     if (l==0x61000000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getwheel(0);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a Pitch Wheel Roll */
      case 0x0111: exit_editmode(); changed = -1;
                   if (isblank(curvoice,curpos) ||
                      (l=page[curpos*NUMVOICES+curvoice]&0xE1000000l) == 0x61000000l) {
                     undraw_col();
                     if (l==0x61000000l)
                       undraw_cvoice (curpos, curvoice, curstaffy);
                     getwheel(-1);
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     draw_col();
                   }
                   break;
      /* enter a Coda */
      case 0x002E: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 || (l=page[k]&0xE0000000l)==0xA0000000l) {
                     undraw_col();
                     if (l==0xA0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     page[k] = 0xA0000300l;
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   else
                      goto _endingNumber;
                   break;
      /* enter a Fine */
      case 0x0121: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 || (l=page[k]&0xE0000000l)==0xA0000000l) {
                     undraw_col();
                     if (l==0xA0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     page[k] = 0xA0000400l;
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   break;
      /* enter an al Coda or al Fine */
      case 0x0026: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j == 0 || (l=page[k]&0xE0000000l)==0xA0000000l) {
                     undraw_col();
                     if (l==0xA0000000l) {
                       drawcontrol = -1;
                       undraw_cvoice (curpos, curvoice, curstaffy);
                       drawcontrol = 0;
                     }
                     if ((page[k]&0xE0000F00l) == 0xA0000500l)
                       page[k] = 0xA0000600l;
                     else
                       page[k] = 0xA0000500l;
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   break;
      /* enter an ending indicator */
      case 0x002F: exit_editmode(); changed = -1;
                   for (i=j=0,k=curpos*NUMVOICES; i<NUMVOICES; i++)
                     if ((page[k+i]&0xE0000000l)!=0x0l)
                       j++;
                   if (j==0) {
                     page[k] = 0xC0000100l;
                     undraw_col();
                     drawcontrol = -1;
                     draw_cvoice (curpos, curvoice, curstaffy, fg);
                     drawcontrol = 0;
                     draw_col();
                   }
                   break;
      /* change ending number */
_endingNumber:
/*    case 0x0002: case 0x0003: case 0x0004: case 0x0005: */ case 0x0006:
/*    case 0x0007: case 0x0008: case 0x0009: */ case 0x000A: case 0x000B:
/*    case 0x001E: case 0x0030: case 0x002E: case 0x0020: */ case 0x0012:
/*    case 0x0021: */
        exit_editmode(); changed = -1;
        if ((page[curpos*NUMVOICES]&0xE0000000l)==0xC0000000l) {
          if (ch == 0x001E) i = 10;
          else if (ch == 0x0030) i = 11;
          else if (ch == 0x002E) i = 12;
          else if (ch == 0x0020) i = 13;
          else if (ch == 0x0012) i = 14;
          else if (ch == 0x0021) i = 15;
          else i = ch-2;
          undraw_col();
          drawcontrol = -1;
          undraw_cvoice (curpos, curvoice, curstaffy);
          drawcontrol = 0;
          for (j=0;j<NUMVOICES;j++)
            if (j!=curvoice)
              undraw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1]);
          l = 0x100l << i;
          if (page[curpos*NUMVOICES]&l)
            page[curpos*NUMVOICES] &= ~l;
          else
            page[curpos*NUMVOICES] |= l;
          redraw_music (curpos,curpos);
          draw_col();
        }
        break;
      /* delete a voice or column */
      case 0x0053: if (editmode)
                     clear();
                   else if (!(isblank(curvoice,curpos))) {
                     changed = -1;
                     undraw_col();
                     drawcontrol = -1;
                     undraw_cvoice (curpos, curvoice, curstaffy);
                     drawcontrol = 0;
                     if (iscontrol(curpos)) {
                       drawlines = -1;
                       for (j=0;j<NUMVOICES;j++)
                         if (j!=curvoice) {
                           undraw_nvoice (curpos, j, voiceinfo[j]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[j]<<1)-1]);
                           drawlines = 0;
                         }
                       for (j=1;j<=numstaves;j++)
                         drawstaff (j,curpos,curpos);
                     }
                     if ((page[curpos*NUMVOICES+curvoice]&0xE0000000l)==0x20000000l) {
                       if ((t = getnexttie (curvoice, curpos)) != -1)
                         undraw_cvoice (t, curvoice, curstaffy);
                       n = curpos; k = getprevbeam(curvoice,curpos);
                       while ((n=getprevbeam(curvoice,n)) != -1)
                         undraw_cvoice (n, curvoice, curstaffy);
                       n = curpos; if (k==-1) k = getnextbeam(curvoice,curpos);
                       while ((n=getnextbeam(curvoice,n)) != -1)
                         if (n != t) undraw_cvoice (n, curvoice, curstaffy);
                       if (k==-1) k = curpos;
                       page[curpos*NUMVOICES+curvoice] = 0l;
                       if (k != curpos) draw_cvoice (k, curvoice, curstaffy, fg);
                       n = k;
                       while ((n=getprevbeam(curvoice,n)) != -1)
                         draw_cvoice (n, curvoice, curstaffy, fg);
                       n = k;
                       while ((n=getnextbeam(curvoice,n)) != -1) {
                         if (n == t) t = -1;
                         draw_cvoice (n, curvoice, curstaffy, fg);
                       }
                       if (t != -1)
                         draw_cvoice (t, curvoice, curstaffy, fg);
                     }
                     else if ((page[curpos*NUMVOICES+curvoice]&0xE0000000l)!=0l)
                       page[curpos*NUMVOICES+curvoice] = 0l;
                     else
                       page[curpos*NUMVOICES] = 0l;
                     draw_col();
                   }
                   else {
                     changed = -1;
                     for (i=j=0; i<NUMVOICES; i++)
                       if ((page[curpos*NUMVOICES+i]&0xE0000000l)!=0l)
                         j++;
                     if (j==0) {
                       undraw_col();
                       drawcontrol = -1;
                       undrawrow_cvoice (curvoice,curpos,NUMCOL-1);
                       drawcontrol = 0;
                       drawlines = -1;
                       for (i = 0; i<NUMVOICES; i++)
                         if (i != curvoice) {
                           undrawrow_nvoice (i,curpos,NUMCOL-1);
                           drawlines = 0;
                         }
                       for (i = curpos+1; i<NUMCOL; i++)
                         for (j = 0; j<NUMVOICES; j++)
                           page[(i-1)*NUMVOICES+j] = page[i*NUMVOICES+j];
                       for (j = 0; j<NUMVOICES; j++)
                         page[(i-1)*NUMVOICES+j] = 0l;
                       redraw_music (curpos, NUMCOL-1);
                       draw_col();
                     }
                   }
                   redraw_info();
                   break;
      /* insert a column */
      case 0x0052: exit_editmode(); changed = -1;
                   for (i=j=0; i<NUMVOICES; i++)
                     if ((page[(NUMCOL-1)*NUMVOICES+i]&0xE0000000l)!=0l)
                       j++;
                   if (j==0) {
                     undraw_col();
                     for (n = curpos, i = 0; i<NUMVOICES; i++) {
                       k = getprevbeam (i,curpos);
                       if (k == -1) {
                         k = getnextbeam (i,curpos);
                         if (k != -1) k = getprevbeam (i,k);
                       }
                       if (k != -1 && k < n) n = k;
                     }
                     drawcontrol = -1;
                     undrawrow_cvoice (curvoice,n,NUMCOL-1);
                     drawcontrol = 0;
                     drawlines = -1;
                     for (i = 0; i<NUMVOICES; i++)
                       if (i != curvoice) {
                         undrawrow_nvoice (i,n,NUMCOL-1);
                         drawlines = 0;
                       }
                     for (i = NUMCOL-1; i>curpos; i--)
                       for (j = 0; j<NUMVOICES; j++)
                         page[i*NUMVOICES+j] = page[(i-1)*NUMVOICES+j];
                     for (j = 0; j<NUMVOICES; j++)
                       page[i*NUMVOICES+j] = 0l;
                     redraw_music (n, NUMCOL-1);
                     draw_col();
                     redraw_info();
                   }
_endswitch:
    }
  }
  VESAQuit();
  if (_sound) FM_Reset();
  unloadkeyboardISR();
  unloadErrorHandler();
  EMSfree (EMShandle[0]);
  EMSfree (EMShandle[1]);
  save_config();
  textcolor (LIGHTCYAN); cprintf ("MIDI Compozer v"VERSION"\n\r");
  textcolor (CYAN); cprintf ("-------------------\n\r");
  textcolor (WHITE); cprintf ("SESSION ENDED\n\r\n");
  textcolor (LIGHTGRAY); cprintf ("If you like this software please feel free to check out other ");
  textcolor (LIGHTRED); cprintf ("Apriori\n\rEnterprises");
  textcolor (LIGHTGRAY); cprintf (" stuff at ");
  textcolor (WHITE); cprintf ("http://apriori.webjump.com/\n\r");
  textcolor (LIGHTGRAY); cprintf ("\n");
  return 0;
}
