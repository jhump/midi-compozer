char *Piano[8] =
{"Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
 "Honky-Tonk Piano", "Electric Piano 1", "Electric Piano 2", "Harpsichord",
 "Clavinet"},
     *ChromPerc[8] =
{"Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba",
 "Xylophone", "Tubular Bells", "Dulcimer"},
     *Organ[8] =
{"Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
 "Reed Organ", "Accordian", "Harmonica", "Tango Accordian"},
     *Guitar[8] =
{"Acoustic Guitar (nylon-strings)", "Acoustic Guitar (steel-strings)",
 "Electric Guitar (jazz)", "Electric Guitar (clean)",
 "Electric Guitar (muted)", "Overdriven Guitar", "Distorted Guitar",
 "Guitar Harmonics"},
     *Bass[8] =
{"Acoustic Bass", "Electric Bass (fingers)", "Electric Bass (picked)",
 "Fretless Bass", "Slap Bass 1", "Slap Bass 2", "Synth Bass 1",
 "Synth Bass 2"},
     *Strings[8] =
{"Violin", "Viola", "Cello", "Contrabasso", "Tremolo Strings",
 "Pizzicato Strings", "Orchestral Harp", "Timpani"},
     *Ensemble[8] =
{"String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
 "Choir \"Ahh\"s", "Voice \"Ooh\"s", "Synth Voice", "Orchestra Hit"},
     *Brass[8] =
{"Trumpet", "Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass Section",
 "Synth Brass 1", "Synth Brass 2"},
     *Reed[8] =
{"Soprano Saxophone", "Alto Saxophone", "Tenor Saxophone", "Baritone Saxophone",
 "Oboe", "English Horn", "Bassoon", "Clarinet"},
     *Pipe[8] =
{"Piccolo", "Flute", "Recorder", "Pan Flute", "Bottle Blow", "Skakuhachi",
 "Whistle", "Ocarina"},
     *Lead[8] =
{"Lead 1 (square wave)", "Lead 2 (sawtooth)", "Lead 3 (calliope)",
 "Lead 4 (chiff)", "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)",
 "Lead 8 (bass+lead)"},
     *Pad[8] =
{"Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
 "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)"},
     *Effects[8] =
{"FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
 "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)"},
     *Ethnic[8] =
{"Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Bagpipe", "Fiddle",
 "Shanai"},
     *Perc[8] =
{"Tinkle Bell", "Agogo", "Steel Drums", "Wood Block", "Taiko Drum",
 "Melodic Tom", "Synth Drum", "Reverse Cymbal"},
     *Soundfx[8] =
{"Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
 "Telephone Ring", "Helicopter", "Applause", "Gun Shot"};
struct {
  char *Name;
  char **Instrum;
} Banks[16] =
{{"PIANO", Piano}, {"CHROMATIC PERCUSSION", ChromPerc},
 {"ORGAN", Organ}, {"GUITAR", Guitar}, {"BASS", Bass}, {"STRINGS", Strings},
 {"ENSEMBLE", Ensemble}, {"BRASS", Brass}, {"REED", Reed}, {"PIPE", Pipe},
 {"SYNTH LEAD", Lead}, {"SYNTH PAD", Pad}, {"SYNTH EFFECTS", Effects},
 {"ETHNIC", Ethnic}, {"PERCUSSIVE", Perc}, {"SOUND EFFECTS", Soundfx}};

