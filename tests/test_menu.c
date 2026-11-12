/* Real LVGL + real font glyphs + real artwork; firmware boot actions are recorded. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lvgl.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include FT_OUTLINE_H
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#define HERCULES_HOST_TEST 1
#define HERCULES_EXTERNAL_UI_FONTS 1
static bool local_fonts_available=true;
static int diagnostic_timeout=0;
#define BOOT_EFI 9
#define BOOT_FOLDER 255
typedef enum {FT_FONT_STYLE_NORMAL=0,FT_FONT_STYLE_ITALIC=1,FT_FONT_STYLE_BOLD=2} lv_ft_style;
typedef struct {int mode;bool enabled,show;char ident[64],parent[64],desc[256];} boot_config;
struct bootmenu_item;
struct bootmenu {struct bootmenu_item *selected;char folder[64];};
struct bootmenu_item {struct bootmenu *bm;boot_config cfg;};
int gui_sw=1080,gui_sh=2248,gui_font_size=48;
lv_group_t *gui_grp;
static FT_Library ft;
static bool no_autoboot=false;
struct host_font {lv_font_t font;FT_Face face;bool bold;};
static bool glyph(const lv_font_t *font,lv_font_glyph_dsc_t *d,uint32_t ch,uint32_t next) {
 (void)next;struct host_font *f=(struct host_font *)font->dsc;memset(d,0,sizeof(*d));if(ch<32)return true;
 if(FT_Load_Char(f->face,ch,FT_LOAD_DEFAULT))return false;if(f->bold&&f->face->glyph->format==FT_GLYPH_FORMAT_OUTLINE)FT_Outline_Embolden(&f->face->glyph->outline,1<<6);if(FT_Render_Glyph(f->face->glyph,FT_RENDER_MODE_NORMAL))return false;
 FT_GlyphSlot s=f->face->glyph;d->adv_w=s->metrics.horiAdvance>>6;d->box_w=s->bitmap.width;d->box_h=s->bitmap.rows;d->ofs_x=s->bitmap_left;d->ofs_y=s->bitmap_top-s->bitmap.rows;d->bpp=8;d->is_placeholder=FT_Get_Char_Index(f->face,ch)==0;return true;
}
static const uint8_t *bitmap(const lv_font_t *font,uint32_t ch) {(void)ch;return ((struct host_font *)font->dsc)->face->glyph->bitmap.buffer;}
static bool empty_glyph(const lv_font_t *f,lv_font_glyph_dsc_t *d,uint32_t c,uint32_t n){(void)f;(void)c;(void)n;memset(d,0,sizeof(*d));d->adv_w=8;return true;}
const lv_font_t lv_font_montserrat_14={.get_glyph_dsc=empty_glyph,.line_height=16};
const lv_font_t *gui_font=&lv_font_montserrat_14;
static lv_font_t *lv_ft_init_rootfs(char *path,int size,lv_ft_style style) {
 char name[4096];if(strstr(path,"hercules-ui-")){if(!local_fonts_available)return NULL;snprintf(name,sizeof(name),"%s/%s",getenv("HERCULES_TEST_FONTS"),strrchr(path,'/')+1);}else snprintf(name,sizeof(name),"%s/root%s",getenv("HERCULES_SIMPLEINIT_SOURCE"),path);struct host_font *f=calloc(1,sizeof(*f));assert(f);assert(!FT_New_Face(ft,name,0,&f->face));assert(!FT_Set_Pixel_Sizes(f->face,0,size));f->bold=(style&FT_FONT_STYLE_BOLD)!=0;
 f->font.dsc=f;f->font.line_height=f->face->size->metrics.height>>6;f->font.base_line=-(f->face->size->metrics.descender>>6);f->font.get_glyph_dsc=glyph;f->font.get_glyph_bitmap=bitmap;return &f->font;
}
static void lv_ft_font_destroy(lv_font_t *font){struct host_font *f=(struct host_font *)font->dsc;FT_Done_Face(f->face);free(f);}
static const char *ids[]={"android","ios","shell","continue","simple-init","uefi-bootmenu","reboots","poweroff","reboot","recovery","bootloader","edl","option1","option2",NULL};
static boot_config *boot_get_config(char *id) {
 boot_config *c=calloc(1,sizeof(*c));assert(c);c->mode=BOOT_EFI;c->enabled=c->show=true;strcpy(c->ident,id);strcpy(c->desc,id);
 if(!strcmp(id,"android"))strcpy(c->desc,"Android (MIUI)");else if(!strcmp(id,"ios"))strcpy(c->desc,"iOS 16.5");else if(!strcmp(id,"shell"))strcpy(c->desc,"UEFI Shell on #93");
 else if(!strcmp(id,"reboots")){c->mode=BOOT_FOLDER;strcpy(c->desc,"Reboot Menu");}else if(!strcmp(id,"uefi-bootmenu")){c->mode=BOOT_FOLDER;strcpy(c->desc,"UEFI Boot Menu");}
 else if(!strcmp(id,"continue")){c->mode=10;strcpy(c->desc,"Continue Boot");}else if(!strcmp(id,"simple-init")){c->mode=11;strcpy(c->desc,"Enter Simple Init");}
 else if(!strncmp(id,"option",6)){strcpy(c->parent,"uefi-bootmenu");c->mode=12;strcpy(c->desc,"UEFI boot option");}
 else {strcpy(c->parent,"reboots");c->mode=4;}
 return c;
}
static char **confd_ls(const char *key) {
 (void)key;size_t n=0,bytes=0;while(ids[n])bytes+=strlen(ids[n++])+1;char **ret=calloc(n+1,sizeof(*ret));char *buf=malloc(bytes),*at=buf;assert(ret&&buf);
 for(size_t i=0;i<n;i++){ret[i]=at;strcpy(at,ids[i]);at+=strlen(at)+1;}return ret;
}
static bool confd_get_boolean(const char *key,bool fallback){return !strcmp(key,"boot.hercules_skin")?true:fallback;}
static char *confd_get_string(const char *key,const char *fallback){return strdup(!strcmp(key,"boot.default")?"android":fallback);}
static void lv_obj_set_hidden(lv_obj_t *o,bool hide){if(hide)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);}
static unsigned boots;static char last_boot[64];static unsigned char *framebuffer;static const char *asset_dir;static uint32_t input_key;static lv_indev_state_t input_state=LV_INDEV_STATE_REL;
#define tlog_notice(...) ((void)0)
static void confd_set_string(const char *key,const char *value){(void)key;(void)value;}
static void confd_set_integer(const char *key,long value){if(!strcmp(key,"boot.timeout"))diagnostic_timeout=value;}
static void bootmenu_boot(void *d){struct bootmenu *m=d;boots++;strcpy(last_boot,m->selected->cfg.ident);}
int bootmenu_draw(void);
#include "bootmenu_hercules.inc"
static int bootmenu_legacy_draw(void){abort();}
int bootmenu_draw(void){return bootmenu_hercules_draw();}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){for(int y=a->y1;y<=a->y2;y++)memcpy(framebuffer+((size_t)y*1080+a->x1)*4,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*4);lv_disp_flush_ready(d);}
static void input(lv_indev_drv_t *d,lv_indev_data_t *v){(void)d;v->key=input_key;v->state=input_state;}
static void step(void){for(int i=0;i<8;i++){lv_tick_inc(10);lv_timer_handler();}}
static void key(uint32_t k){input_key=k;input_state=LV_INDEV_STATE_PR;step();input_state=LV_INDEV_STATE_REL;step();}
static char *asset(const void *n){static char b[4096];snprintf(b,sizeof(b),"%s/%s",asset_dir,(const char *)n);return b;}
static bool svg_name(const void *s){return strstr((const char *)s,".svg")!=NULL;}
static lv_res_t info(lv_img_decoder_t *d,const void *s,lv_img_header_t *h){(void)d;if(lv_img_src_get_type(s)!=LV_IMG_SRC_FILE)return LV_RES_INV;int w,hh,n;if(svg_name(s)){NSVGimage *im=nsvgParseFromFile(asset(s),"px",192);if(!im)return LV_RES_INV;w=im->width*2;hh=im->height*2;nsvgDelete(im);}else if(!stbi_info(asset(s),&w,&hh,&n))return LV_RES_INV;h->always_zero=0;h->w=w;h->h=hh;h->cf=svg_name(s)?LV_IMG_CF_RAW_ALPHA:LV_IMG_CF_TRUE_COLOR;return LV_RES_OK;}
static struct {char name[128];unsigned char *pixels;} decoded[8];static unsigned decoded_count;
static lv_res_t open_image(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s){(void)d;for(unsigned i=0;i<decoded_count;i++)if(!strcmp(decoded[i].name,s->src)){s->img_data=decoded[i].pixels;return LV_RES_OK;}int w,h,n;unsigned char *b;if(svg_name(s->src)){NSVGimage *im=nsvgParseFromFile(asset(s->src),"px",192);if(!im)return LV_RES_INV;w=im->width*2;h=im->height*2;b=calloc((size_t)w*h,4);NSVGrasterizer *rast=nsvgCreateRasterizer();nsvgRasterize(rast,im,0,0,2,b,w,h,w*4);nsvgDeleteRasterizer(rast);nsvgDelete(im);}else b=stbi_load(asset(s->src),&w,&h,&n,4);if(!b)return LV_RES_INV;for(size_t i=0;i<(size_t)w*h;i++){unsigned char t=b[i*4];b[i*4]=b[i*4+2];b[i*4+2]=t;}assert(decoded_count<8);strcpy(decoded[decoded_count].name,s->src);decoded[decoded_count++].pixels=b;s->img_data=b;return LV_RES_OK;}
static void close_image(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s){(void)d;s->img_data=NULL;}
static struct hercules_menu *menu(void){return lv_obj_get_event_user_data(lv_obj_get_child(lv_scr_act(),0),hercules_release);}
static void reset(void){lv_group_remove_all_objs(gui_grp);lv_obj_clean(lv_scr_act());boots=0;last_boot[0]=0;assert(!bootmenu_draw());step();}
static void click(lv_obj_t *obj){assert(!lv_obj_has_state(obj,LV_STATE_DISABLED));lv_event_send(obj,LV_EVENT_CLICKED,NULL);step();}
static unsigned find(struct hercules_advanced *p,const char *id){for(unsigned i=0;i<p->count;i++)if(!strcmp(p->items[i].cfg.ident,id))return i;abort();}
static bool has_text(lv_obj_t *root,const char *text){if(lv_obj_check_type(root,&lv_label_class)&&!strcmp(lv_label_get_text(root),text))return true;for(unsigned i=0;i<lv_obj_get_child_cnt(root);i++)if(has_text(lv_obj_get_child(root,i),text))return true;return false;}
static lv_obj_t *find_text(lv_obj_t *root,const char *text){if(lv_obj_check_type(root,&lv_label_class)&&!strcmp(lv_label_get_text(root),text))return root;for(unsigned i=0;i<lv_obj_get_child_cnt(root);i++){lv_obj_t *r=find_text(lv_obj_get_child(root,i),text);if(r)return r;}return NULL;}
static lv_obj_t *find_image(lv_obj_t *root,const char *name){if(lv_obj_check_type(root,&lv_img_class)&&lv_img_src_get_type(lv_img_get_src(root))==LV_IMG_SRC_FILE&&!strcmp(lv_img_get_src(root),name))return root;for(unsigned i=0;i<lv_obj_get_child_cnt(root);i++){lv_obj_t *r=find_image(lv_obj_get_child(root,i),name);if(r)return r;}return NULL;}
static void assert_logo(lv_obj_t *box,int w,int h){assert(lv_obj_get_width(box)==w&&lv_obj_get_height(box)==h);lv_obj_t *image=lv_obj_get_child(box,0);assert(((lv_img_t *)image)->w==w*2&&((lv_img_t *)image)->h==h*2);assert(lv_img_get_zoom(image)==128);}
static void advance(unsigned ms){for(unsigned i=0;i<ms/10;i++){lv_tick_inc(10);lv_timer_handler();}}
static void fresh(void){hercules_user_interacted=false;no_autoboot=false;reset();assert(menu()->countdown&&menu()->selected==0);}
static void snapshot(const char *dir,const char *name){char path[4096];snprintf(path,sizeof(path),"%s/%s",dir,name);unsigned char *b=malloc(1080*2248*4);for(size_t i=0;i<1080u*2248u;i++){b[i*4]=framebuffer[i*4+2];b[i*4+1]=framebuffer[i*4+1];b[i*4+2]=framebuffer[i*4];b[i*4+3]=255;}assert(stbi_write_png(path,1080,2248,4,b,1080*4));free(b);}
int main(int argc,char **argv){
 assert(argc==3);asset_dir=argv[1];assert(!FT_Init_FreeType(&ft));lv_init();lv_font_t *default_font=lv_ft_init_rootfs("/etc/default.ttf",48,FT_FONT_STYLE_NORMAL),*symbols=lv_ft_init_rootfs("/etc/symbols.ttf",48,FT_FONT_STYLE_NORMAL);default_font->fallback=symbols;gui_font=default_font;framebuffer=calloc(1080*2248,4);lv_disp_draw_buf_t draw;lv_color_t *pixels=malloc(1080*32*sizeof(*pixels));lv_disp_draw_buf_init(&draw,pixels,NULL,1080*32);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=1080;d.ver_res=2248;d.draw_buf=&draw;d.flush_cb=flush;lv_disp_drv_register(&d);
 gui_grp=lv_group_create();lv_indev_drv_t kd;lv_indev_drv_init(&kd);kd.type=LV_INDEV_TYPE_KEYPAD;kd.read_cb=input;lv_indev_t *keyboard=lv_indev_drv_register(&kd);lv_indev_set_group(keyboard,gui_grp);
 lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,info);lv_img_decoder_set_open_cb(decoder,open_image);lv_img_decoder_set_close_cb(decoder,close_image);
 fresh();assert(has_text(menu()->root,"选择启动系统")&&has_text(menu()->root,"Choose an operating system")&&has_text(menu()->root,"十七号旅客")&&has_text(menu()->root,"酷安 @BH8GQV"));assert_logo(menu()->logo[0],189,166);assert_logo(menu()->logo[1],132,154);assert_logo(lv_obj_get_parent(find_image(menu()->root,"hercules-xiaohongshu.svg")),89,40);lv_area_t box,title;lv_obj_get_coords(menu()->logo[0],&box);lv_obj_get_coords(find_text(menu()->control[0],"Android"),&title);assert(box.x2<title.x1);snapshot(argv[2],"headless-android.png");key(LV_KEY_NEXT);assert(menu()->selected==1&&!menu()->countdown&&!strcmp(lv_label_get_text(menu()->launch_text),"启动 iOS"));snapshot(argv[2],"headless-ios.png");key(LV_KEY_ENTER);assert(boots==1&&!strcmp(last_boot,"ios"));
 reset();key(LV_KEY_ENTER);assert(boots==1&&!strcmp(last_boot,"android"));
 reset();click(menu()->control[0]);assert(boots==0&&menu()->selected==0);click(menu()->control[3]);assert(boots==1&&!strcmp(last_boot,"android"));
 for(unsigned i=0;i<12;i++){
  reset();key(LV_KEY_NEXT);key(LV_KEY_NEXT);assert(menu()->active_choice==2&&lv_obj_has_flag(menu()->check[0],LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(menu()->check[1],LV_OBJ_FLAG_HIDDEN));assert(!strcmp(lv_label_get_text(menu()->launch_text),"高级选项"));assert(lv_color_to32(lv_obj_get_style_border_color(menu()->control[1],0))==lv_color_to32(lv_color_hex(0xffffff)));if(!i)snapshot(argv[2],"headless-advanced-focus.png");key(LV_KEY_ENTER);struct hercules_advanced *p=menu()->advanced;assert(p&&p->count==5&&!lv_obj_has_state(p->back,LV_STATE_DISABLED));assert(lv_group_get_focused(gui_grp)==p->back);assert(has_text(p->root,"十七号旅客")&&has_text(p->root,"酷安 @BH8GQV"));assert_logo(lv_obj_get_parent(find_image(p->root,"hercules-xiaohongshu.svg")),89,40);
  if(!i)snapshot(argv[2],"headless-advanced.png");key(LV_KEY_ENTER);assert(!menu()->advanced&&boots==0&&menu()->selected==1);
 }
 reset();click(menu()->control[2]);struct hercules_advanced *p=menu()->advanced;click(p->row[find(p,"reboots")]);assert(!strcmp(menu()->advanced->folder,"reboots"));snapshot(argv[2],"headless-reboots.png");key(LV_KEY_ENTER);assert(menu()->advanced&&!menu()->advanced->folder[0]);key(LV_KEY_ENTER);assert(!menu()->advanced&&boots==0);
 click(menu()->control[2]);p=menu()->advanced;click(p->row[find(p,"uefi-bootmenu")]);assert(menu()->advanced->count==2);key(LV_KEY_ENTER);assert(menu()->advanced&&!menu()->advanced->folder[0]);p=menu()->advanced;click(p->row[find(p,"shell")]);assert(boots==1&&!strcmp(last_boot,"shell"));
 reset();click(menu()->control[2]);p=menu()->advanced;click(p->row[find(p,"reboots")]);p=menu()->advanced;click(p->row[find(p,"bootloader")]);assert(boots==1&&!strcmp(last_boot,"bootloader"));
 reset();lv_event_send(menu()->control[2],LV_EVENT_CLICKED,NULL);step();p=menu()->advanced;lv_event_send(p->back,LV_EVENT_CLICKED,NULL);lv_event_send(p->back,LV_EVENT_CLICKED,NULL);step();assert(!menu()->advanced&&boots==0);
 reset();key(LV_KEY_NEXT);key(LV_KEY_NEXT);key(LV_KEY_NEXT);assert(menu()->active_choice==2);key(LV_KEY_ENTER);assert(menu()->advanced&&boots==0);key(LV_KEY_ENTER);assert(!menu()->advanced&&menu()->selected==1&&menu()->active_choice==1&&!lv_obj_has_flag(menu()->check[1],LV_OBJ_FLAG_HIDDEN));
 fresh();advance(9000);assert(boots==0);advance(1000);assert(boots==1&&!strcmp(last_boot,"android"));advance(15000);assert(boots==1);
 fresh();advance(5000);key(LV_KEY_NEXT);assert(!menu()->countdown&&menu()->selected==1);advance(20000);assert(boots==0);key(LV_KEY_ENTER);assert(boots==1&&!strcmp(last_boot,"ios"));
 fresh();key('x');assert(!menu()->countdown);advance(20000);assert(boots==0);
 fresh();click(menu()->control[1]);assert(!menu()->countdown&&boots==0);click(menu()->control[2]);key(LV_KEY_ENTER);assert(!menu()->advanced&&!menu()->countdown&&menu()->selected==1);advance(20000);assert(boots==0);
 local_fonts_available=false;lv_group_remove_all_objs(gui_grp);lv_obj_clean(lv_scr_act());boots=0;assert(bootmenu_hercules_draw()==-1);step();assert(diagnostic_timeout==-1);advance(15000);assert(boots==0);local_fonts_available=true;
 puts("PASS: public external-font mode preserves the approved UI with locally supplied original font data; all UI/counter/back/SVG regressions pass; missing font resources fail to manual setup without substituted typography or autoboot.");
 lv_indev_delete(keyboard);lv_disp_remove(lv_disp_get_default());step();lv_group_del(gui_grp);lv_img_decoder_delete(decoder);d.draw_ctx_deinit(&d,d.draw_ctx);lv_mem_free(d.draw_ctx);free(pixels);free(framebuffer);for(unsigned i=0;i<decoded_count;i++)stbi_image_free(decoded[i].pixels);lv_ft_font_destroy(default_font);lv_ft_font_destroy(symbols);FT_Done_FreeType(ft);return 0;
}
