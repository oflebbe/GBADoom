

# PrBoom main executable


set(SOURCES
s_sound.c
i_video.c
r_data.c
i_system.c
tables.c
m_recip.c
p_ceilng.c
z_zone.c
p_tick.c
lprintf.c
m_menu.c
am_map.c
info.c
p_setup.c
hu_stuff.c
hu_lib.c
g_game.c
r_main.c
p_sight.c
v_video.c
p_switch.c
r_plane.c
wi_stuff.c
d_main.c
p_inter.c
m_cheat.c
f_finale.c
f_wipe.c
r_patch.c
st_stuff.c
m_bbox.c
p_genlin.c
r_things.c
w_wad.c
doom_iwad.c
r_draw.c
st_gfx.c
p_map.c
d_client.c
st_lib.c
version.c
p_floor.c
p_doors.c
p_user.c
p_telept.c
p_maputl.c
global_data.c
i_audio.c
p_plats.c
sounds.c
p_pspr.c
z_bmalloc.c
d_items.c
p_spec.c
p_mobj.c
gfx/stbar.h
r_hotpath.iwram.c
i_main.c
p_lights.c
p_enemy.c
m_random.c
i_system_sparkfun_thingplus.c
st7789.c
)



# PICO DOOM leave out all windows/mac specific stuff 
add_executable(pico_doom ${SOURCES})
pico_generate_pio_header(pico_doom ${CMAKE_CURRENT_LIST_DIR}/st7789_parallel.pio)
target_include_directories(pico_doom PRIVATE ../include)
target_link_libraries(pico_doom pico_stdlib hardware_divider hardware_spi hardware_dma hardware_pio  hardware_pwm)
pico_set_linker_script( pico_doom ${CMAKE_SOURCE_DIR}/source/sparkfun-thingplus.ld)
pico_enable_stdio_usb(pico_doom 1)
pico_enable_stdio_uart(pico_doom 0)
pico_add_extra_outputs(pico_doom)
