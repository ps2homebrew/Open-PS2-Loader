# Vendored USB IRX (pre-USBD-rewrite)

These IRX modules are built from ps2sdk commit `2dc6b32f` (last tree
**before** `b1f7ff96` “USBD feature update”, 2024-09-04).

## Why

Crash Bandicoot: Wrath of Cortex (USB) is broken with the post-rewrite
USBD baked into modern OPL builds: multi-minute loads then black screen
on play. Last good OPL Beta-2125; first bad Beta-2127.

Hardware confirmation: Beta-2245 + these IRX works (~10s load, playable);
stock Beta-2245 with current SDK USBD does not.

See: https://github.com/ps2homebrew/Open-PS2-Loader/issues/1751

## Rebuild

From a ps2dev environment, checkout ps2sdk at `2dc6b32f` and build
`iop/usb/usbd` (MINI_DRIVER for usbd_mini) and `iop/usb/usbmass_bd`.
