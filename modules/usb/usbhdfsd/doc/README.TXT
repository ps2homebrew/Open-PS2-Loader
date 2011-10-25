This project is based directly off the original "USB_MASS" project.  I've removed the PC testing stuff like VFAT, etc
and some of the debugging things.  I've also removed the RPC server.  In addition I split some files up into
more files and renamed other files.

I don't take credit for this project, see the documents in "docs/usb_mass" for the original readme.txt though some
information in it is no longer valid. The credits have been stripped from the source files, see the bottom of this
document for a list of known contributors if you have contributed to this project, feel free to add to the list.

This project is, of course, still released under AFL 2.0. My reasoning for creating a new project based on the old
one was that there were too many different strains of USB_MASS floating around and I intend to change the project
design significantly enough that I could forsee problems in the future.

This module incorporates a number of fixes to bugs which existed in the old USB_MASS project and increases compatability
with devices.  It correctly supports directory functions such as dopen/dread/etc.

- Herben '06

Marek Olejnik (ole00@post.cz) - original project author
Hermes (support for sector sizes from 512 to 4096 bytes)
raipsu (fs_dopen, fs_dclose, fs_dread, fs_getstat implementation)
MegaMan(several fixes)
Herben (several fixes, reorganization, etc)
