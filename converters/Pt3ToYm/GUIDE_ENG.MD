# PT3 - PT3 to YM6 Converter
--------------------------

This folder contains a portable command-line tool, written in C,
which converts Pro Tracker 3 modules (.pt3) into YM6 files (.ym).

The principle is not to play the music as audio, but to replay the
tracker logic at 50 Hz in order to produce, frame by frame, the AY/YM
registers required by the YM6 format.


## Main files

		pt3.c
			: portable C99 source code.

		pt3.exe
			: compiled Windows binary.


## Compilation

The tool has no external dependencies. It only uses the standard C library
and basic system APIs to browse folders.


### Windows with MinGW / TDM-GCC

From the folder containing pt3.c :

		gcc -O2 -std=c99 -Wall -Wextra -static -o pt3.exe pt3.c

The -static option allows producing a more self-contained .exe. If your toolchain
does not handle static linking well, you can remove -static :

		gcc -O2 -std=c99 -Wall -Wextra -o pt3.exe pt3.c


### Windows with MSVC Build Tools

From a Visual Studio "Developer Command Prompt" :

		cl /nologo /O2 /TC pt3.c /Fe:pt3.exe


### Linux

From the folder containing pt3.c :

		cc -O2 -std=c99 -Wall -Wextra -o pt3 pt3.c

Then run :

		./pt3 file.pt3


### macOS

With clang, provided by Apple development tools :

		clang -O2 -std=c99 -Wall -Wextra -o pt3 pt3.c

or simply :

		cc -O2 -std=c99 -Wall -Wextra -o pt3 pt3.c

Then run :

		./pt3 file.pt3


## Basic usage

Convert a PT3 file to YM6 with an automatic output filename :

		pt3 music.pt3

On Linux/macOS :

		./pt3 music.pt3

The generated file will be :

		music.ym

Explicitly choose the output file :

		pt3 music.pt3 output.ym

or :

		pt3 music.pt3 -o output.ym


## Batch conversion

Convert all .pt3 files from the current folder :

		pt3 "*.pt3"

On Linux/macOS :

		./pt3 "*.pt3"

Convert all .pt3 files from a folder :

		pt3 "mods/*.pt3"

Send the .ym files to an existing output folder :

		pt3 "mods/*.pt3" -o ym6

Important : in batch mode, if -o is used with multiple input files,
the destination must be an already existing folder.

If an error occurs on one file during a batch conversion, the tool continues
with the remaining files. The final return code is 1 if at least one conversion
failed.


## Recursive conversion

The -r option, or --recursive, allows browsing subfolders.

Convert all .pt3 files found in a folder and all its subfolders :

		pt3 -r mods

On Linux/macOS :

		./pt3 -r mods

Send all .ym files to an existing output folder :

		pt3 -r mods -o ym6

Use a recursive file pattern :

		pt3 -r "mods/*.pt3" -o ym6

With -r folder, only files ending with .pt3 are collected.
With -r "folder/*.pt3", the pattern is applied inside the specified folder
and all its subfolders.

Important note : with -o folder, the output is flattened into this folder.

For example :

		mods/a/theme.pt3
		mods/b/theme.pt3

will both produce :

		ym6/theme.ym

The second file may therefore overwrite the first one. To avoid this case,
run the conversion without -o so that .ym files are created next to the .pt3
files, or process subfolders separately.


## Does recursion work on Linux and macOS ?

Yes, it is designed to work on Windows, Linux and macOS.

The code uses two paths depending on the platform :

		Windows       : FindFirstFileA / FindNextFileA
		Linux/macOS   : opendir / readdir / stat

The opendir, readdir and stat functions are standard POSIX APIs available
on Linux and macOS. The same source code should therefore compile and browse
subfolders on these systems.

The version tested locally here was tested on Windows. The Linux/macOS part
uses standard POSIX APIs, but ideally a validation build should still be done
on a real Linux/macOS machine before release.


## Available options

		-o FILE
			Output file for single conversion.

		-o DIR
			Output folder for batch or recursive conversion. The folder must exist.

		-r, --recursive
			Recursively browse subfolders.

		--chip N
			Export only chip N, starting from 1.
			Useful for TurboSound files.

		--no-pt36-ts
			Disable the TurboSound detection heuristic for some PT3.6 files.

		--clock HZ
			Set the master clock frequency written in the YM header.
			Default value is 1773400.

		--max-frames N
			Safety limit during rendering.

		-h, --help
			Display help.


## TurboSound

When a PT3 file contains multiple AY/YM chips, the tool exports one YM file
per chip, like Vortex Tracker.

Example :

		music.1.ym
		music.2.ym

To export only the first chip :

		pt3 music.pt3 --chip 1

To export only the second chip :

		pt3 music.pt3 --chip 2


## Generated YM format

The tool produces a YM6 stream :

		- 50 Hz frames ;
		- AY/YM registers exported frame by frame ;
		- non-interleaved data ;
		- register 13 written with the 0xFF marker when the envelope must not
		  be retriggered.

This behaviour is aligned with the YM export from Vortex Tracker.


## Quick examples

Single file :

		pt3 "Mast - Insomnia (1999).pt3"

All PT3 files in the current folder :

		pt3 "*.pt3"

All PT3 files from a folder and its subfolders :

		pt3 -r "pt3-2025-2026"

All PT3 files recursively to an existing ym6 folder :

		pt3 -r "pt3-2025-2026" -o ym6

With a different clock :

		pt3 music.pt3 --clock 1750000
