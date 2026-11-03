#include <srvros/desk_widgets.h>

void desk_check_configure(struct desk_check *c,struct desk_rect frame,int enabled,int checked,int radio) {
    if(c->checked!=(checked!=0) || c->radio!=(radio!=0))
        desk_button_event(&c->button,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    desk_button_configure(&c->button,frame,enabled);
    c->checked=checked!=0;c->radio=radio!=0;
}
unsigned desk_check_event(struct desk_check *c,struct desk_widget_event event) {
    unsigned flags=desk_button_event(&c->button,event);
    if(flags&DESK_WIDGET_ACTIVATED) {
        int next=c->radio?1:!c->checked;
        if(next!=c->checked) { c->checked=next;flags|=DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW; }
    }
    return flags;
}
static void circle(struct desk_canvas *c,int x,int y,int radius,uint32_t color) {
    if(desk_canvas_scale(c)==2) {
        /* Preserve the logical silhouette, sample its circle on physical cells. */
        struct desk_rect r=desk_canvas_physical_rect(c,desk_intersect(c->clip,
            (struct desk_rect){x-radius,y-radius,2*radius+1,2*radius+1}));
        int64_t cx=(int64_t)x*4+2,cy=(int64_t)y*4+2,rr=radius*4+2;
        for(int py=r.y;py<r.y+r.h;py++)for(int px=r.x;px<r.x+r.w;px++) {
            int64_t dx=2*(int64_t)px+1-cx,dy=2*(int64_t)py+1-cy;
            if(dx*dx+dy*dy<=rr*rr) {
                c->pixels[(size_t)py*c->stride+px]=color&0xffffff;c->pixel_writes++;
            }
        }
        return;
    }
    for(int dy=-radius;dy<=radius;dy++) for(int dx=-radius;dx<=radius;dx++)
        if(dx*dx+dy*dy<=radius*radius) desk_fill(c,(struct desk_rect){x+dx,y+dy,1,1},color);
}
void desk_check_draw(struct desk_canvas *c,const struct desk_check *check,const char *label,const struct desk_theme *t) {
    const struct desk_button *b=&check->button;struct desk_rect r=b->frame,old=c->clip;
    if(r.w<30 || r.h<20) return;
    desk_clip(c,desk_intersect(old,r));
    uint32_t ink=b->enabled?t->text:t->muted;
    uint32_t edge=b->focused?t->accent:t->border;
    int x=r.x+3,y=r.y+(r.h-18)/2;
    if(b->focused) {
        desk_fill(c,(struct desk_rect){r.x,r.y,r.w,1},t->accent);
        desk_fill(c,(struct desk_rect){r.x,r.y+r.h-1,r.w,1},t->accent);
        desk_fill(c,(struct desk_rect){r.x,r.y,1,r.h},t->accent);
        desk_fill(c,(struct desk_rect){r.x+r.w-1,r.y,1,r.h},t->accent);
    }
    uint32_t fill=b->armed && b->inside?t->selected:(b->enabled?desk_theme_field(t):t->track);
    if(check->radio) {
        circle(c,x+8,y+8,8,edge);circle(c,x+8,y+8,6,fill);
        if(check->checked) circle(c,x+8,y+8,4,b->enabled?t->accent:t->muted);
    } else {
        desk_theme_panel(c,(struct desk_rect){x,y,17,17},t,b->armed && b->inside,b->focused);
        if(t->style==DESK_THEME_SILVER) desk_fill(c,(struct desk_rect){x+2,y+2,13,13},fill);
        else {
            if(check->checked && b->enabled) fill=t->accent;
            desk_theme_glass(c,(struct desk_rect){x+2,y+2,13,13},fill,fill,255,t->style==DESK_THEME_AQUA?3:2);
        }
        if(check->checked) {
            uint32_t color=b->enabled?(t->style==DESK_THEME_SILVER?t->accent:0xffffff):t->muted;
            desk_checkmark_draw(c,x+3,y+5,color);
        }
    }
    if(r.w>30 && r.h>=t->body->line_height) desk_label_draw(c,(struct desk_rect){r.x+28,r.y+(r.h-t->body->line_height)/2,r.w-30,t->body->line_height},t->body,label,ink);
    c->clip=old;
}
