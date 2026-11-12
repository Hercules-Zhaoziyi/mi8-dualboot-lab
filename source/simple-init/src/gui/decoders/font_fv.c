/* SPDX-License-Identifier: MIT
 * Optional local-font provider for the redistributable UI component.
 * The release contains no proprietary UI font data. A local firmware build may
 * supply two RAW FFS files using the public resource GUIDs below.
 */
#if defined(ENABLE_GUI) && defined(ENABLE_FREETYPE2) && defined(ENABLE_UEFI) && defined(HERCULES_EXTERNAL_UI_FONTS)
#include <Uefi.h>
#include <Pi/PiFirmwareFile.h>
#include <Library/DxeServicesLib.h>
#include <string.h>
#include "gui/font.h"

static EFI_GUID regular_guid={0xe71a7a5c,0x8196,0x4d48,{0xb6,0x41,0x09,0xd2,0x1a,0x13,0xbd,0xf4}};
static EFI_GUID bold_guid={0x30c1b010,0x8e7b,0x48a7,{0xbd,0x44,0x69,0x78,0xcf,0x57,0xd2,0x10}};
static VOID *font_data[2];
static UINTN font_size[2];

lv_font_t *lv_ft_init_ui_external(char *path,int weight,lv_ft_style style) {
    unsigned index=strstr(path,"hercules-ui-bold.ttf")?1:0;
    if(!font_data[index]){
        EFI_STATUS status=GetSectionFromAnyFv(index?&bold_guid:&regular_guid,EFI_SECTION_RAW,0,&font_data[index],&font_size[index]);
        if(EFI_ERROR(status)||!font_data[index]||font_size[index]==0){font_data[index]=NULL;font_size[index]=0;return NULL;}
    }
    return lv_ft_init_data(font_data[index],font_size[index],weight,style);
}
#endif
