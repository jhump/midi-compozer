/* FILE: getfile.h
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    This file contains the routines in getfile.c
 */

extern void installErrorHandler(void);
extern void unloadErrorHandler(void);
extern char *GetFileName(int autof, char *mask);
