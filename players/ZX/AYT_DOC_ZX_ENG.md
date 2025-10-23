![Image Presentation CPC](../../images/ZX128PRES.jpg)

## Calling Ayt_Builder on ZX SPECTRUM 128/+2/+3

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
		ld bc,AYT_Init		; Address for the init routine if <> 0
        ld a,1			; Number of loops for the music
		call AYT_Builder	; Build the player at DE for file pointed by IX for "A" loop

To play the music, the *player* must be called at the correct frequency.  
Most tracks require the *player* to be called periodically, typically 50 times per second.  
The *AYT* file header specifies this period.  

It is very **important to ensure that no interrupt occurs during the player call**.  
If you are not familiar with the **Z80A** interrupt system, you can use the **DI** instruction before calling the *player*.

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
If the compressor identifies *inactive* registers, they are excluded from the **AYT** data but still require prior initialization.

An initialization function must be called before the player.

The *Ayt_Builder* function builds an initialization routine that will be called **before** using the *player*.  
Two possibilities exist when calling the function:
- If **BC = 0**, *Ayt_Builder* will reserve **16 bytes** after the *player* to create the routine.
- If **BC ≠ 0**, it must contain the address of a reserved **16-byte** area (anywhere in RAM).  
  This area can then be used by the program after initialization.

After calling *Ayt_Builder*:
- **HL** contains the address of the init routine (equal to BC if BC was non-zero).
- **DE** points to the first free byte after the *player* (or the init routine).

**Note:**  
If the file has no inactive registers, the init routine is unnecessary.  
In this case, the init function points to a **RET**, and the routine occupies only 1 byte instead of 34.

To call an initialization routine, use:

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
		ld bc,AYT_Init		; Address for the init routine if <> 0
        ld a,2			; Number of loops for the music
		call AYT_Builder	; Build the player at DE for file pointed by IX

		ld (InitPlayer),hl	; Update the init routine
		...
		...
	InitPlayer equ $+1
		call 0			; After update, CALL will point to the init routine

## Player Call Frequency
The *player* is usually called in sync with the screen refresh rate, 50 Hz.  
This information is available in the **AYT** file header.  

**Warning:** The player was tested on the **ZX SPIN 0.666** emulator, which has a built-in assembler but was somewhat buggy on my machine.

There is no testable Vsync signal on the ZX SPECTRUM 128.  
However, an interrupt occurs once per frame.  
In Z80A IM1 mode, an interrupt generates **RST 0x38h** (equivalent to **CALL 0x0038h**).  
Since the ROM cannot be disconnected and starts at 0x0000, interrupts are executed in ROM, which does not provide a RAM handler.

Fortunately, Z80A allows interrupts to be vectored via IM2.  
This mode was rarely used in older platforms. To use it, a small workaround is required to create a compatible vector table in RAM.

Without going into excessive technical detail, here’s how *AYT_Player* calls are synchronized:

		;
		; Switch to IM2
        ;
		ld bc,256		; 257 bytes to write
		ld a,&c9		; between C900 and CA00 inclusive
		ld h,a			; fill with C9 = RET
		ld l,c
		ld (hl),a
		ld d,h
		ld e,b
		ldir
		ld i,a			; MSB of vector table
		im 2           ; switch to interrupt mode 2

Now, the Z80A **HALT** instruction, placed after EI, waits for the Vsync interrupt.  
The vector table from **0xC900h to 0xCA00h** contains **0xC9h**, the Z80A **RET** instruction.  
Once an interrupt occurs, it cannot repeat until the next **EI**.

		di
		call AYT_Player
		ei			; Enable interrupts
		halt		; Wait for interrupt via IM2

## Pre-Building the Player
It is entirely possible to ***pre-build*** the *player*.

Use *Ayt_Builder* to generate the *player* and initialize the **AYT** file in advance.  
Simply save the generated player and the updated **AYT** file after the function call.

You can then integrate both at the predefined addresses used during the *Ayt_Builder* call.

## Performance

Execution time and memory usage of the *player* depend on several factors:
- Number of active registers detected by the compressor (up to 14)
- *Calling method* (**CALL** or **JP**) across all platforms

The *calling method* defines how the *player* is invoked in Z80A.  
It is a compilation option for the *builder*:
- **CALL**: Use Z80A **CALL** instruction to invoke the *player*.
- **JP**: Use Z80A **JP** instruction.  
  - The *player* does not save **SP**, saving **11 NOPs** (CPC) or **37 T-states**.  
  - Useful only if the calling program modifies **SP**.  
  - Otherwise:
    - The *builder* must be called each time the return address changes:
      - Common in development, requiring the builder in RAM
      - Or if the *player* is called from multiple locations
    - Stack pointer must be restored; any push or call corrupts **AYT** data

Performance table for 10–14 active registers and both calling methods:

| Calling Method | Active Registers | CPU (T-states) | Player Size | Builder Size |
| :-----------: | :--------------: | :------------: | :---------: | :-----------: |
| JP            | 10               | 685            | 148         | 338           |
| JP            | 11               | 738            | 155         | 338           |    
| JP            | 12               | 791            | 162         | 338           |        
| JP            | 13               | 844            | 169         | 338           |        
| JP            | 14               | 897            | 176         | 338           |  
| CALL          | 10               | 722            | 153         | 353           |
| CALL          | 11               | 775            | 160         | 353           |
| CALL          | 12               | 828            | 167         | 353           |
| CALL          | 13               | 881            | 174         | 353           |
| CALL          | 14               | 934            | 181         | 353           |


