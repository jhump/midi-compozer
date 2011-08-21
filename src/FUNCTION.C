/* FILE: function.c
 * AUTHOR: Josh Humphries
 * PURPOSE:
 *    This is a composition program that saves to its own .AEM notation
 * file format but will export to MIDI file format as well. It runs in
 * 640x480x256 video mode via appropriate VESA compatible hardware...
 * It uses an OPL3 (sound blaster pro 2.0 & sound blaster 16) FM synth
 * for playback or OPL2 if that's all ya got... This file contains the
 * guts of the program... other routines and drivers are in other source
 * files and the main() routine is in main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <mem.h>
#include <conio.h>
#include <string.h>
#include <ctype.h>
#include "fm.h"
#include "gr.h"
#include "ems.h"
#include "keys.h"
#include "data.h"
#include "findnote.h"
#include "getfile.h"

/* externs that aren't in header files... */
extern void export_midi (void);
extern void playsong(int beginpage, int begincol, int endpage, int endcol);

/* constants */
#define TOPY 		32
#define LOWY 		445
#define HEIGHT 		(LOWY-TOPY+1)
#define NUMCOL		75
#define NUMVOICES 	10
#define XBEGIN 		5
#define XEND   		(MAXX-5)
#define COLOFFS 	(XBEGIN+30)
#define PAGES		(4096/(NUMCOL*NUMVOICES))

#define VERSION         "1.23"
#define COPYRIGHT       "2000"

typedef struct __menu {
  char name[34], shortcut;
  int flag, *change, submenuitems;
  struct __menu *submenu;
  void (*proc)(void);
} menu;

typedef struct {
  int key;
  void (*proc)(void);
  int *change;
} hotkey;

/* local routines */
void display_startup(void);
int initEMS(void);
void load_config(void);
void save_config(void);
int load_instrum(void);
void change_palette(void);
void display_startup(void);
void redraw_column(int column);
void patch_staves(int column);
void redraw_music(int begin, int end);
void redraw_info(void);
int save_changes(void);
void cline (int x1, int y1, int x2, int y2, char color);
void draw_nsymbol(int x, int y, char *data, char color);
void draw_csymbol(int x, int y, char *data, char color);
void draw_col(void);
void undraw_col(void);
void drawrow_nvoice(char v, int begin, int end);
void drawrow_cvoice(char v, int begin, int end);
void undrawrow_nvoice(char v, int begin, int end);
void undrawrow_cvoice(char v, int begin, int end);
void draw_voice(int column, char voice, int staffy, char color,
		void (*plot)(int x, int y, char color),
		void (*drawline)(int x1, int y1, int x2, int y2, char color),
		void (*drawfont)(int font, int x, int y, char *str, char color, int sp),
		void (*drawcurve)(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, char color));
void draw_nvoice(int column, char voice, int staffy, char color);
void draw_cvoice(int column, char voice, int staffy, char color);
void undraw_nvoice(int column, char voice, int staffy);
void undraw_cvoice(int column, char voice, int staffy);
void redraw_staff(char staff, int begin, int end);
void drawstaff (char staff, int begin, int end);
void enter_editmode(void);
void exit_editmode(void);
int fontlength(int font, char *str);
void fontout(int font, int x, int y, char *str, char color, int sp);
void cfontout(int font, int x, int y, char *str, char color, int sp);
void unfontout(int font, int x, int y, char *str, char color, int sp);
void draw_symbol(int x, int y, char *data, char color,
		 void (*plot)(int x, int y, char color));
void Message(char *str);
int YesNo(char *s);
void BrowseFile(char *name);
void getkeysig(int col);
void writekeysig(void);
void getaccidentals(char voice);
int _tempo (int *t);
int _timesig (int *t);
int isblank(int voice, int column);
int ISCONTROL(unsigned long i);
int iscontrol(int column);
	/* actions started by builtin key actions */
void navigate_menu(void);
char *getinstrumentname(int bank, int instr);
void getinstrument(int def);
void getdrumset(int def);
int getval(char *s, int begin, int end, int def, int percent, int lev);
void getkeypan(void);
int getnote (void);
void getlev(int pv);
void getfade(void);
void getdelta(int t, int d);
void getwheel(int d);
void gethold(void);
void gettempo(void);
void gettimesig(void);
	/* actions started by menu option or hot key */
void redraw_screen(void);
void file_info(void);
void new_song(void);
void open_song(void);
int save_song(void);
int saveas(void);
void quit(void);
void undo(void);
void cut(void);
void copy(void);
void paste(void);
void clear(void);
void transpose(void);
void playback(void);
void playscreen(void);
void playfromscreen(void);
void playfrompos(void);
void default_tempo(void);
void default_timesig(void);
void voice_setup(void);
void staff_setup(void);
void color_select(void);
void about(void);
void view_help(void);
void view_keys(void);
void play_perc(void);

/* globals for color settings */
unsigned char  bg = 15, fg = 0, mg = 7, keyb = 0, wkey = 15, bkey = 0,
	       wkeyp = 12, bkeyp = 12, fgp = 9, mgp = 1, selbg = 0, selfg = 15,
	       selmg = 8, menubg = 7, menufg = 0, menubgh = 8, menufgh = 15,
	       col = 3;
unsigned char *colors[] =
{ &bg, &fg, &mg, &keyb, &wkey, &bkey, &wkeyp, &bkeyp, &fgp, &mgp, &selbg,
  &selfg, &selmg, &menubg, &menufg, &menubgh, &menufgh, &col };
/* EMS memory handle and number of pages allocated */
int EMShandle[2], mappedpage = 0;
unsigned pages = 1;
/* edit controls */
int editmode = 0, markbegin, markend, voice[NUMVOICES], undovoice[NUMVOICES],
    clipbvoice[NUMVOICES], undopos, undosize = 0, undopage, clipboardsize;
unsigned long *clipboard, *undobuffer;
/* song display and playback variables */
int opl3 = 0, gmidi = 0, upd = -1, updk = -1, numstaves = 4, curpage = 0,
    curvoice = 0, curpos = 0, curstaff = 1, curkey[7] = {0, 0, 0, 0, 0, 0, 0},
    changed = 0, curstaffy = ((HEIGHT+37)/5) + TOPY - 37, hipage = 0,
    drawlines = 0, drawcontrol = 0, _sound = -1, timesig = 0x40,
    tempo[2] = {119,2}, lastinstr = 0, mute[NUMVOICES] = {0,0,0,0,0,0,0,0,0,0};
signed char staffinfo[16] = {1,0,7,0,1,0,7,0,1,0,7,0,1,0,7,0},
	    voiceinfo[NUMVOICES] = {1,1,1,2,2,2,3,3,4,4};
unsigned char colbuf[2*HEIGHT];
char filename[81] = "", composer[21] = "", opus[13] = "",
     title[41] = "Untitled", *clefs[9] = {"French Violin", "Treble", "Soprano",
     "Mezzo-Soprano", "Alto", "Tenor", "Barritone", "Bass", "Sub-Bass"};
unsigned long *song, *page;
/* globals for menu structure */
menu filesubmenu[8] =
{ {"New                        Ctrl-N", 0, 0, NULL, 0, NULL, new_song},
  {"Open                  F3   Ctrl-O", 0, 0, NULL, 0, NULL, open_song},
  {"Save                  F2   Ctrl-S", 0, 0, NULL, 0, NULL, save_song},
  {"Save As                    Ctrl-A", 5, 0, NULL, 0, NULL, saveas},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"Export MIDI                Ctrl-E", 0, 0, NULL, 0, NULL, export_midi},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"Exit               Alt-X   Ctrl-Q", 1, 0, NULL, 0, NULL, quit} },
     editsubmenu[7] =
{ {"Undo                       Ctrl-Z", 0, 0, NULL, 0, NULL, undo},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"Cut            Shift-Del   Ctrl-X", 2, 0, NULL, 0, NULL, cut},
  {"Copy            Ctrl-Ins   Ctrl-C", 0, 0, NULL, 0, NULL, copy},
  {"Paste          Shift-Ins   Ctrl-V", 0, 0, NULL, 0, NULL, paste},
  {"Clear          Backspace   Delete", 1, 0, NULL, 0, NULL, clear},
  {"Transpose                  Ctrl-T", 1, 0, NULL, 0, NULL, transpose} },
     playbacksubmenu[4] =
{ {"Playback                   Ctrl-P", 0, 0, NULL, 0, NULL, playback},
  {"Play Screen            Ctrl-Alt-S", 5, 0, NULL, 0, NULL, playscreen},
  {"Play From Screen     Ctrl-Shift-S", 5, 0, NULL, 0, NULL, playfromscreen},
  {"Play From Position     Ctrl-Alt-P", 1, 0, NULL, 0, NULL, playfrompos} },
     songsubmenu[5] =
{ {"Default Tempo               Alt-T", 8, 0, NULL, 0, NULL, default_tempo},
  {"Default Time Sig       Ctrl-Alt-T", 0, 0, NULL, 0, NULL, default_timesig},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"Staff Setup            F5   Alt-A", 0, 0, NULL, 0, NULL, staff_setup},
  {"Voice Setup            F6   Alt-V", 0, 0, NULL, 0, NULL, voice_setup} },
     optionssubmenu[7] =
{ {"Change Colors          F4   Alt-C", 0, 0, NULL, 0, NULL, color_select},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"Use YMF262/OPL3             Alt-Y", 4, 16, &opl3, 0, NULL, NULL},
  {"Use MPU-401 General MIDI    Alt-G", 4, 25, &gmidi, 0, NULL, NULL},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"Update Playback Music       Alt-U", 0, 22, &upd, 0, NULL, NULL},
  {"Update Playback Keyboard    Alt-K", 1, 25, &updk, 0, NULL, NULL} },
     miscsubmenu[5] =
{ {"Redraw Screen              Ctrl-R", 0, 0, NULL, 0, NULL, redraw_screen},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"File Information           Ctrl-F", 0, 0, NULL, 0, NULL, file_info},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"Percussion List            Ctrl-L", 0, 0, NULL, 0, NULL, play_perc} },
     helpsubmenu[4] =
{ {"View Help             F1   Ctrl-H", 5, 0, NULL, 0, NULL, view_help},
  {"Keystroke List             Ctrl-K", 0, 0, NULL, 0, NULL, view_keys},
  {"", 0, 0, NULL, -1, NULL, NULL},
  {"About                    Shift-F1", 0, 0, NULL, 0, NULL, about} },
     mainmenu[7] =
{ {"File", 0, 0, NULL, 8, filesubmenu, NULL},
  {"Edit", 0, 0, NULL, 7, editsubmenu, NULL},
  {"Playback", 0, 0, NULL, 4, playbacksubmenu, NULL},
  {"Song", 0, 0, NULL, 5, songsubmenu, NULL},
  {"Options", 0, 0, NULL, 7, optionssubmenu, NULL},
  {"Miscellaneous", 0, 0, NULL, 5, miscsubmenu, NULL},
  {"Help", 0, 0, NULL, 4, helpsubmenu, NULL} };
unsigned menubar[7] = { 6, 64, 122, 212, 270, 352, 602 };
hotkey hkeys[43] =
{ {0x0431, new_song, NULL}, {0x0418, open_song, NULL},
  {0x003D, open_song, NULL}, {0x041F, save_song, NULL},
  {0x003C, save_song, NULL}, {0x041E, saveas, NULL},
  {0x0412, export_midi, NULL}, {0x0410, quit, NULL},
  {0x022D, quit, NULL}, {0x042C, undo, NULL}, {0x042D, cut, NULL},
  {0x0153, cut, NULL}, {0x042E, copy, NULL}, {0x0452, copy, NULL},
  {0x042F, paste, NULL}, {0x0152, paste, NULL}, {0x000E, clear, NULL},
  {0x0414, transpose, NULL}, {0x0419, playback, NULL},
  {0x061F, playscreen, NULL}, {0x051F, playfromscreen, NULL},
  {0x0619, playfrompos, NULL}, {0x0214, default_tempo, NULL},
  {0x0614, default_timesig, NULL}, {0x021E, staff_setup, NULL},
  {0x003F, staff_setup, NULL}, {0x0040, voice_setup, NULL},
  {0x022F, voice_setup, NULL}, {0x022E, color_select, NULL},
  {0x003E, color_select, NULL}, {0x0423, view_help, NULL},
  {0x003B, view_help, NULL}, {0x0425, view_keys, NULL},
  {0x0216, NULL, &upd}, {0x0225, NULL, &updk}, {0x0215, NULL, &opl3},
  {0x013B, about, NULL}, {0x0413, redraw_screen, NULL},
  {0x0421, file_info, NULL}, {0x0222, NULL, &gmidi},
  {0x0426, play_perc, NULL} };
/* miscellaneous */
int quitflag = 0, menucur, submenucur, menudown = 0;
char undrawbuf[29440];
char ASCIItoScan[128] =
{ 0x03, 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,
  0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C, 0x1A, 0x2B, 0x1B, 0x07, 0x0C,
  0x39, 0x02, 0x28, 0x04, 0x05, 0x06, 0x08, 0x28, 0x0A, 0x0B, 0x09, 0x0D, 0x33, 0x0C, 0x34, 0x35,
  0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x27, 0x27, 0x33, 0x0D, 0x34, 0x35,
  0x03, 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,
  0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C, 0x1A, 0x2B, 0x1B, 0x07, 0x0C,
  0x29, 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,
  0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C, 0x1A, 0x2B, 0x1B, 0x29, 0x0E},
     check[8] = {0x01,0x02,0x02,0x04,0x8C,0xD8,0x70,0x30};
char DrumNames[1152], *Drums[128];
/* converting our notes to MIDI notes */
char MIDINote[7] = {0,2,4,5,7,9,11};
/* writing note names to info bar */
char NoteName[7][2] = {"C","D","E","F","G","A","B"}, AccName[5][3] = {"bb","b","","#","x"};

/****************************************************************************/

void getkeysig(int col) {
  unsigned i,j,k;
  memset (curkey,0,14);
  for (i = 0; i < col; i++)
    if ((page[i*NUMVOICES] & 0xE1000800l) == 0x40000800l) {
      j = (page[i*NUMVOICES]>>8)&0x7;
      k = page[i*NUMVOICES]&0x7F;
      switch (j) {
	case 0: curkey[k%7] = -1; break;
	case 1: curkey[k%7] = -2; break;
	case 2: curkey[k%7] = 1; break;
	case 3: curkey[k%7] = 2; break;
	default:curkey[k%7] = 0;
      }
    }
  return;
}

/****************************************************************************/

void writekeysig(void) {
  int i,j,pos=0;
  /* first sharps in order */
  for (i=0,j=3;i<7;i++,j=(j+4)%7)
    if (curkey[j] == 1)
      page[(pos++)*NUMVOICES] = 0x40000A00l | (j + 35);
  /* then double-sharps in order */
  for (i=0,j=3;i<7;i++,j=(j+4)%7)
    if (curkey[j] == 2)
      page[(pos++)*NUMVOICES] = 0x40000B00l | (j + 35);
  /* then flats in order */
  for (i=0,j=6;i<7;i++,j=(j+3)%7)
    if (curkey[j] == -1)
      page[(pos++)*NUMVOICES] = 0x40000800l | (j + 35);
  /* then double-flats in order */
  for (i=0,j=6;i<7;i++,j=(j+3)%7)
    if (curkey[j] == -2)
      page[(pos++)*NUMVOICES] = 0x40000900l | (j + 35);
  return;
}

/****************************************************************************/

void getaccidentals(char voice) {
  int i, accident[7];
  unsigned j,k;
  unsigned long x;
  for (i=0;i<7;i++) curkey[i] = accident[i] = 0;
  for (i = 0; i < curpos; i++)
    if (((x = page[i*NUMVOICES]) & 0xE0000000l) == 0x80000000l)
      memcpy (accident, curkey, 14);
    else if ((x & 0xE1000800l) == 0x40000800l) {
      j = (x>>8)&0x7;
      k = (x&0x7F) % 7;
      switch (j) {
        case 0: curkey[k] = accident[k] = -1; break;
        case 1: curkey[k] = accident[k] = -2; break;
        case 2: curkey[k] = accident[k] = 1; break;
        case 3: curkey[k] = accident[k] = 2; break;
        default:curkey[k] = accident[k] = 0;
      }
    }
    else if (((x = page[i*NUMVOICES+voice]) & 0xE1000000l) == 0x40000000l) {
      j = (x>>8)&0x7;
      k = (x&0x7F) % 7;
      switch (j) {
        case 0: accident[k] = -1; break;
        case 1: accident[k] = -2; break;
        case 2: accident[k] = 1; break;
        case 3: accident[k] = 2; break;
        default:accident[k] = 0;
      }
    }
  for (i = 0; i < 7; i++) curkey[i] = accident[i];
  return;
}

/****************************************************************************/

int load_instrum(void) {
  FILE *f; char tmp[4]; int i;
  if ((f = fopen ("genmidi.ibk", "rb")) == NULL)
    return -1;
  fread (tmp, 4, 1, f);
  if (memcmp (tmp, "IBK\032", 4)) {
    fclose (f);
    return -2;
  }
  if (fread (&GeneralMIDI, 128*sizeof(FM_Instrument), 1, f) != 1) {
    fclose (f);
    return -2;
  }
  fclose (f);
  if ((f = fopen ("drums.ibk", "rb")) == NULL)
    return -3;
  fread (tmp, 4, 1, f);
  if (memcmp (tmp, "IBK\032", 4)) {
    fclose (f);
    return -4;
  }
  if (fread (&Percussion, 128*sizeof(FM_Instrument), 1, f) != 1) {
    fclose (f);
    return -4;
  }
  if (fread (DrumNames, 1152, 1, f) != 1) {
    fclose (f);
    return -4;
  }
  for (i = 0; i < 128; i++)
    Drums[i] = DrumNames + 9*i;
  fclose (f);
  return 0;
}

/****************************************************************************/

void change_palette(void) {
  char pal[16][3] =
{{0, 0, 0}, {0, 0, 168}, {0, 168, 0}, {0, 168, 168}, {168, 0, 0}, {168, 0, 168},
{168, 84, 0}, {168, 168, 168}, {84, 84, 84}, {84, 84, 252}, {84, 252, 84},
{84, 252, 252}, {252, 84, 84}, {252, 84, 252}, {252, 252, 84}, {252, 252, 252}};
  int i, j;

  for (j = 0; j < 4; j++) {
    /* first sixteen palette entries: standard VGA palette */
    for (i = 0; i < 16; i++)
      setrgbpalette ((j<<6)+i, (pal[i][0])>>2, (pal[i][1])>>2, (pal[i][2])>>2);
    /* next eight entries: grayscale */
    for (i = 1; i <= 8; i++)
      setrgbpalette ((j<<6)+i+15, (i*63)>>3, (i*63)>>3, (i*63)>>3);
    /* next four entries: shades of red */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+23, (i*63)>>2, 0, 0);
    /* next four entries: shades of green */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+27, 0, (i*63)>>2, 0);
    /* next four entries: shades of blue */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+31, 0, 0, (i*63)>>2);
    /* next four entries: shades of yellow */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+35, (i*63)>>2, (i*63)>>2, 0);
    /* next four entries: shades of cyan */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+39, 0, (i*63)>>2, (i*63)>>2);
    /* next four entries: shades of magenta */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+43, (i*63)>>2, 0, (i*63)>>2);
    /* next four entries: shades of orange */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+47, (i*63)>>2, (i*63)>>3, 0);
    /* next four entries: shades of flourescent green */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+51, 0, (i*63)>>2,(i*63)>>3);
    /* next four entries: shades of indigo */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+55, (i*63)>>3, 0, (i*63)>>2);
    /* next four entries: shades of fuchsia */
    for (i = 1; i <= 4; i++)
      setrgbpalette ((j<<6)+i+59, (i*63)>>2, 0, (i*63)>>3);
  }
  return;
}

/****************************************************************************/

void display_startup(void) {
  int i;
  box (0,0,MAXX,MAXY,1);
  if (_sound) {
    /* prep sound card */
    if (gmidi)
      UseMIDI();
    else if (opl3)
      UseOPL3();
    else
      UseOPL2();
    FM_Set();
    FM_Set_Percussion(-1);
    FM_Set_Instrument (0,0,56);
    FM_Set_Instrument (1,0,16);
    FM_Set_Instrument (2,0,16);
    FM_Set_Instrument (3,0,16);
    FM_Set_Instrument (4,0,16);
    FM_Set_Instrument (5,0,36);
    for (i = 1; i < 5; i++) FM_Set_Volume (i,90);
    FM_Set_Volume (0,100);
    FM_Set_Volume (5,115);
    FM_Set_Volume (6,110);
    FM_Set_Volume (7,95);
    FM_Set_Volume (8,80);
    FM_Set_Volume (9,90);
    /* bring on the noise */
    FM_Key_On (0,53);
    FM_Key_On (1,41);
    FM_Key_On (2,45);
    FM_Key_On (3,51);
    FM_Key_On (4,55);
    FM_Key_On (5,29);
    FM_Key_On (SNARE, 40);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (CYMBAL, 49);
  }
  fontout (0,((MAXX-fontlength(0,"Apriori Enterprises"))>>1)+3,61,"Apriori Enterprises",0,0);
  fontout (0,(MAXX-fontlength(0,"Apriori Enterprises"))>>1,58,"Apriori Enterprises",15,0);
  fontout (1,((MAXX-fontlength(1,"presents"))>>1)+2,84,"presents",0,0);
  fontout (1,(MAXX-fontlength(1,"presents"))>>1,82,"presents",9,0);
  if (_sound) {
    delay(150);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (0,55);
    delay(150);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (0,57);
    FM_Key_On (5,29);
    delay(150);
    FM_Key_On (0,55);
    FM_Key_On (1,43);
    FM_Key_On (2,48);
    FM_Key_On (3,52);
    FM_Key_On (4,55);
    FM_Key_On (5,24);
    FM_Key_On (BASS, 35);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (CYMBAL, 49);
  }
  line (50,150,50,320,33);
  line (51,150,51,319,33);
  line (52,150,52,318,33);
  line (53,150,MAXX-50,150,33);
  line (53,151,MAXX-51,151,33);
  line (53,152,MAXX-52,152,33);
  line (MAXX-50,151,MAXX-50,320,35);
  line (MAXX-51,152,MAXX-51,320,35);
  line (MAXX-52,153,MAXX-52,320,35);
  line (50,320,MAXX-53,320,35);
  line (51,319,MAXX-53,319,35);
  line (52,318,MAXX-53,318,35);
  if (_sound) {
    delay(150);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (0,60);
    delay(150);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (0,64);
    FM_Key_On (5,24);
    delay(150);
    FM_Key_On (0,63);
    FM_Key_On (1,42);
    FM_Key_On (2,47);
    FM_Key_On (3,51);
    FM_Key_On (4,54);
    FM_Key_On (5,23);
    FM_Key_On (SNARE, 40);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (CYMBAL, 49);
  }
  draw_symbol (54,141,_MIDI,0,putpixel);
  draw_symbol (49,136,_MIDI,14,putpixel);
  draw_symbol (310,142,_Compozer,0,putpixel);
  draw_symbol (305,137,_Compozer,11,putpixel);
  box (68,264,581,267,0);
  box (63,260,577,263,15);
  if (_sound) {
    delay (600);
    FM_Key_On (SNARE, 37);
    delay (100);
    FM_Key_On (SNARE, 37);
    delay (100);
    FM_Key_On (SNARE, 37);
    delay (100);
    for (i = 0; i < 9; i++) FM_Set_Volume (i,120);
    FM_Key_On (0,59);
    FM_Key_On (BASS, 35);
    FM_Key_On (CYMBAL, 49);
    FM_Key_On (HIHAT, 42);
    FM_Key_On (5,23);
  }
  fontout (2,((MAXX-fontlength(2,"Copyright (c) "COPYRIGHT" Apriori Enterprises"))>>1)+2,287,"Copyright (c) "COPYRIGHT" Apriori Enterprises",0,0);
  fontout (2,(MAXX-fontlength(2,"Copyright (c) "COPYRIGHT" Apriori Enterprises"))>>1,285,"Copyright (c) "COPYRIGHT" Apriori Enterprises",7,0);
  fontout (2,((MAXX-fontlength(2,"All Rights Reserved"))>>1)+2,300,"All Rights Reserved",0,0);
  fontout (2,(MAXX-fontlength(2,"All Rights Reserved"))>>1,298,"All Rights Reserved",7,0);
  fontout (1,((MAXX-fontlength(1,"Press any key to continue."))>>1)+2,402,"Press any key to continue...",0,0);
  fontout (1,(MAXX-fontlength(1,"Press any key to continue."))>>1,400,"Press any key to continue...",15,0);
  if (_sound) {
    delay (100);
    /* stop the noise */
    for (i = 0; i < 11; i++) FM_Key_Off (i);
  }
  /* wait for a keypress... */
  getxkey();
  /* reset FM synth... */
  if (_sound) FM_Reset();
  return;
}

/****************************************************************************/

void load_config(void) {
  FILE *f;
  int  buf[13];

  if ((f = fopen ("compozer.cfg", "rb")) == NULL) {
    save_config();
    return;
  }
  if (fread (buf, 26, 1, f) != 1) {
    fclose (f);
    save_config();
    return;
  }
  else {
    memcpy (&bg, buf, 18);
    opl3 = buf[9];
    gmidi = buf[10];
    upd = buf[11];
    updk = buf[12];
  }
  fclose (f);
  return;
}

/****************************************************************************/

void save_config(void) {
  FILE *f;

  if (f = fopen ("compozer.cfg", "wb")) {
    fwrite (&bg, 18, 1, f);
    fwrite (&opl3, 2, 1, f);
    fwrite (&gmidi, 2, 1, f);
    fwrite (&upd, 2, 1, f);
    fwrite (&updk, 2, 1, f);
    fclose (f);
  }
  return;
}

/****************************************************************************/

void color_select(void) {
  char ch = 0, captions[18][52] =
{ "                              Background Color  -",
  "                 Active Voice Foreground Color  -",
  "              Inactive Voices Foreground Color  -",
  "                   Playback Piano Border Color  -",
  "                Playback Piano Idle White Keys  -",
  "                Playback Piano Idle Black Keys  -",
  "             Playback Piano Pressed White Keys  -",
  "             Playback Piano Pressed Black Keys  -",
  "                   Active Voice Playback Color  -",
  "                Inactive Voices Playback Color  -",
  "                Selected Area Background Color  -",
  "   Selected Area Active Voice Foreground Color  -",
  "Selected Area Inactive Voices Foreground Color  -",
  "                     Menu Bar Background Color  -",
  "                     Menu Bar Foreground Color  -",
  "         Highlighted Menu Bar Background Color  -",
  "         Highlighted Menu Bar Foreground Color  -",
  "                     Position Indicator Column  -" };
  int  i, cur = 0, old = 0; char tmp[4];

  box (0,0,MAXX,MAXY,7);
  for (i = 0; i < 18; i++) {
    outtextxy (30, 23*i + 19, captions[i], 0);
    outtextxy (29, 23*i + 18, captions[i], 15);
    line (440, 23*i + 13, 540, 23*i + 13, 0);
    line (440, 23*i + 33, 540, 23*i + 33, 0);
    line (440, 23*i + 13, 440, 23*i + 33, 0);
    line (540, 23*i + 13, 540, 23*i + 33, 0);
    box (441, 23*i + 14, 539, 23*i + 32, *(colors[i]));
    sprintf (tmp, "%02d", *(colors[i]));
    outtextxy (482, 23*i + 19, tmp, 0);
  }
  outtextxy (30, 19, captions[0], 15);
  outtextxy (29, 18, captions[0], 4);
  line (440, 13, 540, 13, 12);
  line (440, 33, 540, 33, 12);
  line (440, 13, 440, 33, 12);
  line (540, 13, 540, 33, 12);
  outtextxy (33, 450, "Choose Category to Change - \030,\031      \033,\032 - Change Current Color", 0);
  outtextxy (32, 449, "Choose Category to Change -              - Change Current Color", 14);
  outtextxy (32, 449, "                            \030,\031      \033,\032", 10);
  outtextxy (89, 461, "Abort Changes - <escape>      <enter> - Accept and Save Changes", 0);
  outtextxy (88, 460, "Abort Changes -                       - Accept and Save Changes", 14);
  outtextxy (88, 460, "                <escape>      <enter>", 10);
  while (ch != 27 && ch != 13) {
    ch = getkey();
    switch (ch) {
      case 0x1B : load_config(); break;
      case 0x0D : save_config(); break;
      case 0x00 : ch = getkey();
	switch (ch) {
	  case 0x48 : if (cur > 0)
			cur--;
		      else
			cur = 17;
		      break;
	  case 0x50 : if (cur < 17)
			cur++;
		      else
			cur = 0;
		      break;
	  case 0x4B :
	  case 0x4D : if (ch == 0x4B) (*(colors[cur]))--;
		      else (*(colors[cur]))++;
		      (*(colors[cur])) &= 0x3f;
		      box (441, 23*cur + 14, 539, 23*cur + 32, *(colors[cur]));
		      sprintf (tmp, "%02d", *(colors[cur]));
		      outtextxy (482, 23*cur + 19, tmp, 0);
	}
	if (cur != old) {
	  outtextxy (30, 23*old + 19, captions[old], 0);
	  outtextxy (29, 23*old + 18, captions[old], 15);
	  line (440, 23*old + 13, 540, 23*old + 13, 0);
	  line (440, 23*old + 33, 540, 23*old + 33, 0);
	  line (440, 23*old + 13, 440, 23*old + 33, 0);
	  line (540, 23*old + 13, 540, 23*old + 33, 0);
	  old = cur;
	  outtextxy (30, 23*cur + 19, captions[cur], 15);
	  outtextxy (29, 23*cur + 18, captions[cur], 4);
	  line (440, 23*cur + 13, 540, 23*cur + 13, 12);
	  line (440, 23*cur + 33, 540, 23*cur + 33, 12);
	  line (440, 23*cur + 13, 440, 23*cur + 33, 12);
	  line (540, 23*cur + 13, 540, 23*cur + 33, 12);
	}
    }
  }
  redraw_screen();
  return;
}

/****************************************************************************/

void Message(char *str) {
  int xw, xs, xe, ys = (MAXY-36)>>1, ye = (MAXY+36)>>1,
      yw = ye-ys+1;
  xw = (strlen(str) > 28)?
       (strlen(str)<<3):(28<<3);
  if (xw > 619) { xs = 10; xe = MAXX-10; }
  else { xs = (MAXX-xw)>>1; xe = (MAXX+xw)>>1; }
  getbox (undrawbuf, xs-10, ys, xe-xs+21, yw);
  box (xs-9, ys+1, xe+9, ye-1, menubg);
  line (xs-10, ys, xe+10, ys, menufg);
  line (xs-10, ye, xe+10, ye, menufg);
  line (xs-10, ys, xs-10, ye, menufg);
  line (xe+10, ys, xe+10, ye, menufg);
  outtextxy ((MAXX-(strlen(str)<<3))>>1,ys+3,str,menufg);
  outtextxy ((MAXX-(28<<3))>>1,ye-11,"Press any key to continue...",menufg);
  getxkey();
  putbox (undrawbuf, xs-10, ys, xe-xs+21, yw);
  return;
}

/****************************************************************************/

void file_info(void) {
  char st[3][41], stl[3] = {40,20,12}, tmp[31], ch,
       *n[3] = {"Title:","Composer:","Description"};
  int chx, i, j, draw = 0, blink = 0, bc = menubg, k;

  strcpy (st[0], title); strcpy (st[1], composer); strcpy (st[2], opus);
  box (81,173,558,306,menubg);
  line (80,172,80,307,menufg);
  line (559,172,559,307,menufg);
  line (80,172,559,172,menufg);
  line (80,307,559,307,menufg);
  outtextxy (88,178,"File Name:",menufg);
  if (filename[0])
    outtextxy (320-(strlen(filename)<<2),186,filename,menufg);
  else
    outtextxy (320-(strlen("<none>")<<2),186,"<none>",menufg);
  sprintf (tmp, "Number of Pages: %d", hipage+1);
  outtextxy (88,202,tmp,menufg);
  for (i=0;i<3;i++) {
    outtextxy (88,218+i*24,n[i],menufg);
    outtextxy (320-(strlen(st[i])<<2),226+i*24,st[i],menufg);
  }
  box (88,218,88+(strlen(n[0])<<3),225,menubgh);
  outtextxy (88,218,n[0],menufgh);
  outtextxy (88,294,"\030,\031 - Cycle Field    <Enter> - Accept    <Escape> - Cancel", menufg);
  i = j = 0;
  do {
    if (kbhit())
      chx = getxkey();
    else
      chx = 0;
    /* cursor blinking */
    if (blink == 0) {
      blink = 20;
      if (bc == menufg) bc = menubg;
      else bc = menufg;
      line (320+(strlen(st[i])<<2),233+i*24,328+(strlen(st[i])<<2),233+i*24,bc);
    }
    switch (chx&0xff) {
      case 0x48:if (i==0) i = 2; else i--; break;
      case 0x50:if (i==2) i = 0; else i++; break;
      default:
        ch = ASCIIcode(chx);
        if (ch >= 0x20) {
          draw = -1;
          if (strlen(st[i]) < stl[i]) {
            st[i][k=strlen(st[i])] = ch;
            st[i][k+1] = 0;
          }
          else
            st[i][strlen(st[i])-1] = ch;
        }
        else if (ch == 8 && st[i][0] != 0) {
          draw = -1;
          st[i][strlen(st[i])-1] = 0;
        }
    }
    if (i != j) {
      box (88,218+j*24,88+(strlen(n[j])<<3),225+j*24,menubg);
      outtextxy (88,218+j*24,n[j],menufg);
      line (320+(strlen(st[j])<<2),233+j*24,328+(strlen(st[j])<<2),233+j*24,menubg);
      j = i;
      box (88,218+i*24,88+(strlen(n[i])<<3),225+i*24,menubgh);
      outtextxy (88,218+i*24,n[i],menufgh);
      line (320+(strlen(st[i])<<2),233+i*24,328+(strlen(st[i])<<2),233+i*24,bc);
    }
    if (draw) {
      box (81,226+i*24,558,233+i*24,menubg);
      outtextxy (320-(strlen(st[i])<<2),226+i*24,st[i],menufg);
      draw = 0;
    }
    delay (10); /* delay 100th of a second so that the cursor will blink
                   at the proper rate (~5 times a second) */
    blink--;
  } while ((chx&0xfff) != 0x01 && (chx&0xfff) != 0x1C);
  if ((chx&0xfff) == 0x1C) {
    if (strcmp (title, st[0]) || strcmp (composer, st[1]) || strcmp (opus,st[2]))
      changed = -1;
    strcpy (title, st[0]); strcpy (composer, st[1]); strcpy (opus, st[2]);
  }
  redraw_screen();
  return;
}

/****************************************************************************/

void new_song(void) {
  if (changed && save_changes())
    return;
  if ((pages = EMSrealloc (EMShandle[0], 1)) == 0xFFFF) {
    VESAQuit();
    if (_sound) FM_Reset();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    unloadkeyboardISR();
    unloadErrorHandler();
    fprintf (stderr, "compozer: EMS reallocation error\n");
    _exit (1);
  }
  if (EMSmap (EMShandle[0],0,0) == -1) {
    VESAQuit();
    if (_sound) FM_Reset();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    unloadkeyboardISR();
    unloadErrorHandler();
    fprintf (stderr, "compozer: EMS mapping error\n");
    _exit (1);
  }
  mappedpage = 0;
  memset (song, 0, 16384);
  /* set everything to defaults */
  numstaves = 4;
  timesig = 0x40; tempo[0] = 119; tempo[1] = 2; hipage = 0;
  staffinfo[0] = 1; staffinfo[1] = 0;
  staffinfo[2] = 7; staffinfo[3] = 0;
  staffinfo[4] = 1; staffinfo[5] = 0;
  staffinfo[6] = 7; staffinfo[7] = 0;
  staffinfo[8] = 1; staffinfo[9] = 0;
  staffinfo[10] = 7; staffinfo[11] = 0;
  staffinfo[12] = 1; staffinfo[13] = 0;
  staffinfo[14] = 7; staffinfo[15] = 0;
  voiceinfo[0] = 1; voiceinfo[1] = 1; voiceinfo[2] = 1;
  voiceinfo[3] = 2; voiceinfo[4] = 2; voiceinfo[5] = 2;
  voiceinfo[6] = 3; voiceinfo[7] = 3;
  voiceinfo[8] = 4; voiceinfo[9] = 4;
  strcpy(filename, "");
  strcpy(composer, ""); strcpy(opus, ""); strcpy(title, "Untitled");
  curpage = 0; curvoice = 0; curpos = 0;
  curstaff = 1; memset (curkey, 0, 7);
  changed = 0; curstaffy = ((HEIGHT+37)/5) + TOPY - 37;
  page = song; editmode = 0; undosize = 0;
  /* redraw (blank) music */
  box (0,TOPY,MAXX,LOWY,bg);
  redraw_music(0, NUMCOL-1); redraw_info();
  draw_col();
  return;
}

/****************************************************************************/

void BrowseFile(char *name) {
  char buf[77];
  long lines = 0, ch, cur = 0, old = 0, i;
  FILE *f;
  /* open the file */
  if ((f = fopen (name, "rb")) == NULL) {
    sprintf (buf, "Error opening file %s!", name);
    Message (buf);
    return;
  }
  /* read it in */
  for (lines = 0; fgets(buf,77,f) != NULL && lines < 1024; lines++) {
    if ((file[lines] = malloc(strlen(buf)+1)) == NULL) {
      /* on error, notify user and free already allocated memory */
      sprintf (buf, "Not enough memory to load file %s!", name);
      Message (buf);
      for (;lines > 0; lines--)
	free(file[lines-1]);
      return;
    }
    if (buf[strlen(buf)-1] == '\n' || buf[strlen(buf)-1] == '\r') {
      buf[strlen(buf)-1] = 0;
      if (buf[strlen(buf)-1] == '\n' || buf[strlen(buf)-1] == '\r')
        buf[strlen(buf)-1] = 0;
    }
    for (i = 0; buf[i]; i++)
      if (buf[i] == '\t' || buf[i] == '\a' || buf[i] == '\b' ||
          buf[i] == '\v' || buf[i] == '\n' || buf[i] == '\r')
        buf[i] = ' ';
    strcpy (file[lines], buf);
  }
  fclose (f);
  /* display it */
  box (0,0,MAXX,MAXY,menubgh);
  line (11,11,11,MAXY-27,menufgh);
  line (11,11,MAXX-11,11,menufgh);
  line (MAXX-11,11,MAXX-11,MAXY-27,menufgh);
  line (11,MAXY-27,MAXX-11,MAXY-27,menufgh);
  outtextxy (13,MAXY-17,"<escape>,<enter> - Done          \030,\031,<PgUp>,<PgDn>,<Home>,<End> - Scroll File",menufgh);
  /* the actual text of the file */
  box (12,12,MAXX-12,MAXY-28,menubg);
  for (i = 0; i < ((cur+54>lines)?(lines-cur):(54)); i++)
    outtextxy (16,(i<<3)+16,file[i+cur],menufg);
  /* indicators on the side to tell how far into the file we are */
  box (0,cur*441/lines + 11,10,(cur+53)*441/lines + 11,menufgh);
  box (MAXX-10,cur*441/lines + 11,MAXX,(cur+53)*441/lines + 11,menufgh);
  /* process user input */
  do {
    switch (ch = getxkey()&0xFF) {
      case 0x48: if (cur > 0) cur--; break;
      case 0x50: if (cur < lines-53) cur++; break;
      case 0x47: cur = 0; break;
      case 0x4F: cur = lines-53; break;
      case 0x49: if (cur > 54) cur-=54; else cur = 0; break;
      case 0x51: if (cur < lines-108) cur+=54; else cur = lines-53;
    }
    /* update displayed text if cursor moved */
    if (cur != old) {
      box (12,12,MAXX-12,MAXY-28,menubg);
      for (i = 0; i < ((cur+54>lines)?(lines-cur):(54)); i++)
        outtextxy (16,(i<<3)+16,file[i+cur],menufg);
      box (0,old*441/lines + 11,10,(old+53)*441/lines + 11,menubgh);
      box (MAXX-10,old*441/lines + 11,MAXX,(old+53)*441/lines + 11,menubgh);
      old = cur;
      box (0,cur*441/lines + 11,10,(cur+53)*441/lines + 11,menufgh);
      box (MAXX-10,cur*441/lines + 11,MAXX,(cur+53)*441/lines + 11,menufgh);
    }
  } while (ch != 0x01 && ch != 0x1C);
  /* when user quits, free memory allocated for file */
  for (;lines > 0; lines--)
    free(file[lines-1]);
  /* redraw everything */
  redraw_screen();
  return;
}

/****************************************************************************/

void open_song(void) {
  FILE *f; char ID[12], *fname; int i; unsigned _pages;
  int _numstaves, _timesig, _tempo[2], _hipage;
  char _staffinfo[16], _voiceinfo[NUMVOICES], _composer[21],
       _opus[13], _title[41];
  if (changed && save_changes())
    return;
  if ((fname = GetFileName(-1,".AEM")) == NULL)
    return;
  if ((f = fopen (fname, "rb")) == NULL) {
    Message ("Error opening file for reading!");
    return;
  }
  fread (ID, 12, 1, f);
  if (memcmp (ID, "CompozAEM", 10) != 0) {
    Message ("Invalid data in file!");
    fclose (f);
    return;
  }
  if (fread (&_numstaves, 2, 1, f) != 1 || fread (&_timesig, 2, 1, f) != 1 ||
      fread (_tempo, 4, 1, f) != 1 || fread (&_hipage, 2, 1, f) != 1 ||
      fread (_staffinfo, 16, 1, f) != 1 || fread (_voiceinfo, NUMVOICES, 1, f) != 1 ||
      fread (_composer, 21, 1, f) != 1 || fread (_opus, 13, 1, f) != 1 ||
      fread (_title, 41, 1, f) != 1) {
    Message ("Error reading data from file!");
    fclose (f);
    return;
  }
  if ((_pages = EMSrealloc (EMShandle[0], (_hipage/PAGES)+1)) == 0xFFFF ||
      (_pages < (_hipage/PAGES)+1 && _pages < pages)) {
    VESAQuit();
    if (_sound) FM_Reset();
    unloadkeyboardISR();
    unloadErrorHandler();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    fprintf (stderr, "compozer: EMS reallocation error\n");
    _exit (1);
  }
  else if (_pages < (_hipage/PAGES)+1) {
    Message ("Not enough EMS memory to load song!");
    fclose (f);
    return;
  }
  for (i = 0; i < _pages; i++) {
    if (EMSmap (EMShandle[0], 0, i) == -1) {
      VESAQuit();
      if (_sound) FM_Reset();
      unloadkeyboardISR();
      unloadErrorHandler();
      EMSfree (EMShandle[0]);
      EMSfree (EMShandle[1]);
      fprintf (stderr, "compozer: EMS mapping error\n");
      _exit (1);
    }
    memset (song, 0, 16384);
    if (fread (song, 4, NUMVOICES*NUMCOL*PAGES, f) != NUMVOICES*NUMCOL*PAGES) {
      Message ("Unexpected end of file!");
      if (i != mappedpage && EMSmap (EMShandle[0], 0, mappedpage) == -1) {
	VESAQuit();
        if (_sound) FM_Reset();
	unloadkeyboardISR();
        unloadErrorHandler();
	EMSfree (EMShandle[0]);
	EMSfree (EMShandle[1]);
	fprintf (stderr, "compozer: EMS mapping error\n");
	_exit (1);
      }
    }
  }
  fclose (f);
  numstaves = _numstaves; timesig = _timesig;
  memcpy (tempo,_tempo,4); hipage = _hipage; pages = _pages;
  memcpy (staffinfo,_staffinfo,16); memcpy (voiceinfo,_voiceinfo,NUMVOICES);
  strcpy (composer,_composer); strcpy (opus,_opus); strcpy (title,_title);
  curpage = 0; curvoice = 0; curpos = 0; curstaff = voiceinfo[0];
  memset (curkey, 0, 7); changed = 0; mappedpage = 0;
  curstaffy = curstaff*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(curstaff<<1)-1];
  page = song; editmode = 0;
  strcpy(filename, fname);
  if (EMSmap (EMShandle[0], 0, 0) == -1) {
    VESAQuit();
    if (_sound) FM_Reset();
    unloadkeyboardISR();
    unloadErrorHandler();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    fprintf (stderr, "compozer: EMS mapping error\n");
    _exit (1);
  }
  /* redraw music */
  box (0,TOPY,MAXX,LOWY,bg);
  redraw_music(0, NUMCOL-1); redraw_info();
  draw_col();
  return;
}

/****************************************************************************/

int save_song(void) {
  FILE *f; char ID[12] = "CompozAEM\00\00"; int i;
  if (filename[0] == 0)
    return saveas();
  if ((f = fopen (filename, "wb")) == NULL) {
    Message ("Error opening file for writing!");
    return -1;
  }
  if (fwrite (ID, 12, 1, f) != 1 || fwrite (&numstaves, 2, 1, f) != 1 ||
      fwrite (&timesig, 2, 1, f) != 1 || fwrite (tempo, 4, 1, f) != 1 ||
      fwrite (&hipage, 2, 1, f) != 1 || fwrite (staffinfo, 16, 1, f) != 1 ||
      fwrite (voiceinfo, NUMVOICES, 1, f) != 1 || fwrite (composer, 21, 1, f) != 1 ||
      fwrite (opus, 13, 1, f) != 1 || fwrite (title, 41, 1, f) != 1) {
    Message ("Error writing data to file!");
    fclose (f);
    return -1;
  }
  for (i = 0; i < pages; i++) {
    if (EMSmap (EMShandle[0], 0, i) == -1) {
      VESAQuit();
      if (_sound) FM_Reset();
      unloadkeyboardISR();
      unloadErrorHandler();
      EMSfree (EMShandle[0]);
      EMSfree (EMShandle[1]);
      fprintf (stderr, "compozer: EMS mapping error\n");
      _exit (1);
    }
    if (fwrite (song, 4, NUMVOICES*NUMCOL*PAGES, f) != NUMVOICES*NUMCOL*PAGES) {
      Message ("Error writing data to file!");
      fclose (f);
      if (i != mappedpage && EMSmap (EMShandle[0], 0, mappedpage) == -1) {
	VESAQuit();
        if (_sound) FM_Reset();
	unloadkeyboardISR();
        unloadErrorHandler();
	EMSfree (EMShandle[0]);
	EMSfree (EMShandle[1]);
	fprintf (stderr, "compozer: EMS mapping error\n");
	_exit (1);
      }
      return -1;
    }
  }
  fclose (f);
  if (EMSmap (EMShandle[0], 0, mappedpage) == -1) {
    VESAQuit();
    if (_sound) FM_Reset();
    unloadkeyboardISR();
    unloadErrorHandler();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    fprintf (stderr, "compozer: EMS mapping error\n");
    _exit (1);
  }
  changed = 0;
  return 0;
}

/****************************************************************************/

int saveas(void) {
  char *fname; FILE *tmp; int i;
  if ((fname = GetFileName(0,".AEM")) == NULL)
    return -1;
  if ((tmp = fopen(fname,"r")) != NULL) {
    fclose (tmp);
    if ((i=YesNo ("Overwrite Existing File ?")) == -1 || i == 0)
      return -1;
  }
  strcpy (filename, fname);
  return save_song();
}

/****************************************************************************/


void quit(void) {
  if (changed && save_changes())
    return;
  quitflag = -1;
  return;
}

/****************************************************************************/

void undo(void) {
  unsigned long *tmp1 = undobuffer + 0x0400,
		*tmp2 = song + (undopage%PAGES)*NUMVOICES*NUMCOL + undopos*NUMVOICES;
  int i, j;
  /* if undobuffer empty there's nothing to do... */
  if (undosize == 0)
    return;
  if (undopage/PAGES != mappedpage && EMSmap(EMShandle[0], 0, undopage/PAGES) == -1) {
    VESAQuit();
    if (_sound) FM_Reset();
    unloadkeyboardISR();
    unloadErrorHandler();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    fprintf (stderr, "compozer: EMS mapping error\n");
    _exit (1);
  }
  mappedpage = undopage/PAGES;
  /* copy data to be overwritten into tmp1 (redo buffer which will afterwards
     be copied to undo buffer (i.e. undobuffer[>0x800]) */
  memcpy (tmp1, tmp2, undosize*4*NUMVOICES);
  /* copy data from undo buffer to position in song */
  for (i = 0; i < undosize; i++)
    for (j = 0; j < NUMVOICES; j++)
      if (undovoice[j])
        if (ISCONTROL(undobuffer[i*NUMVOICES+j]))
          tmp2[i*NUMVOICES] = undobuffer[i*NUMVOICES];
        else
          tmp2[i*NUMVOICES+j] = undobuffer[i*NUMVOICES+j];
  /* move data from redo buffer to undo buffer */
  memcpy (undobuffer, tmp1, undosize*4*NUMVOICES);
  /* remap EMS to currently displayed page */
  if (curpage/PAGES != mappedpage && EMSmap(EMShandle[0], 0, curpage/PAGES) == -1) {
    VESAQuit();
    if (_sound) FM_Reset();
    unloadkeyboardISR();
    unloadErrorHandler();
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    fprintf (stderr, "compozer: EMS mapping error\n");
    _exit (1);
  }
  mappedpage = curpage/PAGES;
  /* if we modified the currently displayed page or if we
     were in edit mode (in which case we unselected everything),
     redisplay it */
  if (editmode || undopage/PAGES == curpage/PAGES) {
    box (0,TOPY,MAXX,LOWY,bg);
    memset (colbuf, bg, 2*HEIGHT);
    editmode = 0;
    redraw_music(0, NUMCOL-1); redraw_info();
    draw_col();
  }
  changed = -1;
  return;
}

/****************************************************************************/

void cut(void) {
  unsigned long *tmp = page + markbegin*NUMVOICES;
  int i, j;
  /* if nothing is selected, nothing to do... */
  if (!editmode)
    return;
  /* save the stuff in the undo buffer and to the clipboard */
  undosize = clipboardsize = markend - markbegin + 1;
  undopage = curpage; undopos = markbegin;
  memcpy (undovoice, voice, NUMVOICES*2);
  memcpy (clipbvoice, voice, NUMVOICES*2);
  memcpy (undobuffer, tmp, undosize*4*NUMVOICES);
  memcpy (clipboard, tmp, clipboardsize*4*NUMVOICES);
  /* clear it out to zero */
  for (i = 0; i < clipboardsize; i++)
    for (j = 0; j < NUMVOICES; j++)
      if (voice[j])
	tmp[i*NUMVOICES+j] = 0;
  /* unselect area and redisplay the music since it is likely modified */
  editmode = 0;
  box (0,TOPY,MAXX,LOWY,bg);
  memset (colbuf, bg, 2*HEIGHT);
  redraw_music(0, NUMCOL-1); redraw_info();
  draw_col();
  changed = -1;
  return;
}

/****************************************************************************/

void copy(void) {
  unsigned long *tmp = page + markbegin*NUMVOICES;
  /* if nothing is selected, nothing to do... */
  if (!editmode)
    return;
  /* save the stuff in the undo buffer and copy to the clipboard */
  undosize = clipboardsize = markend - markbegin + 1;
  undopage = curpage; undopos = markbegin;
  memcpy (undovoice, voice, NUMVOICES*2);
  memcpy (clipbvoice, voice, NUMVOICES*2);
  memcpy (undobuffer, tmp, undosize*4*NUMVOICES);
  memcpy (clipboard, tmp, clipboardsize*4*NUMVOICES);
  return;
}

/****************************************************************************/

void paste(void) {
  int v = 0, i, j, k, l, begin = (editmode)?(markbegin):(curpos),
      end = (editmode) ?
            ((clipboardsize + begin > markend + 1) ? (markend + 1) : (clipboardsize+begin)):
            ((clipboardsize + begin > NUMCOL) ? (NUMCOL) : (clipboardsize+begin));
  unsigned long *tmp = page + begin*NUMVOICES;
  /* if clipboard is empty, nothing to do... */
  if (clipboardsize == 0)
    return;
  for (i = 0; i < NUMVOICES; i++)
    if (clipbvoice[i]) v++;
  /* single voice - allows to copy from one voice and paste to another */
  if (v == 1) {
    for (i = 0; i < NUMVOICES; i++)
      if (clipbvoice[i]) v = i;
    memset (voice, 0, NUMVOICES*2);
    voice[v] = -1;
    /* save the stuff in the undo buffer */
    undosize = end - begin;
    undopage = curpage; undopos = begin;
    memset (undovoice, 0, NUMVOICES*2);
    undovoice[curvoice] = -1;
    memcpy (undobuffer, tmp, undosize*4*NUMVOICES);
    /* paste data to page */
    for (i = 0; i < undosize; i++)
      if (ISCONTROL(clipboard[i*NUMVOICES])) {
        for (k = l = 0; k < NUMVOICES; k++)
          if (k != curvoice && (tmp[i*NUMVOICES+k]&0xE0000000l)) l++;
        if (l == 0)
          undobuffer[i*NUMVOICES+curvoice] = tmp[i*NUMVOICES] = clipboard[i*NUMVOICES];
      }
      else
        tmp[i*NUMVOICES+curvoice] = clipboard[i*NUMVOICES+v];
  }
  /* multi-voice - copy from numerous but paste goes to same voies */
  else {
    for (i = NUMVOICES; i > 0; i--)
      if (clipbvoice[i-1]) v = i;
    /* save the stuff in the undo buffer */
    undosize = end - begin;
    undopage = curpage; undopos = begin;
    memcpy (undovoice, clipbvoice, NUMVOICES*2);
    memcpy (undobuffer, tmp, undosize*4*NUMVOICES);
    /* paste data to page */
    for (i = 0; i < undosize; i++)
      for (j = 0; j < NUMVOICES; j++)
	if (clipbvoice[j])
          if (ISCONTROL(clipboard[i*NUMVOICES])) {
            for (k = l = 0; k < NUMVOICES; k++)
              if (!(clipbvoice[k]) && (tmp[i*NUMVOICES+k]&0xE0000000l)) l++;
            if (l == 0)
              undobuffer[i*NUMVOICES+v] = tmp[i*NUMVOICES] = clipboard[i*NUMVOICES];
          }
          else
            tmp[i*NUMVOICES+j] = clipboard[i*NUMVOICES+j];
  }
  /* unselect area if one is selected and redisplay music */
  editmode = 0;
  box (0,TOPY,MAXX,LOWY,bg);
  redraw_music(0, NUMCOL-1); redraw_info();
  draw_col();
  changed = -1;
  return;
}

/****************************************************************************/

void clear(void) {
  unsigned long *tmp = page + markbegin*NUMVOICES;
  int i, j;
  /* if nothing is selected, nothing to do... */
  if (!editmode)
    return;
  /* save the stuff in the undo buffer */
  undosize = markend - markbegin + 1;
  undopage = curpage; undopos = markbegin;
  memcpy (undovoice, voice, NUMVOICES*2);
  memcpy (undobuffer, tmp, undosize*4*NUMVOICES);
  /* clear it out to zero */
  for (i = 0; i < undosize; i++)
    for (j = 0; j < NUMVOICES; j++)
      if (voice[j])
	tmp[i*NUMVOICES+j] = 0;
  /* unselect area and redisplay the music since it is likely modified */
  editmode = 0;
  box (0,TOPY,MAXX,LOWY,bg);
  memset (colbuf, bg, 2*HEIGHT);
  redraw_music(0, NUMCOL-1); redraw_info();
  draw_col();
  changed = -1;
  return;
}

/****************************************************************************/

void transpose(void) {
  unsigned long *tmp = page + markbegin*NUMVOICES, l, *tmp2;
  int i, j, octaves = 0, oldoctaves = 0, accs = 0, oldaccs = 0, owkey = 0,
      oldowkey = 0, cur = 0, oldcur = 0, acc1[NUMVOICES][7], acc2[NUMVOICES][7],
      new, ofs[8] = {-1,-2,1,2,0,0,0,0}, rev[5] = {1,0,4,2,3}, note1, note2,
      st[12] = {0,1,1,2,2,3,4,4,5,5,6,6};
  signed char numsteps = 1, oldnsteps = 1, dif;
  char ch = 0, buf[10];
  if (!editmode)
    return;
  /* get info from user */
  getbox (undrawbuf,143,202,354,76);
  box (144,203,495,276,menubg);
  line (143,202,143,277,menufg);
  line (495,202,495,277,menufg);
  line (143,202,495,202,menufg);
  line (143,277,495,277,menufg);
  box (192,209,311,216,menubgh);
  outtextxy (192,209,"Number of Steps",menufgh);
  outtextxy (408,209,"1",menufg);
  outtextxy (192,219,"Each Step is",menufg);
  outtextxy (376,219,"Half-Step",menufg);
  outtextxy (192,229,"Transpose Accidentals",menufg);
  outtextxy (404,229,"No",menufg);
  outtextxy (192,239,"Overwrite Key Signature",menufg);
  outtextxy (404,239,"No",menufg);
  outtextxy (148,255,"    \030,\031 - Change Field     <Enter> - Accept",menufg);
  outtextxy (148,263,"+,-,\032,\033 - Change Value    <Escape> - Cancel",menufg);
  while (ch != 13 && ch != 27) {
    ch = getkey();
    if (ch == '-')
      switch (cur) {
        case 0: if (octaves) {
                  if (--numsteps == -11) numsteps = 10;
                }
                else
                  if (--numsteps == -121) numsteps = 120;
                if (numsteps == 0) numsteps--;
                break;
        case 1: octaves = !octaves; break;
        case 2: accs = !accs; break;
        case 3: owkey = !owkey; break;
      }
    else if (ch == '+')
      switch (cur) {
        case 0: if (octaves) {
                  if (++numsteps == 11) numsteps = -10;
                }
                else
                  if (++numsteps == 121) numsteps = -120;
                if (numsteps == 0) numsteps++;
                break;
        case 1: octaves = !octaves; break;
        case 2: accs = !accs; break;
        case 3: owkey = !owkey; break;
      }
    else if (ch == 0)
      switch (ch=getkey()) {
        case 72:
          if (cur == 0)
            cur = 3;
          else
            cur--;
          break;
        case 80:
          if (cur == 3)
            cur = 0;
          else
            cur++;
          break;
        case 75:
          switch (cur) {
            case 0: if (octaves) {
                      if (--numsteps == -11) numsteps = 10;
                    }
                    else
                      if (--numsteps == -121) numsteps = 120;
                    if (numsteps == 0) numsteps--;
                    break;
            case 1: octaves = !octaves; break;
            case 2: accs = !accs; break;
            case 3: owkey = !owkey; break;
          }
          break;
        case 77:
          switch (cur) {
            case 0: if (octaves) {
                      if (++numsteps == 11) numsteps = -10;
                    }
                    else
                      if (++numsteps == 121) numsteps = -120;
                    if (numsteps == 0) numsteps++;
                    break;
            case 1: octaves = !octaves; break;
            case 2: accs = !accs; break;
            case 3: owkey = !owkey; break;
          }
          break;
      }
    if (octaves != oldoctaves) {
      if (oldoctaves) strcpy (buf, "Octave");
      else strcpy (buf, "Half-Step");
      box (412-(strlen(buf)<<2),219,411+(strlen(buf)<<2),226,menubg);
      if (oldoctaves)
        numsteps *= 12;
      else {
        new = (120+numsteps)/12 - 10;
        if (new == 0 && numsteps < 0)
          numsteps = -1;
        else if (new == 0)
          numsteps = 1;
        else
          numsteps = new;
      }
      oldoctaves = octaves;
      if (octaves) strcpy (buf, "Octave");
      else strcpy (buf, "Half-Step");
      outtextxy (412-(strlen(buf)<<2),219,buf,menufg);
    }
    if (numsteps != oldnsteps) {
      sprintf (buf, "%d", oldnsteps);
      box (412-(strlen(buf)<<2),209,411+(strlen(buf)<<2),216,menubg);
      oldnsteps = numsteps;
      sprintf (buf, "%d", numsteps);
      outtextxy (412-(strlen(buf)<<2),209,buf,menufg);
    }
    else if (accs != oldaccs) {
      if (oldaccs) strcpy (buf, "Yes");
      else strcpy (buf, "No");
      box (412-(strlen(buf)<<2),229,411+(strlen(buf)<<2),236,menubg);
      oldaccs = accs;
      if (accs) strcpy (buf, "Yes");
      else strcpy (buf, "No");
      outtextxy (412-(strlen(buf)<<2),229,buf,menufg);
    }
    else if (owkey != oldowkey) {
      if (oldowkey) strcpy (buf, "Yes");
      else strcpy (buf, "No");
      box (412-(strlen(buf)<<2),239,411+(strlen(buf)<<2),246,menubg);
      oldowkey = owkey;
      if (owkey) strcpy (buf, "Yes");
      else strcpy (buf, "No");
      outtextxy (412-(strlen(buf)<<2),239,buf,menufg);
    }
    else if (cur != oldcur) {
      switch (oldcur) {
        case 0:
          box (192,209,311,216,menubg);
          outtextxy (192,209,"Number of Steps",menufg);
          break;
        case 1:
          box (192,219,287,226,menubg);
          outtextxy (192,219,"Each Step is",menufg);
          break;
        case 2:
          box (192,229,359,236,menubg);
          outtextxy (192,229,"Transpose Accidentals",menufg);
          break;
        case 3:
          box (192,239,375,246,menubg);
          outtextxy (192,239,"Overwrite Key Signature",menufg);
      }
      oldcur = cur;
      switch (cur) {
        case 0:
          box (192,209,311,216,menubgh);
          outtextxy (192,209,"Number of Steps",menufgh);
          break;
        case 1:
          box (192,219,287,226,menubgh);
          outtextxy (192,219,"Each Step is",menufgh);
          break;
        case 2:
          box (192,229,359,236,menubgh);
          outtextxy (192,229,"Transpose Accidentals",menufgh);
          break;
        case 3:
          box (192,239,375,246,menubgh);
          outtextxy (192,239,"Overwrite Key Signature",menufgh);
      }
    }
  }
  /* if enter pressed then do the math... */
  if (ch == 13) {
    /* save the stuff in the undo buffer */
    undosize = markend - markbegin + 1;
    undopage = curpage; undopos = markbegin;
    memcpy (undovoice, voice, NUMVOICES*2);
    memcpy (undobuffer, tmp, undosize*4*NUMVOICES);
    /* transpose it */
    memset (curkey, 0, 14);
    getkeysig (markbegin);
    for (new = 0; new < NUMVOICES; new++)
      for (dif = 0; dif < 7; dif++) {
        acc1[new][dif] = curkey[dif];
        acc2[new][dif] = curkey[dif];
      }
    for (i = 0; i < undosize; i++) {
      if ((*tmp&0xE1000800l) == 0x40000800l) {
        curkey[dif=(*tmp&0x7F)%7] = ofs[(*tmp&0x700)>>8];
        for (new = 0; new < NUMVOICES; new++)
          acc1[new][dif] = acc2[new][dif] = curkey[dif];
      }
      for (j = 0; j < NUMVOICES; j++, tmp++)
        if (voice[j])
          switch ((l=*tmp)&0xE0000000l) {
            case 0x20000000l:
              /* transpose note */
              if (!(l&0x80))
                if (octaves) {
                  new = (l&0x7F)/7;
                  new += numsteps;
                  if (new > 10)
                    new = 10;
                  else if (new < 0)
                    new = 0;
                  new *= 7;
                  new += (l&0x7F)%7;
                  if (new > 70)
                    new -= 7;
                  *tmp = (l&0xFFFFFF80) + new;
                }
                else if (owkey && i > 0 && isblank(j,markbegin+i-1)) {
                  tmp2 = tmp - NUMVOICES;
                  new = (l&0x7F)/7;
                  new += (numsteps+120)/12 - 10;
                  if (new > 10)
                    new = 10;
                  else if (new < 0)
                    new = 0;
                  new *= 7;
                  new += (l&0x7F)%7;
                  new = (new/7)*12 + MIDINote[new%7] + acc1[j][new%7];
                  new += (numsteps + 120)%12;
                  if (new > 120)
                    new -= 12;
                  note1 = new;
                  new = (new/12)*7 + st[new%12];
                  note2 = (new/7)*12 + MIDINote[new%7];
                  dif = note1 - note2;
                  if (dif > 2) dif = 2;
                  else if (dif < -2) dif = -2;
                  *tmp = (l&0xFFFFFF80) + new;
                  if (acc2[j][new%7] != dif) {
                    acc2[j][new%7] = dif;
                    tmp2 = tmp - NUMVOICES;
                    *tmp2 = 0x40000000 + new + (rev[dif+2] << 8);
                  }
                }
                else {
                  new = (l&0x7F)/7;
                  new += (numsteps+120)/12 - 10;
                  if (new > 10)
                    new = 10;
                  else if (new < 0)
                    new = 0;
                  new *= 7;
                  new += (l&0x7F)%7;
                  new += st[(numsteps+120)%12];
                  if (new > 70)
                    new -= 7;
                  *tmp = (l&0xFFFFFF80) + new;
                }
              break;
            case 0x40000000l:
              /* transpose accidental... */
              if (!(l&0x800) && !(l&0x01000000l)) {
                acc1[j][(l&0x7F)%7] = ofs[(l&0x700)>>8];
                if (octaves) {
                  if (accs) {
                    new = (l&0x7F)/7;
                    new += numsteps;
                    if (new > 10)
                      new = 10;
                    else if (new < 0)
                      new = 0;
                    new *= 7;
                    new += (l&0x7F)%7;
                    if (new > 70)
                      new -= 7;
                    *tmp = (l&0xFFFFFF80) + new;
                  }
                  acc2[j][(l&0x7F)%7] = ofs[(l&0x700)>>8];
                }
                else if (owkey)
                  *tmp = 0x0l;
                else if (accs) {
                  /* set type (sharp vs. flat vs. etc...) */
                  dif = ofs[(l&0x700)>>8] - curkey[(l&0x7F)%7];
                  if (dif > 2) dif = 2;
                  else if (dif < -2) dif = -2;
                  /* then set pitch of accidental */
                  new = (l&0x7F)/7;
                  new += (numsteps+120)/12 - 10;
                  if (new > 10)
                    new = 10;
                  else if (new < 0)
                    new = 0;
                  new *= 7;
                  new += (l&0x7F)%7;
                  new += st[(numsteps+120)%12];
                  if (new > 70)
                    new -= 7;
                  *tmp = (l&0xFFFFFF80l) + new;
                  l = *tmp;
                  if (acc2[j][new%7] != dif) {
                    acc2[j][new%7] = dif;
                    *tmp = (l&0xFFFFF8FFl) + (rev[dif+2] << 8);
                  }
                  else
                    *tmp = 0x0l;
                }
                else
                  acc2[j][(l&0x7F)%7] = ofs[(l&0x700)>>8];
              }
              break;
            /* bar (restore accidents) */
            case 0x80000000l:
              for (new = 0; new < NUMVOICES; new++)
                for (dif = 0; dif < 7; dif++) {
                  acc1[new][dif] = curkey[dif];
                  acc2[new][dif] = curkey[dif];
                }
          }
    }
    /* unselect area and redisplay the music since it is modified */
    editmode = 0;
    box (0,TOPY,MAXX,LOWY,bg);
    memset (colbuf, bg, 2*HEIGHT);
    redraw_music(0, NUMCOL-1); redraw_info();
    draw_col();
    changed = -1;
  }
  /* otherwise abort... */
  else
    putbox (undrawbuf,143,202,354,76);
  return;
}

/****************************************************************************/

void playback(void) {
  playsong (0,0,hipage,NUMCOL-1);
  return;
}

/****************************************************************************/

void playscreen(void) {
  playsong (curpage,0,curpage,NUMCOL-1);
  return;
}

/****************************************************************************/

void playfromscreen(void) {
  playsong (curpage,0,hipage,NUMCOL-1);
  return;
}

/****************************************************************************/

void playfrompos(void) {
  playsong (curpage,curpos,hipage,NUMCOL-1);
  return;
}

/****************************************************************************/

void voice_setup(void) {
  int y1, y2, i, j, chx, draw = 0, m[NUMVOICES];
  char tmp[30], ch, vi[NUMVOICES];

  memcpy (vi, voiceinfo, NUMVOICES);
  memcpy (m, mute, NUMVOICES*2);
  box (160,y1=240-((7+NUMVOICES)<<2),480,y2=240+((7+NUMVOICES)<<2),menubg);
  line (160,y1,480,y1,menufg);
  line (160,y2,480,y2,menufg);
  line (160,y1,160,y2,menufg);
  line (480,y1,480,y2,menufg);
  line (207,y1+23,433,y1+23,menufg);
  line (207,y1+24+(NUMVOICES<<3),433,y1+24+(NUMVOICES<<3),menufg);
  line (207,y1+23,207,y1+24+(NUMVOICES<<3),menufg);
  line (433,y1+23,433,y1+24+(NUMVOICES<<3),menufg);
  outtextxy (208,y1+8,"Voice Number    Staff Number",menufg);
  for (i=0;i<NUMVOICES;i++) {
    sprintf (tmp, "   %c %-2d              %d", (m[i])?('*'):(' '), i+1, vi[i]);
    outtextxy (208,y1+24+(i<<3),tmp,menufg);
  }
  box (208,y1+24,432,y1+31,menubgh);
  sprintf (tmp, "   %c 1               %d", (m[0])?('*'):(' '), vi[0]);
  outtextxy (208,y1+24,tmp,menufgh);
  i=j=0;
  outtextxy (164, y1+28+((NUMVOICES+1)<<3), "\030,\031 - Change Voice     <Enter> - Accept", menufg);
  outtextxy (164, y1+28+((NUMVOICES+2)<<3), "-,+ - Change Staff    <Escape> - Cancel", menufg);
  do {
    if (kbhit())
      chx = getxkey();
    else
      chx = 0;
    switch (chx&0xff) {
      case 0x48: if (i > 0) i--;
                 else i = NUMVOICES-1;
                 break;
      case 0x50: if (i < NUMVOICES-1) i++;
                 else i = 0;
                 break;
      case 0x39: m[i] = !m[i]; draw = -1;
                 break;
      default:
        ch = ASCIIcode(chx);
        if (ch == '+') {
          if (vi[i] < 8) vi[i]++;
          else vi[i] = 1;
          draw = -1;
        }
        else if (ch == '-') {
          if (vi[i] > 1) vi[i]--;
          else vi[i] = 8;
          draw = -1;
        }
    }
    if (i != j) {
      box (208,y1+24+(j<<3),432,y1+31+(j<<3),menubg);
      sprintf (tmp, "   %c %-2d              %d", (m[j])?('*'):(' '), j+1, vi[j]);
      outtextxy (208,y1+24+(j<<3),tmp,menufg);
      j = i;
      box (208,y1+24+(i<<3),432,y1+31+(i<<3),menubgh);
      sprintf (tmp, "   %c %-2d              %d", (m[i])?('*'):(' '), i+1, vi[i]);
      outtextxy (208,y1+24+(i<<3),tmp,menufgh);
    }
    if (draw) {
      box (208,y1+24+(i<<3),432,y1+31+(i<<3),menubgh);
      sprintf (tmp, "   %c %-2d              %d", (m[i])?('*'):(' '), i+1, vi[i]);
      outtextxy (208,y1+24+(i<<3),tmp,menufgh);
      draw = 0;
    }
  } while ((chx&0xfff) != 0x01 && (chx&0xfff) != 0x1C);
  if ((chx&0xfff) == 0x1C) {
    memcpy (voiceinfo, vi, NUMVOICES);
    memcpy (mute, m, NUMVOICES*2);
    for (i=1, j=voiceinfo[0]; i<NUMVOICES; i++)
      if (voiceinfo[i] > j) j = voiceinfo[i];
    numstaves = j;
  }
  curstaff = voiceinfo[curvoice];
  curstaffy = curstaff*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(curstaff<<1)-1];
  redraw_screen();
  return;
}

/****************************************************************************/

void default_tempo(void) {
  int t[2];
  if (_tempo (t) == 0) {
    tempo[0] = t[0];
    tempo[1] = t[1];
  }
  return;
}

/****************************************************************************/

void default_timesig(void) {
  int t;
  if (_timesig (&t) == 0)
    timesig = t;
  return;
}

/****************************************************************************/

void staff_setup(void) {
  int y1, y2, i, j, chx, draw = 0; char tmp[36], ch, si[16];

  memcpy (si, staffinfo, 16);
  box (140,y1=240-((8+numstaves)<<2),500,y2=240+((8+numstaves)<<2),menubg);
  line (140,y1,500,y1,menufg);
  line (140,y2,500,y2,menufg);
  line (140,y1,140,y2,menufg);
  line (500,y1,500,y2,menufg);
  line (179,y1+23,461,y1+23,menufg);
  line (179,y1+24+(numstaves<<3),461,y1+24+(numstaves<<3),menufg);
  line (179,y1+23,179,y1+24+(numstaves<<3),menufg);
  line (461,y1+23,461,y1+24+(numstaves<<3),menufg);
  outtextxy (180,y1+8,"Staff Number   Clef          Offset",menufg);
  for (i=0;i<numstaves;i++) {
    sprintf (tmp, "     %d        %-13s    %d", i+1, clefs[si[i<<1]], si[(i<<1)+1]);
    outtextxy (180,y1+24+(i<<3),tmp,menufg);
  }
  box (180,y1+24,460,y1+31,menubgh);
  sprintf (tmp, "     1        %-13s    %d", clefs[si[0]], si[1]);
  outtextxy (180,y1+24,tmp,menufgh);
  i=j=0;
  outtextxy (144, y2-28, "   \030,\031    - Change Staff", menufg);
  outtextxy (144, y2-20, "\033,\032,G,F,C - Change Clef", menufg);
  outtextxy (144, y2-12, "   -,+    - Change Offset", menufg);
  outtextxy (360, y2-24, " <Enter> - Accept", menufg);
  outtextxy (360, y2-16, "<Escape> - Cancel", menufg);
  do {
    if (kbhit())
      chx = getxkey();
    else
      chx = 0;
    switch (chx&0xff) {
      case 0x48: if (i > 0) i--;
                 else i = numstaves-1;
                 break;
      case 0x50: if (i < numstaves-1) i++;
                 else i = 0;
                 break;
      case 0x4B:
      case 0x4D: if ((chx&0xFFF) == 0x4B)
                   if (si[i<<1] > 0) si[i<<1]--;
                   else si[i<<1] = 8;
                 else
                   if (si[i<<1] < 8) si[i<<1]++;
                   else si[i<<1] = 0;
                 draw = -1;
                 break;
      default:
        ch = tolower(ASCIIcode(chx));
        switch (ch) {
          case 'g' : case 'f' : case 'c' :
            if (ch == 'g') si[i<<1] = 1;
            else if (ch == 'f') si[i<<1] = 7;
            else si[i<<1] = 4;
            draw = -1;
            break;
          case '+' : case '-' :
            if (ch == '+') si[(i<<1)+1]++;
            else si[(i<<1)+1]--;
            draw = -1;
        }
    }
    if (i != j) {
      box (180,y1+24+(j<<3),460,y1+31+(j<<3),menubg);
      sprintf (tmp, "     %d        %-13s    %d", j+1, clefs[si[j<<1]], si[(j<<1)+1]);
      outtextxy (180,y1+24+(j<<3),tmp,menufg);
      j = i;
      box (180,y1+24+(i<<3),460,y1+31+(i<<3),menubgh);
      sprintf (tmp, "     %d        %-13s    %d", i+1, clefs[si[i<<1]], si[(i<<1)+1]);
      outtextxy (180,y1+24+(i<<3),tmp,menufgh);
    }
    if (draw) {
      box (180,y1+24+(i<<3),460,y1+31+(i<<3),menubgh);
      sprintf (tmp, "     %d        %-13s    %d", i+1, clefs[si[i<<1]], si[(i<<1)+1]);
      outtextxy (180,y1+24+(i<<3),tmp,menufgh);
      draw = 0;
    }
  } while ((chx&0xfff) != 0x01 && (chx&0xfff) != 0x1C);
  if ((chx&0xfff) == 0x1C)
    memcpy (staffinfo, si, 16);
  redraw_screen();
  return;
}

/****************************************************************************/

void about(void) {
  int l1, l2, b1, b2;
  line (58,156,58,324,menufg);
  line (581,156,581,324,menufg);
  line (58,156,581,156,menufg);
  line (58,324,581,324,menufg);
  box (59,157,580,323,menubg);
  draw_symbol (54,127,_MIDI,menufg,putpixel);
  draw_symbol (310,128,_Compozer,menufg,putpixel);
  fontout (1,(MAXX-fontlength(1,"Copyright (c) "COPYRIGHT" Apriori Enterprises"))>>1,247,"Copyright (c) "COPYRIGHT" Apriori Enterprises",menufg,0);
  line (59,272,580,272,menufg);
  outtextxy ((MAXX-(45<<3))>>1,276,"Apriori Enterprises MIDI Compozer Version "VERSION,menufg);
  outtextxy ((MAXX-(27<<3))>>1,288,"Written By Joshua Humphries",menufg);
  outtextxy ((MAXX-(28<<3))>>1,312,"Press any key to continue...",menufg);
  getxkey();
  redraw_screen();
  return;
}

/****************************************************************************/

void view_help(void) {
  BrowseFile ("compozer.aeh");
  return;
}

/****************************************************************************/

void view_keys(void) {
  BrowseFile ("keys.aeh");
  return;
}

/****************************************************************************/

int YesNo(char *s) {
  int xw, xs, xe, ys = (MAXY-36)>>1, ye = (MAXY+36)>>1,
      yw = ye-ys+1;
  char ch;
  xw = 42<<3;
  xs = (MAXX-xw)>>1; xe = (MAXX+xw)>>1;
  getbox (undrawbuf, xs-10, ys, xe-xs+21, yw);
  box (xs-9, ys+1, xe+9, ye-1, menubg);
  line (xs-10, ys, xe+10, ys, menufg);
  line (xs-10, ye, xe+10, ye, menufg);
  line (xs-10, ys, xs-10, ye, menufg);
  line (xe+10, ys, xe+10, ye, menufg);
  outtextxy ((MAXX-(20<<3))>>1,ys+3,s,menufg);
  outtextxy ((MAXX-(42<<3))>>1,ye-11,"Y - Yes        N - No       <Esc> - Cancel",menufg);
  while ((ch = getxkey()&0xff) != 1 && ch != 0x15 && ch != 0x31);
  putbox (undrawbuf, xs-10, ys, xe-xs+21, yw);
  if (ch == 1) return -1;
  else if (ch == 0x15) return 1;
  else return 0;
}

/****************************************************************************/

int save_changes(void) {
  int i;
  if ((i=YesNo ("Save Changes First ?")) == 1)
    return save_song();
  else
    return i;
}

/****************************************************************************/

void enter_editmode(void) {
  if (editmode) return;
  editmode = -1;
  memset (voice, 0, NUMVOICES*2);
  voice[curvoice] = -1;
  markbegin = markend = curpos;
  box ((markbegin<<3)+COLOFFS,TOPY,(markend<<3)+COLOFFS+7,LOWY, selbg);
  redraw_column (markbegin);
  return;
}

/****************************************************************************/

void exit_editmode(void) {
  if (!editmode) return;
  undraw_col();
  box ((markbegin<<3)+COLOFFS,TOPY,(markend<<3)+COLOFFS+7,LOWY,bg);
  editmode = 0;
  redraw_music(markbegin,markend); redraw_info();
  draw_col();
  return;
}

/****************************************************************************/

void draw_symbol(int x, int y, char *data, char color,
		 void (*plot)(int x, int y, char color)) {
  int Y;
  int xs = x+data[2], xc, ys = y+data[1], yc, sc, tc, cc = data[0];
  data += 3;
  for (yc = 0; cc > 0; cc--, yc++) {
    sc = *data++;
    for (xc = 0; sc > 0; sc--) {
      xc += *data++; tc = *data++;
      if ((Y = ys+yc) >= TOPY && Y <= LOWY)
	for (; tc > 0; tc--)
	  plot (xs + xc++, Y, color);
    }
  }
  return;
}

/****************************************************************************/

int fontlength(int font, char *str) {
  int i, xo;
  for (i = xo = 0; str[i]; i++)
    switch (font % 4) {
      case 0: xo += Font1[str[i] & 0x7F].width; break;
      case 1: xo += Font2[str[i] & 0x7F].width; break;
      case 2: xo += Font3[str[i] & 0x7F].width; break;
      case 3: xo += Font4[str[i] & 0x7F].width;
    }
  return xo;
}

/****************************************************************************/

void fontout(int font, int x, int y, char *str, char color, int sp) {
  int i, xo;
  for (i = xo = 0; str[i]; i++)
    switch (font % 4) {
      case 0: draw_symbol (x+xo, y, Font1[str[i] & 0x7F].data, color, putpixel);
	 xo += sp + Font1[str[i] & 0x7F].width; break;
      case 1: draw_symbol (x+xo, y, Font2[str[i] & 0x7F].data, color, putpixel);
	 xo += sp + Font2[str[i] & 0x7F].width; break;
      case 2: draw_symbol (x+xo, y, Font3[str[i] & 0x7F].data, color, putpixel);
	 xo += sp + Font3[str[i] & 0x7F].width; break;
      case 3: draw_symbol (x+xo, y, Font4[str[i] & 0x7F].data, color, putpixel);
	 xo += sp + Font4[str[i] & 0x7F].width;
    }
  return;
}

/****************************************************************************/

void cfontout(int font, int x, int y, char *str, char color, int sp) {
  int i, xo;
  for (i = xo = 0; str[i]; i++)
    switch (font % 4) {
      case 0: draw_symbol (x+xo, y, Font1[str[i] & 0x7F].data, color, cputpixel);
	 xo += sp + Font1[str[i] & 0x7F].width; break;
      case 1: draw_symbol (x+xo, y, Font2[str[i] & 0x7F].data, color, cputpixel);
	 xo += sp + Font2[str[i] & 0x7F].width; break;
      case 2: draw_symbol (x+xo, y, Font3[str[i] & 0x7F].data, color, cputpixel);
	 xo += sp + Font3[str[i] & 0x7F].width; break;
      case 3: draw_symbol (x+xo, y, Font4[str[i] & 0x7F].data, color, cputpixel);
	 xo += sp + Font4[str[i] & 0x7F].width;
    }
  return;
}

/****************************************************************************/

void unfontout(int font, int x, int y, char *str, char color, int sp) {
  int i, xo;
  for (i = xo = 0; str[i]; i++)
    switch (font % 4) {
      case 0: draw_symbol (x+xo, y, Font1[str[i] & 0x7F].data, color, unputpixel);
	 xo += sp + Font1[str[i] & 0x7F].width; break;
      case 1: draw_symbol (x+xo, y, Font2[str[i] & 0x7F].data, color, unputpixel);
	 xo += sp + Font2[str[i] & 0x7F].width; break;
      case 2: draw_symbol (x+xo, y, Font3[str[i] & 0x7F].data, color, unputpixel);
	 xo += sp + Font3[str[i] & 0x7F].width; break;
      case 3: draw_symbol (x+xo, y, Font4[str[i] & 0x7F].data, color, unputpixel);
	 xo += sp + Font4[str[i] & 0x7F].width;
    }
  return;
}

/****************************************************************************/

void draw_nsymbol(int x, int y, char *data, char color) {
  draw_symbol (x, y, data, color, putpixel);
  return;
}

/****************************************************************************/

void draw_csymbol(int x, int y, char *data, char color) {
  draw_symbol (x, y, data, color, cputpixel);
  return;
}

/****************************************************************************/

void undraw_symbol(int x, int y, char *data) {
  draw_symbol (x, y, data, bg, unputpixel);
}

/****************************************************************************/

void draw_col(void) {
  int i, j;
  for (i = TOPY, j = 0; i <= LOWY; i++) {
    colbuf[j++] = getpixel ((curpos<<3) + COLOFFS-1, i);
    colbuf[j++] = getpixel ((curpos<<3) + COLOFFS+8, i);
  }
  line ((curpos<<3)+COLOFFS-1, TOPY, (curpos<<3)+COLOFFS-1, LOWY, col);
  line ((curpos<<3)+COLOFFS+8, TOPY, (curpos<<3)+COLOFFS+8, LOWY, col);
  return;
}

/****************************************************************************/

void undraw_col(void) {
  int i, j;
  for (i = TOPY, j = 0; i <= LOWY; i++) {
    putpixel ((curpos<<3) + COLOFFS-1, i, colbuf[j++]);
    putpixel ((curpos<<3) + COLOFFS+8, i, colbuf[j++]);
  }
  return;
}

/****************************************************************************/

void draw_voice(int column, char voice, int staffy, char color,
		void (*plot)(int x, int y, char color),
		void (*drawline)(int x1, int y1, int x2, int y2, char color),
		void (*drawfont)(int font, int x, int y, char *str, char color, int sp),
		void (*drawcurve)(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, char color)) {
  unsigned long i;
  unsigned char j, k;
  char buf[40], tmp[5];
  int y1, y2, y3, x, x1, x2, x3;
  unsigned bv, bm, bmv;

  /* this sets putpixel clipping area (can only define y-clipping) */
  high = LOWY; low = TOPY;
  x = (column<<3)+COLOFFS;
  /* some stuff stored in voice one pertains to all voices like: */
	/* bars */
  if (((i = page[column*NUMVOICES]) & 0xE0000000l) == 0x80000000l) {
    y1 = (HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[1];
    y2 = numstaves*(HEIGHT+37)/(numstaves+1) + TOPY - 1 + staffinfo[(numstaves<<1)-1];
    j = i & 0x00000007l;
    if (drawlines)
      switch (j) {
	case 0:drawline(x+7,y1,x+7,y2,color);
	       break;
	case 1:drawline(x+3,y1,x+3,y2,color);
	       drawline(x+7,y1,x+7,y2,color);
	       break;
	case 2:
	case 3:drawline (x+2,y1,x+2,y2,color);
	       drawline (x+5,y1,x+5,y2,color);
	       drawline (x+6,y1,x+6,y2,color);
	       drawline (x+7,y1,x+7,y2,color);
	       break;
	case 4:
	case 5:drawline (x,y1,x,y2,color);
	       drawline (x+1,y1,x+1,y2,color);
	       drawline (x+2,y1,x+2,y2,color);
	       drawline (x+5,y1,x+5,y2,color);
      }
    if (drawcontrol && ((i >> 4) & 0x0F) && j == 3) {
      sprintf (buf, "%dx", ((i>>4)&0xf)+1);
      drawfont (2,x,y1-14,buf,color,0);
    }
    switch (j) {
      case 0:draw_symbol(x,staffy,sbar,color,plot); break;
      case 1:draw_symbol(x,staffy,dbar,color,plot); break;
      case 2:draw_symbol(x,staffy,ends,color,plot); break;
      case 3:draw_symbol(x,staffy,endr,color,plot); break;
      case 4:draw_symbol(x,staffy,begs,color,plot); break;
      case 5:draw_symbol(x,staffy,begr,color,plot);
    }
  }
	/* key signature accidentals */
  else if ((i & 0xE1000800l) == 0x40000800l) {
    j = i&0x7F; j %= 7;
    switch (y2 = (i&0x700)>>8) {
      case 0: case 1:
        if (j < 3) j += 7; break;
      case 2: case 3:
        if (j < 5) j += 7; break;
      default:if (j < 4) j += 7;
    }
    switch (y1 = staffinfo[(voiceinfo[voice]-1)<<1]) {
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
    y1 = (23 - (j>>1) - y1)*9 + staffy;
    if (!(j & 1))
      y1 += 5;
    switch ((i&0x700)>>8) {
      case 0: draw_symbol(x,y1,flat,color,plot); break;
      case 1: draw_symbol(x,y1,doubleflat,color,plot); break;
      case 2: draw_symbol(x,y1,sharp,color,plot); break;
      case 3: draw_symbol(x,y1,doublesharp,color,plot); break;
      default:draw_symbol(x,y1,natural,color,plot);
    }
  }
        /* percussion bank changes */
  else if ((i & 0xE00E8000l) == 0xE0088000l) {
    if (drawcontrol) {
      y1 = (HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[1];
      y1 += (signed char)(i & 0xFF);
      sprintf (buf,"Percussion Bank %d", ((i>>8)&0x7F)+1);
      drawfont(2,x,y1-16,buf,color,0);
    }
  }
	/* time signature & tempo stuff (tempo, accelerando, & ritardando) */
  else if ((i & 0xE0080000l) == 0xE0000000l) {
    if ((i&0xE00E0000l) == 0xE0000000l) {
      /* time signature */
      x1 = (i>>8)&0xff;
      x2 = x1>>5; x1 &= 0x1f;
      if (x1 == 0) {
	if (x2 == 7) draw_symbol (x,staffy,cuttime,color,plot);
	else draw_symbol (x,staffy,stdtime,color,plot);
      }
      else {
	x1++;
	x3 = Timenum[x1%10].width + ((x1>9)?(Timenum[x1/10].width):(0));
	if (x3 > DenomWidth[x2]) {
	  y2 = (x3-DenomWidth[x2])>>1; y1 = 0;
	}
	else {
	  y1 = (DenomWidth[x2]-x3)>>1; y2 = 0;
	}
	y1 += x; y2 += x;
	if (x1 > 9) {
	  draw_symbol (y1,staffy,Timenum[x1/10].data,color,plot);
	  y1+=Timenum[x1/10].width;
	}
	draw_symbol (y1,staffy,Timenum[x1%10].data,color,plot);
	staffy += 18;
	switch (x2) {
	  case 0:draw_symbol(y2,staffy,Timenum[1].data,color,plot); break;
	  case 1:draw_symbol(y2,staffy,Timenum[2].data,color,plot); break;
	  case 2:draw_symbol(y2,staffy,Timenum[4].data,color,plot); break;
	  case 3:draw_symbol(y2,staffy,Timenum[8].data,color,plot); break;
	  case 4:draw_symbol(y2,staffy,Timenum[1].data,color,plot);
		 draw_symbol(y2+9,staffy,Timenum[6].data,color,plot); break;
	  case 5:draw_symbol(y2,staffy,Timenum[3].data,color,plot);
		 draw_symbol(y2+13,staffy,Timenum[2].data,color,plot); break;
	  case 6:draw_symbol(y2,staffy,Timenum[6].data,color,plot);
		 draw_symbol(y2+14,staffy,Timenum[4].data,color,plot);
	}
      }
    }
    else if (drawcontrol) {
      y1 = (HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[1];
      y1 += (signed char)(i & 0xFF) - 8;
      switch (i&0xE00E0000l) {
	/* accelerando */
	case 0xE0020000l: drawfont (3,x,y1-10,"accel.", color,0); break;
	/* ritardando */
	case 0xE0040000l: drawfont (3,x,y1-10,"ritard.", color,0); break;
	/* tempo */
	case 0xE0060000l:
	  sprintf (buf, " = %d bpm", ((i>>8)&0x1FF)+1);
	  j = (i>>20)&0x0F;
	  x1 = x; y2 = y1-16;
	  switch (j&0x07) {
	    case 0: draw_symbol (x,y1,head1,color,plot); x1+=2; y2+=10; break;
	    case 1: draw_symbol (x,y1,head2,color,plot);
		    drawline (x+7,y1,x+7,y1-36,color); break;
	    default:draw_symbol (x,y1,head4,color,plot);
		    drawline (x+7,y1,x+7,y1-36,color);
		    if ((j&0x07) > 2) {
		      x1 += 2;
		      for (k = 0; k < (j&0x07)-3; k++)
			draw_symbol (x+7,y1-36+k*6,flag1b,color,plot);
		      draw_symbol (x+7,y1-36+k*6,flag1a,color,plot);
		    }
	  }
	  if (j&0x08) {
	    draw_symbol (x,y1+4,dot,color,plot);
	    x1 += 4;
	  }
	  drawfont (2,x1+8,y2,buf,color,0);
      }
    }
  }
	/* control stuff like codas, segnos, etc... */
  else if ((i & 0xE0000000l) == 0xA0000000l) {
    if (drawcontrol) {
      y1 = (HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[1];
      y1 += (signed char)(i & 0xFF);
      switch (i & 0xE0000700) {
	case 0xA0000000: drawfont (3, x, y1-18, "D.C.", color,0); break;
	case 0xA0000100: drawfont (3, x, y1-18, "D.S.", color,0); break;
	case 0xA0000200: draw_symbol (x, y1, segno, color, plot); break;
	case 0xA0000300: draw_symbol (x, y1, coda, color, plot); break;
	case 0xA0000400: drawfont (1, x, y1-24, "Fine", color,0); break;
	case 0xA0000500: drawfont (3, x, y1-18, "al Coda", color,0); break;
        case 0xA0000600: drawfont (3, x, y1-18, "al Fine", color,0); break;
        case 0xA0000700: if (i&0x800)
                           drawfont (3, x, y1-18, "Perc.", color,0);
                         else
                           drawfont (3, x, y1-18, "No Perc.", color,0);
      }
    }
  }
	/* endings for repeated sections */
  else if ((i & 0xE0000000l) == 0xC0000000l) {
    if (drawcontrol) {
      y1 = (HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[1];
      y1 += (signed char)(i & 0xFF);
      bv = (i>>8)&0xFFFF;
      bm = 1; k = x2 = buf[0] = 0;
      for (j = 0; j < 16; j++, bm <<= 1)
	if ((bmv = bv & bm) && x2 == 0) {
	  sprintf (buf, "%d", j+1); x1 = k = j; x2++;
	}
	else if (bmv && x1 != j-1) {
	  sprintf (tmp, ",%d", j+1); strcat (buf, tmp); x1 = k = j; x2++;
	}
	else if (bmv && j == 15)
	  strcat (buf, "-16");
	else if (bmv) {
	  x1 = j; x2++;
	}
	else if (!bmv && x1 == j-1 && x1 != k) {
	  sprintf (tmp, "-%d", j); strcat (buf, tmp);
	}
      x3 = fontlength(2,buf);
      drawline (x,y1-2,x,y1-15,color);
      drawline (x,y1-15,x+x3+4,y1-15,color);
      drawfont (2,x+4,y1-13,buf,color,0);
    }
  }
  /* if this is a blank, leave it be */
  else if (((i = page[column*NUMVOICES+voice]) & 0xE0000000l) == 0)
    goto Return;
  /* Now the fun stuff ! */
	/* Notes !!!!! */
  else if ((i & 0xE0000080l) == 0x20000000l) {
    /* lotsa prep stuff... */
    x1 = x2 = x;
    j = i&0x7F;
    y3 = y2 = y1 = (23 - (j>>1) - staffinfo[(voiceinfo[voice]-1)<<1])*9 + staffy;
    k = (i>>16) & 0x4;
    if (!(j & 1)) {
      y3 += 4+((k)?(0):(1)); y1 += 5; y2 += 9;
    }
    if (k)
      x1 += 2;
    else
      x2 += 7;
    if (i & 0x10000) {
      if (k) x1 -= 9;
      else x1 += 9;
    }
    /* draw note head */
    switch (j=(i & 0x700)>>8) {
      case 0: draw_symbol((k)?(x1-=2):(x1),y1,head1,color,plot); break;
      case 1: draw_symbol(x1,y1,head2,color,plot); break;
      default:draw_symbol(x1,y1,head4,color,plot); break;
    }
    /* ledger lines... */
    if (y1 < staffy)
      for (x = staffy-9; x >= y1; x-=9)
	drawline (x1-6,x,x1+11,x,color);
    else if (y1 > staffy+36)
      for (x = staffy+45; x <= y1; x+=9)
	drawline (x1-6,x,x1+11,x,color);
    /* dots & triplet marks */
    if (i & 0x0800)
      draw_symbol(x1,y2,dot,color,plot);
    if (i & 0x1000) {
      if (k) drawfont (2,x1+1,y1+5,"3",color,0);
      else drawfont (2,x1+1,y1-16,"3",color,0);
    }
    /* staccato marks */
    if (i & 0x8000) {
      if (k) draw_symbol(x1,y3,st1,color,plot);
      else draw_symbol(x1,y3,st2,color,plot);
    }
    /* accent marks */
    if (i & 0x2000) {
      if (k) draw_symbol(x1,y3-17,accent,color,plot);
      else draw_symbol(x1,y3+9,accent,color,plot);
    }
    /* ties */
    if (nextpagetie(voice,column))
      if (k)
	drawcurve (x1+5,y1-6,x1+9,y1-20,x1+10,y1-24,XEND,y1-25,color);
      else
	drawcurve (x1+5,y1+6,x1+9,y1+20,x1+10,y1+24,XEND,y1+25,color);
    if (i & 0x4000) {
      if ((x=getprevtie(voice,column)) == -1) {
	if (k)
	  drawcurve (x1+2,y1-6,x1,y1-20,x1-1,y1-24,XBEGIN+1,y1-25,color);
	else
	  drawcurve (x1+2,y1+6,x1,y1+20,x1-1,y1+24,XBEGIN+1,y1+25,color);
      }
      else if (x >= 0) {
	bv = page[x*NUMVOICES+voice]&0x7F;
	y3 = (23 - (bv>>1) - staffinfo[(voiceinfo[voice]-1)<<1])*9 + staffy;
	x <<= 3; x += COLOFFS;
	if (!(bv & 1)) y3 += 4;
	if (k)
	  drawcurve (x1+2,y1-6,x1-2,y1-25,x+9,y3-25,x+5,y3-6,color);
	else
	  drawcurve (x1+2,y1+6,x1-2,y1+25,x+9,y3+25,x+5,y3+6,color);
      }
    }
    /* stems */
    if (j > 0 && j < 3) {
      if (k) drawline (x2,y1,x2,y1+37,color);
      else drawline (x2,y1,x2,y1-37,color);
    }
	/* flag/beam stuff !!!! */
    else if (j > 2) {
      int max, hi, lo, pr, nx, cr, yc, noflags = (i&0x20000l)?(-1):(0);
      unsigned long z;
      hi = lo = i&0x7F;
      pr = getprevbeam(voice,column); x=column;
      while ((x = getprevbeam(voice,x)) != -1) {
        if (((z=page[x*NUMVOICES+voice])&0x7F)<lo) lo=page[x*NUMVOICES+voice]&0x7F;
        if ((z&0x7F)>hi) hi=page[x*NUMVOICES+voice]&0x7F;
        if (z&0x20000l) noflags = -1;
        if (!(z&0x40000l)) k = (z>>16) & 0x4;
      }
      nx = getnextbeam(voice,column); x=column;
      while ((x = getnextbeam(voice,x)) != -1) {
        if (((z=page[x*NUMVOICES+voice])&0x7F)<lo) lo=page[x*NUMVOICES+voice]&0x7F;
        if ((z&0x7F)>hi) hi=page[x*NUMVOICES+voice]&0x7F;
        if (z&0x20000l) noflags = -1;
        if (!(z&0x40000l)) k = (z>>16) & 0x4;
      }
      if (k) max = lo;
      else max = hi;
      if (pr == -1 && nx == -1)
        goto doflags;
      else {
        int nxx = (nx<<3)+COLOFFS, prx = (pr<<3)+COLOFFS;
        if (!k) { nxx += 7; prx += 7; }
        if (nx != -1) nx = (page[nx*NUMVOICES+voice]>>8)&0x7;
        if (pr != -1) pr = (page[pr*NUMVOICES+voice]>>8)&0x7;
        cr = (i>>8)&0x7;
        y3 = (23 - (max>>1) - staffinfo[(voiceinfo[voice]-1)<<1])*9 + staffy;
        if (k) drawline (x2,y1,x2,y3+=35,color);
        else drawline (x2,y1,x2,y3-=37,color);
        if (!noflags) {
          if (pr != -1) {
            int use;
            if (pr < cr && nx >= cr)
              use = pr;
            else
              use = cr;
            for (yc=y3,x=0; x<use-2; x++) {
              drawline (((x2+prx)>>1)-1,yc,x2,yc,color);
              drawline (((x2+prx)>>1)-1,yc+1,x2,yc+1,color);
              drawline (((x2+prx)>>1)-1,yc+2,x2,yc+2,color);
              if (k) yc -= 5;
              else yc += 5;
            }
          }
          if (nx != -1) {
            int use;
            if (nx < cr && pr >= 0)
              use = nx;
            else
              use = cr;
            for (yc=y3,x=0; x<use-2; x++) {
              drawline (x2,yc,((x2+nxx)>>1)+1,yc,color);
              drawline (x2,yc+1,((x2+nxx)>>1)+1,yc+1,color);
              drawline (x2,yc+2,((x2+nxx)>>1)+1,yc+2,color);
              if (k) yc -= 5;
              else yc += 5;
            }
          }
        }
      }
    }
    else if (j > 2) {
      /* flags (if no beams...) */
doflags:
      y3 = 37;
      if (j > 5 && k) y3 += 3;
      if (j == 6) y3 += 3;
      if (j == 6 && k) y3 += 3;
      if (k) drawline (x2,y1,x2,y1+y3,color);
      else drawline (x2,y1,x2,y1-y3,color);
      if (!(i&0x20000)) {
        for (x = 0; x < j-3; x++)
          if (k) draw_symbol (x2,y1+y3-x*6,flag2b,color,plot);
          else draw_symbol (x2,y1-y3+x*6,flag1b,color,plot);
        if (k) draw_symbol (x2,y1+y3-x*6,flag2a,color,plot);
        else draw_symbol (x2,y1-y3+x*6,flag1a,color,plot);
      }
    }
  }
	/* Rests !!!!! */
  else if ((i & 0xE0000080l) == 0x20000080l) {
    j = i&0x7F;
    y2 = y1 = (23 - (j>>1) - staffinfo[(voiceinfo[voice]-1)<<1])*9 + staffy;
    if (!(j & 1)) {
      y1 += 5; y2 += 9;
    }
    switch ((i & 0x700)>>8) {
      case 0: draw_symbol(x,y1,whole,color,plot); break;
      case 1: draw_symbol(x,y1,half,color,plot); break;
      case 2: draw_symbol(x,y1,quarter,color,plot); break;
      case 3: draw_symbol(x,y1,_8th,color,plot); break;
      case 4: draw_symbol(x,y1,_16th,color,plot); break;
      case 5: draw_symbol(x,y1,_32nd,color,plot); break;
      case 6: draw_symbol(x,y1,_64th,color,plot);
    }
    if (i & 0x0800)
      draw_symbol(x,y2,dot,color,plot);
    if (i & 0x1000)
      drawfont (2,x+1,y1-24,"3",color,0);
  }
  /* Accidentals */
  else if ((i & 0xE1000000l) == 0x40000000l) {
    j = i&0x7F;
    y1 = (23 - (j>>1) - staffinfo[(voiceinfo[voice]-1)<<1])*9 + staffy;
    if (!(j & 1))
      y1 += 5;
    switch ((i & 0x700)>>8) {
      case 0: draw_symbol(x,y1,flat,color,plot); break;
      case 1: draw_symbol(x,y1,doubleflat,color,plot); break;
      case 2: draw_symbol(x,y1,sharp,color,plot); break;
      case 3: draw_symbol(x,y1,doublesharp,color,plot); break;
      default:draw_symbol(x,y1,natural,color,plot);
    }
  }
  /* Keyboard Panning */
  else if ((i & 0xE1000000l) == 0x41000000l) {
    y1 = staffy + (signed char)(i & 0xFF);
    drawfont (2,x,y1-16,"Keyboard Pan",color,0);
  }
  /* Dynamics */
  else if ((i & 0xE1000000l) == 0x60000000l) {
    y1 = staffy + (signed char)(i & 0xFF);
    switch (i & 0x300) {
      case 0x000: drawfont (3,x,y1-18,"dim.",color,0); break;
      case 0x100: drawfont (3,x,y1-18,"cresc.",color,0); break;
      case 0x200:
	switch ((i>>10)&0x07) {
	  case 0: drawfont (3,x,y1-18,"ppp",color,-4); break;
	  case 1: drawfont (3,x,y1-18,"pp",color,-4); break;
	  case 2: drawfont (3,x,y1-18,"p",color,-4); break;
	  case 3: drawfont (3,x,y1-18,"mp",color,-3); break;
	  case 4: drawfont (3,x,y1-18,"mf",color,-4); break;
	  case 5: drawfont (3,x,y1-18,"f",color,-7); break;
	  case 6: drawfont (3,x,y1-18,"ff",color,-7); break;
	  case 7: drawfont (3,x,y1-18,"fff",color,-7);
	}
        break;
      case 0x300: sprintf (buf,"Vol = %d%%", (((i>>10)&0x7F)*100l)/127);
                  drawfont(2,x,y1-16,buf,color,0);
    }
  }
  /* Pitch Wheel */
  else if ((i & 0xE1000000l) == 0x61000000l) {
    y1 = staffy + (signed char)(i & 0xFF);
    if (i & 0x100)
      drawfont (2,x,y1-16,"Wheel Roll",color,0);
    else {
      sprintf (buf,"Wheel = %d%%", (((signed char)((i>>16)&0xFF)+1l)*100l)>>7);
      drawfont(2,x,y1-16,buf,color,0);
    }
  }
  /* voice-specific control stuff */
  else if ((i & 0xE0080000l) == 0xE0080000l) {
    y1 = staffy + (signed char)(i & 0xFF);
    switch (i & 0xE00E0000l) {
      /* program change (write the new instrument name) */
      case 0xE0080000l:
        if (!(i & 0x8000l))
          drawfont(2,x,y1-16,getinstrumentname((i>>20)&0x7F,(i>>8)&0x7F),color,0);
        break;
      /* pan - write the value in % (-100% = left, 100% = right) */
      case 0xE00A0000l:
        sprintf (buf,"Pan = %d%%", (((signed char)((i>>8)&0xFF)+1l)*100l)>>7);
	drawfont(2,x,y1-16,buf,color,0); break;
      /* fermata... just draw the symbol */
      case 0xE00C0000l:
        draw_symbol (x,y1,fermata,color,plot); break;
      /* stereo fade */
      case 0xE00E0000l:
        drawfont (2,x,y1-16,"Stereo Fade",color,0);
    }
  }
Return:
  high = MAXY; low = 0;
  return;
}

/****************************************************************************/

void draw_nvoice(int column, char voice, int staffy, char color) {
  draw_voice (column, voice, staffy, color, putpixel, line, fontout, curve);
  return;
}

/****************************************************************************/

void draw_cvoice(int column, char voice, int staffy, char color) {
  draw_voice (column, voice, staffy, color, cputpixel, cline, cfontout, ccurve);
  return;
}

/****************************************************************************/

void undraw_nvoice(int column, char voice, int staffy) {
  draw_voice (column, voice, staffy, bg, putpixel, line, fontout, curve);
  return;
}

/****************************************************************************/

void undraw_cvoice(int column, char voice, int staffy) {
  draw_voice (column, voice, staffy, bg, unputpixel, unline, unfontout, uncurve);
  return;
}

/****************************************************************************/

void drawrow_nvoice(char v, int begin, int end) {
  int i, y =
voiceinfo[v]*(HEIGHT+37)/(numstaves+1)+TOPY-37+staffinfo[(voiceinfo[v]<<1)-1],
      c = (v == curvoice)?(fg):(mg),
      s = (voice[v])?(selfg):(selmg);
  for (i = begin; i <= end; i++)
    draw_nvoice (i, v, y, (editmode)?((i >= markbegin && i <= markend)?(s):(c)):(c));
  return;
}

/****************************************************************************/

void undrawrow_nvoice(char v, int begin, int end) {
  int i, y =
voiceinfo[v]*(HEIGHT+37)/(numstaves+1)+TOPY-37+staffinfo[(voiceinfo[v]<<1)-1];
  for (i = begin; i <= end; i++)
    undraw_nvoice (i, v, y);
  return;
}

/****************************************************************************/

void drawrow_cvoice(char v, int begin, int end) {
  int i, y =
voiceinfo[v]*(HEIGHT+37)/(numstaves+1)+TOPY-37+staffinfo[(voiceinfo[v]<<1)-1],
      c = (v == curvoice)?(fg):(mg),
      s = (voice[v])?(selfg):(selmg);
  for (i = begin; i <= end; i++)
    draw_cvoice (i, v, y, (editmode)?((i >= markbegin && i <= markend)?(s):(c)):(c));
  return;
}

/****************************************************************************/

void undrawrow_cvoice(char v, int begin, int end) {
  int i, y =
voiceinfo[v]*(HEIGHT+37)/(numstaves+1)+TOPY-37+staffinfo[(voiceinfo[v]<<1)-1];
  for (i = begin; i <= end; i++)
    undraw_cvoice (i, v, y);
  return;
}

/****************************************************************************/

void patch_staves(int column) {
  int i, y, c, color;
  for (i = 1; i <= numstaves; i++) {
    y = i*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(i<<1)-1];
    if (editmode && column >= markbegin && column <= markend) {
      if (i == curstaff) color = selfg;
      else color = selmg;
    }
    else
      if (i == curstaff) color = fg;
      else color = mg;
    for (c = 0; c < 5; c++, y += 9)
      line ((column<<3)+COLOFFS, y, (column<<3)+COLOFFS+7, y, color);
  }
  return;
}

/****************************************************************************/

void redraw_column(int column) {
  int i, sel = (editmode)?((column >= markbegin && column <= markend)?(-1):(0)):(0);
  /* first draw inactive voices */
  drawlines = -1;
  for (i = 0; i < NUMVOICES; i++)
    if ((editmode && !voice[i]) || i != curvoice) {
      draw_nvoice (column, i,
voiceinfo[i]*(HEIGHT+37)/(numstaves+1)+TOPY-37+staffinfo[(voiceinfo[i]<<1)-1],
	 (sel)?(selmg):(mg));
      drawlines = 0;
    }
  /* draw staves */
  patch_staves (column);
  /* then draw active voices */
  drawcontrol = -1;
  if (editmode)
    for (i = 0; i < NUMVOICES; i++) {
      if (voice[i]) {
	draw_cvoice (column, i,
voiceinfo[i]*(HEIGHT+37)/(numstaves+1)+TOPY-37+staffinfo[(voiceinfo[i]<<1)-1],
	 (sel)?(selfg):(fg));
	 drawcontrol = 0;
      }
    }
  else {
    draw_cvoice (column, curvoice,
voiceinfo[i]*(HEIGHT+37)/(numstaves+1)+TOPY-37+staffinfo[(voiceinfo[i]<<1)-1],
	 (sel)?(selfg):(fg));
	 drawcontrol = 0;
  }
  /* done */
  return;
}

/****************************************************************************/

void redraw_staff(char staff, int begin, int end) {
  int i; char tmp[10];
  for (i = 0; i < NUMVOICES; i++)
    if ((editmode && !voice[i]) || (!editmode && i != curvoice))
      if (voiceinfo[i] == staff) drawrow_nvoice(i, begin, end);
  drawstaff (staff, begin, end);
  drawcontrol = -1;
  if (editmode) {
    for (i = 0; i < NUMVOICES; i++)
      if (voice[i])
	if (voiceinfo[i] == staff) {
	  drawrow_cvoice(i, begin, end);
	  drawcontrol = 0;
	}
  }
  else if (voiceinfo[curvoice] == staff) drawrow_cvoice(curvoice, begin, end);
  drawcontrol = 0;
  return;
}

/****************************************************************************/

void drawstaff(char staff, int begin, int end) {
  int y, c, color, scolor;
  y = staff*(HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[(staff<<1)-1];
  if (staff == curstaff) {
    color = fg | 0x40; scolor = selfg | 0x40;
  }
  else {
    color = mg; scolor = selmg;
  }
  if (begin == 0) {
    line (XBEGIN, y, XBEGIN, y+36, color);
    switch (staffinfo[(staff<<1)-2]) {
      case 0: draw_nsymbol (XBEGIN, y+9, treble, color); break;
      case 1: draw_nsymbol (XBEGIN, y, treble, color); break;
      case 2: draw_nsymbol (XBEGIN, y+18, alto, color); break;
      case 3: draw_nsymbol (XBEGIN, y+9, alto, color); break;
      case 4: draw_nsymbol (XBEGIN, y, alto, color); break;
      case 5: draw_nsymbol (XBEGIN, y-9, alto, color); break;
      case 6: draw_nsymbol (XBEGIN, y-18, alto, color); break;
      case 7: draw_nsymbol (XBEGIN, y, bass, color); break;
      case 8: draw_nsymbol (XBEGIN, y-9, bass, color); break;
    }
  }
  for (c = 0; c < 5; c++) {
    if (y >= TOPY || y <= LOWY) {
      if (begin == 0)
	line (XBEGIN+1, y, COLOFFS-1, y, color);
      if (editmode) {
	if (begin > markend || end < markbegin)
	  line ((begin<<3)+COLOFFS, y, (end<<3)+COLOFFS+7, y, color);
	else if (begin >= markbegin && end <= markend)
	  line ((begin<<3)+COLOFFS, y, (end<<3)+COLOFFS+7, y, scolor);
	else if (begin >= markbegin) {
	  line ((begin<<3)+COLOFFS, y, (markend<<3)+COLOFFS+7, y, scolor);
	  line ((markend<<3)+COLOFFS+8, y, (end<<3)+COLOFFS+7, y, color);
	}
	else if (end <= markend) {
	  line ((begin<<3)+COLOFFS, y, (markbegin<<3)+COLOFFS-1, y, color);
	  line ((markbegin<<3)+COLOFFS, y, (end<<3)+COLOFFS+7, y, scolor);
	}
	else {
	  line ((begin<<3)+COLOFFS, y, (markbegin<<3)+COLOFFS-1, y, color);
	  line ((markbegin<<3)+COLOFFS, y, (markend<<3)+COLOFFS+7, y, scolor);
	  line ((markend<<3)+COLOFFS+8, y, (end<<3)+COLOFFS+7, y, color);
	}
      }
      else
	line ((begin<<3)+COLOFFS, y, (end<<3)+COLOFFS+7, y, color);
    }
    y += 9;
  }
  return;
}

/****************************************************************************/

void redraw_music(int begin, int end) {
  int i, y, c; char tmp[8];
  if (curpage == 0 && !editmode) {
    fontout (1,5,LOWY-23,opus,fg,0);
    fontout (1,MAXX-fontlength(1,composer)-5,LOWY-23,composer,fg,0);
    fontout (0,(MAXX-fontlength(0,title))>>1,TOPY+2,title,fg,0);
  }
  drawlines = -1;
  for (i = 0; i < NUMVOICES; i++)
    if ((editmode && !voice[i]) || (!editmode && i != curvoice)) {
      drawrow_nvoice(i, begin, end); drawlines = 0;
    }
  if (begin == 0) {
    y = (HEIGHT+37)/(numstaves+1) + TOPY - 37 + staffinfo[1];
    c = (numstaves)*(HEIGHT+37)/(numstaves+1) + TOPY - 1 + staffinfo[(numstaves<<1)-1];
    if (y < TOPY) y = TOPY;
    else if (y > LOWY) y = LOWY;
    line (XBEGIN, y, XBEGIN, c, mg);
  }
  for (i = 1; i <= numstaves; i++)
    drawstaff (i, begin, end);
  drawcontrol = -1;
  if (editmode) {
    for (i = 0; i < NUMVOICES; i++)
      if (voice[i]) {
	drawrow_cvoice(i, begin, end);
	drawcontrol = 0;
      }
  }
  else {
    drawrow_cvoice(curvoice, begin, end);
    drawcontrol = 0;
  }
  return;
}

/****************************************************************************/

void redraw_info(void) {
  char buf[80], i, tmp[4], j, k; unsigned long x;
  box (0, 17, MAXX, 30, menubg);
  if (editmode) {
    sprintf (buf, "Voices: ");
    for (i = 0; i < 10; i++)
      if (voice[i])
	if (i == curvoice) {
	  sprintf (tmp, "  ", i+1);
	  j = strlen (buf);
	  strcat (buf, tmp);
	}
	else {
	  sprintf (tmp, "%d ", i+1);
	  strcat (buf, tmp);
	}
      for (i = strlen(buf); i < 34; i++)
	strcat (buf, " ");
      sprintf (buf+34, "Mark Begin: %-2d                 Mark End: %-2d", markbegin+1, markend+1);
      box ((j<<3)+6, 19, ((j + ((curvoice/9)+1))<<3)+6, 28, menubgh);
      sprintf (tmp, "%d", curvoice+1);
      outtextxy ((j<<3)+6, 20, tmp, menufgh);
  }
  else {
    if (((x = page[curpos*NUMVOICES+curvoice]) & 0xE0000080l) == 0x20000000l) {
      getaccidentals(curvoice);
      k = x & 0x7F;
      if (curvoice > 5) {
        j = MIDINote[k%7] + (k/7)*12 + curkey[k%7];
        sprintf (buf, "Voice: %-2d %s%s (%s)", curvoice+1, NoteName[k%7], AccName[curkey[k%7]+2], Drums[j]);
      }
      else
        sprintf (buf, "Voice: %-2d %s%s", curvoice+1, NoteName[k%7], AccName[curkey[k%7]+2]);
    }
    else
      sprintf (buf, "Voice: %-2d", curvoice+1);
    for (i = strlen(buf); i < 34; i++)
      strcat (buf, " ");
    sprintf (buf+34, "Page: %-3d                        Column: %-2d", curpage+1, curpos+1);
  }
  outtextxy (6, 20, buf, menufg);
  return;
}

/****************************************************************************/

void redraw_screen(void) {
  int i, n;
  box (0, 0, MAXX, 15, menubg);
  line (0, 31, MAXX, 31, menufg);
  box (1, 447, MAXX-1, MAXY-1, wkey);
  line (0, 446, MAXX, 446, keyb);
  line (0, MAXY, MAXX, MAXY, keyb);
  line (0, 446, 0, MAXY, keyb);
  line (MAXX, 446, MAXX, MAXY, keyb);
  for (i = n = 0; i < 71; i++, n++) {
    n %= 7;
    line (i*9, 447, i*9, MAXY-1, keyb);
    if (n != 0 && n != 3) {
      line (i*9-3, 447, i*9-3, 465, keyb);
      line (i*9+3, 447, i*9+3, 465, keyb);
      line (i*9-3, 465, i*9+3, 465, keyb);
      box (i*9-2, 447, i*9+2, 464, bkey);
    }
  }
  for (i = 0; i < 7; i++) {
    outtextxy (menubar[i], 3, mainmenu[i].name, menufg);
    line (menubar[i] + (mainmenu[i].shortcut << 3), 12,
	  menubar[i] + (mainmenu[i].shortcut << 3) + 7, 12, menufg);
  }
  if (editmode) {
    box (0,TOPY,(markbegin<<3)+COLOFFS-1,LOWY,bg);
    box ((markbegin<<3)+COLOFFS,TOPY,(markend<<3)+COLOFFS+7,LOWY,selbg);
    box ((markend<<3)+COLOFFS+8,TOPY,MAXX,LOWY,bg);
  }
  else
    box (0,TOPY,MAXX,LOWY,bg);
  redraw_music(0, NUMCOL-1);
  line (0,16,MAXX,16,menufg);
  redraw_info();
  draw_col();
  return;
}

/****************************************************************************/

int initEMS(void) {
  unsigned tmp;
  if (checkEMS() == -1)
    return -1;
  if ((tmp = EMSpagesavail()) < 3 || tmp == 0xffff)
    return -2;
  if (EMShandlesavail() < 2)
    return -3;
  if ((EMShandle[0] = EMSalloc (1)) == -1)
    return -4;
  if ((EMShandle[1] = EMSalloc (1)) == -1) {
    EMSfree (EMShandle[0]);
    return -4;
  }
  page = song = EMSpageframe;
  clipboard = (unsigned long *)EMSpageframe + 0x1000;
  undobuffer = (unsigned long *)EMSpageframe + 0x1800;
  if (EMSmap (EMShandle[0], 0, 0) == -1 || EMSmap (EMShandle[1], 1, 0) == -1) {
    EMSfree (EMShandle[0]);
    EMSfree (EMShandle[1]);
    return -5;
  }
  mappedpage = 0;
  return 0;
}

/****************************************************************************/

void navigate_menu(void) {
  int menuold = menucur, done = 0, submenuold = submenucur, ch;
  unsigned w = 11 + 33*8, h, x, y = 16, i;
  char color;

  box (menubar[menucur]-2,0,menubar[menucur] +
      (strlen(mainmenu[menucur].name) << 3) + 2, 15, menubgh);
  outtextxy (menubar[menucur], 3, mainmenu[menucur].name, menufgh);
  line (menubar[menucur] + (mainmenu[menucur].shortcut << 3), 12,
	menubar[menucur] + (mainmenu[menucur].shortcut << 3) + 7, 12, menufgh);
  if (menudown) {
    h = 3 + mainmenu[menucur].submenuitems*12;
    x = menubar[menucur] - 6;
    if (x+w > MAXX)
      x = MAXX-w;
    getbox (undrawbuf, x, y+1, w+1, h);
    line (x, y, x, y+h, menufg);
    line (x, y+h, x+w, y+h, menufg);
    line (x+w, y, x+w, y+h, menufg);
    box (x+1,y+1,x+w-1,y+h-1, menubg);
    for (i = 0; i < mainmenu[menucur].submenuitems; i++) {
      if (mainmenu[menucur].submenu[i].submenuitems == -1) {
	line (x+1, i*12+23, x+w-1, i*12+23, menufg);
      }
      else {
	if (i == submenucur) {
	  box (x+1, i*12+17, x+w-1, i*12+30, menubgh);
	  color = menufgh;
	}
	else color = menufg;
	outtextxy (x + 6, i*12+19, mainmenu[menucur].submenu[i].name, color);
	line (x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3), i*12 + 28,
	      x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3)+7, i*12 + 28,
	      color);
	if (mainmenu[menucur].submenu[i].flag &&
	   *(mainmenu[menucur].submenu[i].change)) {
	  outcharxy (x + 6 + (mainmenu[menucur].submenu[i].flag << 3), i*12+19,
                     check, color);
	}
      }
    }
  }
  while (!done) {
    ch = getxkey() & 0x0FFF; /* ignore lock states */
    /* check to see if its a key that will bring a menu down or execute a
       submenu option */
    if (menudown) {
      for (i = 0; i < mainmenu[menucur].submenuitems; i++)
	if (mainmenu[menucur].submenu[i].submenuitems != -1 &&
	   (ch & 0xff) ==
ASCIItoScan[mainmenu[menucur].submenu[i].name[mainmenu[menucur].submenu[i].shortcut]])
	{
	  putbox (undrawbuf, x, y+1, w+1, h);
	  if (!mainmenu[menucur].submenu[i].flag)
	    mainmenu[menucur].submenu[i].proc();
	  else
	    *(mainmenu[menucur].submenu[i].change) =
		!*(mainmenu[menucur].submenu[i].change);
	  done = -1;
	}
    }
    else {
      for (i = 0; i < 7; i++)
	if (i != menucur && (ch & 0xff) == ASCIItoScan[mainmenu[i].name[mainmenu[i].shortcut]]) {
	  menucur = i;
	  menudown = -1;
	}
    }
    switch (ch & 0xff) {
      case 0x00: if (ch != 0x0200)
		 break;
      case 0x44: if (menudown) putbox (undrawbuf, x, y+1, w+1, h);
		 done = -1;
		 break;
      case 0x01: if (menudown) {
		   putbox (undrawbuf, x, y+1, w+1, h);
		   menudown = submenucur = 0;
		 }
		 else done = -1;
		 break;
      case 0x1C: if (menudown)	{
		   putbox (undrawbuf, x, y+1, w+1, h);
		   if (!mainmenu[menucur].submenu[submenucur].flag)
		     mainmenu[menucur].submenu[submenucur].proc();
		   else
		     *(mainmenu[menucur].submenu[submenucur].change) =
			    !*(mainmenu[menucur].submenu[submenucur].change);
		   done = -1;
		 }
		 else {
		   menudown = -1; submenucur = 0;
		   h = 3 + mainmenu[menucur].submenuitems*12;
		   x = menubar[menucur] - 6;
		   if (x+w > MAXX)
		     x = MAXX-w;
		   getbox (undrawbuf, x, y+1, w+1, h);
		   line (x, y, x, y+h, menufg);
		   line (x, y+h, x+w, y+h, menufg);
		   line (x+w, y, x+w, y+h, menufg);
		   box (x+1,y+1,x+w-1,y+h-1, menubg);
		   for (i = 0; i < mainmenu[menucur].submenuitems; i++) {
		     if (mainmenu[menucur].submenu[i].submenuitems == -1) {
		       line (x+1, i*12+23, x+w-1, i*12+23, menufg);
		     }
		     else {
		       if (i == submenucur) {
			 box (x+1, i*12+17, x+w-1, i*12+30, menubgh);
			 color = menufgh;
		       }
		       else color = menufg;
		       outtextxy (x + 6, i*12+19, mainmenu[menucur].submenu[i].name, color);
		       line (x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3), i*12 + 28,
			     x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3)+7, i*12 + 28,
			     color);
		       if (mainmenu[menucur].submenu[i].flag &&
			   *(mainmenu[menucur].submenu[i].change)) {
			 outcharxy (x + 6 + (mainmenu[menucur].submenu[i].flag << 3), i*12+19,
                                    check, color);
		       }
		     }
		   }
		 }
		 break;
      case 0x48: if (menudown) {
		   if (submenucur == 0)
		     submenucur = mainmenu[menucur].submenuitems - 1;
		   else submenucur--;
		   if (mainmenu[menucur].submenu[submenucur].submenuitems == -1)
		     submenucur--;
		 }
		 else {
		   menudown = -1;
		   submenucur = submenuold = mainmenu[menucur].submenuitems-1;
		   h = 3 + mainmenu[menucur].submenuitems*12;
		   x = menubar[menucur] - 6;
		   if (x+w > MAXX)
		     x = MAXX-w;
		   getbox (undrawbuf, x, y+1, w+1, h);
		   line (x, y, x, y+h, menufg);
		   line (x, y+h, x+w, y+h, menufg);
		   line (x+w, y, x+w, y+h, menufg);
		   box (x+1,y+1,x+w-1,y+h-1, menubg);
		   for (i = 0; i < mainmenu[menucur].submenuitems; i++) {
		     if (mainmenu[menucur].submenu[i].submenuitems == -1) {
		       line (x+1, i*12+23, x+w-1, i*12+23, menufg);
		     }
		     else {
		       if (i == submenucur) {
			 box (x+1, i*12+17, x+w-1, i*12+30, menubgh);
			 color = menufgh;
		       }
		       else color = menufg;
		       outtextxy (x + 6, i*12+19, mainmenu[menucur].submenu[i].name, color);
		       line (x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3), i*12 + 28,
			     x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3)+7, i*12 + 28,
			     color);
		       if (mainmenu[menucur].submenu[i].flag &&
			   *(mainmenu[menucur].submenu[i].change)) {
			 outcharxy (x + 6 + (mainmenu[menucur].submenu[i].flag << 3), i*12+19,
                                    check, color);
		       }
		     }
		   }
		 }
		 break;
      case 0x50: if (menudown) {
		   if (submenucur == mainmenu[menucur].submenuitems - 1)
		     submenucur = 0;
		   else submenucur++;
		   if (mainmenu[menucur].submenu[submenucur].submenuitems == -1)
		     submenucur++;
		 }
		 else {
		   menudown = -1; submenucur = 0;
		   h = 3 + mainmenu[menucur].submenuitems*12;
		   x = menubar[menucur] - 6;
		   if (x+w > MAXX)
		     x = MAXX-w;
		   getbox (undrawbuf, x, y+1, w+1, h);
		   line (x, y, x, y+h, menufg);
		   line (x, y+h, x+w, y+h, menufg);
		   line (x+w, y, x+w, y+h, menufg);
		   box (x+1,y+1,x+w-1,y+h-1, menubg);
		   for (i = 0; i < mainmenu[menucur].submenuitems; i++) {
		     if (mainmenu[menucur].submenu[i].submenuitems == -1) {
		       line (x+1, i*12+23, x+w-1, i*12+23, menufg);
		     }
		     else {
		       if (i == submenucur) {
			 box (x+1, i*12+17, x+w-1, i*12+30, menubgh);
			 color = menufgh;
		       }
		       else color = menufg;
		       outtextxy (x + 6, i*12+19, mainmenu[menucur].submenu[i].name, color);
		       line (x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3), i*12 + 28,
			     x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3)+7, i*12 + 28,
			     color);
		       if (mainmenu[menucur].submenu[i].flag &&
			   *(mainmenu[menucur].submenu[i].change)) {
			 outcharxy (x + 6 + (mainmenu[menucur].submenu[i].flag << 3), i*12+19,
                                    check, color);
		       }
		     }
		   }
		 }
		 break;
      case 0x4B: if (menucur == 0) menucur = 6;
		 else menucur--;
		 if (menudown) {
		   if (submenucur >= mainmenu[menucur].submenuitems)
		     submenucur = submenuold = mainmenu[menucur].submenuitems - 1;
		   if (mainmenu[menucur].submenu[submenucur].submenuitems == -1)
		     submenucur++;
		   putbox (undrawbuf, x, y+1, w+1, h);
		   submenuold = submenucur;
		 }
		 break;
      case 0x4D: if (menucur == 6) menucur = 0;
		 else menucur++;
		 if (menudown) {
		   if (submenucur >= mainmenu[menucur].submenuitems)
		     submenucur = submenuold = mainmenu[menucur].submenuitems - 1;
		   if (mainmenu[menucur].submenu[submenucur].submenuitems == -1)
		     submenucur++;
		   putbox (undrawbuf, x, y+1, w+1, h);
		   submenuold = submenucur;
		 }
    }
    if (menucur != menuold) {
      box (menubar[menuold]-2,0,menubar[menuold] +
	  (strlen(mainmenu[menuold].name) << 3) + 2, 15, menubg);
      outtextxy (menubar[menuold], 3, mainmenu[menuold].name, menufg);
      line (menubar[menuold] + (mainmenu[menuold].shortcut << 3), 12,
	    menubar[menuold] + (mainmenu[menuold].shortcut << 3) + 7, 12, menufg);
      menuold = menucur;
      box (menubar[menucur]-2,0,menubar[menucur] +
	  (strlen(mainmenu[menucur].name) << 3) + 2, 15, menubgh);
      outtextxy (menubar[menucur], 3, mainmenu[menucur].name, menufgh);
      line (menubar[menucur] + (mainmenu[menucur].shortcut << 3), 12,
	    menubar[menucur] + (mainmenu[menucur].shortcut << 3) + 7, 12, menufgh);
      if (menudown) {
	h = 3 + mainmenu[menucur].submenuitems*12;
	x = menubar[menucur] - 6;
	if (x+w > MAXX)
	  x = MAXX-w;
	getbox (undrawbuf, x, y+1, w+1, h);
	line (x, y, x, y+h, menufg);
	line (x, y+h, x+w, y+h, menufg);
	line (x+w, y, x+w, y+h, menufg);
	box (x+1,y+1,x+w-1,y+h-1, menubg);
	for (i = 0; i < mainmenu[menucur].submenuitems; i++) {
	  if (mainmenu[menucur].submenu[i].submenuitems == -1) {
	    line (x+1, i*12+23, x+w-1, i*12+23, menufg);
	  }
	  else {
	    if (i == submenucur) {
	      box (x+1, i*12+17, x+w-1, i*12+30, menubgh);
	      color = menufgh;
	    }
	    else color = menufg;
	    outtextxy (x + 6, i*12+19, mainmenu[menucur].submenu[i].name, color);
	    line (x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3), i*12 + 28,
		  x + 6 + (mainmenu[menucur].submenu[i].shortcut << 3)+7, i*12 + 28,
		  color);
	    if (mainmenu[menucur].submenu[i].flag &&
	       *(mainmenu[menucur].submenu[i].change)) {
	      outcharxy (x + 6 + (mainmenu[menucur].submenu[i].flag << 3), i*12+19,
                         check, color);
	    }
	  }
	}
      }
    }
    if (menudown && submenucur != submenuold) {
      box (x+1, submenuold*12+17, x+w-1, submenuold*12+30, menubg);
      color = menufg;
      outtextxy (x + 6, submenuold*12+19, mainmenu[menucur].submenu[submenuold].name, color);
      line (x + 6 + (mainmenu[menucur].submenu[submenuold].shortcut << 3), submenuold*12 + 28,
	    x + 6 + (mainmenu[menucur].submenu[submenuold].shortcut << 3)+7, submenuold*12 + 28,
	    color);
      if (mainmenu[menucur].submenu[submenuold].flag &&
	 *(mainmenu[menucur].submenu[submenuold].change)) {
	outcharxy (x + 6 + (mainmenu[menucur].submenu[submenuold].flag << 3), submenuold*12+19,
                   check, color);
      }
      submenuold = submenucur;
      box (x+1, submenucur*12+17, x+w-1, submenucur*12+30, menubgh);
      color = menufgh;
      outtextxy (x + 6, submenucur*12+19, mainmenu[menucur].submenu[submenucur].name, color);
      line (x + 6 + (mainmenu[menucur].submenu[submenucur].shortcut << 3), submenucur*12 + 28,
	    x + 6 + (mainmenu[menucur].submenu[submenucur].shortcut << 3)+7, submenucur*12 + 28,
	    color);
      if (mainmenu[menucur].submenu[submenucur].flag &&
	 *(mainmenu[menucur].submenu[submenucur].change)) {
	outcharxy (x + 6 + (mainmenu[menucur].submenu[submenucur].flag << 3), submenucur*12+19,
                   check, color);
      }
    }
  }
  box (menubar[menucur]-2,0,menubar[menucur] +
      (strlen(mainmenu[menucur].name) << 3) + 2, 15, menubg);
  outtextxy (menubar[menucur], 3, mainmenu[menucur].name, menufg);
  line (menubar[menucur] + (mainmenu[menucur].shortcut << 3), 12,
	menubar[menucur] + (mainmenu[menucur].shortcut << 3) + 7, 12, menufg);
  menudown = menucur = submenucur = 0;
  return;
}

/****************************************************************************/

char *getinstrumentname(int bank, int instr) {
  static tmp[24];
  if (bank == 0)
    return Banks[bank].Instrum[instr];
  else {
    sprintf (tmp, "Bank %d, Patch %d", bank+1, instr+1);
    return tmp;
  }
}

/****************************************************************************/

void getinstrument(int def) {
  unsigned int instr, bnk, oldinstr, oldbnk, ch, i, oldmbank, mbank;
  char tmp[8], *instrname;
  if (def == -1)
    def = lastinstr;
  oldinstr = instr = def&0x7;
  oldbnk = bnk = (def>>3)&0xF;
  oldmbank = mbank = (def>>8)&0x7F;
  /* draw window */
  box (100,174,540,306,menubg);
  line (99,173,99,307,menufg);
  line (541,173,541,307,menufg);
  line (99,173,541,173,menufg);
  line (99,307,541,307,menufg);
  outtextxy (116,182,"    Instrument Bank:",menufg);
  outtextxy (116,190,"Instrument Category:",menufg);
  line (159,205,159,270,menufg);
  line (481,205,481,270,menufg);
  line (159,205,481,205,menufg);
  line (159,270,481,270,menufg);
  outtextxy (108,278,"          \030,\031 - Change Instrument",menufg);
  outtextxy (404,282,"<Enter> - Accept",menufg);
  outtextxy (108,286,"<PgUp>,<PgDn> - Change Category",menufg);
  outtextxy (396,290,"<Escape> - Cancel",menufg);
  outtextxy (108,294,"          +,- - Change Bank",menufg);
  sprintf (tmp, "%d", mbank+1);
  outtextxy (292,182,tmp,menufg);
  if (mbank == 0)
    outtextxy (292,190,Banks[bnk].Name,menufg);
  else {
    sprintf (tmp, "%d", bnk+1);
    outtextxy (292,190,tmp,menufg);
  }
  for (i = 0; i < 8; i++) {
    instrname = getinstrumentname (mbank, (bnk<<3)|i);
    outtextxy ((MAXX-(strlen(instrname)<<3))>>1,206+(i<<3),instrname,menufg);
  }
  box (160,206+(instr<<3),480,213+(instr<<3),menubgh);
  instrname = getinstrumentname (mbank, (bnk<<3)|instr);
  outtextxy ((MAXX-(strlen(instrname)<<3))>>1,206+(instr<<3),instrname,menufgh);
  do {
    ch = getxkey() & 0xFFF;
    if (ch == 0x48)
      instr = (instr-1)&0x7;
    else if (ch == 0x50)
      instr = (instr+1)&0x7;
    else if (ch == 0x49)
      bnk = (bnk-1)&0xF;
    else if (ch == 0x51)
      bnk = (bnk+1)&0xF;
    else if (ch == 0x39 && _sound) {
      if (gmidi) UseMIDI();
      else if (opl3) UseOPL3();
      else UseOPL2();
      FM_Set();
      FM_Set_Instrument (0, 0, (mbank<<8)+(bnk<<3)+instr);
      FM_Set_Instrument (1, 0, (mbank<<8)+(bnk<<3)+instr);
      FM_Set_Instrument (2, 0, (mbank<<8)+(bnk<<3)+instr);
      FM_Set_Instrument (3, 0, (mbank<<8)+(bnk<<3)+instr);
      FM_Set_Pan (0, -128);
      FM_Key_On (0,48);
      delay (350);
      FM_Set_Pan (0, 0);
      FM_Key_On (0,60);
      delay (350);
      FM_Set_Pan (0, 127);
      FM_Key_On (0,72);
      delay (350);
      FM_Set_Pan (0, 0);
      FM_Key_On (0,84);
      FM_Set_Pan (1, 127);
      FM_Key_On (1,79);
      FM_Set_Pan (2, -128);
      FM_Key_On (2,67);
      FM_Key_On (3,72);
      delay (700);
      FM_Key_Off (0);
      FM_Key_Off (1);
      FM_Key_Off (2);
      FM_Key_Off (3);
    }
    else {
      char ascii = ASCIIcode(ch);
      if (ascii == '-')
        mbank = (mbank-1)&0x7F;
      else if (ascii == '+')
        mbank = (mbank+1)&0x7F;
    }
    if (oldmbank != mbank) {
      box (292,182,532,189,menubg);
      oldmbank = mbank;
      sprintf (tmp, "%d", mbank+1);
      outtextxy (292,182,tmp,menufg);
      oldbnk = bnk-1; /* this forces the next section to redraw */
    }
    if (oldbnk != bnk) {
      box (292,190,532,197,menubg);
      oldbnk = bnk;
      if (mbank == 0)
        outtextxy (292,190,Banks[bnk].Name,menufg);
      else {
        char tmp[4];
        sprintf (tmp, "%d", bnk+1);
        outtextxy (292,190,tmp,menufg);
      }
      box (160,206,480,269,menubg);
      for (i = 0; i < 8; i++) {
        instrname = getinstrumentname (mbank, (bnk<<3)|i);
        outtextxy ((MAXX-(strlen(instrname)<<3))>>1,206+(i<<3),instrname,menufg);
      }
      box (160,206+(instr<<3),480,213+(instr<<3),menubgh);
      instrname = getinstrumentname (mbank, (bnk<<3)|instr);
      outtextxy ((MAXX-(strlen(instrname)<<3))>>1,206+(instr<<3),instrname,menufgh);
    }
    else if (oldinstr != instr) {
      box (160,206+(oldinstr<<3),480,213+(oldinstr<<3),menubg);
      instrname = getinstrumentname (mbank, (bnk<<3)|oldinstr);
      outtextxy ((MAXX-(strlen(instrname)<<3))>>1,206+(oldinstr<<3),instrname,menufg);
      oldinstr = instr;
      box (160,206+(instr<<3),480,213+(instr<<3),menubgh);
      instrname = getinstrumentname (mbank, (bnk<<3)|instr);
      outtextxy ((MAXX-(strlen(instrname)<<3))>>1,206+(instr<<3),instrname,menufgh);
    }
  } while (ch != 0x01 && ch != 0x1C);
  if (ch == 0x1C) {
    unsigned long curval = page[curpos*NUMVOICES+curvoice];
    lastinstr = (bnk << 3) + instr;
    page[curpos*NUMVOICES+curvoice] = 0xE0080000l + (curval&0xFF) + ((long)lastinstr<<8) + ((long)mbank<<20);
  }
  redraw_screen ();
  if (_sound) FM_Reset();
  return;
}

/****************************************************************************/

void getdrumset(int def) {
  unsigned int bnk, oldbnk, chx;
  char tmp[4], ch;
  bnk = oldbnk = def==-1?0:def;
  box (152,212,488,268,menubg);
  line (151,211,151,269,menufg);
  line (489,211,489,269,menufg);
  line (151,211,489,211,menufg);
  line (151,269,489,269,menufg);
  outtextxy (208,220,"Percussion Bank Number:",menufg);
  sprintf (tmp, "%d", bnk+1);
  outtextxy (408,220,tmp,menufg);
  outtextxy (160,236,"-,+",menufg);
  outtextxy (160,244,"\033,\032 - Change Number",menufg);
  outtextxy (160,252,"\030,\031", menufg);
  outtextxy (344,240," <Enter> - Accept",menufg);
  outtextxy (344,248,"<Escape> - Cancel",menufg);
  do {
    chx = getxkey() & 0xFFF; ch = ASCIIcode(chx);
    if (chx == 0x4B || chx == 0x48 || ch == '-')
      bnk = bnk==0?0:bnk-1;
    else if (chx == 0x4D || chx == 0x50 || ch == '+')
      bnk = bnk==127?127:bnk+1;
    else if (chx == 0x47)
      bnk = 0;
    else if (chx == 0x4F)
      bnk = 127;
    else if (chx == 0x49)
      bnk = bnk<8?0:bnk-8;
    else if (chx == 0x51)
      bnk = bnk>119?127:bnk+8;
    if (bnk != oldbnk) {
      oldbnk = bnk;
      box (408,220,440,228,menubg);
      sprintf (tmp, "%d", bnk+1);
      outtextxy (408,220,tmp,menufg);
    }
  } while (chx != 0x01 && chx != 0x1C);
  if (chx == 0x1C) {
    unsigned long curval = page[curpos*NUMVOICES+curvoice];
    page[curpos*NUMVOICES] = 0xE0088000l + (curval&0xFF) + ((long)bnk<<8);
  }
  redraw_screen ();
  return;
}

/****************************************************************************/

int getval(char *s, int begin, int end, int def, int percent, int lev) {
  int ch = 0, x1 = (MAXX-(end-begin+1))>>1, x2 = x1 + end-begin, xc = def, oldxc = xc;
  char buf[8];
  /* draw window */
  getbox (undrawbuf,155,205,330,70);
  box (156,206,483,273,menubg);
  line (155,205,155,274,menufg);
  line (484,205,484,274,menufg);
  line (155,205,484,205,menufg);
  line (155,274,484,274,menufg);
  outtextxy ((MAXX-(strlen(s)<<3))>>1,214,s,menufg);
  line (x1,242,x2,242,menufg);
  outtextxy (164,254,"\033,\032                    <Enter> - Accept",menufg);
  outtextxy (164,262,"-,+                   <Escape> - Cancel",menufg);
  outtextxy (164,258,"    - Change Value",menufg);
  box (x1-2+xc-begin,239,x1+2+xc-begin,245,menubg);
  line (x1-3+xc-begin,238,x1+3+xc-begin,238,menufg);
  line (x1-3+xc-begin,246,x1+3+xc-begin,246,menufg);
  line (x1-3+xc-begin,238,x1-3+xc-begin,246,menufg);
  line (x1+3+xc-begin,238,x1+3+xc-begin,246,menufg);
  if (percent&&lev)
    if (begin < 0)
      sprintf (buf,"%d%%", (((signed char)(xc&0xFF)+1)*100)>>7);
    else
      sprintf (buf,"%d%%", ((xc&0x7F)*100)/127);
  else if (percent)
    sprintf (buf, "%d%%", (xc*100)/end);
  else
    sprintf (buf, "%d", xc);
  outtextxy ((MAXX-(strlen(buf)<<3))>>1,230,buf,menufg);
  do {
    ch = getxkey() & 0xFFF;
    if (ch == 0x4B || ch == 0x4A || ch == 0xC) {
      if (xc > begin)
        xc--;
    }
    else if (ch == 0x4D || ch == 0x4E || ch == 0x10D) {
      if (xc < end)
        xc++;
    }
    else if (ch == 0x47)
      xc = begin;
    else if (ch == 0x4F)
      xc = end;
    else if (ch == 0x49)
      if (xc > begin+9)
        xc -= 10;
      else
        xc = begin;
    else if (ch == 0x51)
      if (xc < end-9)
        xc += 10;
      else
        xc = end;
    if (xc != oldxc) {
      box (x1-3+oldxc-begin,238,x1+3+oldxc-begin,246,menubg);
      line (x1-3+oldxc-begin,242,x1+3+oldxc-begin,242,menufg);
      box (288,230,352,237,menubg);
      oldxc = xc;
      box (x1-2+xc-begin,239,x1+2+xc-begin,245,menubg);
      line (x1-3+xc-begin,238,x1+3+xc-begin,238,menufg);
      line (x1-3+xc-begin,246,x1+3+xc-begin,246,menufg);
      line (x1-3+xc-begin,238,x1-3+xc-begin,246,menufg);
      line (x1+3+xc-begin,238,x1+3+xc-begin,246,menufg);
      if (percent&&lev)
        if (begin < 0)
          sprintf (buf,"%d%%", (((signed char)(xc&0xFF)+1)*100)>>7);
        else
          sprintf (buf,"%d%%", ((xc&0x7F)*100)/127);
      else if (percent)
        sprintf (buf, "%d%%", (xc*100)/end);
      else
        sprintf (buf, "%d", xc);
      outtextxy ((MAXX-(strlen(buf)<<3))>>1,230,buf,menufg);
    }
  } while (ch != 0x01 && ch != 0x1C);
  putbox (undrawbuf,155,205,330,70);
  if (ch == 0x01)
    return -32768;
  else
    return xc;
}

/****************************************************************************/

void getlev(int pv) {
  unsigned i = (pv)?((unsigned)getval("Pan",-128,127,0,-1,-1)):((unsigned)getval("Volume",0,127,MEZZOFORTE,-1,-1));
  if (i != 0x8000)
    if (pv)
      page[curpos*NUMVOICES+curvoice] = 0xE00A0000l + ((i&0xFF) << 8);
    else
      page[curpos*NUMVOICES+curvoice] = 0x60000300l + ((((unsigned long)i)&0x7Fl) << 10l);
  return;
}

/****************************************************************************/

void getfade(void) {
  unsigned i = (unsigned)getval("Stereo Fade - %/Beat",-128,127,0,-1,-1);
  if (i != 0x8000)
    page[curpos*NUMVOICES+curvoice] = 0xE00E0000l + ((i&0xFF) << 8);
  return;
}

/****************************************************************************/

void getdelta(int t, int d) {
  unsigned i = (t)?((unsigned)getval((d)?("Accelerando - BPM/Measure"):("Ritardando - BPM/Measure"),1,256,20,0,0)):
                   ((unsigned)getval((d)?("Crescendo - %/Beat"):("Dimminuendo - %/Beat"),1,128,13,-1,0));
  if (i-- != 0x8000)
    if (t&&d)
      page[curpos*NUMVOICES] = 0xE0020000l + ((i&0xFF) << 8);
    else if (t)
      page[curpos*NUMVOICES] = 0xE0040000l + ((i&0xFF) << 8);
    else if (d)
      page[curpos*NUMVOICES+curvoice] = 0x60000100l + ((((unsigned long)i)&0x7Fl) << 10l);
    else
      page[curpos*NUMVOICES+curvoice] = 0x60000000l + ((((unsigned long)i)&0x7Fl) << 10l);
  return;
}

/****************************************************************************/

void gethold(void) {
  unsigned i = (unsigned)getval("Fermata - Hold Percent",105,360,150,0,0);
  if (i != 0x8000) {
    i -= 105;
    page[curpos*NUMVOICES+curvoice] = 0xE00C0000l + ((i&0xFF) << 8);
  }
  return;
}

/****************************************************************************/

void getwheel (int d) {
  unsigned i = (d)?((unsigned)getval("Pitch Wheel Roll - %/Beat",-128,127,0,-1,-1)):
                   ((unsigned)getval("Pitch Wheel Position",-128,127,0,-1,-1));
  if (i != 0x8000)
    if (d) {
      unsigned j = ((unsigned)getval("Pitch Wheel Roll - Distance",0,127,64,-1,-1));
      if (j != 0x8000)
        page[curpos*NUMVOICES+curvoice] = 0x61000100l + ((((unsigned long)j)&0x7Fl) << 9l) + ((((unsigned long)i)&0xFFl) << 16l);
    }
    else
      page[curpos*NUMVOICES+curvoice] = 0x61000000l + ((((unsigned long)i)&0xFFl) << 16l);
  return;
}

/****************************************************************************/

void gettempo(void) {
  int t[2];
  if (_tempo (t) == 0)
    /* write it to song */
    page[curpos*NUMVOICES] = 0xE0060000l + (((long)t[0]&0x1FF)<<8) + (((long)t[1]&0xF)<<20);
  return;
}

/****************************************************************************/

void gettimesig(void) {
  int t;
  if (_timesig (&t) == 0)
    /* write it to song */
    page[curpos*NUMVOICES] = 0xE0000000l + (((unsigned long)t&0xFF)<<8);
  return;
}

/****************************************************************************/

int _tempo (int *t) {
  int i, j, k, chx, draw = 0; char ch, tmp[8];

  t[0] = tempo[0];
  t[1] = tempo[1];
  /* draw the window */
  getbox (undrawbuf, 160, 194, 320, 92);
  box (160,194,479,285,menubg);
  line (160,194,160,285,menufg);
  line (479,194,479,285,menufg);
  line (160,194,479,194,menufg);
  line (160,285,479,285,menufg);
  box (240, 206, 319, 213, menubgh);
  outtextxy (240, 206, "Tempo", menufgh);
  sprintf (tmp, "%d bpm", t[0]+1);
  outtextxy (340, 206, tmp, menufg);
  outtextxy (240, 228, "Beat Value", menufg);
  outtextxy (164, 265, "\030,\031 - Change Field     <Enter> - Accept", menufg);
  outtextxy (164, 273, "-,+ - Change Value    <Escape> - Cancel", menufg);
  switch (t[1]&0x7) {
    case 0: draw_symbol (343,233,head1,menufg,putpixel); break;
    case 1: draw_symbol (343,250,head2,menufg,putpixel);
            line (350,250,350,216,menufg); break;
    default:draw_symbol (343,250,head4,menufg,putpixel);
            line (350,250,350,216,menufg);
            if ((t[1]&0x7) > 2) {
              for (k = 0; k < (t[1]&0x7)-3; k++)
                draw_symbol (350,216+k*6,flag1b,menufg,putpixel);
              draw_symbol (350,216+k*6,flag1a,menufg,putpixel);
            }
  }
  if (t[1]&0x08)
    if (t[1] == 0x8)
      draw_symbol (345,237,dot,menufg,putpixel);
    else
      draw_symbol (343,254,dot,menufg,putpixel);
  i=j=0;
  /* process user input */
  do {
    if (kbhit())
      chx = getxkey();
    else
      chx = 0;
    switch (chx&0xFFF) {
      case 0x48:
      case 0x50:if (i==0) i=1; else i=0; break;
      default:
        ch = ASCIIcode(chx);
        if (ch == '-') {
          if (i==0) t[0] = (t[0]-1)&0x1FF;
          else t[1] = (t[1]-1)&0x0F;
          draw = -1;
        }
        else if (ch == '+') {
          if (i==0) t[0] = (t[0]+1)&0x1FF;
          else t[1] = (t[1]+1)&0x0F;
          draw = -1;
        }
    }
    if (i != j)
      if (i==0) {
        box (240, 206, 319, 213, menubgh);
        outtextxy (240, 206, "Tempo", menufgh);
        box (240, 228, 319, 235, menubg);
        outtextxy (240, 228, "Beat Value", menufg);
        j=i;
      }
      else {
        box (240, 206, 319, 213, menubg);
        outtextxy (240, 206, "Tempo", menufg);
        box (240, 228, 319, 235, menubgh);
        outtextxy (240, 228, "Beat Value", menufgh);
        j=i;
      }
    if (draw) {
      if (i==0) {
        box (340,206,396,213,menubg);
        sprintf (tmp, "%d bpm", t[0]+1);
        outtextxy (340, 206, tmp, menufg);
      }
      else {
        box (340,216,396,264,menubg);
        switch (t[1]&0x7) {
          case 0: draw_symbol (343,233,head1,menufg,putpixel); break;
          case 1: draw_symbol (343,250,head2,menufg,putpixel);
                  line (350,250,350,216,menufg); break;
          default:draw_symbol (343,250,head4,menufg,putpixel);
                  line (350,250,350,216,menufg);
                  if ((t[1]&0x7) > 2) {
                    for (k = 0; k < (t[1]&0x7)-3; k++)
                      draw_symbol (350,216+k*6,flag1b,menufg,putpixel);
                    draw_symbol (350,216+k*6,flag1a,menufg,putpixel);
                  }
        }
        if (t[1]&0x08)
          if (t[1] == 0x8)
            draw_symbol (345,237,dot,menufg,putpixel);
          else
            draw_symbol (343,254,dot,menufg,putpixel);
      }
      draw = 0;
    }
  } while ((chx & 0xfff) != 0x01 && (chx & 0xfff) != 0x1C);
  putbox (undrawbuf, 160, 194, 320, 92);
  if ((chx & 0xfff) == 0x01)
    return -1;
  else
    return 0;
}

/****************************************************************************/

int _timesig (int *t) {
  int i, j, x1, x2, x3, y1, y2, chx, draw = 0; char ch;

  *t = timesig;
  /* draw the window */
  getbox (undrawbuf, 160, 206, 320, 68);
  box (160,206,479,273,menubg);
  line (160,206,160,273,menufg);
  line (479,206,479,273,menufg);
  line (160,206,479,206,menufg);
  line (160,273,479,273,menufg);
  box (240, 215, 327, 222, menubgh);
  outtextxy (240, 215, "Numerator", menufgh);
  outtextxy (240, 233, "Denominator", menufg);
  outtextxy (164, 253, "\030,\031 - Change Field     <Enter> - Accept", menufg);
  outtextxy (164, 261, "-,+ - Change Value    <Escape> - Cancel", menufg);
  x1 = *t;
  x2 = x1>>5; x1 &= 0x1f;
  if (x1 == 0) {
    if (x2 == 7) draw_symbol (360,210,cuttime,menufg,putpixel);
    else draw_symbol (360,210,stdtime,menufg,putpixel);
  }
  else {
    x1++;
    x3 = Timenum[x1%10].width + ((x1>9)?(Timenum[x1/10].width):(0));
    if (x3 > DenomWidth[x2]) {
      y2 = (x3-DenomWidth[x2])>>1; y1 = 0;
    }
    else {
      y1 = (DenomWidth[x2]-x3)>>1; y2 = 0;
    }
    y1 += 360; y2 += 360;
    if (x1 > 9) {
      draw_symbol (y1,210,Timenum[x1/10].data,menufg,putpixel);
      y1+=Timenum[x1/10].width;
    }
    draw_symbol (y1,210,Timenum[x1%10].data,menufg,putpixel);
    switch (x2) {
      case 0:draw_symbol(y2,228,Timenum[1].data,menufg,putpixel); break;
      case 1:draw_symbol(y2,228,Timenum[2].data,menufg,putpixel); break;
      case 2:draw_symbol(y2,228,Timenum[4].data,menufg,putpixel); break;
      case 3:draw_symbol(y2,228,Timenum[8].data,menufg,putpixel); break;
      case 4:draw_symbol(y2,228,Timenum[1].data,menufg,putpixel);
             draw_symbol(y2+9,228,Timenum[6].data,menufg,putpixel); break;
      case 5:draw_symbol(y2,228,Timenum[3].data,menufg,putpixel);
             draw_symbol(y2+13,228,Timenum[2].data,menufg,putpixel); break;
      case 6:draw_symbol(y2,228,Timenum[6].data,menufg,putpixel);
             draw_symbol(y2+14,228,Timenum[4].data,menufg,putpixel);
    }
  }
  i=j=0;
  /* process user input */
  do {
    if (kbhit())
      chx = getxkey();
    else
      chx = 0;
    switch (chx&0xFFF) {
      case 0x48:
      case 0x50:if (i==0) i=1; else i=0; break;
      default:
        ch = ASCIIcode(chx);
        if (ch == '-') {
          if (i==0) *t = ((*t)&0xE0) | ((--(*t))&0x1F);
          else if (((*t)&0x1F) == 0) {
            if (((*t)>>5) == 7) *t = 0x40;
            else *t = 0xE0;
          }
          else if (((*t)>>5) == 0) *t = ((*t)&0x1F) | 192;
          else *t = ((*t)&0x1F) | ((*t-32)&0xE0);
          draw = -1;
        }
        else if (ch == '+') {
          if (i==0) *t = ((*t)&0xE0) | ((++(*t))&0x1F);
          else if (((*t)&0x1F) == 0) {
            if (((*t)>>5) == 7) *t = 0x40;
            else *t = 0xE0;
          }
          else if (((*t)>>5) == 6) *t &= 0x1F;
          else *t = ((*t)&0x1F) | ((*t+32)&0xE0);
          draw = -1;
        }
    }
    if (i!=j)
      if (i==0) {
        box (240, 215, 327, 222, menubgh);
        outtextxy (240, 215, "Numerator", menufgh);
        box (240, 233, 327, 241, menubg);
        outtextxy (240, 233, "Denominator", menufg);
        j=i;
      }
      else {
        box (240, 215, 327, 222, menubg);
        outtextxy (240, 215, "Numerator", menufg);
        box (240, 233, 327, 241, menubgh);
        outtextxy (240, 233, "Denominator", menufgh);
        j=i;
      }
    if (draw) {
      box (360,210,391,246,menubg);
      x1 = *t;
      x2 = x1>>5; x1 &= 0x1f;
      if (x2 == 7 && x1 != 0) {
        x2 = 1; *t = (x2<<5) | x1;
      }
      else if (x2 != 7 && x1 == 0) {
        x2 = 2; *t = (x2<<5) | x1;
      }
      if (x1 == 0) {
        if (x2 == 7) draw_symbol (360,210,cuttime,menufg,putpixel);
        else draw_symbol (360,210,stdtime,menufg,putpixel);
      }
      else {
        x1++;
        x3 = Timenum[x1%10].width + ((x1>9)?(Timenum[x1/10].width):(0));
        if (x3 > DenomWidth[x2]) {
          y2 = (x3-DenomWidth[x2])>>1; y1 = 0;
        }
        else {
          y1 = (DenomWidth[x2]-x3)>>1; y2 = 0;
        }
        y1 += 360; y2 += 360;
        if (x1 > 9) {
          draw_symbol (y1,210,Timenum[x1/10].data,menufg,putpixel);
          y1+=Timenum[x1/10].width;
        }
        draw_symbol (y1,210,Timenum[x1%10].data,menufg,putpixel);
        switch (x2) {
          case 0:draw_symbol(y2,228,Timenum[1].data,menufg,putpixel); break;
          case 1:draw_symbol(y2,228,Timenum[2].data,menufg,putpixel); break;
          case 2:draw_symbol(y2,228,Timenum[4].data,menufg,putpixel); break;
          case 3:draw_symbol(y2,228,Timenum[8].data,menufg,putpixel); break;
          case 4:draw_symbol(y2,228,Timenum[1].data,menufg,putpixel);
                 draw_symbol(y2+9,228,Timenum[6].data,menufg,putpixel); break;
          case 5:draw_symbol(y2,228,Timenum[3].data,menufg,putpixel);
                 draw_symbol(y2+13,228,Timenum[2].data,menufg,putpixel); break;
          case 6:draw_symbol(y2,228,Timenum[6].data,menufg,putpixel);
                 draw_symbol(y2+14,228,Timenum[4].data,menufg,putpixel);
        }
      }
      draw = 0;
    }
  } while ((chx & 0xfff) != 0x01 && (chx & 0xfff) != 0x1C);
  putbox (undrawbuf, 160, 206, 320, 68);
  if ((chx & 0xfff) == 0x01)
    return -1;
  else
    return 0;
}

/****************************************************************************/

int isblank(int voice, int column) {
  unsigned long i = page[column*NUMVOICES];
  if ((page[column*NUMVOICES+voice]&0xE0000000l) != 0l)
    return 0;
  else if ((i&0xE0000000l) == 0x80000000l)
    return 0;
  else if ((i&0xE0000000l) == 0xA0000000l)
    return 0;
  else if ((i&0xE0000000l) == 0xC0000000l)
    return 0;
  else if ((i&0xE0080000l) == 0xE0000000l)
    return 0;
  else if ((i&0xE1000800l) == 0x40000800l)
    return 0;
  else if ((i&0xE00E8000l) == 0xE0088000l)
    return 0;
  else
    return -1;
}

/****************************************************************************/

int ISCONTROL(unsigned long i) {
  if ((i&0xE0000000l) == 0x80000000l)
    return -1;
  else if ((i&0xE0000000l) == 0xA0000000l)
    return -1;
  else if ((i&0xE0000000l) == 0xC0000000l)
    return -1;
  else if ((i&0xE0080000l) == 0xE0000000l)
    return -1;
  else if ((i&0xE1000800l) == 0x40000800l)
    return -1;
  else
    return 0;
}

/****************************************************************************/

int iscontrol(int column) {
  return ISCONTROL (page[column*NUMVOICES]);
}

/****************************************************************************/

void play_perc(void) {
  int instr = 35, oldinstr = 35, top = 35, oldtop = 35, ch, i;
  char buf[60];
  /* draw window */
  box (100,182,540,298,menubg);
  line (99,181,99,299,menufg);
  line (541,181,541,299,menufg);
  line (99,181,541,181,menufg);
  line (99,299,541,299,menufg);
  outtextxy (260,190,"Percussion List",menufg);
  line (159,205,159,270,menufg);
  line (481,205,481,270,menufg);
  line (159,205,481,205,menufg);
  line (159,270,481,270,menufg);
  outtextxy (108,278,"          \030,\031                        <Enter>",menufg);
  outtextxy (108,282,"              - Change Voice                 - Done",menufg);
  outtextxy (108,286,"<PgUp>,<PgDn>                       <Escape>",menufg);
  for (i = 0; i < 8; i++) {
    outtextxy (168,206+(i<<3),Drums[116-i-top],menufg);
    switch ((116-i-top)%12) {
      case 0: strcpy (buf, "C  "); break;
      case 1: strcpy (buf, "C# "); break;
      case 2: strcpy (buf, "D  "); break;
      case 3: strcpy (buf, "Eb "); break;
      case 4: strcpy (buf, "E  "); break;
      case 5: strcpy (buf, "F  "); break;
      case 6: strcpy (buf, "F# "); break;
      case 7: strcpy (buf, "G  "); break;
      case 8: strcpy (buf, "G# "); break;
      case 9: strcpy (buf, "A  "); break;
     case 10: strcpy (buf, "Bb "); break;
     case 11: strcpy (buf, "B  ");
    }
    switch ((116-i-top)/12) {
      case 2: strcat (buf, "3 octaves below middle C"); break;
      case 3: strcat (buf, "2 octaves below middle C"); break;
      case 4: strcat (buf, "1 octave below middle C"); break;
      case 5: strcat (buf, "octave of middle C"); break;
      case 6: strcat (buf, "1 octave above middle C");
    }
    outtextxy (264,206+(i<<3),buf,menufg);
  }
  box (166,206+((instr-top)<<3),234,213+((instr-top)<<3),menubgh);
  outtextxy (166,206+((instr-top)<<3),Drums[116-instr],menufgh);
  do {
    ch = getxkey() & 0xFFF;
    if (ch == 0x48) {
      instr--;
      if (instr < 35)
        instr = 35;
    }
    else if (ch == 0x50) {
      instr++;
      if (instr > 81)
        instr = 81;
    }
    else if (ch == 0x49) {
      top -= 8;
      if (top < 35)
        top = 35;
      instr = top + (instr-oldtop);
    }
    else if (ch == 0x51) {
      top += 8;
      if (top > 74)
        top = 74;
      instr = top + (instr-oldtop);
    }
    else if (ch == 0x39 && _sound) {
      if (gmidi) UseMIDI();
      else if (opl3) UseOPL3();
      else UseOPL2();
      FM_Set();
      FM_Set_Percussion (-1);
      FM_Key_On (6,116-instr);
      delay (500);
      FM_Key_Off (6);
    }
    if (instr > top + 7) {
      if (instr > 74)
        top = 74;
      else
        top = instr;
    }
    else if (instr < top)
      top = instr;
    if (oldtop != top) {
      box (160,206,480,269,menubg);
      oldtop = top;
      oldinstr = instr;
      for (i = 0; i < 8; i++) {
        outtextxy (168,206+(i<<3),Drums[116-i-top],menufg);
        switch ((116-i-top)%12) {
          case 0: strcpy (buf, "C  "); break;
          case 1: strcpy (buf, "C# "); break;
          case 2: strcpy (buf, "D  "); break;
          case 3: strcpy (buf, "Eb "); break;
          case 4: strcpy (buf, "E  "); break;
          case 5: strcpy (buf, "F  "); break;
          case 6: strcpy (buf, "F# "); break;
          case 7: strcpy (buf, "G  "); break;
          case 8: strcpy (buf, "G# "); break;
          case 9: strcpy (buf, "A  "); break;
         case 10: strcpy (buf, "Bb "); break;
         case 11: strcpy (buf, "B  ");
        }
        switch ((116-i-top)/12) {
          case 2: strcat (buf, "3 octaves below middle C"); break;
          case 3: strcat (buf, "2 octaves below middle C"); break;
          case 4: strcat (buf, "1 octave below middle C"); break;
          case 5: strcat (buf, "octave of middle C"); break;
          case 6: strcat (buf, "1 octave above middle C");
        }
        outtextxy (260,206+(i<<3),buf,menufg);
      }
      box (166,206+((instr-top)<<3),234,213+((instr-top)<<3),menubgh);
      outtextxy (168,206+((instr-top)<<3),Drums[116-instr],menufgh);
    }
    else if (oldinstr != instr) {
      box (168,206+((oldinstr-top)<<3),234,213+((oldinstr-top)<<3),menubg);
      outtextxy (166,206+((oldinstr-top)<<3),Drums[116-oldinstr],menufg);
      oldinstr = instr;
      box (168,206+((instr-top)<<3),234,213+((instr-top)<<3),menubgh);
      outtextxy (166,206+((instr-top)<<3),Drums[116-instr],menufgh);
    }         
  } while (ch != 0x01 && ch != 0x1C);
  redraw_screen ();
  if (_sound) FM_Reset();
  return;
}

/****************************************************************************/

void getkeypan(void) {
  unsigned n, i;
  if ((n = getnote()) == 0x8000)
    return;
  i = (unsigned)getval("Keyboard Pan Range - Half-Steps",1,64,24,0,0);
  if (i != 0x8000) {
    i--;
    page[curpos*NUMVOICES+curvoice] = 0x41000000l + ((((unsigned long)n)&0x7Fl) << 16l) + ((((unsigned long)i)&0x3Fl) << 8l);
  }
  return;
}

/****************************************************************************/

char *notenames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" },
     *octdesc[11] = { "5 octaves below middle",
                      "4 octaves below middle",
                      "3 octaves below middle",
                      "2 octaves below middle",
                      "1 octave below middle",
                      "middle octave",
                      "1 octave above middle",
                      "2 octaves above middle",
                      "3 octaves above middle",
                      "4 octaves above middle",
                      "5 octaves above middle" };

int getnote (void) {
  int ch = 0, x1 = (MAXX-128)>>1, x2 = x1 + 127, xc = 60, oldxc = xc;
  char buf[36];
  /* draw window */
  getbox (undrawbuf,155,205,330,70);
  box (156,206,483,273,menubg);
  line (155,205,155,274,menufg);
  line (484,205,484,274,menufg);
  line (155,205,484,205,menufg);
  line (155,274,484,274,menufg);
  outtextxy ((MAXX-(26<<3))>>1,214,"Keyboard Pan Center - Note",menufg);
  line (x1,242,x2,242,menufg);
  outtextxy (164,254,"\033,\032                    <Enter> - Accept",menufg);
  outtextxy (164,262,"-,+                   <Escape> - Cancel",menufg);
  outtextxy (164,258,"    - Change Value",menufg);
  box (x1-2+xc,239,x1+2+xc,245,menubg);
  line (x1-3+xc,238,x1+3+xc,238,menufg);
  line (x1-3+xc,246,x1+3+xc,246,menufg);
  line (x1-3+xc,238,x1-3+xc,246,menufg);
  line (x1+3+xc,238,x1+3+xc,246,menufg);
  sprintf (buf, "%s - %s (%d)", notenames[xc%12], octdesc[xc/12], xc);
  outtextxy ((MAXX-(strlen(buf)<<3))>>1,230,buf,menufg);
  do {
    ch = getxkey() & 0xFFF;
    if (ch == 0x4B || ch == 0x4A || ch == 0xC) {
      if (xc > 0)
        xc--;
    }
    else if (ch == 0x4D || ch == 0x4E || ch == 0x10D) {
      if (xc < 127)
        xc++;
    }
    else if (ch == 0x47)
      xc = 0;
    else if (ch == 0x4F)
      xc = 127;
    else if (ch == 0x49) {
      if (xc > 11)
        xc -= 12;
      else
        xc = 0;
    }
    else if (ch == 0x51) {
      if (xc < 116)
        xc += 12;
      else
        xc = 127;
    }
    if (xc != oldxc) {
      box (x1-3+oldxc,238,x1+3+oldxc,246,menubg);
      line (x1-3+oldxc,242,x1+3+oldxc,242,menufg);
      box (160,230,480,237,menubg);
      oldxc = xc;
      box (x1-2+xc,239,x1+2+xc,245,menubg);
      line (x1-3+xc,238,x1+3+xc,238,menufg);
      line (x1-3+xc,246,x1+3+xc,246,menufg);
      line (x1-3+xc,238,x1-3+xc,246,menufg);
      line (x1+3+xc,238,x1+3+xc,246,menufg);
      sprintf (buf, "%s - %s (%d)", notenames[xc%12], octdesc[xc/12], xc);
      outtextxy ((MAXX-(strlen(buf)<<3))>>1,230,buf,menufg);
    }
  } while (ch != 0x01 && ch != 0x1C);
  putbox (undrawbuf,155,205,330,70);
  if (ch == 0x01)
    return -32768;
  else
    return xc;
}
