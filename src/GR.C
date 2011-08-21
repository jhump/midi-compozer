/* FILE: gr.c
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains source to a few basic graphic routines but requires
 * a VESA 1.2+ compatible display adapter (via hardware or TSR) capable of
 * 640x480x256 video mode.
 */

#include <stdlib.h>
#include <conio.h>
#include <dos.h>
#include <mem.h>

#define MAXX 639
#define MAXY 479
#define XDIM 640
#define YDIM 480
#define MODE 0x101

/* the following are for 800x600... maybe a later version will allow
 * different modes (i.e. other than the current 640x480) and dynamically
 * resize the display... that would be kewl...
 */
/*
#define MAXX 799
#define MAXY 599
#define XDIM 800
#define YDIM 600
#define MODE 0x103
*/

#define STEP(a)     {s = r + ((long)1 << (a)); 	\
		     r = r >> 1;                \
		     if (s <= v) {		\
		       v -= s;                  \
		       r |= (long)1 << (a);     \
		     }}

char lastpage = 0;
unsigned char VESAInfo[256];
unsigned VESAGran;
int low = 0, high = MAXY;
extern unsigned char mg, fg;

/* initialize video mode to 800x600x256 via VESA */
int VESAInit (void) {
  unsigned segm, offs; int retval;

  segm = FP_SEG(VESAInfo);
  offs = FP_OFF(VESAInfo);
  asm {
    mov  ax, 0x4F02
    mov  bx, MODE
    int  0x10
    cmp  ah, 0x00
    jne  Fail
    cmp  al, 0x4F
    jne  Fail
  }
  delay (100);
  asm {
    mov  ax, 0x4F01
    mov  cx, MODE
    mov  dx, segm
    mov  es, dx
    mov  di, offs
    int  0x10
    mov  cx, es:[di+4]
    mov  ax, 0x0001
    test cx, cx
    jz   Gran64
    mov  ax, 0x0040
    div  cl
Gran64:
    mov  VESAGran, ax
    mov  retval, 0
  }
  asm {
    jmp  Done
Fail:
    mov  retval, -1
Done:
  }
  return retval;
}

/* return to last text mode */
void VESAQuit (void) {
  asm {
    mov  ax, 0x0002
    int  0x10
  }
  textmode (LASTMODE);
  return;
}

/* page flip routine to access >64k of video memory */
void VESAPage (char page) {
  asm {
    pusha
    xor  ah, ah
    mov  al, page
    mul  VESAGran
    mov  dx, ax
    push dx
    xor  bx, bx
    mov  ax, 0x4F05
    int  0x10
    mov  bx, 0x0001
    mov  ax, 0x4F05
    int  0x10
    pop  cx
    cmp  dx, cx
    je   Success
    /* if failed try once more */
    xor  ah, ah
    mov  al, page
    mul  VESAGran
    mov  dx, ax
    xor  bx, bx
    mov  ax, 0x4F05
    int  0x10
Success:
    popa
  }
  return;
}

/* plot a single pixel */
void putpixel (int x, int y, char color) {
  long offs, page;
  char *temp;

  if (x < XDIM && x >= 0 && y <= high && y >= low) {
    page = (long)(((long)(x) + (long)(y)*(long)(XDIM)) >> 16);
    offs = (long)(((long)(x) + (long)(y)*(long)(XDIM)) & 0xFFFF);
    if (page != lastpage) {
      VESAPage (page);
      VESAPage (page);	// call it again to be sure - sometimes a write
			// to video memory may occur before hardware
			// has properly switched banks...
      lastpage = page;
    }
    temp = MK_FP(0xA000,offs);
    *temp = color;
  }
  return;
}

/* get a pixel value from the screen */
char getpixel (int x, int y) {
  long offs, page;
  char *temp;

  if (x > MAXX || y > MAXY)
    return 0;
  else {
    page = (long)(((long)(x) + (long)(y)*(long)(XDIM)) >> 16);
    offs = (long)(((long)(x) + (long)(y)*(long)(XDIM)) & 0xFFFF);
    if (page != lastpage) {
      VESAPage (page);
      VESAPage (page);
      lastpage = page;
    }
    temp = MK_FP(0xA000,offs);
    return *temp;
  }
}

/* plot a single pixel */
void cputpixel (int x, int y, char color) {
  unsigned char tmp;
  tmp = getpixel (x, y);
  if ((tmp & 0x3F) == mg) tmp |= 0x80;
  putpixel (x, y, color | (tmp & 0xC0));
  return;
}

/* unplot a single pixel drawn with cputpixel */
void unputpixel (int x, int y, char color) {
  unsigned char tmp;
  tmp = getpixel (x, y);
  if (tmp & 0x40)
    putpixel (x, y, fg | (tmp & 0xC0));
  else if (tmp & 0x80)
    putpixel (x, y, mg | (tmp & 0x40));
  else
    putpixel (x, y, color);
  return;
}

/* routine to set a palette entry */
void setrgbpalette (char index, char r, char g, char b) {
  outportb(0x3C8, index);
  outportb(0x3C9, r);
  outportb(0x3C9, g);
  outportb(0x3C9, b);
  return;
}

/* draw a line */
void _line (int x1, int y1, int x2, int y2, char color,
	    void (*plot)(int x, int y, char color)) {
  int dx, dy, sx, sy, count, brc, brmax;

  sx = 1;
  sy = 1;
  dx = x2 - x1;
  dy = y2 - y1;
  if (dx < 0) {
    dx = -dx;
    sx = -sx;
  }
  if (dy < 0) {
    dy = -dy;
    sy = -sy;
  }
  if (dx > dy) {
    brmax = dx;
    brc = dx / 2;
    plot (x1, y1, color);
    for (count = dx; count >= 1; count--) {
      x1 = x1 + sx;
      brc = brc + dy;
      if (brc > brmax) {
	brc = brc - dx;
	y1 = y1 + sy;
      }
      plot (x1, y1, color);
    }
  }
  else {
    brmax = dy;
    brc = dy / 2;
    plot (x1, y1, color);
    for (count = dy; count >= 1; count--) {
      y1 = y1 + sy;
      brc = brc + dx;
      if (brc > brmax) {
	brc = brc - dy;
	x1 = x1 + sx;
      }
      plot (x1, y1, color);
    }
  }
  return;
}

/* draw a line */
void line (int x1, int y1, int x2, int y2, char color) {
  _line (x1, y1, x2, y2, color, putpixel);
  return;
}
void cline (int x1, int y1, int x2, int y2, char color) {
  _line (x1, y1, x2, y2, color, cputpixel);
  return;
}
void unline (int x1, int y1, int x2, int y2, char color) {
  _line (x1, y1, x2, y2, color, unputpixel);
  return;
}

/* solid filled box */
void box (int x1, int y1, int x2, int y2, char color) {
  int startx, starty, xdist, ydist, ycount, diff;
  unsigned long offs, page;

  if (x1 > MAXX) x1 = MAXX; if (x2 > MAXX) x2 = MAXX;
  if (y1 > MAXY) y1 = MAXY; if (y2 > MAXY) y2 = MAXY;
  if (y2 > y1) {
    starty = y1;
    ydist = y2 - y1 + 1;
  }
  else {
    starty = y2;
    ydist = y1 - y2 + 1;
  }
  if (x2 > x1) {
    startx = x1;
    xdist = x2 - x1 + 1;
  }
  else {
    startx = x2;
    xdist = x1 - x2 + 1;
  }
  page = (long)(((long)(starty)*(XDIM) + startx)) >> 16;
  VESAPage (page);
  VESAPage (page);
  for (ycount = 0; ycount < ydist; ycount++) {
    offs = (long)(((long)(ycount) + starty)*(XDIM)) + startx;
    if (((offs+xdist) >> 16) > (offs >> 16)) {
      diff = (offs+xdist) & 0xFFFF;
      memset (MK_FP(0xA000,offs & 0xFFFF), color, xdist-diff);
      page++;
      VESAPage (page);
      VESAPage (page);
      memset (MK_FP(0xA000,0), color, diff);
    }
    else {
      if ((offs >> 16) > page) {
	page++;
	VESAPage (page);
	VESAPage (page);
      }
      memset (MK_FP(0xA000, offs & 0xFFFF), color, xdist);
    }
  }
  lastpage = page;
  return;
}

/* get box from screen */
void getbox (char *data, int startx, int starty, int xdist, int ydist) {
  int ycount, diff;
  unsigned long offs, page;

  page = (long)(((long)(starty)*(XDIM) + startx)) >> 16;
  VESAPage (page);
  VESAPage (page);
  for (ycount = 0; ycount < ydist; ycount++) {
    offs = (long)(((long)(ycount) + starty)*(XDIM)) + startx;
    if (((offs+xdist) >> 16) > (offs >> 16)) {
      diff = (offs+xdist) & 0xFFFF;
      memcpy (data + ycount*xdist, MK_FP(0xA000,offs & 0xFFFF), xdist-diff);
      page++;
      VESAPage (page);
      VESAPage (page);
      memcpy (data + ycount*xdist + xdist-diff, MK_FP(0xA000,0), diff);
    }
    else {
      if ((offs >> 16) > page) {
	page++;
	VESAPage (page);
	VESAPage (page);
      }
      memcpy (data + ycount*xdist, MK_FP(0xA000,offs & 0xFFFF), xdist);
    }
  }
  lastpage = page;
  return;
}

/* put box to screen */
void putbox (char *data, int startx, int starty, int xdist, int ydist) {
  int ycount, diff;
  unsigned long offs, page;

  page = (long)(((long)(starty)*(XDIM) + startx)) >> 16;
  VESAPage (page);
  VESAPage (page);
  for (ycount = 0; ycount < ydist; ycount++) {
    offs = (long)(((long)(ycount) + starty)*(XDIM)) + startx;
    if (((offs+xdist) >> 16) > (offs >> 16)) {
      diff = (offs+xdist) & 0xFFFF;
      memcpy (MK_FP(0xA000,offs & 0xFFFF), data + ycount*xdist, xdist-diff);
      page++;
      VESAPage (page);
      VESAPage (page);
      memcpy (MK_FP(0xA000,0), data + ycount*xdist + xdist-diff, diff);
    }
    else {
      if ((offs >> 16) > page) {
	page++;
	VESAPage (page);
	VESAPage (page);
      }
      memcpy (MK_FP(0xA000,offs & 0xFFFF), data + ycount*xdist, xdist);
    }
  }
  lastpage = page;
  return;
}

/* write character pattern to the screen */
void outcharxy (int x, int y, char *data, char color) {
  register unsigned xc, yc;
  register unsigned char bitmask;
  for (yc = y; yc < y + 8; yc++) {
    bitmask = 0x80;
    for (xc = x; xc < x + 8; xc++) {
      if (*data & bitmask)
        putpixel (xc, yc, color);
      bitmask >>= 1;
    }
    data++;
  }
  return;
}

/* write text to the screen */
void outtextxy (int x, int y, unsigned char *s, char color) {
  static char *p = NULL;
  char *blity;
  int charcounter;

  if (!p) p = (char *)getvect (0x1F);
  for (charcounter = 0; s[charcounter]; charcounter++) {
    if (s[charcounter] > 127)
      blity = p + ((s[charcounter]-128) << 3);
    else
      blity = (char *)MK_FP(0xF000,0xFA6E) + (s[charcounter] << 3);
    outcharxy ((charcounter << 3) + x, y, blity, color);
  }
  return;
}

/* fast square root for drawing bezier curves */
int fastsqrt(long v) {
  long r = 0, s;
  STEP(30) STEP(28) STEP(26) STEP(24) STEP(22) STEP(20) STEP(18) STEP (16)
  STEP(14) STEP(12) STEP(10) STEP(8)  STEP(6)  STEP(4)  STEP(2)  STEP(0)
  return r;
}

/* function to determine if a curve is straight enough to approximate with a line */
int straight(long x1, long y1, long x2, long y2, long x3, long y3, long x4, long y4) {
  long numer, denom, d1, d2, p1x, p1y, p2x, p2y, p3x, p3y,p2p1x, p2p1y, p3p1x,
       p3p1y;

  p1x = x1;
  p1y = y1;
  p2x = x4;
  p2y = y4;
  p2p1x = p1x - p2x;
  p2p1y = p1y - p2y;
  p3x = x2;
  p3y = y2;
  p3p1x = p1x - p3x;
  p3p1y = p1y - p3y;
  numer = labs(((p2p1x * p3p1y) >> 8) - ((p3p1x * p2p1y) >> 8));
  denom = fastsqrt(((p2p1x * p2p1x) >> 8) + ((p2p1y * p2p1y) >> 8));
  if (denom == 0)
    d1 = 1024;
  else
    d1 = (numer / denom) << 8;
  p3x = x3;
  p3y = y3;
  p3p1x = p1x - p3x;
  p3p1y = p1y - p3y;
  numer = abs (((p2p1x * p3p1y) >> 8) - ((p3p1x * p2p1y) >> 8));
  denom = fastsqrt (((p2p1x * p2p1x) >> 8) + ((p2p1y * p2p1y) >> 8));
  if (denom == 0)
    d2 = 1024;
  else
    d2 = (numer / denom) << 8;
  if (d1 < 128 && d2 < 128)
    return -1;
  else
    return 0;
}

/* fixed point bezier-curve renderer */
void curvefixed (long _P1X, long _P1Y, long _P2X, long _P2Y, long _P3X, long _P3Y,
		 long _P4X, long _P4Y, char color, long R,
		 void (*plot)(int x, int y, char color)) {
  long LP1X, LP1Y, LP2X, LP2Y, LP3X, LP3Y, LP4X, LP4Y, RP1X, RP1Y, RP2X, RP2Y,
       RP3X, RP3Y, RP4X, RP4Y, HX, HY;

  if (straight(_P1X, _P1Y, _P2X, _P2Y, _P3X, _P3Y, _P4X, _P4Y) || R > 8)
    _line (_P1X >> 8, _P1Y >> 8, _P4X >> 8, _P4Y >> 8, color, plot);
  else {
    LP1X = _P1X;
    LP1Y = _P1Y;
    RP4X = _P4X;
    RP4Y = _P4Y;
    LP2X = (_P1X + _P2X) >> 1;
    LP2Y = (_P1Y + _P2Y) >> 1;
    HX = (_P2X + _P3X) >> 1;
    HY = (_P2Y + _P3Y) >> 1;
    LP3X = (LP2X + HX) >> 1;
    LP3Y = (LP2Y + HY) >> 1;
    RP3X = (_P3X + _P4X) >> 1;
    RP3Y = (_P3Y + _P4Y) >> 1;
    RP2X = (HX + RP3X) >> 1;
    RP2Y = (HY + RP3Y) >> 1;
    RP1X = (LP3X + RP2X) >> 1;
    RP1Y = (LP3Y + RP2Y) >> 1;
    LP4X = RP1X;
    LP4Y = RP1Y;
    curvefixed (LP1X, LP1Y, LP2X, LP2Y, LP3X, LP3Y, LP4X, LP4Y, color, R+1, plot);
    curvefixed (RP1X, RP1Y, RP2X, RP2Y, RP3X, RP3Y, RP4X, RP4Y, color, R+1, plot);
  }
  return;
}

/* bezier-curve renderer */
void curve (int P1X, int P1Y, int P2X, int P2Y, int P3X, int P3Y, int P4X, int P4Y, char color) {
  long _P1X, _P1Y, _P2X, _P2Y, _P3X, _P3Y, _P4X, _P4Y;
  _P1X = (long)(P1X) << 8;
  _P1Y = (long)(P1Y) << 8;
  _P2X = (long)(P2X) << 8;
  _P2Y = (long)(P2Y) << 8;
  _P3X = (long)(P3X) << 8;
  _P3Y = (long)(P3Y) << 8;
  _P4X = (long)(P4X) << 8;
  _P4Y = (long)(P4Y) << 8;
  curvefixed (_P1X, _P1Y, _P2X, _P2Y, _P3X, _P3Y, _P4X, _P4Y, color, 1, putpixel);
  return;
}
void ccurve (int P1X, int P1Y, int P2X, int P2Y, int P3X, int P3Y, int P4X, int P4Y, char color) {
  long _P1X, _P1Y, _P2X, _P2Y, _P3X, _P3Y, _P4X, _P4Y;
  _P1X = (long)(P1X) << 8;
  _P1Y = (long)(P1Y) << 8;
  _P2X = (long)(P2X) << 8;
  _P2Y = (long)(P2Y) << 8;
  _P3X = (long)(P3X) << 8;
  _P3Y = (long)(P3Y) << 8;
  _P4X = (long)(P4X) << 8;
  _P4Y = (long)(P4Y) << 8;
  curvefixed (_P1X, _P1Y, _P2X, _P2Y, _P3X, _P3Y, _P4X, _P4Y, color, 1, cputpixel);
  return;
}
void uncurve (int P1X, int P1Y, int P2X, int P2Y, int P3X, int P3Y, int P4X, int P4Y, char color) {
  long _P1X, _P1Y, _P2X, _P2Y, _P3X, _P3Y, _P4X, _P4Y;
  _P1X = (long)(P1X) << 8;
  _P1Y = (long)(P1Y) << 8;
  _P2X = (long)(P2X) << 8;
  _P2Y = (long)(P2Y) << 8;
  _P3X = (long)(P3X) << 8;
  _P3Y = (long)(P3Y) << 8;
  _P4X = (long)(P4X) << 8;
  _P4Y = (long)(P4Y) << 8;
  curvefixed (_P1X, _P1Y, _P2X, _P2Y, _P3X, _P3Y, _P4X, _P4Y, color, 1, unputpixel);
  return;
}
