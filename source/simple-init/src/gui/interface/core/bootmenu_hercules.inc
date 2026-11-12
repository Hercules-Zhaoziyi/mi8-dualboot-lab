/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Approved static artwork with live LVGL hit targets and original boot logic.
 */
#ifndef HERCULES_HOST_TEST
#include "gui/font.h"
#endif
#include "src/misc/lv_txt.h"
#include "hercules_target.h"
static int bootmenu_legacy_draw(void);
struct hercules_advanced;
static void hercules_show_advanced(void *data);
static lv_obj_t *hercules_logo(lv_obj_t *parent,const char *name,int x,int y,int w,int h);
static bool hercules_user_interacted=false;
enum hercules_font_id { H_HEADING,H_KICKER,H_SUBTITLE,H_CHOICE,H_VERSION,H_ADVANCED,H_ACTION,H_HINT,H_FOOT,H_SMALL,H_FONT_COUNT };

struct hercules_menu {
    struct bootmenu bm;
    struct bootmenu_item item[2];
    lv_obj_t *root, *background, *launch_text, *check[2];
    lv_obj_t *control[4];
    lv_obj_t *logo[2];
    lv_font_t *font[H_FONT_COUNT];
    lv_obj_t *countdown_text;
    lv_timer_t *countdown;
    uint32_t countdown_started;
    unsigned countdown_seconds;
    unsigned selected;
    unsigned active_choice;
    bool leaving;
    struct hercules_advanced *advanced;
};

static lv_coord_t hercules_x(int value) { return (lv_coord_t)((int64_t)value * gui_sw / 846); }
static lv_coord_t hercules_y(int value) { return (lv_coord_t)((int64_t)value * gui_sh / 1858); }

static void hercules_cancel_countdown(struct hercules_menu *menu,bool interacted) {
    if(interacted)hercules_user_interacted=true;
    if(menu->countdown){lv_timer_del(menu->countdown);menu->countdown=NULL;}
    if(menu->countdown_text)lv_obj_set_hidden(menu->countdown_text,true);
}

static void hercules_root_pressed(lv_event_t *event) {
    struct hercules_menu *menu=event->user_data;
    if(menu&&!menu->leaving)hercules_cancel_countdown(menu,true);
}

static void hercules_free_menu(void *data) {
    struct hercules_menu *menu=data;
    for(unsigned i=0;i<H_FONT_COUNT;i++)if(menu->font[i])lv_ft_font_destroy(menu->font[i]);
    free(menu);
}
static void hercules_release(lv_event_t *event) {
    struct hercules_menu *menu=event->user_data;
    menu->leaving=true;
    hercules_cancel_countdown(menu,false);
    /* LVGL deletes children after the parent's DELETE event. Remove focus first,
     * so child destruction cannot focus another control using the freed context. */
    lv_group_remove_all_objs(gui_grp);
    lv_async_call(hercules_free_menu,menu);
}

static void hercules_select(struct hercules_menu *menu, unsigned index) {
    if (index > 2 || menu->leaving) return;
    menu->active_choice=index;
    if(index<2){menu->selected=index;menu->bm.selected=&menu->item[index];}
    for(unsigned i=0;i<2;i++){
        lv_obj_set_hidden(menu->check[i],i!=index);
    }
    for(unsigned i=0;i<3;i++){
        lv_color_t color=lv_color_hex(i==index?0x007bff:0xffffff);
        lv_obj_set_style_border_color(menu->control[i],color,0);
        lv_obj_set_style_border_color(menu->control[i],color,LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(menu->control[i],i==index?3:2,0);
        lv_obj_set_style_border_width(menu->control[i],i==index?3:2,LV_STATE_FOCUSED);
    }
    lv_label_set_text(menu->launch_text,index==0?"启动 " HERCULES_TARGET_0_LABEL:index==1?"启动 " HERCULES_TARGET_1_LABEL:"高级选项");
}

static void hercules_countdown_tick(lv_timer_t *timer) {
    struct hercules_menu *menu=timer->user_data;
    if(menu->leaving){hercules_cancel_countdown(menu,false);return;}
    uint32_t elapsed=lv_tick_elaps(menu->countdown_started);
    if(elapsed>=10000){
        hercules_cancel_countdown(menu,false);
        hercules_select(menu,0);
        menu->leaving=true;
        lv_async_call(bootmenu_boot,&menu->bm);
        return;
    }
    unsigned seconds=(10000-elapsed+999)/1000;
    if(seconds!=menu->countdown_seconds){
        menu->countdown_seconds=seconds;
        lv_label_set_text_fmt(menu->countdown_text,"%u 秒后自动启动 " HERCULES_TARGET_0_LABEL,seconds);
    }
}

struct hercules_advanced {
    struct hercules_menu *main;
    struct bootmenu bm;
    struct bootmenu_item items[32];
    lv_obj_t *root, *row[32], *back;
    lv_font_t *heading, *normal, *small, *button;
    unsigned count;
    bool leaving;
    char folder[64];
};

static void hercules_advanced_free(lv_event_t *event) {
    struct hercules_advanced *page = event->user_data;
    lv_group_remove_all_objs(gui_grp);
    /* Font ownership belongs to the main page and is shared across both pages. */
    free(page);
}

static void hercules_main_focus(struct hercules_menu *menu) {
    unsigned selected=menu->selected;
    /* Adding the first object auto-focuses it; do not change the user's OS choice. */
    menu->leaving = true;
    for (unsigned i = 0; i < 4; i++) lv_group_add_obj(gui_grp, menu->control[i]);
    menu->leaving = false;
    hercules_select(menu,selected);
    lv_group_set_editing(gui_grp, false);
    lv_group_focus_obj(menu->control[selected]);
}

static lv_obj_t *hercules_caption(lv_obj_t *parent, const char *text, const lv_font_t *font, int x, int y, int w, uint32_t color, bool centered) {
    lv_obj_t *label = lv_label_create(parent);
    const lv_font_t *used=font?font:gui_font;
    int ink_top=0;
    for(uint32_t at=0;text[at];){
        uint32_t letter=_lv_txt_encoded_next(text,&at);lv_font_glyph_dsc_t glyph;
        if(lv_font_get_glyph_dsc(used,&glyph,letter,0)&&glyph.box_h){int top=(int)glyph.box_h+glyph.ofs_y;if(top>ink_top)ink_top=top;}
    }
    int inset=(int)used->line_height-used->base_line-ink_top;
    lv_obj_set_pos(label, hercules_x(x), hercules_y(y)-inset);
    lv_obj_set_width(label, hercules_x(w));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font ? font : gui_font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, centered ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static void hercules_glass(lv_obj_t *obj, bool primary) {
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, hercules_x(primary?39:32), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? 0x369fff : 0xffffff), 0);
    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(primary ? 0x0079ff : 0xf3f5ff), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(obj, primary ? LV_OPA_COVER : LV_OPA_30, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_opa(obj, primary ? LV_OPA_50 : LV_OPA_90, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x7890bc), 0);
    lv_obj_set_style_shadow_width(obj, hercules_x(24), 0);
    lv_obj_set_style_shadow_ofs_y(obj, hercules_y(9), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(primary ? 0xffffff : 0x007bff), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(obj, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(obj, primary ? LV_OPA_80 : LV_OPA_50, LV_STATE_PRESSED);
}

static const lv_font_t *hercules_font(struct hercules_menu *main,unsigned id) {
    return main->font[id]?main->font[id]:gui_font;
}

static bool hercules_load_fonts(struct hercules_menu *main) {
    const int size[H_FONT_COUNT]={hercules_x(80),hercules_y(29),hercules_y(35),hercules_y(56),hercules_y(37),hercules_y(37),hercules_y(49),hercules_y(33),hercules_y(20),hercules_y(23)};
    for(unsigned i=0;i<H_FONT_COUNT;i++){
        bool bold=i==H_HEADING||i==H_CHOICE||i==H_ADVANCED||i==H_ACTION;
        main->font[i]=lv_ft_init_rootfs(bold?"/usr/share/fonts/hercules-ui-bold.ttf":"/usr/share/fonts/hercules-ui-regular.ttf",size[i],FT_FONT_STYLE_NORMAL);
#ifdef HERCULES_EXTERNAL_UI_FONTS
        if(!main->font[i])return false;
#else
        if(!main->font[i])main->font[i]=lv_ft_init_rootfs("/etc/default.ttf",size[i],bold?FT_FONT_STYLE_BOLD:FT_FONT_STYLE_NORMAL);
#endif
        if(main->font[i])main->font[i]->fallback=gui_font;
    }
    return true;
}

static void hercules_header(lv_obj_t *root,struct hercules_menu *main,const char *title,const char *english) {
    lv_obj_t *kicker=hercules_caption(root,"DUAL BOOT",hercules_font(main,H_KICKER),42,254,762,0x35414c,true);
    lv_obj_set_style_text_letter_space(kicker,hercules_x(20),0);
    lv_obj_t *heading=hercules_caption(root,title,hercules_font(main,H_HEADING),42,341,762,0x252a32,true);
    lv_obj_set_style_text_letter_space(heading,hercules_x(2),0);
    hercules_caption(root,english,hercules_font(main,H_SUBTITLE),42,453,762,0x828996,true);
}

static lv_obj_t *hercules_flex_group(lv_obj_t *parent,int gap) {
    lv_obj_t *obj=lv_obj_create(parent);lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj,LV_SIZE_CONTENT,LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(obj,hercules_x(gap),0);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void hercules_footer(lv_obj_t *root,struct hercules_menu *main) {
    lv_obj_t *row=hercules_flex_group(root,32);
    lv_obj_align(row,LV_ALIGN_TOP_MID,0,hercules_y(1758));
    lv_obj_t *left=hercules_flex_group(row,10),*right=hercules_flex_group(row,7);
    hercules_logo(left,"hercules-xiaohongshu.svg",0,0,89,40);
    lv_obj_t *name=lv_label_create(left);lv_label_set_text(name,"十七号旅客");lv_obj_set_style_text_font(name,hercules_font(main,H_FOOT),0);lv_obj_set_style_text_color(name,lv_color_hex(0x747b89),0);lv_obj_clear_flag(name,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *dot=lv_obj_create(right);lv_obj_remove_style_all(dot);lv_obj_set_size(dot,hercules_x(10),hercules_x(10));lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_color(dot,lv_color_hex(0x2dba57),0);lv_obj_set_style_bg_opa(dot,LV_OPA_COVER,0);lv_obj_clear_flag(dot,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *coolapk=lv_label_create(right);lv_label_set_text(coolapk,"酷安 @BH8GQV");lv_obj_set_style_text_font(coolapk,hercules_font(main,H_FOOT),0);lv_obj_set_style_text_color(coolapk,lv_color_hex(0x747b89),0);lv_obj_clear_flag(coolapk,LV_OBJ_FLAG_CLICKABLE);
}

static void hercules_advanced_back(void *data) {
    struct hercules_advanced *page = data;
    struct hercules_menu *main = page->main;
    char parent[64] = {0};
    if (page->folder[0]) {
        boot_config *config = boot_get_config(page->folder);
        if (config) { strncpy(parent, config->parent, sizeof(parent)-1); free(config); }
    }
    bool nested = page->folder[0] != 0;
    lv_group_remove_all_objs(gui_grp);
    main->advanced = NULL;
    lv_obj_del(page->root);
    if (nested) {
        /* Keep the parent location on the main context until the async draw. */
        strncpy(main->bm.folder, parent, sizeof(main->bm.folder)-1);
        hercules_show_advanced(main);
    } else hercules_main_focus(main);
}

static void hercules_advanced_enter_folder(void *data) {
    struct hercules_advanced *page = data;
    struct hercules_menu *main = page->main;
    char folder[64]; strncpy(folder, page->bm.selected->cfg.ident, sizeof(folder)); folder[63]=0;
    lv_group_remove_all_objs(gui_grp);
    main->advanced = NULL;
    lv_obj_del(page->root);
    strncpy(main->bm.folder, folder, sizeof(main->bm.folder)-1);
    hercules_show_advanced(main);
}

static void hercules_advanced_event(lv_event_t *event) {
    struct hercules_advanced *page = event->user_data;
    if (!page || page->leaving || event->code != LV_EVENT_CLICKED) return;
    page->leaving = true;
    if (event->target == page->back) { lv_async_call(hercules_advanced_back, page); return; }
    for (unsigned i=0; i<page->count; i++) if (event->target == page->row[i]) {
        page->bm.selected = &page->items[i];
        if (page->items[i].cfg.mode == BOOT_FOLDER) lv_async_call(hercules_advanced_enter_folder, page);
        else lv_async_call(bootmenu_boot, &page->bm);
        return;
    }
    page->leaving = false;
}

static const char *hercules_advanced_name(const boot_config *config) {
    const char *id=config->ident;
    if (!strcmp(id,"continue")) return "继续固件引导";
    if (!strcmp(id,"simple-init")) return "系统维护工具";
    if (!strcmp(id,"uefi-bootmenu")) return "UEFI 启动管理";
    if (!strcmp(id,"reboots")) return "重启与关机";
    if (!strcmp(id,"poweroff")) return "关机";
    if (!strcmp(id,"reboot")) return "重新启动";
    if (!strcmp(id,"recovery")) return "进入 Recovery";
    if (!strcmp(id,"bootloader")) return "进入 FASTBOOT";
    if (!strcmp(id,"edl")) return "进入 EDL";
    if (strstr(config->desc,"Shell")) return "UEFI Shell";
    return config->desc;
}

static void hercules_show_advanced(void *data) {
    struct hercules_menu *main = data;
    struct hercules_advanced *page=calloc(1,sizeof(*page));
    if(!page){hercules_main_focus(main);return;}
    page->main=main; main->advanced=page;
    strncpy(page->folder,main->bm.folder,sizeof(page->folder)-1);
    lv_group_remove_all_objs(gui_grp);
    page->root=lv_obj_create(main->root);
    lv_obj_remove_style_all(page->root);
    lv_obj_set_size(page->root,gui_sw,gui_sh);
    lv_obj_set_pos(page->root,0,0);
    lv_obj_clear_flag(page->root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page->root,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(page->root,hercules_advanced_free,LV_EVENT_DELETE,page);
    lv_obj_t *wall=lv_img_create(page->root);lv_img_set_src(wall,"bg.jpg");lv_obj_clear_flag(wall,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_pos(wall,0,0);
    page->heading=main->font[H_HEADING];page->normal=main->font[H_SUBTITLE];page->small=main->font[H_SMALL];page->button=main->font[H_ACTION];
    const char *heading="高级选项",*english="Advanced options";
    if(!strcmp(page->folder,"reboots")){heading="重启与关机";english="Reboot & power";}
    else if(!strcmp(page->folder,"uefi-bootmenu")){heading="UEFI 启动管理";english="UEFI Boot Menu";}
    hercules_header(page->root,main,heading,english);
    char **names=confd_ls("boot.configs");
    if(names){
        for(size_t i=0;names[i] && page->count<32;i++){
            boot_config *cfg=boot_get_config(names[i]);if(!cfg)continue;
            if(!cfg->enabled||!cfg->show||strcmp(cfg->parent,page->folder)||!strcmp(cfg->ident,"android")||!strcmp(cfg->ident,"ios")){free(cfg);continue;}
            unsigned at=page->count++;page->items[at].bm=&page->bm;memcpy(&page->items[at].cfg,cfg,sizeof(*cfg));free(cfg);
        }
        if(names[0])free(names[0]);free(names);
    }
    lv_obj_t *listbox=lv_obj_create(page->root);lv_obj_remove_style_all(listbox);lv_obj_set_pos(listbox,0,hercules_y(530));lv_obj_set_size(listbox,gui_sw,hercules_y(850));lv_obj_set_scroll_dir(listbox,LV_DIR_VER);lv_obj_set_scrollbar_mode(listbox,LV_SCROLLBAR_MODE_OFF);
    for(unsigned i=0;i<page->count;i++){
        boot_config *cfg=&page->items[i].cfg;
        lv_obj_t *row=page->row[i]=lv_btn_create(listbox);hercules_glass(row,false);lv_obj_set_pos(row,hercules_x(42),hercules_y(i*166+8));lv_obj_set_size(row,hercules_x(762),hercules_y(138));
        char number[16];snprintf(number,sizeof(number),"%02u",i+1);
        hercules_caption(row,number,page->small,30,42,64,0x6e91bc,true);
        hercules_caption(row,hercules_advanced_name(cfg),page->normal,120,25,575,0x283340,false);
        hercules_caption(row,cfg->desc,page->small,120,83,575,0x7c8798,false);
        lv_obj_add_event_cb(row,hercules_advanced_event,LV_EVENT_CLICKED,page);lv_group_add_obj(gui_grp,row);
    }
    page->back=lv_btn_create(page->root);hercules_glass(page->back,true);lv_obj_set_pos(page->back,hercules_x(44),hercules_y(1450));lv_obj_set_size(page->back,hercules_x(760),hercules_y(113));
    hercules_caption(page->back,page->folder[0]?"返回上一页":"返回首页",page->button,0,35,760,0xffffff,true);
    lv_obj_add_event_cb(page->back,hercules_advanced_event,LV_EVENT_CLICKED,page);lv_group_add_obj(gui_grp,page->back);lv_group_set_editing(gui_grp,false);lv_group_focus_obj(page->back);
    hercules_caption(page->root,"音量键选择 · 电源键确认",page->normal,42,1597,762,0x626c7b,true);
    hercules_footer(page->root,main);
}

static void hercules_control_event(lv_event_t *event) {
    struct hercules_menu *menu = event->user_data;
    if (!menu || menu->leaving) return;
    unsigned index;
    for (index = 0; index < 4; index++) if (event->target == menu->control[index]) break;
    if (index == 4) return;
    if(event->code==LV_EVENT_PRESSED||event->code==LV_EVENT_KEY){hercules_cancel_countdown(menu,true);return;}
    if (event->code == LV_EVENT_FOCUSED) {
        if(lv_indev_get_act())hercules_cancel_countdown(menu,true);
        if (index < 3) hercules_select(menu, index);
        return;
    }
    if (event->code != LV_EVENT_CLICKED) return;
    hercules_cancel_countdown(menu,true);
    if (index == 2 || (index==3 && menu->active_choice==2)) {
        hercules_select(menu,2);
        menu->leaving = true;
        menu->bm.folder[0]=0;
        lv_async_call(hercules_show_advanced, menu);
        return;
    }
    if (index < 2) {
        hercules_select(menu, index);
        /* Touch chooses a card; hardware Enter starts it in one press. */
        lv_indev_t *input = lv_indev_get_act();
        if (!input || lv_indev_get_type(input) == LV_INDEV_TYPE_POINTER) return;
    }
    menu->leaving = true;
    lv_async_call(bootmenu_boot, &menu->bm);
}

static lv_obj_t *hercules_image(lv_obj_t *parent, const char *name, int x, int y) {
    lv_obj_t *image = lv_img_create(parent);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(image, x, y);
    lv_img_set_src(image, name);
    return image;
}

static lv_obj_t *hercules_logo(lv_obj_t *parent,const char *name,int x,int y,int w,int h) {
    /* SVG decoding intentionally supersamples at 2x. Keep layout and paint bounds
     * explicit instead of treating the decoded raster dimensions as UI dimensions. */
    lv_obj_t *box=lv_obj_create(parent);lv_obj_remove_style_all(box);
    lv_obj_set_pos(box,x,y);lv_obj_set_size(box,w,h);
    lv_obj_clear_flag(box,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_t *image=hercules_image(box,name,0,0);lv_img_t *data=(lv_img_t *)image;
    if(data->w>0&&data->h>0){
        int zoom_x=w*256/data->w,zoom_y=h*256/data->h;
        int zoom=zoom_x<zoom_y?zoom_x:zoom_y;if(zoom<1)zoom=1;
        lv_img_set_size_mode(image,LV_IMG_SIZE_MODE_VIRTUAL);
        lv_img_set_pivot(image,0,0);lv_img_set_zoom(image,zoom);lv_img_set_antialias(image,true);
        lv_obj_set_pos(image,(w-data->w*zoom/256)/2,(h-data->h*zoom/256)/2);
    }
    return box;
}

static int bootmenu_hercules_draw(void) {
    if (!confd_get_boolean("boot.hercules_skin", false)) return -1;
    /* Assets are calibrated to the dipper framebuffer; other modes keep the classic UI. */
    if (gui_sw < 320 || gui_sh < 480) return -1;
    struct hercules_menu *menu = calloc(1, sizeof(*menu));
    if (!menu) return -1;
    const char *names[] = {HERCULES_TARGET_0_ID, HERCULES_TARGET_1_ID};
    for (unsigned i = 0; i < 2; i++) {
        boot_config *config = boot_get_config((char *)names[i]);
        if (!config) { free(menu); return -1; }
        memcpy(&menu->item[i].cfg, config, sizeof(*config));
        free(config);
        if (!menu->item[i].cfg.enabled || !menu->item[i].cfg.show || menu->item[i].cfg.mode != BOOT_EFI) {
            free(menu); return -1;
        }
        menu->item[i].bm = &menu->bm;
    }
    menu->root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(menu->root);
    lv_obj_set_pos(menu->root, 0, 0);
    lv_obj_set_size(menu->root, gui_sw, gui_sh);
    lv_obj_clear_flag(menu->root, LV_OBJ_FLAG_SCROLLABLE);
    menu->background = hercules_image(menu->root, "bg.jpg", 0, 0);
    if (((lv_img_t *)menu->background)->w != 1080 || ((lv_img_t *)menu->background)->h != 2248) {
        lv_obj_del(menu->root); free(menu); return -1;
    }
    if(gui_sw!=1080 || gui_sh!=2248){
        unsigned zx=((unsigned)gui_sw*256+1079)/1080,zy=((unsigned)gui_sh*256+2247)/2248;
        unsigned zoom=zx>zy?zx:zy;
        lv_img_set_pivot(menu->background,0,0);
        lv_img_set_zoom(menu->background,zoom);
        lv_obj_set_pos(menu->background,(gui_sw-(int)(1080*zoom/256))/2,(gui_sh-(int)(2248*zoom/256))/2);
    }
    lv_obj_add_event_cb(menu->root, hercules_release, LV_EVENT_DELETE, menu);
    lv_obj_add_event_cb(menu->root, hercules_root_pressed, LV_EVENT_PRESSED, menu);
    if(!hercules_load_fonts(menu)){
        confd_set_string("boot.title","Local UI font pack required");
        confd_set_string("boot.subtitle","See docs/INSTALL.md: generate local font FFS resources");
        confd_set_integer("boot.timeout",-1);
        lv_obj_del(menu->root);return -1;
    }
    hercules_header(menu->root,menu,"选择启动系统","Choose an operating system");
    menu->leaving=true;
    lv_group_remove_all_objs(gui_grp);
    const int rect[4][4] = {{42,560,762,210},{42,804,762,210},{42,1067,762,102},{44,1450,760,113}};
    for (unsigned i = 0; i < 4; i++) {
        lv_obj_t *button = menu->control[i] = lv_btn_create(menu->root);
        hercules_glass(button,i==3);
        lv_obj_set_pos(button, hercules_x(rect[i][0]), hercules_y(rect[i][1]));
        lv_obj_set_size(button, hercules_x(rect[i][2]), hercules_y(rect[i][3]));
        lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(button, hercules_x(i == 2 ? 32 : 39), 0);
        lv_obj_set_style_outline_width(button, 0, LV_STATE_DEFAULT);
        if (i >= 2) {
            lv_obj_set_style_border_width(button, 3, LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(button, lv_color_hex(i == 2 ? 0x007bff : 0xffffff), LV_STATE_FOCUSED);
        }
        lv_obj_add_event_cb(button, hercules_control_event, LV_EVENT_FOCUSED, menu);
        lv_obj_add_event_cb(button, hercules_control_event, LV_EVENT_CLICKED, menu);
        lv_obj_add_event_cb(button, hercules_control_event, LV_EVENT_PRESSED, menu);
        lv_obj_add_event_cb(button, hercules_control_event, LV_EVENT_KEY, menu);
        lv_group_add_obj(gui_grp, button);
    }
    menu->logo[0]=hercules_logo(menu->control[0],"hercules-android.svg",hercules_x(47),hercules_y(37),189,166);
    menu->logo[1]=hercules_logo(menu->control[1],"hercules-apple.svg",hercules_x(66),hercules_y(43),132,154);
    for(unsigned i=0;i<2;i++){
        hercules_caption(menu->control[i],i==0?HERCULES_TARGET_0_LABEL:HERCULES_TARGET_1_LABEL,hercules_font(menu,H_CHOICE),238,54,400,0x20242c,false);
        hercules_caption(menu->control[i],i==0?HERCULES_TARGET_0_VERSION:HERCULES_TARGET_1_VERSION,hercules_font(menu,H_VERSION),238,126,400,0x758092,false);
        static const lv_point_t points[]={{0,17},{17,33},{50,0}};
        menu->check[i]=lv_line_create(menu->control[i]);lv_line_set_points(menu->check[i],points,3);lv_obj_set_pos(menu->check[i],hercules_x(678),hercules_y(88));lv_obj_set_style_line_color(menu->check[i],lv_color_hex(0x007bff),0);lv_obj_set_style_line_width(menu->check[i],hercules_x(8),0);lv_obj_set_style_line_rounded(menu->check[i],true,0);lv_obj_clear_flag(menu->check[i],LV_OBJ_FLAG_CLICKABLE);
    }
    hercules_caption(menu->control[2],LV_SYMBOL_SETTINGS,gui_font,30,30,66,0x626b7b,true);
    hercules_caption(menu->control[2],"高级选项",hercules_font(menu,H_ADVANCED),112,33,575,0x303744,false);
    hercules_caption(menu->control[2],">",hercules_font(menu,H_VERSION),696,32,42,0x6d7787,true);
    menu->launch_text=hercules_caption(menu->control[3],"启动 iOS",hercules_font(menu,H_ACTION),0,35,760,0xffffff,true);
    hercules_caption(menu->root,"音量键选择 · 电源键启动",hercules_font(menu,H_HINT),42,1597,762,0x626c7b,true);
    hercules_footer(menu->root,menu);
    menu->countdown_text=hercules_caption(menu->root,"10 秒后自动启动 " HERCULES_TARGET_0_LABEL,hercules_font(menu,H_SMALL),42,1654,762,0x8591a3,true);
    lv_obj_set_hidden(menu->countdown_text,true);
    lv_group_set_editing(gui_grp, false);
    lv_group_set_wrap(gui_grp, true);
    char *initial = confd_get_string("boot.default", "android");
    unsigned selected = initial && strcmp(initial, "ios") == 0 ? 1 : 0;
    free(initial);
    menu->leaving=false;
    hercules_select(menu, selected);
    lv_group_focus_obj(menu->control[selected]);
    if(!hercules_user_interacted&&!no_autoboot){
        menu->countdown_started=lv_tick_get();menu->countdown_seconds=10;
        lv_obj_set_hidden(menu->countdown_text,false);
        menu->countdown=lv_timer_create(hercules_countdown_tick,100,menu);
    }
    tlog_notice("Hercules native boot menu ready: 10-second Android autoboot; first input cancels for this boot");
    return 0;
}
