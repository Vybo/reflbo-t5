#pragma once

#include "sdkconfig.h"

/* The board's pins (T5 spec §4.2): one header per board. */
#if CONFIG_REFLBO_BOARD_T547
#include "board_pins_t547.h"
#else
#include "board_pins_rlcd42.h"
#endif
