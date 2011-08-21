#include <stdio.h>
#include <math.h>

int main (void) {
  int i, j;
  printf ("char FMVolume[128] =\n{");
  for (i = 0; i < 128; i++) {
    if ((j = (int)(64*pow(i/127.0,0.42))) < 10) printf (" ");
    printf ("%d", j);
    if (i==127) printf ("};\n");
    else if ((i&7)==7) printf (",\n ");
    else printf (", ");
  }
  return 0;
}
