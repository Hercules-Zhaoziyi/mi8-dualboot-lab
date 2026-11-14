/* SPDX-License-Identifier: MIT
 * Dipper touch supply: live DT ldoa28 has fixed min=max=3300000uV.
 * Named-resource CMD DB and DRV2 active TCS0 contract matches mi8_fts.c.
 * Require an idle, untriggered empty slot; restore metadata before IRQs resume.
 */
#ifndef H_F_RD
#include <Library/BaseLib.h>
#define H_F_RD(a) MmioRead32(a)
#define H_F_WR(a,v) MmioWrite32(a,v)
#define H_F_SLEEP(us) gBS->Stall(us)
#define H_F_IRQ_OFF() SaveAndDisableInterrupts()
#define H_F_IRQ_RESTORE(v) SetInterruptState(v)
#endif
#ifndef H_F_DB
#define H_F_DB ((const volatile UINT8 *)(UINTN)0x85FE0000)
#endif
#define H_F_TCS_GLOBAL ((UINTN)0x179E0D00)
static struct {UINT32 cfg[3],io[3];BOOLEAN saved;} H_F_Pins;
static const UINTN H_F_Pin[3]={0x03920000,0x0391F000,0x03D17000};
static UINT32 hf_le32(const volatile UINT8*p){return (UINT32)p[0]|(UINT32)p[1]<<8|(UINT32)p[2]<<16|(UINT32)p[3]<<24;}
static UINT32 hf_le16(const volatile UINT8*p){return (UINT32)p[0]|(UINT32)p[1]<<8;}
static UINT32 hf_resource(void){
 const volatile UINT8*b=H_F_DB;const UINT8 name[8]={'l','d','o','a','2','8',0,0};UINT32 found=0;
 if(hf_le32(b)!=1||b[4]!=0xdb||b[5]!=0x30||b[6]!=3||b[7]!=0x0c)return 0;
 for(UINT32 i=0;i<8;i++){
  const volatile UINT8*s=b+8+16*i;UINT32 off=hf_le16(s+2),count=hf_le16(s+6);
  if(!hf_le16(s)||count>512||off>0x20000-144||count*24>0x20000-144-off)continue;
  for(UINT32 j=0;j<count;j++){
   const volatile UINT8*e=b+144+off+24*j;BOOLEAN match=TRUE;
   for(UINT32 k=0;k<8;k++)if(e[k]!=name[k])match=FALSE;
   if(!match)continue;UINT32 address=hf_le32(e+16);
   if((address&0xffff0000)!=0x40000||(address&3)||address>0x4fff4)return 0;
   if(found&&found!=address)return 0;found=address;
  }
 }
 return found;
}
static VOID HftsPowerAbort(void){
 if(!H_F_Pins.saved)return;
 for(UINT32 i=0;i<3;i++){H_F_WR(H_F_Pin[i]+4,H_F_Pins.io[i]&2);H_F_WR(H_F_Pin[i],H_F_Pins.cfg[i]);}
 H_F_Pins.saved=FALSE;
 /* Do not guess an old AOP vote or disable a possibly shared regulator.
  * The request remains the DT's normal3300mV until platform reset/OS takeover.
  */
}
static BOOLEAN hf_gpio(UINTN pin,UINT32 cfg,int level){
 UINT32 old=H_F_RD(pin);if(level>=0)H_F_WR(pin+4,level?2:0);
 H_F_WR(pin,(old&~0x3ffU)|cfg);
 return (H_F_RD(pin)&0x3ff)==cfg&&(level<0||((H_F_RD(pin+4)&2)!=0)==(level!=0));
}
static EFI_STATUS HftsPowerOn(HERCULES_FTS_PROBE*p){
 p->Stage=10;p->Status=10;UINT32 address=hf_resource();p->RpmAddress=address;
 if(!address)return EFI_NOT_FOUND;
 BOOLEAN irq=H_F_IRQ_OFF();UINTN tcs=0;UINT32 ctl=0,ien=H_F_RD(H_F_TCS_GLOBAL),mask=0;
 /* Live DT: ACTIVE_TCS(2), count2, first in config. Slot stride672 from
  * Xiaomi qcom-rpmh-mailbox.c. Do not consider sleep/wake/control groups.
  */
 for(UINT32 slot=0;slot<2;slot++){
  UINTN b=H_F_TCS_GLOBAL+(UINTN)slot*672;UINT32 c=H_F_RD(b+0x14),s=H_F_RD(b+0x18);
  p->TcsIndex=slot;p->SlotControl=c;p->SlotStatus=s;
  if(!(s&1)||(c&0x1000000)||(H_F_RD(H_F_TCS_GLOBAL+4)&(1U<<slot)))continue;
  tcs=b;ctl=c;mask=1U<<slot;break;
 }
 if(!tcs){p->Status=13;H_F_IRQ_RESTORE(irq);return EFI_NOT_READY;}
 UINT32 old_enable=H_F_RD(tcs+0x1c),old_wait=H_F_RD(tcs+0x10);
 UINT32 old[3][3];for(UINT32 i=0;i<3;i++)for(UINT32 k=0;k<3;k++)old[i][k]=H_F_RD(tcs+0x30+20*i+4*k);
 for(UINT32 i=0;i<3;i++){H_F_Pins.cfg[i]=H_F_RD(H_F_Pin[i]);H_F_Pins.io[i]=H_F_RD(H_F_Pin[i]+4);}H_F_Pins.saved=TRUE;
 EFI_STATUS result=EFI_DEVICE_ERROR;BOOLEAN triggered=FALSE,quiet=TRUE;
 p->Stage=11;p->Status=16;
 if(!hf_gpio(H_F_Pin[0],0x3c3,0)||!hf_gpio(H_F_Pin[1],0x1c3,-1))goto done;
 H_F_SLEEP(1000);if(!hf_gpio(H_F_Pin[2],0x203,1))goto done;H_F_SLEEP(5000);
 p->Stage=12;p->Status=14;
 H_F_WR(tcs+0x14,ctl&~0x1010000U);
 H_F_WR(H_F_TCS_GLOBAL+8,mask);
 const UINT32 values[3]={3300,1,7};
 for(UINT32 i=0;i<3;i++){
  H_F_WR(tcs+0x30+20*i,0x10108);H_F_WR(tcs+0x34+20*i,address+4*i);H_F_WR(tcs+0x38+20*i,values[i]);
  if(H_F_RD(tcs+0x30+20*i)!=0x10108||H_F_RD(tcs+0x34+20*i)!=address+4*i||H_F_RD(tcs+0x38+20*i)!=values[i])goto done;
 }
 H_F_WR(tcs+0x10,7);H_F_WR(tcs+0x1c,7);H_F_WR(H_F_TCS_GLOBAL,ien|mask);
 H_F_WR(tcs+0x14,(ctl&~0x1010000U)|0x10000);H_F_WR(tcs+0x14,(ctl&~0x1010000U)|0x1010000);triggered=TRUE;
 p->Stage=13;p->Status=15;
 for(UINT32 i=0;i<2000;i++){if(H_F_RD(H_F_TCS_GLOBAL+4)&mask){result=EFI_SUCCESS;break;}H_F_SLEEP(100);}
 quiet=FALSE;for(UINT32 i=0;i<200;i++){if(H_F_RD(tcs+0x18)&1){quiet=TRUE;break;}H_F_SLEEP(100);}
done:
 H_F_WR(tcs+0x14,ctl&~0x1010000U);H_F_WR(tcs+0x1c,0);H_F_WR(tcs+0x10,0);
 if(!triggered||quiet){
  H_F_WR(H_F_TCS_GLOBAL+8,mask);
  for(UINT32 i=0;i<3;i++)for(UINT32 k=0;k<3;k++)H_F_WR(tcs+0x30+20*i+4*k,old[i][k]);
  H_F_WR(tcs+0x10,old_wait);H_F_WR(tcs+0x1c,old_enable);H_F_WR(tcs+0x14,ctl);H_F_WR(H_F_TCS_GLOBAL,ien);
  for(UINT32 i=0;i<3;i++)for(UINT32 k=0;k<3;k++)if(H_F_RD(tcs+0x30+20*i+4*k)!=old[i][k]){result=EFI_DEVICE_ERROR;p->Status=17;}
  if(H_F_RD(H_F_TCS_GLOBAL)!=ien||H_F_RD(tcs+0x10)!=old_wait||H_F_RD(tcs+0x1c)!=old_enable||H_F_RD(tcs+0x14)!=ctl){result=EFI_DEVICE_ERROR;p->Status=17;}
 }else{
  /* Quarantine an uncompleted request; never replay old addresses while busy. */
  H_F_WR(H_F_TCS_GLOBAL,ien&~mask);result=EFI_TIMEOUT;p->Status=18;
 }
 H_F_IRQ_RESTORE(irq);
 if(EFI_ERROR(result)){HftsPowerAbort();return result;}
 H_F_SLEEP(20000);p->Stage=14;
 if(!hf_gpio(H_F_Pin[0],0x3c3,1)){p->Status=16;HftsPowerAbort();return EFI_DEVICE_ERROR;}
 H_F_SLEEP(100000);p->Status=0;return EFI_SUCCESS;
}
