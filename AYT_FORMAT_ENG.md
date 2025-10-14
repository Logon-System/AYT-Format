# Description of the AYT Format

An *AYT* file is composed of:
- A header.
- The data making up the patterns.
- Sequences of pointers referring to these patterns.
- An end sequence.
- Initialization data for the sound chip.

All 16-bit values are stored in memory in *little endian* format (that is, the low byte of the 16-bit value is stored at Address, and the high byte at Address+1).

## Glossary

* **Active registers:** In a YM file, not all registers are used (for example, if not all channels are active). It is necessary to send data to these registers at each frame because they can change. The number of active registers varies from 1 to 14. Register 13 is always active in this version for player management.
* **Inactive registers:** These are registers that do not change during playback. It is unnecessary to send data to them every frame or store them in the AYT file. However, they may need specific initialization values.
* **Number of frames:** This value, derived from the YM file, determines the total duration of the music. At each frame, data is sent to the **AY8910/AY8912/YM2149** registers.
* **Pattern:** A block of bytes sent to a register of the sound chip. All patterns in a given AYT file have the same size, from 1 to 256 bytes.
* **Sequence:** A sequence is a list of pointers to patterns. The collection of sequences organizes the patterns and reconstructs the entire YM music piece. Each sequence contains as many pointers as there are active registers.
* **Pattern size:** All patterns have the same size in a given AYT file. This size may range from 1 to 256 bytes, with the optimal value being the one that yields the smallest possible AYT file.
* **Number of sequences:** This number is obtained by dividing the number of frames by the pattern size.
* **End sequence:** An additional sequence that points to special values (notably for R13) marking the end of the piece and allowing the player to loop either to the start or to the sequence defined by the composer.
* **Initialization sequence:** A list of pairs (register number, value) used to set initial values before playback. This list ends with a byte whose bit 7 is set to 1.
* **Start sequence:** A pointer within the AYT file to the sequence corresponding to the beginning of the piece. Usually, this is the first sequence in the list, but it can be set to start later if desired.
* **Loop sequence:** When the end sequence is reached, the player jumps to the loop sequence to continue playing. This usually points to the first sequence but may differ (for example, skipping an intro or looping over silence).

## Header Definition
```
Ayt_Start
    struct AYT_Header
    { 
        uint8_t Ayt_Version;         // version aaaabbbb >> a.b
        uint16_t Ayt_ActiveRegs;     // active reg (bit 15:reg0...bit2:reg13) 1=active 0=inactive
        uint8_t Ayt_PatternSize;     // Pattern size 1..255
        uint16_t Ayt_FirstSeqMarker; // Offset from Ayt_Start for Seq Ptr on patterns
        uint16_t Ayt_LoopSeqMarker;  // Offset from Ayt_Start for Loop Seq Ptr 
        uint16_t Ayt_ListInit;       // Offset from Ayt_Start for Init of AY registers (*)
        uint16_t Ayt_NbPatternPtr;   // Nb of pattern ptr seq for music (NbSeq x NbReg)
        uint8_t  Ayt_PlatformFreq;   // Platform (b0..4) & Frequency (b5..7) (see table) (**)
        uint8_t  Ayt_Reserved;       // Reserved for future use (=0)
    }
```

### Ayt_Version – Version Number
**1 byte:** The 8 bits (7 to 0) are coded as: `aaaabbbb` to define version a.b.

### Ayt_ActiveRegs – Active Registers
**2 bytes:** Bits **15 to 2** of this 16-bit value represent **registers 0 to 13** of the AY/YM.  
A bit set to 1 means the register is active; 0 means inactive.  
Inactive registers have no pointers in sequences but are initialized.

### Ayt_PatternSize – Pattern Size
**1 byte:** This value, from **0 to 255**, defines the size of a pattern (the same for the whole file).

### Ayt_FirstSeqMarker – First Sequence
**2 bytes:** Contains the offset of the first sequence relative to the start of the **AYT** file.  
If the AYT file starts at #8000, and this value is #1000, then the first sequence address is #9000.  
A sequence is a group of pointers to patterns for active registers.

### Ayt_LoopSeqMarker – Loop Sequence
**2 bytes:** Contains the offset of the loop sequence relative to the start of the **AYT** file.  
If the AYT file starts at #8000 and this value is #1000, the loop sequence address is #9000.  
The loop sequence can be the same as the first sequence if looping occurs from the beginning.

### Ayt_ListInit – Initialization Data
**2 bytes:** Contains the offset of the register initialization structure relative to the start of the **AYT** file.  
If the **AYT** file starts at #8000 and this value is #1000, the initialization data is at #9000.  
Compressors generate initialization data when the number of active registers is less than 14.  
Each inactive register is initialized since its value is constant.  
The structure is as follows:

```
struct Ay_Init [N registers]
{
    uint8_t Ayt_Reg; // Register number (0–13)
    uint8_t Ayt_Val; // Initialization value
}
defb #FF             // End of Init List
```

If no register initialization is required, the AYT file will only contain 0xFF.

### Ayt_NbPatternPtr – Number of Pattern Pointers
**2 bytes:** Contains the total number of pointers to patterns in the **AYT** file, including the end sequence.  
Each sequence consists of **N pointers** to patterns, where **N = number of active registers**.  
Thus, this field contains **N × total number of sequences**.  
The number of frames equals **(total sequences – 1) × pattern size**.

### Ayt_PlatformFreq – Platform and Frequency
**1 byte:** Bits **0–4** encode the platform ID (5 bits), and bits **5–7** encode the frequency type (3 bits).  
Since the AYT format is **multi-platform**, this information enables **cross-platform transfers**.

| Bits 4..0 | Platform     | Chip Frequency (Hz) |
|:----------:|:-------------|:-------------------:|
| 00000 | AMSTRAD CPC | 1,000,000 |
| 00001 | ORIC | 1,000,000 |
| 00010 | ZXUNO | 1,750,000 |
| 00011 | PENTAGON | 1,750,000 |
| 00100 | TIMEXTS2068 | 1,764,000 |
| 00101 | ZX128 | 1,773,450 |
| 00110 | MSX | 1,789,772 |
| 00111 | ATARI ST | 2,000,000 |
| 01000 | VG5000 | 1,000,000 |
| 11111 | UNKNOWN | |

| Bits 7..5 | Player Call Rate (Hz) |
|:----------:|:---------------------:|
| 000 | 50 |
| 001 | 25 |
| 010 | 60 |
| 011 | 30 |
| 100 | 100 |
| 101 | 200 |
| 111 | UNKNOWN |

### Ayt_Reserved – Reserved
**1 byte:** Nope, not telling you what it’s for! 😏

## Pattern Definition
After the header come the *patterns*.  
A *pattern* is a group of N data bytes sent to the sound chip in N passes, where N equals *Ayt_PatternSize*.

Patterns:
- are **not** specific to an AY register (a pattern can be shared by multiple registers);
- are **not contiguous** in memory since multiple N-byte groups can compose more than two patterns.

## Sequence Definition
After the patterns come the sequences of pointers to *patterns*.  
The sequence start address is calculated from *Ayt_FirstSeqMarker*.  
A sequence consists of N pointers (N = active registers) to *patterns*.

## End Sequence Definition
Located after the last sequence, the *end sequence* marks the end of playback.  
It has two purposes:
- to indicate the end of the song via data intended for *register 13*,
- to mute sound (useful if the YM file was poorly cut).

Register 13 is special since its value should not be rewritten unless changed.  
Updating this register resets the chip’s hardware envelope, which is rarely desired.  
Thus, this register is treated specially.

YM files use the value **#FF** to mean “don’t update this register.”  
AYT files instead use only bits **6 and 7** of *register 13* to determine this:  
- As long as bits 6 and 7 are **1**, the value is not sent to the chip.  
- If bit 7 = 1 and bit 6 = 0, the player knows it’s on the final sequence.

Pattern pointers of the last sequence are special:  
- Registers **R0–R6, R8–R12** point to **0**.  
- Register **R7** points to **#3F** (mute sound channels).  
- Register **R12** contains **0x10111111**, signaling end of song.

**Note:** Depending on the compressor version, these 3 bytes are either found in patterns or (if missing) created after the end sequence.  
(*Tronic, you lazy one! 😆*)

## Initialization Structure Definition
If no register initialization is needed (for example, if 14 active registers are used), only a single byte with **#FF** follows the end sequence.  
If initialization is required, pairs of bytes indicate registers and their initialization values.

See *Ayt_ListInit* for details.

Example: if registers 3, 10, and 11 must be initialized with values 0, 1, and 2, we’ll have:

```
defb 3,0,10,1,11,2,0x0FFh
```
