/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>


SDL_Window *window;
SDL_Renderer* renderer;

uint8_t palette[256*3];

void lcd_set_pal(uint8_t *pal) {
	memcpy(palette, pal, 256*3);
}


void lcd_render_fb(uint8_t *fb) {
	for (int y=0; y<160*4; y++) {
		for (int x=0; x<240*4; x++) {
			int p=fb[240*(y/4)+(x/4)];
			SDL_SetRenderDrawColor(renderer, palette[p*3],palette[p*3+1],palette[p*3+2], SDL_ALPHA_OPAQUE);
			SDL_RenderDrawPoint(renderer, x, y);
		}
	}
	SDL_RenderPresent(renderer);
}


void lcd_init() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Error initializing sdl!\n");
		exit(1);
	}
	if (SDL_CreateWindowAndRenderer(240*4, 160*4, 0, &window, &renderer)!=0) {
		printf("Error creating window!\n");
		exit(1);
	}
}

void snd_init() {

}

char* itoa (int val, char * s, int radix) {
	if (radix!=10) {
		printf("itoa: radix %d unsupported\n", radix);
	}
	sprintf(s, "%d", val);
	return s;
}

char *strupr (char * s) {
	char *p=s;
	while (*p!=0) {
		if (*p>='a' && *p<='z') *p=*p-'a'+'A';
		p++;
	}
	return s;
}

