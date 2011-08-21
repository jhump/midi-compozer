/* FILE: gr.h
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains prototypes to stuff in gr.c
 */

#define MAXX 639
#define MAXY 479
#define XDIM 640
#define YDIM 480
#define MODE 0x101

extern int low, high;

/* initialize video mode to 640x480x256 via VESA */
extern int VESAInit (void);

/* return to last text mode */
extern void VESAQuit (void);

/* page flip routine to access >64k of video memory */
extern void VESAPage (short page);

/* plot a single pixel */
extern void putpixel (int x, int y, char color);
extern void cputpixel (int x, int y, char color);
/* unplot a single pixel drawn with cputpixel */
extern void unputpixel (int x, int y, char color);

/* get a pixel value from the screen */
extern char getpixel (int x, int y);

/* routine to set a palette entry */
extern void setrgbpalette (char index, char r, char g, char b);

/* draw lines */
extern void line (int x1, int y1, int x2, int y2, char color);
extern void cline (int x1, int y1, int x2, int y2, char color);
/* undraw lines drawn with cline */
extern void unline (int x1, int y1, int x2, int y2, char color);

/* solid filled box */
extern void box (int x1, int y1, int x2, int y2, char color);

/* get box from screen */
extern void getbox (char *data, int startx, int starty, int xdist, int ydist);

/* put box to screen */
extern void putbox (char *data, int startx, int starty, int xdist, int ydist);

/* write character pattern to the screen */
extern void outcharxy (int x, int y, char *data, char color);

/* write text to the screen */
extern void outtextxy (int x, int y, char *s, char color);

/* bezier-curve renderers */
extern void curve (int P1X, int P1Y, int P2X, int P2Y, int P3X, int P3Y, int P4X, int P4Y, char color);
extern void ccurve (int P1X, int P1Y, int P2X, int P2Y, int P3X, int P3Y, int P4X, int P4Y, char color);
/* undraw bezier-curves rendered with ccurve */
extern void uncurve (int P1X, int P1Y, int P2X, int P2Y, int P3X, int P3Y, int P4X, int P4Y, char color);

