/* End-to-end adapter checks: real SDL window/clipboard, synthetic SDL events. */
#include "../adapters/sdl3/srvrui_sdl3.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct desk_action key(struct srvrui_sdl3 *h, SDL_Scancode code, SDL_Keymod mod) {
    SDL_Event e = {0}; e.type=SDL_EVENT_KEY_DOWN; e.key.windowID=SDL_GetWindowID(h->window);
    e.key.scancode=code; e.key.mod=mod; return srvrui_sdl3_event(h,&e);
}
static struct desk_action text(struct srvrui_sdl3 *h, Uint32 type, const char *s) {
    SDL_Event e = {0}; e.type=type;
    if (type==SDL_EVENT_TEXT_INPUT) { e.text.windowID=SDL_GetWindowID(h->window); e.text.text=s; }
    else { e.edit.windowID=SDL_GetWindowID(h->window); e.edit.text=s; }
    return srvrui_sdl3_event(h,&e);
}
static struct desk_action button(struct srvrui_sdl3 *h, Uint32 type, float x, float y) {
    SDL_Event e = {0}; e.type=type; e.button.windowID=SDL_GetWindowID(h->window);
    e.button.button=SDL_BUTTON_LEFT; e.button.x=x*h->coordinate_scale; e.button.y=y*h->coordinate_scale;
    return srvrui_sdl3_event(h,&e);
}
static struct desk_action window_event(struct srvrui_sdl3 *h, Uint32 type) {
    SDL_Event e = {0}; e.type=type; e.window.windowID=SDL_GetWindowID(h->window);
    return srvrui_sdl3_event(h,&e);
}
static struct desk_action wheel(struct srvrui_sdl3 *h, float y, SDL_MouseWheelDirection direction) {
    SDL_Event e = {0}; e.type=SDL_EVENT_MOUSE_WHEEL; e.wheel.windowID=SDL_GetWindowID(h->window);
    e.wheel.mouse_x=30*h->coordinate_scale; e.wheel.mouse_y=90*h->coordinate_scale;
    e.wheel.y=y; e.wheel.direction=direction; return srvrui_sdl3_event(h,&e);
}
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *w=SDL_CreateWindow("SrvrUI test",240,200,SDL_WINDOW_HIDDEN); assert(w);
    char *saved=SDL_GetClipboardText(); assert(saved);
    struct desk_text_buffer model; char storage[32], history[32];
    assert(!desk_text_buffer_init(&model,storage,history,sizeof(storage)));
    struct desk_edit edit; desk_edit_init(&edit,&model,0);
    desk_edit_configure(&edit,(struct desk_rect){10,10,200,30},&desk_font_16,1);
    struct desk_button b={0}; desk_button_configure(&b,(struct desk_rect){10,45,100,28},1);
    struct desk_list list; desk_list_init(&list);
    struct desk_list_item items[30]; for (int i=0;i<30;i++) items[i]=(struct desk_list_item){.label=i==1?"Beta":"Alpha",.id=(uint64_t)i};
    assert(!desk_list_set_items(&list,items,30)); desk_list_configure(&list,(struct desk_rect){10,80,180,90},1);
    struct desk_table table; desk_table_init(&table);
    const struct desk_table_column columns[]={{.label="Name",.width=0}};
    const struct desk_table_row rows[]={ {.id=1,.cells={"Alpha"}}, {.id=2,.cells={"Gamma"}} };
    assert(!desk_table_columns(&table,columns,1)); assert(!desk_table_rows(&table,rows,2));
    desk_table_configure(&table,(struct desk_rect){10,175,180,20},1);
    struct desk_ui ui; desk_ui_init(&ui);
    assert(!desk_ui_add(&ui,1,DESK_CONTROL_EDIT,&edit,""));
    assert(!desk_ui_add(&ui,2,DESK_CONTROL_BUTTON,&b,"button"));
    assert(!desk_ui_add(&ui,3,DESK_CONTROL_LIST,&list,""));
    assert(!desk_ui_add(&ui,4,DESK_CONTROL_TABLE,&table,""));
    desk_ui_focus(&ui,1);
    struct srvrui_sdl3 h; assert(!srvrui_sdl3_init(&h,w,&ui));
    assert(h.text_active);

    key(&h,SDL_SCANCODE_A,SDL_KMOD_NONE); assert(!strcmp(model.data,""));
    text(&h,SDL_EVENT_TEXT_INPUT,"a"); assert(!strcmp(model.data,"a"));
    text(&h,SDL_EVENT_TEXT_EDITING,"\xc2\xb4"); assert(h.composing);
    key(&h,SDL_SCANCODE_BACKSPACE,SDL_KMOD_NONE); assert(!strcmp(model.data,"a"));
    text(&h,SDL_EVENT_TEXT_EDITING,"");
    text(&h,SDL_EVENT_TEXT_INPUT,"\xc3\xa9"); assert(!strcmp(model.data,"a\xc3\xa9"));
    assert(!h.composing);

    key(&h,SDL_SCANCODE_A,SDL_KMOD_GUI); assert(model.anchor!=model.caret);
    key(&h,SDL_SCANCODE_C,SDL_KMOD_GUI); char *copied=SDL_GetClipboardText();
    assert(copied && !strcmp(copied,"a\xc3\xa9")); SDL_free(copied);
    key(&h,SDL_SCANCODE_X,SDL_KMOD_GUI); assert(!strcmp(model.data,""));
    key(&h,SDL_SCANCODE_Z,SDL_KMOD_GUI); assert(!strcmp(model.data,"a\xc3\xa9"));
    key(&h,SDL_SCANCODE_Z,SDL_KMOD_GUI|SDL_KMOD_SHIFT); assert(!strcmp(model.data,""));
    key(&h,SDL_SCANCODE_V,SDL_KMOD_GUI); assert(!strcmp(model.data,"a\xc3\xa9"));
    key(&h,SDL_SCANCODE_A,SDL_KMOD_GUI);
    size_t old_anchor=model.anchor,old_caret=model.caret; uint64_t revision=model.revision;
    assert(SDL_SetClipboardText("abcdefghijklmnopqrstuvwxyz0123456789"));
    assert(key(&h,SDL_SCANCODE_V,SDL_KMOD_GUI).flags & DESK_EDIT_FULL);
    assert(!strcmp(model.data,"a\xc3\xa9") && model.anchor==old_anchor && model.caret==old_caret && model.revision==revision);
    assert(SDL_SetClipboardText("\xc0\xaf"));
    assert(key(&h,SDL_SCANCODE_V,SDL_KMOD_GUI).flags & DESK_EDIT_FULL);
    assert(!strcmp(model.data,"a\xc3\xa9") && model.anchor==old_anchor && model.caret==old_caret && model.revision==revision);

    key(&h,SDL_SCANCODE_TAB,SDL_KMOD_NONE); assert(ui.focus==1 && !h.text_active);
    key(&h,SDL_SCANCODE_TAB,SDL_KMOD_SHIFT); assert(ui.focus==0 && h.text_active);
    button(&h,SDL_EVENT_MOUSE_BUTTON_DOWN,20,55); assert(ui.capture==1 && b.armed);
    assert(!(button(&h,SDL_EVENT_MOUSE_BUTTON_UP,220,180).flags & DESK_WIDGET_ACTIVATED));
    assert(ui.capture==-1 && !b.armed);
    button(&h,SDL_EVENT_MOUSE_BUTTON_DOWN,20,55); assert(b.armed);
    window_event(&h,SDL_EVENT_WINDOW_FOCUS_LOST); assert(!b.armed && ui.capture==-1 && !h.down);
    window_event(&h,SDL_EVENT_WINDOW_FOCUS_GAINED);
    button(&h,SDL_EVENT_MOUSE_BUTTON_DOWN,20,55); assert(b.armed);
    SDL_Event motion={0}; motion.type=SDL_EVENT_MOUSE_MOTION; motion.motion.windowID=SDL_GetWindowID(w);
    motion.motion.x=20; motion.motion.y=55; motion.motion.state=0;
    assert(!(srvrui_sdl3_event(&h,&motion).flags & DESK_WIDGET_ACTIVATED)); assert(!b.armed && ui.capture==-1 && !h.down);

    assert(list.scroll.offset==0);
    wheel(&h,-0.01f,SDL_MOUSEWHEEL_NORMAL); assert(list.scroll.offset==0);
    wheel(&h,-0.03f,SDL_MOUSEWHEEL_NORMAL); assert(list.scroll.offset==1);
    wheel(&h,-1.0f,SDL_MOUSEWHEEL_NORMAL); assert(list.scroll.offset==33);
    wheel(&h,-1.0f,SDL_MOUSEWHEEL_FLIPPED); assert(list.scroll.offset==2);
    /* The positive fractional remainder offsets part of the reverse step;
     * truncation leaves a negative remainder until the next wheel event. */
    assert(h.wheel_y < -0.7 && h.wheel_y > -0.8);
    wheel(&h,-0.01f,SDL_MOUSEWHEEL_FLIPPED); assert(list.scroll.offset==1);
    desk_ui_focus(&ui,1); assert(!srvrui_sdl3_sync_text_input(&h));
    text(&h,SDL_EVENT_TEXT_EDITING,"pending"); assert(h.composing && h.text_focus==0);
    desk_ui_focus(&ui,3); assert(!srvrui_sdl3_sync_text_input(&h) && h.text_active);
    assert(!h.composing && h.text_focus==2);
    key(&h,SDL_SCANCODE_B,SDL_KMOD_NONE); assert(list.selected==-1);
    text(&h,SDL_EVENT_TEXT_EDITING,"b"); assert(h.composing);
    key(&h,SDL_SCANCODE_DOWN,SDL_KMOD_NONE); assert(list.selected==-1);
    text(&h,SDL_EVENT_TEXT_INPUT,"b"); assert(!h.composing && list.selected==1);
    text(&h,SDL_EVENT_TEXT_INPUT,"\xc0\xaf"); assert(list.selected==1);
    text(&h,SDL_EVENT_TEXT_EDITING,"pending"); assert(h.composing);
    desk_ui_focus(&ui,4); assert(!srvrui_sdl3_sync_text_input(&h) && h.text_active);
    assert(!h.composing && h.text_focus==3);
    key(&h,SDL_SCANCODE_G,SDL_KMOD_NONE); assert(table.body.selected==-1);
    text(&h,SDL_EVENT_TEXT_INPUT,"g"); assert(table.body.selected==1);
    text(&h,SDL_EVENT_TEXT_EDITING,"pending"); assert(h.composing);
    window_event(&h,SDL_EVENT_WINDOW_FOCUS_LOST);
    assert(!h.composing && !h.text_active && h.text_focus==-1);
    window_event(&h,SDL_EVENT_WINDOW_FOCUS_GAINED);
    assert(h.text_active && h.text_focus==3);
    desk_ui_focus(&ui,1); assert(!srvrui_sdl3_sync_text_input(&h));
    SDL_Event foreign={0}; foreign.type=SDL_EVENT_TEXT_INPUT; foreign.text.windowID=SDL_GetWindowID(w)+1; foreign.text.text="bad";
    assert(!srvrui_sdl3_event(&h,&foreign).flags); assert(!strcmp(model.data,"a\xc3\xa9"));
    foreign=(SDL_Event){0}; foreign.type=SDL_EVENT_WINDOW_FOCUS_LOST; foreign.window.windowID=SDL_GetWindowID(w)+1;
    assert(!srvrui_sdl3_event(&h,&foreign).flags && ui.active);

    uint32_t *before=h.pixels; int before_width=h.canvas.width;
    assert(SDL_SetWindowSize(w,9000,160));
    assert(window_event(&h,SDL_EVENT_WINDOW_RESIZED).flags & SRVRUI_SDL3_ERROR);
    assert(h.pixels==before && h.canvas.width==before_width);
    assert(SDL_SetWindowSize(w,180,160));
    assert(window_event(&h,SDL_EVENT_WINDOW_RESIZED).flags & SRVRUI_SDL3_LAYOUT);
    assert(h.canvas.width>0 && h.canvas.height>0 && h.canvas.pixels==h.pixels);
    desk_fill(&h.canvas,(struct desk_rect){0,0,h.canvas.width,h.canvas.height},0x12a4ef);
    assert(!srvrui_sdl3_present(&h));
    SDL_Surface *surface=SDL_GetWindowSurface(w); Uint8 r,g,blue,a;
    assert(surface && SDL_ReadSurfacePixel(surface,0,0,&r,&g,&blue,&a));
    assert(r==0x12 && g==0xa4 && blue==0xef);
    assert(SDL_SetClipboardText(saved)); SDL_free(saved);
    srvrui_sdl3_destroy(&h); SDL_DestroyWindow(w); SDL_Quit();
    puts("SDL3 adapter: PASS"); return 0;
}
