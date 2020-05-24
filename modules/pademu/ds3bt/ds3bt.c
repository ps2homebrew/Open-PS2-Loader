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
#include "ds3bt.h"

#define DPRINTF(x...) printf(x)
//#define DPRINTF(x...)

static u8 output_01_report[] =
    {
        0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x02,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00};

static u8 led_patterns[][2] =
    {
        {0x1C, 0x02},
        {0x1A, 0x04},
        {0x16, 0x08},
        {0x0E, 0x10},
};

static u8 power_level[] =
    {
        0x00, 0x00, 0x02, 0x06, 0x0E, 0x1E};

int ds3bt_connect(bt_paddata_t *data, u8 *str, int size);
void ds3bt_init(bt_paddata_t *data, int id);
void ds3bt_read_report(bt_paddata_t *data, u8 *in, int bytes);
void ds3bt_rumble(bt_paddata_t *data);

bt_paddrv_t ds3btdrv = {0x0040, 0x0041, ds3bt_connect, ds3bt_init, ds3bt_read_report, ds3bt_rumble};

static u8 press_emu = 0;

#define MAX_DELAY 10
#define CMD_DELAY 2

int ds3bt_connect(bt_paddata_t *data, u8 *str, int size)
{
    DPRINTF("BS3BT: %s \n", str); //PLAYSTATION(R)3 Controller
    
	if (strncmp(str, "PLAYSTATION(R)3 Controller", 26) != 0) {
        return 0;
    }

    return 1;
}

void ds3bt_init(bt_paddata_t *data, int id)
{
    u8 init_buf[PS3_F4_REPORT_LEN + 2];

    init_buf[0] = HID_THDR_SET_REPORT_FEATURE; // THdr
    init_buf[1] = PS3_F4_REPORT_ID;            // Report ID
    init_buf[2] = 0x42;
    init_buf[3] = 0x03;
    init_buf[4] = 0x00;
    init_buf[5] = 0x00;

    btstack_hid_command(&ds3btdrv, init_buf, PS3_F4_REPORT_LEN + 2, data->id);

	data->id = id;
	data->led[0] = led_patterns[id][1];
    data->led[1] = 0;
    DelayThread(CMD_DELAY);
    ds3bt_rumble(data);
    data->btn_delay = 0xFF;
    press_emu = 0;
}

void ds3bt_read_report(bt_paddata_t *data, u8 *in, int bytes)
{
    if (in[8] == HID_THDR_DATA_INPUT) {
        ds3report_t *report = (ds3report_t *)(in + 9);
        if (report->ReportID == PS3_01_REPORT_ID) {

            if (report->RightStickX == 0 && report->RightStickY == 0) // ledrumble cmd causes null report sometime
                return;

            data->data[0] = ~report->ButtonStateL;
            data->data[1] = ~report->ButtonStateH;

            data->data[2] = report->RightStickX; //rx
            data->data[3] = report->RightStickY; //ry
            data->data[4] = report->LeftStickX;  //lx
            data->data[5] = report->LeftStickY;  //ly

            if (bytes == 21 && !press_emu)
                press_emu = 1;

            if (press_emu) {                                //needs emulating pressure buttons
                data->data[6] = report->Right * 255; //right
                data->data[7] = report->Left * 255;  //left
                data->data[8] = report->Up * 255;    //up
                data->data[9] = report->Down * 255;  //down

                data->data[10] = report->Triangle * 255; //triangle
                data->data[11] = report->Circle * 255;   //circle
                data->data[12] = report->Cross * 255;    //cross
                data->data[13] = report->Square * 255;   //square

                data->data[14] = report->L1 * 255; //L1
                data->data[15] = report->R1 * 255; //R1
                data->data[16] = report->L2 * 255; //L2
                data->data[17] = report->R2 * 255; //R2

                report->Power = 0x05;
            } else {
                data->data[6] = report->PressureRight; //right
                data->data[7] = report->PressureLeft;  //left
                data->data[8] = report->PressureUp;    //up
                data->data[9] = report->PressureDown;  //down

                data->data[10] = report->PressureTriangle; //triangle
                data->data[11] = report->PressureCircle;   //circle
                data->data[12] = report->PressureCross;    //cross
                data->data[13] = report->PressureSquare;   //square

                data->data[14] = report->PressureL1; //L1
                data->data[15] = report->PressureR1; //R1
                data->data[16] = report->PressureL2; //L2
                data->data[17] = report->PressureR2; //R2
            }

            if (report->PSButtonState) {                                       //display battery level
                if (report->Select && (data->btn_delay == MAX_DELAY)) { //PS + SELECT
                    if (data->analog_btn < 2)                           //unlocked mode
                        data->analog_btn = !data->analog_btn;

                    data->led[0] = led_patterns[data->id][(data->analog_btn & 1)];
                    data->btn_delay = 1;
                } else {
                    if (report->Power != 0xEE)
                        data->led[0] = power_level[report->Power];

                    if (data->btn_delay < MAX_DELAY)
                        data->btn_delay++;
                }
            } else {
                data->led[0] = led_patterns[data->id][(data->analog_btn & 1)];

                if (data->btn_delay > 0)
                    data->btn_delay--;
            }

            if (report->Power == 0xEE) //charging
                data->led[1] = 1;
            else
                data->led[1] = 0;

			if (data->btn_delay > 0) {
                data->update_rum = 1;
            }
        }
    } else {
        DPRINTF("BS3BT: Unmanaged Input Report: THDR 0x%02X ", in[8]);
        DPRINTF("BS3BT: ID 0x%02X \n", in[9]);
    }
}

void ds3bt_rumble(bt_paddata_t *data)
{
	u8 led_buf[PS3_01_REPORT_LEN + 2];
    mips_memset(led_buf, 0, sizeof(led_buf));
    mips_memcpy(led_buf + 2, output_01_report, sizeof(output_01_report));

    if (data->fix)
        led_buf[0] = 0xA2; // THdr
    else
        led_buf[0] = HID_THDR_SET_REPORT_OUTPUT; // THdr

    led_buf[1] = PS3_01_REPORT_ID; // Report ID

    mips_memcpy(&led_buf[2], output_01_report, sizeof(output_01_report)); // PS3_01_REPORT_LEN);

    if (data->fix) {
        if (data->rrum < 5)
            data->rrum = 0;
    }

    led_buf[3] = 0xFE; //rt
    led_buf[4] = data->rrum; //rp
    led_buf[5] = 0xFE; //lt
    led_buf[6] = data->lrum; //lp

    led_buf[11] = data->led[0] & 0x7F; //LED Conf

    if (data->led[1]) { //means charging, so blink
        led_buf[15] = 0x32;
        led_buf[20] = 0x32;
        led_buf[25] = 0x32;
        led_buf[30] = 0x32;
    }

    btstack_hid_command(&ds3btdrv, led_buf, sizeof(output_01_report) + 2, data->id);
}

int _start(int argc, char *argv[])
{
    btstack_register(&ds3btdrv);

    return MODULE_RESIDENT_END;
}
