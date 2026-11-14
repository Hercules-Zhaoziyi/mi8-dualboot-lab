#include <string.h>
/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Dipper-only PON restart-reason adapter. No voltage, reset-control or EDL writes.
 * Facts: live PM8998 DTB, Xiaomi dipper-p-oss qpnp-power-on.c and power-on.h;
 * arbiter register contract also used by the existing redwood device drivers.
 */
#ifndef H_D_RD
#include <Library/UefiBootServicesTableLib.h>
static UINT32 h_d_rd(UINTN a){return *(volatile UINT32 *)a;}
static void h_d_wr(UINTN a,UINT32 v){*(volatile UINT32 *)a=v;MemoryFence();}
#define H_D_RD h_d_rd
#define H_D_WR h_d_wr
#define H_D_DELAY() gBS->Stall(10)
#endif
static struct {UINT32 version,count,channel,owner;UINT8 type,subtype,old;int phase;} h_dd;
static int h_d_wait(UINTN channel){
    for(unsigned i=0;i<200;i++){
        UINT32 s=H_D_RD(channel+8);
        if(s&1)return (s&14)?-EIO:0;
        H_D_DELAY();
    }
    return -ETIMEDOUT;
}
static int h_d_read(UINT32 channel,UINT8 reg,UINT8 *v){
    UINTN base=0x0E600000+(UINTN)channel*0x80;
    H_D_WR(base,(1u<<27)|((UINT32)reg<<4));
    int e=h_d_wait(base);if(e)return e;
    *v=(UINT8)H_D_RD(base+0x18);return 0;
}
static int hercules_dipper_pon_reason(UINT8 mode){
    memset(&h_dd,0,sizeof(h_dd));h_dd.phase=1;
    if(mode!=1 && mode!=2)return -EINVAL;
    UINT32 version=H_D_RD(0x0C440000),count=H_D_RD(0x0C440004)&0x7ff,channel=512;
    h_dd.version=version;h_dd.count=count;
    if(version<0x50000000 || version>=0x70000000)return -ENOTSUP;
    if(count==0 || count>512)count=512;
    h_dd.phase=2;
    /* A PPID may have aliases for different execution environments.
     * Like the existing device drivers, prefer HLOS-owned channels; never use
     * an alias owned by another system. IRQ-marked HLOS alias wins ties.
     */
    UINT32 best=0;
    for(UINT32 i=0;i<count;i++){
        UINT32 map=H_D_RD(0x0C440900+4*i);
        if(!map || ((map>>8)&0xfff)!=0x008)continue;
        UINT32 owner=H_D_RD(0x0C40A700+4*i)&7;
        if(owner!=0)continue;
        UINT32 score=(map&(1u<<24))?2:1;
        if(score>=best){channel=i;best=score;}
    }
    h_dd.channel=channel;h_dd.phase=3;
    if(channel==512)return -EACCES;
    h_dd.owner=H_D_RD(0x0C40A700+4*channel)&7;
    if(h_dd.owner!=0 || ((H_D_RD(0x0C440900+4*channel)>>8)&0xfff)!=0x008)return -EACCES;
    UINT8 type=0,subtype=0,old=0,back=0;
    int e=h_d_read(channel,4,&type);if(e)return e;
    e=h_d_read(channel,5,&subtype);if(e)return e;
    h_dd.type=type;h_dd.subtype=subtype;h_dd.phase=4;
    if(type!=1 || (subtype!=1 && subtype!=3 && subtype!=4))return -ENOTSUP;
    /* Live DT has no qcom,legacy-hard-reset-offset. Gen2 primary uses bits7:1. */
    UINT8 mask=subtype==4?0xfe:0xfc,shift=subtype==4?1:2;
    e=h_d_read(channel,0x8f,&old);if(e)return e;
    h_dd.old=old;h_dd.phase=5;
#ifdef HERCULES_DIPPER_REBOOT_DIAGNOSTIC_ONLY
    return -EAGAIN; /* read-only probe: no PMIC write, no reset */
#endif
    UINT8 value=(old & (UINT8)~mask)|((mode<<shift)&mask);
    UINTN write=0x0C600000+(UINTN)channel*0x10000;
    h_dd.phase=6;
    H_D_WR(write+0x10,value);
    H_D_WR(write,(UINT32)0x8f<<4);
    e=h_d_wait(write);if(e)return e;
    h_dd.phase=7;
    e=h_d_read(channel,0x8f,&back);if(e)return e;
    if(back!=value)return -EIO;
    printf("dipper reboot: PON subtype=%u reason=%u verified\n",subtype,mode);
    return 0;
}

/* PM8998 primary PON only: factory PS_HOLD shutdown preparation. */
#ifndef H_D_OFF_DELAY
#define H_D_OFF_DELAY() gBS->Stall(500)
#endif
static struct {UINT32 ch;UINT8 ctl,en;int saved;} h_d_off;
static int h_d_power_write(UINT32 ch,UINT8 reg,UINT8 value){
    if(reg!=0x5a && reg!=0x5b)return -EINVAL;
    if((H_D_RD(0x0C40A700+4*ch)&7)!=0 || ((H_D_RD(0x0C440900+4*ch)>>8)&0xfff)!=8)return -EACCES;
    UINTN b=0x0C600000+(UINTN)ch*0x10000;
    H_D_WR(b+0x10,value);H_D_WR(b,(UINT32)reg<<4);
    int e=h_d_wait(b);if(e)return e;
    UINT8 back=0;e=h_d_read(ch,reg,&back);if(e)return e;
    return back==value?0:-EIO;
}
static int hercules_dipper_shutdown_restore(void){
    if(!h_d_off.saved)return 0;
    int e=h_d_power_write(h_d_off.ch,0x5b,h_d_off.en&0x7f);if(e)return e;
    H_D_OFF_DELAY();
    e=h_d_power_write(h_d_off.ch,0x5a,h_d_off.ctl);if(e)return e;
    e=h_d_power_write(h_d_off.ch,0x5b,h_d_off.en);if(!e)h_d_off.saved=0;
    return e;
}
#ifndef H_D_KEY_DELAY
#define H_D_KEY_DELAY() gBS->Stall(10000)
#endif
static int h_d_wait_power_release(UINT32 ch){
    unsigned stable=0;
    /* PM8998 PON INT_RT_STS bit0 is KPDPWR_N, verified by the device input driver.
     * A keypad Enter can arrive while the physical power key is still held.
     */
    h_dd.phase=220;
    for(unsigned i=0;i<500;i++){
        UINT8 status=0;int e=h_d_read(ch,0x10,&status);if(e)return e;
        stable=(status&1)?0:stable+1;
        H_D_KEY_DELAY();
        if(stable>=20)return 0;
    }
    return -ETIMEDOUT;
}
static int hercules_dipper_shutdown_prepare(void){
    memset(&h_dd,0,sizeof(h_dd));h_dd.phase=21;
    if(h_d_off.saved)return -EBUSY;
    UINT32 version=H_D_RD(0x0C440000),count=H_D_RD(0x0C440004)&0x7ff,ch=512,best=0;
    h_dd.version=version;h_dd.count=count;
    if(version<0x50000000 || version>=0x70000000)return -ENOTSUP;
    if(count==0 || count>512)count=512;
    for(UINT32 i=0;i<count;i++){
        UINT32 map=H_D_RD(0x0C440900+4*i);
        if(!map || ((map>>8)&0xfff)!=8 || (H_D_RD(0x0C40A700+4*i)&7)!=0)continue;
        UINT32 score=(map&(1u<<24))?2:1;
        if(score>=best){ch=i;best=score;}
    }
    h_dd.channel=ch;h_dd.phase=22;if(ch==512)return -EACCES;
    UINT8 type=0,sub=0;int e=h_d_read(ch,4,&type);if(e)return e;
    e=h_d_read(ch,5,&sub);if(e)return e;
    h_dd.type=type;h_dd.subtype=sub;if(type!=1 || sub!=4)return -ENOTSUP;
    e=h_d_read(ch,0x5a,&h_d_off.ctl);if(e)return e;
    e=h_d_read(ch,0x5b,&h_d_off.en);if(e)return e;
    e=h_d_wait_power_release(ch);if(e)return e;
    h_d_off.ch=ch;h_d_off.saved=1;h_dd.old=h_d_off.ctl;h_dd.phase=23;
    e=h_d_power_write(ch,0x5b,h_d_off.en&0x7f);if(e)goto restore;
    H_D_OFF_DELAY();h_dd.phase=24;
    e=h_d_power_write(ch,0x5a,(h_d_off.ctl&0xf0)|4);if(e)goto restore;
    h_dd.phase=25;e=h_d_power_write(ch,0x5b,h_d_off.en|0x80);if(e)goto restore;
    h_dd.phase=26;return 0;
restore:
    if(hercules_dipper_shutdown_restore()!=0){h_dd.phase=29;return -EIO;}
    return e;
}

/* qcom,pshold live reg property and SDM845 UEFI map both say 0x0C264000.
 * Xiaomi msm-poweroff.c uses a direct zero write as the PS_HOLD path/fallback.
 * Call only after all PMIC transfers finished and the physical key was released.
 */
#ifndef H_D_DROP_PS_HOLD
#define H_D_DROP_PS_HOLD() H_D_WR(0x0C264000,0)
#define H_D_POWER_WAIT() gBS->Stall(500000)
#endif
static int hercules_dipper_shutdown_drop(void){
    if(!h_d_off.saved)return -EINVAL;
    h_dd.phase=30;
    H_D_DROP_PS_HOLD();
    H_D_POWER_WAIT();
    /* If still executing, no successful poweroff occurred. Do not fall back to
     * the generic SYSTEM_OFF path which rebooted in both device trials.
     */
    h_dd.phase=31;
    int e=hercules_dipper_shutdown_restore();
    return e?e:-ETIMEDOUT;
}
