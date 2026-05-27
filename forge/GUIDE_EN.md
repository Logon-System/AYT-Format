# aytforge - PT1/PT2/PT3/SQT/YM/PSG to AYT converter + CPC/CPC+ builders

---

## 1. Purpose

aytforge is a command-line tool that converts AY/YM music files into AYT files.

Accepted input formats:

- `.pt1`
- `.pt2`
- `.pt3`
- `.sqt`
- `.ym`
- `.ym5`
- `.ym6`
- `.psg`

Expected YM files are uncompressed YM5/YM6 "LeOnArD!" dumps. Packed YM archives,
compressed variants, or files with unsupported special data must be converted to a
plain YM5/YM6 dump before use.

Expected PSG files are AY register streams in AY-Emul/VT format:
"PSG" header + 0x1A, commands 0xFF/0xFE/0xFD, and register writes.
R13 is treated as an event: frames without an R13 write receive the sentinel 0xFF
to avoid re-triggering the envelope.

Expected SQT files are compiled SQ-Tracker modules. This format is imported through
separate logic from PT1/PT2/PT3: its positions combine three channel patterns with
volume, transposition, effect flags, and speed.

The full processing chain is:

```
source PT1/PT2/PT3/SQT/YM/PSG
  -> AY/YM register frames
  -> clock correction if needed
  -> AYT compression
  -> optional generation of a preconfigured CPC/CPC+ player
```

aytforge is not meant for editing or playing music. Its purpose is to produce a
compact AYT file and, if requested, a binary/ASM bundle ready to integrate on the
CPC side.

---

## 2. Origin of the AYT converter

The AYT encoding/compression core comes from the work of Logon System, by Siko,
available in the GitHub repository:

  https://github.com/Logon-System/AYT-Format

aytforge wraps this AYT core into a single CLI, with PT1/PT2/PT3/SQT/YM/PSG input,
clock correction, batch processing, recursion, interactive mode, and CPC/CPC+ builders.

The maxam folder contains the Z80 assembler used to assemble final ASM/bundle tests
and produce SNA/DSK files if needed.

---

## 3. Building

The project is written in C++17 and has no mandatory external dependencies.

Requirements:

| Platform | Compiler |
|----------|----------|
| Windows  | Recent g++ MinGW-w64, or any equivalent C++17 compiler |
| Linux    | g++ or clang++ with C++17 support |
| macOS    | Recent clang++ via Xcode Command Line Tools or Homebrew |

> **Important:** `std::filesystem` must be available. On very old GCC versions, it
> may be necessary to append `-lstdc++fs` at the end of the command.

### 3.1 Building on Windows

From the folder containing this README:

**PowerShell:**

```powershell
g++ -std=c++17 -O2 -Wall src\aytforge.cpp src\pt3_bridge.cpp src\psg_loader.cpp src\ayt_encoder.cpp src\builder_cpc.cpp src\builder_cpcplus.cpp third_party\ym2ayt\YMParser.cpp third_party\ym2ayt\ayt.cpp third_party\ym2ayt\optimizers.cpp third_party\ym2ayt\optimizer_ga.cpp third_party\ym2ayt\optimizer_ils.cpp third_party\ym2ayt\optimizer_sa.cpp third_party\ym2ayt\optimizer_tabu.cpp third_party\ym2ayt\platforms.cpp -o aytforge.exe -static
```

**cmd.exe:**

```bat
g++ -std=c++17 -O2 -Wall ^
  src\aytforge.cpp src\pt3_bridge.cpp src\psg_loader.cpp src\ayt_encoder.cpp src\builder_cpc.cpp src\builder_cpcplus.cpp ^
  third_party\ym2ayt\YMParser.cpp ^
  third_party\ym2ayt\ayt.cpp ^
  third_party\ym2ayt\optimizers.cpp ^
  third_party\ym2ayt\optimizer_ga.cpp ^
  third_party\ym2ayt\optimizer_ils.cpp ^
  third_party\ym2ayt\optimizer_sa.cpp ^
  third_party\ym2ayt\optimizer_tabu.cpp ^
  third_party\ym2ayt\platforms.cpp ^
  -o aytforge.exe -static
```

> **Note:** The `^` character is used to continue a line in cmd.exe. In
> PowerShell, use the single-line command shown above.

**Run:**

```powershell
.\aytforge.exe --help
```

### 3.2 Building on Linux

From the folder containing this README:

```bash
c++ -std=c++17 -O2 -Wall \
  src/aytforge.cpp src/pt3_bridge.cpp src/psg_loader.cpp src/ayt_encoder.cpp src/builder_cpc.cpp src/builder_cpcplus.cpp \
  third_party/ym2ayt/YMParser.cpp \
  third_party/ym2ayt/ayt.cpp \
  third_party/ym2ayt/optimizers.cpp \
  third_party/ym2ayt/optimizer_ga.cpp \
  third_party/ym2ayt/optimizer_ils.cpp \
  third_party/ym2ayt/optimizer_sa.cpp \
  third_party/ym2ayt/optimizer_tabu.cpp \
  third_party/ym2ayt/platforms.cpp \
  -o aytforge
```

**Run:**

```bash
./aytforge --help
```

In the examples below, Windows commands use `.\aytforge.exe`.
On Linux and macOS, simply replace with `./aytforge`.

### 3.3 Building on macOS

From the folder containing this README:

```bash
clang++ -std=c++17 -O2 -Wall \
  src/aytforge.cpp src/pt3_bridge.cpp src/psg_loader.cpp src/ayt_encoder.cpp src/builder_cpc.cpp src/builder_cpcplus.cpp \
  third_party/ym2ayt/YMParser.cpp \
  third_party/ym2ayt/ayt.cpp \
  third_party/ym2ayt/optimizers.cpp \
  third_party/ym2ayt/optimizer_ga.cpp \
  third_party/ym2ayt/optimizer_ils.cpp \
  third_party/ym2ayt/optimizer_sa.cpp \
  third_party/ym2ayt/optimizer_tabu.cpp \
  third_party/ym2ayt/platforms.cpp \
  -o aytforge
```

**Run:**

```bash
./aytforge --help
```

---

## 4. Basic usage

Convert a PT3 to AYT, default CPC target:

```powershell
.\aytforge.exe "music.pt3"
```

Convert a PT2 to AYT:

```powershell
.\aytforge.exe "music.pt2"
```

Convert a PT1 to AYT:

```powershell
.\aytforge.exe "music.pt1"
```

Convert a SQT to AYT:

```powershell
.\aytforge.exe "music.sqt"
```

Convert a YM to AYT:

```powershell
.\aytforge.exe "music.ym"
```

Convert a YM6:

```powershell
.\aytforge.exe "music.ym6"
```

Convert a PSG:

```powershell
.\aytforge.exe "music.psg"
```

Convert without generating the CPC/CPC+ player:

```powershell
.\aytforge.exe "music.pt3" --build-player none
```

Choose the output folder:

```powershell
.\aytforge.exe "music.pt3" --out-dir ayt
```

Analyze only, without writing any file:

```powershell
.\aytforge.exe "music.pt3" --dry-run
```

Show more details:

```powershell
.\aytforge.exe "music.pt3" -v
```

---

## 5. Multiple files, batch and recursion

Provide multiple files explicitly:

```powershell
.\aytforge.exe "a.pt1" "b.pt2" "c.pt3" "d.sqt" "e.ym" "f.ym6" "g.psg" --out-dir ayt
```

Process all compatible files in the current folder, without descending into subfolders:

```powershell
.\aytforge.exe "." --out-dir ayt
```

Process only PT1/PT2/PT3/SQT files in the current folder:

```powershell
.\aytforge.exe "*.pt1" "*.pt2" "*.pt3" "*.sqt" --out-dir ayt
```

Process multiple wildcards:

```powershell
.\aytforge.exe "*.pt1" "*.pt2" "*.pt3" "*.sqt" "*.ym" "*.ym5" "*.ym6" "*.psg" --out-dir ayt
```

Process a full folder recursively:

```powershell
.\aytforge.exe -r "mods" --out-dir ayt
```

Process the current folder and all its subfolders:

```powershell
.\aytforge.exe -r "." --out-dir ayt
```

Recursion has no hard-coded maximum depth. It descends into all subfolders accessible
by the filesystem. Practical limits are access rights, overly long paths, and system
behaviour regarding symbolic links.

In batch mode, an error on one file is reported but processing continues on the
remaining files. At the end, the tool displays the failure count. The exit code is 0
if everything succeeded, 1 if at least one conversion failed.

---

## 6. Clocks, machines and frequency correction

AY/YM music may come from machines with a different audio clock.
aytforge can correct the periods to adapt the music to a target machine.

Main options:

```
--source zx128|pentagon|zxuno|custom
--source-clock HZ
--target cpc|cpcplus|msx|zx128|pentagon|atari|vg5000|custom
--target-clock HZ
--rate 50|25|60|30|100|200|300
--no-scale
```

Defaults:

| Parameter | Value |
|-----------|-------|
| source PT1/PT2/PT3/SQT | zx128, 1773400 Hz |
| target | cpc, 1000000 Hz |
| rate | 50 Hz |

The `--source auto` value is accepted by the CLI. For a PT1/PT2/PT3/SQT, it falls
back to the default zx128 profile. For a YM file, the best approach is to not force
`--source`: the tool will then read the clock from the YM header if present.

For a YM5/YM6 file, if no source is forced, the tool uses the clock and replay
frequency stored in the YM header when available.

For a PSG file, the stream already contains register writes but no reliable loop
point or portable source clock. By default, aytforge therefore uses the zx128 source
profile and 50 Hz. If the PSG already comes from a CPC, use
`--source cpc --target cpc`, or `--no-scale` if no frequency correction is desired.

Examples:

```powershell
.\aytforge.exe "music.pt3" --source zx128 --target cpc
.\aytforge.exe "music.pt3" --source pentagon --target cpc
.\aytforge.exe "music.ym" --target cpc
.\aytforge.exe "music.ym" --source custom --source-clock 2000000 --target cpc
.\aytforge.exe "music.psg" --source zx128 --target cpc
.\aytforge.exe "music.psg" --source cpc --target cpc
```

Disable all frequency correction:

```powershell
.\aytforge.exe "music.pt3" --no-scale
```

---

## 7. AYT quality and compression

Main options:

```
--quality fast|balanced|best
--pattern-size fast|auto|full|N|MIN:MAX/STEP
--optim greedy|sa|ga|tabu|ils|none
--sa-gen-min N
--sa-gen-max N
--sa-gen-ext N
--no-mask
--normalize-patterns
--r13-filter
--export-all-regs
```

### Presets

| Option | Description |
|--------|-------------|
| `--quality fast` | Fast, uses greedy and fast pattern-size. |
| `--quality balanced` | Speed/size trade-off, uses auto and a shorter simulated annealing run. |
| `--quality best` | Default value, tests the widest pattern-size range. |

### Pattern-size

| Value | Description |
|-------|-------------|
| `fast` | Tests 4..64 in steps of 4. |
| `auto` | Tests 8..96 in steps of 4. |
| `full` | Tests 1..255 in steps of 1. |
| `N` | Forces a single pattern size, between 1 and 255. |
| `MIN:MAX/STEP` | Tests a custom range. |

Examples:

```powershell
.\aytforge.exe "music.pt3" --quality fast
.\aytforge.exe "music.pt3" --quality best
.\aytforge.exe "music.pt3" --pattern-size full --optim sa
.\aytforge.exe "music.pt3" --pattern-size 16 --optim greedy
.\aytforge.exe "music.pt3" --pattern-size 1:255/1 --optim ils
```

### Optimization method notes

| Method | Description |
|--------|-------------|
| `none` | No advanced ordering optimization. |
| `greedy` | Fast and deterministic method. |
| `sa` | Simulated annealing. Good trade-off for finding a better size. |
| `ga` | Genetic algorithm. Expert mode. |
| `tabu` | Tabu search. Expert mode. |
| `ils` | Iterative local search. Can be slow, but may find smaller output. |

---

## 8. PT1, PT2, PT3, SQT and TurboSound

Options:

```
--chip auto|all|1|2
--no-pt36-ts
--max-frames N
```

By default, aytforge automatically processes the modules found in the PT3.
For a PT1, PT2, SQT, or standard PT3, this yields a single AYT. For a PT3
TurboSound, the tool can produce one output per chip.

`--max-frames` also acts as a safety limit for PT1/PT2/SQT rendering and PSG
decoding. PT1, PT2, SQT, YM, and PSG inputs are mono-chip in aytforge:
`--chip` must remain set to `auto/all/1` for these formats.

SQT is imported via the SQ-Tracker layer from VortexTracker: the format assembles
channel patterns by position and does not share the same internal model as PT1,
PT2, or PT3. Once rendered to AY registers, the AYT compression and CPC/CPC+
builders are strictly shared.

Examples:

```powershell
.\aytforge.exe "music.pt3" --chip auto
.\aytforge.exe "music.pt3" --chip 1
.\aytforge.exe "music.pt3" --chip 2
```

Disable the PT3.6 TurboSound heuristic:

```powershell
.\aytforge.exe "music.pt3" --no-pt36-ts
```

Limit the analysed duration:

```powershell
.\aytforge.exe "music.pt3" --max-frames 12000
```

---

## 9. Musical loop point

aytforge preserves the musical loop point when one exists in the source.

- For PT1/PT2/PT3/SQT, the loop point is derived from the module's loop position.
- For YM5/YM6, the loop point comes from the `loopFrame` field in the YM header.
- For PSG, no reliable loop point is derived: `loopFrame` remains 0.

The AYT format loops on a sequence entry. In practice, this means the loop point
must fall on an AYT pattern boundary to be exact to the frame.

Expected behaviour:

- If `loopFrame` is aligned with the chosen pattern-size, the loop is exact.
- Otherwise, the loop is placed as close as possible to a compatible AYT boundary.
- The music is not forced to loop back to the beginning unless the source itself loops from the beginning.
- A warning is displayed if the loop point is not aligned.

> **Important:** the musical `loopFrame` concept is different from the `--loops`
> builder option. `--loops` sets the repeat counter injected into the player; it does
> not change the loop point extracted from the PT1/PT2/PT3/SQT or YM file.

---

## 10. CPC / CPC+ Builders

When the target is CPC or CPC+, aytforge can generate, in addition to the AYT file,
a preconfigured player and a small ASM launcher.

The PT1/PT2/PT3/SQT/YM/PSG to AYT conversion is strictly shared. The divergence
starts only after AYT compression:

- `cpc`     → CPC old RAM builder, direct PPI/AY player
- `cpcplus` → CPC+ RAM builder, DMA/ASIC player

Options:

```
--build-player auto|cpc|cpcplus|none
--case call-bundle|call-two|sp-bundle|sp-two
--player-addr 0x0040
--ayt-addr 0x1000
--loops N
--program-addr 0xNNNN
--return-addr 0xNNNN
--asic-in on|off
--asic-out on|off
--dma-channel 0|1|2
--asic-page-on 0xB8
--asic-page-off 0xA0
--no-asm
--no-report
```

Defaults:

| Option | Value |
|--------|-------|
| `--target` | `cpc` |
| `--build-player` | `auto` |
| `--case` | `call-bundle` |
| `--player-addr` | `0x0040` |
| `--loops` | `255` |

With `--target cpcplus`, `--build-player auto` selects the CPC+ RAM builder.
With `--target cpc`, it selects the CPC old RAM builder.

### 10.1 Generated outputs

**Common output:**

- `music.ayt` — compressed AYT file.

**In bundle mode:**

- `music.bundle.bin` — player + AYT runtime in a single compact binary.
- `music.bundle.asm` — ASM example that incbins the bundle and calls the player.
- `music.build.txt` — report with addresses, sizes, CPU info, and produced files.

**In two-file mode:**

- `music.player.bin` — player blob only.
- `music.runtime.ayt` — AYT relocated to the requested runtime address.
- `music.two.asm` — ASM example that incbins the player and the AYT runtime.
- `music.build.txt` — report with addresses, sizes, CPU info, and produced files.

Disable the generated ASM:

```powershell
.\aytforge.exe "music.pt3" --no-asm
```

Disable the report:

```powershell
.\aytforge.exe "music.pt3" --no-report
```

### 10.2 The four builder cases

#### call-bundle

Recommended starting point. A single binary contains both the player and the AYT
runtime. `--player-addr` is the bundle load address. The AYT runtime address is
recalculated automatically right after the player.

```powershell
.\aytforge.exe "music.pt3" --case call-bundle --player-addr 0x0040
```

#### call-two

Two separate binaries: player and AYT runtime. `--player-addr` loads the player,
`--ayt-addr` loads the AYT runtime.

```powershell
.\aytforge.exe "music.pt3" --case call-two --player-addr 0x0040 --ayt-addr 0x1000
```

#### sp-bundle

Like call-bundle, but the launcher uses a JP/SP mode. This is a more advanced mode,
useful when the final integration wants to avoid a classic CALL.

```powershell
.\aytforge.exe "music.pt3" --case sp-bundle --player-addr 0x0040
```

#### sp-two

Two separate files, with JP/SP mode.

```powershell
.\aytforge.exe "music.pt3" --case sp-two --player-addr 0x0040 --ayt-addr 0x1000
```

### 10.2.1 CPC+ options

The CPC+ RAM builder adds ASIC page and DMA channel management:

```
--target cpcplus
--build-player cpcplus
--asic-in on|off
--asic-out on|off
--dma-channel 0|1|2
--asic-page-on 0xB8
--asic-page-off 0xA0
```

By default, the CPC+ builder uses DMA channel 0 and assumes the ASIC page is
connected at both player entry and exit:

```
--asic-in on --asic-out on --dma-channel 0
```

The ASIC page occupies `#4000-#7FFF` when connected. The CPC+ player blob generated
by aytforge — that is, the player code, initialisation routine, padding, and DMA list
— must never overlap this window.

The ASM launcher generated by aytforge must not execute inside `#4000-#7FFF` either,
as the ASIC page may be connected during the player call. In automatic placement mode,
aytforge therefore moves `MyProgram` to `#8000` if the bundle is too large and would
push the launcher into this window. If `--program-addr` explicitly forces an address
within `#4000-#7FFF` on CPC+, generation is refused with an explicit message.

The AYT runtime can be placed anywhere in RAM. If it touches or crosses `#4000-#7FFF`,
the generated player includes the necessary ASIC connection/disconnection code
according to `--asic-in` and `--asic-out`. The `.build.txt` report explicitly states:

```
AYT in ASIC range : yes/no
B0 ASIC off block : yes/no
```

The CPC+ player blob contains the player code, the optional AY initialisation routine,
any padding bytes, and the DMA list aligned to an even address without crossing a
256-byte page boundary.

The default RMR2 values are `#B8` to connect the ASIC page and `#A0` to disconnect it.
They can be changed with `--asic-page-on` and `--asic-page-off` if your program uses
a different lower ROM or memory mapping.

CPC+ bundle example:

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case call-bundle --player-addr 0x0100
```

CPC+ two-file example with AYT at #8000:

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case call-two --player-addr 0x0100 --ayt-addr 0x8000
```

CPC+ two-file example with AYT in the ASIC window:

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case call-two --player-addr 0x2200 --ayt-addr 0x5000 --program-addr 0x9000
```

CPC+ SP/JP example with automatic return address:

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case sp-two --player-addr 0x3200 --ayt-addr 0x8000 --program-addr 0x9000
```

### 10.3 Generated ASM example

In CPC+ bundle mode, the `.bundle.asm` file looks like this:

```asm
;; AYT prebuilt bundle test / CALL mode
;; Generated by aytforge.

AYT_Bundle equ #0040
AYT_Player equ AYT_Bundle
AYT_Init   equ #01AE
MyProgram  equ #0500

        org AYT_Bundle
        incbin "music.bundle.bin"

        org MyProgram
        run $

StartExample
        di
        ld sp,MyStack
        ...
        call AYT_Init
        ei
        ...
        call AYT_Player
        ...
        jr MainLoop
```

In CPC+ two-file mode, the `.two.asm` file places two binaries:

```asm
AYT_Player equ #0040
AYT_Init   equ #01AE
AYT_File   equ #1000
MyProgram  equ #0500

        org AYT_Player
        incbin "music.player.bin"

        org AYT_File
        incbin "music.runtime.ayt"

        org MyProgram
        run $
```

The ASM launcher is an integration example. Its purpose is to quickly validate
loading, addresses, AY initialisation, and the player call. On CPC+, it also includes
ASIC unlocking, ASIC page setup, a VSync wait, and the ei/halt/halt pair used in test
examples. On CPC old RAM, the generated example is simpler and has no separate
`AYT_Init` symbol: any initialisation is embedded in the generated player.
In a real CPC program, you only need to reuse the addresses, the produced binaries,
and the player call that matches your architecture.

---

## 11. Interactive mode

Interactive mode asks the main questions and suggests default values. Its display is
in English to stay consistent with the CLI, the online help, and the messages
generated by the tool.

Launch:

```powershell
.\aytforge.exe "music.pt3" --interactive
```

With display of the equivalent full command:

```powershell
.\aytforge.exe "music.pt3" --interactive --print-command
```

Yes/no questions are answered with `y` or `n`. The legacy `o/oui` responses are still
accepted but are no longer shown by default.

Interactive mode asks for, among other things:

- clock source
- AYT target
- quality mode
- whether to build the CPC/CPC+ player
- builder case
- key addresses
- number of loops
- CPC+ ASIC/DMA/RMR2 parameters if the target is cpcplus
- whether to print the full equivalent command

Available optimisation choices:

| Choice | Description |
|--------|-------------|
| `1. fast` | greedy + fast pattern-size. |
| `2. best` | simulated annealing + full pattern-size 1..255. |
| `3. expert` | manual choice of pattern-size, optimisation method, masking, R13, etc. |

Typical session excerpt:

```
aytforge interactive assistant
Source clock profile (auto/zx128/pentagon/zxuno/custom) [zx128]:
AYT target (cpc/cpcplus/msx/zx128/pentagon/atari/vg5000) [cpc]: cpcplus
Frequency scaling [y]:
Optimization choice [2]:
Build CPC/CPC+ prebuilt player [y]:
Selected builder: CPC+ RAM
Builder case (call-bundle/call-two/sp-bundle/sp-two) [call-bundle]:
Player address [0x0100]:
Loops [255]:
ASM launcher program address (auto/0xNNNN) [auto]:
CPC+ ASIC page at player entry (on/off) [on]:
CPC+ ASIC page after player exit (on/off) [on]:
CPC+ DMA channel (0/1/2) [0]:
CPC+ RMR2 ASIC page on value [0x00B8]:
CPC+ RMR2 ASIC page off value [0x00A0]:
```

In interactive bundle mode, only the player address is asked: it is the bundle load
address. The AYT runtime address is calculated automatically right after the player.

In interactive two-file mode, both the player address and the AYT runtime address are
asked.

In interactive CPC+ mode, aytforge reminds you that the generated player blob must not
overlap `#4000-#7FFF`, and that the generated ASM launcher must not execute within
that window either. The launcher address can remain on auto: aytforge moves it outside
`#4000-#7FFF` if the bundle is too large. If a forced `--program-addr` falls within
that window, generation is refused. If the AYT runtime starts within that window, or
starts before `#4000` and may cross it depending on its size, the tool displays an
explicit message and the final report resolves the issue with "AYT in ASIC range".

In interactive SP/JP mode, the return address can remain on auto. If you provide a
manual return address, it must match the actual return point of your launcher.

---

## 12. Useful commands

Help:

```powershell
.\aytforge.exe --help
```

Simple PT3 to CPC conversion:

```powershell
.\aytforge.exe "music.pt3"
```

Simple PT2 to CPC conversion:

```powershell
.\aytforge.exe "music.pt2"
```

Simple PT1 to CPC conversion:

```powershell
.\aytforge.exe "music.pt1"
```

Simple SQT to CPC conversion:

```powershell
.\aytforge.exe "music.sqt"
```

Simple YM to CPC conversion:

```powershell
.\aytforge.exe "music.ym"
```

Simple PSG to CPC conversion:

```powershell
.\aytforge.exe "music.psg"
```

AYT only, no builder:

```powershell
.\aytforge.exe "music.pt3" --build-player none
```

Batch current folder:

```powershell
.\aytforge.exe "." --out-dir ayt
```

Recursive batch:

```powershell
.\aytforge.exe -r "." --out-dir ayt
```

Fast compression:

```powershell
.\aytforge.exe "music.pt3" --quality fast
```

Maximum reasonable compression:

```powershell
.\aytforge.exe "music.pt3" --quality best
```

Slow/expert compression:

```powershell
.\aytforge.exe "music.pt3" --pattern-size full --optim ils
```

Force a Pentagon source to CPC:

```powershell
.\aytforge.exe "music.pt3" --source pentagon --target cpc
```

Force a custom clock:

```powershell
.\aytforge.exe "music.ym" --source custom --source-clock 2000000 --target cpc
```

Generate a CPC bundle at #4000:

```powershell
.\aytforge.exe "music.pt3" --case call-bundle --player-addr 0x4000
```

Generate two CPC files:

```powershell
.\aytforge.exe "music.pt3" --case call-two --player-addr 0x0040 --ayt-addr 0x4000
```

Print a reproducible full command:

```powershell
.\aytforge.exe "music.pt3" --interactive --print-command
```

---

## 13. Quick option reference

### Input/output

| Option | Description |
|--------|-------------|
| `-o, --output FILE` | Forces the `.ayt` filename when there is only one output. |
| `-P, --out-dir DIR` | Output folder. Long alias: `--out-dir`. |
| `-r, --recursive` | Enables recursive subfolder search. |
| `--dry-run, --analyze` | Analyses and compresses in memory, but writes nothing. |
| `-v, --verbose` | More detailed output. |
| `-q, --quiet` | Minimal output. |

### Machines/frequencies

| Option | Description |
|--------|-------------|
| `--source NAME` | Known source machine: `auto`, `zx128`, `pentagon`, `zxuno`, `custom`. |
| `--source-clock HZ` | Manual source clock, useful with `--source custom`. |
| `--target NAME` | Known target machine: `cpc`, `cpcplus`, `msx`, `zx128`, `pentagon`, `atari`, `vg5000`, `custom`. |
| `--target-clock HZ` | Manual target clock, useful with `--target custom`. |
| `--rate N` | Replay frequency. |
| `--no-scale` | Does not correct periods. |

### Compression

| Option | Description |
|--------|-------------|
| `--quality fast\|balanced\|best` | Global preset. |
| `--pattern-size ...` | Range of AYT pattern sizes to test. |
| `--optim ...` | Optimisation method. |
| `--no-mask` | Disables pattern masking. |
| `--normalize-patterns` | Normalises ignored bits before compression. |
| `--r13-filter` | Filters certain R13 repetitions. |
| `--export-all-regs` | Forces export of all registers. |

### PT1 / PT2 / PT3 / SQT

| Option | Description |
|--------|-------------|
| `--chip auto\|all\|1\|2` | Selects PT3 chips/modules. PT1, PT2, and SQT are mono-chip. |
| `--no-pt36-ts` | Disables the PT3.6 TurboSound heuristic. |
| `--max-frames N` | Limits the number of frames rendered from PT1/PT2/PT3/SQT or decoded from PSG. |

### CPC / CPC+

| Option | Description |
|--------|-------------|
| `--build-player auto\|cpc\|cpcplus\|none` | Enables or disables the player builder. In auto mode, `cpc` selects the CPC old RAM builder and `cpcplus` selects the CPC+ RAM builder. |
| `--case ...` | Selects the player output type. |
| `--player-addr 0xNNNN` | Player address, or bundle address in bundle mode. |
| `--ayt-addr 0xNNNN` | AYT runtime address in two-file mode. |
| `--loops N` | Repeat counter injected into the player. Default: 255. Does not modify the musical loop point from PT1/PT2/PT3/SQT/YM/PSG. |
| `--asic-in on\|off` | CPC+ only. Expected ASIC page state at player entry. |
| `--asic-out on\|off` | CPC+ only. Desired ASIC page state at player exit. |
| `--dma-channel 0\|1\|2` | CPC+ only. DMA channel used by the player. Default: 0. |
| `--asic-page-on 0xNN` | CPC+ only. RMR2 value used to connect the ASIC page. |
| `--asic-page-off 0xNN` | CPC+ only. RMR2 value used to disconnect the ASIC page. |
| `--program-addr 0xNNNN` | Address of the example ASM program. On CPC+, must not be within `#4000-#7FFF`. |
| `--return-addr 0xNNNN` | Return address for SP/JP modes. |
| `--no-asm` | Does not generate the ASM launcher. |
| `--no-report` | Does not generate the `.build.txt` report. |

### Debug

| Option | Description |
|--------|-------------|
| `--emit-ym` | Also writes an intermediate YM6 dump. |
| `--print-command` | Prints the equivalent full command, useful after interactive mode or to make a batch reproducible. |
