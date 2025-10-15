## Calling Ayt_Builder on MSX

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
		ld bc,AYT_Init		; Address for the init routine if <> 0
        ld a,2			; Number of loops for the music
		call AYT_Builder	; Build the player at DE for file pointed by IX for "A" loop

To play the music, the *player* must be called at the correct frequency.  
Most tracks require the *player* to be called periodically, typically 50 times per second.  

On MSX, the video refresh rate is often 60 Hz.  
The **AYT** file header specifies the correct period.  

It is very **important to ensure that no interrupt occurs during the player call**.  
If you are not familiar with the Z80A interrupt system, you can use the **DI** instruction before calling the *player*.

		call AYT_Player	; Execute the player to play the music

### Compilation Option
#### PlayerAccessByJP

If the **PlayerAccessByJP** option is set to 1, you must also provide the return address for the *player*.

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
		ld bc,AYT_Init		; Address for the init routine if <> 0
		ld hl,AYT_Player_Ret	; Address where the player returns
		ld a,2			; Number of times the music should play
		call Ayt_Builder

The return address is placed immediately after the *player* call:

			jp AYT_Player	; Execute the player
	AYT_Player_Ret			; Player return address

## Initialization
If the compressor detects *inactive* registers, they are excluded from the **AYT** data but still require prior initialization.

An initialization function must be called before the player.

The *Ayt_Builder* function constructs an initialization routine that must be called **before** using the *player*.  

Two options exist when calling the function:
- If **BC = 0**, *Ayt_Builder* will reserve **16 bytes** after the *player* for the routine.
- If **BC ≠ 0**, it must contain the address of a reserved **16-byte** area (anywhere in RAM) that can be recovered after initialization.

After calling *Ayt_Builder*:
- **HL** contains the address of the init routine (equal to BC if BC was non-zero)
- **DE** points to the first free byte after the *player* (or after the init routine)

**Note:**  
If the file contains no inactive registers, the init routine is unnecessary.  
In this case, the init function points to a **RET**, and the routine occupies 1 byte instead of 34.

To call an initialization routine, use:

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
		ld bc,AYT_Init		; Address for the init routine if <> 0
        ld a,2			; Number of loops for the music
		call AYT_Builder	; Build the player at DE for file pointed by IX for "A" loop

		ld (InitPlayer),hl	; Update the init routine
		...
		...
	InitPlayer equ $+1
		call 0			; After update, CALL points to the init routine

## Player Call Frequency
The *player* is usually called in sync with the screen refresh rate.  
On **MSX**, refresh is often **60 Hz**, which may make music play faster if it was composed for **50 Hz**.  
This information is available in the **AYT** file header.

**Warning:** The *player* was tested on **OPEN MSX 21.0** with the **SONY HB F1XD** and **MSX BASIC Version 2** ROM.  

To test Vsync, the following code was used.

First, pre-initialize the **VDP**:

        xor a
        out (0x99h),a
        ld a,128+15
        out (0x99h),a

Then wait for Vsync:

    Wait_Vsync
        in a,(0x99h)    ; Read VDP status
        rlca
        jr nc,Wait_Vsync

## Pre-Building the Player
It is entirely possible to ***pre-build*** the *player*.

Use *Ayt_Builder* to generate the *player* and initialize the **AYT** file in advance.  
Simply save the generated player and the updated **AYT** file after the function call.

You can then integrate both at the predefined addresses used during the *Ayt_Builder* call.

## Performance

Execution time and memory usage of the *player* depend on several factors:
- Number of active registers detected by the compressor (up to 14)
- *Calling method* (**CALL** or **JP**) across all platforms
- Configuration of **Asic page** connection/disconnection

The *calling method* defines how the *player* is invoked in Z80A.  
It is a compilation option for the *builder*:
- **CALL**: use Z80A **CALL** instruction
- **JP**: use Z80A **JP** instruction  
  - The *player* does not save **SP**, saving **11 NOPs** (CPC) or **37 T-states**  
  - Useful only if the caller modifies **SP**  
  - Otherwise:
    - The *builder* must be called each time the return address changes
      - Common during development, requiring builder in RAM
      - Or if the *player* is called from multiple locations
    - Stack pointer must be restored; any push or call corrupts **AYT** data

Performance table for 10–14 active registers and both calling methods:

| Calling Method | Active Registers | CPU (T-states) | Player Size | Builder Size |
| :-----------: | :--------------: | :------------: | :---------: | :-----------: |
| JP            | 10               | 643            | 136         | 355           |
| JP            | 11               | 692            | 142         | 355           |    
| JP            | 12               | 741            | 148         | 355           |        
| JP            | 13               | 790            | 155         | 355           |        
| JP            | 14               | 839            | 161         | 355           |  
| CALL          | 10               | 680            | 141         | 370           |
| CALL          | 11               | 729            | 147         | 370           |
| CALL          | 12               | 778            | 153         | 370           |
| CALL          | 13               | 827            | 160         | 370           |
| CALL          | 14               | 876            | 166         | 370           |
