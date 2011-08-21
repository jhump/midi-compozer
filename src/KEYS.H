/* FILE: keys.h
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This header file contains info for calling program to use alternate
 * keyboard ISR for enhanced keyboard I/O.
 */

extern void installkeyboardISR(void);
extern void unloadkeyboardISR(void);
extern char ASCIIcode (unsigned key);
extern unsigned getxkey(void);
extern unsigned char getkey(void);
extern int kbhit(void);

