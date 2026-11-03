#ifndef SRVRUI_SDL3_H
#define SRVRUI_SDL3_H
#include <SDL3/SDL.h>
#include <srvros/desk_ui.h>

/* Optional host adapter. Window/UI are borrowed; canvas storage is owned here.
 * All calls run on the application's main thread. No hidden event loop. */
#define SRVRUI_SDL3_LAYOUT (1u << 16)
#define SRVRUI_SDL3_QUIT (1u << 17)
#define SRVRUI_SDL3_ERROR (1u << 18)
struct srvrui_sdl3 {
    SDL_Window *window;
    struct desk_ui *ui;
    struct desk_canvas canvas;
    uint32_t *pixels;
    float coordinate_scale;
    double wheel_x, wheel_y;
    int down, composing;
    int text_active, text_focus; /* Focused control index, or -1. */
};
/* SDL video and window creation remain with the caller. */
int srvrui_sdl3_init(struct srvrui_sdl3 *host, SDL_Window *window, struct desk_ui *ui);
/* Explicit 1x/2x backing; logical layout/input are unchanged. Default is 1x.
 * Success cancels capture and requires a redraw; failure retains the canvas. */
int srvrui_sdl3_set_density(struct srvrui_sdl3 *host, unsigned density);
void srvrui_sdl3_destroy(struct srvrui_sdl3 *host);
/* Resize invalidates old canvas pointers only on success; returns layout flag. */
struct desk_action srvrui_sdl3_event(struct srvrui_sdl3 *host, const SDL_Event *event);
/* Call after application-driven focus/layout changes. */
int srvrui_sdl3_sync_text_input(struct srvrui_sdl3 *host);
int srvrui_sdl3_present(struct srvrui_sdl3 *host);
#endif
