/* FILE: data.h
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *   This file contains declarations for all the external data necessary
 * for the program.
 */

extern char treble[], alto[], bass[], whole[], half[], quarter[], _8th[], _16th[],
	    _32nd[], _64th[], sharp[], flat[], natural[], doublesharp[], doubleflat[],
	    sbar[], dbar[], begs[], ends[], begr[], endr[], fermata[], head4[],
	    head2[], head1[], dot[], _MIDI[], _Compozer[], st1[], st2[], flag1a[],
	    flag1b[], flag2a[], flag2b[], stdtime[], cuttime[], coda[], segno[],
            accent[], *file[];
extern struct {
  char *Name;
  char **Instrum;
} Banks[16];

extern struct {
  char *data;
  int   width;
} Font1[128], Font2[128], Font3[128], Font4[128], Timenum[10];

int DenomWidth[8] = {9,13,14,15,23,26,28,0};

