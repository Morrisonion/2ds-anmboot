#ifndef TIMER_H
#define TIMER_H
#include "types.h"
#define REG_TIMER_LOAD (*(vu32*)0x17E00600u)
#define REG_TIMER_COUNT (*(vu32*)0x17E00604u)
#define REG_TIMER_CONTROL (*(vu32*)0x17E00608u)
#define REG_TIMER_STICKY (*(vu32*)0x17E0060Cu)
/* 268111856 Hz MPCore private timer effectively counts at CPU/2.
   Start BEFORE rendering so render time is included in the 16.667 ms frame budget. */
#define FRAME_TICKS 2234265u
static inline void frame_start(void){REG_TIMER_CONTROL=0;REG_TIMER_STICKY=1;REG_TIMER_LOAD=FRAME_TICKS;REG_TIMER_CONTROL=1;}
static inline void frame_wait(void){while(REG_TIMER_COUNT>32u){}REG_TIMER_CONTROL=0;REG_TIMER_STICKY=1;}
static inline void wait_one_frame(void){frame_start();frame_wait();}
#endif
