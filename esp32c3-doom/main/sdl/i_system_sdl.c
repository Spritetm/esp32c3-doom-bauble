/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2006 by Colin Phipps, Florian Schulze
 *  
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
 *  Misc system stuff needed by Doom, implemented for POSIX systems.
 *  Timers and signals.
 *
 *-----------------------------------------------------------------------------
 */


#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

#include "doomtype.h"
#include "m_fixed.h"
#include "i_system.h"
#include "doomdef.h"
#include "lprintf.h"
#include "d_main.h"
#include "d_event.h"
#include "global_data.h"
#include "tables.h"

#include "i_system.h"

#include "global_data.h"

#include <SDL2/SDL.h>

/* cphipps - I_GetVersionString
 * Returns a version string in the given buffer 
 */
const char* I_GetVersionString(char* buf, size_t sz)
{
  sprintf(buf,"PocketDoom v%s",VERSION);
  return buf;
}

#define MAX_MESSAGE_SIZE 1024

void I_Error (const char *error, ...)
{
	char msg[MAX_MESSAGE_SIZE];
 
	va_list v;
	va_start(v, error);
	
	vsprintf(msg, error, v);
	
	va_end(v);

	printf("%s", msg);


    fflush( stderr );
	fflush( stdout );

    //fgets(msg, sizeof(msg), stdin);

	I_Quit();
}

static int old_btn;

//KEYD_LEFT, KEYD_DOWN, KEYD_RIGHT, KEYD_UP, KEYD_A, KEYD_B, L, R, START, SELECT


void I_ProcessKeyEvents() {
	SDL_Event e;
	event_t ev;
	while(SDL_PollEvent(&e)!=0) {
		if (e.type==SDL_QUIT) {
			exit(0);
		} else if (e.type==SDL_KEYDOWN || e.type==SDL_KEYUP) {
			ev.type=(e.type==SDL_KEYDOWN)?ev_keydown:ev_keyup;
			ev.data1=-1;
			if (e.key.keysym.sym==SDLK_ESCAPE && e.type==SDL_KEYDOWN) exit(0);
			if (e.key.keysym.sym==SDLK_UP) ev.data1=KEYD_UP;
			if (e.key.keysym.sym==SDLK_DOWN) ev.data1=KEYD_DOWN;
			if (e.key.keysym.sym==SDLK_LEFT) ev.data1=KEYD_LEFT;
			if (e.key.keysym.sym==SDLK_RIGHT) ev.data1=KEYD_RIGHT;
			if (e.key.keysym.sym==SDLK_LCTRL) ev.data1=KEYD_B;
			if (e.key.keysym.sym==SDLK_SPACE) ev.data1=KEYD_A;
			if (e.key.keysym.sym==SDLK_z) ev.data1=KEYD_L;
			if (e.key.keysym.sym==SDLK_x) ev.data1=KEYD_R;
			if (e.key.keysym.sym==SDLK_RETURN) ev.data1=KEYD_START;
			if (e.key.keysym.sym==SDLK_TAB) ev.data1=KEYD_SELECT;
			if (ev.data1!=-1) D_PostEvent(&ev);
		}
	}
}

void I_Quit() {
	while(1);
}

