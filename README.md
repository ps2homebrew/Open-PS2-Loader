# Xbox 2 PlayStation Emulator

## Introduction:
X2P is an Xbox emulator for the PlayStation 2. It was developed in secrecy, with consistent updates over the last 15 years. The GUI is adapted from Open PS2 Loader Beta 1.2.0 1996, but at its core, it's an emulator. Running Xbox games on a PS2 might seem unthinkable, but here's how it's done.

The emulator is entirely written in Assembler—around 120,000 lines—to ensure efficiency surpassing that of C implementations. However, it's important to note that it only operates on DECKARD models, thanks to their faster PPC (replacing the IOP) and a sufficiently large L-cache to handle the Xbox's additional RAM compared to the PS2's capabilities. Full-speed emulation is technically unfeasible, particularly for demanding games like Conker or Oddworld SW. Nevertheless, many games run well, including the first Halo, while some, such as GTA SA, only boot to a black screen.

-----

## Secrets behind X2P:
This project is an April Fools prank **and does NOT work** as a real emulator. The **Introduction** given above **is entirely FAKE!!**  The code is however based on OPL-v1.2.0-2081 and still functions as normal OPL does. There are some exceptions but functionally the ELF is still OPL.

### Differences:
X2P ELF has some differences as compared to OPL, those are:
1. Only USB and other BDM devices (MX4SIO & iLink) are usable.
2. DVD games are loaded from ```massX:/XISO``` and CD ones from ```massX:/XVHD``` (XVHD because it was supposed to be a place to put a XBOX HDD image to sell the prank and CD folder was useless for our purpose anyways).
3. Many settings have been removed from the GUI, hence cannot be enabled.
4. Config also is NEVER loaded at boot time.

### Working:
The disc image containing XRICK is a hybrid one. The PS2 contents are hidden (as ISO9660 allows this), while the Xbox contents are visible. Thus, users opening the image may initially believe it is solely Xbox material. However, in reality, it includes SYSTEM.CNF and XRICK.XBE, which is actually XRICK.ELF. The name has been changed to conceal the April Fools' joke in the OPL menu, and for the PS2 side of the software, the file name doesn't matter as long as it doesn't violate the ISO9660 standard and is correctly referenced in the CNF.

For this reason, we searched for homebrew applications available on both platforms that could also be loaded successfully from OPL. Fortunately, XRick was the only application that met these criteria.

-----

## Theme:
The theme used, **X2P - Xbox to Playstation**, is Designed by Berion and Made by [Ripto](https://github.com/knowahitall) and can be downloaded from the [Releases Page](https://github.com/koraxial/Xbox-2-PlayStation-Emulator-AlFa/releases/tag/THM-v1.0.0).

-----

## Links:
### Downloads:
- [Releases Page]( https://github.com/koraxial/Xbox-2-PlayStation-Emulator-AlFa/releases/tag/17022)
- [Internet Archive](https://web.archive.org/web/20240401135103/https://cdn.discordapp.com/attachments/799243822743289866/1224268410708295721/X2P.7z?ex=661cdfce&is=660a6ace&hm=3f213340eb1dc629ac282262210a821e737d93f425362fab764ceda424a84173&)

### Video showcases:
- [Video by HardLevel](https://youtu.be/KPsUgV0-FTU)
- [Video by korax](https://youtu.be/jvNvM48Oi48?si=u4i8o4hk7K-KBNAU)

-----

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

And lastly, a big thankyou **F**or **O**ur **O**riginal **L**oyal **S**upporters who supported us all the way through!

-----

Did you know there's an awesome easter hidden in the [release](https://github.com/koraxial/Xbox-2-PlayStation-Emulator-AlFa/releases/tag/17022) ELF?
Maybe a file in the source has clues? 😉

[Original OPL Readme (from v1.2.0-2081)](OPL_README.md)
