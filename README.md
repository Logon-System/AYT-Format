# THE AYT FORMAT
***Tronic*** (from the *GPA* group), ***Longshot*** & ***Siko*** (from the *Logon System* group) proudly present a new audio file format called ***AYT***.
It is a compact and simple format usable by any program requiring high **CPU** performance and **cycle-accurate constant-time** operation.

This format is intended for all platforms using an ***AY-3-8910/AY-3-8912*** (General Instrument) sound processor or a compatible chip (***YM-2149*** from Yamaha). 
Several authoring tools and players have been developed and tested for the following platforms: ***CPC***, ***CPC+***, ***MSX***, ***ZX 128***, ***VG5000 (VG5210)***.

![Image Presentation CPC+](./images/AYTPRES1.jpg)

# Principle

Starting from a **YM5/YM6** audio file [^1], an ***AYT*** file is generated using a converter.  
This ***AYT*** file can then be played by an optimized player on several platforms.  
The compressor leverages the sequential nature of chiptune music to optimize memory organization.  
This organization directly influences the player, which is built according to the specific piece of music.  
The **YM** file is divided into patterns, which are arranged through pattern sequences.  
Additional optimizations, such as removing unused registers or reordering data, help reduce file size and/or **CPU** time.

[^1]: The YM format was created by Leonard (Arnaud Carré) http://leonard.oxg.free.fr/ymformat.html

# Features

- Several compression tools are available to create **AYT** files:
  - via command line on any platform,
  - or directly on the web.
- **AYT** files compress very efficiently with **ZX0** since their data is not already processed by another compression algorithm.
- A *builder* system allows the creation of a *player* at a specified address and offers several advantages:
  - once the *player* is created, the builder is no longer needed and can be completely omitted from the final program,
  - it is compact in size,
  - it generates the initialization routine for inactive registers (this routine being transparent for “old” CPC users),
  - it is not necessary to call the *builder* again when the music loops or to initialize registers.
- The *player* is built by the *builder* according to the music it will play:
  - it is highly efficient in terms of **CPU** (*see performance tables*),
  - it occupies very little memory space,
  - it requires no decompression buffer, minimizing the player’s memory footprint in RAM,
  - it handles a configurable number of loops for the music as defined in the **YM** file (looping doesn’t necessarily start at the beginning),
  - it manages sound stop at the end of the loop(s) (not all **YM** files do this),
  - it always runs in constant time (looping, muted player once finished, etc.),
  - it allows playback of music of any size, limited only by the CPU address space or the system’s memory capacity.

## Objectives
### Creating a compact and efficient format

One of the **initial goals** of this project was **to create a new audio file format combining a highly CPU-efficient player while keeping the RAM data size reasonable**.

The aim was to move away from the traditional paradigm involving general-purpose compressors that rely on large buffers and have reached their limits.  
Another approach was to think about the nature of the data beforehand to factorize it better — in other words, not to try to compress it blindly.

The method used here does not *"compress"* data harshly but relies on the particular logic of chiptune composition.  
This characteristic then allows an **AYT** file to be further compressed using tools like **ZX0** or **Shrinker** more efficiently than compressing the raw file directly.  
Statistics from converting **10,000 files** to **AYT** have proven this.  
Thus, an **AYT** file can be stored on disk or in RAM more compactly than many existing solutions.

As is often the case when pushing architectural limits, speed trades off with memory (*There is no free lunch*).  
The **CPU versus size** compromise of the **AYT** format seems quite reasonable.

Ongoing studies aim to further reduce **AYT** file size, as this format easily supports such improvements.  
Since *patterns* are not compressed, it’s entirely feasible to apply lighter compression than heavyweight general-purpose algorithms.  
The format should thus retain its advantage in **CPU** performance. :-)

#### Simplifying compression
Another **important objective** was to **simplify the creation and usage process of this format**.  
Indeed, in this field, things often turn out more complicated than announced, both for **compressors** and **players**.

When converting other formats from **YM**, users often have to enter unclear or complex options.  
Conversion from **YM** also requires accounting for differences in **sound frequencies between the source and target platforms**.  
Most existing projects are designed in *“single-platform”* mode.

While such options remain available in the **AYT** tools, the process has been greatly simplified thanks to **platform codification** (see the **AYT format** section), minimizing what’s required from the user when the program can handle it automatically.  
For instance, the user simply specifies the *target platform*, and the compressor automatically converts frequencies, adjusts sizes, and finds the optimal trade-offs.  
Indeed, frequency conversions can deactivate certain registers and significantly reduce the **AYT** file size.

The **AYT format header contains both the platform and player call frequency**, allowing conversion of **AYT** files from *platform A* to *platform B*.

The compressor exists in multiple forms and is also accessible via a **very simple web interface**, where you can drag & drop a **YM** file for conversion.  
The algorithm automatically finds the best possible compression — no manual retries are needed.

Finally, because **YM** files are sometimes roughly cut (badly terminated files that include part of a loop), another web interface was designed to **sequence, listen to, cut, and manipulate YM files**.

Perfect for making great **AY medleys**...

#### Simplifying the player
Regarding the use of an **AYT** file, the process has been simplified to the extreme.  

The concept of a *"builder"* has been adopted — a function that creates from scratch the code of the *player* based on the desired address and the music file’s address.  
This makes it possible to have a *player* whose code is perfectly adapted to the music it will play.

This concept allows both the *player* and the music to be pre-initialized without requiring the *builder* in the final code.  
To be complete, the *builder* also generates a method to initialize inactive registers according to the platform used.

For example, on a **CPC 464/664/6128**, the program tells the *builder* the address where the *player* must be created, the address of the music file, and how many times it should be played.

The *builder* then returns **the address of the first free byte after the player**, and **the number of NOPs (microseconds) the player call consumes**.  
(This information can be useful for programs requiring constant-time processes.)

When the *player* is called for the **first time**, it initializes the inactive registers.  
This process takes **exactly the same CPU time as for subsequent calls** that play the music.

# Acknowledgments
We would like to thank the following people for their enthusiasm, support, advice, and ideas throughout the development of this new format.

- **Candy (Sébastien Broudin)**, who had already been thinking about “re-patternizing” **YM** files with some promising experiments, and who closely followed our progress on the topic.  
- **Fred Crazy (Frédéric Floch)**, **Ker (Florian Bricogne)**, **Overflow (Olivier Goût)**, **DManu78 (David Manuel)**, **Cheshirecat (Claire Dupas)**, **Shap (Olivier Antoine)**  
  for their insightful technical and musical advice, and for their encouragement.  
- **BdcIron (Amaury Duran)** for his help testing the player on the VG5000 and his expertise on the ZX Spectrum.  
- **Ced (Cédric Quétier)** for the fantastic AYT logo used in the CPC+, MSX and ZX SPECTRUM player presentation.  
- **Made (Carlos Pardo)** for the gorgeous AYT logo used in the “CPC old” player presentation.  
