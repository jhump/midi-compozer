/* FILE: playback.c
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains source code to playback AEM songs on an OPL2/3
 * synth as well as exporting it to MIDI file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <time.h>
#include <string.h>
#include "fm.h"
#include "ems.h"
#include "gr.h"
#include "keys.h"
#include "findnote.h"
#include "getfile.h"

#define NUMVOICES 	10
#define NUMCOL 		75
#define PAGES		(4096/(NUMCOL*NUMVOICES))
#define TOPY 		32
#define LOWY 		445
#define HEIGHT 		(LOWY-TOPY+1)

extern int _sound, opl3, gmidi, curpage, editmode, mappedpage, mute[],
           EMShandle[], curvoice, numstaves, tempo[], timesig, upd, updk,
           hipage;
extern unsigned long *song, *page;
extern char MIDINote[], composer[], opus[], title[];
extern unsigned char bg, fg, mg, keyb, wkey, bkey, wkeyp, bkeyp, fgp, mgp,
                     menubg, menufg, midibytes[];
extern signed char voiceinfo[], staffinfo[];
extern struct {
  char *Name;
  char **Instrum;
} Banks[16];

extern void Message (char *str);
extern int YesNo(char *s);
extern void undraw_col(void);
extern void redraw_music(int begin, int end);
extern void redraw_screen (void);
extern void VESAQuit(void);
extern void draw_nvoice(int column, char voice, int staffy, char color);
extern void draw_col(void);
extern void undraw_col(void);
extern char *getinstrumentname (int bank, int inst);

volatile unsigned long count, ticks;
unsigned long elapsed, realtime, spt;
char key[NUMVOICES][7], accident[NUMVOICES][7], kb[121];
int lastnote[NUMVOICES], pan[NUMVOICES], wheel[NUMVOICES], wheeldest[NUMVOICES],
    percussion, keypan[NUMVOICES], keypandata[NUMVOICES];
void interrupt (*oldFMTimerISR)(void);
  /* number of clock ticks per beat */
unsigned npb[16] = {384,192, 96,48,24,12,6,6,
                    576,288,144,72,36,18,9,9},
         audible[NUMVOICES], repeat[NUMVOICES][2], _segno[NUMVOICES][2],
         _coda[NUMVOICES][2], fine[NUMVOICES][2], ending[NUMVOICES],
         repeatnum[NUMVOICES], vol[NUMVOICES], tsig, tmpo, beat;

/****************************************************************************/
/* a routine to update the status bar */
void update_info(void) {
  char buf[80], i, tmp[4], j, k; unsigned long x;
  box (0, 17, MAXX, 30, menubg);
  sprintf (buf, "Voices: ");
  for (i = 0; i < 10; i++)
    if (audible[i]) {
      sprintf (tmp, "%d ", i+1);
      strcat (buf, tmp);
    }
  for (i = strlen(buf); i < 34; i++)
    strcat (buf, " ");
  sprintf (buf+34, "            Current Page: %d", curpage+1);
  outtextxy (16, 20, buf, menufg);
  return;
}


/****************************************************************************/
/* this will update the screen with played back notes, update the keyboard
   at the bottom of the screen, and sound the note through the sound card */
void Key_On (int voice, unsigned page, unsigned col, int note) {
  int color;
  if (col >= NUMCOL || !audible[voice]) return;
  if (upd) {
    /* update the screen */
    if (voice == curvoice) color = fgp;
    else color = mgp;
    if (page == curpage)
      draw_nvoice(col, voice, voiceinfo[voice]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[voice]<<1)-1], color);
  }
  if (updk) {
    /* update keyboard */
    if ((!percussion || voice < 6) && note < 121 && note >= 0) {
      kb[note]++;
      switch (note%12) {
        case 0:  color = 0; goto _onWhitekey;
        case 2:  color = 1; goto _onWhitekey;
        case 4:  color = 2; goto _onWhitekey;
        case 5:  color = 3; goto _onWhitekey;
        case 7:  color = 4; goto _onWhitekey;
        case 9:  color = 5; goto _onWhitekey;
        case 11: color = 6; goto _onWhitekey;
        case 1:  color = 1; goto _onBlackkey;
        case 3:  color = 2; goto _onBlackkey;
        case 6:  color = 4; goto _onBlackkey;
        case 8:  color = 5; goto _onBlackkey;
        case 10: color = 6; goto _onBlackkey;
      }
_onWhitekey:
      color = (note/12)*7 + color;
      box (color*9+1, 467, color*9+8, 478, wkeyp);
      goto _onDone;
_onBlackkey:
      color = (note/12)*7 + color;
      box (color*9-2, 453, color*9+2, 464, bkeyp);
_onDone:
    }
  }
  /* update sound card */
  FM_Key_On (voice,note);
}

/****************************************************************************/
/* this will update the screen with played back notes, update the keyboard
   at the bottom of the screen, and end the note through the sound card */
void Key_Off (int voice, unsigned page, unsigned col, int note) {
  int color;
  if (col >= NUMCOL || !audible[voice]) return;
  if (upd) {
    /* update the screen */
    if (voice == curvoice) color = fg;
    else color = mg;
    if (page == curpage)
      draw_nvoice(col, voice, voiceinfo[voice]*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(voiceinfo[voice]<<1)-1], color);
  }
  if (updk) {
    /* update keyboard */
    if ((!percussion || voice < 6) && note < 121 && note >= 0 && --kb[note] <= 0) {
      if (kb[note] < 0) kb[note] = 0;
      switch (note%12) {
        case 0:  color = 0; goto _offWhitekey;
        case 2:  color = 1; goto _offWhitekey;
        case 4:  color = 2; goto _offWhitekey;
        case 5:  color = 3; goto _offWhitekey;
        case 7:  color = 4; goto _offWhitekey;
        case 9:  color = 5; goto _offWhitekey;
        case 11: color = 6; goto _offWhitekey;
        case 1:  color = 1; goto _offBlackkey;
        case 3:  color = 2; goto _offBlackkey;
        case 6:  color = 4; goto _offBlackkey;
        case 8:  color = 5; goto _offBlackkey;
        case 10: color = 6; goto _offBlackkey;
      }
_offWhitekey:
      color = (note/12)*7 + color;
      box (color*9+1, 467, color*9+8, 478, wkey);
      goto _offDone;
_offBlackkey:
      color = (note/12)*7 + color;
      box (color*9-2, 453, color*9+2, 464, bkey);
_offDone:
    }
  }
  /* update sound card */
  FM_Key_Off (voice);
}

/****************************************************************************/
/* this function sets the timer to the specified tempo and returns a fixed
   point value equal to the number seconds (or fraction thereof) per timer
   tick */
unsigned long settempo(unsigned tmpo, unsigned beat) {
  unsigned long tps = ((long)tmpo*(long)beat)/60l, timeval;
  if (tps == 0) tps = 1;
  else if (tps > 1024) tps = 1024;
  timeval = 1193180l/tps;
  if (timeval > 0xFFFF) timeval = 0xFFFF;
  outportb (0x43, 0x3C);
  outportb (0x40, timeval&0xFF);
  outportb (0x40, timeval>>8);
  ticks = 0;
  return 65536l/tps;
}

/****************************************************************************/
/* the interrupt which updates timer vars */
void interrupt FMTimerISR (void) {
  if (count) count--;
  ticks++;
  outportb (0x20, 0x20);
  return;
}

/****************************************************************************/
/* a routine to install our new timer ISR */
void installFMTimerISR (void) {
  ticks = count = 0;
  oldFMTimerISR = getvect (0x08);
  setvect (0x08, FMTimerISR);
  return;
}

/****************************************************************************/
/* a routine to restore the standard timer ISR */
void unloadFMTimerISR (void) {
  setvect (0x08, oldFMTimerISR);
  return;
}

/****************************************************************************/
/* a routine used by playsong() to update playback vars based on information
   before playback starting point so the song will play correctly starting
   at any point in the song... */
void goback (unsigned pg, int col) {
  int gcoda = 0, gfine = 0, gsegno = 0, grepeat = 0, gending = 0, gperc = 0,
      gkey[7] = {0,0,0,0,0,0,0}, voice, gaccb =  0, j, k,
      gacc[NUMVOICES][7] = {{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0}},
      gtmpo = 0, gtsig = 0, ginstr[NUMVOICES] = {0,0,0,0,0,0,0,0,0,0},
      gvol[NUMVOICES] = {0,0,0,0,0,0,0,0,0,0}, endnum = 0,
      gpan[NUMVOICES] = {0,0,0,0,0,0,0,0,0,0},
      gwheel[NUMVOICES] = {0,0,0,0,0,0,0,0,0,0};
  unsigned long i;
  /* if nothing behind then return */
  if (pg == 0 && col == 0) return;
  /* else set up variables to work with our loops */
  else if (col == 0) { col = NUMCOL; gaccb = -1; }
  else pg++;
  /* loop through until we hit beginning of piece */
  do {
    pg--;
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
    do {
      col--;
      for (voice = 0; voice < NUMVOICES; voice++)
        switch ((i=page[col*NUMVOICES+voice])&0xE0000000l) {
          /* ignore notes */
          case 0x20000000l: break;
          /* accidentals */
          case 0x40000000l:
            j = (i&0x7F)%7;
            /* keyboard pan */
            if (i&0x01000000) {
              if (!(gpan[voice])) {
                gpan[voice] = -1;
                keypan[voice] = -1;
                keypandata[voice] = (i>>8l)&0x7fff;
              }
            }
            /* key signature */
            else if (i&0x800) {
              if (!(gkey[j])) {
                switch ((i>>8)&7) {
                  case 0:
                    gkey[j] = -1;
                    for (k = 0; k < NUMVOICES; k++) {
                      key[k][j] = -1;
                      if (!(gacc[k][j])) {
                        gacc[k][j] = -1;
                        accident[k][j] = -1;
                      }
                    }
                    break;
                  case 1:
                    gkey[j] = -1;
                    for (k = 0; k < NUMVOICES; k++) {
                      key[k][j] = -2;
                      if (!(gacc[k][j])) {
                        gacc[k][j] = -1;
                        accident[k][j] = -2;
                      }
                    }
                    break;
                  case 2:
                    gkey[j] = -1;
                    for (k = 0; k < NUMVOICES; k++) {
                      key[k][j] = 1;
                      if (!(gacc[k][j])) {
                        gacc[k][j] = -1;
                        accident[k][j] = 1;
                      }
                    }
                    break;
                  case 3:
                    gkey[j] = -1;
                    for (k = 0; k < NUMVOICES; k++) {
                      key[k][j] = 2;
                      if (!(gacc[k][j])) {
                        gacc[k][j] = -1;
                        accident[k][j] = 2;
                      }
                    }
                    break;
                  default:
                    gkey[j] = -1;
                    for (k = 0; k < NUMVOICES; k++) {
                      key[k][j] = 0;
                      if (!(gacc[k][j])) {
                        gacc[k][j] = -1;
                        accident[k][j] = 0;
                      }
                    }
                }
              }
            }
            /* accidental */
            else {
              if (!(gacc[voice][j]) && !gaccb) {
                switch ((i>>8)&7) {
                  case 0:
                    gacc[voice][j] = -1;
                    accident[voice][j] = -1;
                    break;
                  case 1:
                    gacc[voice][j] = -1;
                    accident[voice][j] = -2;
                    break;
                  case 2:
                    gacc[voice][j] = -1;
                    accident[voice][j] = 1;
                    break;
                  case 3:
                    gacc[voice][j] = -1;
                    accident[voice][j] = 2;
                    break;
                  default:
                    gacc[voice][j] = -1;
                    accident[voice][j] = 0;
                    break;
                }
              }
            }
            break;
          /* dynamics / pitch wheel */
          case 0x60000000l:
           if (i&0x1000000l) {
             if (!(gwheel[voice]))
               /* wheel roll */
               if (i&0x100l) {
                 signed char tmpw = (i>>16l)&0xFF;
                 if (tmpw > 0)
                   wheel[voice] += ((unsigned char)((i>>9l)&0x7F) << 1);
                 else if (tmpw < 0)
                   wheel[voice] -= ((unsigned char)((i>>9l)&0x7F) << 1);
                 FM_Pitch_Wheel (voice, wheel[voice]);
               }
               /* wheel position */
               else {
                 wheel[voice] += (i>>16l)&0xFF;
                 FM_Pitch_Wheel (voice, wheel[voice]);
                 gwheel[voice] = -1;
               }
           }
           else {
            /* dynamics */
            if (!(gvol[voice]))
              switch (i&0x300) {
                case 0x200:
                  gvol[voice] = -1;
                  switch ((i>>10)&0x7) {
                    case 0:
                      vol[voice] = PIANISSIMO;
                      FM_Set_Volume (voice, vol[voice]);
                      break;
                    case 1:
                      vol[voice] = PPIANO;
                      FM_Set_Volume (voice, vol[voice]);
                      break;
                    case 2:
                      vol[voice] = PIANO;
                      FM_Set_Volume (voice, vol[voice]);
                      break;
                    case 3:
                      vol[voice] = MEZZOPIANO;
                      FM_Set_Volume (voice, vol[voice]);
                      break;
                    case 4:
                      vol[voice] = MEZZOFORTE;
                      FM_Set_Volume (voice, vol[voice]);
                      break;
                    case 5:
                      vol[voice] = FORTE;
                      FM_Set_Volume (voice, vol[voice]);
                      break;
                    case 6:
                      vol[voice] = FFORTE;
                      FM_Set_Volume (voice, vol[voice]);
                      break;
                    case 7:
                      vol[voice] = FORTISSIMO;
                      FM_Set_Volume (voice, vol[voice]);
                  }
                  break;
                case 0x300:
                  gvol[voice] = -1;
                  vol[voice] = (i>>10)&0x7F;
                  FM_Set_Volume (voice, vol[voice]);
                /* ignore dimminuendos and crescendos... */
              }
           }
           break;
          /* measure bars */
          case 0x80000000l:
            gaccb = -1;
            if (!grepeat) {
              if ((i&0x7) == 3) {
                if (endnum <= 0)
                  for (k = 0; k < NUMVOICES; k++) {
                    repeat[k][0] = pg;
                    repeat[k][1] = col;
                  }
                else
                  grepeat = gending = -1;
                endnum++;
              }
              if ((i&0x7) == 5) {
                grepeat = gending = -1;
                if (endnum <= 0)
                  for (k = 0; k < NUMVOICES; k++) {
                    repeat[k][0] = pg;
                    repeat[k][1] = col;
                  }
              }
            }
            break;
          /* control stuff */
          case 0xA0000000l:
            switch (i&0x700) {
              /* segno */
              case 0x200:
                if (!gsegno) {
                  gsegno = -1;
                  for (k = 0; k < NUMVOICES; k++) {
                    _segno[k][0] = pg;
                    _segno[k][1] = col;
                  }
                }
                break;
              /* coda */
              case 0x300:
                if (!gcoda) {
                  gcoda = -1;
                  for (k = 0; k < NUMVOICES; k++) {
                    _coda[k][0] = pg;
                    _coda[k][1] = col;
                  }
                }
                break;
              /* fine */
              case 0x400:
                if (!gfine) {
                  gfine = -1;
                  for (k = 0; k < NUMVOICES; k++) {
                    fine[k][0] = pg;
                    fine[k][1] = col;
                  }
                }
                break;
              case 0x700:
                if (!gperc) {
                  gperc = -1;
                  if (i&0x800)
                    FM_Set_Percussion (percussion = -1);
                  else
                    FM_Set_Percussion (percussion = 0);
                }
              /* ignore other cases */
            }
            break;
          /* endings */
          case 0xC0000000l:
            if (endnum > 0) endnum--;
            if (!gending) {
              gending = -1;
              for (k = 0; k < NUMVOICES; k++)
                ending[k] = -1;
              j = 1;
              for (k = 0; !(i&(j<<8)) && k < 16; k++, j<<=1);
              if (k != 16)
                for (j = 0; j < NUMVOICES; j++)
                  repeatnum[j] = k+1;
            }
            break;
          /* miscellaneous */
          case 0xE0000000l:
            switch (i&0x000E0000l) {
              /* time signature */
              case 0x00000:
                if (!gtsig) {
                  gtsig = -1;
                  tsig = (i>>8)&0xFF;
                }
                break;
              /* tempo */
              case 0x60000:
                if (!gtmpo) {
                  gtmpo = -1;
                  tmpo = (i>>8)&0x1FF;
                  beat = npb[(i>>20)&0xF];
                }
                break;
              /* instrument change */
              case 0x80000:
                if (!(ginstr[voice])) {
                  ginstr[voice] = -1;
                  FM_Set_Instrument (voice, (i>>20)&0x7F, (i>>8)&0x7F);
                }
                break;
              /* pan */
              case 0xA0000:
                if (!(gpan[voice])) {
                  gpan[voice] = -1;
                  pan[voice] = (signed char)((i>>8)&0xFF);
                  FM_Set_Pan (voice, pan[voice]);
                }
              /* ignoring others... */
            }
        }
    } while (col > 0);
    col = NUMCOL;
    gaccb = -1;
  } while (pg > 0);
  if (!grepeat && endnum <= 0)
    for (k = 0; k < NUMVOICES; k++) {
      repeat[k][0] = 0;
      repeat[k][1] = 0;
    }
  /* remap the original EMS page */
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
  return;
}

/****************************************************************************/
/* the guts of this file... this routine plays the song from the specified
   starting point to the end point... it returns immediately if the proper
   hardware is not present */
void playsong(int beginpage, int begincol, int endpage, int endcol) {
  unsigned char i, j, note, followvoice = 0, tienote[NUMVOICES],
                tie[NUMVOICES], lastitem[NUMVOICES];
  unsigned cur[NUMVOICES][4], timeleft[NUMVOICES], playing, ch = 0, tmpv,
           play[NUMVOICES], found, timenext, vd = 0, accent[NUMVOICES],
           stoccato[NUMVOICES], ds[NUMVOICES], pd = 0, dc[NUMVOICES], wd = 0,
           fermata[NUMVOICES], cpage = curpage, startnote[NUMVOICES];
  unsigned long nextevent, event;
  signed long tmpod = 0, vold[NUMVOICES], panfxd[NUMVOICES], pand[NUMVOICES],
              tmpofxd, volfxd[NUMVOICES], wheeld[NUMVOICES], wheelfxd[NUMVOICES];
  int savedmode = editmode;

  if (!_sound) {
    Message ("No Adlib compatible sound devices found!");
    return;
  }
  else if (gmidi)
    UseMIDI();
  else if (opl3)
    UseOPL3();
  else
    UseOPL2();
  FM_Set();
  for (i = 0; i < NUMVOICES; i++) FM_Set_Instrument (i,0,0);
  /* default to no percussion (change when a marker is encountered) */
  FM_Set_Percussion (percussion = 0);
  /* default these values */
  memset (repeat, 0, 4*NUMVOICES); memset (_segno, 0, 4*NUMVOICES);
  memset (_coda, -1, 4*NUMVOICES); memset (key, 0, 7*NUMVOICES);
  memset (accident, 0, 7*NUMVOICES); memset (timeleft, 0, 2*NUMVOICES);
  memset (accent, 0, 2*NUMVOICES); memset (stoccato, 0, 2*NUMVOICES);
  memset (vold, 0, 4*NUMVOICES); memset (fermata, 0, 2*NUMVOICES);
  memset (fine, 0, 4*NUMVOICES); memset (ending, 0, 2*NUMVOICES);
  memset (startnote, 0, 2*NUMVOICES);
  memset (tie, 0, NUMVOICES); memset (tienote, 0x23, NUMVOICES);
  memset (pan, 0, 2*NUMVOICES); memset (pand, 0, 4*NUMVOICES);
  memset (wheel, 0, 2*NUMVOICES); memset (wheeld, 0, 4*NUMVOICES);
  memset (wheeldest, 0, 2*NUMVOICES); memset (lastitem, 0, NUMVOICES);
  memset (keypan, 0, 2*NUMVOICES); memset (keypandata, 0, 2*NUMVOICES);
  tmpo = tempo[0]+1; beat = npb[tempo[1]&0xF]; memset (kb, 0, 121);
  tsig = timesig; playing = NUMVOICES; memset (play, -1, 2*NUMVOICES);
  memset (ds, 0, 2*NUMVOICES); memset (dc, 0, 2*NUMVOICES); timenext = 0;
  for (i = 0; i < NUMVOICES; i++) {
    audible[i] = !mute[i];
    lastnote[i] = 0x80;
    cur[i][0] = beginpage;
    cur[i][1] = begincol-1;
    cur[i][2] = beginpage;
    cur[i][3] = begincol-1;
    repeatnum[i] = 1;
    vol[i] = MEZZOFORTE;
    repeat[i][1] = (unsigned)-1;
    _segno[i][1] = (unsigned)-1;
    _coda[i][1] = (unsigned)-1;
  }
  /* search back for tempos, dynamics, instruments, codas, segnos, and
     repeats */
  goback (beginpage, begincol);
  /* update fixed point representations of data that was possibly changed
     by call to goback() */
  tmpofxd = ((unsigned long)tmpo) << 20l;
  for (i = 0; i < NUMVOICES; i++) {
    volfxd[i] = ((unsigned long)vol[i]) << 20l;
    panfxd[i] = ((long)pan[i]) << 20l;
    wheelfxd[i] = ((long)wheel[i]) << 20l;
  }
  /* EMS map the proper page if necessary */
  if (beginpage != curpage) {
    if (mappedpage != beginpage/PAGES) {
      if (EMSmap (EMShandle[0],0,beginpage/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = beginpage/PAGES;
    }
    page = song + (beginpage%PAGES)*NUMVOICES*NUMCOL;
    curpage = beginpage;
  }
  editmode = 0;
  undraw_col();
  if (upd) {
    box (0,TOPY,MAXX,LOWY,bg);
    redraw_music(0, NUMCOL-1);
  }
  update_info();
  /* install our timer ISR and set the new timer value */
  elapsed = 0;
  realtime = time(NULL);
  installFMTimerISR();
  spt = settempo(tmpo,beat);
  /* play the song... */
  do {
    /* if key pressed, process it */
    if (kbhit()) {
      ch = getxkey() & 0x0FFF;
      switch (ch) {
        /* the "P" key pauses playback */
        case 0x0019:
          for (i = 0; i < NUMVOICES; i++)
            Key_Off (i,cur[i][2],cur[i][3],lastnote[i]);
          event = count;
          count = 0;
          Message ("Playback Paused");
          for (i = 0; i < NUMVOICES; i++)
            if (!(lastnote[i]&0x80)) Key_On (i,cur[i][2],cur[i][3],lastnote[i]);
          count = event;
          break;
        /* Shift + Number toggles a voice */
        case 0x0102: case 0x0103: case 0x0104: case 0x0105: case 0x0106:
        case 0x0107: case 0x0108: case 0x0109: case 0x010A: case 0x010B:
          i = (ch & 0xFF) - 2;
          if (!audible[i]) {
            audible[i] = !audible[i];
            if (!(lastnote[i]&0x80)) Key_On (i,cur[i][2],cur[i][3],lastnote[i]);
          }
          else {
            Key_Off (i,cur[i][2],cur[i][3],lastnote[i]);
            audible[i] = !audible[i];
          }
          update_info();
          break;
        /* Alt + Number turns on a voice */
        case 0x0202: case 0x0203: case 0x0204: case 0x0205: case 0x0206:
        case 0x0207: case 0x0208: case 0x0209: case 0x020A: case 0x020B:
          i = (ch & 0xFF) - 2;
          if (!audible[i]) {
            audible[i] = -1;
            if (!(lastnote[i]&0x80)) Key_On (i,cur[i][2],cur[i][3],lastnote[i]);
            update_info();
          }
          break;
        /* Ctrl + Number turns off a voice */
        case 0x0402: case 0x0403: case 0x0404: case 0x0405: case 0x0406:
        case 0x0407: case 0x0408: case 0x0409: case 0x040A: case 0x040B:
          i = (ch & 0xFF) - 2;
          if (audible[i]) {
            Key_Off (i,cur[i][2],cur[i][3],lastnote[i]);
            audible[i] = 0;
            update_info();
          }
      }
    }
    /* see if we are ready for music */
    if (count == 0) {
      for (i = 0; i < NUMVOICES; i++) timeleft[i] -= timenext;
      /* update volumes if changing */
      if (vd)
        for (i = 0; i < NUMVOICES; i++)
          if (vold[i]) {
            if (-vold[i] >= volfxd[i]) {
              volfxd[i] = 0;
              vold[i] = 0;
              vd--;
            }
            else if (vold[i] >= ((long)FORTISSIMO<<20l) - volfxd[i]) {
              volfxd[i] = (long)FORTISSIMO<<20l;
              vold[i] = 0;
              vd--;
            }
            else
              volfxd[i] += vold[i];
            tmpv = vol[i];
            vol[i] = volfxd[i] >> 20l;
            if (tmpv != vol[i])
              FM_Set_Volume (i, vol[i]);
          }
      /* update pan levels if changing */
      if (pd)
        for (i = 0; i < NUMVOICES; i++)
          if (pand[i]) {
            if (-pand[i] >= panfxd[i] - ((long)(-128)<<20l)) {
              panfxd[i] = (long)(-128)<<20l;
              pand[i] = 0;
              pd--;
            }
            else if (pand[i] >= ((long)127<<20l) - panfxd[i]) {
              panfxd[i] = (long)127<<20l;
              pand[i] = 0;
              pd--;
            }
            else
              panfxd[i] += pand[i];
            tmpv = pan[i];
            pan[i] = panfxd[i] >> 20l;
            if (tmpv != pan[i])
              FM_Set_Pan (i, pan[i]);
          }
      /* update wheel position if changing */
      if (wd)
        for (i = 0; i < NUMVOICES; i++)
          if (wheeld[i]) {
            if (-wheeld[i] >= wheelfxd[i] - ((long)(-128)<<20l)) {
              wheelfxd[i] = (long)(-128)<<20l;
              wheeld[i] = 0;
              wd--;
            }
            else if (wheeld[i] >= ((long)127<<20l) - wheelfxd[i]) {
              wheelfxd[i] = (long)127<<20l;
              wheeld[i] = 0;
              wd--;
            }
            else
              wheelfxd[i] += wheeld[i];
            tmpv = wheel[i];
            wheel[i] = wheelfxd[i] >> 20l;
            if (wheeld[i] > 0 && wheel[i] >= wheeldest[i]) {
              wheeld[i] = 0;
              wheel[i] = wheeldest[i];
              wd--;
            }
            else if (wheeld[i] < 0 && wheel[i] <= wheeldest[i]) {
              wheeld[i] = 0;
              wheel[i] = wheeldest[i];
              wd--;
            }
            if (tmpv != wheel[i])
              FM_Pitch_Wheel (i, wheel[i]);
          }
      /* update tempo if changing */
      if (tmpod) {
        if (-tmpod >= tmpofxd) {
          tmpofxd = 0;
          tmpod = 0;
        }
        else if (tmpod >= (512l<<20l) - tmpofxd) {
          tmpofxd = 512l<<20l;
          tmpod = 0;
        }
        else
          tmpofxd += tmpod;
        tmpo = tmpofxd >> 20l;
        if (tmpo == 0) {
          tmpo++;
          tmpofxd = ((long)tmpo) << 20l;
          tmpod = 0;
        }
        elapsed += ticks*spt;
        spt = settempo (tmpo, beat);
      }
      /* if voice was accented then restore its volume */
      for (i = 0; i < NUMVOICES; i++)
        if (play[i] && timeleft[i] == 0 && accent[i]) {
          FM_Set_Volume (i, vol[i]);
          accent[i] = 0;
        }
      /* if note was stoccato then end voice short and pause */
      for (i = 0; i < NUMVOICES; i++)
        if (play[i] && timeleft[i] == 0 && stoccato[i]) {
          Key_Off (i,cur[i][0],cur[i][1],lastnote[i]);
          timeleft[i] = stoccato[i];
          stoccato[i] = 0;
        }
      /* loop through voices */
      for (i = 0; i < NUMVOICES; i++)
        if (play[i] && timeleft[i] == 0) {
          found = nextevent = 0;
          do {
            nextevent = getnextitem (i, cur[i][1] + 1, cur[i][0]);
            if (cur[i][0] != (nextevent&0xFFFFl)) {
              cur[i][0] = nextevent&0xFFFFl;
              memcpy (accident[i], key[i], 7);
            }
            cur[i][1] = nextevent>>16l;
            /* is this voice done ? */
            if ((nextevent&0xFFFFl) > endpage ||
                ((nextevent&0xFFFFl) == endpage && (nextevent>>16l) > endcol)) {
              play[i] = 0; playing--;
              if (i == followvoice)
                while (!play[followvoice] && followvoice < NUMVOICES-1)
                  followvoice++;
            }
            else {
              event = getevent(i,nextevent);
              switch (event&0xE0000000l) {
                 /* invalid address in song */
                case 0x00000000l:
                  play[i] = 0; playing--;
                  if (i == followvoice)
                    while (!play[followvoice] && followvoice < NUMVOICES-1)
                      followvoice++;
                  break;
                /* note or rest */
                case 0x20000000l: found = -1; break;
                /* accidental or key signature */
                case 0x40000000l:
                  /* key signature */
                  if ((event&0xE1000800l) == 0x40000800l)
                    switch ((event>>8)&0x07) {
                      case 0: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = -1; break;
                      case 1: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = -2; break;
                      case 2: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = 1; break;
                      case 3: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = 2; break;
                      default:key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = 0;
                    }
                  else if ((event&0xE1000800l) == 0x40000000l) {
                    switch ((event>>8)&0x07) {
                      case 0: accident[i][(event&0x7F)%7] = -1; break;
                      case 1: accident[i][(event&0x7F)%7] = -2; break;
                      case 2: accident[i][(event&0x7F)%7] = 1; break;
                      case 3: accident[i][(event&0x7F)%7] = 2; break;
                      default:accident[i][(event&0x7F)%7] = 0;
                    }
                    if ((event&0x7F) == tienote[i])
                      lastitem[i] = -1;
                  }
                  /* keyboard pan */
                  else if ((event&0xE1000000l) == 0x41000000l) {
                    keypan[i] = -1;
                    keypandata[i] = (event>>8l)&0x7f3f;
                    if (pand[i]) {
                      pd--;
                      pand[i] = 0;
                    }
                  }
                  break;
                /* dynamics / pitch wheel */
                case 0x60000000l:
                 /* pitch wheel */
                 if (event&0x1000000l) {
                   /* wheel roll */
                   if (event & 0x100l) {
                     if (!wheeld[i])
                       wd++;
                     wheeld[i] = (signed char)((event>>16)&0xFF);
                     wheeld[i] <<= 20l;
                     wheeld[i] /= (long)beat;
                     if (wheeld[i] > 0)
                       wheeldest[i] = wheel[i] + ((unsigned char)((event>>9)&0x7F) << 1);
                     else
                       wheeldest[i] = wheel[i] - ((unsigned char)((event>>9)&0x7F) << 1);
                   }
                   /* wheel position */
                   else {
                     wheel[i] = (signed char)((event>>16)&0xFF);
                     wheelfxd[i] = (long)(wheel[i]) << 20l;
                     FM_Pitch_Wheel (i, wheel[i]);
                     if (wheeld[i]) {
                       wd--;
                       wheeld[i] = 0;
                     }
                   }
                 }
                 /* dynamics */
                 else {
                  /* immediate volumes */
                  switch (event&0xE0000300l) {
                    /* dimmenuendo */
                    case 0x60000000l:
                      if (!vold[i])
                        vd++;
                      vold[i] = 1+(event>>10l)&0x7Fl;
                      vold[i] <<= 20l;
                      vold[i] /= (long)beat;
                      vold[i] = -vold[i];
                      break;
                    /* crescendo */
                    case 0x60000100l:
                      if (!vold[i])
                        vd++;
                      vold[i] = 1+(event>>10l)&0x7Fl;
                      vold[i] <<= 20l;
                      vold[i] /= (long)beat;
                      break;
                    /* vol levels */
                    case 0x60000200l:
                      switch ((event>>10)&0x7) {
                        case 0:
                          vol[i] = PIANISSIMO;
                          volfxd[i] = (long)PIANISSIMO << 20l;
                          FM_Set_Volume (i, PIANISSIMO);
                          break;
                        case 1:
                          vol[i] = PPIANO;
                          volfxd[i] = (long)PPIANO << 20l;
                          FM_Set_Volume (i, PPIANO);
                          break;
                        case 2:
                          vol[i] = PIANO;
                          volfxd[i] = (long)PIANO << 20l;
                          FM_Set_Volume (i, PIANO);
                          break;
                        case 3:
                          vol[i] = MEZZOPIANO;
                          volfxd[i] = (long)MEZZOPIANO << 20l;
                          FM_Set_Volume (i, MEZZOPIANO);
                          break;
                        case 4:
                          vol[i] = MEZZOFORTE;
                          volfxd[i] = (long)MEZZOFORTE << 20l;
                          FM_Set_Volume (i, MEZZOFORTE);
                          break;
                        case 5:
                          vol[i] = FORTE;
                          volfxd[i] = (long)FORTE << 20l;
                          FM_Set_Volume (i, FORTE);
                          break;
                        case 6:
                          vol[i] = FFORTE;
                          volfxd[i] = (long)FFORTE << 20l;
                          FM_Set_Volume (i, FFORTE);
                          break;
                        case 7:
                          vol[i] = FORTISSIMO;
                          volfxd[i] = (long)FORTISSIMO << 20l;
                          FM_Set_Volume (i, FORTISSIMO);
                      }
                      if (vold[i]) {
                        vd--;
                        vold[i] = 0;
                      }
                      break;
                    case 0x60000300l:
                      vol[i] = (event>>10)&0x7F;
                      volfxd[i] = (long)(vol[i]) << 20l;
                      FM_Set_Volume (i, vol[i]);
                      if (vold[i]) {
                        vd--;
                        vold[i] = 0;
                      }
                  }
                 }
                 break;
                /* bar */
                case 0x80000000l:
                  memcpy (accident[i], key[i], 7);
                  lastitem[i] = 0;
                  switch (event&0x7) {
                    /* end repeat */
                    case 3: if (ending[i]) {
                              cur[i][0] = repeat[i][0];
                              cur[i][1] = repeat[i][1];
                              repeatnum[i]++;
                            }
                            else {
                              note = ((event>>4)&0xF) + 1;
                              if (note == 1) note++;
                              if (repeatnum[i] < note) {
                                cur[i][0] = repeat[i][0];
                                cur[i][1] = repeat[i][1];
                                repeatnum[i]++;
                              }
                              else {
                                repeat[i][0] = cur[i][0];
                                repeat[i][1] = cur[i][1];
                                repeatnum[i] = 1;
                                ending[i] = 0;
                              }
                            }
                            break;
                    /* begin repeat */
                    case 5: repeat[i][0] = cur[i][0];
                            repeat[i][1] = cur[i][1];
                            repeatnum[i] = 1;
                            ending[i] = 0;
                  }
                  break;
                /* control */
                case 0xA0000000l:
                  switch ((event>>8)&7) {
                    case 0: if (!dc[i]) {
                              cur[i][0] = 0; cur[i][1] = (unsigned)-1;
                              memcpy (accident[i], key[i], 7);
                              dc[i] = -1;
                            }
                            /* this code used to clear the D.C. flag
                             * for this voice- but that causes the potential
                             * for an infinite loop... */
                            /*else
                              dc[i] = 0;*/
                            break;
                    case 1: if (!ds[i]) {
                              cur[i][0] = _segno[i][0];
                              cur[i][1] = _segno[i][1];
                              memcpy (accident[i], key[i], 7);
                              ds[i] = -1;
                            }
                            else
                              ds[i] = 0;
                            break;
                    case 2: _segno[i][0] = cur[i][0];
                            _segno[i][1] = cur[i][1];
                            break;
                    case 3: if (cur[i][0] == _coda[i][0] && cur[i][1] == _coda[i][1]) {
                              nextevent = getending (-1, cur[i][1] + 1, cur[i][0]);
                              cur[i][0] = nextevent&0xFFFFl;
                              cur[i][1] = nextevent>>16l;
                              memcpy (accident[i], key[i], 7);
                              /* is this voice done ? */
                              if ((nextevent&0xFFFFl) > endpage ||
                                 ((nextevent&0xFFFFl) == endpage && (nextevent>>16l) > endcol)) {
                                play[i] = 0; playing--;
                                if (i == followvoice)
                                  while (!play[followvoice] && followvoice < NUMVOICES-1)
                                    followvoice++;
                              }
                              ds[i] = dc[i] = 0;
                            }
                            else {
                              _coda[i][0] = cur[i][0];
                              _coda[i][1] = cur[i][1];
                            }
                            break;
                    case 4: if (cur[i][0] == fine[i][0] && cur[i][1] == fine[i][1]) {
                              play[i] = 0; playing--;
                              if (i == followvoice)
                                while (!play[followvoice] && followvoice < NUMVOICES-1)
                                  followvoice++;
                            }
                            else {
                              fine[i][0] = cur[i][0];
                              fine[i][1] = cur[i][1];
                            }
                            break;
                    case 7: if (i == followvoice)
                              if ((event>>8)&8) {
                                for (j = 6; j < NUMVOICES; j++)
                                  if (!(lastnote[j]&0x80))
                                    Key_Off (j,cur[j][2],cur[j][3],lastnote[j]);
                                FM_Set_Percussion (percussion = -1);
                              }
                              else {
                                for (j = 6; j < NUMVOICES; j++)
                                  if (!(lastnote[j]&0x80))
                                    Key_Off (j,cur[j][2],cur[j][3],lastnote[j]);
                                FM_Set_Percussion (percussion = 0);
                              }
                  }
                  break;
                /* endings */
                case 0xC0000000l:
                  ending[i] = -1;
                  nextevent = getending (repeatnum[i], cur[i][1], cur[i][0]);
                  cur[i][0] = nextevent&0xFFFFl;
                  cur[i][1] = nextevent>>16l;
                  if (repeatnum[i] != 1)
                    memcpy (accident[i], key[i], 7);
                  break;
                /* etc... */
                case 0xE0000000l:
                  switch (event&0xE00E0000l) {
                    /* time signature */
                    case 0xE0000000l:
                      if (i == followvoice)
                        tsig = (event>>8)&0xFF;
                      break;
                    /* accelerando */
                    case 0xE0020000l:
                      if (i == followvoice) {
                        tmpod = 1+(event>>8l)&0xFFl;
                        tmpod <<= 20l;
                          /* cut time */
                        if ((tsig&0x1F) == 0 && ((tsig>>5)&7) == 7)
                          tmpod /= (long)(2*npb[1]);
                          /* standard time */
                        else if ((tsig&0x1F) == 0)
                          tmpod /= (long)(4*npb[2]);
                          /* other */
                        else
                          tmpod /= (long)(((tsig&0x1F)+1)*npb[(tsig>>5)&7]);
                      }
                      break;
                    /* ritardando */
                    case 0xE0040000l:
                      if (i == followvoice) {
                        tmpod = 1+(event>>8l)&0xFFl;
                        tmpod <<= 20l;
                          /* cut time */
                        if ((tsig&0x1F) == 0 && ((tsig>>5)&7) == 7)
                          tmpod /= (long)(2*npb[1]);
                          /* standard time */
                        else if ((tsig&0x1F) == 0)
                          tmpod /= (long)(4*npb[2]);
                          /* other */
                        else
                          tmpod /= (long)(((tsig&0x1F)+1)*npb[(tsig>>5)&7]);
                        tmpod = -tmpod;
                      }
                      break;
                    /* tempo */
                    case 0xE0060000l:
                      if (i == followvoice) {
                        tmpod = 0;
                        tmpo = (event>>8)&0x1FF;
                        tmpofxd = ((unsigned long)tmpo) << 20;
                        beat = npb[(event>>20)&0xF];
                        elapsed += ticks*spt;
                        spt = settempo (tmpo, beat);
                      }
                      break;
                    /* program change */
                    case 0xE0080000l:
                      if (event&0x00008000) {
                        if (percussion && i == followvoice)
                          FM_Set_DrumSet ((event>>8)&0x7F);
                      }
                      else
                        FM_Set_Instrument (i, (event>>20)&0x7F,(event>>8)&0x7F);
                      break;
                    /* pan */
                    case 0xE00A0000l:
                      keypan[i] = 0;
                      pan[i] = (signed char)((event>>8)&0xFF);
                      panfxd[i] = (long)(pan[i]) << 20l;
                      FM_Set_Pan (i, pan[i]);
                      if (pand[i]) {
                        pd--;
                        pand[i] = 0;
                      }
                      break;
                    /* fermata */
                    case 0xE00C0000l:
                      fermata[i] = ((event>>8)&0xFF) + 105;
                      break;
                    /* stereo fade */
                    case 0xE00E0000l:
                      keypan[i] = 0;
                      if (!pand[i])
                        pd++;
                      pand[i] = (signed char)((event>>8)&0xFF);
                      pand[i] <<= 20l;
                      pand[i] /= (long)beat;
                  }
              }
            }
          } while (!found && play[i]);
          nextevent = event;
          event &= 0x7F;
          note = (event/7)*12 + MIDINote[event%7] + accident[i][event%7];
          if (!(lastnote[i]&0x80) &&
             ((nextevent&0xE0004080l) != 0x20004000l ||
              tienote[i] != (nextevent&0x7F) ||
              (lastitem[i] && lastnote[i] != note)))
            Key_Off (i,cur[i][2],cur[i][3],lastnote[i]);
          if (found) {
            /* scrolling music follows voice one */
            if (i == followvoice) {
              if (cur[followvoice][0] != curpage) {
                if (mappedpage != cur[followvoice][0]/PAGES) {
                  if (EMSmap (EMShandle[0],0,cur[followvoice][0]/PAGES)) {
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
                  mappedpage = cur[followvoice][0]/PAGES;
                }
                page = song + (cur[followvoice][0]%PAGES)*NUMVOICES*NUMCOL;
                curpage = cur[followvoice][0];
                if (upd) {
                  box (0,TOPY,MAXX,LOWY,bg);
                  redraw_music(0, NUMCOL-1);
                }
                update_info();
              }
            }
              /* event is a rest */
            if (nextevent & 0x80) {
              timeleft[i] = npb[(nextevent>>8)&0xF];
              /* if note is triplet */
              if (nextevent&0x1000l)
                timeleft[i] = (timeleft[i]<<1)/3;
              /* if note has fermata */
              if (fermata[i]) {
                timeleft[i] = ((long)timeleft[i]*(long)fermata[i])/100;
                fermata[i] = 0;
              }
              note = 0x80;
            }
              /* event is a note */
            else {
              if (note > 127) note -= 12;
              timeleft[i] = npb[(nextevent>>8)&0xF];
              /* if note is triplet */
              if (nextevent&0x1000l)
                timeleft[i] = (timeleft[i]<<1)/3;
              /* if note has fermata */
              if (fermata[i]) {
                timeleft[i] = ((long)timeleft[i]*(long)fermata[i])/100;
                fermata[i] = 0;
              }
              /* if note stoccato */
              if (nextevent&0x8000l) {
                stoccato[i] = timeleft[i];
                timeleft[i] = (3*timeleft[i])>>2;
                stoccato[i] -= timeleft[i];
              }
              /* if note is accented */
              if (nextevent&0x2000l) {
                accent[i] = -1;
                if ((vol[i]*120)/100 > FORTISSIMO)
                  FM_Set_Volume (i, FORTISSIMO);
                else
                  FM_Set_Volume (i, (vol[i]*120)/100);
              }
              if (!(nextevent&0x4000l) ||
                  tienote[i] != (nextevent&0x7F) ||
                  (lastitem[i] && lastnote[i] != note)) {
                startnote[i] = -1;
                cur[i][2] = cur[i][0];
                cur[i][3] = cur[i][1];
                tie[i] = 0;
              }
              else
                tie[i] = -1;
              /* keep track of position on clef (vs. actual pitch) for ties */
              tienote[i] = nextevent&0x7F;
            }
            lastitem[i] = 0;
            if (!tie[i])
              lastnote[i] = note;
          }
        }
      /* sound notes */
      for (i = 0; i < NUMVOICES; i++)
        if (startnote[i]) {
          if (keypan[i]) {
            int n = (keypandata[i]>>8)&0x7f, r = (keypandata[i]&0x3f)+1;
            if (lastnote[i] >= n+r)
              pan[i] = 127;
            else if (lastnote[i] <= n-r)
              pan[i] = -128;
            else
              pan[i] = ((lastnote[i]-n)<<7)/r;
            if (pan[i] > 127) pan[i] = 127;
            else if (pan[i] < -128) pan[i] = -128;
            FM_Set_Pan (i, pan[i]);
            panfxd[i] = pan[i] << 20l;
          }
          Key_On (i, cur[i][0], cur[i][1], lastnote[i]);
          startnote[i] = 0;
        }
      /* update pause time */
        /* volume or tempo still changing, update it every tick */
      if (vd || pd || wd || tmpod)
        timenext = 1;
      else {
        timenext = 0xFFFF;
        for (i = 0; i < NUMVOICES; i++)
          if (play[i] && timenext > timeleft[i]) timenext = timeleft[i];
      }
      /* reset timer */
      count = timenext;
    }
  } while (ch != 0x0001 && playing);
  for (i = 0; i < NUMVOICES; i++)
    FM_Key_Off (i);
  /* reset timer ISR */
  unloadFMTimerISR();
  /* reset timer value */
  outportb (0x43, 0x3C);
  outportb (0x40, 0xFF);
  outportb (0x40, 0xFF);
  /* reset clock to what it should be (as if the timer were never sped up) */
  elapsed += ticks*spt;
  realtime += elapsed >> 16;
  stime (&realtime);
  /* EMS map the proper page if necessary */
  if (curpage != cpage) {
    if (mappedpage != cpage/PAGES) {
      if (EMSmap (EMShandle[0],0,cpage/PAGES)) {
        VESAQuit();
        FM_Reset();
        EMSfree (EMShandle[0]);
        EMSfree (EMShandle[1]);
        unloadkeyboardISR();
        unloadErrorHandler();
        fprintf (stderr, "compozer: EMS mapping error\n");
        _exit (1);
      }
      mappedpage = cpage/PAGES;
    }
    page = song + (cpage%PAGES)*NUMVOICES*NUMCOL;
    curpage = cpage;
  }
  editmode = savedmode;
  /* redraw the screen... */
  redraw_screen();
  /* done */
  return;
}





/****************************************************************************/
/* this routine writes a variable length value */
void writevarlen (unsigned long v, FILE *f) {
  v &= 0x0FFFFFFFl;
  if (v > 0x001FFFFFl)
    fputc (((v>>21l)&0x7F)|0x80, f);
  if (v > 0x00003FFFl)
    fputc (((v>>14l)&0x7F)|0x80, f);
  if (v > 0x0000007Fl)
    fputc (((v>>7l)&0x7F)|0x80, f);
  fputc (v&0x7F, f);
  return;
}


/****************************************************************************/
/* this routine (very similar to the above one), writes a MIDI file. It first
   asks the user to pick a filename, then writes the data out. */
void export_midi (void) {
  FILE *f;
  int ii;
  unsigned char i, j, note, *fname, tie[NUMVOICES], tienote[NUMVOICES],
                lastitem[NUMVOICES];
  unsigned cur[NUMVOICES][4], timeleft[NUMVOICES], playing, play[NUMVOICES],
           found, timenext, vd = 0, accent[NUMVOICES], stoccato[NUMVOICES],
           ds[NUMVOICES], pd = 0, dc[NUMVOICES], fermata[NUMVOICES], wd = 0;
  unsigned long nextevent, event, tmpv, trklen[NUMVOICES][2];
  signed long tmpod = 0, vold[NUMVOICES], panfxd[NUMVOICES], pand[NUMVOICES],
              tmpofxd, volfxd[NUMVOICES], wheeld[NUMVOICES], wheelfxd[NUMVOICES];
  /* get output file name */
  if ((fname = GetFileName(0,".MID")) == NULL)
    return;
  if ((f = fopen(fname,"r")) != NULL) {
    fclose (f);
    if ((ii=YesNo ("Overwrite Existing File ?")) == -1 || ii == 0)
      return;
  }
  /* open file */
  if ((f = fopen (fname, "w+b")) == NULL) {
    Message ("Error opening file for writing!");
    return;
  }
  /* default these values */
  memset (repeat, 0, 4*NUMVOICES); memset (_segno, 0, 4*NUMVOICES);
  memset (_coda, -1, 4*NUMVOICES); memset (key, 0, 7*NUMVOICES);
  memset (accident, 0, 7*NUMVOICES); memset (timeleft, 0, 2*NUMVOICES);
  memset (accent, 0, 2*NUMVOICES); memset (stoccato, 0, 2*NUMVOICES);
  memset (audible, -1, 2*NUMVOICES); memset (vold, 0, 4*NUMVOICES);
  memset (fermata, 0, 2*NUMVOICES); memset (fine, 0, 4*NUMVOICES);
  memset (ending, 0, 2*NUMVOICES); memset (lastitem, 0, NUMVOICES);
  memset (tie, 0, NUMVOICES); memset (tienote, 0, NUMVOICES);
  memset (pan, 0, 2*NUMVOICES); memset (pand, 0, 4*NUMVOICES);
  memset (wheel, 0, 2*NUMVOICES); memset (wheeld, 0, 4*NUMVOICES);
  memset (keypan, 0, 2*NUMVOICES); memset (keypandata, 0, 2*NUMVOICES);
  tmpo = tempo[0]+1; beat = npb[tempo[1]&0xF]; memset (kb, 0, 121);
  tsig = timesig; playing = NUMVOICES; memset (play, -1, 2*NUMVOICES);
  memset (ds, 0, 2*NUMVOICES); memset (dc, 0, 2*NUMVOICES); timenext = 0;
  for (i = 0; i < NUMVOICES; i++) {
    lastnote[i] = 0x80;
    cur[i][0] = 0;
    cur[i][1] = (unsigned)-1;
    cur[i][2] = 0;
    cur[i][3] = (unsigned)-1;
    repeatnum[i] = 1;
    vol[i] = MEZZOFORTE;
    repeat[i][1] = (unsigned)-1;
    _segno[i][1] = (unsigned)-1;
    _coda[i][1] = (unsigned)-1;
  }
  tmpofxd = ((unsigned long)tmpo) << 20l;
  for (i = 0; i < NUMVOICES; i++) {
    volfxd[i] = ((unsigned long)vol[i]) << 20l;
    panfxd[i] = ((long)pan[i]) << 20l;
    wheelfxd[i] = ((long)wheel[i]) << 20l;
  }
  /* setup file out routines */
  MIDIFILE_Set();
  undraw_col();
  /* write the header */
  fwrite ("MThd", 4, 1, f);
  midibytes[0] = midibytes[1] = midibytes[2] = midibytes[4] = midibytes[6] = midibytes[8] = 0;
  midibytes[3] = 6; midibytes[5] = 1; midibytes[7] = 10; midibytes[9] = 96;
  fwrite (midibytes, 10, 1, f);
  /* write the song */
  for (i = 0; i < NUMVOICES; i++) {
    MIDIFILE_Set_Percussion (percussion = 0);
    /* setup track info... */
    fwrite ("MTrk",4,1,f);
    /* write out zero track length for now... we'll fix it later */
    midibytes[0] = midibytes[1] = midibytes[2] = midibytes[3] = 0;
    fwrite (midibytes,4,1,f);
    trklen[i][0] = ftell(f);
    midibytes[0] = 0xff;
    midibytes[1] = 0x01;
    if (i == 0) {
      writevarlen (0,f);
      fwrite (midibytes,2,1,f);
      writevarlen (strlen(title),f);
      fwrite (title, strlen(title), 1, f);
    }
    else if (i == 1) {
      writevarlen (0,f);
      fwrite (midibytes,2,1,f);
      writevarlen (strlen(composer),f);
      fwrite (composer, strlen(composer), 1, f);
    }
    else if (i == 2) {
      writevarlen (0,f);
      fwrite (midibytes,2,1,f);
      writevarlen (strlen(opus),f);
      fwrite (opus, strlen(opus), 1, f);
    }
    midibytes[1] = 0x04;
    writevarlen (0,f);
    fwrite (midibytes,2,1,f);
    if (percussion && i > 5) {
      writevarlen (10, f);
      fwrite ("Percussion",10,1,f);
    }
    else {
      writevarlen (strlen(Banks[0].Instrum[0]), f);
      fwrite (Banks[0].Instrum[0],strlen(Banks[0].Instrum[0]),1,f);
    }
    if (i == 0) {
      int nn, dd;
      spt = ((long)tmpo*(long)beat)/60l;
      if (spt == 0) spt = 1;
      else if (spt > 1024) spt = 1024;
      spt = 96000000l/spt;
      if (spt > 16777215l)
        spt = 16777215l;
      midibytes[0] = 0xff;
      midibytes[1] = 0x51;
      midibytes[2] = 0x03;
      midibytes[3] = (spt>>16l)&0xFF;
      midibytes[4] = (spt>>8l)&0xFF;
      midibytes[5] = (spt)&0xFF;
      writevarlen (0,f);
      fwrite (midibytes, 6, 1, f);
        /* cut time */
      if ((tsig&0x1F) == 0 && ((tsig>>5)&7) == 7) {
        nn = 2; dd = 1;
      }
        /* standard time */
      else if ((tsig&0x1F) == 0) {
        nn = 4; dd = 2;
      }
        /* other */
      else {
        nn = (tsig&0x1F)+1;
        dd = (tsig>>5)&7;
      }
      midibytes[0] = 0xff;
      midibytes[1] = 0x58;
      midibytes[2] = 0x04;
      midibytes[3] = nn;
      midibytes[4] = dd;
      midibytes[5] = 48;
      midibytes[6] = 8;
      writevarlen (0,f);
      fwrite (midibytes, 7, 1, f);
    }
    MIDIFILE_Set_Instrument (i, 0, 0, f, 0);
    MIDIFILE_Set_Volume (i, MEZZOFORTE, f, 0);
    MIDIFILE_Set_Pan (i, 0, f, 0);
    vd = pd = tmpod = count = timenext = 0;
    while (play[i]) {
      timeleft[i] -= count;
      /* update volumes if changing */
      if (vd)
        if (vold[i]) {
          if (-vold[i] >= volfxd[i]) {
            volfxd[i] = 0;
            vold[i] = 0;
            vd--;
          }
          else if (vold[i] >= ((long)FORTISSIMO<<20l) - volfxd[i]) {
            volfxd[i] = (long)FORTISSIMO<<20l;
            vold[i] = 0;
            vd--;
          }
          else
            volfxd[i] += vold[i];
          tmpv = vol[i];
          vol[i] = volfxd[i] >> 20l;
          if (tmpv != vol[i]) {
            timenext = MIDIFILE_Set_Volume (i, vol[i], f, timenext);
          }
        }
      /* update pan levels if changing */
      if (pd)
        if (pand[i]) {
          if (-pand[i] >= panfxd[i] - ((long)(-128)<<20l)) {
            panfxd[i] = (long)(-128)<<20l;
            pand[i] = 0;
            pd--;
          }
          else if (pand[i] >= ((long)127<<20l) - panfxd[i]) {
            panfxd[i] = (long)127<<20l;
            pand[i] = 0;
            pd--;
          }
          else
            panfxd[i] += pand[i];
          tmpv = pan[i];
          pan[i] = panfxd[i] >> 20l;
          if (tmpv != pan[i]) {
            timenext = MIDIFILE_Set_Pan (i, pan[i], f, timenext);
          }
        }
      /* update wheel position if changing */
      if (wd)
        if (wheeld[i]) {
          if (-wheeld[i] >= wheelfxd[i] - ((long)(-128)<<20l)) {
            wheelfxd[i] = (long)(-128)<<20l;
            wheeld[i] = 0;
            wd--;
          }
          else if (wheeld[i] >= ((long)127<<20l) - wheelfxd[i]) {
            wheelfxd[i] = (long)127<<20l;
            wheeld[i] = 0;
            wd--;
          }
          else
            wheelfxd[i] += wheeld[i];
          tmpv = wheel[i];
          wheel[i] = wheelfxd[i] >> 20l;
          if (wheeld[i] > 0 && wheel[i] >= wheeldest[i]) {
            wheeld[i] = 0;
            wheel[i] = wheeldest[i];
            wd--;
          }
          else if (wheeld[i] < 0 && wheel[i] <= wheeldest[i]) {
            wheeld[i] = 0;
            wheel[i] = wheeldest[i];
            wd--;
          }
          if (tmpv != wheel[i]) {
            timenext = MIDIFILE_Pitch_Wheel (i, wheel[i], f, timenext);
          }
        }
      /* update tempo if changing */
      if (tmpod) {
        if (-tmpod >= tmpofxd) {
          tmpofxd = 0;
          tmpod = 0;
        }
        else if (tmpod >= (512l<<20l) - tmpofxd) {
          tmpofxd = 512l<<20l;
          tmpod = 0;
        }
        else
          tmpofxd += tmpod;
        tmpv = tmpo;
        tmpo = tmpofxd >> 20l;
        if (tmpo == 0) {
          tmpo++;
          tmpofxd = ((long)tmpo) << 20l;
          tmpod = 0;
        }
        if (tmpv != tmpo)
          if (i == 0) {
            spt = ((long)tmpo*(long)beat)/60l;
            if (spt == 0) spt = 1;
            else if (spt > 1024) spt = 1024;
            spt = 96000000l/spt;
            if (spt > 16777215l)
              spt = 16777215l;
            midibytes[0] = 0xff;
            midibytes[1] = 0x51;
            midibytes[2] = 0x03;
            midibytes[3] = (spt>>16l)&0xFF;
            midibytes[4] = (spt>>8l)&0xFF;
            midibytes[5] = (spt)&0xFF;
            writevarlen (timenext,f);
            timenext = 0;
            fwrite (midibytes, 6, 1, f);
          }
      }
      /* if voice was accented then restore its volume */
      if (play[i] && timeleft[i] == 0 && accent[i]) {
        timenext = MIDIFILE_Set_Volume (i, vol[i], f, timenext);
        accent[i] = 0;
      }
      /* if note was stoccato then end voice short and pause */
      if (play[i] && timeleft[i] == 0 && stoccato[i]) {
        timenext = MIDIFILE_Key_Off (i,f,timenext);
        timeleft[i] = stoccato[i];
        stoccato[i] = 0;
      }
      /* write stuff to file until we find the next note or rest */
      if (play[i] && timeleft[i] == 0) {
        found = nextevent = 0;
        do {
          nextevent = getnextitem (i, cur[i][1] + 1, cur[i][0]);
          cur[i][0] = nextevent&0xFFFFl;
          cur[i][1] = nextevent>>16l;
          /* is this voice done ? */
          if ((nextevent&0xFFFFl) > hipage ||
              ((nextevent&0xFFFFl) == hipage && (nextevent>>16l) >= NUMCOL)) {
            play[i] = 0; playing--;
          }
          else {
            event = getevent(i,nextevent);
            switch (event&0xE0000000l) {
               /* invalid address in song */
              case 0x00000000l:
                play[i] = 0; playing--;
                break;
              /* note or rest */
              case 0x20000000l: found = -1; break;
              /* accidental or key signature */
              case 0x40000000l:
                /* key signature */
                if ((event&0xE1000800l) == 0x40000800l)
                  switch ((event>>8)&0x07) {
                    case 0: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = -1; break;
                    case 1: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = -2; break;
                    case 2: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = 1; break;
                    case 3: key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = 2; break;
                    default:key[i][(event&0x7F)%7] = accident[i][(event&0x7F)%7] = 0;
                  }
                else if ((event&0xE1000800l) == 0x40000000l) {
                  switch ((event>>8)&0x07) {
                    case 0: accident[i][(event&0x7F)%7] = -1; break;
                    case 1: accident[i][(event&0x7F)%7] = -2; break;
                    case 2: accident[i][(event&0x7F)%7] = 1; break;
                    case 3: accident[i][(event&0x7F)%7] = 2; break;
                    default:accident[i][(event&0x7F)%7] = 0;
                  }
                  if ((event&0x7F) == tienote[i])
                    lastitem[i] = -1;
                }
                else if ((event&0xE1000800l) == 0x41000000l) {
                  keypan[i] = -1;
                  keypandata[i] = (event>>8l)&0x7fff;
                  if (pand[i]) {
                    pd--;
                    pand[i] = 0;
                  }
                }
                break;
              /* dynamics / pitch wheel */
              case 0x60000000l:
               /* pitch wheel */
               if (event&0x1000000l) {
                 /* wheel roll */
                 if (event & 0x100l) {
                   if (!wheeld[i])
                     wd++;
                   wheeld[i] = (signed char)((event>>16)&0xFF);
                   wheeld[i] <<= 20l;
                   wheeld[i] /= (long)beat;
                   if (wheeld[i] > 0)
                     wheeldest[i] = wheel[i] + ((unsigned char)((event>>9)&0x7F) << 1);
                   else
                     wheeldest[i] = wheel[i] - ((unsigned char)((event>>9)&0x7F) << 1);
                 }
                 /* wheel position */
                 else {
                   wheel[i] = (signed char)((event>>16)&0xFF);
                   wheelfxd[i] = (long)(wheel[i]) << 20l;
                   timenext = MIDIFILE_Pitch_Wheel (i, wheel[i], f, timenext);
                   if (wheeld[i]) {
                     wd--;
                     wheeld[i] = 0;
                   }
                 }
               }
               /* dynamics */
               else {
                /* immediate volumes */
                switch (event&0xE0000300l) {
                  /* dimmenuendo */
                  case 0x60000000l:
                    if (!vold[i])
                      vd++;
                    vold[i] = 1+(event>>10l)&0x7Fl;
                    vold[i] <<= 20l;
                    vold[i] /= (long)beat;
                    vold[i] = -vold[i];
                    break;
                  /* crescendo */
                  case 0x60000100l:
                    if (!vold[i])
                      vd++;
                    vold[i] = 1+(event>>10l)&0x7Fl;
                    vold[i] <<= 20l;
                    vold[i] /= (long)beat;
                    break;
                  /* vol levels */
                  case 0x60000200l:
                    switch ((event>>10)&0x7) {
                      case 0:
                        vol[i] = PIANISSIMO;
                        volfxd[i] = (long)PIANISSIMO << 20l;
                        timenext = MIDIFILE_Set_Volume (i, PIANISSIMO, f, timenext);
                        break;
                      case 1:
                        vol[i] = PPIANO;
                        volfxd[i] = (long)PPIANO << 20l;
                        timenext = MIDIFILE_Set_Volume (i, PPIANO, f, timenext);
                        break;
                      case 2:
                        vol[i] = PIANO;
                        volfxd[i] = (long)PIANO << 20l;
                        timenext = MIDIFILE_Set_Volume (i, PIANO, f, timenext);
                        break;
                      case 3:
                        vol[i] = MEZZOPIANO;
                        volfxd[i] = (long)MEZZOPIANO << 20l;
                        timenext = MIDIFILE_Set_Volume (i, MEZZOPIANO, f, timenext);
                        break;
                      case 4:
                        vol[i] = MEZZOFORTE;
                        volfxd[i] = (long)MEZZOFORTE << 20l;
                        timenext = MIDIFILE_Set_Volume (i, MEZZOFORTE, f, timenext);
                        break;
                      case 5:
                        vol[i] = FORTE;
                        volfxd[i] = (long)FORTE << 20l;
                        timenext = MIDIFILE_Set_Volume (i, FORTE, f, timenext);
                        break;
                      case 6:
                        vol[i] = FFORTE;
                        volfxd[i] = (long)FFORTE << 20l;
                        timenext = MIDIFILE_Set_Volume (i, FFORTE, f, timenext);
                        break;
                      case 7:
                        vol[i] = FORTISSIMO;
                        volfxd[i] = (long)FORTISSIMO << 20l;
                        timenext = MIDIFILE_Set_Volume (i, FORTISSIMO, f, timenext);
                    }
                    if (vold[i]) {
                      vd--;
                      vold[i] = 0;
                    }
                    break;
                  case 0x60000300l:
                    vol[i] = (event>>10)&0x7F;
                    volfxd[i] = (long)(vol[i]) << 20l;
                    timenext = MIDIFILE_Set_Volume (i, vol[i], f, timenext);
                    if (vold[i]) {
                      vd--;
                      vold[i] = 0;
                    }
                }
               }
               break;
              /* bar */
              case 0x80000000l:
                memcpy (accident[i], key[i], 7);
                lastitem[i] = 0;
                switch (event&0x7) {
                  /* end repeat */
                  case 3: if (ending[i]) {
                            cur[i][0] = repeat[i][0];
                            cur[i][1] = repeat[i][1];
                            repeatnum[i]++;
                          }
                          else {
                            note = ((event>>4)&0xF) + 1;
                            if (note == 1) note++;
                            if (repeatnum[i] < note) {
                              cur[i][0] = repeat[i][0];
                              cur[i][1] = repeat[i][1];
                              repeatnum[i]++;
                            }
                            else {
                              repeat[i][0] = cur[i][0];
                              repeat[i][1] = cur[i][1];
                              repeatnum[i] = 1;
                              ending[i] = 0;
                            }
                          }
                          break;
                  /* begin repeat */
                  case 5: repeat[i][0] = cur[i][0];
                          repeat[i][1] = cur[i][1];
                          repeatnum[i] = 1;
                          ending[i] = 0;
                }
                break;
              /* control */
              case 0xA0000000l:
                switch ((event>>8)&7) {
                  case 0: if (!dc[i]) {
                            cur[i][0] = cur[i][1] = 0;
                            memcpy (accident[i], key[i], 7);
                            dc[i] = -1;
                          }
                          /* commented out to avoid infinite loop... */
                          /*else
                            dc[i] = 0;*/
                          break;
                  case 1: if (!ds[i]) {
                            cur[i][0] = _segno[i][0];
                            cur[i][1] = _segno[i][1];
                            memcpy (accident[i], key[i], 7);
                            ds[i] = -1;
                          }
                          else
                            ds[i] = 0;
                          break;
                  case 2: _segno[i][0] = cur[i][0];
                          _segno[i][1] = cur[i][1];
                          break;
                  case 3: if (cur[i][0] == _coda[i][0] && cur[i][1] == _coda[i][1]) {
                            nextevent = getending (-1, cur[i][1] + 1, cur[i][0]);
                            cur[i][0] = nextevent&0xFFFFl;
                            cur[i][1] = nextevent>>16l;
                            memcpy (accident[i], key[i], 7);
                            /* is this voice done ? */
                            if ((nextevent&0xFFFFl) > hipage ||
                               ((nextevent&0xFFFFl) == hipage && (nextevent>>16l) >= NUMCOL)) {
                              play[i] = 0; playing--;
                            }
                            ds[i] = dc[i] = 0;
                          }
                          else {
                            _coda[i][0] = cur[i][0];
                            _coda[i][1] = cur[i][1];
                          }
                          break;
                  case 4: if (cur[i][0] == fine[i][0] && cur[i][1] == fine[i][1]) {
                            play[i] = 0; playing--;
                          }
                          else {
                            fine[i][0] = cur[i][0];
                            fine[i][1] = cur[i][1];
                          }
                          break;
                  case 7: if (i > 5)
                            if ((event>>8)&8) {
                              if (!(lastnote[i]&0x80)) {
                                timenext = MIDIFILE_Key_Off (i,f,timenext);
                              }
                              MIDIFILE_Set_Percussion (percussion = -1);
                            }
                            else {
                              if (!(lastnote[i]&0x80)) {
                                timenext = MIDIFILE_Key_Off (i,f,timenext);
                              }
                              MIDIFILE_Set_Percussion (percussion = 0);
                            }
                }
                break;
              /* endings */
              case 0xC0000000l:
                ending[i] = -1;
                nextevent = getending (repeatnum[i], cur[i][1], cur[i][0]);
                cur[i][0] = nextevent&0xFFFFl;
                cur[i][1] = nextevent>>16l;
                if (repeatnum[i] != 1)
                  memcpy (accident[i], key[i], 7);
                break;
              /* etc... */
              case 0xE0000000l:
                switch (event&0xE00E0000l) {
                  /* time signature */
                  case 0xE0000000l:
                    tsig = (event>>8)&0xFF;
                    {
                      int nn, dd;
                      /* cut time */
                      if ((tsig&0x1F) == 0 && ((tsig>>5)&7) == 7) {
                        nn = 2; dd = 1;
                      }
                      /* standard time */
                      else if ((tsig&0x1F) == 0) {
                        nn = 4; dd = 2;
                      }
                      /* other */
                      else {
                        nn = (tsig&0x1F)+1;
                        dd = (tsig>>5)&7;
                      }
                      midibytes[0] = 0xff;
                      midibytes[1] = 0x58;
                      midibytes[2] = 0x04;
                      midibytes[3] = nn;
                      midibytes[4] = dd;
                      midibytes[5] = 48;
                      midibytes[6] = 8;
                      writevarlen (timenext,f);
                      timenext = 0;
                      fwrite (midibytes, 7, 1, f);
                    }
                    break;
                  /* accelerando */
                  case 0xE0020000l:
                    tmpod = 1+(event>>8l)&0xFFl;
                    tmpod <<= 20l;
                      /* cut time */
                    if ((tsig&0x1F) == 0 && ((tsig>>5)&7) == 7)
                      tmpod /= (long)(2*npb[1]);
                      /* standard time */
                    else if ((tsig&0x1F) == 0)
                      tmpod /= (long)(4*npb[2]);
                      /* other */
                    else
                      tmpod /= (long)(((tsig&0x1F)+1)*npb[(tsig>>5)&7]);
                    break;
                  /* ritardando */
                  case 0xE0040000l:
                    tmpod = 1+(event>>8l)&0xFFl;
                    tmpod <<= 20l;
                      /* cut time */
                    if ((tsig&0x1F) == 0 && ((tsig>>5)&7) == 7)
                      tmpod /= (long)(2*npb[1]);
                      /* standard time */
                    else if ((tsig&0x1F) == 0)
                      tmpod /= (long)(4*npb[2]);
                      /* other */
                    else
                      tmpod /= (long)(((tsig&0x1F)+1)*npb[(tsig>>5)&7]);
                    tmpod = -tmpod;
                    break;
                  /* tempo */
                  case 0xE0060000l:
                    tmpod = 0;
                    tmpo = (event>>8)&0x1FF;
                    tmpofxd = ((unsigned long)tmpo) << 20;
                    beat = npb[(event>>20)&0xF];
                    if (i == 0) {
                      spt = ((long)tmpo*(long)beat)/60l;
                      if (spt == 0) spt = 1;
                      else if (spt > 1024) spt = 1024;
                      spt = 96000000l/spt;
                      if (spt > 16777215l)
                      spt = 16777215l;
                      midibytes[0] = 0xff;
                      midibytes[1] = 0x51;
                      midibytes[2] = 0x03;
                      midibytes[3] = (spt>>16l)&0xFF;
                      midibytes[4] = (spt>>8l)&0xFF;
                      midibytes[5] = (spt)&0xFF;
                      writevarlen (timenext,f);
                      timenext = 0;
                      fwrite (midibytes, 6, 1, f);
                    }
                    break;
                  /* program change */
                  case 0xE0080000l:
                    /* percussion bank change */
                    if (event&0x00008000) {
                      if (i == 6 && percussion)
                        timenext = MIDIFILE_Set_DrumSet ((event>>8)&0x7F, f, timenext);
                    }
                    else if (!percussion || i < 6) {
                      char *tmpv2;
                      unsigned tmpv3 = (event>>20)&0x7F;
                      tmpv = (event>>8)&0x7F;
                      writevarlen (timenext, f);
                      timenext = 0;
                      midibytes[0] = 0xff;
                      midibytes[1] = 0x04;
                      fwrite (midibytes, 2, 1, f);
                      tmpv2 = getinstrumentname (tmpv3,tmpv);
                      writevarlen (strlen(tmpv2), f);
                      fwrite (tmpv2,strlen(tmpv2),1,f);
                      timenext = MIDIFILE_Set_Instrument (i, tmpv3, tmpv, f, timenext);
                    }
                    break;
                  /* pan */
                  case 0xE00A0000l:
                    keypan[i] = 0;
                    pan[i] = (signed char)((event>>8)&0xFF);
                    panfxd[i] = (long)(pan[i]) << 20l;
                    timenext = MIDIFILE_Set_Pan (i, pan[i], f, timenext);
                    if (pand[i]) {
                      pd--;
                      pand[i] = 0;
                    }
                    break;
                  /* fermata */
                  case 0xE00C0000l:
                    fermata[i] = ((event>>8)&0xFF) + 105;
                    break;
                  /* stereo fade */
                  case 0xE00E0000l:
                    keypan[i] = 0;
                    if (!pand[i])
                      pd++;
                    pand[i] = (signed char)((event>>8)&0xFF);
                    pand[i] <<= 20l;
                    pand[i] /= (long)beat;
                }
            }
          }
        } while (!found && play[i]);
        nextevent = event;
        event &= 0x7F;
        note = (event/7)*12 + MIDINote[event%7] + accident[i][event%7];
        if (!(lastnote[i]&0x80) &&
           ((nextevent&0xE0004080l) != 0x20004000l ||
            tienote[i] != (nextevent&0x7F) ||
            (lastitem[i] && lastnote[i] != note))) {
          timenext = MIDIFILE_Key_Off (i,f,timenext);
        }
        if (found) {
          /* event is a rest */
          if (nextevent & 0x80) {
            timeleft[i] = npb[(nextevent>>8)&0xF];
            /* if note is triplet */
            if (nextevent&0x1000l)
              timeleft[i] = (timeleft[i]<<1)/3;
            /* if note has fermata */
            if (fermata[i]) {
              timeleft[i] = ((long)timeleft[i]*(long)fermata[i])/100;
              fermata[i] = 0;
            }
            note = 0x80;
          }
          /* event is a note */
          else {
            if (note > 127) note -= 12;
            timeleft[i] = npb[(nextevent>>8)&0xF];
            /* if note is triplet */
            if (nextevent&0x1000l)
              timeleft[i] = (timeleft[i]<<1)/3;
            /* if note has fermata */
            if (fermata[i]) {
              timeleft[i] = ((long)timeleft[i]*(long)fermata[i])/100;
              fermata[i] = 0;
            }
            /* if note stoccato */
            if (nextevent&0x8000l) {
              stoccato[i] = timeleft[i];
              timeleft[i] = (3*timeleft[i])>>2;
              stoccato[i] -= timeleft[i];
            }
            /* if note is accented */
            if (nextevent&0x2000l) {
              accent[i] = -1;
              if ((vol[i]*120)/100 > FORTISSIMO) {
                timenext = MIDIFILE_Set_Volume (i, FORTISSIMO, f, timenext);
              }
              else {
                timenext = MIDIFILE_Set_Volume (i, (vol[i]*120)/100, f, timenext);
              }
            }
            if (!(nextevent&0x4000l) ||
                tienote[i] != (nextevent&0x7F) ||
                (lastitem[i] && lastnote[i] != note)) {
              if (keypan[i]) {
                int n = (keypandata[i]>>8)&0x7f, r = (keypandata[i]&0x3f)+1;
                if (note >= n+r)
                  pan[i] = 127;
                else if (note <= n-r)
                  pan[i] = -128;
                else
                  pan[i] = ((note-n)<<7)/r;
                if (pan[i] > 127) pan[i] = 127;
                else if (pan[i] < -128) pan[i] = -128;
                timenext = MIDIFILE_Set_Pan (i, pan[i], f, timenext);
                panfxd[i] = pan[i] << 20l;
              }
              timenext = MIDIFILE_Key_On (i, note, f, timenext);
              cur[i][2] = cur[i][0];
              cur[i][3] = cur[i][1];
              tie[i] = 0;
            }
            else
              tie[i] = -1;
            /* keep track of position on clef (vs. actual pitch) for ties */
            tienote[i] = nextevent&0x7F;
          }
          lastitem[i] = 0;
          if (!tie[i])
            lastnote[i] = note;
        }
      }
      /* update pause time */
      if (play[i])
        /* volume or tempo still changing, update it every tick */
        if (vd || pd || wd || tmpod)
          count = 1;
        else
          count = timeleft[i];
      timenext += count;
    }
    writevarlen (timenext, f);
    timenext = 0;
    midibytes[0] = 0xff;
    midibytes[1] = 0x2f;
    midibytes[2] = 0;
    fwrite (midibytes, 3, 1, f);
    trklen[i][1] = ftell(f) - trklen[i][0];
    trklen[i][0] -= 4;
  }
  fflush (f);
  /* seek in the file to correct track lengths (now that they are known) */
  for (i = 0; i < NUMVOICES; i++) {
    fseek (f, trklen[i][0], SEEK_SET);
    fputc ((trklen[i][1]>>24l)&0xFF, f);
    fputc ((trklen[i][1]>>16l)&0xFF, f);
    fputc ((trklen[i][1]>>8l)&0xFF, f);
    fputc ((trklen[i][1])&0xFF, f);
  }
  /* close file */
  fclose (f);
  /* re-set file out stuff (since it also changes local vars used by
     FM/MIDI routines) */
  MIDIFILE_Reset();
  /* done */
  draw_col();
  return;
}
