![Image Presentation CPC+](../../images/AYTPRES1.jpg)
## Calling Ayt_Builder on CPC 464+/6128+/GX4000

On Amstrad **"CPC+"** machines, there is an additional parameter related to the technical specifics of these models, which include **DMAs** that allow updating of **AY** registers *almost* automatically.

    ld ix,AYT_File       ; AYT_File is the address where the AYT file is located
    ld de,AYT_Player     ; AYT_Player is the address where the player will be built
    ld bc,#0101          ; State of the ASIC page on entry (B) and on exit (C) (0=off/1=on)
    ld a,2               ; A indicates how many times the music will be played
    call Ayt_Builder

There are some constraints inherent to the space occupied by the special I/O page of the �Plus,� referred to in this document as the **Asic page**.

The **Asic page**, which allows access to the **DMA** configuration, is located between **0x4000 and 0x7FFF**.  
When it is connected, it hides the RAM located in that range.  
- The address of *AYT_Player* must **never** be defined between **0x4000 and 0x7FFF**, as it needs to write into the **Asic page** to function.  
- The **AYT** file can be placed anywhere in RAM. The *player* will handle connecting or disconnecting the **Asic page** as needed.

### Parameter BC: Asic Page State
The BC register passed to *Ayt_Builder* indicates the state of the Asic page when calling the *player* and the desired state upon exiting the *player*.

There are **several possible configurations** when working with the Asic page.

For example:
- You might call the player while the Asic page is already connected.  
- You might want the player to disconnect the Asic page upon exit.

The player will therefore:
- Disconnect the Asic page if it is connected on entry **and** the **AYT** file overlaps the area used by that page.  
- Connect the Asic page to update the DMA control registers.  
- Connect or disconnect the Asic page according to the specified configuration.

Each of these actions may take a few microseconds, so the context is important to ensure optimal CPU performance.  
For instance, two opposite scenarios can occur:

- The Asic page is always connected, and the **AYT** file is not located between **0x4000 and 0x7FFF**.  
  In this case, both B and C should be set to #01.  
  Since the file is not in the Asic page, the player will not disconnect it, nor will it need to connect it to configure the DMA.  
  It also won�t need to reconnect it since it is already active.  
  This configuration saves **20 NOPs** and **14 bytes of RAM**.

- The Asic page is connected on entry, and the **AYT** file is located between **0x4000 and 0x7FFF**, and must be disconnected on exit.  
  Here, B should be #01 and C should be #00.  
  Since the **AYT** file is in the Asic page, the player must disconnect the Asic to access the data, then reconnect it to configure the DMA, and finally disconnect it again before returning.  
  In this configuration, the player consumes **20 NOPs** and **14 bytes of RAM** more than in the previous one.

The following table summarizes all possible configurations.  
It is therefore advisable to avoid placing the file in the Asic zone if the Asic page is open on entry.

| AYT File between 0x4000�0x7FFF | Asic State In (B) | Asic State Out (C) | CPU (Nops) | Extra Bytes |
| :-----------------------------: | :---------------: | :----------------: | :---------: | :-----------: |
| YES | ON (B=#01) | ON (C=#01) | +14 | +8 |
| YES | ON (B=#01) | OFF (C=#00) | +20 | +14 |
| YES | OFF (B=#00) | ON (C=#01) | +7 | +5 |
| YES | OFF (B=#00) | OFF (C=#00) | +13 | +9 |
| NO | ON (B=#01) | ON (C=#01) | +0 :-) | +0 |
| NO | ON (B=#01) | OFF (C=#00) | +7 | +5 |
| NO | OFF (B=#00) | ON (C=#01) | +7 | +5 |
| NO | OFF (B=#00) | OFF (C=#00) | +13 | +9 |

---

### Compilation Options
#### PlayerAccessByJP
As with the *player* on older CPC models, it is possible to define the **PlayerAccessByJP** option, which requires the return address of the *player* to be stored in the HL register when calling *Ayt_Builder*.  
This saves *11 microseconds* if the calling program already modifies the stack pointer itself.  
If your program does not do so, this option should remain set to 0 to avoid having to manually save and restore the stack pointer (otherwise, pushing an address could overwrite **AYT** data).

#### PlayerDMAUsed_SAR / PlayerDMAUsed_DCSRMask
There are two other compilation options specific to the �Plus� hardware capabilities:  
- **PlayerDMAUsed_SAR** defines which DMA channel (among the 3 available) will be used by the *player*.  
  - 3 constants are predefined in the source: **AYT_Asic_SAR0**, **AYT_Asic_SAR1**, **AYT_Asic_SAR2**.  
  - By default, DMA channel 0 is used: **PlayerDMAUsed_SAR equ AYT_Asic_SAR0**.  
  - To change it, if already used elsewhere, simply modify the number in the definition.  
- **PlayerDMAUsed_DCSRMask** defines the DMA mask used by the *player* so it can coexist with other DMA channels without interference.  
  - 3 constants are predefined: **AYT_Asic_DCSRM0**, **AYT_Asic_DCSRM1**, **AYT_Asic_DCSRM2**.  
  - This parameter must match the same DMA channel defined by **PlayerDMAUsed_SAR**.  
  - By default, DMA channel 0 is used: **PlayerDMAUsed_DCSRMask equ AYT_Asic_DCSRM0**.

#### Ayt_AsicPage_On / Ayt_AsicPage_Off
Connecting the **Asic page** involves a "gate array" register called **RMR2**.  
This register is multifunctional and also allows selecting one of the first 8 cartridge ROMs as the �lower ROM� at address #0000, #4000, or #8000.  

The folks at Amstrad, in their infinite wisdom, didn�t think it necessary to make this register readable.  
As a result, updating it requires knowing its previous state, which depends on each program�s configuration.  

The *player* currently does **not** support dynamic management of varying lower-ROM configurations.  
If the *player* is always called from a single configuration, the equivalences **Ayt_AsicPage_On** and **Ayt_AsicPage_Off** can be used to define that setup.  

By default, the most common codes are used � **#A0** for disconnection and **#B8** for connection.  
These default values assume that the lower ROM number is 0 and that its mapping is at #0000 when the **Asic page** is disconnected.  

Users who need the player to restore a different ROM number and/or mapping (other than #0000) can adjust these equivalences based on the **RMR2** description table below:

| Bits 7.6.5 | Bits 4.3 | Bits 2.1.0 | Values | Asic Page | Lower ROM |
| :-----------: | :--------------: | :---------: | :-----------: | :------------: | :-----------: |
| 1.0.1  | 0.0   | 0.0.0 to 1.1.1  | **#A0** to #A7  | OFF           | 0 to 7 at #0000 |
| 1.0.1  | 0.1   | 0.0.0 to 1.1.1  | #A8 to #AF      | OFF           | 0 to 7 at #4000 |
| 1.0.1  | 1.0   | 0.0.0 to 1.1.1  | #B0 to #B7      | OFF           | 0 to 7 at #8000 |
| 1.0.1  | 1.1   | 0.0.0 to 1.1.1  | **#B8** to #BF  | ON            | 0 to 7 at #0000 |

#### Player in ROM
Currently, the **player** is designed for use in **RAM**.  
It relies on *self-modifying code* to manage its internal variables.  
This mechanism is used for updating the *sequence pointer* and *pattern counter*, and also the Z80A **SP** register when the *player* is called using the �CALL� method (see the **PlayerAccessByJP** option).  

If you plan to use the generated player in a **ROM**, modifying the *builder* to support this is not too complex.  

However, a ROM-compatible version will be released if there is sufficient demand.  
Note that this version will lose between **6 and 9 NOPs** of CPU time (depending on **PlayerAccessByJP equ 0**) and will require reserving **3 to 5 bytes** of RAM to store its variables.

### Initialization
If the compressor identifies certain *inactive* registers, they are excluded from the **AYT** data but still require prior initialization.

Since the �Plus� player is much faster than on earlier CPC models, its execution time can become shorter than the time required for the initialization routine (which does not use **DMA**).  
Therefore, an initialization function must be called before invoking the *player*.

The *Ayt_Builder* function builds an initialization routine that must be called **before** using the *player*.

Upon exiting *Ayt_Builder*:
- The **HL** register contains the address of the initialization routine.  
- The **DE** register contains the pointer to the first free byte after the *player* (or initialization routine).

**Note:**  
If the file contains no inactive registers, the initialization routine becomes unnecessary.  
In that case, the initialization function will simply point to a `RET` (occupying 1 byte instead of 34 in the player).

Here�s an example of how to call the initialization routine:

    ld ix,AYT_File       ; AYT_File is the address where the AYT file is located
    ld de,AYT_Player     ; AYT_Player is the address where the player will be built
    ld a,2               ; A indicates how many times the music will be played
    ld bc,#aabb          ; Asic page configuration on entry and exit
    call Ayt_Builder
    ld (InitPlayer),hl   ; Update initialization routine address
    ...
    ...

### Ayt_Player Call Frequency

To play the music, the *player* must be called at the required frequency.  
Most music tracks require the *player* to be called periodically **50 times per second**.  
The **AYT** file header specifies this playback period.  

It is very **important to ensure that no interrupt can occur during the player call**.  

If you are not familiar with the **Z80A** interrupt system, you can use the **DI** instruction before calling the *player*.

		call AYT_Player	; Play the music

The call frequency of the *player* is generally based on the screen refresh rate, which is **50 Hertz**.  
This information is available in the **AYT** file header (see the **AYT** format description).  

On **CPC+**, the 50 Hz frequency is generated by the �asic� **CRTC 6845** configuration, which sends its **VSYNC signal** through the �asic� **PPI 8255**.  
It is possible to test the VSYNC signal using the following code:

           ld b,#f5        ; Port B of the PPI 8255
    Wait_Vsync
            rra             ; Test bit 0
            jr nc,Wait_Vsync     ; Wait until bit 0 becomes 1

Interrupts on CPC systems also make it possible to synchronize playback with very high precision.

---

### Performance

Execution time and memory usage performance of the player depend on several factors:
- The number of active registers detected by the compressor (up to 14).  
- The *call method* (**CALL** or **JP**) used on all platforms.  
- The *connect/disconnect configuration* of the **Asic page** discussed earlier.

The *call method* refers to how the *player* is invoked in Z80A code.  
This method is a *builder* compilation option.  
- When the call method is **CALL**, the program invokes the player using the Z80A **CALL** instruction.  
- When the call method is **JP**, the *player* must be called using the Z80A **JP** instruction. In this case, the programmer must provide the return address to the *builder*.  
  - The *player* then skips saving the **SP** register, saving **11 NOPs** (on CPC) or **37 T-states**.  
  - This is useful only if the calling program modifies **SP** anyway.  
  - Otherwise, it introduces issues:
    - You must rebuild the *player* every time the return address changes (common during development or when called from multiple places).  
    - You must restore the stack pointer since any `PUSH` or `CALL` could corrupt **AYT** data.

The table below shows *player* performance between 10 and 14 active registers for both call methods.

| Call Method | Asic Config | Active Registers | CPU (Nops) | Player Size (bytes) | Builder Size (bytes) |
| :-----------: | :-------------: | :--------------: | :---------: | :-----------: | :------------: |
| JP | OPTIMAL | 10 | 178 | 188 | 596 |
| JP | OPTIMAL | 11 | 190 | 196 | 596 |
| JP | OPTIMAL | 12 | 202 | 204 | 596 |
| JP | OPTIMAL | 13 | 214 | 212 | 596 |
| JP | OPTIMAL | 14 | 226 | 220 | 596 |
| CALL | NON OPTIMAL | 10 | 196 | 198 | 612 |
| CALL | NON OPTIMAL | 11 | 208 | 206 | 612 |
| CALL | NON OPTIMAL | 12 | 220 | 214 | 612 |
| CALL | NON OPTIMAL | 13 | 232 | 222 | 612 |
| CALL | NON OPTIMAL | 14 | 244 | 230 | 612 |
