# Xbox 2 PlayStation Emulator

## Introduction:
X2P is an Xbox emulator for the PlayStation 2. It was developed in secrecy, with consistent updates over the last 15 years. The GUI is adapted from Open PS2 Loader Beta 1.2.0 1996, but at its core, it's an emulator. Running Xbox games on a PS2 might seem unthinkable, but here's how it's done.

The emulator is entirely written in Assembler—around 120,000 lines—to ensure efficiency surpassing that of C implementations. However, it's important to note that it only operates on DECKARD models, thanks to their faster PPC (replacing the IOP) and a sufficiently large L-cache to handle the Xbox's additional RAM compared to the PS2's capabilities. Full-speed emulation is technically unfeasible, particularly for demanding games like Conker or Oddworld SW. Nevertheless, many games run well, including the first Halo, while some, such as GTA SA, only boot to a black screen.

## Secret behind X2P:
This project is an April Fools prank **and does no work** as a real emulator. The **Introduction** given above **is entirely fake!!**  The code is however based on OPL-v1.2.0-2081 and still unctions as normal OPL does. There are some exceptions in the GUI but functionally the ELF is still OPL.

## How To:

1. Use a USB drive (other media are not supported by X2P) and create a single partition using the exFAT file system. FAT32 is an option, but be aware that games which exceed 4GiB would need to be split.
2. Add DVD game images to "mass:/XISO/" and CD ones to "mass:/XVHD".

## Special Thanks:

Background Music by Tobias Lorsbach (Logic Moon):
- [Logic Moon Sound](https://freesound.org/people/LogicMoon/sounds/728162/)
- [Logic Moon Website](https://logic-moon.de/)

Boot Sound by ChristmasKrumble666:
- [ChristmasKrumble666 Sound](https://freesound.org/people/ChristmasKrumble666/sounds/727267/)

Sound Effects by LaurenPonder:
- [LaurenPonder Sound 1](https://freesound.org/people/LaurenPonder/sounds/638903/)
- [LaurenPonder Sound 2](https://freesound.org/people/LaurenPonder/sounds/638899/)

Font by iamnotxyzzy:
- [Font by iamnotxyzzy](https://fontstruct.com/fontstructions/show/1495632)

Code Base by PS2Devs:
- [PS2Devs GitHub](https://github.com/ps2homebrew)

Did you know there's an awesome easter hidden in the release ELF?
Maybe a file in the source has it? 😉
