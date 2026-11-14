/* SPDX-License-Identifier: MIT */
#ifndef HERCULES_FTS_PROBE_H
#define HERCULES_FTS_PROBE_H
#define HERCULES_FTS_PROBE_GUID {0x5b3ec7e1,0xd16f,0x4b5d,{0xa8,0x94,0x07,0x2d,0x2e,0x5a,0x62,0x0f}}
typedef struct {UINT32 Revision,Stage,Status,Pins,Transfers,Events,Errors;UINT8 Id[8];UINT32 RpmAddress,SlotStatus,SlotControl,TcsIndex;} HERCULES_FTS_PROBE;
#endif
