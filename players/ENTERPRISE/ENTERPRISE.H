;=====================================================================================================================
; Logon System / Longshot
; ENTERPRISE 128
; Constantes relatives aux circuits
;=====================================================================================================================
;---------------------------------------------------------------------------------------------------------------------
; NICK
;---------------------------------------------------------------------------------------------------------------------
MB_VINT_ON		equ %10000000	; Video interrupt at start of the mode line
MB_VINT_OFF		equ %00000000   ; No video interrupt
MB_COLOURMODE_M2C	equ %00000000	; Mode 2 couleurs (Eq mode 2 CPC)
MB_COLOURMODE_M4C	equ %00100000	; Mode 4 couleurs (Eq mode 1 CPC)
MB_COLOURMODE_M16C	equ %01000000	; Mode 16 couleurs (Eq mode 0 CPC)
MB_COLOURMODE_M256C	equ %01100000	; Mode 256 couleurs
MB_VRES_FRESO		equ %00010000	; Full resolution mode 
MB_VRES_RRESO		equ %00010000	; Repetition de ligne
MB_VIDEOMODE_VSYNC	equ %00000000	; VSYNC 
MB_VIDEOMODE_PIXEL	equ %00000010	; PIXEL Mode
MB_VIDEOMODE_ATTRB 	equ %00000100	; ATTRIBUTE Mode
MB_VIDEOMODE_CH256	equ %00000110	; CH256 Mode
MB_VIDEOMODE_CH128	equ %00001000	; CH128 Mode
MB_VIDEOMODE_CH64	equ %00001010	; CH64 Mode
MB_VIDEOMODE_UNUSED	equ %00001100	; Unused
MB_VIDEOMODE_LPIXEL	equ %00001110	; LPIXEL Mode
MB_RELOAD_ON		equ %00000001   ; Indique demarrage Liste (et fin de frame)
MB_RELOAD_OFF		equ %00000000   ; 


COLOUR_GREEN_000	equ %00000000
COLOUR_GREEN_001	equ %00000010
COLOUR_GREEN_010	equ %00010000
COLOUR_GREEN_011	equ %00010010
COLOUR_GREEN_100	equ %10000000
COLOUR_GREEN_101	equ %10010000
COLOUR_GREEN_110	equ %10000010
COLOUR_GREEN_111	equ %10010010

COLOUR_RED_000		equ %00000000
COLOUR_RED_001		equ %00000001
COLOUR_RED_010		equ %00001000
COLOUR_RED_011		equ %00001001
COLOUR_RED_100		equ %01000000
COLOUR_RED_101		equ %01000001
COLOUR_RED_110		equ %01001000
COLOUR_RED_111		equ %01001001

COLOUR_BLUE_00		equ %00000000
COLOUR_BLUE_01		equ %00000100
COLOUR_BLUE_10		equ %00100000
COLOUR_BLUE_11		equ %00100100
