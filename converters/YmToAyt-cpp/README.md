# YM2AYT Converter

A native C++ command-line utility for converting `.YM` audio files (used by the Atari ST, Amstrad CPC, etc.) into the highly optimized custom `.AYT` format, primarily targeting resource-constrained sound chips like the **AY-3-8910/YM2149F** family.

The tool uses an advanced pattern search and an optional Genetic Algorithm (GA) optimization to find the smallest, most efficient representation of the musical data, as overlap deduplication is a difficult optimization problem.

## Table of Contents

1.  [Features](#features)
2.  [Building](#building)
3.  [Usage](#usage)
4.  [Options Reference](#options-reference)
      - [General Options](#general-options)
      - [Pattern Search & Optimization](#pattern-search--optimization)
      - [Sound Scaling Options](#sound-scaling-options)
      - [Target Architecture](#target-architecture)
      - [Genetic Algorithm Optimization (GA)](#genetic-algorithm-optimization-ga)

## Features

  * Converts standard `.YM` format files.
  * Supports multiple input files in a single run.
  * Advanced pattern matching logic with customizable search ranges.
  * Genetic Algorithm (GA) driven optimization for maximum compression.
  * Platform-specific clock rate adjustment (e.g., Amstrad CPC, Atari ST).
  * Option to scale period, noise, and envelope values.

## Building

This project is designed to be built with a standard C++ compiler (C++11 or later) and relies only on the C++ standard library for portability.
For instance, if you're using g++ (adjust flags as necessary for your system):

```bash
  g++ -std=c++17 -O2 -Wall ym_to_ayt.cpp -o ym2ayt
```

Under windows (mingw or cygwin) you might add -static option to avoid a missing DLL error at runtime:
```bash
  g++ -std=c++17 -O2 -Wall ym_to_ayt.cpp -o ym2ayt.exe -static
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



### Example 2: Genetic Algorithm Optimization

By default, a fast optimization method is applied (glutony), but if you're seeking for smaller files, you can use a better, but sower method. The recommended method is using Genetic Algorithm. Enable the GA methof  with `-o ga option`, or simply `-O2` .

```
./ym2ayt -O2 my_track.ym
```

You can control large population and extended search, althought default values provide good results

```bash
./ym2ayt -O ga --ga-pop-size 40 120 --ga-max-gen 250000 -t atari my_long_track.ym
```
The two parameters (Mu and Lambda) for population size are used for the exploration/exploitation balance:
 * mu correspond to the number of the best individuals,  selected and kept after evaluation
 * lambda corresponds to the children generated, by combining and mutating genes from their parents. Parents are randomly picked amond the mu best individuals. By defaut mu=10, lambda=40. You can try 1000, 4000 on a faster computer (and then reduce max generation as it will converge faster)

```bash
./ym2ayt -O ga --ga-pop-size 1000 4000 --ga-max-gen 2000 --ga-ext-gen 1000  -t atari my_track.ym
```


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
| | | - `auto`: Equivalent to `4:64/4`. | |
| | | - `full`: Equivalent to `1:128/1`. | |
| `-l` | `--only-evenly-looping` | Skip pattern sizes that do not loop at the beginning of a pattern (improves playback synchronization). | `false` |
| `-O` | `--overlap-optim` | Overlap optimization method amng `none`, `gutony`, `ga`, `sa`, `tabu`, `ils`  | `glutony` |
| `-O0` | | Shortcut for -O non | |
| `-O1` | | Shortcut for -O glutony | |
| `-O2` | | Shortcut for -O ga | |
| `-O3` | | Shortcut for -O ga -p full -ga-pop-size 50000 | |

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



