;;**********************************************************************************************************************************************
;; VG 5000 
;;**********************************************************************************************************************************************
;; EXAMPLE / SAMPLE
;; The VG5000 does not have a built-in sound card. 
;; Two cards exist that can be connected to the expansion port (VG5210 (ay-3-8910) and the older VG5232 (ay-3-8912)).
;; https://sites.google.com/view/vg5000-hardware/vg5k-new-hardware/carte-son-st%C3%A9r%C3%A9o/Fabrication-du-VG5210
;; The DCVG5K emulator unfortunately does not support these cards (http://dcvg5k.free.fr/)
;; There is no read port to know if a VSYNC is in progress. 
;; However, an IRQ occurs at each Vsync and therefore allows the player to synchronize. 
;; It should be noted, however, that a ROM is present at vector address #38 and it is therefore necessary to switch to IM2 
;; to eliminate any system interference.
;;
;; Thanks to BdcIron (Amaury Durand) for the help about Vdp (visit his site https://vg5000.zilog.fr) and his test of the player 
;; on a machine with a 32k extension and a VG5210 sound card
;;
;;**********************************************************************************************************************************************
;;------------------------------------------------------------------------------------------------------------
;; VG5000 Header (for .k7) but need to transform datas)
;;------------------------------------------------------------------------------------------------------------
;		db "VG5KAYT "			; name 8 bytes 
;		dw Ayt_Player+166+16-1		; size word
;		dw RunVg5000			; address
;		db 0				; type code asm
;;------------------------------------------------------------------------------------------------------------
PlayerAccessByJP	equ 0
;;------------------------------------------------------------------------------------------------------------
MyProgram	equ #5000 ; -13
;;------------------------------------------------------------------------------------------------------------
		org MyProgram
RunVg5000
		di
		ld sp,Mapile		; New Stack
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
		;
		;-----------------------------------------------------------------------------------------------
		; Build the player routine (needs 355 or 370 bytes) 
		;-----------------------------------------------------------------------------------------------
		;
		ld ix,AYT_File		; Ptr on AYT_File
		ld de,AYT_Player	; Ptr of Adress where Player is built
		ld bc,0			; Ptr on 16 bytes for init (if 0, Builder create the init after the player)
		ld a,255		; Nb of loop for the music
    if PlayerAccessByJP
		ld hl,AYT_Player_Ret	; Ptr where player come back in MyProgram
    endif
		call AYT_Builder	; Build the player at #<d>00 for file pointed by <ix> for <a> loop
		ld (InitRegAy),hl
InitRegAy	equ $+1
		call 0
    if PlayerAccessByJP
		ld (AYT_Player_Ret+1),sp ; Save Stack Pointer 
    endif
		;-----------------------------------------------------------------------------------------------
		; Main Code Playing Music
		;-----------------------------------------------------------------------------------------------
MainLoop	
		;-----------------------------------------------------------
		; Border Red color with VDP
		;-----------------------------------------------------------
		; 
                ld a,#21           ; Select register 1
                out (#8f),a
                ld a,%00000001     ; red
                out (#cf),a
               
                ld a,#28           ; R0+Exe
                out (#8f),a
                ld a,%10000010     ; MAT register
                out (#cf),a
		;
		;-----------------------------------------------------------
		; Calling player (JP or CALL method
		;-----------------------------------------------------------
		;
		di			; Interrupt off (for SP)
    if PlayerAccessByJP
		jp AYT_Player		; jump to the player
AYT_Player_Ret	ld sp,0			; address return of the player

    else
		call AYT_Player
    endif
		;-----------------------------------------------------------
		; Border Black color with VDP
		;-----------------------------------------------------------
                ld a,#21           ; Select register 1
                out (#8f),a
                ld a,%00000000     ; black
                out (#cf),a
               
                ld a,#28           ; R0+Exe
                out (#8f),a
                ld a,%10000010     ; MAT register
                out (#cf),a
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
		ld bc,280		; 10+ (bc x 26 Ts)
wait_border
		dec bc
		ld a,b
		or c
		jr nz,wait_border

		jr MainLoop
;;------------------------------------------------------------------------------------------------------------
;; Stack
		ds 20
Mapile
;;============================================================================================================================================
AYT_Builder
		read "AytPlayerBuilder-VG5000.asm"
;;
;;**********************************************************************************************************************************************
;; FILE AYT 
;;**********************************************************************************************************************************************
AYT_File
		incbin "discmal.ayt"	
AYT_Player	
		ds 370+16,#FF

save "VG5KAYT.BIN",MyProgram,Ayt_Player+370+16-MyProgram