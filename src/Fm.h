/* FILE: fm.h
 * AUTHOR: Joshua Humphries
 * PURPOSE:
 *    These are prototypes of routines in fm.c, midifile.c, and playback.c
 */

#define BASS	6
#define SNARE   7
#define CYMBAL  8
#define HIHAT   9

#define PIANISSIMO  15
#define PPIANO      31
#define PIANO       47
#define MEZZOPIANO  63
#define MEZZOFORTE  79
#define FORTE       95
#define FFORTE     111
#define FORTISSIMO 127

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

extern FM_Instrument GeneralMIDI[], Percussion[];
extern int FMNumbers[];

extern void (*FM_Set)(void), (*FM_Reset)(void), (*FM_Key_Off)(int voice),
     (*FM_Key_On)(int voice, int note),
     (*FM_Set_Volume)(int voice, int vol),
     (*FM_Set_Pan)(int voice, signed char pan),
     (*FM_Set_Instrument)(int voice, int bnk_num, int ins_num),
     (*FM_Pitch_Wheel)(int voice, int val);
extern void UseOPL2(void);
extern void UseOPL3(void);
extern void UseMIDI(void);
extern int FM_Detect(void);
extern void FM_Set_DrumSet(int bank);
extern void FM_Set_Percussion(int rhythm);

/* these routines are in midifile.c */
extern void MIDIFILE_Set (void);
extern void MIDIFILE_Reset (void);
extern int  MIDIFILE_Key_Off (int voice, FILE *f, unsigned long wait);
extern int  MIDIFILE_Key_On (int voice, int note, FILE *f, unsigned long wait);
extern int  MIDIFILE_Set_Volume (int voice, int vol, FILE *f, unsigned long wait);
extern int  MIDIFILE_Set_Pan (int voice, signed char pan, FILE *f, unsigned long wait);
extern int  MIDIFILE_Set_Instrument (int voice, int bnk_num, int ins_num, FILE *f, unsigned long wait);
extern int  MIDIFILE_Pitch_Wheel(int voice, int val, FILE *f, unsigned long wait);
extern int  MIDIFILE_Set_DrumSet(int bank, FILE *f, unsigned long wait);
extern void MIDIFILE_Set_Percussion(int rhythm);

/* this routine is in playback.c */
void playsong(int beginpage, int begincol, int endpage, int endcol);
