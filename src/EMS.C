/* FILE: ems.c
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file has some EMS routines to allow programs access to upper
 * memory. All it contains is a check for EMS, check for amount available and
 * a routine to allocate, map, and deallocate EMS memory.
 */

#include <dos.h>
#include <fcntl.h>
#include <io.h>

void *EMSpageframe;

int checkEMS (void) {
  int fd; unsigned segm; char stat;

  if ((fd = open ("EMMXXXX0", O_RDWR)) == -1)
    return -1;
  else {
    close (fd);
    asm {
      mov ah, 0x41
      int 0x67
      mov stat, ah
      mov segm, bx
    }
    if (stat || !segm)
      return -1;
    else
      EMSpageframe = MK_FP(segm,0);
    return 0;
  }
}

unsigned EMSpagesavail (void) {
  unsigned avail; char stat;

  asm {
    mov ah, 0x46	/* this interrupt hopefully prevents DOS 6.0 */
    int 0x67		/* from crashing when EMM386.EXE is in AUTO mode */
    mov ah, 0x42
    int 0x67
    mov avail, bx
    mov stat, ah
  }
  if (stat)
    return 0xffff; /* this will completely foul things up if the user has a
		      GIGAbyte of available RAM... I doubt that will be a
		      problem though... :)  */
  else return avail;
}

int EMShandlesavail (void) {
  unsigned used; char stat;

  asm {
    mov ah, 0x4B
    int 0x67
    mov used, bx
    mov stat, ah
  }
  if (stat)
    return -1;
  return 255-used;
}

int EMSalloc (unsigned pages) {
  int handle;
  unsigned char stat;

  asm {
    mov ah, 0x43
    mov bx, pages
    int 0x67
    mov stat, ah
    mov handle, dx
  }
  if (stat)
    return -1;
  else
    return handle;
}

int EMSmap (int handle, char phys, unsigned log) {
  char stat;

  asm {
    mov ah, 0x44
    mov al, phys
    mov bx, log
    mov dx, handle
    int 0x67
    mov stat, ah
  }
  if (stat)
    return -1;
  else
    return 0;
}

int EMSfree (int handle) {
  char stat;

  asm {
    mov ah, 0x45
    mov dx, handle
    int 0x67
    cmp ah, 0x80
    je _Retry
    cmp ah, 0x81
    je _Retry
    cmp ah, 0x86
    je _Retry
    jmp _FreeEnd
_Retry:
    mov ah, 0x45
    mov dx, handle
    int 0x67
_FreeEnd:
    mov stat, ah
  }
  if (stat)
    return -1;
  else
    return 0;
}

unsigned EMSrealloc (int handle, unsigned newpages) {
  unsigned char stat;

  asm {
    mov ah, 0x51
    mov bx, newpages
    mov dx, handle
    int 0x67
    mov stat, ah
    cmp ah, 0x87
    je _reallocFail
    cmp ah, 0x88
    je _reallocFail
    mov stat, ah
    jmp _reallocEnd
_reallocFail:
    mov stat, 0
_reallocEnd:
    mov newpages, bx
  }
  if (stat)
    return 0xFFFF;
  else
    return newpages;
}

