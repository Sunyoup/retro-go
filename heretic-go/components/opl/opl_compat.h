// opl_compat.h
// retro-go/heretic-go: the OPL music player files come from prboom-go, this header
// replaces the prboom headers they used (doomtype.h, lprintf.h, w_wad.h, ...).

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;
typedef int boolean;
#ifndef false
#define false 0
#define true 1
#endif

#define PACKEDATTR __attribute__((packed))

// little endian only
#define doom_htows(x) ((short)(x))
#define doom_htoms(x) ((short)(x))
#define doom_htowl(x) ((int)(x))

enum { LO_INFO, LO_WARN, LO_ERROR };
#define lprintf(level, ...) printf(__VA_ARGS__)

// The GENMIDI lump (OPL instruments), set by the app before opl_synth_player.init()
extern const void *opl_genmidi_lump;
#define W_CacheLumpName(name) (opl_genmidi_lump)
#define W_UnlockLumpName(name) ((void)0)

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
