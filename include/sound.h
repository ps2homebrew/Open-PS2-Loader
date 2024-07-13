#ifndef __SOUND_H
#define __SOUND_H

enum SFX {
    SFX_BOOT = 0,
    SFX_CANCEL,
    SFX_CONFIRM,
    SFX_CURSOR,
    SFX_MESSAGE,
    SFX_TRANSITION,
    SFX_BD_CONNECT,
    SFX_BD_DISCONNECT,

    SFX_COUNT
};

void audioInit(void);
void audioEnd(void);
void audioSetVolume(void);

int sfxInit(int bootSnd);
int sfxGetSoundDuration(int id);
void sfxPlay(int id);

void bgmStart(void);
void bgmStop(void);
int isBgmPlaying(void);
void bgmMute(void);
void bgmUnMute(void);

#endif
