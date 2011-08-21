#include <stdio.h>

int main (void) {
  FILE *f, *g;
  char buf[80];
  if ((f = fopen ("k", "r")) == NULL) {
    perror ("k");
    return 1;
  }
  if ((g = fopen ("..\\keys.aeh", "w")) == NULL) {
    perror ("..\\keys.aeh");
    return 1;
  }
  while (fgets (buf, 80, f)) {
    int i, j;
    for (i = j = 0; buf[i]; i++)
      if (buf[i] != '\r' && buf[i] != '\n')
        buf[j++] = buf[i];
      buf[j] = 0;
    fwrite (buf, strlen(buf), 1, g);
  }
  fclose (f); fclose (g);
  return 0;
}


