/*
  padhook.c Open PS2 Loader In Game Reset

  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.

  Reset SPU function taken from PS2SDK freesd.
  Copyright (c) 2004 TyRaNiD <tiraniddo@hotmail.com>
  Copyright (c) 2004,2007 Lukasz Bruun <mail@lukasz.dk>

  PadOpen Hooking function inspired from ps2rd.
  Hook scePadPortOpen/scePad2CreateSocket instead of scePadRead/scePad2Read
  Copyright (C) 2009 jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright (C) 2009 misfire <misfire@xploderfreax.de>
*/

#include <iopcontrol.h>
#include "ee_core.h"
#include "iopmgr.h"
#include "modmgr.h"
#include "util.h"
#include "spu.h"
#include "padhook.h"
#include "padpatterns.h"
#include "syshook.h"
#include "tlb.h"
#ifdef GSM
#include "gsm_api.h"
#endif
#ifdef IGS
#include "igs_api.h"
#endif
#ifdef CHEAT
#include "cheat_api.h"
#endif

/* scePadPortOpen & scePad2CreateSocket prototypes */
static int (*scePadPortOpen)(int port, int slot, void *addr);
static int (*scePad2CreateSocket)(pad2socketparam_t *SocketParam, void *addr);

/* Monitored pad data */
static paddata_t Pad_Data;

/* Monitored power button data */
static powerbuttondata_t Power_Button;

/* IGR Thread ID */
static int IGR_Thread_ID = -1;

/* IGR thread stack & stack size */
#define IGR_STACK_SIZE (16 * 512)
static u8 IGR_Stack[IGR_STACK_SIZE] __attribute__((aligned(16)));

/* Extern symbol */
extern void *_gp;

// Shutdown Dev9 hardware
static void Shutdown_Dev9()
{
    u16 dev9_hw_type;

    DIntr();
    ee_kmode_enter();

    // Get dev9 hardware type
    dev9_hw_type = *DEV9_R_146E & 0xf0;

    // Shutdown Pcmcia
    if (dev9_hw_type == 0x20) {
        *DEV9_R_146C = 0;
        *DEV9_R_1474 = 0;
    }
    // Shutdown Expansion Bay
    else if (dev9_hw_type == 0x30) {
        *DEV9_R_1466 = 1;
        *DEV9_R_1464 = 0;
        *DEV9_R_1460 = *DEV9_R_1464;
        *DEV9_R_146C = *DEV9_R_146C & ~4;
        *DEV9_R_1460 = *DEV9_R_146C & ~1;
    }

    //Wait a sec
    delay(5);

    ee_kmode_exit();
    EIntr();
}

// Load home ELF
static void t_loadElf(void)
{
    int ret;
    char *argv[2];
    t_ExecData elf;

    if (!DisableDebug)
        GS_BGCOLOUR = 0x80FF00; // Blue Green

    // Init RPC & CMD
    SifInitRpc(0);

    if (!DisableDebug)
        GS_BGCOLOUR = 0x000080; // Dark Red

    // Apply Sbv patches
    sbv_patch_disable_prefix_check();

    if (!DisableDebug)
        GS_BGCOLOUR = 0xFF8000; // Blue sky

    // Load basic modules
    LoadModule("rom0:SIO2MAN", 0, NULL);
    LoadModule("rom0:MCMAN", 0, NULL);

    // Load exit ELF
    argv[0] = ExitPath;
    argv[1] = NULL;

    ret = LoadElf(argv[0], &elf);

    if (!ret && elf.epc) {

        // Exit services
        LoadFileExit();
        SifExitIopHeap();
        SifExitRpc();

        FlushCache(0);
        FlushCache(2);

        if (!DisableDebug)
            GS_BGCOLOUR = 0x0080FF; // Orange

        // Execute BOOT.ELF
        _ExecPS2((void *)elf.epc, (void *)elf.gp, 1, argv);
    }

    if (!DisableDebug) {
        GS_BGCOLOUR = 0x0000FF; // Red
        delay(5);
    }

    // Return to PS2 Browser
    _Exit(0);
}

// Poweroff PlayStation 2
static void PowerOff_PS2(void)
{
    // Shutdown Dev9 hardware
    Shutdown_Dev9();

    DIntr();
    ee_kmode_enter();

    // PowerOff PS2
    *CDVD_R_SDIN = 0x00;
    *CDVD_R_SCMD = 0x0F;

    ee_kmode_exit();
    EIntr();
}

// In Game Reset Thread
static void IGR_Thread(void *arg)
{
    u32 Cop0_Index, Cop0_Perf;

    // Place our IGR thread in WAIT state
    // It will be woken up by our IGR interrupt handler
    SleepThread();

    DPRINTF("IGR thread woken up!\n");

    // If Pad Combo is Start + Select then Return to Home, else if Pad Combo is UP then take IGS
    if ((Pad_Data.combo_type == IGR_COMBO_START_SELECT)
#ifdef IGS
        || ((Pad_Data.combo_type == IGR_COMBO_UP) && (EnableGSMOp))
#endif
            ) {
        if (!DisableDebug)
            GS_BGCOLOUR = 0xFFFFFF; // White

        // Re-Init RPC & CMD
        SifInitRpc(0);

        if (!DisableDebug)
            GS_BGCOLOUR = 0x800000; // Dark Blue

        // Reset SPU
        ResetSPU();

        if (!DisableDebug)
            GS_BGCOLOUR = 0x0000FF; // Red

        // Reset IO Processor
        while (!Reset_Iop(NULL, 0)) {
            ;
        }

        // Remove kernel hooks
        Remove_Kernel_Hooks();

        // Check Translation Look-Aside Buffer
        // Some game (GT4, GTA) modify memory map
        // A re-init is needed to properly access memory
        Cop0_Index = GetCop0(0);

        // Init TLB
        if (Cop0_Index != 0x26) {
            InitializeTLB();
        }

        // Check Performance Counter
        // Some game (GT4) start performance counter
        // When counter overflow, an exception occur, so stop them
        Cop0_Perf = GetCop0(25);

        // Stop Performance Counter
        if (Cop0_Perf & 0x80000000) {
            __asm__ __volatile__(
                " mfc0 $3, $25;"
                " lui	 $2, 0x8000;"
                " or	 $3, $3, $2;"
                " xor	 $3, $3, $2;"
                " mtc0 $3, $25;"
                " sync.p;");
        }

#ifdef GSM
        if (EnableGSMOp) {
            if (!DisableDebug)
                GS_BGCOLOUR = 0x00FF00; // Green
            DPRINTF("Stopping GSM...\n");
            DisableGSM();
        }
#endif

#ifdef CHEAT
        if (EnableCheatOp) {
            if (!DisableDebug)
                GS_BGCOLOUR = 0xFF0000; // Blue
            DPRINTF("Stopping PS2RD Cheat Engine...\n");
            DisableCheats();
        }
#endif

        if (!DisableDebug)
            GS_BGCOLOUR = 0x00FFFF; // Yellow

        while (!SifIopSync()) {
            ;
        }

        if (!DisableDebug)
            GS_BGCOLOUR = 0xFF80FF; // Pink

        // Init RPC & CMD
        SifInitRpc(0);

#ifdef IGS
        if ((Pad_Data.combo_type == IGR_COMBO_UP) && (EnableGSMOp))
            InGameScreenshot();
#endif

        if (!DisableDebug)
            GS_BGCOLOUR = 0x008000; // Dark Green

        // Exit services
        SifExitRpc();

        // Execute home loader
        if (ExitPath[0] != '\0')
            _ExecPS2(t_loadElf, &_gp, 0, NULL);

        // Return to PS2 Browser
        _Exit(0);
    }

    // If combo is R3 + L3 or Reset failed, Poweroff PS2
    PowerOff_PS2();
}

// IGR VBLANK_END interrupt handler install to monitor combo trick in pad data aera
static int IGR_Intc_Handler(int cause)
{
    int i;
    u8 pad_pos_state, pad_pos_frame, pad_pos_combo1, pad_pos_combo2;

    // Copy values via the uncached segment, to bypass the cache.
    pad_pos_state = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_state];
    pad_pos_frame = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_frame];
    pad_pos_combo1 = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_combo1];
    pad_pos_combo2 = ((u8 *)UNCACHED_SEG(Pad_Data.pad_buf))[Pad_Data.pos_combo2];

    // First check pad state
    if (((Pad_Data.libpad == IGR_LIBPAD_V1) && (pad_pos_state == IGR_PAD_STABLE_V1)) ||
        ((Pad_Data.libpad == IGR_LIBPAD_V2) && (pad_pos_state == IGR_PAD_STABLE_V2))) {
        // Check if pad buffer is still alive with pad data frame counter
        // If pad frame change save it, otherwise tell to syshook to re-install padOpen hook
        if (Pad_Data.vb_count++ >= 10) {
            if (pad_pos_frame != Pad_Data.prev_frame) {
                padOpen_hooked = 1;
                Pad_Data.prev_frame = pad_pos_frame;
            } else {
                padOpen_hooked = 0;
            }
            Pad_Data.vb_count = 0;
        }

        // Combo R1 + L1 + R2 + L2
        if (pad_pos_combo1 == IGR_COMBO_R1_L1_R2_L2) {
            // Combo Start + Select, R3 + L3 or UP
            if ((pad_pos_combo2 == IGR_COMBO_START_SELECT) || // Start + Select combo, so reset
                (pad_pos_combo2 == IGR_COMBO_R3_L3)           // R3 + L3 combo, so poweroff
#ifdef IGS
                || ((pad_pos_combo2 == IGR_COMBO_UP) && (EnableGSMOp)) // UP combo, so take IGS
#endif
                )

                Pad_Data.combo_type = pad_pos_combo2;
        }
    }

    ee_kmode_enter();

    // Check power button press
    if ((*CDVD_R_NDIN & 0x20) && (*CDVD_R_POFF & 0x04)) {
        // Increment button press counter
        Power_Button.press++;

        // Cancel poweroff to catch the second button press
        *CDVD_R_SDIN = 0x00;
        *CDVD_R_SCMD = 0x1B;
    }

    // Start VBlank counter when power button is pressed
    if (Power_Button.press) {
        // Check number of power button press after 1 ~ sec
        if (Power_Button.vb_count++ >= 50) {
            if (Power_Button.press == 1)
                Pad_Data.combo_type = IGR_COMBO_R3_L3; // power button press 1 time, so poweroff
            else
                Pad_Data.combo_type = IGR_COMBO_START_SELECT; // power button press 2 time, so reset
        }
    }

    ee_kmode_exit();

    // If power button or combo is press
    // Disable all interrupts (not SBUS, TIMER2, TIMER3 use by kernel)
    // Suspend and Change priority of all threads other then our IGR thread
    // Wakeup and Change priority of our IGR thread
    if (Pad_Data.combo_type != 0x00) {
        // Disable Interrupts
        iDisableIntc(kINTC_GS);
        iDisableIntc(kINTC_VBLANK_START);
        iDisableIntc(kINTC_VBLANK_END);
        iDisableIntc(kINTC_VIF0);
        iDisableIntc(kINTC_VIF1);
        iDisableIntc(kINTC_VU0);
        iDisableIntc(kINTC_VU1);
        iDisableIntc(kINTC_IPU);
        iDisableIntc(kINTC_TIMER0);
        iDisableIntc(kINTC_TIMER1);

        // Loop for each threads
        for (i = 1; i < 256; i++) {
            if (i != IGR_Thread_ID) {
                // Suspend all threads
                iSuspendThread(i);
                iChangeThreadPriority(i, 127);
            }
        }

        DPRINTF("IGR: trying to wake IGR thread...\n");
        iChangeThreadPriority(IGR_Thread_ID, 0);
        // WakeUp IGR thread
        iWakeupThread(IGR_Thread_ID);
    }

    ExitHandler();

    return 0;
}

// Install IGR thread, and Pad interrupt handler
static void Install_IGR(void *addr, int libpad)
{
    ee_thread_t thread_param;

    // Reset power button data
    Power_Button.press = 0;
    Power_Button.vb_count = 0;

    // Init Pad_Data informations
    Pad_Data.vb_count = 0;
    Pad_Data.libpad = libpad;
    Pad_Data.pad_buf = addr;
    Pad_Data.combo_type = 0x00;
    Pad_Data.prev_frame = 0x00;

    // Set positions of pad data and pad state in buffer
    if (libpad == IGR_LIBPAD_V1) {
        Pad_Data.pos_combo1 = 3;
        Pad_Data.pos_combo2 = 2;
        Pad_Data.pos_state = 112;
        Pad_Data.pos_frame = 88;
    } else {
        Pad_Data.pos_combo1 = 29;
        Pad_Data.pos_combo2 = 28;
        Pad_Data.pos_state = 4;
        Pad_Data.pos_frame = 124;
    }

    // Create and start IGR thread
    thread_param.gp_reg = &_gp;
    thread_param.func = IGR_Thread;
    thread_param.stack = (void *)IGR_Stack;
    thread_param.stack_size = IGR_STACK_SIZE;
    thread_param.initial_priority = 127;
    IGR_Thread_ID = CreateThread(&thread_param);

    StartThread(IGR_Thread_ID, NULL);

    // Create IGR interrupt handler
    AddIntcHandler(kINTC_VBLANK_END, IGR_Intc_Handler, 0);
    EnableIntc(kINTC_VBLANK_END);
}

// Hook function for libpad scePadPortOpen
static int Hook_scePadPortOpen(int port, int slot, void *addr)
{
    int ret;

    // Make sure scePadPortOpen function is still available
    if (port == 0 && slot == 0) {
        DPRINTF("IGR: Hook_scePadPortOpen - padOpen hooking check...\n");
        Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_CHECK);
    }

    // Call original scePadPortOpen function
    ret = scePadPortOpen(port, slot, addr);

    // Install IGR with libpad1 parameters
    if (port == 0 && slot == 0) {
        DPRINTF("IGR: Hook_scePadPortOpen - installing IGR...\n");
        Install_IGR(addr, IGR_LIBPAD_V1);
    }

    return ret;
}

// Hook function for libpad2 scePad2CreateSocket
static int Hook_scePad2CreateSocket(pad2socketparam_t *SocketParam, void *addr)
{
    int ret;

    // Make sure scePad2CreateSocket function is still available
    if (SocketParam->port == 0 && SocketParam->slot == 0)
        Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_CHECK);

    // Call original scePad2CreateSocket function
    ret = scePad2CreateSocket(SocketParam, addr);

    // Install IGR with libpad2 parameters
    if (SocketParam->port == 0 && SocketParam->slot == 0)
        Install_IGR(addr, IGR_LIBPAD_V2);

    return ret;
}

// This function patch the padOpen calls. (scePadPortOpen or scePad2CreateSocket)
int Install_PadOpen_Hook(u32 mem_start, u32 mem_end, int mode)
{
    u32 *ptr, *ptr2;
    u32 inst, fncall;
    u32 mem_size, mem_size2;
    u32 pattern[1], mask[1];
    int i, found, patched;

    pattern_t padopen_patterns[NB_PADOPEN_PATTERN] = {
        {padPortOpenpattern0, padPortOpenpattern0_mask, sizeof(padPortOpenpattern0), 1},
        {pad2CreateSocketpattern0, pad2CreateSocketpattern0_mask, sizeof(pad2CreateSocketpattern0), 2},
        {pad2CreateSocketpattern1, pad2CreateSocketpattern1_mask, sizeof(pad2CreateSocketpattern1), 2},
        {pad2CreateSocketpattern2, pad2CreateSocketpattern2_mask, sizeof(pad2CreateSocketpattern2), 2},
        {padPortOpenpattern1, padPortOpenpattern1_mask, sizeof(padPortOpenpattern1), 1},
        {padPortOpenpattern2, padPortOpenpattern2_mask, sizeof(padPortOpenpattern2), 1},
        {padPortOpenpattern3, padPortOpenpattern3_mask, sizeof(padPortOpenpattern3), 1}};

    found = 0;
    patched = 0;

    // Loop for each libpad version
    for (i = 0; i < NB_PADOPEN_PATTERN; i++) {
        ptr = (u32 *)mem_start;
        while (ptr) {
            // Purple while PadOpen pattern search
            if (!DisableDebug)
                GS_BGCOLOUR = 0x800080; //Purple

            mem_size = mem_end - (u32)ptr;

            // First try to locate the orginal libpad's PadOpen function
            ptr = find_pattern_with_mask(ptr, mem_size, padopen_patterns[i].pattern, padopen_patterns[i].mask, padopen_patterns[i].size);
            if (ptr) {
                DPRINTF("IGR: found padopen pattern%d at 0x%08x mode=%d\n", i, (int)ptr, mode);
                found = 1;

                // Green while PadOpen patches
                if (!DisableDebug)
                    GS_BGCOLOUR = 0x008000; //Dark green

                // Save original PadOpen function
                if (padopen_patterns[i].version == IGR_LIBPAD_V1)
                    scePadPortOpen = (void *)ptr;
                else
                    scePad2CreateSocket = (void *)ptr;

                if (mode == PADOPEN_HOOK) {
                    // Retrieve PadOpen call Instruction code
                    inst = 0x00000000;
                    inst |= 0x03ffffff & ((u32)ptr >> 2);

                    // Make pattern with function call code saved above
                    // Ignore bits 24-27 because Jump type can be J(8) or JAL(C)
                    pattern[0] = inst;
                    mask[0] = 0xf0ffffff;

                    DPRINTF("IGR: searching opcode %08x witk mask %08x\n", (int)pattern[0], (int)mask[0]);

                    // Search & patch for calls to PadOpen
                    ptr2 = (u32 *)0x00100000;
                    while (ptr2) {
                        mem_size2 = 0x01ff0000 - (u32)ptr2;

                        ptr2 = find_pattern_with_mask(ptr2, mem_size2, pattern, mask, sizeof(pattern));
                        if (ptr2) {
                            DPRINTF("IGR: found padOpen call at 0x%08x\n", (int)ptr2);

                            patched = 1;

                            fncall = (u32)ptr2;

                            // Get PadOpen call Jump Instruction type. (JAL or J)
                            inst = (ptr2[0] & 0x0f000000);

                            // Get Hook_PadOpen call Instruction code
                            if (padopen_patterns[i].version == IGR_LIBPAD_V1) {
                                DPRINTF("IGR: Hook_scePadPortOpen addr 0x%08x\n", (int)Hook_scePadPortOpen);
                                inst |= 0x03ffffff & ((u32)Hook_scePadPortOpen >> 2);
                            } else {
                                DPRINTF("IGR: Hook_scePad2CreateSocket addr 0x%08x\n", (int)Hook_scePad2CreateSocket);
                                inst |= 0x03ffffff & ((u32)Hook_scePad2CreateSocket >> 2);
                            }

                            DPRINTF("IGR: patching padopen call at addr 0x%08x with opcode %08x\n", (int)fncall, (int)inst);
                            // Overwrite the original PadOpen function call with our function call
                            _sw(inst, fncall);
                        }
                    }

                    if (!patched) {
                        DPRINTF("IGR: 2nd padOpen patch attempt...\n");

                        // Make pattern with function address saved above
                        pattern[0] = (u32)ptr;
                        mask[0] = 0xffffffff;

                        DPRINTF("IGR: searching opcode %08x witk mask %08x\n", (int)pattern[0], (int)mask[0]);

                        // Search & patch for PadOpen function address
                        ptr2 = (u32 *)0x00100000;
                        while (ptr2) {
                            mem_size2 = 0x01ff0000 - (u32)ptr2;

                            ptr2 = find_pattern_with_mask(ptr2, mem_size2, pattern, mask, sizeof(pattern));
                            if (ptr2) {
                                DPRINTF("IGR: found padOpen call at 0x%08x\n", (int)ptr2);

                                patched = 1;

                                fncall = (u32)ptr2;

                                // Get Hook_PadOpen function address
                                if (padopen_patterns[i].version == IGR_LIBPAD_V1) {
                                    DPRINTF("IGR: Hook_scePadPortOpen addr 0x%08x\n", (int)Hook_scePadPortOpen);
                                    inst = (u32)Hook_scePadPortOpen;
                                } else {
                                    DPRINTF("IGR: Hook_scePad2CreateSocket addr 0x%08x\n", (int)Hook_scePad2CreateSocket);
                                    inst = (u32)Hook_scePad2CreateSocket;
                                }

                                DPRINTF("IGR: patching padopen call at addr 0x%08x with opcode %08x\n", (int)fncall, (int)inst);
                                // Overwrite the original PadOpen function address with our function address
                                _sw(inst, fncall);
                            }
                        }
                    }
                } else {
                    DPRINTF("IGR: no hooking requested, breaking loop...\n");
                    // Hooking is not required and padOpen function was found, so stop searching
                    break;
                }

                // Increment search pointer
                //ptr += padopen_patterns[i].size;
                ptr += (padopen_patterns[i].size >> 2);
            }
        }

        // If a padOpen function call was patched or ( hooking is not required and a padOpen function was found ), so stop the libpad version search loop
        if (patched == 1 || (mode == PADOPEN_CHECK && found == 1)) {
            DPRINTF("IGR: job done exiting...\n");
            break;
        }
    }

    // Done
    if (!DisableDebug)
        GS_BGCOLOUR = 0x000000; //Black

    return patched;
}

