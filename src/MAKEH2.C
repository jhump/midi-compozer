unsigned char data1[81000], data2[81000];

#include <stdio.h>

void main (void) {
  int yc, yb, ye, xm = 540, xc, bi, cur, i, j;
  unsigned char buf[150][65], tmp;
  FILE *f;

  f = fopen("data1.tga","r");
  fseek(f,18,SEEK_SET);
  fread (data1, 81000, 1, f);
  fclose (f);
  f = fopen("data2.tga","r");
  fseek(f,18,SEEK_SET);
  fread (data2, 81000, 1, f);
  fclose (f);

  printf ("char _MIDI[] =\n{");
  memset (buf, 0, 150*65);
  for (yc = 0; yc < 150; yc++)
    for (xc = 0, bi = 1, cur = -1; xc < xm; xc++) {
      if ((tmp = data1[yc*540+xc]) < 255 && cur) {
        bi++; (buf[yc][bi])++; (buf[yc][0])++; cur = 0;
      }
      else if (tmp == 255 && !cur) {
        bi++; (buf[yc][bi])++; cur = -1;
      }
      else
        (buf[yc][bi])++;
    }
  for (yb = 0; yb < 150 && buf[yb][0] == 0; yb++);
  for (ye = 150; ye > 0 && buf[ye-1][0] == 0; ye--);
  printf ("%d, %d, 0,", ye-yb, yb);
  for (i = yb; i < ye; i++) {
    if (((i-yb) & 3) == 0) printf ("\n ");
    printf ("%d,", buf[i][0]);
    if (buf[i][0]) {
      for (bi = 0; bi < buf[i][0]; bi++) {
        printf ("%d,%d", buf[i][bi*2+1], buf[i][bi*2+2]);
        if (bi < buf[i][0]-1) printf (",");
      }
      if (i < ye-1) printf (",      ");
    }
    else printf ("      ");
  }
  printf ("},\n");

  printf ("     _Compozer[] =\n{");
  memset (buf, 0, 150*65);
  for (yc = 0; yc < 150; yc++)
    for (xc = 0, bi = 1, cur = -1; xc < xm; xc++) {
      if ((tmp = data2[yc*540+xc]) < 255 && cur) {
        bi++; (buf[yc][bi])++; (buf[yc][0])++; cur = 0;
      }
      else if (tmp == 255 && !cur) {
        bi++; (buf[yc][bi])++; cur = -1;
      }
      else
        (buf[yc][bi])++;
    }
  for (yb = 0; yb < 150 && buf[yb][0] == 0; yb++);
  for (ye = 150; ye > 0 && buf[ye-1][0] == 0; ye--);
  printf ("%d, %d, 0,", ye-yb, yb);
  for (i = yb; i < ye; i++) {
    if (((i-yb) & 3) == 0) printf ("\n ");
    printf ("%d,", buf[i][0]);
    if (buf[i][0]) {
      for (bi = 0; bi < buf[i][0]; bi++) {
        printf ("%d,%d", buf[i][bi*2+1], buf[i][bi*2+2]);
        if (bi < buf[i][0]-1) printf (",");
      }
      if (i < ye-1) printf (",      ");
    }
    else printf ("      ");
  }
  printf ("};\n");

  return;
}
