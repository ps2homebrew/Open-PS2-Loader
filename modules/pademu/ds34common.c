#include "ds34common.h"

#include "loadcore.h"
#include "sysclib.h"

void translate_pad_guitar(const struct ds3guitarreport *in, struct ds2report *out, uint8_t guitar_hero_format)
{
    out->RightStickX = 0x7F;
    out->RightStickY = 0x7F;
    out->LeftStickX = 0x7F;
    out->LeftStickY = -(in->Whammy);

    static const u8 dpad_mapping[] = {
        (DS2ButtonUp),
        (DS2ButtonUp | DS2ButtonRight),
        (DS2ButtonRight),
        (DS2ButtonDown | DS2ButtonRight),
        (DS2ButtonDown),
        (DS2ButtonDown | DS2ButtonLeft),
        (DS2ButtonLeft),
        (DS2ButtonUp | DS2ButtonLeft),
        0,
    };

    u8 dpad = in->Dpad > DS4DpadDirectionReleased ? DS4DpadDirectionReleased : in->Dpad;

    out->nButtonStateL = ~(in->Select | in->Start << 3 | dpad_mapping[dpad]);

    out->nLeft = 0;
    out->nL2 = 1;

    if (guitar_hero_format) {
        // GH PS3 Guitars swap Yellow and Blue
        // Interestingly, it is only GH PS3 Guitars that do this, all the other instruments including GH Drums don't have this swapped.
        out->nButtonStateH = ~(in->Green << 1 | in->Blue << 4 | in->Red << 5 | in->Yellow << 6 | in->Orange << 7);
        if (in->AccelX > 512 || in->AccelX < 432) {
            out->nL2 = 0;
        }
    } else {
        out->nButtonStateH = ~(in->StarPower | in->Green << 1 | in->Yellow << 4 | in->Red << 5 | in->Blue << 6 | in->Orange << 7);
    }
}

void translate_pad_ds3(const struct ds3report *in, struct ds2report *out, u8 pressure_emu)
{
    out->nButtonStateL = ~in->ButtonStateL;
    out->nButtonStateH = ~in->ButtonStateH;

    out->RightStickX = in->RightStickX;
    out->RightStickY = in->RightStickY;
    out->LeftStickX = in->LeftStickX;
    out->LeftStickY = in->LeftStickY;

    if (pressure_emu) { // needs emulating pressure buttons
        out->PressureRight = in->Right * 255;
        out->PressureLeft = in->Left * 255;
        out->PressureUp = in->Up * 255;
        out->PressureDown = in->Down * 255;

        out->PressureTriangle = in->Triangle * 255;
        out->PressureCircle = in->Circle * 255;
        out->PressureCross = in->Cross * 255;
        out->PressureSquare = in->Square * 255;

        out->PressureL1 = in->L1 * 255;
        out->PressureR1 = in->R1 * 255;
        out->PressureL2 = in->L2 * 255;
        out->PressureR2 = in->R2 * 255;
    } else {
        out->PressureRight = in->PressureRight;
        out->PressureLeft = in->PressureLeft;
        out->PressureUp = in->PressureUp;
        out->PressureDown = in->PressureDown;

        out->PressureTriangle = in->PressureTriangle;
        out->PressureCircle = in->PressureCircle;
        out->PressureCross = in->PressureCross;
        out->PressureSquare = in->PressureSquare;

        out->PressureL1 = in->PressureL1;
        out->PressureR1 = in->PressureR1;
        out->PressureL2 = in->PressureL2;
        out->PressureR2 = in->PressureR2;
    }
}

void translate_pad_ds4(const struct ds4report *in, struct ds2report *out, u8 have_touchpad)
{
    static const u8 dpad_mapping[] = {
        (DS2ButtonUp),
        (DS2ButtonUp | DS2ButtonRight),
        (DS2ButtonRight),
        (DS2ButtonDown | DS2ButtonRight),
        (DS2ButtonDown),
        (DS2ButtonDown | DS2ButtonLeft),
        (DS2ButtonLeft),
        (DS2ButtonUp | DS2ButtonLeft),
        0,
    };

    u8
        dpad = in->Dpad > DS4DpadDirectionReleased ? DS4DpadDirectionReleased : in->Dpad, // Just in case an unexpected value appears
        select = in->Share,
        start = in->Option;

    if (have_touchpad && in->TPad) {
        if (!in->nFinger1Active) {
            if (in->Finger1X < 960)
                select = 1;
            else
                start = 1;
        }

        if (!in->nFinger2Active) {
            if (in->Finger2X < 960)
                select = 1;
            else
                start = 1;
        }
    }

    out->nButtonStateL = ~(select | in->L3 << 1 | in->R3 << 2 | start << 3 | dpad_mapping[dpad]);
    out->nButtonStateH = ~(in->L2 | in->R2 << 1 | in->L1 << 2 | in->R1 << 3 | in->Triangle << 4 | in->Circle << 5 | in->Cross << 6 | in->Square << 7);

    out->RightStickX = in->RightStickX;
    out->RightStickY = in->RightStickY;
    out->LeftStickX = in->LeftStickX;
    out->LeftStickY = in->LeftStickY;

    out->PressureRight = out->nRight ? 0 : 255;
    out->PressureLeft = out->nLeft ? 0 : 255;
    out->PressureUp = out->nUp ? 0 : 255;
    out->PressureDown = out->nDown ? 0 : 255;

    out->PressureTriangle = in->Triangle * 255;
    out->PressureCircle = in->Circle * 255;
    out->PressureCross = in->Cross * 255;
    out->PressureSquare = in->Square * 255;

    out->PressureL1 = in->L1 * 255;
    out->PressureR1 = in->R1 * 255;
    out->PressureL2 = in->PressureL2;
    out->PressureR2 = in->PressureR2;
}
