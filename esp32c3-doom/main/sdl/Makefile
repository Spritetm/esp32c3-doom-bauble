SRCS := am_map.o dbopl.o d_client.o d_items.o d_main.o f_finale.o f_wipe.o global_data.o 
SRCS += hu_lib.o hu_stuff.o i_audio.o info.o i_video.o lprintf.o  g_game.o i_system_sdl.o i_main_sdl.o
SRCS += m_bbox.o m_cheat.o m_menu.o m_random.o m_recip.o p_ceilng.o p_doors.o p_enemy.o p_floor.o 
SRCS += p_genlin.o p_inter.o p_lights.o p_map.o p_maputl.o p_mobj.o p_plats.o p_pspr.o p_setup.o 
SRCS += p_sight.o p_spec.o p_switch.o p_telept.o p_tick.o p_user.o r_data.o r_draw.o 
SRCS += r_hotpath.iwram.o r_main.o r_patch.o r_plane.o r_things.o sounds.o s_sound.o 
SRCS += st_gfx.o st_lib.o st_stuff.o tables.o version.o v_video.o wi_stuff.o w_wad.o z_bmalloc.o 
SRCS += z_zone.o sdl.o
TARGET=doom

CFLAGS = -I../ -DHAVE_CONFIG_H -DHAVE_UNISTD_H -ggdb $(shell sdl2-config --cflags)
LDFLAGS = -lm -ggdb $(shell sdl2-config --libs)

$(TARGET): $(SRCS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: ../%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

clean:
	rm -f *.o
	rm -f $(TARGET)