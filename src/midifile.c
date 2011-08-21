/* FILE: midifile.c
 * AUTHOR: Josh Humphries
 * PURPOSE:
 *      This file contains an interface similar to that of fm.c except it
 * is for writing MIDI files instead of playing them...
 */

#include <stdio.h>
#include "fm.h"

extern signed char notes[], freqs[], pwheel[];
extern char regBD, levels[], pan_[], vol_[];
extern void writevarlen (unsigned long v, FILE *f);
unsigned char midibytes[32];

void MIDIFILE_Set (void) {
  int i;
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

void MIDIFILE_Reset (void) {
  int i;
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

int MIDIFILE_Key_Off (int voice, FILE *f, unsigned long wait) {
  voice &= 0xF;
  if ((regBD & 0x20) && voice > 5) {
    midibytes[0] = 0x99;
    midibytes[1] = notes[voice];
    midibytes[2] = 0;
  }
  else {
    if (voice == 9) voice++;
    midibytes[0] = 0x90 + voice;
    midibytes[1] = notes[voice];
    midibytes[2] = 0;
  }
  notes[voice] = -1;
  writevarlen (wait, f);
  fwrite (midibytes, 3, 1, f);
  return 0;
}

int MIDIFILE_Key_On (int voice, int note, FILE *f, unsigned long wait) {
  voice &= 0xF;
  if (notes[voice] >= 0) {
    wait = MIDIFILE_Key_Off (voice, f, wait);
  }
  if ((regBD & 0x20) && voice > 5) {
    if (note < 35) note = 35;
    if (note > 81) note = 81;
    midibytes[0] = 0x99;
    midibytes[1] = note;
    midibytes[2] = levels[voice];
  }
  else {
    if (voice == 9) voice++;
    midibytes[0] = 0x90 + voice;
    midibytes[1] = note;
    midibytes[2] = 127;
  }
  notes[voice] = note;
  writevarlen (wait, f);
  fwrite (midibytes, 3, 1, f);
  return 0;
}

int MIDIFILE_Set_Volume (int voice, int vol, FILE *f, unsigned long wait) {
  voice &= 0xF;
  if (vol > 127) vol = 127;
  else if (vol < 0) vol = 0;
  if ((regBD & 0x20) && voice > 5 && voice < 10) {
    levels[voice] = vol;
    return wait;
  }
  else {
    if (voice == 9) voice++;
    midibytes[0] = 0xB0 + voice;
    midibytes[1] = 7;
    midibytes[2] = vol;
    writevarlen (wait, f);
    fwrite (midibytes, 3, 1, f);
  }
  vol_[voice] = vol;
  return 0;
}

int MIDIFILE_Set_Pan (int voice, signed char pan, FILE *f, unsigned long wait) {
  voice &= 0xF;
  pan >>= 1; pan += 64;
  if (pan < 0) pan = 0;
  if ((regBD & 0x20) && voice > 5) {
    midibytes[0] = 0xB9;
    midibytes[1] = 10;
    midibytes[2] = pan;
  }
  else {
    if (voice == 9) voice++;
    midibytes[0] = 0xB0 + voice;
    midibytes[1] = 10;
    midibytes[2] = pan;
  }
  writevarlen (wait, f);
  fwrite (midibytes, 3, 1, f);
  pan_[voice] = pan;
  return 0;
}

int MIDIFILE_Set_Instrument (int voice, int bnk_num, int ins_num, FILE *f, unsigned long wait) {
  voice &= 0xF;
  if ((regBD & 0x20) && voice > 5)
    return wait;
  else {
    if (voice == 9) voice++;
    midibytes[0] = 0xB0 + voice;
    midibytes[1] = 0;
    midibytes[2] = 0;
    writevarlen (wait, f);
    fwrite (midibytes, 3, 1, f);

    midibytes[0] = 0xB0 + voice;
    midibytes[1] = 32;
    midibytes[2] = bnk_num;
    writevarlen (0, f);
    fwrite (midibytes, 3, 1, f);

    midibytes[0] = 0xC0 + voice;
    midibytes[1] = ins_num;
    writevarlen (0, f);
    fwrite (midibytes, 2, 1, f);
  }
  return 0;
}

int MIDIFILE_Pitch_Wheel(int voice, int val, FILE *f, unsigned long wait) {
  voice &= 0xF;
  if (val > 127) val = 127;
  else if (val < -128) val = -128;
  val += 128;
  val <<= 6;
  if ((regBD & 0x20) && voice > 5 && voice < 10)
    return wait;
  else {
    if (voice == 9) voice++;
    midibytes[0] = 0xE0 + voice;
    midibytes[1] = val & 0x7F;
    midibytes[2] = val >> 7;
    writevarlen (wait, f);
    fwrite (midibytes, 3, 1, f);
  }
  pwheel[voice] = val;
  return 0;
}

int MIDIFILE_Set_DrumSet (int bank, FILE *f, unsigned long wait) {
  if (regBD & 0x20) {
    midibytes[0] = 0xB9;
    midibytes[1] = 0;
    midibytes[2] = bank&0x7F;
    writevarlen (wait, f);
    fwrite (midibytes, 3, 1, f);
  }
  return 0;
}

void MIDIFILE_Set_Percussion(int rhythm) {
  regBD = ((rhythm)?(0x20):(0));
  levels[6] = levels[7] = levels[8] = levels[9] = MEZZOFORTE;
  return;
}
