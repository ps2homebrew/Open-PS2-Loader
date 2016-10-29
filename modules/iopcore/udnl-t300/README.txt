UDNL (Updater) module (TOOL SDK v3.00)	-	2014/08/03
----------------------------------------------------------

UDNL is a updater module, which is used for the final phase (Boot mode 3) within the IOP reset cycle. It is responsible for loading the requested IOPRP image into RAM and kickstarting the final IOP bootup sequence.

UDNL modules can be found in the boot ROM (rom0:) of all known Playstation 2 consoles (including other types that were based on it too, like the System 246 and PSX), DVD player updates, and HDDOSD/PSBBN game/software installations, in one form or another.

The UDNL module of the boot ROM image from TOOL SDK v3.00 was used as a basis for this new module. The original code was from the UDNL module of boot ROM v2.20. Since UDNL modules all have the same basic functionality, I did not make a new disassembly but simply brought over the changes to the code.

It seems like Sony had designed at least 3 working types of executable files for the IOP:
Type 1 -> Slightly modified MIPS COFF (I didn't test COFF loading because I don't have any of such modules, and it seems to be expecting a static COFF). The GPR and CPR mask fields seem to have been repurposed.
Type 2 -> ??? (Seems to be recognized, but the integrated LoadModule function doesn't handle this type)
Type 3 -> ELF (Seems to be expecting a static ELF).
Type 4 -> IRX (We all know what this is!)

So far, I think that the only prominent difference between the rom0:UDNL module of a Protokernel console and one from a newer boot ROM is the lack of a breakpoint that gets hit when an IOPRP image fails to load. I don't know whether a Protokernel console's UDNL module supports all 4 executable types, since I haven't checked.

UDNL modules that come with the DVD players and TOOL units (at least, from the newer SDK revisions) seem to have fewer bugs, most notably because they were solely designed to reboot the IOP with their embedded IOPRP images and the mechanism for facilitating that does work. Such modules do not support reading UDNL images from devices and lack code for doing so. Their embedded IOPRP images seem to be either compressed and/or encrypted as well.

Comment out FULL_UDNL within udnl.c to build a UDNL module that works like the Sony UDNL module that ships with DVD players. Such a UDNL module will reset the IOP with only their embedded IOPRP images.

Syntax:
-------
	UDNL <last IOPRP image>...<first IOPRP image> [-v] [-nobusini] [-nocacheini]

UDNL accepts multiple IORP images in the command passed to it. It'll scan through all the specified images to locate the newest version of each module that is specified in IOPBTCONF (which acts like CONFIG.SYS of MS-DOs, for those who know what that is), and will build a list of modules to load.

It will look for modules from among the list of specified IOPRP images, in the order in which they are specified. rom0: will be automatically included at the very end of the IOPRP image list.

UDNL may also have a valid IOPRP image embedded within it, and it will automatically use that IOPRP image. The Sony ROM UDNL module requires that the programmer does NOT specify any commands for it if an IOPRP image is embedded within it (See below). This limitation does not exist for this clone.

-v:
	The -v command is elusive; although recognized by UDNL, no code actually reacts to its presence.
	(It's most likely a verbosity level control though)

-nobusini (or just -nb):
	SSBUS initialization is skipped.

-nocacheini (or just -nc):
	Cache initialization is skipped.

IOPBTCONF files can include other IOPBTCONF files, although this has some limitations (see below).

IOPBTCONF file syntax:
----------------------
@<address>	- The address at which to start loading modules from.
!include <file>	- Includes the specified file.
!addr <address>	- Includes a module that exists at the specified memory address.
#<characters>	- The '#' symbol marks the whole line as a comment.

Lines beginning with other characters are automatically assumed to be the filenames of modules that are to be loaded.

Note: SYSMEM and LOADCORE have to be the first and second modules to be loaded respectively.

Differences between the ROM UDNL module of retail consoles and the TOOL unit's:
-------------------------------------------------------------------------------
			TOOL				Retail
Arguments		More options			Only "-v" is recognized
Device checklist	Whitelist + encrypted		Blacklist
Startup sequence	Within kernel mode		With only interrupts disabled (With CpuDisableIntr)
Module relocation	Done by separate function	Code is inline

Known bugs and limitations:
---------------------------
1. IOPBTCONF files cannot include another file (using "!include") in the middle of the list of modules, or UDNL will break its own module list (Look at the comments within the source code for ParseIOPBTCONF()).
2. If an IOPRP image is embedded within UDNL, specifying at least one IOPRP image as a command will cause UDNL to overwrite its internal IOPRP image (and in the process of doing so, will most likely destroy other things as well).
3. Certain devices cannot be used for storing IOPRP images, due to an internal blacklist (Refer to isIllegalBootDevice()). Blacklisted devices are "mc", "hd", "net" and "dev".
4. When IOPBTCONF files include another IOPBTCONF file that is being parsed and its modules are located, the modules will only be scanned from starting with the IOPRP image that contains the IOPBTCONF that is being parsed (e.g. if the 2nd IOPRP image contains a IOPBTCONF file that is included by another file, the modules listed in the IOPBTCONF file will only be scanned for stating with the 2nd IOPRP image. Files in the first IOPRP image will be IGNORED). Hence, keep the order of your IOPRP images/commands in mind!
5. The design for loading modules from an embedded IOPRP image of the boot ROM UDNL module is flawed. Its design seemed to be to allow the programmer to use either an embedded IOPRP image OR IOPRP images that are stored as accessible files. If the embedded IOPRP image is to be used, it would just get its reset data structure to point to the embedded IOPRP image as the buffer containing the IOPRP images... but that causes the reset data structure itself to become unprotected during the IOP bootup sequence and will possibly cause corruption under certain conditions.
	The Sony DVD player UDNL modules were designed to allocate memory for storing its reset data structure and the embedded IOPRP image. It'll check the image and copy the image into the buffer, as if it is a normal IOPRP image that is stored as a file.
6. If the IOPRP image (Other than the boot ROM) contains an IOPBTCONF file which includes another file, Modules will only be scanned for and selected from IOPRP images which come after the image which contains the IOPBTCONF file which included another IOPBTCONF file:
	boot ROM
	IOPRP image #3
	IOPRP image #2	(Contains IOPTBTCONF which includes IOPBTCONF again)
	IOPRP image #1

	This means that when the list of modules within IOPBTCONF is parsed from IOPRP image #2, modules will only be selected from IOPRP image #2 and later. When the UDNL module finds the "!include" statement and parses through the boot ROM IOPBTCONF file (since no further images after IOPRP image #2 has one), only modules from IOPRP image #3 and the boot ROM will be selected.

#1 only applies to the Sony UDNL module, as it doesn't reset the relative module pointer. This clone has that bug fixed.
#3 only applies to the Sony UDNL module. This clone has that check totally commented out.
#5 only applies to the Sony boot ROM UDNL modules. This clone has a modified system that works similarly to how the DVD player UDNL modules do, but retains the full extended functionality of a boot ROM UDNL module.

TOOL unit UDNL modules do not have any of the bugs, but share only the limitations.
