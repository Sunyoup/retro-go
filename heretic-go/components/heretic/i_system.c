// I_system.c
// retro-go: the platform independent part of the old DOS I_IBM.C / I_SOUND.C.
// The platform (video, input, timer, sound output) is implemented by the app.

#include <stdlib.h>
#include "doomdef.h"
#include "sounds.h"

int UpdateState;

// Sound settings, saved in the config file
int snd_Channels;
int snd_DesiredMusicDevice, snd_DesiredSfxDevice;
int snd_MusicDevice, snd_SfxDevice;
int snd_MaxVolume, snd_MusicVolume;
int snd_SBport, snd_SBirq, snd_SBdma, snd_Mport;

byte *pcscreen, *destscreen, *destview;

fixed_t FixedMul(fixed_t a, fixed_t b)
{
	return ((int64_t)a * b) >> FRACBITS;
}

fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
	return ((int64_t)a << FRACBITS) / b;
}

byte *I_AllocLow(int length)
{
	byte *mem = calloc(1, length);
	if (!mem)
		I_Error("I_AllocLow: alloc of %i failed", length);
	return mem;
}

void I_InitNetwork(void)
{
	// single player game only
	doomcom = calloc(1, sizeof(*doomcom));
	netgame = false;
	doomcom->id = 0x12345678;
	doomcom->numplayers = doomcom->numnodes = 1;
	doomcom->deathmatch = false;
	doomcom->consoleplayer = 0;
	doomcom->ticdup = 1;
	doomcom->extratics = 0;
}

void I_NetCmd(void)
{
	I_Error("I_NetCmd when not in netgame");
}

// CyberMan 3D controller (DOS only)
int isCyberPresent = 0;

void I_ReadCyberCmd(ticcmd_t *cmd)
{
}

void I_Tactile(int on, int off, int total)
{
}

int I_GetSfxLumpNum(sfxinfo_t *sound)
{
	if (sound->name == 0)
		return 0;
	if (sound->link)
		sound = sound->link;
	return W_GetNumForName(sound->name);
}
