/* FILE: load.c
 * AUTHOR: Josh Humphries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fm.h"



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
  fclose (f);
  return 0;
}
