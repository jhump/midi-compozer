#include <stdio.h>

int main (void) {
  char buffer[512]; int i, j;
  while (fgets(buffer, 512, stdin)) {
    for (i = j = 0; buffer[i]; i++) {
      if (buffer[i] != 10 && buffer[i] != 13)
        buffer[j++] = buffer[i];
    }
    buffer[j] = 0;
    fwrite (buffer, strlen(buffer), 1, stdout);
  }
  return 0;
}
