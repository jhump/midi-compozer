/* FILE: fm.c
 * AUTHOR: Josh Humphries
 * PURPOSE:
 *    This file contains routines for playing back music via an Adlib
 * compatible sound card (Yamaha OPL/2 @ I/O Port 0x388) which are most of
 * them with an OPL/2. If the user specifies that an OPL/3 is available
 * then the routines will use the features for stereo OPL/2 playback
 * (instead of using 4-operator instruments...). Also now supported is
 * playback via General MIDI card (must be at I/O Port 0x330 for these
 * routines to work). Latest addition - support of the pitch wheel (easy
 * for G-MIDI, not too easy for FM but... it was worth it).
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <dos.h>

#define PIANISSIMO  15
#define PPIANO      31
#define PIANO       47
#define MEZZOPIANO  63
#define MEZZOFORTE  79
#define FORTE       95
#define FFORTE     111
#define FORTISSIMO 127

/* FM Instrument definition */
typedef struct {
    unsigned char SoundCharacteristic[2];
    unsigned char Level[2];
    unsigned char AttackDecay[2];
    unsigned char SustainRelease[2];
    unsigned char WaveSelect[2];
    unsigned char Feedback;
    unsigned char PercVoice;
    char rsv1;
    unsigned char PercPitch;
    char rsv2[2];
} FM_Instrument;

FM_Instrument GeneralMIDI[128], Percussion[128];
/* convert MIDI notes to FM octave/frequency */
int FMNumbers[256] =
/* C      C#     D      Eb     E      F      F#     G      Ab     A      Bb     B */
{172,0, 183,0, 194,0, 205,0, 217,0, 230,0, 244,0, 258,0, 274,0, 290,0, 307,0, 326,0, /* octave 0 */
 345,0, 365,0, 387,0, 410,0, 435,0, 460,0, 488,0, 517,0, 547,0, 580,0, 615,0, 651,0, /* octave 1 */
 690,0, 730,0, 774,0, 820,0, 870,0, 920,0, 976,0, 517,1, 547,1, 580,1, 615,1, 651,1, /* octave 2 */
 690,1, 730,1, 774,1, 820,1, 870,1, 920,1, 976,1, 517,2, 547,2, 580,2, 615,2, 651,2, /* octave 3 */
 690,2, 730,2, 774,2, 820,2, 870,2, 920,2, 976,2, 517,3, 547,3, 580,3, 615,3, 651,3, /* octave 4 */
 690,3, 730,3, 774,3, 820,3, 870,3, 920,3, 976,3, 517,4, 547,4, 580,4, 615,4, 651,4, /* octave 5 */
 690,4, 730,4, 774,4, 820,4, 870,4, 920,4, 976,4, 517,5, 547,5, 580,5, 615,5, 651,5, /* octave 6 */
 690,5, 730,5, 774,5, 820,5, 870,5, 920,5, 976,5, 517,6, 547,6, 580,6, 615,6, 651,6, /* octave 7 */
 690,6, 730,6, 774,6, 820,6, 870,6, 920,6, 976,6, 517,7, 547,7, 580,7, 615,7, 651,7, /* octave 8 */
 690,7, 730,7, 774,7, 820,7, 870,7, 920,7, 976,7,1023,7, 547,7, 580,7, 615,7, 651,7, /* octave 9 */
 690,7, 730,7, 774,7, 820,7, 870,7, 920,7, 976,7,1023,7}; /* octave 10 */

/* some stuff to keep track of OPL2/3 since the registers are write-only */
     /* last played notes for masking out key-on without harming release */
signed char notes[11] = {0,0,0,0,0,0,0,0,0,0,0},
            freqs[11] = {0,0,0,0,0,0,0,0,0,0,0},
            /* table for pitch wheel with FM */
            pwheel[11] = {0,0,0,0,0,0,0,0,0,0,0};
char regBD = 0,
     levels[22] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
     pan_[11] = {0,0,0,0,0,0,0,0,0,0,0},
     vol_[11] = { MEZZOFORTE,MEZZOFORTE,MEZZOFORTE,MEZZOFORTE,MEZZOFORTE,
       MEZZOFORTE,MEZZOFORTE,MEZZOFORTE,MEZZOFORTE,MEZZOFORTE,MEZZOFORTE },
     /* array to determine operator 1 number for a channel */
     offsets[9] = {0,1,2,8,9,10,16,17,18},
     offsetsp[11] = {0,1,2,8,9,10,16,20,18,21,17},
     /* percussion voices mapped to which channels ? */
     percv[4] = {6,6,6,6},
     /* sound card mode: 0 = OPL2, 1 = OPL3, 2 = GeneralMIDI */
     mode = -1,
     /* table to linearize FM levels... */
     FMVolume[128] =
{ 0,  8, 11, 13, 14, 16, 17, 18,
 20, 21, 22, 22, 23, 24, 25, 26,
 26, 27, 28, 28, 29, 30, 30, 31,
 31, 32, 32, 33, 33, 34, 34, 35,
 35, 36, 36, 37, 37, 38, 38, 38,
 39, 39, 40, 40, 41, 41, 41, 42,
 42, 42, 43, 43, 43, 44, 44, 45,
 45, 45, 46, 46, 46, 47, 47, 47,
 47, 48, 48, 48, 49, 49, 49, 50,
 50, 50, 51, 51, 51, 51, 52, 52,
 52, 52, 53, 53, 53, 54, 54, 54,
 54, 55, 55, 55, 55, 56, 56, 56,
 56, 57, 57, 57, 57, 58, 58, 58,
 58, 59, 59, 59, 59, 60, 60, 60,
 60, 60, 61, 61, 61, 61, 62, 62,
 62, 62, 62, 63, 63, 63, 63, 64};

/* prototypes for the following variable functions... */
	/* OPL2 functions */
void OPL2FM_Set(void);
void OPL2FM_Reset(void);
void OPL2FM_Key_Off(int voice);
void OPL2FM_Key_On(int voice, int note);
void OPL2FM_Set_Volume(int voice, int vol);
void OPL2FM_Set_Pan(int voice, signed char pan);
void OPL2FM_Set_Instrument(int voice, int bnk_num, int ins_num);
void OPL2FM_Pitch_Wheel(int voice, int val);
	/* OPL3 functions */
void OPL3FM_Set(void);
void OPL3FM_Reset(void);
void OPL3FM_Key_Off(int voice);
void OPL3FM_Key_On(int voice, int note);
void OPL3FM_Set_Volume(int voice, int vol);
void OPL3FM_Set_Pan(int voice, signed char pan);
void OPL3FM_Set_Instrument(int voice, int bnk_num, int ins_num);
void OPL3FM_Pitch_Wheel(int voice, int val);
        /* General MIDI functions */
void MIDIFM_Set(void);
void MIDIFM_Reset(void);
void MIDIFM_Key_Off(int voice);
void MIDIFM_Key_On(int voice, int note);
void MIDIFM_Set_Volume(int voice, int vol);
void MIDIFM_Set_Pan(int voice, signed char pan);
void MIDIFM_Set_Instrument(int voice, int bnk_num, int ins_num);
void MIDIFM_Pitch_Wheel(int voice, int val);
void FM_Set_Percussion (int rhythm);
void FM_Set_DrumSet (int bank);
/* routines that are variable depending on the FM chip available */
void (*FM_Set)(void) = OPL2FM_Set,
     (*FM_Reset)(void) = OPL2FM_Reset,
     (*FM_Key_Off)(int voice) = OPL2FM_Key_Off,
     (*FM_Key_On)(int voice, int note) = OPL2FM_Key_On,
     (*FM_Set_Volume)(int voice, int vol) = OPL2FM_Set_Volume,
     (*FM_Set_Pan)(int voice, signed char pan) = OPL2FM_Set_Pan,
     (*FM_Set_Instrument)(int voice, int bnk_num, int ins_num) = OPL2FM_Set_Instrument,
     (*FM_Pitch_Wheel)(int voice, int val) = OPL2FM_Pitch_Wheel;

void OPL2WriteFM(int reg, unsigned char data) {
  int i;
  outportb (0x388, reg);
  for (i=0; i<6; i++) inportb (0x388);
  outportb (0x389, data);
  for (i=0; i<36; i++) inportb (0x389);
  return;
}

void OPL3WriteFM(int chip, int reg, unsigned char data) {
  int i;
  if (chip) {
    outportb (0x38a, reg);
    for (i=0; i<6; i++) inportb (0x38a);
    outportb (0x38b, data);
    for (i=0; i<36; i++) inportb (0x38b);
  }
  else {
    outportb (0x388, reg);
    for (i=0; i<6; i++) inportb (0x388);
    outportb (0x389, data);
    for (i=0; i<36; i++) inportb (0x389);
  }
  return;
}

void MIDIWriteFM(int cmd, unsigned char data) {
  int i;

  for (i = 0; !(inportb(0x331)&0x80) && i < 30000; i++) inportb (0x330);
  if (cmd)
    outportb (0x331,data);
  else {
    for (i = 0; (inportb(0x331)&0x40) && i < 30000; i++);
    outportb (0x330,data);
  }
  return;
}

/* this will detect presence of OPL/2 chip... if we are using an
   OPL/3, then we'll check this (since they are compatible) and trust
   user when it comes to using OPL/3 features */
int FM_Detect(void) {
  char a, b;
  OPL2WriteFM (4,0x60);
  OPL2WriteFM (4,0x80);
  a = inportb (0x388);
  OPL2WriteFM (2,0xFF);
  OPL2WriteFM (4,0x21);
  delay(1);
  b = inportb (0x388);
  OPL2WriteFM (4,0x60);
  OPL2WriteFM (4,0x80);
  if ((a & 0xE0) == 0 && (b & 0xE0) == 0xC0)
    return 0;
  else
    return -1;
}

void UseOPL2(void) {
  if (mode != -1)
    FM_Reset();
  FM_Set = OPL2FM_Set;
  FM_Reset = OPL2FM_Reset;
  FM_Key_Off = OPL2FM_Key_Off;
  FM_Key_On = OPL2FM_Key_On;
  FM_Set_Volume = OPL2FM_Set_Volume;
  FM_Set_Pan = OPL2FM_Set_Pan;
  FM_Set_Instrument = OPL2FM_Set_Instrument;
  FM_Pitch_Wheel = OPL2FM_Pitch_Wheel;
  mode = 0;
  return;
}

void UseOPL3(void) {
  if (mode != -1)
    FM_Reset();
  FM_Set = OPL3FM_Set;
  FM_Reset = OPL3FM_Reset;
  FM_Key_Off = OPL3FM_Key_Off;
  FM_Key_On = OPL3FM_Key_On;
  FM_Set_Volume = OPL3FM_Set_Volume;
  FM_Set_Pan = OPL3FM_Set_Pan;
  FM_Set_Instrument = OPL3FM_Set_Instrument;
  FM_Pitch_Wheel = OPL3FM_Pitch_Wheel;
  mode = 1;
  return;
}

void UseMIDI(void) {
  if (mode != -1)
    FM_Reset();
  FM_Set = MIDIFM_Set;
  FM_Reset = MIDIFM_Reset;
  FM_Key_Off = MIDIFM_Key_Off;
  FM_Key_On = MIDIFM_Key_On;
  FM_Set_Volume = MIDIFM_Set_Volume;
  FM_Set_Pan = MIDIFM_Set_Pan;
  FM_Set_Instrument = MIDIFM_Set_Instrument;
  FM_Pitch_Wheel = MIDIFM_Pitch_Wheel;
  mode = 2;
  return;
}

void OPL2FM_Set(void) {
  int i;
  OPL2WriteFM(1,0x20);
  for (i = 2; i < 0xF6; i++)
    OPL2WriteFM(i,0);
  regBD = 0;
  for (i = 0; i < 11; i++) {
    levels[i*2] = 0;
    levels[i*2+1] = 0;
    pan_[i] = 0;
    vol_[i] = MEZZOFORTE;
    notes[i] = -1;
    freqs[i] = 0;
    pwheel[i] = 0;
  }
  return;
}

void OPL3FM_Set(void) {
  int i;
  OPL3WriteFM(0,1,0x20);
  for (i = 2; i < 0xF6; i++) {
    OPL3WriteFM(0,i,0);
    OPL3WriteFM(1,i,0);
  }
  OPL3WriteFM(1,5,1);
  regBD = 0;
  for (i = 0; i < 11; i++) {
    levels[i*2] = 0;
    levels[i*2+1] = 0;
    pan_[i] = 0;
    vol_[i] = MEZZOFORTE;
    notes[i] = -1;
    freqs[i] = 0;
    pwheel[i] = 0;
  }
  return;
}

void MIDIFM_Set(void) {
  int i;
  MIDIWriteFM (1, 0xFF);
  MIDIWriteFM (1, 0x3F);
  for (i=0; i<16; i++) {
    MIDIWriteFM (0, 0xb0 + i);
    MIDIWriteFM (0, 123);
    MIDIWriteFM (0, 0);
    MIDIWriteFM (0, 0xb0 + i);
    MIDIWriteFM (0, 121);
    MIDIWriteFM (0, 0);
    MIDIWriteFM (0, 0xb0 + i);
    MIDIWriteFM (0, 7);
    MIDIWriteFM (0, MEZZOFORTE);
    MIDIWriteFM (0, 0xb0 + i);
    MIDIWriteFM (0, 10);
    MIDIWriteFM (0, 64);
  }
  MIDIWriteFM (0, 0xF0);
  MIDIWriteFM (0, 0x7E);
  MIDIWriteFM (0, 0x7F);
  MIDIWriteFM (0, 0x09);
  MIDIWriteFM (0, 0x01);
  MIDIWriteFM (0, 0xF7);
  MIDIWriteFM (0, 0xF0);
  MIDIWriteFM (0, 0x7F);
  MIDIWriteFM (0, 0x7F);
  MIDIWriteFM (0, 0x04);
  MIDIWriteFM (0, 0x01);
  MIDIWriteFM (0, 0x7F);
  MIDIWriteFM (0, 0x7F);
  MIDIWriteFM (0, 0xF7);
  regBD = 0;
  for (i = 0; i < 11; i++) {
    levels[i*2] = 0;
    levels[i*2+1] = 0;
    pan_[i] = 0;
    vol_[i] = MEZZOFORTE;
    notes[i] = -1;
    freqs[i] = 0;
    pwheel[i] = 0;
  }
  return;
}

void OPL2FM_Reset(void) {
  int i;
  OPL2WriteFM(1,0);
  for (i = 2; i < 0xF6; i++)
    OPL2WriteFM(i,0);
  regBD = 0;
  for (i = 0; i < 11; i++) {
    levels[i*2] = 0;
    levels[i*2+1] = 0;
    pan_[i] = 0;
    vol_[i] = MEZZOFORTE;
    notes[i] = -1;
    freqs[i] = 0;
    pwheel[i] = 0;
  }
  return;
}

void OPL3FM_Reset(void) {
  int i;
  OPL3WriteFM(0,1,0);
  for (i = 2; i < 0xF6; i++)
    if (i != 5) {
      OPL3WriteFM(0,i,0);
      OPL3WriteFM(1,i,0);
    }
  OPL3WriteFM(1,5,0);
  regBD = 0;
  for (i = 0; i < 11; i++) {
    levels[i*2] = 0;
    levels[i*2+1] = 0;
    pan_[i] = 0;
    vol_[i] = MEZZOFORTE;
    notes[i] = -1;
    freqs[i] = 0;
    pwheel[i] = 0;
  }
  return;
}

void MIDIFM_Reset(void) {
  int i;
  for (i=0; i<16; i++) {
    MIDIWriteFM (0, 0xb0 + i);
    MIDIWriteFM (0, 123);
    MIDIWriteFM (0, 0);
    MIDIWriteFM (0, 0xb0 + i);
    MIDIWriteFM (0, 121);
    MIDIWriteFM (0, 0);
  }
  MIDIWriteFM (1, 0xFF);
  regBD = 0;
  for (i = 0; i < 11; i++) {
    levels[i*2] = 0;
    levels[i*2+1] = 0;
    pan_[i] = 0;
    vol_[i] = MEZZOFORTE;
    notes[i] = -1;
    freqs[i] = 0;
    pwheel[i] = 0;
  }
  return;
}

void OPL2FM_Key_Off(int voice) {
  unsigned char reg_num;

  voice %= 10;
  /* turn voice off */
	/* if percussion */
  if ((regBD & 0x20) && voice > 5) {
    switch (percv[voice-6]) {
      case 6:  regBD &= 0xEF; break;
      case 7:  regBD &= 0xF7; break;
      case 8:  regBD &= 0xFB; break;
      case 9:  regBD &= 0xFD; break;
      case 10: regBD &= 0xFE; break;
    }
    OPL2WriteFM(0xBD,regBD);
  }
  else if (voice == 9) return;
  else {
    reg_num = 0xB0 + (voice);
    OPL2WriteFM(reg_num,freqs[voice] & 0x1F);
  }
  notes[voice] = -1;
  return;
}

void OPL3FM_Key_Off(int voice) {
  unsigned char reg_num;

  voice %= 10;
  /* turn voice off */
	/* if percussion */
  if ((regBD & 0x20) && voice > 5) {
    switch (percv[voice-6]) {
      case 6:  regBD &= 0xEF; break;
      case 7:  regBD &= 0xF7; break;
      case 8:  regBD &= 0xFB; break;
      case 9:  regBD &= 0xFD; break;
      case 10: regBD &= 0xFE; break;
    }
    OPL2WriteFM(0xBD,regBD);
  }
  else if (voice == 9) return;
  else {
    reg_num = 0xB0 + (voice);
    OPL3WriteFM(0,reg_num,freqs[voice] & 0x1F);
    OPL3WriteFM(1,reg_num,freqs[voice] & 0x1F);
  }
  notes[voice] = -1;
  return;
}

void MIDIFM_Key_Off(int voice) {
  voice &= 0xF;
  if ((regBD & 0x20) && voice > 5) {
    MIDIWriteFM (0, 0x99);
    MIDIWriteFM (0, notes[voice]);
    MIDIWriteFM (0, 0);
  }
  else {
    if (voice == 9) voice++;
    MIDIWriteFM (0, 0x90 + voice);
    MIDIWriteFM (0, notes[voice]);
    MIDIWriteFM (0, 0);
  }
  notes[voice] = -1;
  return;
}

void OPLNote2Freq (int note, int voice, int *freq, int *octave) {
  int f1, f2, d = 0;

  /* use frequency/octave pairs in FMNumbers array and alter values
     based on pitch wheel value */
  if (pwheel[voice] == -128)
    note -= 2;
  else if (pwheel[voice] <= -64) {
    note--;
    d = pwheel[voice] + 64;
  }
  else if (pwheel[voice] < 64)
    d = pwheel[voice];
  else if (pwheel[voice] < 127) {
    note++;
    d = pwheel[voice] - 64;
  }
  else if (pwheel[voice] == 127)
    note += 2;
  if (note > 127) note = 127;
  else if (note < 0) note = 0;
  if (note == 127 && d > 0) d = 0;
  else if (note == 0 && d < 0) d = 0;
  f1 = FMNumbers[note<<1];
  *octave = FMNumbers[(note<<1)+1];
  /* bend up */
  if (d > 0) {
    f2 = FMNumbers[(note+1)<<1];
    if (f2 < f1) f2 <<= 1;
    f1 += (d*(f2-f1))>>6;
    if (f1 > 1023) f1 = 1023;
  }
  /* bend down */
  else if (d < 0) {
    f2 = FMNumbers[(note-1)<<1];
    if (f2 > f1) f2 >>= 1;
    f1 -= (d*(f2-f1))>>6;
  }
  *freq = f1;
  return;
}

void OPL2FM_Key_On(int voice, int note) {
  register unsigned char reg_num;
  int freq, octave;
  unsigned char tmp;
  if (notes[voice] >= 0)
    OPL2FM_Key_Off(voice);
  voice %= 10;
  if ((regBD & 0x20) && voice > 5) {
    FM_Set_Instrument (voice, 0, note);
    voice = (percv[voice-6] = Percussion[note].PercVoice);
    note = Percussion[note].PercPitch;
    OPLNote2Freq (note, voice, &freq, &octave);
    if (voice < 9) {
      reg_num = 0xa0 + (voice);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb0 + (voice);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
    else {
      reg_num = 0xa8 - (voice % 9);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb8 - (voice % 9);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
    switch (voice) {
      case 6:  regBD |= 0x10; break;
      case 7:  regBD |= 0x08; break;
      case 8:  regBD |= 0x04; break;
      case 9:  regBD |= 0x02; break;
      case 10: regBD |= 0x01; break;
    }
    OPL2WriteFM(0xBD,regBD);
  }
  else if (voice == 9) return;
  else {
    OPLNote2Freq (note, voice, &freq, &octave);
    reg_num = 0xa0 + (voice);
    OPL2WriteFM(reg_num,freq & 0xff);
    reg_num = 0xb0 + (voice);
    tmp = (freq >> 8) | (octave << 2) | 0x20;
    OPL2WriteFM(reg_num,tmp);
  }
  notes[voice] = note;
  freqs[voice] = tmp;
  return;
}

void OPL3FM_Key_On(int voice, int note) {
  register unsigned char reg_num;
  int freq, octave;
  unsigned char tmp;
  if (notes[voice] >= 0)
    OPL3FM_Key_Off(voice);
  voice %= 10;
  if ((regBD & 0x20) && voice > 5) {
    FM_Set_Instrument (voice, 0, note);
    voice = (percv[voice-6] = Percussion[note].PercVoice);
    note = Percussion[note].PercPitch;
    OPLNote2Freq (note, voice, &freq, &octave);
    if (voice < 9) {
      reg_num = 0xa0 + (voice);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb0 + (voice);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
    else {
      reg_num = 0xa8 - (voice % 9);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb8 - (voice % 9);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
    switch (voice) {
      case 6:  regBD |= 0x10; break;
      case 7:  regBD |= 0x08; break;
      case 8:  regBD |= 0x04; break;
      case 9:  regBD |= 0x02; break;
      case 10: regBD |= 0x01; break;
    }
    OPL2WriteFM(0xBD,regBD);
  }
  else if (voice == 9) return;
  else {
    OPLNote2Freq (note, voice, &freq, &octave);
    reg_num = 0xa0 + (voice);
    OPL3WriteFM(0,reg_num,freq & 0xff);
    OPL3WriteFM(1,reg_num,freq & 0xff);
    reg_num = 0xb0 + (voice);
    tmp = (freq >> 8) | (octave << 2) | 0x20;
    OPL3WriteFM(0,reg_num,tmp);
    OPL3WriteFM(1,reg_num,tmp);
  }
  notes[voice] = note;
  freqs[voice] = tmp;
  return;
}

void MIDIFM_Key_On(int voice, int note) {
  voice &= 0xF;
  if (notes[voice] >= 0)
    MIDIFM_Key_Off (voice);
  if ((regBD & 0x20) && voice > 5) {
    if (note < 35) note = 35;
    if (note > 81) note = 81;
    MIDIWriteFM (0, 0x99);
    MIDIWriteFM (0, note);
    MIDIWriteFM (0, levels[voice]);
  }
  else {
    if (voice == 9) voice++;
    MIDIWriteFM (0, 0x90 + voice);
    MIDIWriteFM (0, note);
    MIDIWriteFM (0, 127);
  }
  notes[voice] = note;
  return;
}

void OPL2FM_Pitch_Wheel (int voice, int val) {
  register unsigned char reg_num;
  int freq, octave, note;
  unsigned char tmp;
  if (val > 127) val = 127;
  else if (val < -128) val = -128;
  voice %= 10;
  if ((regBD & 0x20) && voice > 5) {
    voice = percv[voice-6];
    pwheel[voice] = val;
    note = notes[voice];
    OPLNote2Freq (note, voice, &freq, &octave);
    if (voice < 9) {
      reg_num = 0xa0 + (voice);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb0 + (voice);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
    else {
      reg_num = 0xa8 - (voice % 9);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb8 - (voice % 9);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
  }
  else if (voice == 9) return;
  else {
    pwheel[voice] = val;
    note = notes[voice];
    OPLNote2Freq (note, voice, &freq, &octave);
    reg_num = 0xa0 + (voice);
    OPL2WriteFM(reg_num,freq & 0xff);
    reg_num = 0xb0 + (voice);
    tmp = (freq >> 8) | (octave << 2) | 0x20;
    OPL2WriteFM(reg_num,tmp);
  }
  freqs[voice] = tmp;
  return;
}

void OPL3FM_Pitch_Wheel (int voice, int val) {
  register unsigned char reg_num;
  int freq, octave, note;
  unsigned char tmp;
  if (val > 127) val = 127;
  else if (val < -128) val = -128;
  voice %= 10;
  if ((regBD & 0x20) && voice > 5) {
    voice = percv[voice-6];
    pwheel[voice] = val;
    note = notes[voice];
    OPLNote2Freq (note, voice, &freq, &octave);
    if (voice < 9) {
      reg_num = 0xa0 + (voice);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb0 + (voice);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
    else {
      reg_num = 0xa8 - (voice % 9);
      OPL2WriteFM(reg_num,freq & 0xff);
      reg_num = 0xb8 - (voice % 9);
      tmp = ((freq >> 8) | (octave << 2)) & 0x1F;
      OPL2WriteFM(reg_num,tmp);
    }
  }
  else if (voice == 9) return;
  else {
    pwheel[voice] = val;
    note = notes[voice];
    OPLNote2Freq (note, voice, &freq, &octave);
    reg_num = 0xa0 + (voice);
    OPL3WriteFM(0,reg_num,freq & 0xff);
    OPL3WriteFM(1,reg_num,freq & 0xff);
    reg_num = 0xb0 + (voice);
    tmp = (freq >> 8) | (octave << 2) | 0x20;
    OPL3WriteFM(0,reg_num,tmp);
    OPL3WriteFM(1,reg_num,tmp);
  }
  freqs[voice] = tmp;
  return;
}

void MIDIFM_Pitch_Wheel (int voice, int val) {
  voice &= 0xF;
  if (val > 127) val = 127;
  else if (val < -128) val = -128;
  val += 128;
  val <<= 6;
  if ((regBD & 0x20) && voice > 5 && voice < 10)
    return;
  else {
    if (voice == 9) voice++;
    MIDIWriteFM (0, 0xE0 + voice);
    MIDIWriteFM (0, val & 0x7F);
    MIDIWriteFM (0, val >> 7);
  }
  pwheel[voice] = val;
  return;
}

void OPL2ChangeVol(int opr, int vol) {
  int tmp = levels[opr];
  if (vol > 127) vol = 127;
  else if (vol < 0) vol = 0;
  vol = ((63 - (tmp & 0x3F)) * FMVolume[vol]) >> 6;
  if (vol > 63) vol = 63;
  OPL2WriteFM (opr + 0x40, (63 - vol) | (tmp & 0xC0));
  return;
}

void OPL3ChangeVol(int opr, int vol, int pan) {
  int tmp = levels[opr], voll, volr;
  if (vol > 127) vol = 127;
  else if (vol < 0) vol = 0;
  voll = volr = vol;
  /* even the scale so its -127 -> 127 instead of -128 -> 127 */
  if (pan == -128) pan++;
  /* pan to right */
  if (pan > 0) {
    voll *= (127-pan);
    voll /= 127;
  }
  /* pan to left */
  else if (pan < 0) {
    volr *= (pan+127);
    volr /= 127;
  }
  voll = ((63 - (tmp & 0x3F)) * FMVolume[voll]) >> 6;
  volr = ((63 - (tmp & 0x3F)) * FMVolume[volr]) >> 6;
  if (voll > 63) voll = 63;
  if (volr > 63) volr = 63;
  OPL3WriteFM (0, opr + 0x40, (63 - voll) | (tmp & 0xC0));
  OPL3WriteFM (1, opr + 0x40, (63 - volr) | (tmp & 0xC0));
  return;
}

void OPL2FM_Set_Volume(int voice, int vol) {
  unsigned char op_num;
  voice %= 10;
  if (vol > 127) vol = 127;
  else if (vol < 0) vol = 0;
  if ((regBD & 0x20) && voice > 5)
    op_num = offsetsp[percv[voice-6]];
  else if (voice == 9) return;
  else
    op_num = offsets[voice];
  OPL2ChangeVol(op_num,vol);
  if (!(regBD & 0x20) || voice < 7) {
    op_num += 3;
    OPL2ChangeVol(op_num,vol);
  }
  vol_[voice] = vol;
  return;
}

void OPL3FM_Set_Volume(int voice, int vol) {
  unsigned char op_num;
  voice %= 10;
  if (vol > 127) vol = 127;
  else if (vol < 0) vol = 0;
  if ((regBD & 0x20) && voice > 5)
    op_num = offsetsp[percv[voice-6]];
  else if (voice == 9) return;
  else
    op_num = offsets[voice];
  if ((regBD & 0x20) && voice > 5) {
    OPL2ChangeVol(op_num,vol);
    if (voice < 7)
      OPL2ChangeVol(op_num+3,vol);
  }
  else {
    OPL3ChangeVol(op_num,vol,pan_[voice]);
    op_num += 3;
    OPL3ChangeVol(op_num,vol,pan_[voice]);
  }
  vol_[voice] = vol;
  return;
}

void MIDIFM_Set_Volume(int voice, int vol) {
  voice &= 0xF;
  if (vol > 127) vol = 127;
  else if (vol < 0) vol = 0;
  if ((regBD & 0x20) && voice > 5 && voice < 10)
    levels[voice] = vol;
  else {
    if (voice == 9) voice++;
    MIDIWriteFM (0, 0xB0 + voice);
    MIDIWriteFM (0, 7);
    MIDIWriteFM (0, vol);
  }
  vol_[voice] = vol;
  return;
}

void OPL2FM_Set_Pan(int voice, signed char pan) {
  return;
}

void OPL3FM_Set_Pan(int voice, signed char pan) {
  unsigned char op_num;
  voice %= 10;
  if ((regBD & 0x20) && voice > 5) return;
  else if (voice == 9) return;
  else
    op_num = offsets[voice];
  OPL3ChangeVol(op_num,vol_[voice] & 0x7F,pan);
  op_num += 3;
  OPL3ChangeVol(op_num,vol_[voice] & 0x7F,pan);
  pan_[voice] = pan;
  return;
}

void MIDIFM_Set_Pan(int voice, signed char pan) {
  voice &= 0xF;
  pan >>= 1; pan += 64;
  if (pan < 0) pan = 0;
  if ((regBD & 0x20) && voice > 5) {
    MIDIWriteFM (0, 0xB9);
    MIDIWriteFM (0, 10);
    MIDIWriteFM (0, pan);
  }
  else {
    if (voice == 9) voice++;
    MIDIWriteFM (0, 0xB0 + voice);
    MIDIWriteFM (0, 10);
    MIDIWriteFM (0, pan);
  }
  pan_[voice] = pan;
  return;
}

void OPL2FM_Set_Instrument(int voice, int bnk_num, int ins_num) {
  register unsigned char op_cell_num;
  FM_Instrument *ins;
  int cell_offset, mod = 0;

  voice %= 10;
  if ((regBD & 0x20) && voice > 5) {
    ins = &(Percussion[ins_num]);
    cell_offset = offsetsp[ins->PercVoice];
    if (ins->PercVoice == 7 || ins->PercVoice == 9)
      mod = 1;
  }
  else {
    ins = &(GeneralMIDI[ins_num]);
    if (voice == 9) return;
    cell_offset = offsets[voice];
  }

  /* set sound characteristic */
  op_cell_num = 0x20 + (char)cell_offset;

  OPL2WriteFM(op_cell_num,ins->SoundCharacteristic[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL2WriteFM(op_cell_num,ins->SoundCharacteristic[1]);
  }

  /* set sound levels */
  levels[cell_offset] = ins->Level[mod];
  OPL2ChangeVol(cell_offset,vol_[voice] & 0x7F);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    levels[cell_offset+3] = ins->Level[1];
    OPL2ChangeVol(cell_offset+3,vol_[voice] & 0x7F);
  }

  /* set Attack/Decay */
  op_cell_num = 0x60 + (char)cell_offset;
  OPL2WriteFM(op_cell_num,ins->AttackDecay[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL2WriteFM(op_cell_num,ins->AttackDecay[1]);
  }

  /* set Sustain/Release */
  op_cell_num = 0x80 + (char)cell_offset;
  OPL2WriteFM(op_cell_num,ins->SustainRelease[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL2WriteFM(op_cell_num, ins->SustainRelease[1]);
  }

  /* set Wave Select */
  op_cell_num = 0xE0 + (char)cell_offset;
  OPL2WriteFM(op_cell_num,ins->WaveSelect[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL2WriteFM(op_cell_num,ins->WaveSelect[1]);
  }

  if ((regBD & 0x20) && voice > 5)
    voice = ins->PercVoice;
  /* set Feedback/Selectivity */
  if (voice < 9) {
    op_cell_num = (unsigned char)0xC0 + (unsigned char)(voice);
    OPL2WriteFM(op_cell_num,(ins->Feedback) & 0x0f);
  }
  return;
}

void OPL3FM_Set_Instrument(int voice, int bnk_num, int ins_num) {
  register unsigned char op_cell_num;
  FM_Instrument *ins;
  int cell_offset, mod = 0;

  voice %= 10;
  if ((regBD & 0x20) && voice > 5) {
    ins = &(Percussion[ins_num]);
    cell_offset = offsetsp[ins->PercVoice];
    if (ins->PercVoice == 7 || ins->PercVoice == 9)
      mod = 1;
  }
  else {
    ins = &(GeneralMIDI[ins_num]);
    if (voice == 9) return;
    cell_offset = offsets[voice];
  }

  /* set sound characteristic */
  op_cell_num = 0x20 + (char)cell_offset;

  OPL3WriteFM(0,op_cell_num,ins->SoundCharacteristic[mod]);
  OPL3WriteFM(1,op_cell_num,ins->SoundCharacteristic[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL3WriteFM(0,op_cell_num,ins->SoundCharacteristic[1]);
    OPL3WriteFM(1,op_cell_num,ins->SoundCharacteristic[1]);
  }

  /* set sound levels */
  levels[cell_offset] = ins->Level[mod];
  if ((regBD & 0x20) && voice > 5) {
    OPL2ChangeVol(cell_offset,vol_[voice] & 0x7F);
    if (ins->PercVoice < 7) {
      levels[cell_offset+3] = ins->Level[1];
      OPL2ChangeVol(cell_offset+3,vol_[voice] & 0x7F);
    }
  }
  else {
    OPL3ChangeVol (cell_offset, vol_[voice], pan_[voice]);
    levels[cell_offset+3] = ins->Level[1];
    OPL3ChangeVol (cell_offset+3, vol_[voice], pan_[voice]);
  }

  /* set Attack/Decay */
  op_cell_num = 0x60 + (char)cell_offset;
  OPL3WriteFM(0,op_cell_num,ins->AttackDecay[mod]);
  OPL3WriteFM(1,op_cell_num,ins->AttackDecay[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL3WriteFM(0,op_cell_num,ins->AttackDecay[1]);
    OPL3WriteFM(1,op_cell_num,ins->AttackDecay[1]);
  }

  /* set Sustain/Release */
  op_cell_num = 0x80 + (char)cell_offset;
  OPL3WriteFM(0,op_cell_num,ins->SustainRelease[mod]);
  OPL3WriteFM(1,op_cell_num,ins->SustainRelease[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL3WriteFM(0,op_cell_num, ins->SustainRelease[1]);
    OPL3WriteFM(1,op_cell_num, ins->SustainRelease[1]);
  }

  /* set Wave Select */
  op_cell_num = 0xE0 + (char)cell_offset;
  OPL3WriteFM(0,op_cell_num,ins->WaveSelect[mod]);
  OPL3WriteFM(1,op_cell_num,ins->WaveSelect[mod]);
  if (!(regBD & 0x20) || voice < 7 || ins->PercVoice < 7) {
    op_cell_num += 3;
    OPL3WriteFM(0,op_cell_num,ins->WaveSelect[1]);
    OPL3WriteFM(1,op_cell_num,ins->WaveSelect[1]);
  }

  if ((regBD & 0x20) && voice > 5)
    voice = ins->PercVoice;
  /* set Feedback/Selectivity */
  if ((regBD & 0x20) && voice > 5) {
    op_cell_num = (unsigned char)0xC0 + (unsigned char)(voice);
    OPL3WriteFM(0,op_cell_num,((ins->Feedback) & 0x0F) | 0x30);
  }
  else {
    op_cell_num = (unsigned char)0xC0 + (unsigned char)(voice);
    OPL3WriteFM(0,op_cell_num,((ins->Feedback) & 0x0F) | 0x10);
    OPL3WriteFM(1,op_cell_num,((ins->Feedback) & 0x0F) | 0x20);
  }
  return;
}

void MIDIFM_Set_Instrument(int voice, int bnk_num, int ins_num) {
  voice &= 0xF;
  if ((regBD & 0x20) && voice > 5)
    return;
  else {
    if (voice == 9) voice++;
    MIDIWriteFM (0, 0xB0 + voice);
    MIDIWriteFM (0, 32);
    MIDIWriteFM (0, 0);
    MIDIWriteFM (0, 0xB0 + voice);
    MIDIWriteFM (0, 0);
    MIDIWriteFM (0, bnk_num);
    MIDIWriteFM (0, 0xC0 + voice);
    MIDIWriteFM (0, ins_num);
  }
  return;
}

/* this is done the same way regardless of OPL chip... */
void FM_Set_Percussion (int rhythm) {
  /* set flag for all cards */
  regBD = ((rhythm)?(0x20):(0));
  /* if not General MIDI, set OPL chip */
  if (mode != 2)
    OPL2WriteFM(0xBD, regBD);
  /* turn percussion channel up and use note velocity to control volumes */
  else {
    MIDIWriteFM (0, 0xB9);
    MIDIWriteFM (0, 7);
    MIDIWriteFM (0, 127);
    levels[6] = levels[7] = levels[8] = levels[9] = MEZZOFORTE;
  }
  return;
}

void FM_Set_DrumSet (int bank) {
  if (mode == 2 && (regBD & 0x20)) {
    MIDIWriteFM (0, 0xB9);
    MIDIWriteFM (0, 32);
    MIDIWriteFM (0, 0);
    MIDIWriteFM (0, 0xB9);
    MIDIWriteFM (0, 0);
    MIDIWriteFM (0, bank&0x7F);
  }
}
