/* FILE: function.h
 * AUTHOR: Josh Humphries
 * PURPOSE:
 *    This is file contains prototype of all the multitudes of routines
 * in function.c.... just in case main.c needs some of them to be public
 * for it... It also declares all the functions.c variables...
 */

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
#define ISBARBACK(a)  	(((a) & 0xE0000004l) == 0x80000004l)
#define ISBARNORMAL(a)	(((a) & 0xE0000004l) == 0x80000000l)

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
extern void display_startup(void);
extern int initEMS(void);
extern void load_config(void);
extern void save_config(void);
extern int load_instrum(void);
extern void change_palette(void);
extern void display_startup(void);
extern void redraw_column(int column);
extern void patch_staves(int column);
extern void redraw_music(int begin, int end);
extern void redraw_info(void);
extern int save_changes(void);
extern void cline (int x1, int y1, int x2, int y2, char color);
extern void draw_nsymbol(int x, int y, char *data, char color);
extern void draw_csymbol(int x, int y, char *data, char color);
extern void draw_col(void);
extern void undraw_col(void);
extern void drawrow_nvoice(char v, int begin, int end);
extern void drawrow_cvoice(char v, int begin, int end);
extern void undrawrow_nvoice(char v, int begin, int end);
extern void undrawrow_cvoice(char v, int begin, int end);
extern void draw_voice(int column, char voice, int staffy, char color,
                void (*plot)(int x, int y, char color),
		void (*drawline)(int x1, int y1, int x2, int y2, char color),
		void (*drawfont)(int font, int x, int y, char *str, char color, int sp),
		void (*drawcurve)(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, char color));
extern void draw_nvoice(int column, char voice, int staffy, char color);
extern void draw_cvoice(int column, char voice, int staffy, char color);
extern void undraw_nvoice(int column, char voice, int staffy);
extern void undraw_cvoice(int column, char voice, int staffy);
extern void redraw_staff(char staff, int begin, int end);
extern void drawstaff (char staff, int begin, int end);
extern void enter_editmode(void);
extern void exit_editmode(void);
extern int fontlength(int font, char *str);
extern void fontout(int font, int x, int y, char *str, char color, int sp);
extern void cfontout(int font, int x, int y, char *str, char color, int sp);
extern void unfontout(int font, int x, int y, char *str, char color, int sp);
extern void draw_symbol(int x, int y, char *data, char color,
                 void (*plot)(int x, int y, char color));
extern void Message(char *str);
extern void BrowseFile(char *name);
extern void getkeysig(int col);
extern void writekeysig(void);
extern void getaccidentals(char voice);
extern int isblank(int voice, int column);
extern int iscontrol(int column);
         /* actions started by builtin key actions */
extern void navigate_menu(void);
extern void getlev(int pv);
extern char *getinstrumentname(int bank, int instr);
extern void getinstrument(int def);
extern void getdrumset(int def);
extern void getdelta(int t, int d);
extern void getwheel(int d);
extern void getfade(void);
extern void gethold(void);
extern void getkeypan(void);
extern int getnote (void);
extern void gettempo(void);
extern void gettimesig(void);
         /* actions started by menu option or hot key */
extern void redraw_screen(void);
extern void file_info(void);
extern void new_song(void);
extern void open_song(void);
extern int save_song(void);
extern int saveas(void);
extern void export_midi(void);
extern void quit(void);
extern void undo(void);
extern void cut(void);
extern void copy(void);
extern void paste(void);
extern void clear(void);
extern void transpose(void);
extern void playback(void);
extern void playscreen(void);
extern void playfromscreen(void);
extern void playfrompos(void);
extern void default_tempo(void);
extern void voice_setup(void);
extern void staff_setup(void);
extern void color_select(void);
extern void view_help(void);
extern void view_keys(void);
 
/* globals for color settings */
extern unsigned char  bg, fg, mg, keyb, wkey, bkey, wkeyp, bkeyp, fgp, mgp,
                      selbg, selfg, selmg, menubg, menufg, menubgh, menufgh,
                      col;
extern unsigned char *colors[];
/* EMS memory handle and number of pages allocated */
extern int EMShandle[], mappedpage;
extern unsigned pages;
/* edit controls */
extern int editmode, markbegin, markend, voice[], undovoice[],
           clipbvoice[], undopos, undosize, undopage, clipboardsize;
extern unsigned long *clipboard, *undobuffer;
/* song display and playback variables */
extern int opl3, numstaves, curpage, curvoice, curpos, curstaff,
           curkey[], changed, curstaffy, hipage, drawlines, drawcontrol,
           _sound, timesig, tempo[], mute[];
extern signed char staffinfo[], voiceinfo[];
extern unsigned char colbuf[];
extern char filename[], composer[], opus[], title[];
extern unsigned long *song, *page;
/* globals for menu structure */
extern menu filesubmenu[], editsubmenu[], playbacksubmenu[],
            songsubmenu[], optionssubmenu[], miscsubmenu[],
            helpsubmenu[], mainmenu[];
extern unsigned menubar[];
extern hotkey hkeys[];
/* miscellaneous */
extern int quitflag, menucur, submenucur, menudown;
extern char undrawbuf[];
extern char ASCIItoScan[], check[];
extern FM_Instrument GeneralMIDI[], Percussion[];
extern char DrumNames[], *Drums[];
/* converting our notes to MIDI notes & MIDI notes to FM-synth frequency numbers */
extern char MIDINote[];
extern int FMNumbers[];
