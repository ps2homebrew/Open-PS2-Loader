#include "irx.h"
#include "types.h"
#include "loadcore.h"
#include "stdio.h"
#include "sifrpc.h"
#include "sysclib.h"
#include "thbase.h"
#include "thsemap.h"
#include "../pademu.h"
#include "../btstack/btstack.h"
#include "ds4bt.h"

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

static u8 rgbled_patterns[][2][3] =
    {
        {{0x00, 0x00, 0x10}, {0x00, 0x00, 0x7F}}, // light blue/blue
        {{0x00, 0x10, 0x00}, {0x00, 0x7F, 0x00}}, // light green/green/
        {{0x10, 0x10, 0x00}, {0x7F, 0x7F, 0x00}}, // light yellow/yellow
        {{0x00, 0x10, 0x10}, {0x00, 0x7F, 0x7F}}, // light cyan/cyan
};

int ds4bt_connect(bt_paddata_t *data, u8 *str, int size);
void ds4bt_init(bt_paddata_t *data, int id);
void ds4bt_read_report(bt_paddata_t *data, u8 *in, int bytes);
void ds4bt_rumble(bt_paddata_t *data);

bt_paddrv_t ds4btdrv = {0x0070, 0x0071, ds4bt_connect, ds4bt_init, ds4bt_read_report, ds4bt_rumble};

#define MAX_DELAY 10
#define CMD_DELAY 2

int ds4bt_connect(bt_paddata_t *data, u8 *str, int size)
{
	if (strncmp(str, "Wireless Controller", 19) == 0) {
        DPRINTF("BS4BT: %s \n", str);
        return 1;
    }
    return 0;
}

void ds4bt_init(bt_paddata_t *data, int id)
{
    u8 init_buf[2];

    init_buf[0] = HID_THDR_GET_REPORT_FEATURE; // THdr
    init_buf[1] = PS4_02_REPORT_ID;            // Report ID

    btstack_hid_command(&ds4btdrv, init_buf, 2, data->id);

	data->id = id;
    data->led[0] = rgbled_patterns[id][1][0];
    data->led[1] = rgbled_patterns[id][1][1];
    data->led[2] = rgbled_patterns[id][1][2];
    data->led[3] = 0;
    DelayThread(CMD_DELAY);
    ds4bt_rumble(data);
    data->btn_delay = 0xFF;
}

void ds4bt_read_report(bt_paddata_t *data, u8 *in, int bytes)
{
    if (in[8] == HID_THDR_DATA_INPUT) {
        ds4report_t *report = (ds4report_t *)(in + 9);
        if (report->ReportID == PS4_11_REPORT_ID) {
            u8 up = 0, down = 0, left = 0, right = 0;
            switch (report->Dpad) {
                case 0:
                    up = 1;
                    break;
                case 1:
                    up = 1;
                    right = 1;
                    break;
                case 2:
                    right = 1;
                    break;
                case 3:
                    down = 1;
                    right = 1;
                    break;
                case 4:
                    down = 1;
                    break;
                case 5:
                    down = 1;
                    left = 1;
                    break;
                case 6:
                    left = 1;
                    break;
                case 7:
                    up = 1;
                    left = 1;
                    break;
                case 8:
                    up = 0;
                    down = 0;
                    left = 0;
                    right = 0;
                    break;
            }

            if (bytes > 63 && report->TPad) {
                if (!report->Finger1Active) {
                    if (report->Finger1X < 960)
                        report->Share = 1;
                    else
                        report->Option = 1;
                }

                if (!report->Finger2Active) {
                    if (report->Finger2X < 960)
                        report->Share = 1;
                    else
                        report->Option = 1;
                }
            }

            data->data[0] = ~(report->Share | report->L3 << 1 | report->R3 << 2 | report->Option << 3 | up << 4 | right << 5 | down << 6 | left << 7);
            data->data[1] = ~(report->L2 | report->R2 << 1 | report->L1 << 2 | report->R1 << 3 | report->Triangle << 4 | report->Circle << 5 | report->Cross << 6 | report->Square << 7);

            data->data[2] = report->RightStickX; //rx
            data->data[3] = report->RightStickY; //ry
            data->data[4] = report->LeftStickX;  //lx
            data->data[5] = report->LeftStickY;  //ly

            data->data[6] = right * 255; //right
            data->data[7] = left * 255;  //left
            data->data[8] = up * 255;    //up
            data->data[9] = down * 255;  //down

            data->data[10] = report->Triangle * 255; //triangle
            data->data[11] = report->Circle * 255;   //circle
            data->data[12] = report->Cross * 255;    //cross
            data->data[13] = report->Square * 255;   //square

            data->data[14] = report->L1 * 255;   //L1
            data->data[15] = report->R1 * 255;   //R1
            data->data[16] = report->PressureL2; //L2
            data->data[17] = report->PressureR2; //R2

            if (report->PSButton) {                                     //display battery level
                if (report->Share && (data->btn_delay == MAX_DELAY)) { //PS + SELECT
                    if (data->analog_btn < 2)                           //unlocked mode
                        data->analog_btn = !data->analog_btn;

                    data->led[0] = rgbled_patterns[data->id][(data->analog_btn & 1)][0];
                    data->led[1] = rgbled_patterns[data->id][(data->analog_btn & 1)][1];
                    data->led[2] = rgbled_patterns[data->id][(data->analog_btn & 1)][2];
					data->btn_delay = 1;
                } else {
                    data->led[0] = report->Battery;
                    data->led[1] = 0;
                    data->led[2] = 0;

                    if (data->btn_delay < MAX_DELAY)
                        data->btn_delay++;
                }
            } else {
                data->led[0] = rgbled_patterns[data->id][(data->analog_btn & 1)][0];
                data->led[1] = rgbled_patterns[data->id][(data->analog_btn & 1)][1];
                data->led[2] = rgbled_patterns[data->id][(data->analog_btn & 1)][2];

                if (data->btn_delay > 0)
                    data->btn_delay--;
            }

            if (report->Power != 0xB && report->Usb_plugged) //charging
                data->led[3] = 1;
            else
                data->led[3] = 0;

			if (data->btn_delay > 0) {
                data->update_rum = 1;
            }
        }
    } else {
        DPRINTF("BS4BT: Unmanaged Input Report: THDR 0x%02X ", in[8]);
        DPRINTF("BS4BT: ID 0x%02X \n", in[9]);
    }
}

void ds4bt_rumble(bt_paddata_t *data)
{
    u8 led_buf[PS4_11_REPORT_LEN + 2];
    mips_memset(led_buf, 0, sizeof(led_buf));

    led_buf[0] = HID_THDR_SET_REPORT_OUTPUT; // THdr
    led_buf[1] = PS4_11_REPORT_ID;           // Report ID
    led_buf[2] = 0x80;                       //update rate 1000Hz
    led_buf[4] = 0xFF;

    led_buf[7] = data->rrum * 255;
    led_buf[8] = data->lrum;

    led_buf[9] = data->led[0];  //r
    led_buf[10] = data->led[1]; //g
    led_buf[11] = data->led[2]; //b

    if (data->led[3]) { //means charging, so blink
        led_buf[12] = 0x80; // Time to flash bright (255 = 2.5 seconds)
        led_buf[13] = 0x80; // Time to flash dark (255 = 2.5 seconds)
    }

    btstack_hid_command(&ds4btdrv, led_buf, PS4_11_REPORT_LEN + 2, data->id);
}

int _start(int argc, char *argv[])
{
    btstack_register(&ds4btdrv);

    return MODULE_RESIDENT_END;
}
