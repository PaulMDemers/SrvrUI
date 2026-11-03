/* SrvrUI host showcase. The application owns all state and the SDL event loop. */
#include <SDL3/SDL.h>
#include <srvros/desk_ui.h>
#include "srvrui_sdl3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SILVER=1, AERO, AQUA, CHECK, SLIDER, EDIT, LIST, BUTTON };
struct app {
    SDL_Window *window;
    FILE *trace;
    struct srvrui_sdl3 host;
    struct desk_ui ui;
    struct desk_theme theme;
    struct desk_check themes[3], check;
    struct desk_slider slider;
    struct desk_edit edit;
    struct desk_text_buffer text;
    struct desk_text_history journal;
    char text_data[2048], text_history[2048], journal_data[8192];
    struct desk_list list;
    struct desk_button button;
    unsigned style, clicks;
    char status[160];
};
static const struct desk_list_item items[] = {
    {"Aurora", "A quiet beginning", 1, 0}, {"Birch", "A familiar place", 2, 0},
    {"Cedar", "A small discovery", 3, 0}, {"Driftwood", "Collected by the sea", 4, 0},
    {"Ember", "A warm glow", 5, 0}, {"Fern", "A little green", 6, 0},
    {"Grove", "Room to grow", 7, 0}, {"Harbor", "A sheltered shore", 8, 0},
    {"Indigo", "A deep blue", 9, 0}, {"Juniper", "Evergreen", 10, 0},
    {"Kestrel", "A bird in flight", 11, 0}, {"Linden", "A shady tree", 12, 0},
    {"Meadow", "Open ground", 13, 0}, {"Northstar", "A guiding light", 14, 0},
    {"Orchard", "Fruit in season", 15, 0}, {"Pebble", "A smooth stone", 16, 0},
    {"Quarry", "A place of stone", 17, 0}, {"River", "Always moving", 18, 0},
    {"Sparrow", "A familiar visitor", 19, 0}, {"Tide", "The sea returns", 20, 0},
    {"Umber", "An earthy color", 21, 0}, {"Vale", "Between the hills", 22, 0},
    {"Willow", "A graceful tree", 23, 0}, {"Zenith", "The highest point", 24, 0}
};
static const char *names[] = {"Silver", "Aero", "Aqua"};
static struct desk_rect rect(int x,int y,int w,int h) { return (struct desk_rect){x,y,w,h}; }
static void layout(struct app *a) {
    int w=a->host.canvas.width, h=a->host.canvas.height, compact=w<700;
    int m=20, gap=12, inner=w-2*m;
    for (int i=0;i<3;i++)
        desk_check_configure(&a->themes[i],rect(m+i*105,80,100,28),1,a->style==(unsigned)i,1);
    desk_check_configure(&a->check,rect(m,126,145,28),1,a->check.checked,0);
    desk_slider_configure(&a->slider,rect(m+165,126,inner-165,28),1);
    if (compact) {
        int available=h-318;
        int eh=available/2, lh=available-eh;
        if(eh<65) eh=65;
        if(lh<65) lh=65;
        desk_edit_configure(&a->edit,rect(m,183,inner,eh),a->theme.body,1);
        desk_list_configure(&a->list,rect(m,183+eh+30,inner,lh),1);
    } else {
        int col=(inner-gap)/2, height=h-282;
        if(height<100) height=100;
        desk_edit_configure(&a->edit,rect(m,183,col,height),a->theme.body,1);
        desk_list_configure(&a->list,rect(m+col+gap,183,inner-col-gap,height),1);
    }
    desk_button_configure(&a->button,rect(m,h-75,145,30),1);
    desk_button_centered(&a->button,1);
    desk_ui_layout_changed(&a->ui);
    srvrui_sdl3_sync_text_input(&a->host);
}
static void draw(struct app *a) {
    struct desk_canvas *c=&a->host.canvas;
    int w=c->width,h=c->height, compact=w<700;
    desk_clip(c,rect(0,0,w,h));
    desk_fill(c,rect(0,0,w,h),a->theme.surface);
    desk_theme_gradient(c,rect(0,0,w,59),a->theme.chrome_top,a->theme.chrome_bottom);
    desk_text(c,a->theme.heading,20,10,"SrvrUI showcase",a->theme.text);
    desk_text(c,a->theme.body,20,37,"A small workspace for trying the controls",a->theme.muted);
    desk_text(c,a->theme.body,20,62,"Appearance",a->theme.text);
    desk_text(c,a->theme.body,20,109,"Options",a->theme.text);
    char value[48]; snprintf(value,sizeof(value),"Value: %d",a->slider.value);
    desk_text(c,a->theme.body,185,109,value,a->theme.text);
    desk_text(c,a->theme.body,20,163,"Notes",a->theme.text);
    int lx=compact?20:a->list.frame.x;
    int ly=compact?a->list.frame.y-20:163;
    desk_text(c,a->theme.body,lx,ly,"Collection",a->theme.text);
    desk_ui_draw(c,&a->ui,&a->theme);
    desk_label_ellipsis(c,rect(180,h-70,w-200,20),a->theme.body,a->status,a->theme.text);
    desk_label_ellipsis(c,rect(20,h-31,w-40,20),a->theme.body,
        "Tab to navigate  |  Type in Notes  |  Scroll the collection  |  Ctrl/Command-Z to undo",a->theme.muted);
}
static int paint(struct app *a) { draw(a); return srvrui_sdl3_present(&a->host); }
static void action(struct app *a,struct desk_action r) {
    if(r.flags&SRVRUI_SDL3_LAYOUT) layout(a);
    if(r.flags&DESK_EDIT_FULL) snprintf(a->status,sizeof(a->status),"Text rejected: invalid input or 2047-byte limit. Notes retained.");
    else if(r.id>=SILVER && r.id<=AQUA && (r.flags&DESK_WIDGET_CHANGED)) {
        a->style=r.id-SILVER;
        desk_theme_configure_style(&a->theme,a->style,0,1,16);
        for(int i=0;i<3;i++) desk_check_configure(&a->themes[i],a->themes[i].button.frame,1,a->style==(unsigned)i,1);
        snprintf(a->status,sizeof(a->status),"%s appearance selected.",names[a->style]);
    } else if(r.id==CHECK && (r.flags&DESK_WIDGET_CHANGED))
        snprintf(a->status,sizeof(a->status),"Option %s.",a->check.checked?"enabled":"disabled");
    else if(r.id==SLIDER && (r.flags&DESK_WIDGET_CHANGED))
        snprintf(a->status,sizeof(a->status),"Value set to %d.",a->slider.value);
    else if(r.id==LIST && (r.flags&DESK_WIDGET_CHANGED) && a->list.selected>=0)
        snprintf(a->status,sizeof(a->status),"Selected %s.",items[a->list.selected].label);
    else if(r.id==BUTTON && (r.flags&DESK_WIDGET_ACTIVATED)) {
        ++a->clicks;
        snprintf(a->status,sizeof(a->status),"Action completed %u time%s.",a->clicks,a->clicks==1?"":"s");
    }
    else if(r.id==EDIT && (r.flags&DESK_WIDGET_CHANGED))
        snprintf(a->status,sizeof(a->status),"Notes: %zu / 2047 bytes.",a->text.length);
}
static int init(struct app *a) {
    memset(a,0,sizeof(*a));
    if(!SDL_Init(SDL_INIT_VIDEO)) return -1;
    a->window=SDL_CreateWindow("SrvrUI showcase",900,650,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!a->window) return -1;
    if(!SDL_SetWindowMinimumSize(a->window,420,500)) return -1;
    desk_ui_init(&a->ui);
    if(desk_theme_configure_style(&a->theme,0,0,1,16) ||
       desk_text_buffer_init(&a->text,a->text_data,a->text_history,sizeof(a->text_data)) ||
       desk_text_buffer_history(&a->text,&a->journal,a->journal_data,sizeof(a->journal_data))) return -1;
    const char *initial="Welcome to SrvrUI.\n\nEdit these notes, select text, and try undo.\nSwitch appearances to see your work stay put.";
    if(desk_text_buffer_set(&a->text,initial,strlen(initial))<0) return -1;
    desk_edit_init(&a->edit,&a->text,1); desk_edit_scrollbars(&a->edit,1);
    desk_list_init(&a->list); if(desk_list_set_items(&a->list,items,(int)(sizeof(items)/sizeof(items[0])))<0) return -1;
    desk_slider_init(&a->slider); if(desk_slider_range(&a->slider,0,100,1)<0) return -1; desk_slider_set(&a->slider,50);
    for(int i=0;i<3;i++) if(desk_ui_add(&a->ui,SILVER+i,DESK_CONTROL_CHECK,&a->themes[i],names[i])) return -1;
    if(desk_ui_add(&a->ui,CHECK,DESK_CONTROL_CHECK,&a->check,"Enable option") ||
       desk_ui_add(&a->ui,SLIDER,DESK_CONTROL_SLIDER,&a->slider,NULL) ||
       desk_ui_add(&a->ui,EDIT,DESK_CONTROL_EDIT,&a->edit,NULL) ||
       desk_ui_add(&a->ui,LIST,DESK_CONTROL_LIST,&a->list,NULL) ||
       desk_ui_add(&a->ui,BUTTON,DESK_CONTROL_BUTTON,&a->button,"Try action")) return -1;
    snprintf(a->status,sizeof(a->status),"Ready to explore.");
    if(srvrui_sdl3_init(&a->host,a->window,&a->ui)<0) return -1;
    layout(a); return srvrui_sdl3_sync_text_input(&a->host);
}
static void close_app(struct app *a) { srvrui_sdl3_destroy(&a->host); if(a->window) SDL_DestroyWindow(a->window); SDL_Quit(); }
static int save_ppm(struct app *a,const char *path) {
    FILE *f=fopen(path,"wb"); if(!f) return -1;
    struct desk_canvas *c=&a->host.canvas;
    fprintf(f,"P6\n%d %d\n255\n",c->width,c->height);
    for(int y=0;y<c->height;y++) for(int x=0;x<c->width;x++) {
        uint32_t p=c->pixels[y*c->stride+x]; unsigned char rgb[3]={(unsigned char)(p>>16),(unsigned char)(p>>8),(unsigned char)p};
        if(fwrite(rgb,1,3,f)!=3) { fclose(f); return -1; }
    }
    return fclose(f);
}
static void trace_geometry(struct app *a,const char *reason) {
    int w=0,h=0,pw=0,ph=0;
    SDL_GetWindowSize(a->window,&w,&h); SDL_GetWindowSizeInPixels(a->window,&pw,&ph);
    fprintf(a->trace,"%s window=%dx%d pixels=%dx%d canvas=%dx%d display_scale=%.3f density=%.3f coordinate_scale=%.3f\n",
        reason,w,h,pw,ph,a->host.canvas.width,a->host.canvas.height,
        SDL_GetWindowDisplayScale(a->window),SDL_GetWindowPixelDensity(a->window),a->host.coordinate_scale);
}
static struct desk_action dispatch(struct app *a,SDL_Event *e) {
    int capture_before=a->ui.capture;
    struct desk_action r=srvrui_sdl3_event(&a->host,e); action(a,r);
    if(a->trace) {
        if(e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP)
            fprintf(a->trace,"button type=%u x=%.2f y=%.2f button=%u capture=%d->%d action=%u flags=0x%x slider=%d text_length=%zu\n",
                e->type,e->button.x,e->button.y,e->button.button,capture_before,a->ui.capture,r.id,r.flags,a->slider.value,a->text.length);
        else if(e->type==SDL_EVENT_MOUSE_MOTION)
            fprintf(a->trace,"motion x=%.2f y=%.2f state=0x%x capture=%d->%d action=%u flags=0x%x slider=%d text_length=%zu\n",
                e->motion.x,e->motion.y,e->motion.state,capture_before,a->ui.capture,r.id,r.flags,a->slider.value,a->text.length);
        if(r.flags&SRVRUI_SDL3_LAYOUT) trace_geometry(a,"resize");
        fflush(a->trace);
    }
    return r;
}
static int click(struct app *a,struct desk_rect r) {
    SDL_Event e; memset(&e,0,sizeof(e)); e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.windowID=SDL_GetWindowID(a->window); e.button.button=SDL_BUTTON_LEFT; e.button.x=(r.x+r.w/2)*a->host.coordinate_scale; e.button.y=(r.y+r.h/2)*a->host.coordinate_scale;
    dispatch(a,&e); e.type=SDL_EVENT_MOUSE_BUTTON_UP; return (dispatch(a,&e).flags&SRVRUI_SDL3_ERROR)?-1:0;
}
static int smoke(struct app *a,const char *dir) {
    if(!SDL_CreateDirectory(dir)) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    desk_text_buffer_select(&a->text,0,7);
    size_t anchor=a->text.anchor, caret=a->text.caret; int scroll=a->list.scroll.offset;
    for(int i=0;i<3;i++) {
        if(click(a,a->themes[i].button.frame)<0 || a->style!=(unsigned)i || a->text.anchor!=anchor || a->text.caret!=caret || a->list.scroll.offset!=scroll) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
        char path[1024]; if(snprintf(path,sizeof(path),"%s/showcase-%s.ppm",dir,names[i]) >= (int)sizeof(path)) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; } if(paint(a)<0 || save_ppm(a,path)<0) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    }
    if(click(a,a->check.button.frame)<0 || !a->check.checked || click(a,a->button.frame)<0 || a->clicks!=1) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    if(click(a,a->list.frame)<0 || a->list.selected<0) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    struct desk_rect thumb=desk_slider_thumb(&a->slider); SDL_Event drag; memset(&drag,0,sizeof(drag));
    drag.type=SDL_EVENT_MOUSE_BUTTON_DOWN; drag.button.windowID=SDL_GetWindowID(a->window); drag.button.button=SDL_BUTTON_LEFT;
    drag.button.x=(thumb.x+thumb.w/2)*a->host.coordinate_scale; drag.button.y=(thumb.y+thumb.h/2)*a->host.coordinate_scale;
    int old=a->slider.value; dispatch(a,&drag);
    memset(&drag,0,sizeof(drag)); drag.type=SDL_EVENT_MOUSE_MOTION; drag.motion.windowID=SDL_GetWindowID(a->window); drag.motion.state=SDL_BUTTON_LMASK;
    drag.motion.x=(a->slider.frame.x+a->slider.frame.w-5)*a->host.coordinate_scale; drag.motion.y=(thumb.y+thumb.h/2)*a->host.coordinate_scale; dispatch(a,&drag);
    if(a->slider.value==old) { fprintf(stderr,"Smoke drag did not change slider\n"); return -1; }
    float release_x=drag.motion.x, release_y=drag.motion.y;
    memset(&drag,0,sizeof(drag)); drag.type=SDL_EVENT_MOUSE_BUTTON_UP; drag.button.windowID=SDL_GetWindowID(a->window); drag.button.button=SDL_BUTTON_LEFT; drag.button.x=release_x; drag.button.y=release_y; dispatch(a,&drag);
    memset(&drag,0,sizeof(drag)); drag.type=SDL_EVENT_MOUSE_BUTTON_DOWN; drag.button.windowID=SDL_GetWindowID(a->window); drag.button.button=SDL_BUTTON_LEFT;
    drag.button.x=(a->button.frame.x+10)*a->host.coordinate_scale; drag.button.y=(a->button.frame.y+10)*a->host.coordinate_scale; dispatch(a,&drag);
    drag.type=SDL_EVENT_MOUSE_BUTTON_UP; drag.button.x=0; drag.button.y=0; dispatch(a,&drag);
    if(a->clicks!=1 || a->ui.capture!=-1) { fprintf(stderr,"Smoke outside release did not cancel button\n"); return -1; }
    if(click(a,a->edit.frame)<0) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    size_t length=a->text.length; SDL_Event e; memset(&e,0,sizeof(e)); e.type=SDL_EVENT_TEXT_INPUT; e.text.windowID=SDL_GetWindowID(a->window); e.text.text="X"; dispatch(a,&e);
    if(a->text.length!=length+1) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    memset(&e,0,sizeof(e)); e.type=SDL_EVENT_KEY_DOWN; e.key.windowID=SDL_GetWindowID(a->window); e.key.scancode=SDL_SCANCODE_Z; e.key.mod=SDL_KMOD_CTRL; dispatch(a,&e);
    if(a->text.length!=length) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    int previous_width=a->host.canvas.width;
    SDL_SetWindowSize(a->window,420,500); SDL_SyncWindow(a->window);
    memset(&e,0,sizeof(e)); e.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED; e.window.windowID=SDL_GetWindowID(a->window); e.window.data1=420; e.window.data2=500;
    if(dispatch(a,&e).flags&SRVRUI_SDL3_ERROR) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    char path[1024]; if(snprintf(path,sizeof(path),"%s/showcase-compact.ppm",dir) >= (int)sizeof(path)) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    if(a->host.canvas.width>=previous_width || a->host.canvas.width<1 || a->host.canvas.height<1) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    memset(&e,0,sizeof(e)); e.type=SDL_EVENT_MOUSE_WHEEL; e.wheel.windowID=SDL_GetWindowID(a->window); e.wheel.mouse_x=(a->list.frame.x+30)*a->host.coordinate_scale; e.wheel.mouse_y=(a->list.frame.y+30)*a->host.coordinate_scale; e.wheel.y=-2; dispatch(a,&e);
    scroll=a->list.scroll.offset; if(scroll<=0) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    anchor=a->text.anchor; caret=a->text.caret;
    if(click(a,a->themes[0].button.frame)<0 || a->style!=0 || a->list.scroll.offset!=scroll || a->text.anchor!=anchor || a->text.caret!=caret) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    if(paint(a)<0 || save_ppm(a,path)<0) { fprintf(stderr,"Smoke check failed at line %d\n",__LINE__); return -1; }
    puts("Smoke passed; wrote showcase-{Silver,Aero,Aqua,compact}.ppm"); return 0;
}
int main(int argc,char **argv) {
    int is_smoke=argc==3 && !strcmp(argv[1],"--smoke");
    int is_trace=argc==3 && !strcmp(argv[1],"--trace");
    if(argc>1 && !is_smoke && !is_trace) { fprintf(stderr,"Usage: %s [--smoke OUTPUT_DIRECTORY | --trace LOG_PATH]\n",argv[0]); return 2; }
    struct app a; if(init(&a)<0) { fprintf(stderr,"SDL showcase init: %s\n",SDL_GetError()); close_app(&a); return 1; }
    if(is_trace) { a.trace=fopen(argv[2],"w"); if(!a.trace) { perror("trace"); close_app(&a); return 1; } trace_geometry(&a,"init"); fflush(a.trace); }
    if(is_smoke) { int result=smoke(&a,argv[2]);  close_app(&a); return result<0; }
    int running=1, dirty=1, failed=0;
    while(running) {
        if(dirty) { if(paint(&a)<0) { failed=1; break; } dirty=0; }
        SDL_Event e; if(!SDL_WaitEvent(&e)) { failed=1; break; }
        do { struct desk_action r=dispatch(&a,&e); if(r.flags&SRVRUI_SDL3_QUIT) running=0; if(r.flags&SRVRUI_SDL3_ERROR) { running=0; failed=1; break; } if(r.flags || e.type==SDL_EVENT_WINDOW_EXPOSED) dirty=1; } while(SDL_PollEvent(&e));
    }
    if(failed) fprintf(stderr,"SDL showcase error: %s\n",SDL_GetError());
    if(a.trace && fclose(a.trace)) failed=1;
    close_app(&a); return failed;
}
