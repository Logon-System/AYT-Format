;;**********************************************************************************************************************************************
;;
;; EXAMPLE / SAMPLE
;; MSX 
;; OPEN MSX 21.0 MACHINE SONY HB F1XD
;; MSX BASIC VERSION 2.0 
;; CREATE VIRTUAL DISK ON A FILE DISK
;; UNDER BASIC : BLOAD "MSXAYT.BIN",R
;; ALTERNATIVE
;; DISK MSXDSK.DSK ON https://webmsx.org/ THEN LOAD "PLAY" + RUN
;; SETTINGS QUICK OPTION DISPLAY PAL (50 HZ) BETTER FOR MUSIC SPEED
;;
;;**********************************************************************************************************************************************
PlayerAccessByJP	equ 0
;;----------------------------
MyProgram	equ #8000-7
;;----------------------------
		org MyProgram
;;------------------------------------------------------------------------------------------------------------
;; Entete MSX
;;------------------------------------------------------------------------------------------------------------
		db #fe		; octet magique
		dw RunMsx
		dw Ayt_Player+166+16-1
		dw RunMsx
;;------------------------------------------------------------------------------------------------------------
RunMsx
		di
		;
		; Build the player routine (needs 296 or 312 bytes) 
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
		;==================================================================================================================
		;;
		;; 3.580.000 Ts >> 60 Frame (ntsc)>> 59659 Ts /Frame >> 59659-(729+12)=58918 >> 26 ts by loop >> 2266 lpops
		;;
		xor a			; Init Vsync port
		out (#99),a
		ld a,15+128
		out (#99),a
MainLoop
WaitVBlank1:
  		in a,(#99)      	; read VDP status
		rlca
    		jr nc,WaitVBlank1 	; wait for Vsync

		ld bc,400		; 5 + (bc x 26)
WaitLoop:
		dec bc
		ld a,b
		or c
		jr nz, WaitLoop
		;
		ld a,#f3	; 4	; color text & bg
		out (#99),a	; 11
		ld a,#80+7	; 4
		out (#99),a	; 11

    if PlayerAccessByJP
		jp AYT_Player		; jump to the player			
AYT_Player_Ret	ld sp,0			; address return of the player

    else
		call AYT_Player		;; 729 Ts
    endif
		ld a,#f1	; 4
		out (#99),a	; 11
		ld a,#80+7	; 4
		out (#99),a	; 11
		jr MainLoop	; 12 Ts

;==================================================================================================================
Ayt_Builder
		read "AytPlayerBuilder-MSX.asm"

;;**********************************************************************************************************************************************
;; FILE AYT 
;;**********************************************************************************************************************************************
AYT_File
;		incbin "kenotron.ayt"		; 14 registres 0..13
;		incbin "logon.ayt"		; 3 registres en moins 3, 11, 12
		incbin "../../ayt-files/still_scrolling.ayt"
AYT_Player	

save "MSXAYT.BIN",MyProgram,Ayt_Player+166+16-MyProgram
