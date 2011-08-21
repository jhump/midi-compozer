unsigned char font1[39702], font2[29141], font3[8827],font4[20672];
int  font1c[190] =
{0,4,10,11,25,14,41,13,57,22,85,21,110,4,118,8,127,8,140,9,153,15,170,5,178,7,
 188,4,195,8,205,12,221,10,234,12,249,12,265,12,280,12,295,12,310,12,325,12,
 340,12,356,4,366,5,375,15,392,15,408,15,427,11,441,24,467,20,489,18,509,17,
 528,20,551,17,571,16,589,21,611,22,634,10,646,14,662,22,685,17,704,26,731,20,
 753,20,775,17,794,20,816,20,838,14,855,16,874,20,895,20,916,28,945,19,967,19,
 987,17,1010,6,1017,8,1026,6,1038,13,1052,15,1069,5,1079,12,1094,14,1111,10,
 1124,14,1141,10,1153,11,1164,12,1179,14,1196,6,1202,9,1215,15,1232,6,1241,20,
 1265,14,1282,12,1297,14,1314,14,1331,11,1344,9,1355,9,1366,11,1382,14,1397,19,
 1419,13,1433,14,1448,11,1463,8,1475,2,1481,8,1492,15,1507,20},
     font2c[190] =
{0,2,8,6,17,11,31,10,44,18,65,17,85,2,90,7,98,7,109,9,120,13,136,4,143,6,153,2,
 158,7,167,10,182,6,193,10,206,9,219,10,232,9,245,10,258,10,271,10,284,10,
 298,2,305,4,312,13,327,14,343,13,358,8,370,20,392,17,411,14,428,14,445,15,
 463,13,479,12,493,16,511,15,529,6,538,8,548,16,566,13,582,19,603,16,622,15,
 640,12,654,15,672,15,689,10,702,14,718,16,736,16,754,22,779,16,797,16,814,14,
 832,5,839,7,848,5,856,10,868,12,882,3,891,10,902,11,916,9,928,11,941,9,953,10,
 963,11,976,12,989,6,995,6,1005,12,1018,6,1026,17,1046,12,1060,10,1072,11,
 1086,11,1098,8,1108,7,1117,7,1125,12,1138,12,1151,17,1169,12,1182,12,1195,10,
 1210,7,1222,1,1228,7,1237,13,1250,17},
     font3c[190] =
{0,1,5,3,10,6,17,5,24,9,35,9,45,1,48,3,53,3,58,5,65,7,73,2,77,3,83,1,86,3,90,5,
 98,3,104,5,111,5,118,6,125,5,132,5,139,5,146,5,153,5,161,1,165,2,168,6,176,6,
 184,6,192,4,198,10,210,8,220,7,229,7,238,8,248,7,256,6,264,8,274,8,284,3,
 289,4,295,8,305,7,313,11,325,8,335,8,345,6,353,8,363,8,372,5,380,7,388,8,
 398,7,408,11,420,8,430,8,440,7,449,3,453,3,456,3,462,3,469,6,477,2,481,5,
 486,6,494,4,500,6,507,4,513,5,518,5,525,6,532,3,535,3,540,6,547,3,551,9,561,6,
 568,5,575,6,582,6,589,4,594,4,600,3,604,6,610,7,618,9,628,5,635,7,642,5,649,3,
 655,1,659,3,665,5,670,9},
     font4c[190] =
{0,5,14,6,26,8,38,9,53,11,71,11,91,2,99,7,106,7,122,6,134,9,148,3,157,8,170,2,
 178,8,190,7,202,7,215,8,228,8,241,8,254,8,268,8,282,7,294,7,306,8,320,3,330,4,
 342,9,357,9,372,9,388,6,399,14,417,11,433,11,451,11,466,11,482,13,498,13,
 516,12,531,15,550,8,562,10,574,13,590,10,605,17,625,14,644,11,659,11,676,11,
 692,10,707,11,723,11,739,11,756,11,772,14,789,14,808,10,820,12,836,8,848,2,
 855,8,870,7,883,5,896,3,905,8,919,7,931,7,944,8,957,7,967,12,980,9,994,8,
 1009,4,1015,8,1028,8,1041,5,1051,12,1069,8,1083,8,1094,10,1109,8,1122,6,
 1134,6,1146,5,1157,8,1171,6,1184,10,1198,9,1211,8,1225,7,1237,7,1247,5,1255,7,
 1269,8,1278,14};

#include <stdio.h>

void writedata1 (int i);
void writedata2 (int i);
void writedata3 (int i);
void writedata4 (int j);

void main (void) {
  FILE *f;
  int   i, j;
  f = fopen("font1.tga","r");
  fseek(f,18,SEEK_SET);
  fread (font1, 39702, 1, f);
  fclose (f);
  f = fopen("font2.tga","r");
  fseek(f,18,SEEK_SET);
  fread (font2, 29141, 1, f);
  fclose (f);
  f = fopen("font3.tga","r");
  fseek(f,18,SEEK_SET);
  fread (font3, 8827, 1, f);
  fclose (f);
  f = fopen("font4.tga","r");
  fseek(f,18,SEEK_SET);
  fread (font4, 20672, 1, f);
  fclose (f);
  printf ("/* FILE: fonts.h\n");
  printf (" * AUTHOR: Joshua Humphries\n");
  printf (" * PURPOSE:\n");
  printf (" *    This file contains data for the fonts...\n");
  printf (" */\n\n");
  printf ("char _32[] = {0,0,0},\n");
  for (i = 0; i < 95; i++)
    writedata1 (i);
  for (i = 0; i < 95; i++)
    writedata2 (i);
  for (i = 0; i < 95; i++)
    writedata3 (i);
  for (i = 0; i < 95; i++)
    writedata4 (i);
  printf ("struct {\n  char *data;\n  int width;\n} Font1[128] =");
  for (i = 0; i < 128; i++) {
    if ((i & 3) == 0)
      if (i == 0) printf ("\n{");
      else printf ("\n ");
    if (i == 32)
      printf ("{_32, 12}, ");
    else {
      printf ("{_%d_1, %d}", (i > 32)?(i):(127), ((i > 32)?(font1c[(i-33)*2+1]):(font1c[189]))+2);
      if (i == 127) printf ("},\n");
      else printf (", ");
    }
  }
  printf ("  Font2[128] =");
  for (i = 0; i < 128; i++) {
    if ((i & 3) == 0)
      if (i == 0) printf ("\n{");
      else printf ("\n ");
    if (i == 32)
      printf ("{_32, 8}, ");
    else {
      printf ("{_%d_2, %d}", (i > 32)?(i):(127), ((i > 32)?(font2c[(i-33)*2+1]):(font2c[189]))+1);
      if (i == 127) printf ("},\n");
      else printf (", ");
    }
  }
  printf ("  Font3[128] =");
  for (i = 0; i < 128; i++) {
    if ((i & 3) == 0)
      if (i == 0) printf ("\n{");
      else printf ("\n ");
    if (i == 32)
      printf ("{_32, 4}, ");
    else {
      printf ("{_%d_3, %d}", (i > 32)?(i):(127), ((i > 32)?(font3c[(i-33)*2+1]):(font3c[189]))+1);
      if (i == 127) printf ("},\n");
      else printf (", ");
    }
  }
  printf ("  Font4[128] =");
  for (i = 0; i < 128; i++) {
    if ((i & 3) == 0)
      if (i == 0) printf ("\n{");
      else printf ("\n ");
    if (i == 32)
      printf ("{_32, 5}, ");
    else {
      printf ("{_%d_4, %d}", (i > 32)?(i):(127), ((i > 32)?(font4c[(i-33)*2+1]):(font4c[189]))+1);
      if (i == 127) printf ("};\n");
      else printf (", ");
    }
  }
  return;
}

void writedata1 (int i) {
  int yc, yb, ye, xo = font1c[i*2], xm = font1c[i*2+1], xc, bi, cur;
  unsigned char buf[26][17], tmp;

  printf ("     _%d_1[] =\n{", i+33);
  memset (buf, 0, 26*17);
  for (yc = 0; yc < 26; yc++)
    for (xc = 0, bi = 1, cur = -1; xc < xm; xc++) {
      if ((tmp = font1[yc*1527+xo+xc]) < 255 && cur) {
        bi++; (buf[yc][bi])++; (buf[yc][0])++; cur = 0;
      }
      else if (tmp == 255 && !cur) {
        bi++; (buf[yc][bi])++; cur = -1;
      }
      else
        (buf[yc][bi])++;
    }
  for (yb = 0; yb < 26 && buf[yb][0] == 0; yb++);
  for (ye = 26; ye > 0 && buf[ye-1][0] == 0; ye--);
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
  return;
}

void writedata2 (int i) {
  int yc, yb, ye, xo = font2c[i*2], xm = font2c[i*2+1], xc, bi = 1, cur = -1;
  unsigned char buf[23][17], tmp;

  printf ("     _%d_2[] =\n{", i+33);
  memset (buf, 0, 23*17);
  for (yc = 0; yc < 23; yc++)
    for (xc = 0, bi = 1, cur = -1; xc < xm; xc++) {
      if ((tmp = font2[yc*1267+xo+xc]) < 255 && cur) {
        bi++; (buf[yc][bi])++; (buf[yc][0])++; cur = 0;
      }
      else if (tmp == 255 && !cur) {
        bi++; (buf[yc][bi])++; cur = -1;
      }
      else
        (buf[yc][bi])++;
    }
  for (yb = 0; yb < 23 && buf[yb][0] == 0; yb++);
  for (ye = 23; ye > 0 && buf[ye-1][0] == 0; ye--);
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
  return;
}

void writedata3 (int i) {
  int yc, yb, ye, xo = font3c[i*2], xm = font3c[i*2+1], xc, bi, cur;
  unsigned char buf[13][17], tmp;

  printf ("     _%d_3[] =\n{", i+33);
  memset (buf, 0, 13*17);
  for (yc = 0; yc < 13; yc++)
    for (xc = 0, bi = 1, cur = -1; xc < xm; xc++) {
      if ((tmp = font3[yc*679+xo+xc]) < 255 && cur) {
        bi++; (buf[yc][bi])++; (buf[yc][0])++; cur = 0;
      }
      else if (tmp == 255 && !cur) {
        bi++; (buf[yc][bi])++; cur = -1;
      }
      else
        (buf[yc][bi])++;
    }
  for (yb = 0; yb < 13 && buf[yb][0] == 0; yb++);
  for (ye = 13; ye > 0 && buf[ye-1][0] == 0; ye--);
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
  return;
}

void writedata4 (int j) {
  int yc, yb, ye, xo = font4c[j*2], xm = font4c[j*2+1], xc, bi, cur, i;
  unsigned char buf[16][17], tmp;

  printf ("     _%d_4[] =\n{", j+33);
  memset (buf, 0, 16*17);
  for (yc = 0; yc < 16; yc++)
    for (xc = 0, bi = 1, cur = -1; xc < xm; xc++) {
      if ((tmp = font4[yc*1292+xo+xc]) < 255 && cur) {
        bi++; (buf[yc][bi])++; (buf[yc][0])++; cur = 0;
      }
      else if (tmp == 255 && !cur) {
        bi++; (buf[yc][bi])++; cur = -1;
      }
      else
        (buf[yc][bi])++;
    }
  for (yb = 0; yb < 16 && buf[yb][0] == 0; yb++);
  for (ye = 16; ye > 0 && buf[ye-1][0] == 0; ye--);
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
  if (j == 94)
    printf ("};\n");
  else
    printf ("},\n");
  return;
}
