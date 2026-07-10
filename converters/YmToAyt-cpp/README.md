# YM2AYT Converter

A native C++ command-line utility for converting `.YM` audio files (used by the Atari ST, Amstrad CPC, etc.) into the highly optimized custom `.AYT` format, primarily targeting resource-constrained sound chips like the **AY-3-8910/YM2149F** family.

The tool uses an advanced pattern search and an optional Genetic Algorithm (GA) optimization to find the smallest, most efficient representation of the musical data, as overlap deduplication is a difficult optimization problem.

## Table of Contents

1.  [Features](#features)
2.  [Building](#building)
3.  [Usage](#usage)
4.  [Optimizing for Compressed Size (compress-sweep.sh)](#optimizing-for-compressed-size-compress-sweepsh)
5.  [Options Reference](#options-reference)
      - [General Options](#general-options)
      - [Pattern Search & Optimization](#pattern-search--optimization)
      - [Sound Scaling Options](#sound-scaling-options)
      - [Target Architecture](#target-architecture)
      - [Genetic Algorithm Optimization (GA)](#genetic-algorithm-optimization-ga)

## Features

  * Converts standard `.YM` format files.
  * Supports multiple input files in a single run.
  * Advanced pattern matching logic.
  * Meta heuristics optimization (Simulated anhealing, genetic algorithms) for maximum compression.
  * Platform-specific clock rate adjustment (e.g., Amstrad CPC, Atari ST).
  * Option to scale period, noise, and envelope values.

## Building

This project is designed to be built with a standard C++ compiler (C++11 or later) and relies only on the C++ standard library for portability.
For instance, if you're using g++ (adjust flags as necessary for your system):

```bash
  g++ -std=c++17 -O2 -Wall *.cpp -o ym2ayt
```

Under windows (mingw or cygwin) you might add -static option to avoid a missing DLL error at runtime:
```bash
  g++ -std=c++17 -O2 -Wall *.cpp -o ym2ayt.exe -static
```

Additionally you can strip the binary :
```bash
  strip ym2ayt.exe
```

## Usage

The basic syntax is to provide the program name followed by any options and one or more input `.YM` files.

```bash
./ym2ayt [OPTIONS] YM-FILE [YM-FILE...]
```

### Example 1: Basic Conversion for Amstrad CPC

Convert a track, setting the target to the Amstrad CPC's clock speed, and save the result as `my_tune.ayt`.

```bash
./ym2ayt -t cpc my_tune.ym
```

You can specify multiple files, or use wildcards

```bash
./ym2ayt -t cpc *.ym
```

### Example 2: Advanced Optimization

By default, a fast optimization method is applied (greedy), but if you're seeking for smaller files, you can use a better, but slower method. The recommended method is using Simulated Anhealing or Genetic Algorithm. Enable the GA method with `-O ga` option. Or you can simply use `-O2` option shortcut

However, if you're compressing audio data (using ZX0 for instance), you might get smaller files by *not* using a slow optimization method, and only using greedy. 

```
./ym2ayt -O2 my_track.ym
```

You can control large population and extended search, althought default values provide good results

```bash
./ym2ayt -O ga --ga-pop-size 40 120 --ga-gen-max 250000 -t atari my_long_track.ym
```
The two parameters (Mu and Lambda) for population size are used for the exploration/exploitation balance:
 * mu correspond to the number of the best individuals,  selected and kept after evaluation
 * lambda corresponds to the children generated, by combining and mutating genes from their parents. Parents are randomly picked amond the mu best individuals. By defaut mu=10, lambda=40. You can try 1000, 4000 on a faster computer (and then reduce max generation as it will converge faster)

```bash
./ym2ayt -O ga --ga-pop-size 1000 4000 --ga-gen-max 2000 --ga-gen-ext 1000  -t atari my_track.ym
```
## Masking 

By default, an additional method is enabled. It results in sending random data when register values can be ignored. It provides better results, at it reduces the number of unique sequences. If works usually well, but if you hear sound artifacts (due to internal PSG counter not reset), you can force using 0s instead of random values with `-Oz` or `--norm-patterns`. Final files will be slightly bigger, but the songs should play without any artifacts. You also can completly force using strict deduplication method only  with `--disable-pattern-masking` 

## Optimizing for Compressed Size (compress-sweep.sh)

When the `.AYT` is going to be **crunched** by a Z80 packer (ZX0, Shrinkler, …)
before being embedded in a final binary — typically a **size-restricted
production such as a 4K demo** — the goal is *not* the smallest raw `.AYT`, but
the smallest file **after compression**. These two are not the same: a slightly
looser `.AYT` heap often exposes more long-range redundancy for the cruncher, so
it can compress *smaller* than a more tightly packed one.

Because the `.AYT` is read through pointers (random access into the heap), it is
fully decompressed into RAM once at load time. Decompression speed therefore
matters little here — ratio is what counts — which is why a heavier cruncher like
Shrinkler is a perfectly reasonable, deliberate choice despite its slower,
larger decompressor.

`compress-sweep.sh` explores this trade-off **without modifying the converter**:
it just invokes it repeatedly. For each input file it sweeps
`pattern size × optimization stage × masking × normalize`, compresses every
candidate with a compressor you provide, and ranks the results by compressed
size. It works on one or many `.ym` files.

### Key finding: pattern size is the dominant lever

Across tunes, **the pattern size is by far the most important parameter** for the
final compressed size, and the best size *varies per tune* (hence the per-file
sweep). By contrast, the heavy metaheuristics (`ga`, `sa`, `ils`, `tabu`) bring
almost nothing once the data is compressed: **`greedy` is enough**. For fast
sweeps, restrict the stages to `--stages "none greedy"`.

### Invocation

Two ready-made presets point at the Linux binaries in `compressors/`:

```bash
# ZX0 — tiny, fast, buffer-free decompressor (great default)
./compress-sweep.sh --preset zx0 --stages "none greedy" yms/*.ym

# Shrinkler — best ratio, slower/larger decompressor (chosen on purpose)
./compress-sweep.sh --preset shrinkler --stages "none greedy" yms/*.ym
```

Or pass any compressor via a command template (`{in}` = candidate `.ayt`,
`{out}` = compressed output):

```bash
./compress-sweep.sh --compressor './compressors/zx0.exe -f {in} {out}' yms/my_tune.ym
```

Useful axes: `--sizes MIN:MAX:STEP` (default `4:64:4`, use `1:128:1` for a full
scan), `--stages "..."`, `--masking off|on|both`, `--normalize off|on|both`,
`--seed N`, `--keep` (save the best `.ayt` per file), `-v` (show every candidate).

### What you get

A per-file recap comparing the compressed-aware optimum against the naive
"smallest raw `.AYT`" choice, plus a global ranking and a full CSV
(`compress-sweep.csv`). Example (ZX0, `--stages "none greedy"`):

```
fichier      b.size b.stage  comp*  | comp@raw  gain%
an_base.ym   12     greedy   3338   | 3503      +4.7
intro.ym     12     greedy   1543   | 1739      +11.3
logon.ym     16     greedy   2295   | 2295      +0.0
TOTAL                        7176   | 7537      +4.8
```

`comp*` is the smallest compressed size found; `comp@raw` is what you would have
shipped by naively minimizing the raw `.AYT`. Here, picking parameters by
compressed size saves ~5% overall (up to +11% on a single tune) — and note the
winning pattern size differs between tunes (12 vs 16).

## Options Reference

| Option (Short) | Option (Long) | Description | Default |
| :--- | :--- | :--- | :--- |
| `-h` | `--help` | Show the help message and exit. | N/A |
| `-v` | `--verbose` | Increase verbosity level. | `1` |
| `-q` | `--quiet` | Set verbosity to the lowest level (0). | N/A |

### Pattern Search & Optimization

| Option (Short) | Option (Long) | Description | Default |
| :--- | :--- | :--- | :--- |
| `-p` | `--pattern-size` | Define the pattern search range. Accepts several formats: | `4:64/4` (`auto`) |
| | | - `N`: Set minimum and maximum size to $N$ (`N:N/1`). | |
| | | - `N:M`: Set size range from $N$ to $M$, with step 1 (`N:M/1`). | |
| | | - `N:M/S`: Set size range from $N$ to $M$, with step $S$. | |
| | | - `auto`: Equivalent to `4:96/4`. | |
| | | - `full`: Equivalent to `1:128/1`. | |
| `-l` | `--only-evenly-looping` | Skip pattern sizes that do not loop at the beginning of a pattern (improves playback synchronization). | `false` |
| `-O` | `--overlap-optim` | Overlap optimization method amng `none`, `gutony`, `ga`, `sa`, `tabu`, `ils`  | `greedy` |
| `-O0` | | Shortcut for -O none | |
| `-O1` | | Shortcut for -O greedy -Om | |
| `-O2` | | Shortcut for -O sa -Om | |
| `-O3` | | Shortcut for -O ga -Om -p full -ga-pop-size 50000 | |
| `-Om` | `--pattern-masking`| Enables pattern masking | |
| `-Oz` | `--norm-patterns`| Enables normlizing pattern (usng 0s where ata can be masked) | |


### Sound Scaling Options

These options allow you to apply a constant multiplier $F > 0$ to specific register values before pattern search. This is useful for adjusting the sound characteristics to better suit the target platform.

| Option (Short) | Option (Long) | Description | Default |
| :--- | :--- | :--- | :--- |
| `-sp` | `--scale-period F` | Multiply tone periods (R0-R5) by factor $F$. | `-1.0` (Disabled) |
| `-sn` | `--scale-noise F` | Multiply noise period (R6) by factor $F$. | `-1.0` (Disabled) |
| `-sv` | `--scale-envelope F` | Multiply envelope period (R11-R12) by factor $F$. | `-1.0` (Disabled) |

### Target Architecture

| Option (Short) | Option (Long) | Description | Default |
| :--- | :--- | :--- | :--- |
| `-t` | `--target ARCH` | Name of the target architecture to set the clock rate and model ID. | `cpc` |
| | | *Available values include:* `cpc`, `atari`,  | |
| `-o` | `--output FILE` | Specify the name of the output `.AYT` file. | (Input filename + `.ayt`) |
| `-P` | `--output-path PATH` | Specify the folder where to store results. | `""` (Current directory) |
| `-s` | `--save` | Save intermediary files (e.g., pre-processed data). | `false` |
| `-c` | `--csv` | Export statistics in CSV format alongside the `.AYT` file. | `false` |

### Genetic Algorithm Optimization (GA)

The GA is an advanced compression mode, enabled using `-O ga` option. These options control the evolution process.

| Option (Long) | Arguments | Description | Default |
| :--- | :--- | :--- | :--- |
| `--ga-pop-size` | `M L` | Define the population sizes: $\mu$ (parents) and $\lambda$ (children). | `10 40` |
| `--ga-gen-min` | `N` | Minimum number of generations to run. | `50000` |
| `--ga-gen-max` | `N` | Absolute maximum number of generations to run. | `100000` |
| `--ga-gen-ext`| `N` | When a new best solution is found, extend the max generation count by $N$ to search further in the current solution's region. | `50000` |



