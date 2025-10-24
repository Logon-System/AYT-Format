;;**********************************************************************************************************************************************
;; ZX SPECTRUM WITH AY-3-8912 (128, +2, +3, ...)
;;**********************************************************************************************************************************************
;; EXAMPLE / SAMPLE
;; NOTE :
;; I adapted my source to remove directives that the ZXSPIN emulator does not know (and I do not have the documentation for this assembler, 
;; which I have not found). This emulator and especially its assembler/debugger are buggy and it is not uncommon for it to go into an unrecoverable 
;; loop of "memory access violation". Maybe an OS problem... 
;; The memory page between C000-FFFF where the code is assembled cannot be selected. 
;; The assembler seems to select page 7 without anyone knowing exactly why, and does not assemble after &C000, whether I am in ZX 128 or +2 emulation. 
;; The debugger menu on page management is just indigestible, incomprehensible and buggy, limiting disassembly to one page, without the screen being refreshed... (!!).
;;
;; If you are used to ZX, the code must be able to compile over a 16k page boundary. 
;; I invite you to take the separate Builder version (which comes from the Winape assembler on Cpc) where the compilation directives are present because 
;; I had to merge the builder with the example to be able to assemble it (read "file.asm" is not known). 
;; There are probably minor adjustments to be made (& instead of # for hexadecimal values). 
;; Because of this assembly problem, the music is not complete, but the example shows 95% of the music playing stably with a raster that indicates the execution length.
;;
;; If you make a version that compiles on most assemblers for ZX, please let me know so that it can benefit the rest of the ZX community.
;; Longshot / Logon System @ logon.system@free.fr
;;**********************************************************************************************************************************************
		org 32768
Start_Example
MAIN
		di
		ld sp,mapile
		;
		;-----------------------------------------------------------------------------------------------
		; Swith to Im2
		;-----------------------------------------------------------------------------------------------
		ld bc,256		; 257 bytes to write
		ld a,&c9		; between C900 and CA00 included
		ld h,a			; with C9 byte = RET
		ld l,c
		ld (hl),a
		ld d,h
		ld e,b
		ldir
		ld i,a			; Msb of vector tab
		im 2


		; ld bc,&7ffd
		; ld a,7	; Wtf
		; out (c),a
		;
		;-----------------------------------------------------------------------------------------------
		; Build the player routine (needs 338 or 353 bytes) + 19 bytes (init player) 
		;-----------------------------------------------------------------------------------------------
		;
		ld ix,AYT_File		; Ptr on AYT_File
		ld de,AYT_Player	; Ptr of Adress where Player is built
		ld a,1			; Nb of loop for the music
		call AYT_Builder	; Build the player at &<d>00 for file pointed by <ix> for <a> loop
		ld (InitRegAy),hl
InitRegAy	equ $+1
		call 0			; Init Ay reg
		;
		;
		;-----------------------------------------------------------------------------------------------
		; Main Code Playing Music
		;-----------------------------------------------------------------------------------------------
MainLoop	
		;-----------------------------------------------------------
		; Magenta border
		;-----------------------------------------------------------
		ld a,3
		out (&fe),a   	; select border color
		;
		;-----------------------------------------------------------
		; Calling player (JP or CALL method
		;-----------------------------------------------------------
		;
		di
		call AYT_Player
		;
		;-----------------------------------------------------------
		; Black border
		;-----------------------------------------------------------
		xor a
		out (&fe),a
		;
		;-----------------------------------------------------------
		; Waiting Vsync
		;-----------------------------------------------------------
		ei			; Int authorized
		halt			; wait for irq on vsync via Im2
		; 
		;-----------------------------------------------------------
		; Little delay to let electron beam to be visible
		;-----------------------------------------------------------
		ld bc,250		; 10+ (bc x 26 Ts)
wait_border
		dec bc
		ld a,b
		or c
		jr nz,wait_border
		;
		jr MainLoop
;;------------------------------------------------------------------------------------------------------------
;; Stack
		ds 20
mapile
;;============================================================================================================================================ 
AYT_Player
	  	ds 353,&FF
		
AYT_Builder
;;============================================================================================================================================ 
;;              _____  __     _____  _____  _____    _____  _____  _____  _____    _____  __     _____  __ __  _____  _____
;;             |  |  ||  |   |_   _|| __  ||  _  |  |   __||  _  ||   __||_   _|  |  _  ||  |   |  _  ||  |  ||   __|| __  |
;;             |  |  ||  |__   | |  |    -||     |  |   __||     ||__   |  | |    |   __||  |__ |     ||_   _||   __||    -|
;;             |_____||_____|  |_|  |__|__||__|__|  |__|   |__|__||_____|  |_|    |__|   |_____||__|__|  |_|  |_____||__|__|
;;
;;                                                                                            ,,
;;                                 db      `YMM'   `MM'    MMP""MM""YMM                      *MM
;;                                ;MM:       VMA   ,V      P'   MM   `7                       MM
;;                               ,V^MM.       VMA ,V            MM      `7MM  `7MM  `7Mb,od8  MM,dMMb.   ,pW"Wq.
;;                              ,M  `MM        VMMP             MM        MM    MM    MM' "'  MM    `Mb 6W'   `Wb
;;                              AbmmmqMA        MM              MM        MM    MM    MM      MM     M8 8M     M8
;;                             A'     VML       MM              MM        MM    MM    MM      MM.   ,M9 YA.   ,A9
;;                           .AMA.   .AMMA.   .JMML.          .JMML.      `Mbod"YML..JMML.    P^YbmdP'   `Ybmd9'
;;
;;
;;============================================================================================================================================
;; 
;;                                                              _ (`-.         ('-.
;;                                                             ( (OO  )       ( OO ).-.
;;                                            ,----.          _.`     \       / . --. /
;;                                           '  .-./-')      (__...--''       | \-.  \
;;                                           |  |_( O- )      |  /  | |     .-'-'  |  |
;;                                           |  | .--, \      |  |_.' |      \| |_.'  |
;;                                          (|  | '. (_/      |  .___.'       |  .-.  |
;;                                           |  '--'  |  .-.  |  |      .-.   |  | |  | .-.
;;                                            `------'   `-'  `--'      `-'   `--' `--' `-'
;;
;;
;;                                                         .-') _      .-')                   .-')     .-') _       ('-.    _   .-')
;;                                                        ( OO ) )    ( OO ).                ( OO ).  (  OO) )    _(  OO)  ( '.( OO )_
;;   ,--.       .-'),-----.   ,----.      .-'),-----. ,--./ ,--,'    (_)---\_)   ,--.   ,--.(_)---\_) /     '._  (,------.  ,--.   ,--.)
;;   |  |.-')  ( OO'  .-.  ' '  .-./-')  ( OO'  .-.  '|   \ |  |\    /    _ |     \  `.'  / /    _ |  |'--...__)  |  .---'  |   `.'   |
;;   |  | OO ) /   |  | |  | |  |_( O- ) /   |  | |  ||    \|  | )   \  :` `.   .-')     /  \  :` `.  '--.  .--'  |  |      |         |
;;   |  |`-' | \_) |  |\|  | |  | .--, \ \_) |  |\|  ||  .     |/     '..`''.) (OO  \   /    '..`''.)    |  |    (|  '--.   |  |'.'|  |
;;   |  '---.'   \ |  | |  |(|  | '. (_/   \ |  | |  ||  |\    |     .-._)   \  |   /  /\_  .-._)   \    |  |     |  .--'   |  |   |  |
;;   |      |     `'  '-'  ' |  '--'  |     `'  '-'  '|  | \   |     \       /  `-./  /.__) \       /    |  |     |  `---.  |  |   |  |
;;   `------'       `-----'   `------'        `-----' `--'  `--'      `-----'     `--'       `-----'     `--'     `------'  `--'   `--'
;;
;;============================================================================================================================================
;; AY Turbo Player for ZX SPECTRUM 128 
;; AYT Player is generated by AYT Builder
;; CONSTANT CPU Execution, NO buffer, NO unpacking, LOOP everywhere, NO Address limitations,  NO ROM Flipping, TINY Size
;; AYT SeqMarker
;;
;; 30th October 2025
;; Credits 
;; Delphi original AYT SeqMarker   
;; Web YM to AYT converter       : Tronic (GPA)
;; C Unix/Windows AYT converter  : Siko (Logon System) (Stephane Sikora @ https://www.sikorama.fr)
;; ZX AYT Z80A player/Builder    : Longshot (Logon System) (Serge Querne @ logon.system@free.fr)
;;
;;============================================================================================================================================
;; Performances
;; ------------
;; Option PlayerAccessByJP equ 0	; CALL player
;;     10 registers : 722 Ts (call included) / Player Size: 153 bytes (*)
;;     11 registers : 775 Ts (call included) / Player Size: 160 bytes (*)
;;     12 registers : 828 Ts (call included) / Player Size: 167 bytes (*)
;;     13 registers : 881 Ts (call included) / Player Size: 174 bytes (*)
;;     14 registers : 934 Ts (call included) / Player Size: 181 bytes
;;     Size of Ayt_Builder : 353 bytes
;; Option PlayerAccessByJP equ 1	; JP player
;;     10 registers : 685 Ts (jp included) / Player Size: 148 bytes (*)
;;     11 registers : 738 Ts (jp included) / Player Size: 155 bytes (*)
;;     12 registers : 791 Ts (jp included) / Player Size: 162 bytes (*)
;;     13 registers : 844 Ts (jp included) / Player Size: 169 bytes (*)
;;     14 registers : 897 Ts (jp included) / Player Size: 176 bytes
;;     Size of Ayt_Builder : 338 bytes
;; -------------
;; (*) With less than 13 registers, performance could vary by a few Ts & bytes depending on the position of the constant registers.
;; (*) Size of player If no ay reg to set then +1 byte else +16 bytes
;;----------------------------------------------------------------------------------------------------------------------------------------------
;; Init AY Reg routine(only when register number < 14)
;;----------------------------------------------------
;; Size of routine : 16 bytes
;; The AY register initialization routine is created if constant registers require prior initialization. 
;; Builder returns the address where the initialization routine was created. 
;; Note that if the initialization routine is not necessary, it will contain a single ret.
;;--------------------------------------------------------------------------
;; Notes :
;; Ayt Builder can be removed once music player built & music data initialized.
;; (i.e. you can use the player and the data file completely independently of the Ayt builder)
;; Performance is given for 10 to 14 non-constant registers, but the player can handle a variable number of constant registers. 
;; For example, if a song uses only two channels and nine active registers, then performance will be better.
;;============================================================================================================================================
;;============================================================================================================================================
;; Technical information about AYT Format
;; --------------------------------------
;; Ayt_Start
;; struct AYT_Header
;; { 
;;  uint8_t Ayt_Version      	; version aaaabbbb >> a.b
;;  uint16_t Ayt_ActiveRegs     ; active reg (bit 15:reg 0...bit 2:reg 13) 1=active 0:inactive
;;  uint8_t Ayt_PatternSize  	; Pattern size 1..255
;;  uint16_t Ayt_FirstSeqMarker ; Offset from Ayt_Start for Seq Ptr on patterns
;;  uint16_t Ayt_LoopSeqMarker 	; Offset from Ayt_Start for Loop Seq Ptr 
;;  uint16_t Ayt_ListInit	; Offset from Ayt_Start for Init of ay reg (*)
;;  uint16_t Ayt_NbPatternPtr  	; Nb of pattern ptr seq for music (NbSeq x NbReg)
;;  uint8_t  Ayt_PlatformFreq	; Platform (b0..4) & Frequency (b5..7) (see table) (**)
;;  uint8_t  Ayt_Reserved	; Reserved for future version (=0)
;; }
;; Start of Patterns
;;
;; (*)
;; struct Ay_Init 
;; {
;;  uint8_t Ayt_Reg		; No reg (0 to 13) 
;;  uint8_t Ayt_Val
;; } x N register to initialize
;; Last byte=#ff
;;
;; (**) 
;; bit 4..0 00000 AMSTRAD CPC (1 000 000 hz)
;;          00001 ORIC        (1 000 000 hz)
;;          00010 ZXUNO       (1 750 000 hz)
;;          00011 PENTAGON    (1 750 000 hz)
;;          00100 TIMEXTS2068 (1 764 000 hz)
;;          00101 ZX128       (1 773 450 hz)
;;          00110 MSX         (1 789 772 hz)
;;          00111 ATARI ST    (2 000 000 hz)
;;          01000 VG5000      (1 000 000 hz) (Ext card VG5210)
;;          11111 UNKNOWN  
;;
;; bit 7..5 000 50 hz
;;          001 25 hz
;;          010 60 hz
;;          011 30 hz
;;          111 UNKNOWN
;;
;; Start of Sequences of ptr on Pattern by Reg  
;;
;; A "Last Sequence" is generated after the last seq of AYT file.
;; Last sequence consists of Ayt_NbRegs pointers on 3 special 'pattern' bytes
;; This is done :
;; - to mute the music at end of play (some ym music don't do the job sometimes)
;; - to give end music status to player
;; Detail of the last sequence : Ayt_NbReg x WordPointer + 3 'pattern' bytes
;;               7 identical ptr on pattern 1 b with 0
;;               1 ptr for r7 on pattern 1b with 3f (mute ay)
;;               4/5 ptr for r8-r12/r13 on pattern 1 b with 0
;;		 1 ptr for r13 on pattern 1b 
;;			bit 7 >> if 1 : R13 not played or Music End
;;			bit 6 >> If 0 : If bit 7==1 then Music End Else r13 is not updated while playing std pattern 
;;                      bit 5 to 0 >> reserved (always 1)
;; i.e. this sequence points to these values (with Nbregs= 14) : 0,0,0,0,0,0,0,3F,0,0,0,0,0,BF
;;============================================================================================================================================
;;
;;----------------------------------------------------------------------------------------------------------------------------------------------
;; Compilation option
;;----------------------------------------------------------------------------------------------------------------------------------------------
;;
;; PlayerAccessByJP option
;;
;; In most situations, the player can be called with the Z80A "call" instruction. 
;; The player uses the stack as a fast data pointer. 
;; It therefore saves the SP register and returns it when it finishes its work.
;; However, in a few exceptional situations, you might need to modify SP in your main code (to also use it as a fast pointer), 
;; and it would therefore not be useful for the player to waste time saving it. 
;; This, however, requires calling the player differently. 
;; The "PlayerAccessByJP" compile option, when set to 1, allows you to assemble "Ayt_Builder" to handle access to the player via the Z80A "JP" instruction. 
;; It is then necessary to specify the return address in the input parameters of "Ayt_Builder" (in the HL register). 
;; Access to the player is then done with a "JP Player" instead of a "CALL Player", and the player will then return to your main program with a JP on the 
;; address you provided in HL to the "Ayt_builder".
;; In terms of technical figures, this option allows you to reduce the size of the built player by 5 bytes, and to save 37 Ts of CPU 
;; (since the SP backup is no longer ensured) 
;; Also I draw your attention to the following point: this call method implies determining in advance the return address of the "player". 
;; In development, if the return address is a label that varies, this forces the presence of the Ayt_Builder in memory at each compilation.
;;
;;
PlayerAccessByJP	equ 1		; If 1, requires you to take into account that SP has been wildly modified
;;----------------------------------------------------------------------------------------------------------------------------------------------

;;----------------------------------------------------------------------------------------------------------------------------------------------
AYT_OFS_Version		equ 0	;; Not yet used (hey it's the 1st version)
AYT_OFS_ActiveRegs	equ 1	;; active reg (bit 0:reg 0...bit 13:reg 13) 1=active (only r12 off is managed in v1)
AYT_OFS_PatternSize	equ 3	;; Pattern size in bytes from 1 to 255
AYT_OFS_FirstSeq	equ 4	;; Offset on first Seq Ptr for start
AYT_OFS_LoopSeq		equ 6	;; Offset on Seq Ptr for loop
AYT_OFS_ListInit	equ 8	;; Offset on Ay Init List
AYT_OFS_NbPatternPtr	equ 10	;; Nb of pattern ptr seq for music (NbSeq x NbReg)
AYT_OFS_PlatformFreq	equ 12  ;; Platform & Freq of play (see table)
AYT_OFS_Reserved	equ 13	;; Rhaaaaaaa
AYT_SIZE_HEADER		equ 14	;; Header size to find first pattern

    ifnot PlayerAccessByJP
	OFS_B1_PtrSaveSP	equ Ayt_PtrSaveSP-Ayt_Player_B1_Start
	OFS_B3_Ayt_ReloadSP	equ Ayt_ReloadSP-Ayt_Player_B3_Start	
    endif
OFS_B1_FirstSeq		equ Ayt_FirstSeq-Ayt_Player_B1_Start 
OFS_B1_to_PatIdx	equ Ayt_PatternIdx-Ayt_FirstSeq
OFS_B1_to_B2_Start	equ Ayt_Player_B2_Start-Ayt_PatternIdx

OFS_B3_SeqPatPtr_Upd	equ Ayt_SeqPatternPtr_Upd-Ayt_Player_B3_Start		
OFS_B3_PatCountPtr1	equ Ayt_PatCountPtr1-Ayt_Player_B3_Start
OFS_B3_PatCountPtr2	equ Ayt_PatCountPtr2-Ayt_Player_B3_Start			
OFS_B3_MusicCnt		equ Ayt_MusicCnt-Ayt_Player_B3_Start
OFS_B3_MusicCntPtr	equ Ayt_MusicCntPtr-Ayt_Player_B3_Start

AYT_B2_SIZE		equ Ayt_Player_B2_End-Ayt_Player_B2_Start	
AYT_B3_SIZE		equ Ayt_Player_B3_End-Ayt_Player_B3_Start
AYT_B4_SIZE		equ Ayt_Player_B4_End-Ayt_Player_B4_Start

_AYT_BUILDER_SIZE	equ AYT_Builder_End-AYT_Builder_Start
;;----------------------------------------------------------------------------------------------------------------------------------------------
;;
;;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;; Ayt Builder
;;
;;	in :	ix=address AYT file
;;		de=msb address Player
;;		a=nbloop expected
;;		hl (optional)=return address of player (only if Builder is compiled with PlayerAccessByJP equ 1)
;;	out : 
;;              hl=ptr on the init routine to call 1 time before to use player address.)
;;		de=ptr on the first free byte after the player
;;
;;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
AYT_Builder_Start
		di
		ld (Ayt_MusicCnt),a		; Nb of loop for music
		ld a,(ix+AYT_OFS_PatternSize)
		ld (Ayt_PatternSize),a		; Set Pattern Size
    if PlayerAccessByJP
		ld (Ayt_ExitPtr01),hl		; Set Main code return address
    else
		push de				; save ptr on first bloc for SP reload
    endif	
		;;-------------------------------------------------------------------------------------------------------------------------------
		;; In AYT file , relocate sequence list ptr on rxx data. (absolute address)
		;; In >> ix=Ptr on AYT File
		;; Out >> de not modified
		;;-------------------------------------------------------------------------------------------------------------------------------
		push de				; save ptr for player
		push ix
		pop bc				; bc=Ptr on start of AYT file
		; 
		ld l,(ix+AYT_OFS_ListInit)	; Offset on Init List 
		ld h,(ix+AYT_OFS_ListInit+1)
		add hl,bc			; Ptr on Ay Init List 
		ld (Ayt_PtrInitList),hl		; In Block 4 if created
		ld a,(hl)			; If empty list, RET as init code
		ld (Ayt_InitCreate),a
		;
		ld l,(ix+AYT_OFS_LoopSeq)	; Offset of Loop Sequence
		ld h,(ix+AYT_OFS_LoopSeq+1)
		add hl,bc
		ld (Ayt_LoopSeq),hl		; Set Loop Sequence
		; 
		ld l,(ix+AYT_OFS_FirstSeq)	; read offset of first seq from start
		ld h,(ix+AYT_OFS_FirstSeq+1)
		add hl,bc			; Ptr on first sequence
		ld (Ayt_FirstSeq),hl
		push hl
		pop iy				; IY ptr on first sequence of pattern ptr by reg
		;
		ld hl,AYT_SIZE_HEADER
		add hl,bc
		ld b,h
		ld c,l
		;
		ld e,(ix+AYT_OFS_NbPatternPtr)
		ld d,(ix+AYT_OFS_NbPatternPtr+1); Nb of Pattern Ptr in all Sequence of AYT file
Ayt_PtrFix_b1
		ld l,(iy+0)
		ld h,(iy+1)			; Read Offset Seq
		add hl,bc			; Absolute Address for Seq Ptr on Rxx
		ld (iy+0),l
		ld (iy+1),h
		inc iy				; Next Seq Ptr rxx+1
		inc iy
		dec de
		ld a,d
		or e
		jr nz,Ayt_PtrFix_b1		; loop all sequence
		pop de

		;;-------------------------------------------------------------------------------------------------------------------------------
		;; Set ptr on var 1st bloc for later updates
		;;-------------------------------------------------------------------------------------------------------------------------------
		ld l,e
		ld h,d
		ld bc,OFS_B1_FirstSeq		; go to "ptr on first seq"
		add hl,bc
		push hl				; save Ptr on sequence ptr
		ld c,OFS_B1_to_PatIdx		; go to ptr index pattern 
		add hl,bc
		push hl				; save ptr of update of pattern idx

 		;;-------------------------------------------------------------------------------------------------------------------------------
		;; Searching first ay active reg 
		;;-------------------------------------------------------------------------------------------------------------------------------
		ld l,(ix+AYT_OFS_ActiveRegs)
		ld h,(ix+AYT_OFS_ActiveRegs+1)	; bit 15, 14, 13, ... of HL are active ay reg 0, 1, 2, 3... if 1
		ld a,#f0			; ay num 1st reg
Ayt_SearchActiveReg
		add hl,hl			; seek 1st non constant reg
		jr c,Ayt_ActiveReg_Found	; Active reg found				; 
		inc a				; not found , next reg
		cp #f0+13
		jr nz,Ayt_SearchActiveReg
		jr $				; error in Ayt file >> no active reg in file check ayt file
Ayt_ActiveReg_Found
		ld (Ayt_FirstAyReg),a
		;;
		;;-------------------------------------------------------------------------------------------------------------------------------
		;; Copy 1st bloc of loader in ram
		;;-------------------------------------------------------------------------------------------------------------------------------
		;
		push hl				; save activeregs 
		ld hl,Ayt_PLayer_B1_Start
		ld bc,Ayt_Player_B1_End-Ayt_Player_B1_Start
		ldir
		pop hl

		;;-------------------------------------------------------------------------------------------------------------------------------
		;; Manage remaining active regs while creating player ay-send block
		;;-------------------------------------------------------------------------------------------------------------------------------
		;
Ayt_CreateActiveReg
		push hl				; save active regs
		ld hl,Ayt_Player_B2_Start	; copy "send ay block" with "inc c"
		ld c,Ayt_Player_B2_End-Ayt_Player_B2_Start
		ldir
		pop hl
		inc a				; ay reg counter
		add hl,hl			; seek next active reg
		jr c,Ayt_NextActReg		; next reg (+1) is active 
Ayt_SearchNextActReg				; current ay reg is not active
		cp #f0+12				; is it the last one before r13?
		jr z,Ayt_Active_Reg		; yes, set 'ld a,12'
		inc a				; searching next active reg
		add hl,hl			; 
		jr nc,Ayt_SearchNextActReg	; search for ay reg active
Ayt_Active_Reg
		ex de,hl			; ay reg active found and not consecutive
		dec hl				; roll back last byte copied
		ld (hl),#3e			; coding [ld a,<regA>] instead of [inc a]
		inc hl
		ld (hl),a
		inc hl
		ex de,hl			; Current ptr for player restored
                jr nz,Ayt_CreateActiveReg	; not last register
		jr Ayt_LastReg
Ayt_NextActReg
		cp #f0+13
		jr nz,Ayt_CreateActiveReg	; Create next "send ay bloc"
		dec de				; Last reg before r13 was r12, then inc a deleted (r13 manage "a" )
Ayt_LastReg
		;
		; Create Last Part R13 & Loop control
		;
		ld hl,Ayt_Player_B3_Start
		ld c,AYT_B3_SIZE
		push de
		push de		
		ldir
		;
		; Link var
		;
		pop bc				; Ptr on struct B3 for calc
		pop iy				; Ptr on struct
		ld hl,OFS_B3_MusicCnt
		add hl,bc			; HL=Abs ptr on MusicCntPtr
		ld (iy+OFS_B3_MusicCntPtr),l
		ld (iy+OFS_B3_MusicCntPtr+1),h
		;
		pop hl				; HL=ptr on pattern idx ptr
		ld (iy+OFS_B3_PatCountPtr1),l
		ld (iy+OFS_B3_PatCountPtr1+1),h
		ld (iy+OFS_B3_PatCountPtr2),l
		ld (iy+OFS_B3_PatCountPtr2+1),h
		pop hl
		ld (iy+OFS_B3_SeqPatPtr_Upd),l	; HL=ptr on sequence ptr 
		ld (iy+OFS_B3_SeqPatPtr_Upd+1),h
		;
    ifnot PlayerAccessByJP				; Player called by call then reload SP
		ld hl,OFS_B3_Ayt_ReloadSP
		add hl,bc
		pop iy				; rec struct B1
		ld (iy+OFS_B1_PtrSaveSP),l
		ld (iy+OFS_B1_PtrSaveSP+1),h
    endif	
		;----------------------------------------------------------------
		; Last block for init routine
		;----------------------------------------------------------------
		ex de,hl
		ld (hl),#c9			; Ret if no ay reg to set
Ayt_InitDefByBuilder
Ayt_InitCreate	equ $+1
		ld a,0				; Ay Reg to init ? (0xFF=Empty list)
		inc a
		ret z				; No (14 register or non available) & give ptr
		ex de,hl
		push de
		ld hl,Ayt_Player_B4_Start
		ld bc,AYT_B4_SIZE		; block 4 created
		ldir		
		pop hl				; Give ptr to user
		ret	

;===============================================================================================================================================
; Raw code template for the constructor
;===============================================================================================================================================
;-----------------------------------------------------------------------------------------------------------------------------------------------
Ayt_Player_B1_Start
    ifnot PlayerAccessByJP
Ayt_PtrSaveSP	equ $+2
		ld (0),sp		; 20 ts SP is saved if player is "called"
    endif		
Ayt_FirstSeq	equ $+1
		ld sp,0
Ayt_FirstAyReg	equ $+1
		ld a,#f0		; Ay Idx Select & i/o Port (bit 14=1)
		ld bc,#c0fd		; Ay Data Select (bit 14=1) +1 (outi pre decr B)
Ayt_PatternIdx	equ $+1
		ld de,0
Ayt_Player_B1_End
;-----------------------------------------------------------------------------------------------------------------------------------------------
Ayt_Player_B2_Start	
		out (#fd),a 		; Select Ay Idx Acc and %1111
		pop hl			; Get Pattern Ptr
		add hl,de		; + offset in Pattern
		outi			; Send to Ay 
		inc b			; B++ for data port
		inc a			; AY Idx++
Ayt_Player_B2_End
;-----------------------------------------------------------------------------------------------------------------------------------------------
Ayt_Player_B3_Start
		pop hl			; Get Pattern Ptr for r13
		add hl,de		; + offset in Pattern
		bit 7,(hl)		; is it a valid reg ?
					; 
		jr z,Ayt_SendAY_r13	; 12/07/07/07/12/07 
		bit 6,(hl)		; 00/12/12/12/00/12 is it the end of music ?
		jr nz,Ayt_NotLastSeq	; 00/12/07/07/00/12
Ayt_MusicCnt equ $+1			; no!
		ld a,0			; 00/00/07/07/00/00 loop counter 
		dec a			; 00/00/04/04/00/00
		jr z,Ayt_MusicEnd	; 00/00/12/07/00/00 loop finished ?
Ayt_MusicCntPtr equ $+1			; 	            Player on mute
		ld (0),a		; 00/00/00/13/00/00 music loops > loop counter upd
Ayt_LoopSeq  equ $+1
		ld sp,0			; 00/00/00/10/00/00 Ptr on sequence loop
		jr Ayt_Player_NewSeq	; 00/00/00/12/00/00 >>  
Ayt_MusicEnd				
		dec (hl)		; 00/00/11/00/00/00
		dec (hl)		; 00/00/11/00/00/00
		inc (hl)		; 00/00/11/00/00/00
		inc (hl)		; 00/00/11/00/00/00
		cp (hl)			; 00/00/07/00/00/00
		nop			; 00/00/04/00/00/00
		jr Ayt_PlayerExit	; 00/00/12/00/00/00 >> 3th=116 Ts

		;------------------------
Ayt_NotLastSeq				;   
		jr Ayt_Player_r13end	; 00/12/00/00/00/12
		;------------------------
Ayt_SendAY_r13				; <<<
		inc a			; 04/00/00/00/04/00 
		out (#fd),a		; 11/00/00/00/11/00 Select R13 AY reg
	        outi        		; 16/00/00/00/16/00 SendAY Data
Ayt_Player_r13end			; <<<
					;------------------
Ayt_PatCountPtr1 equ $+1
		ld a,(0)		; 13/13/00/00/13/13 Get Pattern offset
		inc a			; 04/04/00/00/04/04 Offset +1
Ayt_PatternSize	equ $+1
		cp  0			; 07/07/00/00/07/07 Pattern completed ?
		jr z,Ayt_EndPattern	; 12/12/00/00/07/07
		cp (hl)			; 00/00/00/00/07/07
		nop			; 00/00/00/00/04/04
		inc hl			; 00/00/00/00/06/06
		jr Ayt_SeqPat		; 00/00/00/00/12/12
Ayt_EndPattern				; 
Ayt_Player_NewSeq
Ayt_SeqPatternPtr_Upd equ $+2		; !!! Critical here !!!
		ld (0),sp		; 20/20/00/20/00/00 update ptr of seq on patterns
		xor a			; 04/04/00/04/00/00 offset all pattern to 0
Ayt_SeqPat
Ayt_PatCountPtr2 equ $+1		; 
		ld (0),a		; 13/13/00/13/13/13 update offset on patterns (4th=116Ts)(1th=116Ts)(2th=116Ts)
Ayt_PlayerExit
    if PlayerAccessByJP
Ayt_ExitPtr01	equ $+1
		jp 0 			; exit from player via JP 
    else	
Ayt_ReloadSP	equ $+1
		ld sp,0			; Player was called , SP is restored
		ret			; and return to main code
    endif
Ayt_Player_B3_End
;-----------------------------------------------------------------------------------------------------------------------------------------------
; Init of not active AY reg
; 
Ayt_Player_B4_Start
		ld bc,#c0fd		; Ay Data Select (bit 14=1) +1 (outi pre decr B)
Ayt_PtrInitList	equ $+1
		ld hl,0
Ayt_InitReg_Loop
		ld a,(hl)		; get register
		bit 7,a			; End list of init ay reg
		ret nz			; yes
		or #f0			; Ay Idx Select & i/o Port (bit 14=1)
		inc hl			; no ptr on data
		out (#fd),a 		; Select Ay Idx 
		outi			; Send to Ay 
		jr Ayt_InitReg_Loop	; next reg
Ayt_Player_B4_End
;-----------------------------------------------------------------------------------------------------------------------------------------------
Ayt_Builder_End
;;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;;**********************************************************************************************************************************************
;; FILE AYT LOGON PHOENIX 13 REGISTRES
;;**********************************************************************************************************************************************
AYT_File
;;		incbin "kenotron.ayt"		; 14 registers 0..13
;;		incbin "logon.ayt"		; 11 registers (- 3, 11, 12)
		incbin "../../ayt-files/still_scrolling_final_zx.ayt"
End_Example
save "bin/EXE8000.BIN",Start_Example,End_Example-Start_Example