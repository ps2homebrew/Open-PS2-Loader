#ifndef IMPORTS_H
#define IMPORTS_H

#define IMPORT_BIN2C(_n) \
    extern void *_n[];   \
    extern int size_##_n
// Try to keep this list alphabetical

/* Irx modules */

IMPORT_BIN2C(apemodpatch_irx);

IMPORT_BIN2C(audsrv_irx);

IMPORT_BIN2C(bdm_irx);

IMPORT_BIN2C(bdm_ata_cdvdman_irx);

IMPORT_BIN2C(bdm_cdvdman_irx);

IMPORT_BIN2C(bdm_mcemu_irx);

IMPORT_BIN2C(bdmevent_irx);

IMPORT_BIN2C(bdmfs_fatfs_irx);

IMPORT_BIN2C(bt_pademu_irx);

IMPORT_BIN2C(cdvdfsv_irx);

IMPORT_BIN2C(cleareffects_irx);

IMPORT_BIN2C(deci2_img);

IMPORT_BIN2C(drvtif_irx);

IMPORT_BIN2C(drvtif_ingame_irx);

IMPORT_BIN2C(ds34bt_irx);

IMPORT_BIN2C(ds34usb_irx);

IMPORT_BIN2C(filexio_irx);

IMPORT_BIN2C(genvmc_irx);

IMPORT_BIN2C(hdd_cdvdman_irx);

IMPORT_BIN2C(hdd_hdpro_cdvdman_irx);

IMPORT_BIN2C(lwnbdsvr_irx);

IMPORT_BIN2C(hdd_mcemu_irx);

IMPORT_BIN2C(hdpro_atad_irx);

IMPORT_BIN2C(httpclient_irx);

IMPORT_BIN2C(IEEE1394_bd_irx);

IMPORT_BIN2C(iLinkman_irx);

IMPORT_BIN2C(imgdrv_irx);

IMPORT_BIN2C(ingame_smstcpip_irx);

IMPORT_BIN2C(iomanx_irx);

IMPORT_BIN2C(ioptrap_irx);

IMPORT_BIN2C(isofs_irx);

IMPORT_BIN2C(iremsndpatch_irx);

IMPORT_BIN2C(libsd_irx);

IMPORT_BIN2C(mcman_irx);

IMPORT_BIN2C(mcserv_irx);

IMPORT_BIN2C(nbns_irx);

IMPORT_BIN2C(netman_irx);

IMPORT_BIN2C(f2techioppatch_irx);

IMPORT_BIN2C(padman_irx);

IMPORT_BIN2C(poweroff_irx);

IMPORT_BIN2C(ppctty_irx);

IMPORT_BIN2C(ps2atad_irx);

IMPORT_BIN2C(ps2dev9_irx);

IMPORT_BIN2C(ps2fs_irx);

IMPORT_BIN2C(ps2hdd_irx);

IMPORT_BIN2C(ps2ips_irx);

IMPORT_BIN2C(ps2ip_irx);

IMPORT_BIN2C(ps2link_irx);

IMPORT_BIN2C(resetspu_irx);

IMPORT_BIN2C(sio2man_irx);

IMPORT_BIN2C(mx4sio_bd_irx);

IMPORT_BIN2C(smap_irx);

IMPORT_BIN2C(smap_ingame_irx);

IMPORT_BIN2C(smb_mcemu_irx);

IMPORT_BIN2C(smb_cdvdman_irx);

IMPORT_BIN2C(smbinit_irx);

IMPORT_BIN2C(smbman_irx);

IMPORT_BIN2C(smsutils_irx);

IMPORT_BIN2C(tifinet_irx);

IMPORT_BIN2C(tifinet_ingame_irx);

IMPORT_BIN2C(udptty_irx);

IMPORT_BIN2C(udptty_ingame_irx);

IMPORT_BIN2C(udnl_irx);

IMPORT_BIN2C(usbd_irx);

IMPORT_BIN2C(usbmass_bd_irx);

IMPORT_BIN2C(usb_pademu_irx);

IMPORT_BIN2C(xhdd_irx);

/*--    Theme Sound Effects    ----------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------------------------------------------------*/
IMPORT_BIN2C(boot_adp);

IMPORT_BIN2C(bd_connect_adp);

IMPORT_BIN2C(bd_disconnect_adp);

IMPORT_BIN2C(cancel_adp);

IMPORT_BIN2C(confirm_adp);

IMPORT_BIN2C(cursor_adp);

IMPORT_BIN2C(message_adp);

IMPORT_BIN2C(transition_adp);

/* icon images */

IMPORT_BIN2C(icon_sys);

IMPORT_BIN2C(icon_icn);

/* OPL Config */
IMPORT_BIN2C(conf_theme_OPL_cfg);

/* Raw Font */
IMPORT_BIN2C(poeveticanew_raw);

#endif
