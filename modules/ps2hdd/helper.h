/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: helper.h 1421 2007-07-07 01:56:53Z radad $
*/

#ifndef _HELPER_H
#define _HELPER_H

#define ATAD_MODE_READ 0x00
#define ATAD_MODE_WRITE 0x01

#define t_shddInfo ata_devinfo_t

#define atadInit ata_get_devinfo
#define atadDmaTransfer ata_device_dma_transfer
#define atadSceUnlock ata_device_sec_unlock
#define atadIdle ata_device_idle
#define atadSceIdentifyDrive ata_device_sce_security_init
#define atadGetStatus ata_device_smart_get_status
#define atadUpdateAttrib ata_device_smart_save_attr
#define atadFlushCache ata_device_flush_cache
#define atadIs48bit ata_device_is_48bit
#define atadSetTransferMode ata_device_set_transfer_mode

#endif
