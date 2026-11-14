/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include <stdint.h>
#include <stdbool.h>
enum {H_TAP_DOWN=1,H_TAP_MOVE,H_TAP_LOST,H_TAP_UP,H_TAP_CLICK};
struct hercules_tap_guard {uint32_t sequence;uintptr_t owner;int32_t x,y;bool armed,cancelled,done;};
static bool hercules_tap_update(struct hercules_tap_guard*g,uint32_t sequence,uintptr_t owner,unsigned event,int32_t x,int32_t y,int32_t left,int32_t top,int32_t right,int32_t bottom,int32_t slop,bool scrolling){
 if(g->sequence!=sequence)*g=(struct hercules_tap_guard){.sequence=sequence};
 if(event==H_TAP_DOWN){
  if(!g->owner&&!g->done&&!g->cancelled){g->owner=owner;g->x=x;g->y=y;g->armed=true;}
  else if(g->owner!=owner)g->cancelled=true;
  return false;
 }
 if(g->owner!=owner)return false;
 if(event==H_TAP_LOST){g->cancelled=true;return false;}
 int64_t dx=(int64_t)x-g->x,dy=(int64_t)y-g->y;
 bool inside=x>=left&&x<=right&&y>=top&&y<=bottom;
 if(!inside||scrolling||dx*dx+dy*dy>(int64_t)slop*slop)g->cancelled=true;
 if(event==H_TAP_CLICK){bool ok=g->armed&&!g->cancelled&&inside;g->armed=false;g->done=true;return ok;}
 return false;
}
