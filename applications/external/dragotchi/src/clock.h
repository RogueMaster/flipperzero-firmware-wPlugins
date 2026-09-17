#ifndef __clock_h__
#define __clock_h__
#include <stdint.h>
/* Current time as a UNIX-style timestamp derived from the RTC. */
uint32_t game_now(void);
#endif
