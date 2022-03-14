#pragma once

#include "stdbool.h"
#include "types.h"

#include "ds34common.h"

enum PadMacroAxes {
    PadMacroAxisLX = 0,
    PadMacroAxisLY = 1,
    PadMacroAxisRX = 2,
    PadMacroAxisRY = 3,
};

/**
 * Initialize macro settings
 * @param padMacroSettings Settings passed from the config
 */
void padMacroInit(u32 padMacroSettings);

/**
 * React to button presses so the macro settings can be changed
 * @param rep DS2 controller report
 * @param special_button Is the "special button" pressed (e.g. PS button on DS3 controller)
 * @return true if the internal state of PadMacro was changed
 */
bool padMacroPerform(struct ds2report *rep, bool special_button);
