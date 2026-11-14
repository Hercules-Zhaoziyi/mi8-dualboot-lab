/* SPDX-License-Identifier: MIT
 * Dipper ST FTM5 runtime input. GPIO open-drain transport follows the board
 * contract already used by dipper-xnu mi8_fts.c. No firmware/voltage programming.
 */
#include <Uefi.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Protocol/AbsolutePointer.h>
#include <Protocol/HerculesFtsProbe.h>
#include "HerculesFtsDecode.h"
#include "HerculesFtsPower.h"
#define SDA 0x03921000
#define SCL 0x03922000
#define IRQ 0x0391F000
#define RST 0x03920000
#define VDD 0x03D17000
static EFI_GUID ProbeGuid=HERCULES_FTS_PROBE_GUID;
static HERCULES_FTS_PROBE Probe={.Revision=1};
static EFI_HANDLE ProbeHandle,TouchHandle;
static EFI_EVENT PollEvent,ExitEvent;
static EFI_ABSOLUTE_POINTER_MODE Mode={.AbsoluteMaxX=1079,.AbsoluteMaxY=2247};
static EFI_ABSOLUTE_POINTER_STATE Queue[64];
static UINTN Head,Tail,Count;
static struct hfts_point Point;
static BOOLEAN Fault,Stopped;
static UINT32 OldSdaCfg,OldSclCfg,OldSdaOut,OldSclOut;
static VOID Delay(VOID){gBS->Stall(3);}
static VOID Release(UINTN pin){MmioWrite32(pin,MmioRead32(pin)&~0x23cU);}
static VOID Low(UINTN pin){MmioWrite32(pin+4,0);MmioWrite32(pin,(MmioRead32(pin)&~0x3cU)|0x200U);}
static BOOLEAN HighScl(VOID){Release(SCL);for(UINTN i=0;i<200;i++){if(MmioRead32(SCL+4)&1)return TRUE;gBS->Stall(1);}Probe.Status=1;return FALSE;}
static VOID Stop(VOID){Low(SCL);Low(SDA);Delay();(VOID)HighScl();Delay();Release(SDA);Delay();}
static BOOLEAN Start(BOOLEAN repeated){Release(SDA);if(!HighScl())return FALSE;Delay();if(!repeated&&!(MmioRead32(SDA+4)&1)){Probe.Status=2;return FALSE;}Low(SDA);Delay();Low(SCL);return TRUE;}
static BOOLEAN ByteOut(UINT8 value){
 for(UINTN bit=0;bit<8;bit++){if(value&0x80)Release(SDA);else Low(SDA);Delay();if(!HighScl())return FALSE;Delay();Low(SCL);value<<=1;}
 Release(SDA);Delay();if(!HighScl())return FALSE;Delay();BOOLEAN ack=!(MmioRead32(SDA+4)&1);Low(SCL);Release(SDA);if(!ack)Probe.Status=3;return ack;
}
static BOOLEAN ByteIn(UINT8 *value,BOOLEAN last){
 UINT8 data=0;Release(SDA);
 for(UINTN bit=0;bit<8;bit++){Delay();if(!HighScl())return FALSE;Delay();data=(UINT8)((data<<1)|(MmioRead32(SDA+4)&1));Low(SCL);}
 if(last)Release(SDA);else Low(SDA);Delay();if(!HighScl())return FALSE;Delay();Low(SCL);Release(SDA);*value=data;return TRUE;
}
static BOOLEAN ReadCommand(CONST UINT8 *cmd,UINTN n,UINT8 *data,UINTN size){
 Probe.Transfers++;Probe.Status=0;
 if(!Start(FALSE)||!ByteOut(0x92))goto fail;
 for(UINTN i=0;i<n;i++)if(!ByteOut(cmd[i]))goto fail;
 if(!Start(TRUE)||!ByteOut(0x93))goto fail;
 for(UINTN i=0;i<size;i++)if(!ByteIn(&data[i],i+1==size))goto fail;
 Stop();return TRUE;
fail:Stop();Probe.Errors++;return FALSE;
}
static BOOLEAN WriteCommand(CONST UINT8 *cmd,UINTN n){
 Probe.Transfers++;Probe.Status=0;if(!Start(FALSE)||!ByteOut(0x92))goto fail;
 for(UINTN i=0;i<n;i++)if(!ByteOut(cmd[i]))goto fail;Stop();return TRUE;
fail:Stop();Probe.Errors++;return FALSE;
}
static VOID Enqueue(VOID){
 if(Count==64){Fault=TRUE;Count=0;Probe.Status=7;return;}
 EFI_ABSOLUTE_POINTER_STATE *s=&Queue[Head];ZeroMem(s,sizeof(*s));s->CurrentX=Point.x;s->CurrentY=Point.y;s->ActiveButtons=Point.down?EFI_ABSP_TouchActive:0;Head=(Head+1)%64;Count++; 
}
static VOID EFIAPI Poll(IN EFI_EVENT Event,IN VOID *Context){
 (VOID)Event;(VOID)Context;if(Fault||Stopped)return;
 if(MmioRead32(IRQ+4)&1)return;
 CONST UINT8 cmd=0x86;UINT8 event[8];
 for(UINTN i=0;i<8;i++){
  if(!ReadCommand(&cmd,1,event,8)){Fault=TRUE;Count=0;return;}
  if(!event[0])break;Probe.Events++;
  if(event[0]==0xf3||event[0]==0x03){Fault=TRUE;Count=0;Probe.Status=8;return;}
  if(hfts_decode(&Point,event))Enqueue();if(Fault)return;
  if(MmioRead32(IRQ+4)&1)break;
 }
}
static EFI_STATUS EFIAPI Reset(IN EFI_ABSOLUTE_POINTER_PROTOCOL *This,IN BOOLEAN Extended){
 (VOID)This;(VOID)Extended;EFI_TPL old=gBS->RaiseTPL(TPL_NOTIFY);Head=Tail=Count=0;ZeroMem(&Point,sizeof(Point));gBS->RestoreTPL(old);return Fault?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI GetState(IN EFI_ABSOLUTE_POINTER_PROTOCOL *This,OUT EFI_ABSOLUTE_POINTER_STATE *State){
 (VOID)This;if(!State)return EFI_INVALID_PARAMETER;EFI_TPL old=gBS->RaiseTPL(TPL_NOTIFY);EFI_STATUS status=EFI_NOT_READY;
 if(Fault||Stopped)status=EFI_DEVICE_ERROR;else if(Count){CopyMem(State,&Queue[Tail],sizeof(*State));Tail=(Tail+1)%64;Count--;status=EFI_SUCCESS;}gBS->RestoreTPL(old);return status;
}
static EFI_ABSOLUTE_POINTER_PROTOCOL Pointer={.Reset=Reset,.GetState=GetState,.Mode=&Mode};
static VOID EFIAPI WaitInput(IN EFI_EVENT Event,IN VOID *Context){(VOID)Context;if(Count)gBS->SignalEvent(Event);}
static VOID EFIAPI ExitBoot(IN EFI_EVENT Event,IN VOID *Context){(VOID)Event;(VOID)Context;Stopped=TRUE;if(PollEvent)gBS->SetTimer(PollEvent,TimerCancel,0);Release(SDA);Release(SCL);}
static VOID RestorePins(VOID){MmioWrite32(SDA+4,OldSdaOut&2);MmioWrite32(SCL+4,OldSclOut&2);MmioWrite32(SDA,OldSdaCfg);MmioWrite32(SCL,OldSclCfg);}
EFI_STATUS EFIAPI HerculesFtsEntry(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable){
 (VOID)ImageHandle;(VOID)SystemTable;EFI_STATUS st=gBS->InstallMultipleProtocolInterfaces(&ProbeHandle,&ProbeGuid,&Probe,NULL);if(EFI_ERROR(st))return st;
 Probe.Stage=1;Probe.Pins=((MmioRead32(VDD+4)&1)?1:0)|((MmioRead32(RST+4)&1)?2:0)|((MmioRead32(SDA+4)&1)?4:0)|((MmioRead32(SCL+4)&1)?8:0);
 BOOLEAN power_initialized=FALSE;
 if((Probe.Pins&3)!=3){
  if(EFI_ERROR(HftsPowerOn(&Probe)))return EFI_SUCCESS;
  power_initialized=TRUE;
  UINT32 vc=MmioRead32(VDD),vi=MmioRead32(VDD+4),rc=MmioRead32(RST),ri=MmioRead32(RST+4);
  Probe.Pins=((vc&0x200?(vi&2)!=0:(vi&1)!=0)?1:0)|((rc&0x200?(ri&2)!=0:(ri&1)!=0)?2:0);
 }
 OldSdaCfg=MmioRead32(SDA);OldSclCfg=MmioRead32(SCL);OldSdaOut=MmioRead32(SDA+4);OldSclOut=MmioRead32(SCL+4);Release(SDA);Release(SCL);Probe.Stage=2;
 CONST UINT8 idcmd[5]={0xfa,0x20,0,0,0};if(!ReadCommand(idcmd,5,Probe.Id,8))goto fail;
 Probe.Stage=3;if(Probe.Id[0]!=0x36||Probe.Id[1]!=0x39){Probe.Status=5;goto fail;}
 if(power_initialized){
  CONST UINT8 readycmd=0x86;UINT8 ready[8];
  for(UINTN i=0;i<100;i++){if(!ReadCommand(&readycmd,1,ready,8))goto fail;if(ready[0]==0x03)break;gBS->Stall(20000);}
 }

 CONST UINT8 scan[3]={0xa0,0,1};if(!WriteCommand(scan,3))goto fail;Probe.Stage=4;
 BOOLEAN echoed=FALSE;CONST UINT8 readone=0x86;UINT8 event[8];
 for(UINTN i=0;i<100;i++){
  if(!ReadCommand(&readone,1,event,8))goto fail;
  if(event[0]==0x43&&event[1]==1&&event[2]==0xa0&&event[3]==0&&event[4]==1){echoed=TRUE;break;}gBS->Stall(5000);
 }
 if(!echoed){Probe.Status=6;goto fail;}
 Probe.Stage=5;
 st=gBS->CreateEvent(EVT_NOTIFY_WAIT,TPL_CALLBACK,WaitInput,NULL,&Pointer.WaitForInput);if(EFI_ERROR(st))goto fail;
 st=gBS->CreateEvent(EVT_TIMER|EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Poll,NULL,&PollEvent);if(EFI_ERROR(st))goto fail;
 st=gBS->CreateEvent(EVT_SIGNAL_EXIT_BOOT_SERVICES,TPL_NOTIFY,ExitBoot,NULL,&ExitEvent);if(EFI_ERROR(st))goto fail;
 st=gBS->InstallMultipleProtocolInterfaces(&TouchHandle,&gEfiAbsolutePointerProtocolGuid,&Pointer,NULL);if(EFI_ERROR(st))goto fail;
 st=gBS->SetTimer(PollEvent,TimerPeriodic,200000);if(EFI_ERROR(st)){Fault=TRUE;Probe.Status=9;return EFI_SUCCESS;}
 Probe.Stage=6;Probe.Status=0;return EFI_SUCCESS;
fail:
 if(PollEvent)gBS->CloseEvent(PollEvent);if(Pointer.WaitForInput)gBS->CloseEvent(Pointer.WaitForInput);if(ExitEvent)gBS->CloseEvent(ExitEvent);Probe.Errors++;RestorePins();if(power_initialized)HftsPowerAbort();return EFI_SUCCESS;
}
