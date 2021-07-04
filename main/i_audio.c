/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  System interface for sound.
 *
 *-----------------------------------------------------------------------------
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "z_zone.h"

#include "m_swap.h"
#include "i_sound.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "d_main.h"

#include "m_fixed.h"

#include "global_data.h"

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int I_StartSound(int id, int channel, int vol, int sep)
{
	if ((channel < 0) || (channel >= MAX_CHANNELS))
		return -1;

	printf("STUB: I_StartSound id=%d channel=%d\n", id, channel);

	return channel;
}

void I_ShutdownSound(void)
{
    if (_g->sound_inited) {
		lprintf(LO_INFO, "I_ShutdownSound: ");
		lprintf(LO_INFO, "\n");
        _g->sound_inited = false;
	}
}

//static SDL_AudioSpec audio;

void I_InitSound(void) {
	I_InitMusic();

	// Finished initialization.
    lprintf(LO_INFO,"I_InitSound: sound ready");
}

void I_InitMusic(void) {
	printf("STUB: I_InitMusic\n");
}

void I_PlaySong(int handle, int looping) {
    if(handle == mus_None)
        return;
	printf("STUB: I_PlaySong %d\n", handle);
}


void I_PauseSong (int handle){
	printf("STUB: I_PauseSong %d\n", handle);
}

void I_ResumeSong (int handle) {
	printf("STUB: I_ResumeSong %d\n", handle);
}

void I_StopSong(int handle) {
	printf("STUB: I_StopSong %d\n", handle);
}

void I_SetMusicVolume(int volume) {
	printf("STUB: I_SetMusicVolume %d\n", volume);
}
