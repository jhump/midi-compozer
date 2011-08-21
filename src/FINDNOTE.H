/* FILE: findnote.h
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains prototypes for routines in findnote.c
 */

/* returns column number of previous note or -1 if it hits beginning of
   page without a note or if it hits a rest */
int getprevtie(int voice, int column);

/* for redrawing notes: return column of next note if it is tied, otherwise
   return -1 (stops at end of page) */
int getnexttie(int voice, int column);

/* if next note is on this page or next note (on any following pages) is not
   tied then returns -1 */
int nextpagetie(int voice, int column);

/* trace back for previous note - if it is greater than an 8th note (no flags
   or beams) or if a bar is hit first return -1, otherwise return column */
int getprevbeam(int voice, int column);

/* same as above but searches forward instead */
int getnextbeam(int voice, int column);

/* searches back and returns _value_ of previous note or accidental; returns
   -1 if no prior notes found; if flag then it will return value of key
   signature accidentals... otherwise it will keep searching back */
unsigned long getprevnote(int voice, int column, int flag);

/* searches back and returns _value_ of previous rest; returns
   -1 if no prior rests found; */
unsigned long getprevrest(int voice, int column);

/* returns column in high word and page in low word of next action */
unsigned long getnextitem(int voice, int column, unsigned pg);

/* finds ending with ending number and returns column in high word and page
   in low word; returns -1 if no ending found (end of song) - if ending < 0
   then searches forward for a coda instead */
unsigned long getending(int ending, int column, unsigned pg);

/* returns value at colpg (where high word is column and low word is page)
   for voice... if colpg is invalid (beyond end of song) it returns 0 (which
   is a NULL event) */
unsigned long getevent(int voice, unsigned long colpg);
