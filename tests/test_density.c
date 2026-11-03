#include <srvros/desk_widgets.h>
#include <srvros/desk_image.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { W=103,H=83,MAX_STRIDE=W*2+7,MAX_ROWS=H*2 };
static uint32_t full[MAX_STRIDE*MAX_ROWS+2], tiled[MAX_STRIDE*MAX_ROWS+2];
static uint32_t source[64*48];
static void scene(struct desk_canvas *c,unsigned style) {
    struct desk_theme theme;assert(!desk_theme_configure_style(&theme,style,1,2,16));
    desk_fill(c,(struct desk_rect){0,0,W,H},0xf1e3d5);
    desk_theme_gradient(c,(struct desk_rect){0,-INT32_MAX+10,W,INT32_MAX},0x123456,0xabcdef);
    desk_theme_gradient(c,(struct desk_rect){-8,-5,130,43},0xffffff,0x193769);
    desk_theme_glass(c,(struct desk_rect){-3,19,72,31},0xbadcea,0x123456,184,8);
    desk_theme_panel(c,(struct desk_rect){50,5,51,31},&theme,1,0);
    desk_text(c,&desk_font_16,-3,0,"Ag é Ω\nText",0x102030);
    desk_text(c,&desk_font_20,9,31,"Read",theme.text);
    desk_text(c,&desk_font_mono_16,0,58,"box ┌─┐",theme.text);
    desk_text(c,&desk_font_digits_32,60,48,"7",theme.text);
    struct desk_image im;desk_image_init(&im);
    assert(!desk_image_set(&im,source,64,48));
    desk_image_configure(&im,(struct desk_rect){65,30,32,26},1);
    desk_image_draw(c,&im,&theme);
    struct desk_check check={0};
    desk_check_configure(&check,(struct desk_rect){1,45,35,25},1,1,0);
    desk_check_draw(c,&check,"",&theme);
    desk_check_configure(&check,(struct desk_rect){35,45,35,25},1,1,1);
    desk_check_draw(c,&check,"",&theme);
    desk_fill(c,(struct desk_rect){INT32_MIN,0,INT32_MAX,5},0);
    desk_text(c,&desk_font_16,INT32_MIN,INT32_MIN,"x\nx",0);
    desk_text(c,&desk_font_16,INT32_MAX,INT32_MAX,"x\nx",0);
}

static uint64_t hash(const uint32_t *p,size_t count) {
    uint64_t h=14695981039346656037ull;
    for(size_t i=0;i<count;i++)for(unsigned b=0;b<4;b++)h=(h^((p[i]>>(b*8))&255))*1099511628211ull;
    return h;
}

int main(void) {
    for(unsigned i=0;i<64*48;i++)source[i]=((i%3?255u:137u)<<24)|((i*173u)&0xffffff);
    for(unsigned density=1;density<=2;density++)for(unsigned style=0;style<3;style++) {
        int stride=W*density+7,rows=H*density;
        size_t count=(size_t)stride*rows+2;
        for(size_t i=0;i<count;i++)full[i]=tiled[i]=0x91abcdef;
        struct desk_canvas a,b;
        assert(!desk_canvas_init_scaled(&a,full+1,W,H,stride,density));
        assert(!desk_canvas_init_scaled(&b,tiled+1,W,H,stride,density));
        scene(&a,style);
        for(int y=0;y<H;y+=7)for(int x=0;x<W;x+=11) {
            desk_clip(&b,(struct desk_rect){x,y,11,7});scene(&b,style);
        }
        assert(!memcmp(full,tiled,count*sizeof(*full)));
        assert(a.pixel_writes==b.pixel_writes);
        assert(full[0]==0x91abcdef && full[count-1]==0x91abcdef);
        for(int y=0;y<rows;y++)for(int x=W*density;x<stride;x++)assert(full[1+y*stride+x]==0x91abcdef);
        assert(desk_canvas_pixel_width(&a)==W*(int)density && desk_canvas_pixel_height(&a)==rows);
        struct desk_rect physical=desk_canvas_physical_rect(&a,(struct desk_rect){-2,3,5,2});
        assert(physical.x==0 && physical.y==3*(int)density && physical.w==3*(int)density && physical.h==2*(int)density);
        /* Captured by compiling this scene against merged SrvrUI 76deeb30. */
        static const uint64_t reference[]={0x55302883b2d27630ull,0x7be08c2bd784d8b0ull,0x9d42c3086192cbc6ull};
        if(density==1)assert(hash(full,count)==reference[style]);
    }
    struct desk_canvas c,prior;assert(!desk_canvas_init(&c,full,3,3,3));prior=c;
    assert(desk_canvas_init_scaled(&c,full,3,3,6,0)<0);
    assert(desk_canvas_init_scaled(&c,full,3,3,6,3)<0);
    assert(desk_canvas_init_scaled(&c,full,3,3,5,2)<0);
    assert(desk_canvas_init_scaled(&c,full,4097,3,8192,2)<0);
    assert(desk_canvas_init_scaled(&c,full,3,INT32_MAX,6,2)<0);
    assert(!memcmp(&c,&prior,sizeof(c)));
    c.scale=0;assert(desk_canvas_scale(&c)==1);
    memset(full,0,sizeof(full));desk_fill(&c,(struct desk_rect){1,1,1,1},0x123456);
    assert(full[4]==0x123456 && c.pixel_writes==1);
    /* A tiny independent raster oracle: physical bearings/alpha, logical pen. */
    const struct desk_glyph base_g[]={{0xfffd,0,1,1,0,-1,2}};
    const uint8_t base_a[]={255},dense_a[]={255,0,255,0,255,0};
    const struct desk_glyph dense_g[]={{0xfffd,0,3,2,0,-2,4}};
    const struct desk_font_raster raster={dense_g,dense_a};
    const struct desk_font custom={base_g,base_a,1,1,2,&raster};
    assert(desk_text_width(&custom,"??")==4);
    memset(full,0,sizeof(full));assert(!desk_canvas_init_scaled(&c,full,6,3,12,2));
    desk_text(&c,&custom,0,0,"??",0xffffff);
    assert(full[0]==0xffffff && full[1]==0 && full[2]==0xffffff && full[4]==0xffffff);
    assert(full[12]==0 && full[13]==0xffffff && full[14]==0 && c.pixel_writes==6);
    struct desk_font fallback=custom;fallback.density2=NULL;
    memset(full,0,sizeof(full));c.pixel_writes=0;desk_text(&c,&fallback,0,0,"?",0xffffff);
    assert(full[0]==0xffffff && full[1]==0xffffff && full[12]==0xffffff && full[13]==0xffffff && c.pixel_writes==4);
    /* A 4x2 image fitted into 2x1 logical pixels retains all samples at 2x. */
    const uint32_t image_pixels[]={0xff112233,0xff223344,0xff334455,0xff445566,
        0xff556677,0xff667788,0xff778899,0xff8899aa};
    struct desk_image im;desk_image_init(&im);
    assert(!desk_image_set(&im,image_pixels,4,2));
    desk_image_configure(&im,(struct desk_rect){0,0,4,3},1);
    assert(!desk_canvas_init_scaled(&c,full,4,3,8,2));
    desk_image_draw(&c,&im,&desk_theme_silver);
    for(unsigned y=0;y<2;y++)for(unsigned x=0;x<4;x++)
        assert(full[(y+2)*8+x+2]==(image_pixels[y*4+x]&0xffffff));
    /* Built-in high coverage contains real subpixel detail, not replicated 1x. */
    const struct desk_font *fonts[]={&desk_font_16,&desk_font_20,&desk_font_mono_16,&desk_font_digits_32};
    for(unsigned f=0;f<4;f++) {
        const struct desk_font *font=fonts[f];assert(font->density2);
        for(unsigned i=0;i<font->count;i++)assert(font->glyphs[i].codepoint==font->density2->glyphs[i].codepoint);
        memset(full,0,sizeof(full));assert(!desk_canvas_init_scaled(&c,full,W,H,MAX_STRIDE,2));
        desk_text(&c,font,2,2,f==3?"89":"Ag é",0xffffff);
        unsigned varied=0;
        for(int y=0;y<H*2;y+=2)for(int x=0;x<W*2;x+=2) {
            const uint32_t *p=full+y*MAX_STRIDE+x;
            varied+=p[0]!=p[1] || p[0]!=p[MAX_STRIDE] || p[0]!=p[MAX_STRIDE+1];
        }
        assert(varied>5);
    }
    puts("density: PASS clipping/stride/tiles, physical accounting, raster detail, logical metrics and fallback");
    return 0;
}
