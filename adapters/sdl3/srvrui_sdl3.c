#include "srvrui_sdl3.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static struct desk_control *focused_text_control(struct srvrui_sdl3 *h) {
    if (!h->ui->active || h->ui->focus < 0 || (unsigned)h->ui->focus >= h->ui->count) return NULL;
    struct desk_control *c = &h->ui->controls[h->ui->focus];
    if (c->kind == DESK_CONTROL_EDIT) {
        struct desk_edit *e = c->state;
        return e->enabled && e->focused ? c : NULL;
    }
    if (c->kind == DESK_CONTROL_LIST) {
        struct desk_list *l = c->state;
        return l->enabled && l->focused ? c : NULL;
    }
    if (c->kind == DESK_CONTROL_TABLE) {
        struct desk_table *t = c->state;
        return t->body.enabled && t->body.focused ? c : NULL;
    }
    return NULL;
}

static struct desk_edit *focused_edit(struct srvrui_sdl3 *h) {
    struct desk_control *c = focused_text_control(h);
    return c && c->kind == DESK_CONTROL_EDIT ? c->state : NULL;
}

static int resize_canvas(struct srvrui_sdl3 *h) {
    int w, height;
    if (!SDL_GetWindowSize(h->window, &w, &height)) return -1;
    float density = SDL_GetWindowPixelDensity(h->window);
    float scale = SDL_GetWindowDisplayScale(h->window);
    float coordinates = density > 0 ? scale / density : 1;
    if (!isfinite(coordinates) || coordinates <= 0) coordinates = 1;
    double width = floor(w / (double)coordinates), rows = floor(height / (double)coordinates);
    if (width < 1 || rows < 1 || width > 8192 || rows > 8192) {
        SDL_SetError("SrvrUI canvas dimensions must be between 1 and 8192");
        return -1;
    }
    int iw = (int)width, ih = (int)rows;
    if (h->pixels && iw == h->canvas.width && ih == h->canvas.height) {
        h->coordinate_scale = coordinates;
        return 0;
    }
    uint32_t *pixels = calloc((size_t)iw * (size_t)ih, sizeof(*pixels));
    if (!pixels) { SDL_SetError("SrvrUI canvas allocation failed"); return -1; }
    struct desk_canvas canvas;
    if (desk_canvas_init(&canvas, pixels, iw, ih, iw)) { free(pixels); return -1; }
    free(h->pixels);
    h->pixels = pixels;
    h->canvas = canvas;
    h->coordinate_scale = coordinates;
    return 0;
}

int srvrui_sdl3_sync_text_input(struct srvrui_sdl3 *h) {
    struct desk_edit *e = focused_edit(h);
    struct desk_control *c = focused_text_control(h);
    int target = c ? h->ui->focus : -1;
    if (target != h->text_focus) {
        h->composing = 0;
        if (h->text_active) {
            if (!SDL_StopTextInput(h->window)) return -1;
            h->text_active = 0;
            h->text_focus = -1;
        }
        if (target >= 0) {
            if (!SDL_StartTextInput(h->window)) return -1;
            h->text_active = 1;
            h->text_focus = target;
        }
    }
    if (c) {
        struct desk_rect r = e ? desk_edit_viewport(e) :
            c->kind == DESK_CONTROL_LIST ? ((struct desk_list *)c->state)->frame :
            ((struct desk_table *)c->state)->frame;
        float s = h->coordinate_scale;
        SDL_Rect area = {(int)(r.x*s), (int)(r.y*s), (int)(r.w*s), (int)(r.h*s)};
        if (!SDL_SetTextInputArea(h->window, &area, 0)) return -1;
    }
    return 0;
}

int srvrui_sdl3_init(struct srvrui_sdl3 *h, SDL_Window *window, struct desk_ui *ui) {
    if (!h || !window || !ui) { SDL_SetError("SrvrUI requires a window and UI"); return -1; }
    *h = (struct srvrui_sdl3){.window=window, .ui=ui, .coordinate_scale=1, .text_focus=-1};
    if (resize_canvas(h) || srvrui_sdl3_sync_text_input(h)) {
        srvrui_sdl3_destroy(h);
        return -1;
    }
    return 0;
}

void srvrui_sdl3_destroy(struct srvrui_sdl3 *h) {
    if (!h) return;
    if (h->text_active) (void)SDL_StopTextInput(h->window);
    if (h->down) (void)SDL_CaptureMouse(false);
    if (h->ui) (void)desk_ui_pointer(h->ui, (struct desk_widget_event){.type=DESK_WIDGET_CANCEL}, 0);
    free(h->pixels);
    *h = (struct srvrui_sdl3){0};
}

int srvrui_sdl3_present(struct srvrui_sdl3 *h) {
    /* Never retain the SDL surface across resizes. SrvrUI's high byte is unused,
     * not alpha: explicitly use XRGB8888 and convert to the host format. */
    SDL_Surface *target = SDL_GetWindowSurface(h->window);
    if (!target) return -1;
    SDL_Surface *source = SDL_CreateSurfaceFrom(h->canvas.width, h->canvas.height,
        SDL_PIXELFORMAT_XRGB8888, h->pixels, h->canvas.stride * (int)sizeof(uint32_t));
    if (!source) return -1;
    bool ok = SDL_BlitSurfaceScaled(source, NULL, target, NULL, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(source);
    return ok && SDL_UpdateWindowSurface(h->window) ? 0 : -1;
}

static uint32_t modifiers(SDL_Keymod mod) {
    return ((mod & SDL_KMOD_CTRL) ? SRV_KEY_CTRL : 0) |
        ((mod & SDL_KMOD_SHIFT) ? SRV_KEY_SHIFT : 0) |
        ((mod & SDL_KMOD_ALT) ? SRV_KEY_ALT : 0) |
        ((mod & SDL_KMOD_GUI) ? SRV_KEY_META : 0) |
        ((mod & SDL_KMOD_CAPS) ? SRV_KEY_CAPS : 0);
}

static int position(float value, float scale) {
    double n = floor(value / (double)scale);
    if (!isfinite(n)) return 0;
    if (n < INT32_MIN) return INT32_MIN;
    if (n > INT32_MAX) return INT32_MAX;
    return (int)n;
}

static unsigned insert_text(struct desk_edit *e, const char *text) {
    if (!text) return 0;
    size_t n = strlen(text);
    if (!desk_text_utf8_valid(text, n)) return DESK_EDIT_FULL;
    /* Match the editor's single-line contract; reject control characters rather
     * than hiding newlines in a field. Multiline accepts tabs and newlines. */
    for (size_t i=0; i<n; ++i) {
        unsigned char c = (unsigned char)text[i];
        if ((c < 32 && !(e->multiline && (c == '\n' || c == '\t'))) || c == 127)
            return DESK_EDIT_FULL;
    }
    int result = desk_text_buffer_insert(e->text, text, n);
    if (result < 0) return DESK_EDIT_FULL;
    if (!result) return 0;
    e->preferred_x = -1;
    desk_edit_reveal(e);
    return DESK_WIDGET_CHANGED | DESK_WIDGET_REDRAW;
}

static unsigned clipboard(struct desk_edit *e, unsigned flags) {
    if (flags & DESK_EDIT_PASTE) {
        char *text = SDL_GetClipboardText();
        if (!text) return SRVRUI_SDL3_ERROR;
        unsigned result = insert_text(e, text);
        SDL_free(text);
        return result;
    }
    struct desk_text_buffer *t = e->text;
    size_t lo = t->caret < t->anchor ? t->caret : t->anchor;
    size_t hi = t->caret > t->anchor ? t->caret : t->anchor;
    if (lo == hi) return 0;
    char *copy = malloc(hi-lo+1);
    if (!copy) { SDL_SetError("SrvrUI clipboard allocation failed"); return SRVRUI_SDL3_ERROR; }
    memcpy(copy, t->data+lo, hi-lo);
    copy[hi-lo] = 0;
    bool ok = SDL_SetClipboardText(copy);
    free(copy);
    if (!ok) return SRVRUI_SDL3_ERROR;
    /* A failed copy must never destroy the selected text. */
    if (flags & DESK_EDIT_CUT) return insert_text(e, "");
    return 0;
}

static int wheel_delta(double *rest, double delta) {
    if (!isfinite(delta)) return 0;
    *rest += delta;
    if (*rest > 8192) *rest = 8192;
    if (*rest < -8192) *rest = -8192;
    int whole = (int)*rest;
    *rest -= whole;
    return whole;
}

struct desk_action srvrui_sdl3_event(struct srvrui_sdl3 *h, const SDL_Event *ev) {
    struct desk_action result = {0};
    SDL_WindowID id = SDL_GetWindowID(h->window);
    SDL_WindowID event_id = 0;
    switch (ev->type) {
    case SDL_EVENT_QUIT: result.flags = SRVRUI_SDL3_QUIT; return result;
    case SDL_EVENT_MOUSE_MOTION: event_id=ev->motion.windowID; break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: event_id=ev->button.windowID; break;
    case SDL_EVENT_MOUSE_WHEEL: event_id=ev->wheel.windowID; break;
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: event_id=ev->key.windowID; break;
    case SDL_EVENT_TEXT_INPUT: event_id=ev->text.windowID; break;
    case SDL_EVENT_TEXT_EDITING: event_id=ev->edit.windowID; break;
    default:
        if (ev->type >= SDL_EVENT_WINDOW_FIRST && ev->type <= SDL_EVENT_WINDOW_LAST)
            event_id=ev->window.windowID;
        else return result;
    }
    if (event_id != id) return result;
    switch (ev->type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: result.flags=SRVRUI_SDL3_QUIT; break;
    case SDL_EVENT_WINDOW_RESIZED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        result.flags=resize_canvas(h) ? SRVRUI_SDL3_ERROR : SRVRUI_SDL3_LAYOUT | DESK_WIDGET_REDRAW;
        h->down=0; h->wheel_x=h->wheel_y=0;
        (void)SDL_CaptureMouse(false);
        result.flags |= desk_ui_pointer(h->ui,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},0).flags;
        break;
    case SDL_EVENT_WINDOW_EXPOSED: result.flags=DESK_WIDGET_REDRAW; break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        h->down=h->composing=0; h->wheel_x=h->wheel_y=0;
        (void)SDL_CaptureMouse(false);
        result.flags=desk_ui_window_focus(h->ui,0) | DESK_WIDGET_REDRAW;
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        result.flags=desk_ui_window_focus(h->ui,1) | DESK_WIDGET_REDRAW;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.button != SDL_BUTTON_LEFT) break;
        h->down=ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (h->down) h->composing=0;
        result=desk_ui_pointer(h->ui,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,
            .x=position(ev->button.x,h->coordinate_scale), .y=position(ev->button.y,h->coordinate_scale),
            .down=h->down,.pressed=h->down},ev->button.timestamp/1000);
        /* SDL also captures automatically while held on supported backends. */
        (void)SDL_CaptureMouse(h->down && h->ui->capture >= 0);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        /* The state in the event repairs a missed release, without activating. */
        if (h->down && !(ev->motion.state & SDL_BUTTON_LMASK)) {
            h->down=0;
            result=desk_ui_pointer(h->ui,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},0);
            (void)SDL_CaptureMouse(false);
        } else {
            result=desk_ui_pointer(h->ui,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,
                .x=position(ev->motion.x,h->coordinate_scale), .y=position(ev->motion.y,h->coordinate_scale),
                .down=h->down},ev->motion.timestamp/1000);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        if (!h->ui->active) break;
        double sign=ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        int dx=wheel_delta(&h->wheel_x,ev->wheel.x*sign*32);
        int dy=wheel_delta(&h->wheel_y,-ev->wheel.y*sign*32);
        result=desk_ui_scroll(h->ui,position(ev->wheel.mouse_x,h->coordinate_scale),
            position(ev->wheel.mouse_y,h->coordinate_scale),dx,dy);
        break;
    }
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: {
        /* Composing keys belong to the native input method, not the document. */
        if (h->composing && !(ev->key.mod & (SDL_KMOD_CTRL|SDL_KMOD_GUI))) break;
        struct srv_key_event key={.abi_version=SRV_KEY_EVENT_VERSION,.struct_size=sizeof(key),
            .time_us=ev->key.timestamp/1000,.usage=(uint32_t)ev->key.scancode,
            .modifiers=modifiers(ev->key.mod),.flags=(ev->type==SDL_EVENT_KEY_DOWN?SRV_KEY_DOWN_EVENT:0) |
                (ev->key.repeat?SRV_KEY_REPEAT:0)};
        /* Physical events deliberately carry no printable codepoint. */
        result=desk_ui_key(h->ui,&key);
        struct desk_edit *e=focused_edit(h);
        if (e && (result.flags & (DESK_EDIT_COPY|DESK_EDIT_CUT|DESK_EDIT_PASTE)))
            result.flags |= clipboard(e,result.flags);
        break;
    }
    case SDL_EVENT_TEXT_EDITING:
        if (focused_text_control(h)) h->composing=ev->edit.text && ev->edit.text[0];
        break;
    case SDL_EVENT_TEXT_INPUT: {
        struct desk_edit *e=focused_edit(h);
        h->composing=0;
        if (e) {
            result.id=h->ui->controls[h->ui->focus].id;
            result.flags=insert_text(e,ev->text.text);
        } else if (focused_text_control(h) && ev->text.text &&
                   desk_text_utf8_valid(ev->text.text,strlen(ev->text.text))) {
            const char *p=ev->text.text;
            while (*p) {
                struct srv_key_event key={.abi_version=SRV_KEY_EVENT_VERSION,
                    .struct_size=sizeof(key), .time_us=ev->text.timestamp/1000,
                    .flags=SRV_KEY_DOWN_EVENT, .codepoint=desk_utf8_next(&p)};
                struct desk_action next=desk_ui_key(h->ui,&key);
                result.id=next.id;
                result.flags|=next.flags;
            }
        }
        break;
    }
    default: break;
    }
    if (srvrui_sdl3_sync_text_input(h)) result.flags |= SRVRUI_SDL3_ERROR;
    return result;
}
