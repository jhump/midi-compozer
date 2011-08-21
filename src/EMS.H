/* FILE: ems.h
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file has some EMS routine prototypes from ems.c
 */

extern void *EMSpageframe;

extern int checkEMS (void);
extern unsigned EMSpagesavail (void);
extern int EMShandlesavail (void);
extern int EMSalloc (unsigned pages);
extern int EMSmap (int handle, char phys, unsigned log);
extern int EMSfree (int handle);
extern unsigned EMSrealloc (int handle, unsigned newpages);


