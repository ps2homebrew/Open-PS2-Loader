#ifndef __SOUND_H
#define __SOUND_H

#define NUM_SFX_FILES 6

extern struct audsrv_adpcm_t sfx[NUM_SFX_FILES];

int sfxInit(void);
void sfxVolume(void);

int thmSfxEnabled;

#endif
