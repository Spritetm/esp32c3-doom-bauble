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
#include "input.h"

#include "i_system.h"

#include "global_data.h"
#include "hid_ev.h"
#include "usb_hid_keys.h"


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
static int old_axis[2];

#define LIM 10000

void I_ProcessKeyEvents() {
	hid_ev_t hev;
	event_t ev;
	while (input_get_event(&hev)) {
		if (hev.type==HIDEV_EVENT_KEY_DOWN || hev.type==HIDEV_EVENT_KEY_UP) {
			ev.type=(hev.type==HIDEV_EVENT_KEY_DOWN)?ev_keydown:ev_keyup;
			const int btns[]={KEYD_B, KEYD_A, KEYD_L, KEYD_R, KEYD_SELECT, KEYD_START, KEYD_UP, KEYD_DOWN, KEYD_LEFT, KEYD_RIGHT};
			const int codes[]={KEY_Z, KEY_X, KEY_A, KEY_S, KEY_TAB, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT};
			for (int i=0; i<10; i++) {
				if (hev.key.keycode==codes[i]) {
					ev.data1=btns[i];
					D_PostEvent(&ev);
				}
			}
		} else if (hev.type==HIDEV_EVENT_JOY_AXIS && hev.no<2) {
			ev.data1=hev.no?KEYD_DOWN:KEYD_RIGHT;
			if (hev.joyaxis.pos>LIM && old_axis[hev.no]<=LIM) {
				ev.type=ev_keydown;
				D_PostEvent(&ev);
			} else if (hev.joyaxis.pos<=LIM && old_axis[hev.no]>LIM) {
				ev.type=ev_keyup;
				D_PostEvent(&ev);
			}

			ev.data1=hev.no?KEYD_UP:KEYD_LEFT;
			if (hev.joyaxis.pos<-LIM && old_axis[hev.no]>=-LIM) {
				ev.type=ev_keydown;
				D_PostEvent(&ev);
			} else if (hev.joyaxis.pos>=-LIM && old_axis[hev.no]<-LIM) {
				ev.type=ev_keyup;
				D_PostEvent(&ev);
			}

			old_axis[hev.no]=hev.joyaxis.pos;
		} else if (hev.type==HIDEV_EVENT_JOY_BUTTONDOWN || hev.type==HIDEV_EVENT_JOY_BUTTONUP) {
			const int btns[]={KEYD_B, KEYD_A, KEYD_B, KEYD_A, KEYD_L, KEYD_R, KEYD_SELECT, KEYD_START};
			if (hev.no<8) {
				ev.type=(hev.type==HIDEV_EVENT_JOY_BUTTONDOWN)?ev_keydown:ev_keyup;
				ev.data1=btns[hev.no];
				D_PostEvent(&ev);
			}
		} else if (hev.type==HIDEV_EVENT_MOUSE_BUTTONDOWN || hev.type==HIDEV_EVENT_MOUSE_BUTTONUP) {
			const int btns[]={KEYD_B, KEYD_A, KEYD_START, KEYD_SELECT, KEYD_L, KEYD_R};
			if (hev.no<6) {
				ev.type=(hev.type==HIDEV_EVENT_MOUSE_BUTTONDOWN)?ev_keydown:ev_keyup;
				ev.data1=btns[hev.no];
				D_PostEvent(&ev);
			}
		} else if (hev.type==HIDEV_EVENT_MOUSE_MOTION) {
			ev.type=(hev.mouse_motion.dx>3)?ev_keydown:ev_keyup;
			ev.data1=KEYD_LEFT;
			D_PostEvent(&ev);
			ev.type=(hev.mouse_motion.dx<-3)?ev_keydown:ev_keyup;
			ev.data1=KEYD_RIGHT;
			D_PostEvent(&ev);
			ev.type=(hev.mouse_motion.dy>3)?ev_keydown:ev_keyup;
			ev.data1=KEYD_DOWN;
			D_PostEvent(&ev);
			ev.type=(hev.mouse_motion.dy<-3)?ev_keydown:ev_keyup;
			ev.data1=KEYD_UP;
			D_PostEvent(&ev);
		} else if (hev.type==HIDEV_EVENT_MOUSE_WHEEL) {
			ev.type=(hev.mouse_wheel.d>1)?ev_keydown:ev_keyup;
			ev.data1=KEYD_L;
			D_PostEvent(&ev);
			ev.type=(hev.mouse_wheel.d<-1)?ev_keydown:ev_keyup;
			ev.data1=KEYD_R;
			D_PostEvent(&ev);
		}
	}
}

void I_Quit() {
	while(1);
}

