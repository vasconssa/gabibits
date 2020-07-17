#include "device/vk_device.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "world/renderer.h"
#include "world/world.h"
#include "camera.h"
#include "macros.h"
#include "device/types.h"
#include "device/input_manager.h"
#include "sx/timer.h"
#include <time.h>
#include <limits.h>
#include <math.h>


GameDevice game_device;

extern bool next_event(OsEvent* ev);
extern void create_window(u_int16_t width, u_int16_t height);
/*extern void set_fullscreen(bool full);*/
/*extern void set_title (const char* title);*/

bool process_events(InputManager* input_manager, World* world, bool vsync) {
    bool exit = false;
    bool reset = false;
    OsEvent event;
	while (next_event(&event))
	{
		switch (event.type)
		{
		case OST_BUTTON:
		case OST_AXIS:
		case OST_STATUS:
            input_manager_read(input_manager, &event);
			break;

		case OST_RESOLUTION:
			reset   = true;
            game_device.viewport.xmax = event.resolution.width;
            game_device.viewport.ymax = event.resolution.height;
            game_device.camera.cam.viewport = game_device.viewport;
            renderer_resize(world->renderer, event.resolution.width, event.resolution.height);
			break;

		case OST_EXIT:
			exit = true;
			break;

		case OST_PAUSE:
			/*pause();*/
			break;

		case OST_RESUME:
			/*unpause();*/
			break;

		case OST_TEXT:
			break;

		default:
			break;
		}
	}

    return exit;
}

int device_run(void* win, void* data) {
    sx_tm_init();
    uint64_t last_time = 0;
    DeviceWindow* window = (DeviceWindow*)win;
    game_device.viewport.xmin = 0.0;
    game_device.viewport.xmax = window->width;
    game_device.viewport.ymin = 0.0;
    game_device.viewport.ymax = window->height;

    const sx_alloc* alloc = sx_alloc_malloc();

    World* world = create_world(alloc);
    game_device.input_manager = create_input_manager(alloc);
    fps_init(&game_device.camera, 60, game_device.viewport, 1.1, 8096.0);
    /*game_device.camera.cam.pos.z = -14378137;*/
    game_device.camera.cam.pos.x = 1800.26;
    game_device.camera.cam.pos.y = 1205.66;
    game_device.camera.cam.pos.z = 1899.47;

    /*game_device.camera.cam.pos.x = 15.2;*/
    /*game_device.camera.cam.pos.y = 16.5;*/
    /*game_device.camera.cam.pos.z = 17.4;*/

    /*struct nk_font_atlas *atlas;*/
    /*struct nk_context* ctx = nk_gui_init();*/
    /*struct nk_context* ctx = (struct nk_context*)data;*/
    /*nk_font_stash_begin(&atlas);*/
    /*struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "DroidSans.ttf", 14, 0);*/
    /*nk_style_set_font(ctx, &droid->handle);*/
    /*nk_font_stash_end();*/
    float dt = 0;
    /*struct nk_colorf bg;*/
    /*bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;*/
    game_init(world);
    while(!process_events(game_device.input_manager, world, false)) {
        /*nk_input_begin(ctx);*/
        sx_vec3 m = mouse_axis(game_device.input_manager, MA_CURSOR);
        /*nk_input_motion(ctx, (int)m.x, (int)m.y);*/
        /*nk_input_button(ctx, NK_BUTTON_LEFT, (int)m.x, (int)m.y, mouse_button(game_device.input_manager, MB_LEFT));*/
        /*nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)m.x, (int)m.y, mouse_button(game_device.input_manager, MB_MIDDLE));*/
        /*nk_input_button(ctx, NK_BUTTON_RIGHT, (int)m.x, (int)m.y, mouse_button(game_device.input_manager, MB_RIGHT));*/
        /*nk_input_end(ctx);*/
         /* GUI */
        /*if (nk_begin(ctx, "Demo", nk_rect(50, 50, 200, 200),*/
            /*NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|*/
            /*NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))*/
        /*{*/
            /*enum {EASY, HARD};*/
            /*static int op = EASY;*/

            /*nk_layout_row_static(ctx, 30, 80, 1);*/
            /*if (nk_button_label(ctx, "button"))*/
                /*fprintf(stdout, "button pressed\n");*/
            /*nk_layout_row_dynamic(ctx, 30, 2);*/
            /*if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;*/
            /*if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;*/
            /*nk_layout_row_dynamic(ctx, 25, 1);*/
            /*nk_property_float(ctx,"altitude", -54378137.0, &game_device.camera.cam.pos.z, 54378137.0, 1000000, 1);*/

        /*}*/
        /*nk_end(ctx);*/
        dt = (float)sx_tm_sec(sx_tm_laptime(&last_time));
        fps_camera_update(&game_device.camera, dt);
        world_update(world);
        /*vk_renderer_draw(dt);*/
        input_manager_update(game_device.input_manager);
        /*printf("cam_position: (%f, %f, %f)\n", game_device.camera.cam.pos.x, game_device.camera.cam.pos.y, game_device.camera.cam.pos.z);*/
        /*printf("fps: %lf\n", 1.0/dt);*/
        /*dt2 = sx_tm_sec(sx_tm_diff(t2, t));*/
    }
    world_destroy(world);
    sx_free(alloc, game_device.input_manager->keyboard);
    sx_free(alloc, game_device.input_manager->mouse);
    sx_free(alloc, game_device.input_manager->touch);
    sx_free(alloc, game_device.input_manager);

    return 0;
}

GameDevice* device() {
    return &game_device;
}
