# AYT Console Player (CLI)
by Tronic /GPA + Longshot & Siko / Logon System

## Introduction

**AYT Console Player** is a command-line interface (CLI) application designed to play audio files in **AYT format** 

This player is written in standard **C++17** and uses the **SDL2** library for cross-platform audio output. It includes the full AYT file format parser and register expansion logic, along with a high-fidelity **AY-3-8910** sound chip emulation core.

It supports two DAC models (AY and YM) and allows for overriding the master clock and replay rate.

-----

## Build Instructions

The player requires the **SDL2** development library to be installed on your system. It should be straightforward to compile it on many platforms, as it only requires SDL2 for audio playback.  Here are some instructions for building under windows and linux, using g++.

### Linux / macOS (GCC/Clang)

The compilation uses `sdl2-config` to automatically retrieve the necessary compiler flags and linker arguments.

```bash
# Compile and link
g++ -std=c++17 -O2 -pipe -s \
    ayt-player-cli.cpp -o ayt-player \
    $(sdl2-config --cflags --libs)

# Run
./ayt-player my_song.ayt
```

### Windows (MinGW-w64)

The Windows compilation is configured for robustness and distribution, linking the GCC runtime libraries **statically** to avoid dependencies like `libmcfthread-1.dll`, while linking **dynamically** to **`SDL2.dll`** (which must be distributed with the executable).

**Prerequisites:** Ensure you have the 64-bit SDL2 development libraries installed and accessible to MinGW.

```bash
# Compile and link (Ensure -lSDL2main and -lSDL2 are present)
g++ -std=c++17 -O2 -pipe -s \
     ayt-player-cli.cpp -o ayt-player.exe \
    -lmingw32 \
    -lSDL2main -lSDL2 \
    -Wl,-Bstatic -lstdc++ -static-libgcc

# Distribution Note: 
# The resulting ayt-player.exe MUST be distributed with the matching 64-bit SDL2.dll.
```

-----

## Usage Examples

The player takes the input file path as the first argument and supports some optional command-line flags.

### Basic Playback

Simply run the executable followed by the AYT file path. The playback will start at 48000 Hz using the YM DAC model. Press **Enter** to stop playback.

```bash
./ayt-player my_awesome_track.ayt
```

### Specifying Sample Rate

Use the `-rate` flag to define the audio output sample rate (e.g., for 44100 Hz).

```bash
./ayt-player my_awesome_track.ayt -rate=44100
```

### Changing the DAC Model

By default, the player uses the YM (Atari ST, modern) volume table for higher dynamic range. Use the `-dac` flag to select a different model:

  * **YM** (Default): YM2149F Volume Table.
  * **AY**: AY-3-8910 Volume Table (e.g., Amstrad CPC, Spectrum).


```bash
# Use AY volume table
./ayt-player my_awesome_track.ayt -dac=ay
```

### Overriding the Master Clock

If the playback speed or pitch sounds incorrect, you can force the AY chip's master clock frequency using the `-clock` flag (in Hertz).

```bash
# Force a 2MHz clock (2000000 Hz)
./ayt-player my_awesome_track.ayt -clock=2000000
```

#  Additional Notes

## Associate ayt with the player

### Linux

Copy the player to place you can execute it (e.g. /usr/local/bin), and in your windows manager, you can associate ayt files to this command:

```bash
xterm -e /usr/local/bin/aytplayer  
```

or make a bash script taking $1 parameter:

```bash
xterm -e /usr/local/bin/aytplayer $1
```

### Windows

Under windows you can create a .bat file

```bat
\path\to\aytplayer.exe %1%
```
then associate this .bat file to ayt files. 
