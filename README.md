# Xbox 2 PlayStation Emulator

## Introduction:
X2P is an Xbox emulator for the PlayStation 2. It was developed in secrecy, with consistent updates over the last 15 years. The GUI is adapted from Open PS2 Loader Beta 1.2.0 1996, but at its core, it's an emulator. Running Xbox games on a PS2 might seem unthinkable, but here's how it's done.

The emulator is entirely written in Assembler—around 120,000 lines—to ensure efficiency surpassing that of C implementations. However, it's important to note that it only operates on DECKARD models, thanks to their faster PPC (replacing the IOP) and a sufficiently large L-cache to handle the Xbox's additional RAM compared to the PS2's capabilities. Full-speed emulation is technically unfeasible, particularly for demanding games like Conker or Oddworld SW. Nevertheless, many games run well, including the first Halo, while some, such as GTA SA, only boot to a black screen.

## Secrets behind X2P:
This project is an April Fools prank **and does NOT work** as a real emulator. The **Introduction** given above **is entirely FAKE!!**  The code is however based on OPL-v1.2.0-2081 and still functions as normal OPL does. There are some exceptions but functionally the ELF is still OPL.

### Differences:
X2P ELF has some differences as compared to OPL, those are:
1. Only USB and other BDM devices (MX4SIO & iLink) are usable.
2. DVD games are loaded from ```massX:/XISO``` and CD ones from ```massX:/XVHD``` (XVHD because it was supposed to have a XBOX HDD image to sell the prank and CD folder was useless for our purpose anyways).
3. Many settings have been removed from the GUI, hence cannot be enabled.
4. Config also is NEVER loaded at boot time.
   

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
