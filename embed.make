
ee_core/ee_core.elf: ee_core
	echo "-EE core"
	$(MAKE) $(IGS_FLAGS) $(PADEMU_FLAGS) $(EECORE_EXTRA_FLAGS) -C $<

$(EE_ASM_DIR)ee_core.s: ee_core/ee_core.elf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ eecore_elf

$(EE_ASM_DIR)udnl.s: $(UDNL_OUT) | $(EE_ASM_DIR)
	$(BIN2S) $(UDNL_OUT) $@ udnl_irx

modules/iopcore/imgdrv/imgdrv.irx: modules/iopcore/imgdrv
	$(MAKE) -C $<

$(EE_ASM_DIR)imgdrv.s: modules/iopcore/imgdrv/imgdrv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ imgdrv_irx

$(EE_ASM_DIR)eesync.s: $(PS2SDK)/iop/irx/eesync-nano.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ eesync_irx

modules/iopcore/cdvdman/bdm_cdvdman.irx: modules/iopcore/cdvdman
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_BDM=1 -C $< all

$(EE_ASM_DIR)bdm_cdvdman.s: modules/iopcore/cdvdman/bdm_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdm_cdvdman_irx

modules/iopcore/cdvdman/smb_cdvdman.irx: modules/iopcore/cdvdman
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_SMB=1 -C $< all

$(EE_ASM_DIR)smb_cdvdman.s: modules/iopcore/cdvdman/smb_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smb_cdvdman_irx

modules/iopcore/cdvdman/hdd_cdvdman.irx: modules/iopcore/cdvdman
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_HDD=1 -C $< all

$(EE_ASM_DIR)hdd_cdvdman.s: modules/iopcore/cdvdman/hdd_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdd_cdvdman_irx

modules/iopcore/cdvdman/hdd_hdpro_cdvdman.irx: modules/iopcore/cdvdman
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_HDPRO=1 -C $< all

$(EE_ASM_DIR)hdd_hdpro_cdvdman.s: modules/iopcore/cdvdman/hdd_hdpro_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdd_hdpro_cdvdman_irx

modules/iopcore/cdvdfsv/cdvdfsv.irx: modules/iopcore/cdvdfsv
	$(MAKE) -C $<

$(EE_ASM_DIR)cdvdfsv.s: modules/iopcore/cdvdfsv/cdvdfsv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cdvdfsv_irx

modules/iopcore/patches/iremsndpatch/iremsndpatch.irx: modules/iopcore/patches/iremsndpatch
	$(MAKE) -C $<

$(EE_ASM_DIR)iremsndpatch.s: modules/iopcore/patches/iremsndpatch/iremsndpatch.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iremsndpatch_irx

modules/iopcore/patches/apemodpatch/apemodpatch.irx: modules/iopcore/patches/apemodpatch
	$(MAKE) -C $<

$(EE_ASM_DIR)apemodpatch.s: modules/iopcore/patches/apemodpatch/apemodpatch.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ apemodpatch_irx

modules/iopcore/patches/f2techioppatch/f2techioppatch.irx: modules/iopcore/patches/f2techioppatch
	$(MAKE) -C $<

$(EE_ASM_DIR)f2techioppatch.s: modules/iopcore/patches/f2techioppatch/f2techioppatch.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ f2techioppatch_irx

modules/iopcore/patches/cleareffects/cleareffects.irx: modules/iopcore/patches/cleareffects
	$(MAKE) -C $<

$(EE_ASM_DIR)cleareffects.s: modules/iopcore/patches/cleareffects/cleareffects.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cleareffects_irx

modules/iopcore/resetspu/resetspu.irx: modules/iopcore/resetspu
	$(MAKE) -C $<

$(EE_ASM_DIR)resetspu.s: modules/iopcore/resetspu/resetspu.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ resetspu_irx

modules/mcemu/bdm_mcemu.irx: modules/mcemu
	$(MAKE) $(MCEMU_DEBUG_FLAGS) $(PADEMU_FLAGS) USE_BDM=1 -C $< all

$(EE_ASM_DIR)bdm_mcemu.s: modules/mcemu/bdm_mcemu.irx
	$(BIN2S) $< $@ bdm_mcemu_irx

modules/mcemu/hdd_mcemu.irx: modules/mcemu
	$(MAKE) $(MCEMU_DEBUG_FLAGS) $(PADEMU_FLAGS) USE_HDD=1 -C $< all

$(EE_ASM_DIR)hdd_mcemu.s: modules/mcemu/hdd_mcemu.irx
	$(BIN2S) $< $@ hdd_mcemu_irx

modules/mcemu/smb_mcemu.irx: modules/mcemu
	$(MAKE) $(MCEMU_DEBUG_FLAGS) $(PADEMU_FLAGS) USE_SMB=1 -C $< all

$(EE_ASM_DIR)smb_mcemu.s: modules/mcemu/smb_mcemu.irx
	$(BIN2S) $< $@ smb_mcemu_irx

modules/isofs/isofs.irx: modules/isofs
	$(MAKE) -C $<

$(EE_ASM_DIR)isofs.s: modules/isofs/isofs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ isofs_irx

$(EE_ASM_DIR)usbd.s: $(PS2SDK)/iop/irx/usbd_mini.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbd_irx

$(EE_ASM_DIR)libsd.s: $(PS2SDK)/iop/irx/libsd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ libsd_irx

$(EE_ASM_DIR)audsrv.s: $(PS2SDK)/iop/irx/audsrv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ audsrv_irx

$(EE_OBJS_DIR)libds34bt.a: modules/ds34bt/ee/libds34bt.a
	cp $< $@

modules/ds34bt/ee/libds34bt.a: modules/ds34bt/ee
	$(MAKE) -C $<

modules/ds34bt/iop/ds34bt.irx: modules/ds34bt/iop
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34bt.s: modules/ds34bt/iop/ds34bt.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34bt_irx

$(EE_OBJS_DIR)libds34usb.a: modules/ds34usb/ee/libds34usb.a
	cp $< $@

modules/ds34usb/ee/libds34usb.a: modules/ds34usb/ee
	$(MAKE) -C $<

modules/ds34usb/iop/ds34usb.irx: modules/ds34usb/iop
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34usb.s: modules/ds34usb/iop/ds34usb.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34usb_irx

modules/pademu/bt_pademu.irx: modules/pademu
	$(MAKE) -C $< USE_BT=1

$(EE_ASM_DIR)bt_pademu.s: modules/pademu/bt_pademu.irx
	$(BIN2S) $< $@ bt_pademu_irx

modules/pademu/usb_pademu.irx: modules/pademu
	$(MAKE) -C $< USE_USB=1

$(EE_ASM_DIR)usb_pademu.s: modules/pademu/usb_pademu.irx
	$(BIN2S) $< $@ usb_pademu_irx

$(EE_ASM_DIR)bdm.s: $(PS2SDK)/iop/irx/bdm.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdm_irx

$(EE_ASM_DIR)bdmfs_fatfs.s: $(PS2SDK)/iop/irx/bdmfs_fatfs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdmfs_fatfs_irx

$(EE_ASM_DIR)iLinkman.s: $(PS2SDK)/iop/irx/iLinkman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iLinkman_irx

ifeq ($(DEBUG),1)
# block device drivers with printf's
$(EE_ASM_DIR)usbmass_bd.s: $(PS2SDK)/iop/irx/usbmass_bd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbmass_bd_irx

$(EE_ASM_DIR)IEEE1394_bd.s: $(PS2SDK)/iop/irx/IEEE1394_bd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ IEEE1394_bd_irx

$(EE_ASM_DIR)mx4sio_bd.s: $(PS2SDK)/iop/irx/mx4sio_bd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mx4sio_bd_irx
else
# block device drivers without printf's
$(EE_ASM_DIR)usbmass_bd.s: $(PS2SDK)/iop/irx/usbmass_bd_mini.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbmass_bd_irx

$(EE_ASM_DIR)IEEE1394_bd.s: $(PS2SDK)/iop/irx/IEEE1394_bd_mini.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ IEEE1394_bd_irx

$(EE_ASM_DIR)mx4sio_bd.s: $(PS2SDK)/iop/irx/mx4sio_bd_mini.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mx4sio_bd_irx
endif

modules/bdmevent/bdmevent.irx: modules/bdmevent
	$(MAKE) -C $<

$(EE_ASM_DIR)bdmevent.s: modules/bdmevent/bdmevent.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdmevent_irx

$(EE_ASM_DIR)ps2dev9.s: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2dev9_irx

modules/network/SMSUTILS/SMSUTILS.irx: modules/network/SMSUTILS
	$(MAKE) -C $<

$(EE_ASM_DIR)smsutils.s: modules/network/SMSUTILS/SMSUTILS.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smsutils_irx

$(EE_ASM_DIR)ps2ip.s: $(PS2SDK)/iop/irx/ps2ip-nm.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ip_irx

modules/network/SMSTCPIP/SMSTCPIP.irx: modules/network/SMSTCPIP
	$(MAKE) $(SMSTCPIP_INGAME_CFLAGS) -C $< rebuild

$(EE_ASM_DIR)ingame_smstcpip.s: modules/network/SMSTCPIP/SMSTCPIP.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ingame_smstcpip_irx

modules/network/smap-ingame/smap.irx: modules/network/smap-ingame
	$(MAKE) -C $<

$(EE_ASM_DIR)smap_ingame.s: modules/network/smap-ingame/smap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smap_ingame_irx

$(EE_ASM_DIR)smap.s: $(PS2SDK)/iop/irx/smap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smap_irx

$(EE_ASM_DIR)netman.s: $(PS2SDK)/iop/irx/netman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ netman_irx

$(EE_ASM_DIR)ps2ips.s: $(PS2SDK)/iop/irx/ps2ips.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ips_irx

$(EE_ASM_DIR)smbman.s: $(PS2SDK)/iop/irx/smbman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smbman_irx

modules/network/smbinit/smbinit.irx: modules/network/smbinit
	$(MAKE) -C $<

$(EE_ASM_DIR)smbinit.s: modules/network/smbinit/smbinit.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smbinit_irx

$(EE_ASM_DIR)ps2atad.s: $(PS2SDK)/iop/irx/ps2atad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2atad_irx

$(EE_ASM_DIR)hdpro_atad.s: $(PS2SDK)/iop/irx/hdproatad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdpro_atad_irx

$(EE_ASM_DIR)poweroff.s: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ poweroff_irx

modules/hdd/xhdd/xhdd.irx: modules/hdd/xhdd
	$(MAKE) -C $<

$(EE_ASM_DIR)xhdd.s: modules/hdd/xhdd/xhdd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ xhdd_irx

$(EE_ASM_DIR)ps2hdd.s: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	$(BIN2S) $< $@ ps2hdd_irx

$(EE_ASM_DIR)ps2fs.s: $(PS2SDK)/iop/irx/ps2fs-osd.irx
	$(BIN2S) $< $@ ps2fs_irx

modules/vmc/genvmc/genvmc.irx: modules/vmc/genvmc
	$(MAKE) $(MOD_DEBUG_FLAGS) -C $<

$(EE_ASM_DIR)genvmc.s: modules/vmc/genvmc/genvmc.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ genvmc_irx

modules/network/lwnbdsvr/lwnbdsvr.irx: modules/network/lwnbdsvr
	$(MAKE) -C $<

$(EE_ASM_DIR)lwnbdsvr.s: modules/network/lwnbdsvr/lwnbdsvr.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ lwnbdsvr_irx

$(EE_ASM_DIR)udptty.s: $(PS2SDK)/iop/irx/udptty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udptty_irx

modules/debug/udptty-ingame/udptty.irx: modules/debug/udptty-ingame
	$(MAKE) -C $<

$(EE_ASM_DIR)udptty-ingame.s: modules/debug/udptty-ingame/udptty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udptty_ingame_irx

$(EE_ASM_DIR)ioptrap.s: $(PS2SDK)/iop/irx/ioptrap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ioptrap_irx

modules/debug/ps2link/ps2link.irx: modules/debug/ps2link
	$(MAKE) -C $<

$(EE_ASM_DIR)ps2link.s: modules/debug/ps2link/ps2link.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2link_irx

modules/network/nbns/nbns.irx: modules/network/nbns
	$(MAKE) -C $<

$(EE_ASM_DIR)nbns-iop.s: modules/network/nbns/nbns.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ nbns_irx

modules/network/httpclient/httpclient.irx: modules/network/httpclient
	$(MAKE) -C $<

$(EE_ASM_DIR)httpclient-iop.s: modules/network/httpclient/httpclient.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ httpclient_irx

$(EE_ASM_DIR)iomanx.s: $(PS2SDK)/iop/irx/iomanX.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iomanx_irx

$(EE_ASM_DIR)filexio.s: $(PS2SDK)/iop/irx/fileXio.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ filexio_irx

$(EE_ASM_DIR)sio2man.s: $(PS2SDK)/iop/irx/freesio2.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sio2man_irx

$(EE_ASM_DIR)padman.s: $(PS2SDK)/iop/irx/freepad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ padman_irx

$(EE_ASM_DIR)mcman.s: $(PS2SDK)/iop/irx/mcman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv.s: $(PS2SDK)/iop/irx/mcserv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcserv_irx

$(EE_ASM_DIR)poeveticanew.s: thirdparty/PoeVeticaNew.ttf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ poeveticanew_raw

$(EE_ASM_DIR)icon_sys.s: gfx/icon.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys

$(EE_ASM_DIR)icon_icn.s: gfx/opl.icn | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_icn

$(EE_ASM_DIR)icon_sys_A.s: misc/icon_A.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys_A

$(EE_ASM_DIR)icon_sys_J.s: misc/icon_J.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys_J

$(EE_ASM_DIR)icon_sys_C.s: misc/icon_C.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys_C

$(EE_ASM_DIR)conf_theme_OPL.s: misc/conf_theme_OPL.cfg | $(EE_ASM_DIR)
	$(BIN2S) $< $@ conf_theme_OPL_cfg

$(EE_ASM_DIR)boot.s: audio/boot.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ boot_adp

$(EE_ASM_DIR)cancel.s: audio/cancel.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cancel_adp

$(EE_ASM_DIR)confirm.s: audio/confirm.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ confirm_adp

$(EE_ASM_DIR)cursor.s: audio/cursor.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cursor_adp

$(EE_ASM_DIR)message.s: audio/message.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ message_adp

$(EE_ASM_DIR)transition.s: audio/transition.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ transition_adp

$(EE_ASM_DIR)IOPRP_img.s: modules/iopcore/IOPRP.img | $(EE_ASM_DIR)
	$(BIN2S) $< $@ IOPRP_img

$(EE_ASM_DIR)drvtif_ingame_irx.s: modules/debug/drvtif-ingame.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ drvtif_ingame_irx

$(EE_ASM_DIR)tifinet_ingame_irx.s: modules/debug/tifinet-ingame.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ tifinet_ingame_irx

$(EE_ASM_DIR)drvtif_irx.s: modules/debug/drvtif.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ drvtif_irx

$(EE_ASM_DIR)tifinet_irx.s: modules/debug/tifinet.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ tifinet_irx

$(EE_ASM_DIR)deci2_img.s: modules/debug/deci2.img | $(EE_ASM_DIR)
	$(BIN2S) $< $@ deci2_img
