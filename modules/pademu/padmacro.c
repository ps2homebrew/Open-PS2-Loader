#include "loadcore.h"
#include "stdio.h"
#include "sysclib.h"
#include "types.h"
#include "ds34common.h"
#include "padmacro.h"


// The minimum value that makes this code work is 1, but then the turbo is too fast
// and the game misses some button presses.
#define TURBO_MIN_DELAY 2

typedef union
{
    struct
    {
        u32 left_stick_slowdown  : 4;
        u32 right_stick_slowdown : 4;
        u32 l_slowdown_enable    : 1;
        u32 l_slowdown_toggle    : 1;
        u32 r_slowdown_enable    : 1;
        u32 r_slowdown_toggle    : 1;
        u32 lx_invert            : 1;
        u32 ly_invert            : 1;
        u32 rx_invert            : 1;
        u32 ry_invert            : 1;
        u32 turbo_speed          : 2;
    };
    u32 raw;
} PadMacroSettings_t;

// Button pressures are stored in a different order than button bits
// so we use a lookup table of offsets in the structure to find them more easily
static const u8 button_to_analog_lookup[12] = {
    offsetof(struct ds2report, PressureUp),
    offsetof(struct ds2report, PressureRight),
    offsetof(struct ds2report, PressureDown),
    offsetof(struct ds2report, PressureLeft),
    offsetof(struct ds2report, PressureL2),
    offsetof(struct ds2report, PressureR2),
    offsetof(struct ds2report, PressureL1),
    offsetof(struct ds2report, PressureR1),
    offsetof(struct ds2report, PressureTriangle),
    offsetof(struct ds2report, PressureCircle),
    offsetof(struct ds2report, PressureCross),
    offsetof(struct ds2report, PressureSquare),
};

static struct
{
    u16 buttons;
    u8 special_button;
} gPreviousButtons = {0};
static u16 gLeftStickSlowdownMask;
enum StickSlowdownToggle {
    SLOWDOWN_PRESS_MODE = 0,
    SLOWDOWN_TOGGLE_MODE = 2,
    SLOWDOWN_TOGGLE_ON = 3,

};
static u8 gLeftStickSlowdownToggle;
static u16 gRightStickSlowdownMask;
static u8 gRightStickSlowdownToggle;
static u8 gStickInvertBitfield;
static u8 gTurboButtonWaiting = 0; // Waiting for user to choose which button to add to turbo
static u16 gTurboButtonMask = 0;         // bit 1 for each button added to turbo
static u8 gTurboSpeed = 0;
static u8 gTurboButtonCounter = 0;

void padMacroInit(u32 padMacroSettings)
{
    PadMacroSettings_t settings;
    settings.raw = padMacroSettings;
    if (settings.l_slowdown_enable != 0) {
        gLeftStickSlowdownMask = (1 << settings.left_stick_slowdown);
        gLeftStickSlowdownToggle = settings.l_slowdown_toggle ? SLOWDOWN_TOGGLE_MODE : SLOWDOWN_PRESS_MODE;
    } else {
        gLeftStickSlowdownMask = 0;
        gLeftStickSlowdownToggle = SLOWDOWN_PRESS_MODE;
    }

    if (settings.r_slowdown_enable != 0) {
        gRightStickSlowdownMask = (1 << settings.right_stick_slowdown);
        gRightStickSlowdownToggle = settings.r_slowdown_toggle ? SLOWDOWN_TOGGLE_MODE : SLOWDOWN_PRESS_MODE;
    } else {
        gRightStickSlowdownMask = 0;
        gRightStickSlowdownToggle = SLOWDOWN_PRESS_MODE;
    }

    gStickInvertBitfield =
        (settings.lx_invert << PadMacroAxisLX) |
        (settings.ly_invert << PadMacroAxisLY) |
        (settings.rx_invert << PadMacroAxisRX) |
        (settings.ry_invert << PadMacroAxisRY);

    gTurboSpeed = settings.turbo_speed + TURBO_MIN_DELAY;
}

static u8 scaleAnalogStick(u16 buttons, u8 stick_input, u8 axis)
{
    u8 invert = (gStickInvertBitfield >> axis) & 1;
    if (invert) {
        u8 inverted = ~stick_input;
        if (inverted != 128 && stick_input != 128) { // avoid converting values close to middle
            stick_input = inverted;
        }
    }

    u8 left_stick = (axis < PadMacroAxisRX);
    u16 mask = left_stick ? gLeftStickSlowdownMask : gRightStickSlowdownMask;
    u8 toggle_mode = left_stick ? gLeftStickSlowdownToggle : gRightStickSlowdownToggle;
    u8 do_slowdown = 0;
    if (toggle_mode == SLOWDOWN_PRESS_MODE) {
        do_slowdown = (buttons & mask) != 0;
    } else {
        do_slowdown = toggle_mode & 1;
    }

    if (do_slowdown) {
        s16 stick_signed = (s16)stick_input;
        stick_signed -= 128;
        // Scale about 72%
        stick_signed *= 23;
        stick_signed /= 32;
        stick_signed += 128;
        return (u8)stick_signed;
    } else {
        return stick_input;
    }
}

static void scaleAnalogSticks(struct ds2report *rep)
{
    u16 buttons = ~rep->nButtonState;
    rep->RightStickX = scaleAnalogStick(buttons, rep->RightStickX, PadMacroAxisRX); // rx
    rep->RightStickY = scaleAnalogStick(buttons, rep->RightStickY, PadMacroAxisRY); // ry
    rep->LeftStickX = scaleAnalogStick(buttons, rep->LeftStickX, PadMacroAxisLX);   // lx
    rep->LeftStickY = scaleAnalogStick(buttons, rep->LeftStickY, PadMacroAxisLY);   // ly
}

static u8 changeInternalState(struct ds2report *rep, u8 special_button)
{
    u16 buttons = ~rep->nButtonState;
    u8 action_happened = 0;
    if (!special_button) {
        if (gTurboButtonWaiting) {
            if (gPreviousButtons.buttons != buttons && buttons != 0) {
                // If a button is pressed and it's different from the previously pressed button
                // This is because the player might release PS button first, then release Start
                // before choosing the button to turbo-press
                rep->nButtonState = 0xFFFF;
                gTurboButtonMask ^= buttons;
                gTurboButtonWaiting = 0;
                action_happened = 1;
            }
        }

        if (gLeftStickSlowdownToggle != SLOWDOWN_PRESS_MODE) {
            u8 left_slowdown_change =
                (gPreviousButtons.buttons & gLeftStickSlowdownMask) !=
                (buttons & gLeftStickSlowdownMask);
            if (left_slowdown_change && (buttons & gLeftStickSlowdownMask)) {
                gLeftStickSlowdownToggle ^= 1;
            }
        }

        if (gRightStickSlowdownToggle != SLOWDOWN_PRESS_MODE) {
            u8 right_slowdown_change =
                (gPreviousButtons.buttons & gRightStickSlowdownMask) !=
                (buttons & gRightStickSlowdownMask);
            if (right_slowdown_change && (buttons & gRightStickSlowdownMask)) {
                gRightStickSlowdownToggle ^= 1;
            }
        }

        gPreviousButtons.buttons = buttons;
        gPreviousButtons.special_button = special_button;
        return action_happened;
    } else if (
        gPreviousButtons.buttons == buttons &&
        gPreviousButtons.special_button == special_button) {
        // Special button is pressed, but no button change
        rep->nButtonState = 0xFFFF; // Ignore inputs while special button is pressed
        return action_happened;
    }

    rep->nButtonState = 0xFFFF;
    gPreviousButtons.buttons = buttons;
    gPreviousButtons.special_button = special_button;

    switch (buttons) {
        case DS2ButtonUp:
            gStickInvertBitfield ^= (1 << PadMacroAxisLY);
            action_happened = 1;
            break;

        case DS2ButtonLeft:
            gStickInvertBitfield ^= (1 << PadMacroAxisLX);
            action_happened = 1;
            break;

        case DS2ButtonDown:
            gStickInvertBitfield ^= (1 << PadMacroAxisRY);
            action_happened = 1;
            break;

        case DS2ButtonRight:
            gStickInvertBitfield ^= (1 << PadMacroAxisRX);
            action_happened = 1;
            break;

        case DS2ButtonStart:
            gTurboButtonWaiting = !gTurboButtonWaiting;
            action_happened = 1;
            break;

        default:
            break;
    }

    return action_happened;
}

static void performTurbo(struct ds2report *rep)
{
    int i;

    gTurboButtonCounter++;
    if (gTurboButtonCounter >= 2 * gTurboSpeed) {
        // Reset counter
        gTurboButtonCounter = 0;
    }

    if (gTurboButtonCounter < gTurboSpeed) {
        // Let go of all turbo-ized buttons
        rep->nButtonState |= gTurboButtonMask;
        // First 4 bits correspond to buttons without pressure sensing
        u16 turbo_button_tmp = gTurboButtonMask >> 4;
        for (i = 0; i < 12; i++) {
            if (turbo_button_tmp & 1) {
                // Set analog pressure to 0 as well
                ((u8 *)rep)[button_to_analog_lookup[i]] = 0;
            }

            turbo_button_tmp >>= 1;
        }
    }
}

u8 padMacroPerform(struct ds2report *rep, u8 special_button)
{
    u8 action_happened = changeInternalState(rep, special_button);

    scaleAnalogSticks(rep);
    performTurbo(rep);

    return action_happened;
}
