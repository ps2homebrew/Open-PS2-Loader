#include <defs.h>
#include <errno.h>
#include <stdio.h>
#include <dmacman.h>
#include <dev9.h>
#include <intrman.h>
#include <loadcore.h>
#include <modload.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <irx.h>

#include <smapregs.h>
#include <speedregs.h>

#include <smstcpip.h>

#include "main.h"
#include "xfer.h"

/*  There is a difference in how the transmissions are made,
    between this driver and the SONY original.

    SONY:
        1. NETDEV calls SMAP's xmit callback.
        2. SMAP's xmit event handler issues that XMIT event (for the main thread).
        3. XMIT event copies the frames to the Tx buffer and sets up the BDs.
        4. Tx DNV handler is called (always, regardless of what event it is being handled!).
        5. Interrupts are re-enabled, except for TXDNV.
        6. If there are frames to transmit, write to TX_GNP_0 and enable the TXDNV interrupt

        The TXDNV interrupt is hence always disabled, but only enabled when a frame(s) is sent.
        Perhaps there is a lack of a check on the TXEND interrupt because it is polling on the TX BDs before every transmission.

        I don't know how to test this, but it seems like the TXDNV interrupt is asserted for as long as
        there is no valid frame to transmit, hence this design by SONY.

    OPL:
        1. Network stack calls SMAPSendPacket().
        2. TX_GNP_0 is checked and waited on (Tx channel 0 is in single mode)
        3. Frame is copied into the Tx buffer and a BD is set up.
        4. TX_GNP_0 is written to. */

#define DEV9_SMAP_ALL_INTR_MASK (SMAP_INTR_EMAC3 | SMAP_INTR_RXEND | SMAP_INTR_TXEND | SMAP_INTR_RXDNV | SMAP_INTR_TXDNV)
// Unlike the SONY original, the RXDNV interrupt is not handled as statistics are not recorded.
// For the sake of simplicity, Tx channel 0 is operated in single-mode. Do not handle TXDNV.
#define DEV9_SMAP_INTR_MASK     (SMAP_INTR_EMAC3 | SMAP_INTR_RXEND)
// The Tx interrupt events are handled separately
#define DEV9_SMAP_INTR_MASK2    (SMAP_INTR_EMAC3 | SMAP_INTR_RXEND)

struct SmapDriverData SmapDriverData;

static unsigned int EnableAutoNegotiation = 1;
static unsigned int EnablePinStrapConfig = 0;
static unsigned int SmapConfiguration = 0x5E0;

static void _smap_write_phy(volatile u8 *emac3_regbase, unsigned int address, u16 value)
{
    u32 PHYRegisterValue;
    unsigned int i;

    PHYRegisterValue = (address & SMAP_E3_PHY_REG_ADDR_MSK) | SMAP_E3_PHY_WRITE | ((SMAP_DsPHYTER_ADDRESS & SMAP_E3_PHY_ADDR_MSK) << SMAP_E3_PHY_ADDR_BITSFT);
    PHYRegisterValue |= ((u32)value) << SMAP_E3_PHY_DATA_BITSFT;

    i = 0;
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_STA_CTRL, PHYRegisterValue);

    for (; !(SMAP_EMAC3_GET32(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP); i++) {
        DelayThread(1000);
        if (i >= 100)
            break;
    }

    // if(i>=100) printf("smap: %s: > %d ms\n", "_smap_write_phy", i);
}

static u16 _smap_read_phy(volatile u8 *emac3_regbase, unsigned int address)
{
    unsigned int i;
    u32 value, PHYRegisterValue;
    u16 result;

    PHYRegisterValue = (address & SMAP_E3_PHY_REG_ADDR_MSK) | SMAP_E3_PHY_READ | ((SMAP_DsPHYTER_ADDRESS & SMAP_E3_PHY_ADDR_MSK) << SMAP_E3_PHY_ADDR_BITSFT);

    i = 0;
    result = 0;
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_STA_CTRL, PHYRegisterValue);

    do {
        if (SMAP_EMAC3_GET32(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP) {
            if (SMAP_EMAC3_GET32(SMAP_R_EMAC3_STA_CTRL) & SMAP_E3_PHY_OP_COMP) {
                if ((value = SMAP_EMAC3_GET32(SMAP_R_EMAC3_STA_CTRL)) & SMAP_E3_PHY_OP_COMP) {
                    result = (u16)(value >> SMAP_E3_PHY_DATA_BITSFT);
                    break;
                }
            }
        }

        DelayThread(1000);
        i++;
    } while (i < 100);

    if (i >= 100)
        printf("smap: %s: > %u ms\n", "_smap_read_phy", i);

    return result;
}

static inline void RestartAutoNegotiation(volatile u8 *emac3_regbase, unsigned short int bmsr)
{
    // DEBUG_PRINTF("smap: restarting auto nego (BMCR=0x%x, BMSR=0x%x)\n", _smap_read_phy(emac3_regbase, SMAP_DsPHYTER_BMCR), bmsr);
    _smap_write_phy(emac3_regbase, SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_ANEN | SMAP_PHY_BMCR_RSAN);
}

static int InitPHY(struct SmapDriverData *SmapDrivPrivData)
{
    int i, result;
    unsigned int LinkSpeed100M, LinkFDX, FlowControlEnabled, AutoNegoRetries;
    u32 emac3_value;
    u16 RegDump[6], value, value2;
    volatile u8 *emac3_regbase;

    LinkSpeed100M = 0;
    // DEBUG_PRINTF("smap: Resetting PHY\n");

    _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_RST);
    for (i = 0; _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR) & SMAP_PHY_BMCR_RST; i++) {
        if (i <= 0) {
            DEBUG_PRINTF("smap: PHY reset error\n");
            return -1;
        }
        if (SmapDrivPrivData->NetDevStopFlag)
            return 0;

        DelayThread(1000);
    }

    if (!EnableAutoNegotiation) {
        // DEBUG_PRINTF("smap: no auto mode (conf=0x%x)\n", SmapConfiguration);

        LinkSpeed100M = 0 < (SmapConfiguration & 0x180); /* Toggles between SMAP_PHY_BMCR_10M and SMAP_PHY_BMCR_100M. */
        value = LinkSpeed100M << 13;
        if (SmapConfiguration & 0x140)
            value |= SMAP_PHY_BMCR_DUPM;
        _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR, value);

    WaitLink:
        DEBUG_PRINTF("smap: Waiting Valid Link for %dMbps\n", LinkSpeed100M ? 100 : 10);

        i = 0;
        while (1) {
            DelayThread(200000);
            if (SmapDrivPrivData->NetDevStopFlag)
                return 0;
            i++;
            if (_smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMSR) & SMAP_PHY_BMSR_LINK)
                break;
            if (i >= 5)
                SmapDrivPrivData->LinkStatus = 0;
        }

        SmapDrivPrivData->LinkStatus = 1;
    } else {
        if (!EnablePinStrapConfig) {
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR, 0);
            value = _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMSR) & 0xFFFF;
            if (!(value & 0x4000))
                SmapConfiguration = SmapConfiguration & 0xFFFFFEFF; /* 100Base-TX FDX */
            if (!(value & 0x2000))
                SmapConfiguration = SmapConfiguration & 0xFFFFFF7F; /* 100Base-TX HDX */
            if (!(value & 0x1000))
                SmapConfiguration = SmapConfiguration & 0xFFFFFFBF; /* 10Base-TX FDX */
            if (!(value & 0x0800))
                SmapConfiguration = SmapConfiguration & 0xFFFFFFDF; /* 10Base-TX HDX */

            DEBUG_PRINTF("smap: no strap mode (conf=0x%x, bmsr=0x%x)\n", SmapConfiguration, value);

            value = _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_ANAR);
            value = (SmapConfiguration & 0x5E0) | (value & 0x1F);
            DEBUG_PRINTF("smap: anar=0x%x\n", value);
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_ANAR, value);
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_ANEN | SMAP_PHY_BMCR_RSAN);
        } else {
            if (!(_smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR) & SMAP_PHY_BMCR_ANEN)) {
                goto WaitLink;
            }
        }

        DEBUG_PRINTF("smap: auto mode (BMCR=0x%x ANAR=0x%x)\n", _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR) & 0xFFFF, _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_ANAR) & 0xFFFF);

    RepeatAutoNegoProcess:
        for (AutoNegoRetries = 0; AutoNegoRetries < 3; AutoNegoRetries++) {
            for (i = 0; i < 3; i++) {
                DelayThread(1000000);
                if (SmapDrivPrivData->NetDevStopFlag)
                    return 0;
            }

            value = _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMSR);
            if ((value & (SMAP_PHY_BMSR_ANCP | 0x10)) == SMAP_PHY_BMSR_ANCP) { /* 0x30: SMAP_PHY_BMSR_ANCP and Remote fault. */
                /* This seems to be checking for the link-up status. */
                for (i = 0; !(_smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMSR) & SMAP_PHY_BMSR_LINK); i++) {
                    DelayThread(200000);
                    if (SmapDrivPrivData->NetDevStopFlag)
                        return 0;
                    if (i >= 20)
                        break;
                }

                if (i < 20) {
                    /* Auto negotiaton completed successfully. */
                    SmapDrivPrivData->LinkStatus = 1;
                    break;
                } else
                    RestartAutoNegotiation(SmapDrivPrivData->emac3_regbase, value);
            } else
                RestartAutoNegotiation(SmapDrivPrivData->emac3_regbase, value);
        }

        /* If automatic negotiation fails, manually figure out which speed and duplex mode to use. */
        if (AutoNegoRetries >= 3) {
            // DEBUG_PRINTF("smap: waiting valid link for 100Mbps Half-Duplex\n");

            _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_100M);
            DelayThread(1000000);
            if (SmapDrivPrivData->NetDevStopFlag)
                return 0;

            for (i = 0; !(_smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMSR) & SMAP_PHY_BMSR_LINK); i++) {
                DelayThread(100000);
                if (SmapDrivPrivData->NetDevStopFlag)
                    return 0;
                if (i >= 30)
                    break;
            }

            if (i >= 30) {
                // DEBUG_PRINTF("smap: waiting valid link for 10Mbps Half-Duplex\n");

                _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR, SMAP_PHY_BMCR_10M);
                DelayThread(1000000);
                if (SmapDrivPrivData->NetDevStopFlag)
                    return 0;

                for (i = 0; !(_smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMSR) & SMAP_PHY_BMSR_LINK); i++) {
                    DelayThread(100000);
                    if (SmapDrivPrivData->NetDevStopFlag)
                        return 0;
                    if (i >= 30)
                        break;
                }

                if (i >= 0x1E)
                    goto RepeatAutoNegoProcess;
                else
                    SmapDrivPrivData->LinkStatus = 1;
            } else
                SmapDrivPrivData->LinkStatus = 1;
        }
    }

    for (i = 0; i < 6; i++)
        RegDump[i] = _smap_read_phy(SmapDrivPrivData->emac3_regbase, i);

    // DEBUG_PRINTF("smap: PHY: %04x %04x %04x %04x %04x %04x\n", RegDump[SMAP_DsPHYTER_BMCR], RegDump[SMAP_DsPHYTER_BMSR], RegDump[SMAP_DsPHYTER_PHYIDR1], RegDump[SMAP_DsPHYTER_PHYIDR2], RegDump[SMAP_DsPHYTER_ANAR], RegDump[SMAP_DsPHYTER_ANLPAR]);

    if (RegDump[SMAP_DsPHYTER_PHYIDR1] == SMAP_PHY_IDR1_VAL && (RegDump[SMAP_DsPHYTER_PHYIDR2] & SMAP_PHY_IDR2_MSK) == SMAP_PHY_IDR2_VAL) {
        if (EnableAutoNegotiation) {
            _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_FCSCR);
            _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_RECR);
            DelayThread(500000);
            value = _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_FCSCR);
            value2 = _smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_RECR);
            if ((value2 != 0) || (value >= 0x11)) {
                // DEBUG_PRINTF("smap: FCSCR=%d RECR=%d\n", value, value2);
                _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMCR, 0);
                goto WaitLink;
            }
        }

        DEBUG_PRINTF("smap: PHY chip: DP83846A%d\n", (RegDump[SMAP_DsPHYTER_PHYIDR2] & SMAP_PHY_IDR2_REV_MSK) + 1);

        if (!EnableAutoNegotiation) {
            if ((RegDump[SMAP_DsPHYTER_BMCR] & (SMAP_PHY_BMCR_DUPM | SMAP_PHY_BMCR_100M)) == 0) {
                _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_10BTSCR, 0x104);
            }
        } else {
            if ((RegDump[SMAP_DsPHYTER_ANAR] & 0x1E0) == 0x20) {
                _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_10BTSCR, 0x104);
            }
        }

        if ((RegDump[SMAP_DsPHYTER_PHYIDR2] & SMAP_PHY_IDR2_REV_MSK) == 0) {
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, 0x13, 1);
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_PHYCTRL, 0x1898);
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, 0x1F, 0);
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, 0x1D, 0x5040);
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, 0x1E, 0x8C);
            _smap_write_phy(SmapDrivPrivData->emac3_regbase, 0x13, 0);
        }
    }

    FlowControlEnabled = 0;
    if (RegDump[SMAP_DsPHYTER_BMCR] & SMAP_PHY_BMCR_ANEN) {
        value = RegDump[SMAP_DsPHYTER_ANAR] & RegDump[SMAP_DsPHYTER_ANLPAR];
        LinkSpeed100M = 0 < (value & 0x180);
        LinkFDX = 0 < (value & 0x140);
        if (LinkFDX)
            FlowControlEnabled = 0 < (value & 0x400);
    } else {
        LinkSpeed100M = RegDump[SMAP_DsPHYTER_BMCR] >> 13 & 1;
        LinkFDX = RegDump[SMAP_DsPHYTER_BMCR] >> 8 & 1;
        FlowControlEnabled = SmapConfiguration >> 10 & 1;
    }

    if (LinkSpeed100M)
        result = LinkFDX ? 8 : 4;
    else
        result = LinkFDX ? 2 : 1;

    SmapDrivPrivData->LinkMode = result;
    if (FlowControlEnabled)
        SmapDrivPrivData->LinkMode |= 0x40;

    DEBUG_PRINTF("smap: %s %s Duplex Mode %s Flow Control\n", LinkSpeed100M ? "100BaseTX" : "10BaseT", LinkFDX ? "Full" : "Half", FlowControlEnabled ? "with" : "without");

    emac3_regbase = SmapDrivPrivData->emac3_regbase;
    emac3_value = SMAP_EMAC3_GET32(SMAP_R_EMAC3_MODE1) & 0x67FFFFFF;
    if (LinkFDX)
        emac3_value |= SMAP_E3_FDX_ENABLE;
    if (FlowControlEnabled)
        emac3_value |= SMAP_E3_FLOWCTRL_ENABLE | SMAP_E3_ALLOW_PF;
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE1, emac3_value);

    return 0;
}

// Checks the status of the Ethernet link
static void CheckLinkStatus(struct SmapDriverData *SmapDrivPrivData)
{
    if (!(_smap_read_phy(SmapDrivPrivData->emac3_regbase, SMAP_DsPHYTER_BMSR) & SMAP_PHY_BMSR_LINK)) {
        // Link lost
        SmapDrivPrivData->LinkStatus = 0;
        InitPHY(SmapDrivPrivData);
    }
}

// This timer callback starts the Ethernet link check event.
static unsigned int LinkCheckTimerCB(struct SmapDriverData *SmapDrivPrivData)
{
    CheckLinkStatus(SmapDrivPrivData);
    return SmapDrivPrivData->LinkCheckTimer.lo;
}

static int Dev9IntrCb(int flag)
{
    unsigned int IntrReg;
    USE_SPD_REGS;
    volatile u8 *smap_regbase, *emac3_regbase;

#if USE_GP_REGISTER
    void *OldGP;

    OldGP = SetModuleGP();
#endif

    emac3_regbase = SmapDriverData.emac3_regbase;
    smap_regbase = SmapDriverData.smap_regbase;
    while ((IntrReg = SPD_REG16(SPD_R_INTR_STAT) & DEV9_SMAP_INTR_MASK) != 0) {
        if (IntrReg & SMAP_INTR_EMAC3) {
            SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_EMAC3;
            SMAP_EMAC3_SET32(SMAP_R_EMAC3_INTR_STAT, SMAP_E3_INTR_TX_ERR_0 | SMAP_E3_INTR_SQE_ERR_0 | SMAP_E3_INTR_DEAD_0);
        }
        if (IntrReg & SMAP_INTR_RXEND) {
            SMAP_REG16(SMAP_R_INTR_CLR) = SMAP_INTR_RXEND;
            HandleRxIntr(&SmapDriverData);
        }
    }

#if USE_GP_REGISTER
    SetGP(OldGP);
#endif

    return 0;
}

static void Dev9PreDmaCbHandler(int bcr, int dir)
{
    volatile u8 *smap_regbase;
    unsigned short int SliceCount;

    smap_regbase = SmapDriverData.smap_regbase;
    SliceCount = bcr >> 16;
    if (dir != DMAC_TO_MEM) {
        SMAP_REG16(SMAP_R_TXFIFO_SIZE) = SliceCount;
        SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_DMAEN;
    } else {
        SMAP_REG16(SMAP_R_RXFIFO_SIZE) = SliceCount;
        SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_DMAEN;
    }
}

static void Dev9PostDmaCbHandler(int bcr, int dir)
{
    volatile u8 *smap_regbase;

    smap_regbase = SmapDriverData.smap_regbase;
    if (dir != DMAC_TO_MEM) {
        while (SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_DMAEN) {
        };
    } else {
        while (SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_DMAEN) {
        };
    }
}

int SMAPStart(void)
{
    int result;
    volatile u8 *emac3_regbase;

#if USE_GP_REGISTER
    void *OldGP;

    OldGP = SetModuleGP();
#endif

    if (!SmapDriverData.SmapIsInitialized) {
        emac3_regbase = SmapDriverData.emac3_regbase;

        dev9IntrEnable(DEV9_SMAP_INTR_MASK2);

        // Initialize the PHY, only if there's no valid link status. It should have already been previously initialized successfully by the previous instance of SMAP.
        result = (!(_smap_read_phy(emac3_regbase, SMAP_DsPHYTER_BMSR) & SMAP_PHY_BMSR_LINK)) ? InitPHY(&SmapDriverData) : 0;
        if (result == 0 && !SmapDriverData.NetDevStopFlag) {
            SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE0, SMAP_E3_TXMAC_ENABLE | SMAP_E3_RXMAC_ENABLE);
            DelayThread(10000);
            SmapDriverData.SmapIsInitialized = 1;

            if (!SmapDriverData.EnableLinkCheckTimer) {
                USec2SysClock(5000000, &SmapDriverData.LinkCheckTimer); // Since there isn't a thread that allows the link checking code to determine whether frames were received or not and to check the link status, just reduce the polling frequency.
                SetAlarm(&SmapDriverData.LinkCheckTimer, (void *)&LinkCheckTimerCB, &SmapDriverData);
                SmapDriverData.EnableLinkCheckTimer = 1;
            }
        } else
            SmapDriverData.NetDevStopFlag = 0;
    }

#if USE_GP_REGISTER
    SetGP(OldGP);
#endif

    return 0;
}

void SMAPStop(void)
{
    volatile u8 *emac3_regbase;

#if USE_GP_REGISTER
    void *OldGP;

    OldGP = SetModuleGP();
#endif
    SmapDriverData.NetDevStopFlag = 1;

    if (SmapDriverData.SmapIsInitialized) {
        emac3_regbase = SmapDriverData.emac3_regbase;

        dev9IntrDisable(DEV9_SMAP_INTR_MASK2);
        SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE0, 0);
        SmapDriverData.NetDevStopFlag = 0;
        SmapDriverData.SmapIsInitialized = 0;
    }

#if USE_GP_REGISTER
    SetGP(OldGP);
#endif
}

static int ParseSmapConfiguration(const char *cmd, unsigned int *configuration)
{
    const char *CmdStart, *DigitStart;
    unsigned int result, base, character, value;

    DigitStart = CmdStart = cmd;
    base = 10;

    if (CmdStart[0] == '0') {
        if (CmdStart[1] != '\0') {
            if (CmdStart[1] == 'x') {
                DigitStart += 2;
                base = 16;
            } else {
                DigitStart++;
            }
        }
    }
    if (DigitStart[0] == '\0') {
        goto fail_end;
    }

    result = 0;
    character = DigitStart[0];
    do {
        if (character - '0' < 10) {
            value = character - '0';
        } else if (character - 'a' < 6) {
            value = character - 'a' - 0x57;
        } else
            goto fail_end;

        if (value >= base)
            goto fail_end;

        result = result * base + value;
    } while ((character = *(++DigitStart)) != '\0');
    *configuration = result;

    return 0;
fail_end:
    // printf("smap: %s: %s - invalid digit\n", "scan_number", CmdStart);
    return -1;
}

int smap_init(int argc, char *argv[])
{
    int result, i;
    unsigned short int eeprom_data[4], checksum16;
    unsigned int mac_address;
    USE_SPD_REGS;
    USE_SMAP_REGS;
    USE_SMAP_EMAC3_REGS;
    USE_SMAP_TX_BD;
    USE_SMAP_RX_BD;

    checksum16 = 0;
    while (argc > 0) {
        if (strcmp("-auto", *argv) == 0) {
            EnableAutoNegotiation = 1;
        } else if (strcmp("-no_auto", *argv) == 0) {
            EnableAutoNegotiation = 0;
        } else if (strcmp("-strap", *argv) == 0) {
            EnablePinStrapConfig = 1;
        } else if (strcmp("-no_strap", *argv) == 0) {
            EnablePinStrapConfig = 0;
        } else {
            if (ParseSmapConfiguration(*argv, &SmapConfiguration) != 0)
                return -1;
        }

        argc--;
        argv++;
    }

    /*
    if (argc != 0)
        return -1;
*/

    SmapDriverData.smap_regbase = smap_regbase;
    SmapDriverData.emac3_regbase = emac3_regbase;
    if ((SPD_REG16(SPD_R_REV_3) & SPD_CAPS_SMAP) == 0)
        return -1;
    if (SPD_REG16(SPD_R_REV_1) < 0x11)
        return -6; // Minimum: revision 17, ES2.

    dev9IntrDisable(DEV9_SMAP_ALL_INTR_MASK);

    /* Reset FIFOs. */
    SMAP_REG8(SMAP_R_TXFIFO_CTRL) = SMAP_TXFIFO_RESET;
    for (i = 9; SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_RESET; i--) {
        if (i <= 0)
            return -2;
        DelayThread(1000);
    }

    SMAP_REG8(SMAP_R_RXFIFO_CTRL) = SMAP_RXFIFO_RESET;
    for (i = 9; SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_RESET; i--) {
        if (i <= 0)
            return -3;
        DelayThread(1000);
    }

    SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE0, SMAP_E3_SOFT_RESET);
    for (i = 9; SMAP_EMAC3_GET32(SMAP_R_EMAC3_MODE0) & SMAP_E3_SOFT_RESET; i--) {
        if (i <= 0)
            return -4;
        DelayThread(1000);
    }

    SMAP_REG8(SMAP_R_BD_MODE) = 0;
    for (i = 0; i < SMAP_BD_MAX_ENTRY; i++) {
        tx_bd[i].ctrl_stat = 0;
        tx_bd[i].reserved = 0;
        tx_bd[i].length = 0;
        tx_bd[i].pointer = 0;
    }

    for (i = 0; i < SMAP_BD_MAX_ENTRY; i++) {
        rx_bd[i].ctrl_stat = SMAP_BD_RX_EMPTY;
        rx_bd[i].reserved = 0;
        rx_bd[i].length = 0;
        rx_bd[i].pointer = 0;
    }

    SMAP_REG16(SMAP_R_INTR_CLR) = DEV9_SMAP_ALL_INTR_MASK;

    /* Retrieve the MAC address and verify it's integrity. */
    bzero(eeprom_data, 8);
    if ((result = dev9GetEEPROM(eeprom_data)) < 0) {
        return (result == -1 ? -7 : -4);
    }

    for (i = 0; i < 3; i++)
        checksum16 += eeprom_data[i];
    if (eeprom_data[0] == 0 && eeprom_data[1] == 0 && eeprom_data[2] == 0) {
        return -5;
    }
    if (checksum16 != eeprom_data[3])
        return -5;

    SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE1, SMAP_E3_FDX_ENABLE | SMAP_E3_IGNORE_SQE | SMAP_E3_MEDIA_100M | SMAP_E3_RXFIFO_2K | SMAP_E3_TXFIFO_1K | SMAP_E3_TXREQ0_SINGLE | SMAP_E3_TXREQ1_SINGLE);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_TxMODE1, 7 << SMAP_E3_TX_LOW_REQ_BITSFT | 15 << SMAP_E3_TX_URG_REQ_BITSFT);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_RxMODE, SMAP_E3_RX_STRIP_PAD | SMAP_E3_RX_STRIP_FCS | SMAP_E3_RX_INDIVID_ADDR | SMAP_E3_RX_BCAST | SMAP_E3_RX_MCAST);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_INTR_STAT, SMAP_E3_INTR_TX_ERR_0 | SMAP_E3_INTR_SQE_ERR_0 | SMAP_E3_INTR_DEAD_0);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_INTR_ENABLE, SMAP_E3_INTR_TX_ERR_0 | SMAP_E3_INTR_SQE_ERR_0 | SMAP_E3_INTR_DEAD_0);

    mac_address = (u16)(eeprom_data[0] >> 8 | eeprom_data[0] << 8);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_ADDR_HI, mac_address);

    mac_address = ((u16)(eeprom_data[1] >> 8 | eeprom_data[1] << 8) << 16) | (u16)(eeprom_data[2] >> 8 | eeprom_data[2] << 8);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_ADDR_LO, mac_address);

    SMAP_EMAC3_SET32(SMAP_R_EMAC3_PAUSE_TIMER, 0xFFFF);

    SMAP_EMAC3_SET32(SMAP_R_EMAC3_GROUP_HASH1, 0);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_GROUP_HASH2, 0);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_GROUP_HASH3, 0);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_GROUP_HASH4, 0);

    SMAP_EMAC3_SET32(SMAP_R_EMAC3_INTER_FRAME_GAP, 4);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_TX_THRESHOLD, 12 << SMAP_E3_TX_THRESHLD_BITSFT);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_RX_WATERMARK, 16 << SMAP_E3_RX_LO_WATER_BITSFT | 128 << SMAP_E3_RX_HI_WATER_BITSFT);

    // Unlike the SONY original, register the interrupt handler for only RXEND and EMAC3.
    dev9RegisterIntrCb(5, &Dev9IntrCb); /* RXEND */
    dev9RegisterIntrCb(6, &Dev9IntrCb); /* EMAC3 */

    dev9RegisterPreDmaCb(1, &Dev9PreDmaCbHandler);
    dev9RegisterPostDmaCb(1, &Dev9PostDmaCbHandler);

    return 0;
}

int SMAPGetMACAddress(unsigned char *buffer)
{
    unsigned int mac_address_lo, mac_address_hi;
    volatile u8 *emac3_regbase;

    emac3_regbase = SmapDriverData.emac3_regbase;

    mac_address_hi = SMAP_EMAC3_GET32(SMAP_R_EMAC3_ADDR_HI);
    mac_address_lo = SMAP_EMAC3_GET32(SMAP_R_EMAC3_ADDR_LO);
    buffer[0] = mac_address_hi >> 8;
    buffer[1] = mac_address_hi;
    buffer[2] = mac_address_lo >> 24;
    buffer[3] = mac_address_lo >> 16;
    buffer[4] = mac_address_lo >> 8;
    buffer[5] = mac_address_lo;

    return 0;
}
