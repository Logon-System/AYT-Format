## Calling Ayt_Builder on VG5000 (+ VG5232 or VG5210 Sound Card)

The **VG5000** does not natively include an **AY-3-8910** or **AY-3-8912**.

Two sound cards exist to overcome this limitation.  
Specifications for these cards can be found at:

https://sites.google.com/view/vg5000-hardware/vg5k-new-hardware/carte-son-st%C3%A9r%C3%A9o/Fabrication-du-VG5210

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
        ld a,2			; Number of loops for the music
		call AYT_Builder	; Build the player at DE for file pointed by IX for "A" loop

To play the music, the *player* must be called at the correct frequency.  
Most tracks require the *player* to be called periodically, typically 50 times per second.

It is very **important to ensure no interrupts occur during the player call**.  
If you are not familiar with Z80A interrupts, use **DI** before calling the *player*.

		call AYT_Player	; Execute the player to play the music

### Compilation Option
#### PlayerAccessByJP

If the **PlayerAccessByJP** option is set to 1, you must also provide the return address of the *player*.

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
		ld hl,AYT_Player_Ret	; Address where the player returns
		ld a,2			; Number of times the music should play
		call Ayt_Builder

The return address is placed immediately after the *player* call:

			jp AYT_Player	; Execute the player
	AYT_Player_Ret			; Player return address

## Initialization
If the compressor detects *inactive* registers, they are excluded from the **AYT** data but still require prior initialization.

An initialization function must be called before the player.

*Ayt_Builder* constructs an init routine that must be called **before** using the *player*.  

After calling *Ayt_Builder*:
- **HL** contains the address of the init routine
- **DE** points to the first free byte after the *player* (or after the init routine)

**Note:**  
If no inactive registers exist, the init routine is unnecessary.  
In this case, the routine points to a **RET**, and occupies 1 byte instead of 16.

To call an initialization routine:

		ld ix,AYT_File		; Address of the AYT file
		ld de,AYT_Player	; Address where the player will be generated
        ld a,2			; Number of loops for the music
		call AYT_Builder	; Build the player at DE for file pointed by IX for "A" loop

		ld (InitPlayer),hl	; Update init routine
		...
		...
	InitPlayer equ $+1
		call 0			; After update, CALL points to the init routine

## Player Call Frequency
The *player* is usually called in sync with the screen refresh rate (50 Hz).  
This information is available in the **AYT** file header.

**Note:** The *player* was tested on real hardware because the **DCVG5K 2023.04.12** emulator does not emulate **VG5210** and **VG5232** sound cards.  
The test machine included a **32 KB RAM extension** to compensate for the limited native memory.

As on the ZX SPECTRUM 128, there is no testable **Vsync** signal.  

However, an interrupt occurs once per frame.  
In Z80A IM1 mode, the interrupt generates **RST 0x38h**, equivalent to **CALL 0x0038h**.  
Like the ZX, the ROM cannot be disconnected and begins at 0x0000, so interrupt calls occur in ROM.

Fortunately, Z80A supports relocation of interrupts via **IM2** (vectorized mode).  
This mode was rarely used on older platforms.  
To use it, a workaround must be implemented to create a vector table compatible with all situations.

Without going into too much technical detail, the method used to time *AYT_Player* calls is:

		;
		; Switch to IM2
            ; 
		ld bc,256		; 257 bytes to write
		ld a,&c9		; fill addresses from C900 to CA00 inclusive
		ld h,a			; C9 byte = RET
		ld l,c
		ld (hl),a
		ld d,h
		ld e,b
		ldir
		ld i,a			; MSB of vector table
		im 2           ; switch to interrupt mode 2

Here, Z80A **HALT**, placed after **EI**, waits for the Vsync interrupt.  
The table from **0xC900h to 0xCA00h** contains **0xC9h** bytes, which correspond to **RET**.  
Once the interrupt occurs, it cannot repeat until the next **EI**.

		di
		call AYT_Player
		ei			; Enable interrupts
		halt		; Wait for Vsync interrupt via IM2

## Pre-Building the Player
It is entirely possible to ***pre-build*** the *player*.

Use *Ayt_Builder* to generate the *player* and initialize the **AYT** file in advance.  
Simply save the generated player and updated **AYT** file after the function call.

Then integrate them at the predefined addresses used during *Ayt_Builder*.

## Performance

Execution time and memory usage of the *player* depend on:
- Number of active registers detected (max 14)
- *Calling method* (**CALL** or **JP**) across all platforms

Calling method defines how the *player* is invoked in Z80A:
- **CALL**: use Z80A **CALL**
- **JP**: use Z80A **JP**, providing a return address
  - The *player* does not save **SP**, saving **11 NOPs** (CPC) or **37 T-states**
  - Useful only if the caller modifies **SP**
  - Otherwise:
    - Builder must be called each time the return address changes
      - Common in development, requiring builder in RAM
      - Or if the *player* is called from multiple locations
    - Stack pointer must be restored; any push or call corrupts **AYT** data

Performance table for 10–14 active registers and both calling methods:

| Calling Method | Active Registers | CPU (T-states) | Player Size | Builder Size |
| :-----------: | :--------------: | :------------: | :---------: | :-----------: |
| JP            | 10               | 643            | 136         | 348           |
| JP            | 11               | 692            | 142         | 348           |    
| JP            | 12               | 741            | 148         | 348           |        
| JP            | 13               | 790            | 155         | 348           |        
| JP            | 14               | 839            | 161         | 348           |  
| CALL          | 10               | 680            | 141         | 363           |
| CALL          | 11               | 729            | 147         | 363           |
| CALL          | 12               | 778            | 153         | 363           |
| CALL          | 13               | 827            | 160         | 363           |
| CALL          | 14               | 876            | 166         | 363           |


