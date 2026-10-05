#ifndef __ANNOUNCE_H
#define __ANNOUNCE_H

// Broadcasts a PS2RichPresence LAUNCH packet over UDP so companion apps on the LAN can react to the game being started (e.g. Discord rich presence).
// Packet spec: https://github.com/brkzlr/PS2RichPresence/blob/master/protocol/ps2rp_protocol.h
void announceGameLaunch(const char *titleId, const char *displayName);

#endif
