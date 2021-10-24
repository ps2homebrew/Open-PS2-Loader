// sio2man hook code taken from opl mcemu:
/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
*/

#include "pademu.h"

#ifdef BT

#include "ds34bt.h"

#define PAD_INIT       ds34bt_init
#define PAD_GET_STATUS ds34bt_get_status
#define PAD_RESET      ds34bt_reset
#define PAD_GET_DATA   ds34bt_get_data
#define PAD_SET_RUMBLE ds34bt_set_rumble
#define PAD_SET_MODE   ds34bt_set_mode

#elif defined(USB)

#include "ds34usb.h"

#define PAD_INIT       ds34usb_init
#define PAD_GET_STATUS ds34usb_get_status
#define PAD_RESET      ds34usb_reset
#define PAD_GET_DATA   ds34usb_get_data
#define PAD_SET_RUMBLE ds34usb_set_rumble
#define PAD_SET_MODE   ds34usb_set_mode

#else
#error "must define mode"
#endif

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

typedef struct
{
    u8 mode;
    u8 mode_p;
    u8 mode_id;
    u8 mode_cfg;
    u8 mode_lock;
    u8 enabled;
    u8 vibration;
    u8 lrum;
    u8 rrum;
    u8 mask[4];
} pad_status_t;

#define DIGITAL_MODE 0x41
#define ANALOG_MODE  0x73
#define ANALOGP_MODE 0x79
#define CONFIG_MODE  0xF3

#define MAX_PORTS 4

#define PAD_STATE_RUNNING 0x08

IRX_ID("pademu", 1, 1);

PtrRegisterLibraryEntires pRegisterLibraryEntires; /* Pointer to RegisterLibraryEntires routine */
Sio2McProc pSio2man25, pSio2man51;                 /* Pointers to SIO2MAN routines */
pad_status_t pad[MAX_PORTS];

static u8 pad_inited = 0;
static u8 pad_enable = 0;
static u8 pad_options = 0;

static u8 mtap_enabled = 0;
static u8 mtap_inited = 0;
static u8 mtap_slot = 0;
static u8 mtap_port = 0;

int install_sio2hook();

int hookRegisterLibraryEntires(iop_library_t *lib);
void hookSio2man25(sio2_transfer_data_t *sd);
void hookSio2man51(sio2_transfer_data_t *sd);
void InstallSio2manHook(void *exp, int ver);

void pademu_hookSio2man(sio2_transfer_data_t *td, Sio2McProc sio2proc);
void pademu_setup(u8 ports, u8 vib);
void pademu(sio2_transfer_data_t *td);
void pademu_cmd(int port, u8 *in, u8 *out, u8 out_size);

void pademu_mtap(sio2_transfer_data_t *td);

extern struct irx_export_table _exp_pademu;

int _start(int argc, char *argv[])
{
    u8 pad_vibration = 0x03;

    pad_enable = 0x03;

    if (argc > 1) {
        pad_enable = argv[1][0];
        pad_vibration = argv[1][1];
        mtap_enabled = argv[1][2] & 1;
        mtap_port = (argv[1][2] >> 1) & 1;
        pad_options = (argv[1][2] >> 2) & 1; // enable workaround for fake ds3
    }

    if (RegisterLibraryEntries(&_exp_pademu) != 0) {
        return MODULE_NO_RESIDENT_END;
    }

    SetRebootTimeLibraryHandlingMode(&_exp_pademu, 2);

    u8 vmc = 0;

    if (argc > 1)
        vmc = argv[1][3];

    if (!vmc) {
        if (!install_sio2hook())
            return MODULE_NO_RESIDENT_END;
    }

    pademu_setup(pad_enable, pad_vibration);

    return MODULE_RESIDENT_END;
}

void _exit(int mode)
{
    PAD_RESET();
}

int install_sio2hook()
{
    register void *exp;

    /* looking for LOADCORE's library entry table */
    exp = GetExportTable("loadcore", 0x100);
    if (exp == NULL) {
        DPRINTF("Unable to find loadcore exports.\n");
        return 0;
    }

    /* hooking LOADCORE's RegisterLibraryEntires routine */
    pRegisterLibraryEntires = (PtrRegisterLibraryEntires)HookExportEntry(exp, 6, hookRegisterLibraryEntires);

    /* searching for a SIO2MAN export table */
    exp = GetExportTable("sio2man", 0x201);
    if (exp != NULL) {
        /* hooking SIO2MAN's routines */
        InstallSio2manHook(exp, 1);
    } else {
        DPRINTF("SIO2MAN exports not found.\n");
    }

    return 1;
}

void InstallSio2manHook(void *exp, int ver)
{
    /* hooking SIO2MAN entry #25 (used by MCMAN and old PADMAN) */
    pSio2man25 = HookExportEntry(exp, 25, hookSio2man25);
    /* hooking SIO2MAN entry #51 (used by MC2_* modules and PADMAN) */
    pSio2man51 = HookExportEntry(exp, 49 + (ver * 2), hookSio2man51);
}

/* Hook for the LOADCORE's RegisterLibraryEntires call */
int hookRegisterLibraryEntires(iop_library_t *lib)
{
    register int ret;

    if (!memcmp(lib->name, "sio2man", 8)) {
        ret = pRegisterLibraryEntires(lib);
        if (ret == 0) {
            ReleaseLibraryEntries((struct irx_export_table *)lib);
            /* hooking SIO2MAN's routines */
            InstallSio2manHook(&lib[1], GetExportTableSize(&lib[1]) >= 61);
        } else {
            DPRINTF("registering library %s failed, error %d\n", lib->name, ret);
            return ret;
        }
    }

    DPRINTF("registering library %s\n", lib->name);

    return pRegisterLibraryEntires(lib);
}

/* Hook for SIO2MAN entry #25 */
void hookSio2man25(sio2_transfer_data_t *sd)
{
    pademu_hookSio2man(sd, pSio2man25);
}

/* Hook for SIO2MAN entry #51 */
void hookSio2man51(sio2_transfer_data_t *sd)
{
    pademu_hookSio2man(sd, pSio2man51);
}

void pademu_hookSio2man(sio2_transfer_data_t *td, Sio2McProc sio2proc)
{
    register u32 ctrl, port1, port2;

    ctrl = td->regdata[0];
    port1 = td->regdata[0] & 0x03;
    port2 = td->regdata[1] & 0x03;

    if ((ctrl & 0xF0) == 0x40) {
        if (td->port_ctrl2[port1] == 0x00030064 && td->in[0] == 0x21 && mtap_enabled) {
            sio2proc = pademu_mtap;
        } else if (td->in[0] == 0x01) {
            if (port2 == 1) { // 2 sio cmds
                if (mtap_inited) {
                    if (pad[0].enabled) {
                        td->in[0] = 0x00;
                        sio2proc(td);
                        sio2proc = pademu;
                    }
                } else {
                    if (pad[0].enabled && pad[1].enabled) { // emulating 2 pads
                        sio2proc = pademu;
                    } else if (pad[0].enabled || pad[1].enabled) { // only one
                        if (pad[0].enabled) {
                            ctrl = 0;
                        } else if (pad[1].enabled) {
                            for (ctrl = 5; ctrl < td->in_size - 3; ctrl++) {
                                if (td->in[ctrl] == 0x01 && (td->in[ctrl + 1] & 0xF0) == 0x40 && td->in[ctrl + 2] == 0x00) {
                                    if (ctrl != 5 && ctrl != 9 && ctrl != 21)
                                        continue;
                                    else
                                        break;
                                }
                            }
                            if (ctrl + 3 == td->in_size) {
                                return;
                            }
                        }
                        td->in[ctrl] = 0x00;
                        sio2proc(td);
                        td->in[ctrl] = 0x01;
                        sio2proc = pademu;
                    }
                }
            } else {
                if (mtap_inited) {
                    if (mtap_port == port1) {
                        sio2proc = pademu;
                    }
                } else {
                    if (pad[port1].enabled) { // emulating this port
                        sio2proc = pademu;
                    }
                }
            }
        }
    }

    sio2proc(td);
}

void pademu_setup(u8 ports, u8 vib)
{
    u8 i;

    for (i = 0; i < MAX_PORTS; i++) {
        pad[i].mode = 0;
        pad[i].mode_p = 0;
        pad[i].mode_id = DIGITAL_MODE;
        pad[i].mode_cfg = 0;
        pad[i].mode_lock = 0;

        pad[i].enabled = ((ports >> i) & 1);
        pad[i].vibration = ((vib >> i) & 1);

        pad[i].mask[0] = 0xFF;
        pad[i].mask[1] = 0xFF;
        pad[i].mask[2] = 0x03;
        pad[i].mask[3] = 0x00;

        pad[i].lrum = 2;
        pad[i].rrum = 2;
    }
}

u8 pademu_data[6][6] =
    {
        {0x00, 0x00, 0x02, 0x00, 0x00, 0x5A},
        {0x03, 0x02, 0x00, 0x02, 0x01, 0x00},
        {0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
        {0x00, 0x00, 0x01, 0x01, 0x01, 0x14},
        {0x00, 0x00, 0x02, 0x00, 0x01, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

void pademu(sio2_transfer_data_t *td)
{
    int port;
    u8 port1, port2, cmd_size;
    u8 *in, *out;

    port1 = td->regdata[0] & 0x03;
    port2 = td->regdata[1] & 0x03;

    td->stat6c = 0x1100; //?
    td->stat70 = 0x0F;   //?

    if (!pad_inited) {
        pad_inited = PAD_INIT(pad_enable, pad_options);
    }

    if (port2 == 1) {
        // find next cmd
        for (cmd_size = 5; cmd_size < td->in_size - 3; cmd_size++) {
            if (td->in[cmd_size] == 0x01 && (td->in[cmd_size + 1] & 0xF0) == 0x40 && td->in[cmd_size + 2] == 0x00) {
                if (cmd_size != 5 && cmd_size != 9 && cmd_size != 21)
                    continue;
                else
                    break;
            }
        }

        if (cmd_size + 3 == td->in_size) {
            DPRINTF("Second cmd not found!\n");
            return;
        }

        if (mtap_inited) {
            if (mtap_port == 0) {
                in = td->in;
                out = td->out;
                port = 0;
            } else {
                in = (u8 *)&td->in[cmd_size];
                out = (u8 *)&td->out[cmd_size];
                port = 0;
                cmd_size = td->in_size - cmd_size;
            }
        } else {
            if (pad[0].enabled) {
                in = td->in;
                out = td->out;
                port = 0;

                if (pad[1].enabled) { // emulating ports 0 & 1
                    pademu_cmd(1, (u8 *)&td->in[cmd_size], (u8 *)&td->out[cmd_size], td->in_size - cmd_size);
                }
            } else { // emulating only port 1
                in = (u8 *)&td->in[cmd_size];
                out = (u8 *)&td->out[cmd_size];
                port = 1;
                cmd_size = td->in_size - cmd_size;
            }
        }
    } else {
        in = td->in;
        out = td->out;
        port = port1;
        cmd_size = td->in_size;

        if (mtap_inited) {
            port = mtap_slot;
        }
    }

    pademu_cmd(port, in, out, cmd_size);
}

void pademu_cmd(int port, u8 *in, u8 *out, u8 out_size)
{
    u8 i;

    mips_memset(out, 0x00, out_size);

    if (!(PAD_GET_STATUS(port) & PAD_STATE_RUNNING)) {
        pad[port].lrum = 2;
        pad[port].rrum = 2;
        return;
    }

    out[0] = 0xFF;
    out[1] = CONFIG_MODE;
    out[2] = 0x5A;

    switch (in[1]) {
        case 0x40: // set vref param
            mips_memcpy(&out[3], &pademu_data[0], 6);
            break;

        case 0x41: // query button mask
            if (pad[port].mode_id != DIGITAL_MODE) {
                out[3] = pad[port].mask[0];
                out[4] = pad[port].mask[1];
                out[5] = pad[port].mask[2];
                out[6] = pad[port].mask[3];
                out[7] = 0x00;
                out[8] = 0x5A;
            }
            break;

        case 0x43: // enter/exit config mode
            if (pad[port].mode_cfg) {
                pad[port].mode_cfg = in[3];
                break;
            }

            pad[port].mode_cfg = in[3];
        case 0x42: // read data
            if (in[1] == 0x42) {
                if (pad[port].vibration) { // disable/enable vibration
                    PAD_SET_RUMBLE(in[pad[port].lrum], in[pad[port].rrum], port);
                }
            }

            i = PAD_GET_DATA(&out[3], out_size - 3, port);

            if (pad[port].mode_lock == 0) { // mode unlocked
                if (pad[port].mode != i) {
                    pad[port].mode = i;

                    if (pad[port].mode)
                        pad[port].mode_id = ANALOG_MODE;
                    else
                        pad[port].mode_id = DIGITAL_MODE;
                }
            }

            out[1] = pad[port].mode_id;
            break;

        case 0x44: // set mode and lock
            pad[port].mode = in[3];
            pad[port].mode_lock = in[4];
            if (pad[port].mode) {
                if (pad[port].mode_p) {
                    pad[port].mode_id = ANALOGP_MODE;
                } else {
                    pad[port].mode_id = ANALOG_MODE;
                }
            } else {
                pad[port].mode_id = DIGITAL_MODE;
            }
            PAD_SET_MODE(pad[port].mode, pad[port].mode_lock, port);
            break;

        case 0x45: // query model and mode
            mips_memcpy(&out[3], &pademu_data[1], 6);
            out[5] = pad[port].mode;
            break;

        case 0x46: // query act
            if (in[3] == 0x00)
                mips_memcpy(&out[3], &pademu_data[2], 6);
            else
                mips_memcpy(&out[3], &pademu_data[3], 6);

            break;

        case 0x47: // query comb
            mips_memcpy(&out[3], &pademu_data[4], 6);
            break;

        case 0x4C: // query mode
            if (in[3] == 0x00)
                out[6] = 0x04;
            else
                out[6] = 0x07;

            break;

        case 0x4D: // set act align
            mips_memcpy(&out[3], &pademu_data[5], 6);

            for (i = 0; i < 6; i++) { // vibration
                if (in[3 + i] == 0x00)
                    pad[port].rrum = i + 3;

                if (in[3 + i] == 0x01)
                    pad[port].lrum = i + 3;
            }
            break;

        case 0x4F: // set button info
            pad[port].mode_id = ANALOGP_MODE;
            pad[port].mode_p = 1;

            out[8] = 0x5A;

            pad[port].mask[0] = in[3];
            pad[port].mask[1] = in[4];
            pad[port].mask[2] = in[5];
            pad[port].mask[3] = in[6];
            break;
    }
}

static u8 mtap_data[] = {
    0xff, 0x80, 0x5a, 0x00, 0x00, 0x5a};

#define MAX_SLOT 4

void pademu_mtap(sio2_transfer_data_t *td)
{
    u8 port1;

    port1 = td->regdata[0] & 0x01;

    mips_memset(td->out, 0x00, td->out_size);
    td->stat6c = 0x0001d100;

    if (port1 != mtap_port) {
        return;
    }

    switch (td->in[1]) {
        case 0x12: // returns slot number for pad
            mips_memcpy(td->out, &mtap_data, sizeof(mtap_data));
            td->out[3] = MAX_SLOT;
            td->stat6c = 0x00001100;
            mtap_inited = 1;
            break;

        case 0x13: // returns slot number for mc
            break;

        case 0x21: // changes slot for pad
            mips_memcpy(td->out, &mtap_data, sizeof(mtap_data));
            mtap_slot = td->out[5] = td->in[2]; // slot
            td->out[6] = 0x5a;
            td->stat6c = 0x00001100;
            break;

        case 0x22: // changes slot for mc
            break;
    }
}
