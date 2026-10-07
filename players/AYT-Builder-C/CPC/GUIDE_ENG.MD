# AYT PC BUILD - DISTRIBUTION
--------------------------

## Status of this distribution

This folder contains the PC tool that replaces the *AYT runtime Z80 builder*.

It reads a source .ayt file, directly generates the specialized Z80 player, relocates the AYT runtime pointers, and then produces the files usable on Amstrad CPC.

The examples in this guide use the .ayt files located at the **root of the repository**, in `ayt-files/`, with:

		player_addr = #0040
		loops       = 2

Results are not shipped pre-generated: they are created by the commands below (in an `out/` folder).


## Folder content

		players/AYT-Builder-C/CPC/
			bin/aytpcbuild.c
			- Portable C99 source.

			bin/build/aytpcbuild-win.exe
			- Precompiled Windows executable.

			bin/build/aytpcbuild-menu.bat
			- Interactive Windows menu script to run the executable without typing the whole command line.

			web/aytpcbuild-web.html
			- Web application (browser).

		ayt-files/   (at the repository root)
		Source AYT files used in the examples:
			david_whittaker_demo.ayt
			med-Merci_Monsieur.ayt
			med-My_Ai_Is_Dumb.ayt
			still_scrolling.ayt


## Compiling the tool

From the repository root.

Linux or macOS:

		cc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild players/AYT-Builder-C/CPC/bin/aytpcbuild.c

Windows with GCC:

		gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild.exe players\AYT-Builder-C\CPC\bin\aytpcbuild.c

The C code does not use any external dependencies.

Windows without a compiler: use `players\AYT-Builder-C\CPC\bin\build\aytpcbuild-win.exe` directly in place of `aytpcbuild`.

In the following examples, `aytpcbuild` means the compiled executable, run from the repository root.


## First example (5 minutes)

Generate, for the music `ayt-files/still_scrolling.ayt`, a bundle (player + data) loaded at #0040, with a small test program:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
			--player-addr 0x0040 --ayt-addr 0x1000 --loops 2 --mode call \
			--out-bundle out/still_scrolling.bundle.bin \
			--out-asm-bundle out/still_scrolling.bundle.asm \
			--report out/still_scrolling.report.txt

(create the `out/` folder first: `mkdir out`)

The tool prints:

		AYT PC build ok: player=359 bytes, ayt=11051 bytes at #01A7, cpu=463 nops, init block present

and writes in `out/`:

		still_scrolling.bundle.bin     (11410 bytes = player 359 + AYT runtime 11051, loaded at #0040)
		still_scrolling.bundle.asm     (test program, RUN #2CD2)
		still_scrolling.report.txt     (readable report)

The test program then has to be assembled (see [Assembling the test ASMs](#assembling-the-test-asms)):

		cd out
		<assembler> still_scrolling.bundle.asm still_scrolling.bundle-demo.bin

The resulting binary loads at #0040 and runs at #2CD2.


## Windows interactive script

The `bin\build\aytpcbuild-menu.bat` script expects the executable next to it, named `aytpcbuild.exe`, and reads the `.ayt` files from the `ayt\` sub-folder of the current folder. So prepare a working folder:

		copy  players\AYT-Builder-C\CPC\bin\build\aytpcbuild-menu.bat   -> work\
		copy  players\AYT-Builder-C\CPC\bin\build\aytpcbuild-win.exe    -> work\aytpcbuild.exe
		copy  ayt-files\*.ayt                                           -> work\ayt\

Then, from `work\`, run:

		aytpcbuild-menu.bat

The script prompts for:

		- the AYT file to process;
		- the generation case: call_two, call_bundle, sp_two or sp_bundle;
		- the player address;
		- the AYT runtime address;
		- the number of loops;
		- the program_addr and return_addr addresses in SP/JP mode;
		- the output folder;
		- whether or not to generate the mini ASM and the TXT report.

The script only launches the aytpcbuild executable. It does not assemble anything and does not create *-demo.bin files.

### Example with still_scrolling.ayt

Example of responses to generate a CALL bundle for still_scrolling.ayt:

*AYT Choice*
		- Choose still_scrolling.ayt from the list.


| Option | Parameter value | 
| :-----------: | :--------------: |
| Generation case  | 2. CALL - compact bundle  |
| Player CPC address | 0x0040  |
| Initial AYT address for the tool | 0x1000  |
| Number of plays / loops |  2 |
|  Root output folder |  results\manual |
|  Final folder | results\manual\call_bundle\still_scrolling |
|  Generate mini test ASM |    O |
|  Generate TXT report |    O |

The script then runs aytpcbuild and produces:

		results\manual\call_bundle\still_scrolling\still_scrolling.bundle.bin
		results\manual\call_bundle\still_scrolling\still_scrolling.bundle.asm
		results\manual\call_bundle\still_scrolling\still_scrolling.report.txt

The generated ASM file remains portable inside its folder. Its INCBIN directive only contains the binary filename, not the full path:
		incbin "still_scrolling.bundle.bin"

Therefore, it should not output:
		incbin "results\manual\call_bundle\still_scrolling\still_scrolling.bundle.bin"


## Web Html/Js Application

Open in a web browser:

		players/AYT-Builder-C/CPC/web/aytpcbuild-web.html

then load one or more `.ayt` files (for example those in `ayt-files/`).


### General syntax

		aytpcbuild --input music.ayt --player-addr 0x0100 \
		--ayt-addr 0x1000 --loops N --mode jp|call [--return-addr ADDR] \
		[--out-player player.bin] [--out-ayt-runtime music.runtime.ayt] \
		[--out-bundle runtime.bin] [--bundle-base ADDR] \
		[--out-asm-two test-two.asm] [--out-asm-bundle test-bundle.asm] \
		[--program-addr ADDR] \
		[--report report.txt]


#### Options

		--input file.ayt
			Source AYT file.

		--player-addr ADDR
			CPC address where the generated player will be loaded.

		--ayt-addr ADDR
			CPC address of the AYT runtime in the two-files setup.

		If --out-bundle is requested, the tool always produces a compact bundle.
		In this case, the AYT runtime is placed immediately after the player and its address is automatically recalculated. The --ayt-addr value then serves as the initial address before compaction.

		--loops N
			Number of times the music plays, from 0 to 255.
			The examples in this guide use --loops 2.

		--mode call
			Generates a player called with:
				call AYT_Player
			In this mode, the player saves and restores SP by itself.

		--mode jp
			Generates the "fast" SP/JP mode.
			The programme must call the player with:
				jp AYT_Player
				The player uses SP as a fast pointer, then returns via a JP to the address provided by --return-addr.

		--return-addr ADDR
			Mandatory in jp mode.
			For the test ASMs generated by the tool:
				return_addr = program_addr + #24

		--program-addr ADDR
				Address of the mini test ASM program generated by --out-asm-two or --out-asm-bundle.
  			In call mode, this option is optional.
			If absent, the tool places the launcher at #0500 when possible, otherwise right after the generated data.
			In jp mode, this option is optional but must remain consistent with --return-addr.

		--out-player file.bin
			Writes the generated Z80 player.

		--out-ayt-runtime file.ayt
			Writes the AYT runtime, with relocated sequence pointers.

		--out-bundle file.bin
			Writes a compact bundle:
				player + ayt-runtime
			There is no padding hole in this file.

		--bundle-base ADDR
			Base of the bundle.
			In this version, this address must match --player-addr.

		--out-asm-two file.asm
			Generates a mini test ASM source for the two-files setup:
				player.bin + runtime.ayt
				This option also requires --out-player and --out-ayt-runtime.

		--out-asm-bundle file.asm
			Generates a mini test ASM source for the bundle setup:
				bundle.bin
				This option also requires --out-bundle.

		--report file.txt
			Writes a human-readable text report: sizes, addresses, active registers, initialization block, CPU cost, generated files and RUN of the test ASMs.

		--help
			Displays short help.


### The 4 generation cases

#### 1. call_two

CALL mode, two files:
		<name>.player.bin
		<name>.runtime.ayt
		<name>.two.asm
		<name>.report.txt

#### 2. call_bundle
CALL mode, compact bundle:

		<name>.bundle.bin
		<name>.bundle.asm
		<name>.report.txt

#### 3. sp_two
SP/JP mode, two files:

		<name>.player.bin
		<name>.runtime.ayt
		<name>.two.asm
		<name>.report.txt

#### 4. sp_bundle
SP/JP mode, compact bundle:

		<name>.bundle.bin
		<name>.bundle.asm
		<name>.report.txt

After assembling the test .asm files, you also get `<name>.two-demo.bin` or `<name>.bundle-demo.bin`.


### Command examples

The examples below are meant to be run from the repository root, using the files in `ayt-files/`. Outputs go to `out/<case>/<name>/` (create the folder first, e.g. `mkdir -p out/call_two/still_scrolling`).

#### CALL two files, still_scrolling example:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode call \
		--out-player out/call_two/still_scrolling/still_scrolling.player.bin \
		--out-ayt-runtime out/call_two/still_scrolling/still_scrolling.runtime.ayt \
		--out-asm-two out/call_two/still_scrolling/still_scrolling.two.asm \
		--report out/call_two/still_scrolling/still_scrolling.report.txt

Result: 359-byte player at #0040, AYT runtime at #1000, test program (RUN) at #3B2B.

#### CALL compact bundle, still_scrolling example:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode call \
		--out-bundle out/call_bundle/still_scrolling/still_scrolling.bundle.bin \
		--out-asm-bundle out/call_bundle/still_scrolling/still_scrolling.bundle.asm \
		--report out/call_bundle/still_scrolling/still_scrolling.report.txt

Result: 11410-byte bundle at #0040, AYT runtime at #01A7, RUN at #2CD2.

#### SP/JP two files, still_scrolling example:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode jp \
		--program-addr 0x3B2B \
		--return-addr 0x3B4F \
		--out-player out/sp_two/still_scrolling/still_scrolling.player.bin \
		--out-ayt-runtime out/sp_two/still_scrolling/still_scrolling.runtime.ayt \
		--out-asm-two out/sp_two/still_scrolling/still_scrolling.two.asm \
		--report out/sp_two/still_scrolling/still_scrolling.report.txt

Result: 355-byte player at #0040, AYT runtime at #1000, RUN at #3B2B.

#### SP/JP compact bundle, still_scrolling example:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode jp \
		--program-addr 0x2CCE \
		--return-addr 0x2CF2 \
		--out-bundle out/sp_bundle/still_scrolling/still_scrolling.bundle.bin \
		--out-asm-bundle out/sp_bundle/still_scrolling/still_scrolling.bundle.asm \
		--report out/sp_bundle/still_scrolling/still_scrolling.report.txt

Result: 11406-byte bundle at #0040, AYT runtime at #01A3, RUN at #2CCE.

#### Processing every file of the repository at once (CALL bundle)

		mkdir -p out/call_bundle
		for f in ayt-files/*.ayt; do
		  n=$(basename "$f" .ayt)
		  mkdir -p "out/call_bundle/$n"
		  aytpcbuild --input "$f" --player-addr 0x0040 --ayt-addr 0x1000 --loops 2 --mode call \
		    --out-bundle "out/call_bundle/$n/$n.bundle.bin" \
		    --out-asm-bundle "out/call_bundle/$n/$n.bundle.asm" \
		    --report "out/call_bundle/$n/$n.report.txt"
		done

#### Choosing program-addr and return-addr in SP/JP mode

In jp mode, `--return-addr` is mandatory and equals `program-addr + #24` for the generated test ASMs. To get the addresses without computing them, first generate the matching CALL case and read the `RUN` address in its report:

- two files: `--program-addr` = RUN of the call_two case;
- bundle: `--program-addr` = RUN of the call_bundle case - 4 (the SP/JP player is 4 bytes shorter).

The values are given in the tables below.


### Assembling the test ASMs

The generated .asm files use a Maxam-style syntax (`org`, `equ`, `incbin`, `run`). **Any modern Z80 assembler** will do (for example rasm, sjasmplus, fantams, pasmo, etc.); Maxam is not included in this repository. Depending on the assembler, the `run $` directive or the way the output is written (binary or SNA file) may need a small adaptation: refer to its documentation.

The ASM source must be able to access the files called by INCBIN. The ASMs generated by aytpcbuild only use the filename, for example:

		incbin "still_scrolling.bundle.bin"

So run the assembler **from the folder** that contains the .asm and the referenced .bin (or put them together).

Example, call_two case:

		cd out/call_two/still_scrolling
		<assembler> still_scrolling.two.asm still_scrolling.two-demo.bin

Example, call_bundle case:

		cd out/call_bundle/still_scrolling
		<assembler> still_scrolling.bundle.asm still_scrolling.bundle-demo.bin

(`<assembler>` and its command line depend on the chosen tool.) With fantams, for instance:

		fantams still_scrolling.bundle.asm -o still_scrolling.bundle-demo.bin

The result loads at the player address (#0040) and runs at the `RUN` address given in the report (#2CD2 for this bundle).

If you prefer an online assembler, any web Z80 assembler accepting several files will do; provide the .asm and the .bin together.


## Summary of the repository AYT files

All the examples below use (values generated by aytpcbuild from `ayt-files/`):

	player_addr = #0040
	loops       = 2

### CALL two files:

| AYT name | AYT Runtime | Player size | demo RUN | 
| :-----------: | :--------------: | :-------------: | :---------: |
| david_whittaker_demo.ayt | #1000 | 326 bytes | #3BB0 |
| med-Merci_Monsieur.ayt | #1000 | 326 bytes | #4574 |
| med-My_Ai_Is_Dumb.ayt | #1000 | 359 bytes | #3D37 |
| still_scrolling.ayt | #1000 | 359 bytes | #3B2B |


### CALL compact bundle:

| AYT name | AYT Runtime | Bundle size | demo RUN | 
| :-----------: | :--------------: | :-------------: | :---------: |
| david_whittaker_demo.ayt | #0186 | 11510 bytes | #2D36 |
| med-Merci_Monsieur.ayt | #0186 | 14010 bytes | #36FA |
| med-My_Ai_Is_Dumb.ayt | #01A7 | 11934 bytes | #2EDE |
| still_scrolling.ayt | #01A7 | 11410 bytes | #2CD2 |

### SP/JP two files:
| AYT name | AYT Runtime | Player size | demo RUN | return | 
| :-----------: | :--------------: | :-------------: | :---------: | :-------: |
| david_whittaker_demo.ayt | #1000 | 322 bytes | #3BB0 | #3BD4 |
| med-Merci_Monsieur.ayt | #1000 | 322 bytes | #4574 | #4598 |
| med-My_Ai_Is_Dumb.ayt | #1000 | 355 bytes | #3D37 | #3D5B |
| still_scrolling.ayt | #1000 | 355 bytes | #3B2B | #3B4F |

### SP/JP compact bundle:

| AYT name | AYT Runtime | Bundle size | demo RUN | return | 
| :-----------: | :--------------: | :-------------: | :---------: | :-------: |
| david_whittaker_demo.ayt | #0182 | 11506 bytes | #2D32 | #2D56 |
| med-Merci_Monsieur.ayt | #0182 | 14006 bytes | #36F6 | #371A |
| med-My_Ai_Is_Dumb.ayt | #01A3 | 11930 bytes | #2EDA | #2EFE |
| still_scrolling.ayt | #01A3 | 11406 bytes | #2CCE | #2CF2 |

(In SP/JP mode, `demo RUN` is the value to pass as `--program-addr`, and `return` as `--return-addr`.)


## Important notes

1. --out-bundle always generates a compact bundle.
The old mode with holes no longer exists.

2. In a bundle, the AYT runtime is placed immediately after the player.
The report gives the final address.

3. In SP/JP mode, --return-addr is mandatory.
For the test ASMs generated by the tool:
		return_addr = program_addr + #24

4. The .runtime.ayt files are not the original source AYT files.
They contain pointers relocated for the final CPC address.

5. The prebuilt player no longer depends on the Z80 ASM builder at runtime.
The builder is replaced by the PC tool aytpcbuild.
