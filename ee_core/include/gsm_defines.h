/*
#
# Graphics Synthesizer Mode Selector (a.k.a. GSM) - Force (set and keep) a GS Mode, then load & exec a PS2 ELF
#-------------------------------------------------------------------------------------------------------------
# Copyright 2009, 2010, 2011 doctorxyz & dlanor
# Copyright 2011, 2012 doctorxyz, SP193 & reprep
# Copyright 2013 Bat Rastard
# Copyright 2014, 2015, 2016 doctorxyz
# Licenced under Academic Free License version 2.0
# Review LICENSE file for further details.
#
# Private definitions header file for GSM.
#
*/

.equ TRAP_BASE,   0x12000000
.equ TRAP_MASK,   0x1FFFEF0F
.equ GS_BASE,     0x12000000
.equ GS_PMODE,    0x0000
.equ GS_SMODE1,   0x0010
.equ GS_SMODE2,   0x0020
.equ GS_SRFSH,    0x0030
.equ GS_SYNCH1,   0x0040
.equ GS_SYNCH2,   0x0050
.equ GS_SYNCV,    0x0060
.equ GS_DISPFB1,  0x0070
.equ GS_DISPLAY1, 0x0080
.equ GS_DISPFB2,  0x0090
.equ GS_DISPLAY2, 0x00A0
.equ GS_BGCOLOUR, 0x00E0
.equ GS_CSR,      0x1000

#Add - on video modes(made possible by SP193 and reprep)
GS_MODE_DTV_576P=0x53

#GSMSourceSetGsCrt
.equ Source_INT,      0 # HALF
.equ Source_MODE,     2 # HALF
.equ Source_FFMD,     4 # HALF

#GSMDestSetGsCrt
.equ Target_INT,      0 # HALF
.equ Target_MODE,     2 # HALF
.equ Target_FFMD,     4 # HALF

#GSMSourceGSRegs
.equ Source_PMODE,     0 # DWORD
.equ Source_SMODE1,    8 # DWORD
.equ Source_SMODE2,   16 # DWORD
.equ Source_SRFSH,    24 # DWORD
.equ Source_SYNCH1,   32 # DWORD
.equ Source_SYNCH2,   40 # DWORD
.equ Source_SYNCV,    48 # DWORD
.equ Source_DISPFB1,  56 # DWORD
.equ Source_DISPLAY1, 64 # DWORD
.equ Source_DISPFB2,  72 # DWORD
.equ Source_DISPLAY2, 80 # DWORD

#GSMDestGSRegs
.equ Target_PMODE,     0 # DWORD
.equ Target_SMODE1,    8 # DWORD
.equ Target_SMODE2,   16 # DWORD
.equ Target_SRFSH,    24 # DWORD
.equ Target_SYNCH1,   32 # DWORD
.equ Target_SYNCH2,   40 # DWORD
.equ Target_SYNCV,    48 # DWORD
.equ Target_DISPFB1,  56 # DWORD
.equ Target_DISPLAY1, 64 # DWORD
.equ Target_DISPFB2,  72 # DWORD
.equ Target_DISPLAY2, 80 # DWORD

#GSMFlags
.equ X_offset,              0 # WORD
.equ Y_offset,              4 # WORD
.equ ADAPTATION_fix,        8 # BYTE
.equ PMODE_fix,             9 # BYTE
.equ SMODE1_fix,           10 # BYTE
.equ SMODE2_fix,           11 # BYTE
.equ SRFSH_fix,            12 # BYTE
.equ SYNCH_fix,            13 # BYTE
.equ SYNCV_fix,            14 # BYTE
.equ DISPFB_fix,           15 # BYTE
.equ DISPLAY_fix,          16 # BYTE
.equ FIELD_fix,            17 # BYTE
.equ gs576P_param,         18 # BYTE

#GSMAdapts
.equ Adapted_DISPLAY1,             0 # DWORD
.equ Adapted_DISPLAY2,             8 # DWORD
.equ Interlace_FRAME_Mode_Flag,   16 # BYTE -> Double Height for SMODE2's INT=1 (Interlace Mode) and FFMD=1 (FRAME Mode. Read every line)
.equ SMODE2_adaptation,           17 # BYTE -> Adapted SMODE2 patch value
