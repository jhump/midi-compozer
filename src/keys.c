/* FILE: keys.c
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file provides an alternate keyboard ISR for better keyboard I/O
 * along with some helper routines to that end.
 */

#include <dos.h>

#define LOCKFLAGS (*(char *)(0x00400017l))

volatile char shiftflags[3];
volatile char bufhead = 0;
volatile char buftail = 0;
volatile char timer = 0;
volatile unsigned keybuffer[32];
int oldshiftflags;
void interrupt (*oldkeyboardISR)(void), (*oldtimerISR)(void);
unsigned char prefix[81] =
{ 0x1c, 0x1d, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
  0xaa, 0xab, 0x54, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4,
  0xb5, 0x35, 0xb6, 0x54, 0x38, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd,
  0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xe1, 0x47, 0x48, 0x49, 0xc4, 0x4b,
  0xc5, 0x4d, 0xc6, 0x4f, 0x50, 0x51, 0x52, 0x53, 0xc7, 0xc8, 0xc9, 0xca,
  0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xc1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
  0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf },
	      ASCII[128] =
{0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,'q','w','e','r','t',
 'y','u','i','o','p','[',']',13,0,'a','s','d','f','g','h','j','k','l',';','\'',
 '`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,'\\',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	      ASCIIshift[128] =
{0,27,'!','@','#','$','%','^','&','*','(',')','_','+',8,9,'Q','W','E','R','T',
 'Y','U','I','O','P','{','}',13,0,'A','S','D','F','G','H','J','K','L',':','"',
 '~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,0,
 0,0,0,0,0,'7','8','9','-','4','5','6','+','1','2','3','0','.',0,0,'|',0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void putbuf (unsigned code) {
  char tmp = buftail+1;
  tmp &= 0x1F;
  if (tmp == bufhead) {
    nosound(); sound(880); timer = 2;
  }
  else {
    keybuffer[buftail] = code; buftail = tmp;
  }
  return;
}

unsigned getbuf (void) {
  char tmp = bufhead;
  if (tmp != buftail) {
    bufhead++; bufhead &= 0x1F;
    return keybuffer[tmp];
  }
  else
    return 0xffff;
}

unsigned keycode (unsigned scan) {
  unsigned char lo, hi;
  /* first determine lo-byte - will equal low-byte of scan unless hi-byte
     is non-zero (0xE0 or 0xE1) */
  hi = scan >> 8;
  lo = scan & 0xff;
  if (hi == 0xE1)
    lo = 0xE1;
  else if (hi == 0xE0) {

    if ((lo -= 0x1c) > 80)
      lo = 0xE0;
    else
      lo = prefix[lo];
  }
  /* then determine hi-byte which will be a mask of shift flags */
  hi = 0;
	/* low nibble is a mask of shift key flags */
  if (shiftflags[0]) hi |= 0x01; /* shift key */
  if (shiftflags[1]) hi |= 0x02; /* alt key */
  if (shiftflags[2]) hi |= 0x04; /* ctrl key */
	/* high nibble is a mask of lock state flags */
  hi |= LOCKFLAGS & 0x70;
  /* done... */
  return ((unsigned)hi << 8) + lo;
}

char ASCIIcode (unsigned key) {
  char tmp;
  if ((key & 0x0FFF)== 0x04E1) /* ctrl+break will return ^C */
    return 0x03;
  else if (key & 0x80) /* all other scan codes over 0x7F are unknown */
    return 0;
  if (key & 0x0200) 	/* alt key always return zero then scancode */
    return 0;
  if (key & 0x0400) {	/* ctrl key */
    if ((tmp = ASCIIcode((key & 0xff) | 0x100)) > 63 && tmp < 96)
      return tmp - 64;
    else if (tmp == 0x0d)
      return 0x0a;
    else if (tmp == 0x08)
      return 0x7f;
    else
      return 0;
  }
  /* toggle shift flag for keypad keys if num lock */
  if ((key & 0x2000) && (key & 0xff) > 0x46 && (key & 0xff) < 0x54)
    if (key & 0x0100) key &= 0xfeff;
    else key |= 0x0100;
  /* toggle shift flag for alpha keys if caps lock */
  if ((key & 0x4000) && (((key & 0xff) > 0x0F && (key & 0xff) < 0x1A) ||
     ((key & 0xff) > 0x1D && (key & 0xff) < 0x27) ||
     ((key & 0xff) > 0x2B && (key & 0xff) < 0x33)))
    if (key & 0x0100) key &= 0xfeff;
    else key |= 0x0100;
  if (key & 0x0100) 	/* shift key */
    return ASCIIshift[key & 0x7F];
  else
    return ASCII[key & 0x7F];
}

void interrupt keyboardISR (void) {
  static unsigned char prefix = 0x00;
  static unsigned lastcode = 0x0000;
  unsigned char scan, tmp;
  unsigned code;

  /* read scancode for key */
  scan = inportb (0x60);
  /* clear the key */
  tmp = inportb (0x61);
  outportb (0x61, tmp | 0x80);
  outportb (0x61, tmp);
  /* if it's a prefix, store it and exit */
  if (scan == 0xE0 || scan == 0xE1) {
    prefix = scan;
    outportb (0x20, 0x20);
    return;
  }
  /* store scancode plus any applicable prefix into code */
  code = scan + ((unsigned)prefix << 8);
  /* reset prefix so it doesn't kluge upcoming keystrokes... */
  prefix = 0x00;
  /* get the keycode and place it in the key buffer... beep if buffer full
     OR update shiftflags if necessary */
  if (code & 0x80)	/* break code */
    switch (code & 0xff7f) {
      /* lock keys change lights on break code because typematic rate may
	 throw off state if user holds down key */
      case 0x003A: if (LOCKFLAGS & 0x40)	/* caps lock */
		     LOCKFLAGS &= 0xBF;
		   else
		     LOCKFLAGS |= 0x40;
		   break;
      case 0x0045: if (lastcode != 0xE19D)   /* avoid pause key confusion */
		     if (LOCKFLAGS & 0x20)	/* num lock */
		       LOCKFLAGS &= 0xDF;
		     else
		       LOCKFLAGS |= 0x20;
		   break;
      case 0x0046: if (LOCKFLAGS & 0x10)	/* scroll lock */
		     LOCKFLAGS &= 0xEF;
		   else
		     LOCKFLAGS |= 0x10;
		   break;
      /* update shift flags */
      case 0x0036: if (lastcode == 0x0036)
		     putbuf (0x0100 | (((unsigned)LOCKFLAGS & 0x70) << 8));
		   shiftflags[0] = 0; break;	/* shift key */
      case 0x002A: if (lastcode == 0x002A)
		     putbuf (0x0100 | (((unsigned)LOCKFLAGS & 0x70) << 8));
		   shiftflags[0] = 0; break;	/* shift key */
      case 0x0038: if (lastcode == 0x0038)
		     putbuf (0x0200 | ((unsigned)(LOCKFLAGS & 0x70) << 8));
		   shiftflags[1] = 0; break;	/* alt key */
      case 0xE038: if (lastcode == 0xE038)
		     putbuf (0x0200 | ((unsigned)(LOCKFLAGS & 0x70) << 8));
		   shiftflags[1] = 0; break;	/* alt key */
      case 0x001D: if (lastcode == 0x001D)
		     putbuf (0x0400 | ((unsigned)(LOCKFLAGS & 0x70) << 8));
		   shiftflags[2] = 0; break;	/* ctrl key */
      case 0xE01D: if (lastcode == 0xE01D)
		     putbuf (0x0400 | ((unsigned)(LOCKFLAGS & 0x70) << 8));
		   shiftflags[2] = 0;		/* ctrl key */
    }
  else			/* make code */
    switch (code) {
      case 0xE02A:
      case 0xE036: break; /* bogus codes generated by some E0 keys
			     when shift keys are down */
      case 0x002A:
      case 0x0036: shiftflags[0] = -1; break;	/* shift key */
      case 0x0038:
      case 0xE038: shiftflags[1] = -1; break;	/* alt key */
      case 0x001D:
      case 0xE01D: shiftflags[2] = -1; break;	/* ctrl key */
      default: /* put key in keyboard buffer */
	if (code != 0x0045 || lastcode != 0xE11D)
	  /* this way bogus code generated by pause key won't go */
	  putbuf (keycode(code));
	else
	  code = 0xE145;
    }
  lastcode = code;
  /* update lock LEDs through BIOS (safe and easy way...) */
  asm {
    mov ah, 0x01
    int 0x16
  }
  /* return EOI to PIC */
  outportb (0x20, 0x20);
  return;
}

void interrupt timerISR (void) {
  if (timer > 0 && --timer == 0) nosound();
  return;
}

void installkeyboardISR (void) {
  /* save BIOS lock state */
  oldshiftflags = LOCKFLAGS;
  /* install our ISR */
  oldkeyboardISR = getvect (0x09);
  oldtimerISR = getvect (0x1C);
  setvect (0x09, keyboardISR);
  setvect (0x1C, timerISR);
  return;
}

void unloadkeyboardISR (void) {
  /* restore BIOS lock state */
  LOCKFLAGS = oldshiftflags;
  /* restore original ISR */
  setvect (0x09, oldkeyboardISR);
  setvect (0x1C, oldtimerISR);
  /* just in case keyboard ISR started a beep and we uninstalled timer
     ISR before it finished */
  nosound();
  return;
}

unsigned getxkey (void) {
  unsigned val = 0xffff;
  while (val == 0xffff) val = getbuf();
  return val;
}

unsigned char getkey (void) {
  static unsigned char last = 0xff;
  static unsigned next;
  if (last == 0)
    last = ((next &= 0xff)== 0)?(0xff):(next);
  else
    last = ASCIIcode(next = getxkey());
  return last;
}

int kbhit (void) {
  return bufhead - buftail;
}
