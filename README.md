# midi-compozer

This is an old music composition program for DOS.

It was written in the early->mid 90's (before the prevalance of modern 32-bit operating systems). It was a fun exercise to learn how to interact with hardware on x86 platform, including installing ISRs (for keyboard handling and timer callbacks) and working with EMS (an old-school API for accessing more than 640kb of memory from 16-bit code).

I actually still use this software today, thanks to [DOSBox](https://www.dosbox.com/).

BTW, the code is heinous, I know. It is structured poorly, not very modular (including too much copy+pasta and several ridiculously large functions), and doesn't sufficiently use data structures and modeling techniques that would have made the code much easier to follow. I wrote it while I was still in school -- that's the only excuse I can offer...

----

## Archeology

When I started writing this program, I had a [SoundBlaster 16](https://en.wikipedia.org/wiki/Sound_Blaster_16) in my computer. I used a shareware DOS program to write music. But the software was missing a lot of features and was buggy. So I decided to write my own. The user interface of MIDI Compozer, even down to many of the keyboard controls, is very similar to that shareware program (unfortunately, I cannot recall what it was called or I'd give proper credit).

I had learned Pascal and C and also learned enough assembly to be dangerous. I had a book on writing sophisticated DOS programs that documented DOS "system calls" (`int 12h`), how to interact with video hardware through the BIOS (including how to interact with [VESA-compatible](https://en.wikipedia.org/wiki/VESA_BIOS_Extensions) graphics cards), how to interact with things like [EMS](https://en.wikipedia.org/wiki/Expanded_memory#EMS)/[XMS](https://en.wikipedia.org/wiki/Extended_memory#eXtended_Memory_Specification_(XMS)) memory managers, and more.

I had used some of this info to write image and animation players (low-res animations, FLI/FLC format). I had also used this to write simple video games and to write a fractal renderer/viewer.

So I dug up information on how to program an [OPL2/OPL3](https://en.wikipedia.org/wiki/Yamaha_YMF262) synthesizer card (very popular hardware for computer music cards, including Adlib and SoundBlaster cards), and got to work.

### Technology

The first things I wrote were the simple OPL2/OPL3 interaction code and the keyboard [ISR](https://en.wikipedia.org/wiki/Interrupt_handler) handling code. These were fun to learn and implement. The next thing I toyed with was EMS. All my previous programs just limited themselves to 640kb of memory. (I would soon go on to use [`djgcc`](https://en.wikipedia.org/wiki/DJGCC), which provides a 32-bit [DPMI](https://en.wikipedia.org/wiki/DOS_Protected_Mode_Interface), for easily accessing all of a computer's memory. But I started writing MIDI Compozer when computers didn't have much memory. If I recall correctly, my computer at the time was a 40Mhz Cyrix 486 with a whopping 4mb of RAM.)

I had done plenty of graphics programming before, so the simple code to draw the UI was straight-forward. The bezier-curve code was something I had written for a school programming project, so I was able to re-use that.

I had not done modern windows-style GUIs before. I had done several command-line and keyboard user interfaces, and I did have a school programming project where I had to build something similar to what MIDI Compozer offers: a keyboard-driven UI with a main menu at the top. I had done some simple mouse-driven UIs in the form of simple games (in particular, I'd written my own versions of Breakout and Solitaire that had mouse interfaces, and a more complicated Galaga-like game that worked with mouse, keyboard, or joystick for controls). But I decided not to tackle implementing mouse support in this UI. The shareware program that served as inspiration was also keyboard only, and I had really gotten comfortable with it. So I didn't miss having mouse support.

So the program's main loop just waits until it sees key presses get stored into a ring buffer of key codes. The ring buffer is populated from the keyboard ISR. I used a custom keyboard ISR because some of the things I wanted to detect were not provided by standard keyboard-handling (particularly, `alt` as a stand-alone keypress, vs. just as a modifier for another keypress).

The next big thing to write was the code that played music. This was an interesting experience of learning how to implement and install a timing ISR, so that things played at the right pace. In past programs, I had just written code that calculated how long to sleep between actions. But it seemed easier this time to normalize time to "ticks" that were longer or shorter based on the music's tempo. This would greatly simplify how playback decides when the next action should occur. As it turns out, it also made it much easier to re-use much of the playback logic for generating a MIDI file. So the timer ISR just increments a ticker while the program [busy waits](https://en.wikipedia.org/wiki/Busy_waiting) until it sees the ticker reach a certain point, also handling key presses it observes while waiting. When the song's tempo changes, the timer ISR is adjusted to tick more or less frequently.

The final things I learned and implemented were related to the [MIDI](https://en.wikipedia.org/wiki/MIDI) specification. I wrote the code that exports a song to a MIDI file (re-using much of the playback code, but sadly not actually sharing it: exporting to MIDI is effectively a fork of the playback code). And finally, I added support for General MIDI music hardware (such as Roland music cards as well as later SoundBlaster cards, like the SoundBlaster 32).

This final step allowed for up to 10 instruments, even in normal (non-percussive) mode. General MIDI hardware tends to support a much higher level of polyphony (often 32 or more voices), but I had designed the software to work with the limits of my original playback hardware: an OPL2/OPL3 synthesizer. An OPL2 limits playback to 10 voices in percussive mode (6 non-percussive + 4 percussive) or just 9 voices in non-percussive mode. OPL3 chips double this number to 20 and 18, respectively, but only with 100% left or right pan. So I chose to support just 10 or 9 voices, but added stereo support with an OPL3. Instead of using 20 or 18 distinct voices, the same instrument and note is emitted to two voices, one panned hard left and the other panned hard right. Arbitrary panning of the voice is done by adjusting the volumes of the two voices.

### Tools

What you will sadly not find in this repo is information on the tools I used to create some of the data files, like the IBK instrument bank files. I had found shareware DOS programs for editing instrument bank files. The shareware music composition program that served as a sort of "template" for MIDI Compozer included its own. But I really didn't like it. So I found an IBK meant to simulate the instruments in a General MIDI bank (128 different instruments/timbres). I then tweaked them, using the instrument bank editor. In some cases, I preferred the instruments in that shareware program, so I swapped those in for some of the instruments.

I originally developed this program and built it using Borland Turbo C++. The executable included in this repo was built with a later toolset: Borland C++ 4.5. In the source directory is a `Makefile` for building the program using Borland C++. I have not bothered trying to build this with more recent or open-source tools.

----

*(This repo was exported from http://code.google.com/p/midi-compozer on 3/21/2015.)*
