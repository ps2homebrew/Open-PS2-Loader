// # Header for Per-Game CHEAT

#ifdef IGS
#define IGS_VERSION "0.1"
#endif

#ifndef __PGCHT_H
#define __PGCHT_H

#ifdef CHEAT
#define CHEAT_VERSION "0.5.3.65.g774d1"

#define MAX_HOOKS 5
#define MAX_CODES 250
#define MAX_CHEATLIST (MAX_HOOKS * 2 + MAX_CODES * 2)

int gEnableCheat; // Enables PS2RD Cheat Engine - 0 for Off, 1 for On
int gCheatMode;   // Cheat Mode - 0 Enable all cheats, 1 Cheats selected by user

int gCheatList[MAX_CHEATLIST]; //Store hooks/codes addr+val pairs
#endif